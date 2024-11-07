/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <ctype.h>		/* toupper */
#include <errno.h>		/* for ETIMEDOUT, errno */
#include <fcntl.h>		/* SEEK_*, O_* */
#include <assert.h>		/* assert */

#include <xml.h>

#include <base/types.h>
#include <base/logging.h>

#include <api.h>
#include <arch.h>

#include <dsp/effects.h>
#include <software/renderer.h>
#include "device.h"
#include "io.h"

#define BACKEND_NAME_ALIAS	"Audio Files"
#define BACKEND_NAME		"Audio Stream"
#define DEFAULT_RENDERER	AAX_NAME_STR""

#define USE_PID			true
#define USE_PLAYBACK_THREAD	true
#ifdef WIN32
# define USE_CAPTURE_THREAD	false
#else
# define USE_CAPTURE_THREAD	true
#endif

static _aaxDriverDetect _aaxStreamDriverDetect;
static _aaxDriverNewHandle _aaxStreamDriverNewHandle;
static _aaxDriverFreeHandle _aaxALSADriverFreeHandle;
static _aaxDriverGetDevices _aaxStreamDriverGetDevices;
static _aaxDriverGetInterfaces _aaxStreamDriverGetInterfaces;
static _aaxDriverConnect _aaxStreamDriverConnect;
static _aaxDriverDisconnect _aaxStreamDriverDisconnect;
static _aaxDriverDisconnect  _aaxStreamDriverFlush;
static _aaxDriverSetup _aaxStreamDriverSetup;
static _aaxDriverCaptureCallback _aaxStreamDriverCapture;
static _aaxDriverPlaybackCallback _aaxStreamDriverPlayback;
static _aaxDriverSetPosition _aaxStreamDriverSetPosition;
static _aaxDriverSetName _aaxStreamDriverSetName;
static _aaxDriverGetName _aaxStreamDriverGetName;
static _aaxDriverRender _aaxStreamDriverRender;
static _aaxDriverState _aaxStreamDriverState;
static _aaxDriverParam _aaxStreamDriverParam;
static _aaxDriverThread _aaxStreamDriverThread;

static char _file_default_renderer[MAX_ID_STRLEN] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxStreamDriverBackend =
{
   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_file_default_renderer,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxStreamDriverDetect,
   (_aaxDriverNewHandle *)&_aaxStreamDriverNewHandle,
   (_aaxDriverFreeHandle *)&_aaxALSADriverFreeHandle,
   (_aaxDriverGetDevices *)&_aaxStreamDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxStreamDriverGetInterfaces,

   (_aaxDriverSetName *)&_aaxStreamDriverSetName,
   (_aaxDriverGetName *)&_aaxStreamDriverGetName,
   (_aaxDriverRender *)&_aaxStreamDriverRender,
   (_aaxDriverThread *)&_aaxStreamDriverThread,

   (_aaxDriverConnect *)&_aaxStreamDriverConnect,
   (_aaxDriverDisconnect *)&_aaxStreamDriverDisconnect,
   (_aaxDriverSetup *)&_aaxStreamDriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxStreamDriverCapture,
   (_aaxDriverPlaybackCallback *)&_aaxStreamDriverPlayback,
   (_aaxDriverDisconnect *)&_aaxStreamDriverFlush,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,
   (_aaxDriverSetPosition *)&_aaxStreamDriverSetPosition,

   ( _aaxDriverGetSetSources*)_aaxSoftwareDriverGetSetSources,

   (_aaxDriverState *)&_aaxStreamDriverState,
   (_aaxDriverParam *)&_aaxStreamDriverParam,
   (_aaxDriverLog *)&_aaxStreamDriverLog
};

typedef struct
{
   void *handle;
   char *name;

   struct _meta_t meta;

   char use_iothread;
   char copy_to_buffer; // true if Capture has to copy the data unmodified
   char start_with_fill;
   char end_of_file;

   uint8_t bits_sample;
   uint8_t no_channels;
   uint32_t channel_mask;
   enum aaxFormat format;
   int mode;
   float latency;
   float frequency;
   size_t no_samples;
   unsigned int no_bytes;
   float buffer_fill; // pct
   float refresh_rate;
   float dt;

   _io_t *io;
   _ext_t* ext;

   char *interfaces;
   _aaxRenderer *render;

   // the capture or the playback thread.
   struct threat_t iothread;

   // there are two buffers:
   //  * ioBuffer for reading/writing data from or to a file or socket.
   //  * rawBuffer for managing data to or from a device, could be encoded.
   //
   // data has to be transfered between the two when capturing or writing.
   // ioBuffer needs to be locked if accessed, rawBuffer does not.
   _aaxMutex *ioBufLock;
   _data_t *ioBuffer;
   _data_t *rawBuffer;

   // the recv function can block causing ioBufLock to be held for too long.
   // use a separate buffer for recv to circumvent this problem.
   _data_t *captureBuffer;

#if USE_PID
   struct {
      float I;
      float err;
   } PID;
   struct {
      float aim;
   } fill;
#endif

} _driver_t;

static _ext_t* _aaxGetFormat(const char*, enum aaxRenderMode);

static int _aaxStreamDriverReadThread(void*);
static int _aaxStreamDriverWriteThread(void*);
static size_t _aaxStreamDriverWriteChunk(const void*);
static ssize_t _aaxStreamDriverReadChunk(const void*);

static char default_renderer[256];

static int
_aaxStreamDriverDetect(UNUSED(int mode))
{
   snprintf(default_renderer, 255, "%s: %s/AeonWaveOut.wav", BACKEND_NAME, tmpDir());
   return true;
}

static void *
_aaxStreamDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));
   if (handle)
   {
      handle->mode = mode;
      handle->rawBuffer = _aaxDataCreate(1, IOBUF_SIZE, 1);
      handle->ioBuffer = _aaxDataCreate(1, IOBUF_SIZE, 1);
#if USE_CAPTURE_THREAD
      handle->use_iothread = 1;
      handle->captureBuffer = _aaxDataCreate(1, IOBUF_SIZE, 1);
#elif USE_PLAYBACK_THREAD
      handle->use_iothread = 1;
#endif
   }

   return handle;
}

static int
_aaxALSADriverFreeHandle(UNUSED(void *id))
{
   return true;
}


static void *
_aaxStreamDriverConnect(void *config, const void *id, xmlId *xid, const char *device, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *renderer = (char *)device;
   char *s = NULL;

   if (!renderer) {
      renderer = default_renderer;
   }

   if (xid || renderer)
   {
      s = default_renderer;
      if (renderer)
      {
         unsigned int devlenold = strlen(BACKEND_NAME_ALIAS":");
         unsigned int devlen = strlen(BACKEND_NAME":");
         if (!strncasecmp(renderer, BACKEND_NAME":", devlen))
         {
            renderer += devlen;
            while (*renderer == ' ' && *renderer != '\0') renderer++;
         }
         else if (!strncasecmp(renderer, BACKEND_NAME_ALIAS":", devlenold))
         {
            renderer += devlenold;
            while (*renderer == ' ' && *renderer != '\0') renderer++;
         }
         else if (strstr(renderer, ": "))
         {
            _AAX_FILEDRVLOG("File: renderer not supported.");
            return NULL;
         }

         if (strcasecmp(renderer, "default")) {
            s = _aax_strdup(renderer);
         }
      }
      else if (xid)
      {
         char *ptr = xmlAttributeGetString(xid, "name");
         if (ptr)
         {
            s = _aax_strdup(ptr);
            xmlFree(ptr);
         }
      }

      if (s && (*s == '~'))
      {
         const char *home = userHomeDir();
         if (home)
         {
            size_t hlen = strlen(home);
            size_t slen = strlen(s);
            char *ptr;

            ptr = realloc(s, slen+hlen);
            if (ptr)
            {
               s = ptr+hlen-1;
               memmove(s, ptr, slen+1);
               memcpy(ptr, home, hlen);
               s = ptr;
            }
         }
      }
   }

   if (!handle) {
      id = handle = _aaxStreamDriverNewHandle(mode);
   }

   if (handle)
   {
      handle->handle = config;
      handle->name = s;

      snprintf(_file_default_renderer, MAX_ID_STRLEN, "%s",DEFAULT_RENDERER);

      if (xid)
      {
         float f;
         int i;

         f = (float)xmlNodeGetDouble(xid, "frequency-hz");
         if (f)
         {
            if (f < (float)_AAX_MIN_MIXER_FREQUENCY)
            {
               _AAX_FILEDRVLOG("File: frequency too small.");
               f = (float)_AAX_MIN_MIXER_FREQUENCY;
            }
            else if (f > _AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_FILEDRVLOG("File: frequency too large.");
               f = (float)_AAX_MAX_MIXER_FREQUENCY;
            }
            handle->frequency = f;
         }

         i = xmlNodeGetInt(xid, "channels");
         if (i)
         {
            if (i < 1)
            {
               _AAX_FILEDRVLOG("File: no. tracks too small.");
               i = 1;
            }
            else if (i > _AAX_MAX_SPEAKERS)
            {
               _AAX_FILEDRVLOG("File: no. tracks too great.");
               i = _AAX_MAX_SPEAKERS;
            }
            handle->no_channels = i;
         }

         i = xmlNodeGetInt(xid, "bits-per-sample");
         if (i)
         {
            if (i != 16)
            {
               _AAX_FILEDRVLOG("File: unsopported bits-per-sample");
               i = 16;
            }
            handle->bits_sample = i;
         }

         if (xmlNodeGetBool(xid, COPY_TO_BUFFER)) {
            handle->copy_to_buffer = 1;
         }
      }
   }
   else if (s != default_renderer) free(s);

   return handle;
}

static int
_aaxStreamDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int ret = true;

   if (handle)
   {
      if (handle->iothread.started)
      {
         handle->iothread.started = false;

         _aaxSignalTrigger(&handle->iothread.signal);
         _aaxThreadJoin(handle->iothread.ptr);
      }
      _aaxSignalFree(&handle->iothread.signal);
      _aaxMutexDestroy(handle->ioBufLock);

      if (handle->iothread.ptr) {
         _aaxThreadDestroy(handle->iothread.ptr);
      }

      if (handle->name && handle->name != default_renderer) {
         free(handle->name);
      }

      if (handle->ext)
      {
         void *buf = NULL;
         ssize_t size = 0;

         // do one last update, no need to lock
         if (handle->ext->update)
         {
            size_t offs = 0;
            do
            {
               buf = handle->ext->update(handle->ext, &offs, &size, true);
               if (offs > 0) {
                  ret = handle->io->write(handle->io, handle->ioBuffer);
               }
            }
            while (offs > 0);
         }

         // if update returns non NULL then header needs updating.
         if (buf && (handle && handle->io))
         {
            if (handle->io->update_header) {
               ret = handle->io->update_header(handle->io, buf, size);
            }
         }
         handle->ext->close(handle->ext);
         handle->ext = _ext_free(handle->ext);
      }
      if (handle->io)
      {
         handle->io->close(handle->io);
         handle->io = _io_free(handle->io);
      }

      if (handle->render)
      {
         handle->render->close(handle->render->id);
         free(handle->render);
      }

      _aaxDataDestroy(handle->ioBuffer);
      _aaxDataDestroy(handle->rawBuffer);
#if USE_CAPTURE_THREAD
      _aaxDataDestroy(handle->captureBuffer);
#endif
      if (handle->interfaces) {
         free(handle->interfaces);
      }

      _aax_free_meta(&handle->meta);
      free(handle);
   }

   return ret;
}

static int
_aaxStreamDriverFlush(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int ret = false;

   if (handle->ext->flush) {
      ret = handle->ext->flush(handle->ext);
   }

   return ret;
}

static int
_aaxStreamDriverSetup(const void *id, float *refresh_rate, int *fmt,
                    unsigned int *tracks, float *speed, int *bitrate,
                    int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
   char m = (handle->mode == AAX_MODE_READ) ? 0 : 1;
   char *s, *protname, *server, *path, *extension, *mip_level;
   int res, port, rate, size, safe_path;
   int level = 0, rv = false;
   _protocol_t protocol;
   _data_t *ioBuffer;
   size_t headerSize;
   float period_ms;

   assert(handle);

   handle->format = *fmt;
   handle->bits_sample = aaxGetBitsPerSample(*fmt);
   handle->frequency = *speed;

   rate = *speed;
   size = *tracks * rate * handle->bits_sample / *refresh_rate;

   period_ms = 1000.0f / period_rate;
   if (period_ms < 4.0f) period_ms = 4.0f;
   period_rate = 1000.0f / period_ms;

   s = strdup(handle->name);

   mip_level = _url_get_param(s, "level", NULL);
   if (mip_level) level = atoi(mip_level);

   protocol = _url_split(s, &protname, &server, &path, &extension, &port);
   if (!protocol) {
      safe_path = isSafeDir(path, m);
   } else {
      safe_path = true;
   }

#if 0
 printf("\nname: '%s'\n", handle->name);
 printf("protocol: '%s'\n", protname);
 printf("server: '%s'\n", server);
 printf("path: '%s'\n", path);
 printf("ext: '%s'\n", extension);
 printf("port: %i\n", port);
 printf("timeout period: %4.1f ms\n", period_ms);
 printf("refresh rate: %f\n", *refresh_rate);
 printf("buffer size: %i bytes\n", size);
 printf("mip level: %u\n", level);
 printf("stream mode: %s\n", m ? "write" : "read");
 printf("safe dir: %s\n\n", safe_path ? "yes" : "no");
#endif

   handle->io = _io_create(protocol);
   if (!handle->io)
   {
      _aaxStreamDriverLog(id, 0, 0, "Unable to create the protocol");
      return rv;
   }

   handle->io->set_param(handle->io, __F_FLAGS, handle->mode);

   res = false;
   ioBuffer = handle->ioBuffer;
   if (safe_path)
   {
      switch (protocol)
      {
      case PROTOCOL_HTTP:
      case PROTOCOL_HTTPS:
         handle->io->set_param(handle->io, __F_NO_BYTES, 2*size);

         handle->io->set_param(handle->io, __F_PORT, port);
         if (handle->io->open(handle->io, ioBuffer, server, path) >= 0)
         {
            int fmt = handle->io->get_param(handle->io, __F_EXTENSION);
            if (fmt)
            {
               handle->ext = _ext_free(handle->ext);
               handle->ext = _ext_create(fmt);
               if (handle->ext)
               {
                  handle->no_bytes = rv;
                  res = true;
               }
            }
            else
            {
               handle->ext = _ext_free(handle->ext);
               handle->ext = _aaxGetFormat(handle->name, handle->mode);
               if (handle->ext)
               {
                  handle->no_bytes = rv;
                  res = true;
               }
            }

            if (!handle->ext)
            {
               _aaxStreamDriverLog(id, 0, 0, "Unsupported file extension");
               handle->io->close(handle->io);
               handle->io = _io_free(handle->io);
            }
         }
         else
         {
            char s[256];
            snprintf(s, 255, "Connection failed: %s", strerror(errno));
            _aaxStreamDriverLog(id, 0, 0, s);
         }
         break;
      case PROTOCOL_DIRECT:
         handle->io->set_param(handle->io, __F_FLAGS, handle->mode);
         if (handle->io->open(handle->io, ioBuffer, path, NULL) >= 0)
         {
            handle->ext = _ext_free(handle->ext);
            handle->ext = _aaxGetFormat(handle->name, handle->mode);
            if (handle->ext)
            {
               handle->no_bytes= handle->io->get_param(handle->io,__F_NO_BYTES);
               _aaxProcessSetPriority(-20);
               res = true;
            }
         }
         else
         {
            if (handle->mode != AAX_MODE_READ) {
               _aaxStreamDriverLog(id, 0, 0, "File is read-ony");
            } else {
               _aaxStreamDriverLog(id, 0, 0, "File read error");
            }
            handle->io->close(handle->io);
            handle->io = _io_free(handle->io);
         }
         break;
      default:
         _aaxStreamDriverLog(id, 0, 0, "Unknown protocol");
         break;
      }
   }
   else // if (m && !protocol && !safe_path)
   {
      char err[256];
      snprintf(err, 255, "Security alert: unsafe path '%s'", path);
      err[255] = '\0';
      _aaxStreamDriverLog(id, 0, 0, err);
   }

   if (res)
   {
      int file_format = _FMT_NONE;
      size_t period_frames;

      file_format = handle->io->get_param(handle->io, __F_FMT);
      if (file_format == _FMT_NONE) {
         file_format = handle->ext->supported(extension);
      }

      period_frames = (size_t)rintf(rate / period_rate);

      res = handle->ext->setup(handle->ext, handle->mode, &headerSize, rate,
                               *tracks, file_format, period_frames, *bitrate);
      handle->ext->set_param(handle->ext,__F_COPY_DATA, handle->copy_to_buffer);
      handle->ext->set_param(handle->ext, __F_NO_BYTES, handle->no_bytes);
      handle->ext->set_param(handle->ext, __F_MIP_LEVEL, level);
      if (handle->io->protocol == PROTOCOL_HTTP ||
          handle->io->protocol == PROTOCOL_HTTPS)
      {
         handle->ext->set_param(handle->ext, __F_IS_STREAM, 1);
      }

      if (res && ((handle->io->fds.fd >= 0) || m))
      {
         void *header = _aaxDataGetData(ioBuffer, 0);
         size_t no_samples = period_frames;
         ssize_t reqSize = headerSize;
         void *ptr = NULL;

         if (!m) // handle->mode == AAX_MODE_READ
         {
            do
            {
               if (reqSize)
               {
                  int tries = 500; /* approx. 500 miliseconds */
                  do
                  { // no need to lock here: the capture thread is created later
                     res = handle->io->read(handle->io, ioBuffer, reqSize);
                     if (res <= 0) msecSleep(1);
                  }
                  while (res == 0 && --tries);

                  if (res < 0)
                  {
                     if (res == __F_EOF)
                     {
                         _aaxStreamDriverLog(id, 0,0, "Unexpected end-of-file");
                         return false;
                     }
                     _aaxStreamDriverLog(id, 0, 0, "Timeout");
                     break;
                  }
               }

               reqSize = res;
               ptr = handle->ext->open(handle->ext, header, &reqSize,
                                       handle->no_bytes);
               if (reqSize) {
                  _aaxDataMove(ioBuffer, 0, NULL, reqSize);
               }
            }
            while (ptr && reqSize);
         }
         else // write
         {
            reqSize = res;
            ptr = handle->ext->open(handle->ext, header, &reqSize,
                                    handle->no_bytes);
            if (reqSize) {
               _aaxDataMove(ioBuffer, 0, NULL, reqSize);
            }

            if (ptr && reqSize > 0)
            {
               _aaxDataAdd(handle->rawBuffer, 0, ptr, reqSize);
               res = reqSize;
               ptr = NULL;
            }
         }
         headerSize = reqSize;

         if (headerSize && res) //  == (int)headerSize)
         {
            rate = handle->ext->get_param(handle->ext, __F_FREQUENCY);

            handle->frequency = (float)rate;
            handle->fill.aim = IOBUF_THRESHOLD/(float)rate;
            handle->format = handle->ext->get_param(handle->ext, __F_FMT);
            handle->no_channels = handle->ext->get_param(handle->ext, __F_TRACKS);
            handle->channel_mask = handle->ext->get_param(handle->ext, __F_CHANNEL_MASK);

            *fmt = handle->format;
            *speed = handle->frequency;
            *tracks = handle->no_channels;
            *refresh_rate = period_rate;

            handle->refresh_rate = period_rate;
            handle->dt = 1.0f/handle->refresh_rate;
            handle->no_samples = no_samples;
            if (handle->no_channels && handle->bits_sample && handle->frequency)
            {
               handle->latency = (float)_MAX(no_samples,(PERIOD_SIZE*8/(handle->no_channels*handle->bits_sample))) / (float)handle->frequency;
            }

            if (handle->use_iothread)
            {
               _aaxSignalInit(&handle->iothread.signal);

               handle->iothread.ptr = _aaxThreadCreate();
               if (handle->mode == AAX_MODE_READ) {
                  handle->ioBufLock = _aaxMutexCreate(handle->ioBufLock);

                  res = _aaxThreadStart(handle->iothread.ptr,
                                        _aaxStreamDriverReadThread, handle, 20,
					"aaxStreamRead");
               } else {
                  res = _aaxThreadStart(handle->iothread.ptr,
                                       _aaxStreamDriverWriteThread, handle, 20,
				       "aaxStreamWrite");
               }
            }
            else {
               res = thrd_success;
            }

            if (res == thrd_success)
            {
               if (handle->use_iothread) {
                  handle->iothread.started = true;
               }

               handle->render = _aaxSoftwareInitRenderer(handle->latency,
                                                      handle->mode, registered);
               if (handle->render)
               {
                  const char *rstr = handle->render->info(handle->render->id);
                  snprintf(_file_default_renderer, MAX_ID_STRLEN, "%s %s",
                                                   DEFAULT_RENDERER, rstr);
               }
               rv = true;
            }
            else {
               _aaxStreamDriverLog(id, 0, 0, "Internal error: thread failed");
            }
         }
         else if (headerSize) {
            _aaxStreamDriverLog(id, 0, 0, "Incorrect header");
         }
         else
         {
            char *l = strdup(_aaxStreamDriverLog(NULL, 0, 0, NULL));
            _aaxStreamDriverLog(id, 0, 0, l);
            free(l);
         }

         if (!rv)
         {
            handle->ext = _ext_free(handle->ext);
            handle->io->close(handle->io);
            handle->io = _io_free(handle->io);
         }
      }
   }
   else {
//    _aaxStreamDriverLog(id, 0, 0, "Unable to intialize the file format");
   }
   free(s);

   return rv;
}

static size_t
_aaxStreamDriverPlayback(const void *id, void *src, UNUSED(float pitch), float gain, UNUSED(char batched))
{
   _driver_t *handle = (_driver_t *)id;
   _aaxRingBuffer *rb = (_aaxRingBuffer *)src;
   size_t no_samples, offs, outbuf_size, scratchsize;
   unsigned int rb_bps, file_bps, file_tracks;
   unsigned char *scratch, *databuf;
   _data_t *ioBuffer;
   int32_t **tracks;
   ssize_t res = 0;

   assert(rb);
   assert(id != 0);

   /*
    * Opening the  file for playback is delayed until actual playback to
    * prevent setting the file size to zero just bij running aaxinfo -d
    * which always opens a file in playback mode.
    */
   ioBuffer = handle->ioBuffer;
   if (handle->io->fds.fd < 0) {
      handle->io->open(handle->io, ioBuffer, handle->name, NULL);
   }

   offs = rb->get_parami(rb, RB_OFFSET_SAMPLES);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES) - offs;
   rb_bps = rb->get_parami(rb, RB_BYTES_SAMPLE);

   file_bps = handle->ext->get_param(handle->ext, __F_BITS_PER_SAMPLE)/8;
   file_tracks = handle->ext->get_param(handle->ext, __F_TRACKS);
   assert(file_tracks == handle->no_channels);

   outbuf_size = file_tracks * no_samples*_MAX(file_bps, rb_bps);
   if ((handle->rawBuffer == 0) || (_aaxDataGetSize(handle->rawBuffer) < 2*outbuf_size))
   {
      _aaxDataDestroy(handle->rawBuffer);
      handle->rawBuffer = _aaxDataCreate(1, 2*outbuf_size, 1);
      if (handle->rawBuffer == 0) return -1;
   }
   databuf = _aaxDataGetData(handle->rawBuffer, 0);
   scratch = databuf + outbuf_size;
   scratchsize = outbuf_size; // _aaxDataGetSize(handle->rawBuffer)/2;

   assert(outbuf_size <= _aaxDataGetSize(handle->rawBuffer));

   // NOTE: Need RB_RW in case it is used as a mirroring file-backend
   //       See _aaxSoftwareMixerPlay
   tracks = (int32_t**)rb->get_tracks_ptr(rb, RB_RW);
   if (fabsf(gain - 1.0f) > LEVEL_32DB)
   {
      unsigned int t;
      for (t=0; t<file_tracks; t++)
      {
         int32_t *ptr = (int32_t*)tracks[t]+offs;
         _batch_imul_value(ptr, ptr, sizeof(int32_t), no_samples, gain);
      }
   }

   res = handle->ext->cvt_to_intl(handle->ext, databuf, (const int32_t**)tracks,
                                  offs, &no_samples, scratch, scratchsize);
   rb->release_tracks_ptr(rb);
   _aaxDataIncreaseOffset(handle->rawBuffer, 0, res);

   // Move data from rawBuffer to ioBuffer
   if (!_aaxMutexLockTimed(handle->ioBufLock, handle->dt))
   {
      res = _aaxDataGetDataAvail(handle->rawBuffer, 0);
      _aaxDataMoveData(handle->rawBuffer, 0, handle->ioBuffer, 0, res);
      _aaxMutexUnLock(handle->ioBufLock);
   }

   if (batched) {
      _aaxStreamDriverWriteChunk(id);
   } else {
#if USE_PLAYBACK_THREAD
      _aaxSignalTrigger(&handle->iothread.signal);
#else
      _aaxStreamDriverWriteChunk(id);
#endif
   }

   return (res >= 0) ? (res-res) : -1; // (res - no_samples);
}

static ssize_t
_aaxStreamDriverCapture(const void *id, void **dst, ssize_t *offset, size_t *frames, UNUSED(void *scratch), UNUSED(size_t scratchSize), float gain, char batched)
{
   _driver_t *handle = (_driver_t *)id;
   int file_tracks = handle->ext->get_param(handle->ext, __F_TRACKS);
   ssize_t offs = *offset, xoffs = *offset;
// size_t xframes = *frames;
   int num = 5*file_tracks;
   size_t bytes = 0;

   assert(*frames);

   *offset = 0;
   if (handle->io->fds.fd >= 0 && frames && dst)
   {
      int32_t **tracks = (int32_t**)dst;
      unsigned char *data;
      ssize_t res, no_samples;
      float new_rate;
      size_t samples;

      no_samples = (ssize_t)*frames;
      *frames = 0;

      data = NULL;
      bytes = 0;
      samples = no_samples;
      res = __F_NEED_MORE;	// for handle->start_with_fill == true
      do
      {
         // handle->start_with_fill == true if the previous session was
         // a call to  handle->ext->fill and it returned __F_NEED_MORE
         if (!handle->start_with_fill)
         {
            if (!data)
            {
               // copy or convert data from ext's internal buffer to tracks[]
               // this allows ext or fmt to convert it to a supported format.
               if (handle->copy_to_buffer)
               {
                  do
                  {
                     res = handle->ext->copy(handle->ext, tracks[0], offs, &samples);
                     offs += samples;
                     no_samples -= samples;
                     *frames += samples;
                     if (res > 0) bytes += res;
                  }
                  while (res > 0);
               }
               else
               {
                  res = handle->ext->cvt_from_intl(handle->ext, tracks, offs, &samples);
                  offs += samples;
                  no_samples -= samples;
                  *frames += samples;
                  if (res > 0) bytes += res;
               }
            }
            else	/* convert data still in the buffer */
            {
               ssize_t avail = _aaxDataGetDataAvail(handle->rawBuffer, 0);
               res = handle->ext->fill(handle->ext, data, &avail);
               _aaxDataMove(handle->rawBuffer, 0, NULL, avail);
            }
         } /* handle->start_with_fill */
         handle->start_with_fill = false;

         if (res == __F_EOF) {
            handle->end_of_file = true;
            bytes = __F_EOF;
            break;
         }

         if (handle->end_of_file && res == __F_NEED_MORE)
         {
            handle->start_with_fill = true;
            bytes = __F_EOF;
            break;
         }

         /* res holds the number of bytes that are actually converted */
         /* or (-3) __F_PROCESS if the next chunk can be processed         */
         /* or (-2) __F_NEED_MORE if fmt->fill requires more data          */
         /* or (-1) __F_EOF if an error occured, or end of file            */
         if (res == __F_PROCESS)
         {
            data = NULL;
            samples = no_samples;
         }
         else if (res == __F_NEED_MORE || no_samples > 0)
         {
            ssize_t avail;

            if (handle->use_iothread && !batched)
            {
               if (handle->io->protocol == PROTOCOL_DIRECT)
               {
                  _aaxSignalTrigger(&handle->iothread.signal);
                  usecSleep(1);
               }
            }
            else {
               _aaxStreamDriverReadChunk(id);
            }

            // Move data from ioBuffer to rawBuffer
            if (!_aaxMutexLockTimed(handle->ioBufLock, handle->dt))
            {
               float target, input, err, P, I; // , D
               float freq, delay_sec;

               avail = _aaxDataGetDataAvail(handle->ioBuffer, 0);
               res = _aaxDataMoveData(handle->ioBuffer, 0, handle->rawBuffer, 0, avail);
               _aaxMutexUnLock(handle->ioBufLock);

               delay_sec = 1.0f/handle->refresh_rate;
               freq = handle->frequency;

               /* present error */
               target = handle->fill.aim*freq/IOBUF_THRESHOLD;
               input = (float)avail/IOBUF_THRESHOLD;
               P = err = input - target;

               /* accumulation of past errors */
               I = _MINMAX(handle->PID.I + err*delay_sec, 0.5f, 1.5f);
               handle->PID.I = I;

               /* prediction of future errors, from current rate of change */
//             D = (handle->PID.err - err)/delay_sec;
//             handle->PID.err = err;

               err = _MINMAX(0.40f*P + 0.97f*I, -1.0, 1.0);
               handle->buffer_fill = err;
               xoffs = err;
# if 0
 float fact = _MINMAX((1.0f + err), 0.9f, 1.1f);
 printf("target: %2.1f, avail: %2.1f, err: %2.1f (\033[92;4mP: %2.1f, I: %2.1f\033[0m), fact: %2.1f, xoffs: %li\n", target, input, err, P, I, fact, xoffs);
# endif
            }
            data = _aaxDataGetData(handle->rawBuffer, 0); // needed above
         }
      }
      while (no_samples > 0 && --num);

      // Some formats may change the sample rate mid-stream to save bandwith
      new_rate = (float)handle->ext->get_param(handle->ext, __F_FREQUENCY);
      handle->frequency = new_rate;

      if (!handle->copy_to_buffer && bytes > 0)
      {
         /* gain is netagive for auto-gain mode */
         gain = fabsf(gain);
         if (fabsf(gain - 1.0f) > 0.05f)
         {
            int t;
            for (t=0; t<file_tracks; t++)
            {
               int32_t *ptr = (int32_t*)tracks[t] + offs;
               _batch_imul_value(ptr, ptr, sizeof(int32_t), *frames, gain);
            }
         }
      }
      *offset = offs-xoffs;
   }

   return bytes;
}

static int
_aaxStreamDriverSetName(const void *id, int type, const char *name)
{
   _driver_t *handle = (_driver_t *)id;
   int ret = false;
   if (handle)
   {
      switch (type)
      {
      case AAX_MUSIC_PERFORMER_STRING:
      case AAX_MUSIC_PERFORMER_UPDATE:
         handle->meta.artist = strdup(name);
         ret = true;
         break;
      case AAX_TRACK_TITLE_STRING:
      case AAX_TRACK_TITLE_UPDATE:
         handle->meta.title = strdup(name);
         ret = true;
         break;
      case AAX_MUSIC_GENRE_STRING:
         handle->meta.genre = strdup(name);
         ret = true;
         break;
      case AAX_TRACK_NUMBER_STRING:
         handle->meta.trackno = strdup(name);
         ret = true;
         break;
      case AAX_ALBUM_NAME_STRING:
         handle->meta.album = strdup(name);
         ret = true;
         break;
      case AAX_RELEASE_DATE_STRING:
         handle->meta.date = strdup(name);
         ret = true;
         break;
      case AAX_SONG_COMPOSER_STRING:
         handle->meta.composer = strdup(name);
         ret = true;
         break;
      case AAX_SONG_COPYRIGHT_STRING:
         handle->meta.copyright = strdup(name);
         ret = true;
         break;
      case AAX_SONG_COMMENT_STRING:
         handle->meta.comments = strdup(name);
         ret = true;
         break;
      case AAX_ORIGINAL_PERFORMER_STRING:
         handle->meta.original = strdup(name);
         ret = true;
         break;
      case AAX_CONTACT_STRING:
         handle->meta.contact = strdup(name);
         ret = true;
         break;
      case AAX_COVER_IMAGE_DATA:
         handle->meta.image = strdup(name);
         ret = true;
         break;
      default:
         break;
      }
   }
   return ret;
}

static char *
_aaxStreamDriverGetName(const void *id, int type)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   if (handle)
   {
      assert ((int)AAX_MUSIC_PERFORMER_STRING > (int)AAX_MODE_WRITE_MAX);

      switch (type)
      {
      case AAX_MODE_READ:
      case AAX_MODE_WRITE_STEREO:
      case AAX_MODE_WRITE_SPATIAL:
      case AAX_MODE_WRITE_SURROUND:
      case AAX_MODE_WRITE_HRTF:
         if (handle->name) {
            ret = handle->name;
         } else {
            ret = "default";
         }
         break;
      default:
         if (handle->ext)
         {
            switch (type)
            {
            case AAX_MUSIC_PERFORMER_STRING:
               ret = handle->io->name(handle->io, __F_ARTIST);
               if (!ret) {
                  ret = handle->ext->name(handle->ext, __F_ARTIST);
               }
               if (!ret) {
                  ret = handle->meta.artist;
               }
               break;
            case AAX_MUSIC_PERFORMER_UPDATE:
               ret = handle->io->name(handle->io, __F_ARTIST|__F_NAME_CHANGED);
               if (!ret) {
                  ret = handle->ext->name(handle->ext, __F_ARTIST|__F_NAME_CHANGED);
               }
               if (!ret) {
                  ret = handle->meta.artist;
               }
               break;
            case AAX_TRACK_TITLE_STRING:
               ret = handle->io->name(handle->io, __F_TITLE);
               if (!ret) {
                  ret = handle->ext->name(handle->ext, __F_TITLE);
               }
               if (!ret) {
                  ret = handle->meta.title;
               }
               break;
            case AAX_TRACK_TITLE_UPDATE:
               ret = handle->io->name(handle->io, __F_TITLE|__F_NAME_CHANGED);
               if (!ret) {
                  ret = handle->ext->name(handle->ext, __F_TITLE|__F_NAME_CHANGED);
               }
               if (!ret) {
                  ret = handle->meta.title;
               }
               break;
            case AAX_MUSIC_GENRE_STRING:
               ret = handle->io->name(handle->io, __F_GENRE);
               if (!ret) {
                  ret = handle->ext->name(handle->ext, __F_GENRE);
               }
               if (!ret) {
                  ret = handle->meta.genre;
               }
               break;
            case AAX_TRACK_NUMBER_STRING:
               ret = handle->io->name(handle->io, __F_TRACKNO);
               if (!ret) {
                  ret = handle->ext->name(handle->ext, __F_TRACKNO);
               }
               if (!ret) {
                  ret = handle->meta.trackno;
               }
               break;
            case AAX_ALBUM_NAME_STRING:
               ret = handle->io->name(handle->io, __F_ALBUM);
               if (!ret) {
                  ret = handle->ext->name(handle->ext, __F_ALBUM);
               }
               if (!ret) {
                  ret = handle->meta.album;
               }
               break;
            case AAX_RELEASE_DATE_STRING:
               ret = handle->io->name(handle->io, __F_DATE);;
               if (!ret) {
                  ret = handle->ext->name(handle->ext, __F_DATE);
               }
               if (!ret) {
                  ret = handle->meta.date;
               }
               break;
            case AAX_SONG_COMPOSER_STRING:
               ret = handle->io->name(handle->io, __F_COMPOSER);
               if (!ret) {
                  ret = handle->ext->name(handle->ext, __F_COMPOSER);
               }
               if (!ret) {
                  ret = handle->meta.composer;
               }
               break;
            case AAX_SONG_COPYRIGHT_STRING:
               ret = handle->io->name(handle->io, __F_COPYRIGHT);
               if (!ret) {
                  ret = handle->ext->name(handle->ext, __F_COPYRIGHT);
               }
               if (!ret) {
                  ret = handle->meta.copyright;
               }
               break;
            case AAX_SONG_COMMENT_STRING:
               ret = handle->io->name(handle->io, __F_COMMENT);
               if (!ret) {
                  ret = handle->ext->name(handle->ext, __F_COMMENT);
               }
               if (!ret) {
                  ret = handle->meta.comments;
               }
               break;
            case AAX_ORIGINAL_PERFORMER_STRING:
               ret = handle->io->name(handle->io, __F_ORIGINAL);
               if (!ret) {
                   ret = handle->ext->name(handle->ext, __F_ORIGINAL);
               }
               if (!ret) {
                  ret = handle->meta.original;
               }
               break;
            case AAX_CONTACT_STRING:
               ret = handle->io->name(handle->io, __F_WEBSITE);
               if (!ret) {
                   ret = handle->ext->name(handle->ext, __F_WEBSITE);
               }
               if (!ret) {
                  ret = handle->meta.contact;
               }
               break;
            case AAX_COVER_IMAGE_DATA:
               ret = handle->ext->name(handle->ext, __F_IMAGE);
               if (!ret) {
                  ret = handle->meta.image;
               }
               break;
            default:
               break;
            }
         }
      }
   }

   return ret ? strdup(ret) : ret;
}

_aaxRenderer*
_aaxStreamDriverRender(const void* config)
{
   _driver_t *handle = (_driver_t *)config;
   return handle->render;
}


static int
_aaxStreamDriverState(UNUSED(const void *id), enum _aaxDriverState state)

{
   int rv = false;
   switch(state)
   {
   case DRIVER_AVAILABLE:
   case DRIVER_PAUSE:
   case DRIVER_RESUME:
   case DRIVER_SUPPORTS_PLAYBACK:
   case DRIVER_SUPPORTS_CAPTURE:
      rv = true;
      break;
   case DRIVER_SHARED_MIXER:
   case DRIVER_NEED_REINIT:
   default:
      break;
   }
   return rv;
}

static float
_aaxStreamDriverParam(const void *id, enum _aaxDriverParam param)
{
   _driver_t *handle = (_driver_t *)id;
   float rv = 0.0f;
   if (handle)
   {
      switch(param)
      {
      /* float */
      case DRIVER_LATENCY:
         rv = handle->latency;
         break;
      case DRIVER_MAX_VOLUME:
         rv = 1.0f;
         break;
      case DRIVER_MIN_VOLUME:
         rv = 0.0f;
         break;
      case DRIVER_VOLUME:
         rv = 1.0f;
         break;
      case DRIVER_FREQUENCY:
         rv = (float)handle->frequency;
         break;
      case DRIVER_BUFFER_FILL:
         rv = handle->buffer_fill;
         break;

      /* int */
      case DRIVER_MIN_FREQUENCY:
         rv = 8000.0f;
         break;
      case DRIVER_MAX_FREQUENCY:
         rv = _AAX_MAX_MIXER_FREQUENCY;
         break;
      case DRIVER_MIN_TRACKS:
         rv = 1.0f;
         break;
      case DRIVER_MAX_TRACKS:
         rv = (float)_AAX_MAX_SPEAKERS;
         break;
      case DRIVER_NO_BYTES:
         rv = (float)handle->ext->get_param(handle->ext, __F_NO_BYTES);
         // Not all extensions return DRIVER_NO_BYTES
         if (!rv) rv = handle->no_samples*handle->bits_sample/8;
         break;
      case DRIVER_BLOCK_SIZE:
         rv = (float)handle->ext->get_param(handle->ext, __F_BLOCK_SIZE);
         break;
      case DRIVER_BITRATE:
         rv = (float)handle->ext->get_param(handle->ext, __F_BITRATE);
         break;
      case DRIVER_MIN_PERIODS:
      case DRIVER_MAX_PERIODS:
         rv = 1.0f;
         break;
      case DRIVER_MAX_SOURCES:
         rv = ((_handle_t*)(handle->handle))->backend.ptr->getset_sources(0, 0);
         break;
      case DRIVER_MAX_SAMPLES:
         rv = (float)handle->ext->get_param(handle->ext, __F_NO_SAMPLES);
         break;
      case DRIVER_LOOP_COUNT:
         rv = (float)handle->ext->get_param(handle->ext, __F_LOOP_COUNT);
         break;
      case DRIVER_LOOP_START:
         rv = (float)handle->ext->get_param(handle->ext, __F_LOOP_START);
         break;
      case DRIVER_LOOP_END:
         rv = (float)handle->ext->get_param(handle->ext, __F_LOOP_END);
         break;
      case DRIVER_ENVELOPE_SUSTAIN:
         rv = (float)handle->ext->get_param(handle->ext, __F_ENVELOPE_SUSTAIN);
         break;
      case DRIVER_SAMPLED_RELEASE:
         rv = (float)handle->ext->get_param(handle->ext, __F_SAMPLED_RELEASE);
         break;
      case DRIVER_FAST_RELEASE:
         rv = (float)handle->ext->get_param(handle->ext, __F_FAST_RELEASE);
         break;
      case DRIVER_NO_PATCHES:
         rv = (float)handle->ext->get_param(handle->ext, __F_NO_PATCHES);
         break;
      case DRIVER_BASE_FREQUENCY:
         rv = handle->ext->get_param(handle->ext, __F_BASE_FREQUENCY);
         break;
      case DRIVER_LOW_FREQUENCY:
         rv = handle->ext->get_param(handle->ext, __F_LOW_FREQUENCY);
         break;
      case DRIVER_HIGH_FREQUENCY:
         rv = handle->ext->get_param(handle->ext, __F_HIGH_FREQUENCY);
         break;
      case DRIVER_PITCH_FRACTION:
         rv = handle->ext->get_param(handle->ext, __F_PITCH_FRACTION);
         break;
      case DRIVER_TREMOLO_RATE:
         rv = handle->ext->get_param(handle->ext, __F_TREMOLO_RATE);
         break;
      case DRIVER_TREMOLO_DEPTH:
         rv = handle->ext->get_param(handle->ext, __F_TREMOLO_DEPTH);
         break;
      case DRIVER_TREMOLO_SWEEP:
         rv = handle->ext->get_param(handle->ext, __F_TREMOLO_SWEEP);
         break;
      case DRIVER_VIBRATO_RATE:
         rv = handle->ext->get_param(handle->ext, __F_VIBRATO_RATE);
         break;
      case DRIVER_VIBRATO_DEPTH:
         rv = handle->ext->get_param(handle->ext, __F_VIBRATO_DEPTH);
         break;
      case DRIVER_VIBRATO_SWEEP:
         rv = handle->ext->get_param(handle->ext, __F_VIBRATO_SWEEP);
         break;
      case DRIVER_SAMPLE_DELAY:
         rv = (float)handle->no_samples;
         break;

      /* boolean */
      case DRIVER_SEEKABLE_SUPPORT:
         if (handle->ext && handle->io)
         {
            if (handle->ext->get_param(handle->ext, __F_POSITION) &&
                handle->io->get_param(handle->io, __F_POSITION) != -1)
            {
                rv = (float)true;
            }
         }
         break;
      case DRIVER_TIMER_MODE:
      case DRIVER_BATCHED_MODE:
         rv = (float)true;
         break;
      case DRIVER_SHARED_MODE:
      default:
         /* envelopes */
         if (param >= DRIVER_ENVELOPE_LEVEL &&
             param < DRIVER_ENVELOPE_LEVEL_MAX)
         {
            int type = __F_ENVELOPE_LEVEL + (param & 0xF);
            rv = (float)handle->ext->get_param(handle->ext, type);
         }
         else if (param >= DRIVER_ENVELOPE_RATE &&
                  param < DRIVER_ENVELOPE_RATE_MAX)
         {
            int type = __F_ENVELOPE_RATE + (param & 0xF);
            rv = (float)handle->ext->get_param(handle->ext, type);
         }
         break;
      }
   }
   return rv;
}

static int
_aaxStreamDriverSetPosition(const void *id, off_t samples)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = 0;

   if (handle->ext)
   {
      off_t bytes = handle->ext->set_param(handle->ext, __F_POSITION, samples);
      rv = handle->io->set_param(handle->io, __F_POSITION, bytes);
   }
   return rv;
}

static char *
_aaxStreamDriverGetDevices(UNUSED(const void *id), int mode)
{
   static const char *rd[2] = { BACKEND_NAME"\0\0", BACKEND_NAME"\0\0" };
   return (char *)rd[mode];
}

static char *
_aaxStreamDriverGetInterfaces(const void *id, UNUSED(const char *devname), int mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *rv = NULL;

   if (handle && !handle->interfaces)
   {
      char interfaces[2048];
      size_t buflen;
      char *ptr;
      int i = 0;

      ptr = interfaces;
      buflen = 2048;

      for (i=0; i<_EXT_MAX; i++)
      {
         _ext_t* ext = _ext_create(i);
         if (ext)
         {
            if (ext->detect(ext, mode))
            {
               char *ifs;
               size_t len;

               ifs = ext->interfaces(i, mode);
               len = ifs ? strlen(ifs) : 0;
               if (ifs && len)
               {
                  snprintf(ptr, buflen, "%s ", ifs);
                  buflen -= len+1;
                  ptr += len+1;
               }
            }
            _ext_free(ext);
         }
      }

      if (ptr != interfaces)
      {
         *(ptr-1) = '\0';
         ptr++;
         rv = handle->interfaces = malloc(ptr-interfaces);
         if (rv) {
            memcpy(rv, interfaces, ptr-interfaces);
         }
      }
   }
   else if (handle) {
      rv = handle->interfaces;
   }

   return rv;
}

char *
_aaxStreamDriverLog(const void *id, UNUSED(int prio), UNUSED(int type), const char *str)
{
   _driver_t *handle = (_driver_t *)id;
   static char _errstr[256];

   if (str)
   {
      if (handle && handle->io && (handle->io->protocol == PROTOCOL_HTTP ||
                                   handle->io->protocol == PROTOCOL_HTTPS)) {
          snprintf(_errstr, 256, "HTTP: %s\n", str);
      } else {
         snprintf(_errstr, 256, "Stream: %s\n", str);
      }
      _errstr[255] = '\0';  /* always null terminated */

      if (handle) {
         __aaxDriverErrorSet(handle->handle, AAX_BACKEND_ERROR, (char*)&_errstr);
      }
      _AAX_SYSLOG(_errstr);
   }

   return (char*)&_errstr;
}

/*-------------------------------------------------------------------------- */

float note2freq(uint8_t d) {
   return 440.0f*powf(2.0f, ((float)d-69.0f)/12.0f);
}

float cents2pitch(float p, float r) {
   return powf(2.0f, p*r/12.0f);
}

float cents2modulation(float p, float r) {
   return powf(2.0f, p*r/12.0f);
}

static _ext_t*
_aaxGetFormat(const char *url, enum aaxRenderMode mode)
{
   char *s, *protocol, *server, *path, *extension;
   _ext_t *rv = NULL;
   int port, res;

   if (!url) return rv;

   extension = NULL;
   s = strdup(url);
   res = _url_split(s, &protocol, &server, &path, &extension, &port);
   switch (res)
   {
   case PROTOCOL_HTTP:
   case PROTOCOL_HTTPS:
      if (path) extension = strrchr(path, '.');
      if (!extension) extension = ".mp3";
      break;
   case PROTOCOL_DIRECT:
      if (path) extension = strrchr(path, '.');
      break;
   default:
      break;
   }

   if (extension++)
   {
      int i;
      for (i=0; i<_EXT_MAX; i++)
      {
         rv = _ext_create(i);
         if (rv && rv->detect(rv, mode) && rv->supported(extension)) {
            break;
         }
         rv = _ext_free(rv);
      }
   }
   free(s);

   return rv;
}

static size_t
_aaxStreamDriverWriteChunk(const void *id)
{
   _driver_t *handle = (_driver_t*)id;
   size_t rv = 0;

   if (handle->io)
   {
      _data_t *buf = handle->ioBuffer;
      while (_aaxDataGetDataAvail(buf, 0))
      {
         ssize_t res = 0;

         if (!_aaxMutexLockTimed(handle->ioBufLock, handle->dt))
         {
            res = handle->io->write(handle->io, buf);
            _aaxMutexUnLock(handle->ioBufLock);
         }

         if (res > 0)
         {
            rv += res;
            if (handle->ext->update)
            {
               size_t spos = 0;
               ssize_t usize;
               void *buf = handle->ext->update(handle->ext, &spos, &usize,
                                               false);

               // if update returns non NULL then header needs updating.
               if (buf && handle->io->update_header) {
                  res = handle->io->update_header(handle->io, buf, usize);
               }
            }
         }
         else
         {
            _AAX_FILEDRVLOG(strerror(errno));
            break;
         }
      }
   }

   return rv;
}

static int
_aaxStreamDriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   _intBufferData *dptr_sensor;
   const _aaxDriverBackend *be;
   _aaxRingBuffer *dest_rb;
   _aaxAudioFrame *smixer;
// _driver_t *be_handle;
   int state, tracks;
   float delay_sec;
   float freq;
   int res;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return false;
   }

   be = handle->backend.ptr;
   delay_sec = 1.0f/handle->info->period_rate;

   tracks = 2;
   freq = 48000.0f;
   smixer = NULL;
   dest_rb = be->get_ringbuffer(MAX_EFFECTS_TIME, handle->info->mode);
   if (dest_rb)
   {
      dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
      if (dptr_sensor)
      {
         _aaxMixerInfo* info;
         _sensor_t* sensor;

         sensor = _intBufGetDataPtr(dptr_sensor);
         smixer = sensor->mixer;
         info = smixer->info;

         freq = info->frequency;
         tracks = info->no_tracks;
         dest_rb->set_parami(dest_rb, RB_NO_TRACKS, tracks);
         dest_rb->set_format(dest_rb, AAX_PCM24S, true);
         dest_rb->set_paramf(dest_rb, RB_FREQUENCY, freq);
         dest_rb->set_paramf(dest_rb, RB_DURATION_SEC, delay_sec);
         dest_rb->init(dest_rb, true);
         dest_rb->set_state(dest_rb, RB_STARTED);

         handle->ringbuffer = dest_rb;
         _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
      }
   }

   dest_rb = handle->ringbuffer;
   if (!dest_rb) {
      return false;
   }

   /* get real duration, it might have been altered for better performance */
   delay_sec = dest_rb->get_paramf(dest_rb, RB_DURATION_SEC);

   be->state(handle->backend.handle, DRIVER_PAUSE);
   state = AAX_SUSPENDED;

// be_handle = (_driver_t *)handle->backend.handle;
   _aaxMutexLock(handle->thread.signal.mutex);
   do
   {
      float dt = delay_sec;

      if TEST_FOR_FALSE(handle->thread.started) {
         break;
      }

      if (state != handle->state)
      {
         if (_IS_PAUSED(handle) || (!_IS_PLAYING(handle) && _IS_STANDBY(handle))) {
            be->state(handle->backend.handle, DRIVER_PAUSE);
         }
         else if (_IS_PLAYING(handle) || _IS_STANDBY(handle)) {
            be->state(handle->backend.handle, DRIVER_RESUME);
         }

         state = handle->state;
      }

      if (_IS_PLAYING(handle))
      {
         res = _aaxSoftwareMixerThreadUpdate(handle, handle->ringbuffer);

#if USE_PID
// Warning: enabling this code makes writing to audio files
//          as if it where batched mode (dt becomes 0.0f)
# if 0
         do
         {
            float target, input, err, P, I;

            target = be_handle->fill.aim;
            input = (float)_aaxDataGetDataAvail(be_handle->rawBuffer, 0)/freq;
            err = input - target;

            /* present error */
            P = err;

            /*  accumulation of past errors */
            be_handle->PID.I += err*delay_sec;
            I = be_handle->PID.I;

            err = 0.40f*P + 0.97f*I;
            dt = _MINMAX((delay_sec + err), 1e-6f, 1.5f*delay_sec);
# if 0
 printf("target: %8.1f, avail: %8.1f, err: %- 8.1f, delay: %5.4f (%5.4f)\r", target*freq, input*freq, err*freq, dt, delay_sec);
# endif
         }
         while (0);
# endif
#endif

         if (handle->batch_finished) { // batched mode
            _aaxSemaphoreRelease(handle->batch_finished);
         }
      }

      res = _aaxSignalWaitTimed(&handle->thread.signal, dt);
   }
   while (res == AAX_TIMEOUT || res == true);

   _aaxMutexUnLock(handle->thread.signal.mutex);

   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      be->destroy_ringbuffer(handle->ringbuffer);
      handle->ringbuffer = NULL;
   }

   return handle ? true : false;
}

static int
_aaxStreamDriverWriteThread(void *id)
{
   _driver_t *handle = (_driver_t*)id;

   _aaxMutexLock(handle->iothread.signal.mutex);

   do
   {
      _aaxSignalWait(&handle->iothread.signal);
      _aaxStreamDriverWriteChunk(id);
   }
   while(handle->iothread.started);

   do
   {
      size_t res = _aaxDataGetDataAvail(handle->rawBuffer, 0);
      _aaxDataMoveData(handle->rawBuffer, 0, handle->ioBuffer, 0, res);
   }
   while (_aaxStreamDriverWriteChunk(id));

   _aaxMutexUnLock(handle->iothread.signal.mutex);

   return handle ? true : false;
}

static ssize_t
_aaxStreamDriverReadChunk(const void *id)
{
   _driver_t *handle = (_driver_t*)id;
   _data_t *ioBuffer = handle->ioBuffer;
   size_t size;
   ssize_t res;

   _aaxMutexLock(handle->ioBufLock);
   size = _aaxDataGetFreeSpace(ioBuffer, 0);
   res = handle->io->read(handle->io, ioBuffer, size);
   _aaxMutexUnLock(handle->ioBufLock);

   if (res == -1) {
      handle->end_of_file = true;
   }

   return res;
}

static ssize_t
_aaxStreamDriverThreadReadChunk(const void *id)
{
   _driver_t *handle = (_driver_t*)id;
   _data_t *captureBuffer = handle->captureBuffer;
   ssize_t res = 0;
   size_t size;

   size = _aaxDataGetFreeSpace(captureBuffer, 0);
   if (size) {
      res = handle->io->read(handle->io, captureBuffer, size);
   }

   size = _aaxDataGetDataAvail(captureBuffer, 0);
   if (size)
   {
      // Move data from captureBuffer to ioBuffer
      if (!_aaxMutexLockTimed(handle->ioBufLock, handle->dt))
      {
         res = _aaxDataMoveData(captureBuffer, 0, handle->ioBuffer, 0, size);
         _aaxMutexUnLock(handle->ioBufLock);
      }
   }

   if (res == -1) {
      handle->end_of_file = true;
   }

   return res;
}

static int
_aaxStreamDriverReadThread(void *id)
{
   _driver_t *handle = (_driver_t*)id;
   ssize_t res;

   /* read (clear) all bytes already sent from the server */
   /* until the threshold is reached                      */
   if (handle->io->protocol != PROTOCOL_DIRECT)
   {
      do
      {
         _aaxDataSetOffset(handle->ioBuffer, 0, 0);
         res = _aaxStreamDriverThreadReadChunk(id);
      }
      while (res > IOBUF_THRESHOLD);
   }

   if (!handle->copy_to_buffer) {
      res = _aaxStreamDriverReadChunk(id);
   }

   // wait for our first job
   _aaxMutexLock(handle->iothread.signal.mutex);

   do {
      _aaxSignalWaitTimed(&handle->iothread.signal, handle->dt);
      res = _aaxStreamDriverReadChunk(id);
   }
   while(res >= 0 && handle->iothread.started);

   _aaxMutexUnLock(handle->iothread.signal.mutex);

   return handle ? true : false;
}
