/*
 * Copyright 2005-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_RMALLOC_H
# ifdef HAVE_UNISTD_H
#  include <unistd.h>		/* read, write, close, lseek */
# endif
# include <rmalloc.h>
#else
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
# ifdef HAVE_UNISTD_H
#  include <unistd.h>		/* read, write, close, lseek */
# endif
#include <string.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <sys/stat.h>
#include <errno.h>		/* for ETIMEDOUT, errno */
#include <fcntl.h>		/* SEEK_*, O_* */
#include <assert.h>		/* assert */

#include <xml.h>

#include <base/types.h>
#include <base/threads.h>
#include <base/logging.h>

#include <api.h>
#include <arch.h>

#include <software/renderer.h>
#include "file.h"
#include "filetype.h"
#include "audio.h"

#define BACKEND_NAME_OLD	"File"
#define BACKEND_NAME		"Audio Files"
#define DEFAULT_RENDERER	AAX_NAME_STR""
#define MAX_ID_STRLEN		64

#define PERIOD_SIZE		4096
#define IOBUF_SIZE		4*PERIOD_SIZE
#define DEFAULT_OUTPUT_RATE	22050
#define WAVE_HEADER_SIZE	11
#define WAVE_EXT_HEADER_SIZE	17
#ifndef O_BINARY
# define O_BINARY	0
#endif

static _aaxDriverDetect _aaxFileDriverDetect;
static _aaxDriverNewHandle _aaxFileDriverNewHandle;
static _aaxDriverGetDevices _aaxFileDriverGetDevices;
static _aaxDriverGetInterfaces _aaxFileDriverGetInterfaces;
static _aaxDriverConnect _aaxFileDriverConnect;
static _aaxDriverDisconnect _aaxFileDriverDisconnect;
static _aaxDriverSetup _aaxFileDriverSetup;
static _aaxDriverCaptureCallback _aaxFileDriverCapture;
static _aaxDriverCallback _aaxFileDriverPlayback;
static _aaxDriverGetName _aaxFileDriverGetName;
static _aaxDriverRender _aaxFileDriverRender;
static _aaxDriverState _aaxFileDriverState;
static _aaxDriverParam _aaxFileDriverParam;

static char _file_default_renderer[MAX_ID_STRLEN] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxFileDriverBackend =
{
   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_file_default_renderer,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxFileDriverDetect,
   (_aaxDriverNewHandle *)&_aaxFileDriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxFileDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxFileDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxFileDriverGetName,
   (_aaxDriverRender *)&_aaxFileDriverRender,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxFileDriverConnect,
   (_aaxDriverDisconnect *)&_aaxFileDriverDisconnect,
   (_aaxDriverSetup *)&_aaxFileDriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxFileDriverCapture,
   (_aaxDriverCallback *)&_aaxFileDriverPlayback,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)&_aaxFileDriverState,
   (_aaxDriverParam *)&_aaxFileDriverParam,
   (_aaxDriverLog *)&_aaxFileDriverLog
};

typedef struct
{
   int fd;
   int fmode;
   char *name;

   int mode;
   float latency;
   float frequency;
   enum aaxFormat format;
   uint8_t no_channels;
   uint8_t bits_sample;
   size_t no_samples;

   char *ptr, *scratch;
   size_t buf_len;

   struct threat_t thread;
   uint8_t buf[IOBUF_SIZE];
   size_t bufpos;
   size_t bytes_avail;

   _aaxFmtHandle* fmt;
   char *interfaces;
   _aaxRenderer *render;

} _driver_t;

const char *default_renderer = BACKEND_NAME": /tmp/AeonWaveOut.wav";
static _aaxFmtHandle* _aaxGetFormat(const char*, enum aaxRenderMode);
static void* _aaxFileDriverWriteThread(void*);
static void* _aaxFileDriverReadThread(void*);

static int
_aaxFileDriverDetect(int mode)
{
   _aaxExtensionDetect* ftype;
   int i, rv = AAX_FALSE;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   i = 0;
   while ((ftype = _aaxFileTypes[i++]) != NULL)
   {
      _aaxFmtHandle* type = ftype();
      if (type)
      {
         rv = type->detect(mode);
         free(type);
         if (rv) break;
      }
   }

   return rv;
}

static void *
_aaxFileDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));
   if (handle)
   {
      _aaxExtensionDetect *ftype = _aaxFileTypes[0];

      handle->fd = -1;
      handle->mode = mode;
      if (ftype)
      {
         _aaxFmtHandle* type = ftype();
         if (type && type->detect(mode)) {
            handle->fmt = type;
         }
         else
         {
            free(type);
            free(handle);
            handle = NULL;
         }
      }
   }

   return handle;
}

static void *
_aaxFileDriverConnect(const void *id, void *xid, const char *device, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *renderer = (char *)device;
   char *s = NULL;

   if (!renderer) {
      renderer = (char*)default_renderer;
   }

   if (xid || renderer)
   {
      s = (char *)default_renderer;
      if (renderer)
      {
         unsigned int devlenold = 5; /* strlen(BACKEND_NAME_OLD":"); */
         unsigned int devlen = 12;   /* strlen(BACKEND_NAME":");     */
         if (!strncasecmp(renderer, BACKEND_NAME":", devlen))
         {
            renderer += devlen;
            while (*renderer == ' ' && *renderer != '\0') renderer++;
         }
         else if (!strncasecmp(renderer, BACKEND_NAME_OLD":", devlenold))
         {
            renderer += devlenold;
            while (*renderer == ' ' && *renderer != '\0') renderer++;
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
      handle = _aaxFileDriverNewHandle(mode);
   }

   if (handle)
   {
      static const int _mode[] = {
         O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,
         O_RDONLY|O_BINARY
      };
      int m = (handle->mode > 0) ? 0 : 1;

      handle->fmode = _mode[m];
      handle->fmt = _aaxGetFormat(s, mode);
      if (handle->fmt)
      {
         handle->name = s;

         snprintf(_file_default_renderer, MAX_ID_STRLEN, "%s", DEFAULT_RENDERER);

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
         }
      }
      else
      {
         _aaxFileDriverLog(id, 0, 0, "Unsupported file format");
         _aaxFileDriverDisconnect(handle);
         handle = 0;
      }
   }

   return handle;
}

static int
_aaxFileDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int ret = AAX_TRUE;

   if (handle)
   {
      size_t offs, size;
      void *buf = NULL;

      if (handle->thread.started)
      {
         handle->thread.started = AAX_FALSE;
         _aaxSignalTrigger(&handle->thread.signal);
         _aaxThreadJoin(handle->thread.ptr);
      }

      _aaxSignalFree(&handle->thread.signal);
      if (handle->thread.ptr) {
         _aaxThreadDestroy(handle->thread.ptr);
      }

      free(handle->ptr);
      if (handle->name && handle->name != default_renderer) {
         free(handle->name);
      }

      if (handle->fmt)
      {
         if (handle->fmt->update && handle->fmt->id) {
            buf = handle->fmt->update(handle->fmt->id, &offs, &size, AAX_TRUE);
         }
         if (buf)
         {
            lseek(handle->fd, offs, SEEK_SET);
            ret = write(handle->fd, buf, size);
         }
         handle->fmt->close(handle->fmt->id);
      }
      close(handle->fd);

      if (handle->render) {
         handle->render->close(handle->render->id);
      }

      free(handle->fmt);
      free(handle->interfaces);
      free(handle);
   }

   return ret;
}

static int
_aaxFileDriverSetup(const void *id, float *refresh_rate, int *fmt,
                    unsigned int *tracks, float *speed, int *bitrate,
                    int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
   size_t bufsize, period_frames;
   int rate, rv = AAX_FALSE;
   float period_ms;

   assert(handle);

   handle->format = *fmt;
   handle->bits_sample = aaxGetBitsPerSample(*fmt);
   handle->frequency = *speed;
   rate = *speed;

   period_ms = 1000.0f / period_rate;
   if (period_ms < 4.0f) period_ms = 4.0f;
   period_rate = 1000.0f / period_ms;

   period_frames = (size_t)rintf(rate / period_rate);
   handle->fmt->id = handle->fmt->setup(handle->mode, &bufsize, rate,
                                        *tracks, *fmt, period_frames, *bitrate);
   if (handle->fmt->id)
   {
      handle->fd = open(handle->name, handle->fmode, 0644);
      if (handle->fd >= 0)
      {
         size_t no_samples = period_frames;
         void *header = NULL;
         void *buf = NULL;
         struct stat st;
         int res = AAX_TRUE;

         fstat(handle->fd, &st);

         if (bufsize) {
            header = malloc(bufsize);
         }

         do
         {
            if (handle->mode == AAX_MODE_READ && header && bufsize)
            {
               res = read(handle->fd, header, bufsize);
               if (res <= 0) break;
            }

            bufsize = res;
            buf = handle->fmt->open(handle->fmt->id, header, &bufsize, st.st_size);
            res = bufsize;

            if (buf)
            {
               if (handle->mode != AAX_MODE_READ && buf) {
                  res = write(handle->fd, buf, bufsize);
               } else if (handle->mode == AAX_MODE_READ && !buf) {
                  res = lseek(handle->fd, bufsize, SEEK_SET);
               }
            }
//          else { 
//             break;
//          }
         }
         while (handle->mode == AAX_MODE_READ && buf && bufsize);

         free(header);

         if (bufsize && res == bufsize)
         {
            rate = handle->fmt->get_param(handle->fmt->id, __F_FREQ);

            handle->frequency = (float)rate;
            handle->format = handle->fmt->get_param(handle->fmt->id, __F_FMT);
            handle->no_channels = handle->fmt->get_param(handle->fmt->id, __F_TRACKS);

            *fmt = handle->format;
            *speed = handle->frequency;
            *tracks = handle->no_channels;
            *refresh_rate = period_rate;

            handle->no_samples = no_samples;
            handle->latency = (float)_MAX(no_samples, (PERIOD_SIZE*8/(handle->no_channels*handle->bits_sample))) / (float)handle->frequency;

            handle->thread.ptr = _aaxThreadCreate();
            handle->thread.signal.mutex = _aaxMutexCreate(handle->thread.signal.mutex);
            _aaxSignalInit(&handle->thread.signal);
            if (handle->mode == AAX_MODE_READ) {
               res = _aaxThreadStart(handle->thread.ptr,
                                     _aaxFileDriverReadThread, handle, 20);
            } else {
               res = _aaxThreadStart(handle->thread.ptr,
                                     _aaxFileDriverWriteThread, handle, 20);
            }

            if (res == 0)
            {
               handle->thread.started = AAX_TRUE;
               handle->render = _aaxSoftwareInitRenderer(handle->latency,
                                                      handle->mode, registered);
               if (handle->render)
               {
                  const char *rstr = handle->render->info(handle->render->id);
                  snprintf(_file_default_renderer, MAX_ID_STRLEN, "%s %s",
                                                   DEFAULT_RENDERER, rstr);
               }
               rv = AAX_TRUE;
            }
            else {
               _aaxFileDriverLog(id, 0, 0, "Internal error: thread failed");
            }
         }
         else {
            _aaxFileDriverLog(id, 0, 0, "Incorrect header");
         }

         if (!rv)
         {
            close(handle->fd);
            handle->fd = -1;
            free(handle->fmt->id);
            handle->fmt->id = 0;
         }
      }
      else
      {
         _aaxFileDriverLog(id, 0, 0, "Unable to open the file");
         free(handle->fmt->id);
         handle->fmt->id = 0;
      }
   }
   else {
      _aaxFileDriverLog(id, 0, 0, " Unable to intialize the file format");
   }

   return rv;
}

static size_t
_aaxFileDriverPlayback(const void *id, void *src, float pitch, float gain)
{
   _driver_t *handle = (_driver_t *)id;  
   _aaxRingBuffer *rb = (_aaxRingBuffer *)src;
   size_t no_samples, offs, outbuf_size;
   unsigned int rb_bps, file_bps, file_tracks;
   int32_t** sbuf;
   char *scratch;
   ssize_t res;

   assert(rb);
   assert(id != 0);

   offs = rb->get_parami(rb, RB_OFFSET_SAMPLES);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES) - offs;
   rb_bps = rb->get_parami(rb, RB_BYTES_SAMPLE);

   file_bps = handle->fmt->get_param(handle->fmt->id, __F_BITS)/8;
   file_tracks = handle->fmt->get_param(handle->fmt->id, __F_TRACKS);
   assert(file_tracks == handle->no_channels);

   _aaxMutexLock(handle->thread.signal.mutex);

   outbuf_size = file_tracks * no_samples*_MAX(file_bps, rb_bps);
   if ((handle->ptr == 0) || (handle->buf_len < outbuf_size))
   {
      char *p = 0;

      _aax_free(handle->ptr);
      handle->ptr = (char *)_aax_malloc(&p, 2*outbuf_size);
      handle->scratch = (char *)p;
      handle->buf_len = outbuf_size;
   }
   scratch = (char*)handle->scratch + outbuf_size;

   assert(outbuf_size <= handle->buf_len);

   // NOTE: Need RB_RW in case it is used as a slaved file-backend
   //       See _aaxSoftwareMixerPlay
   sbuf = (int32_t**)rb->get_tracks_ptr(rb, RB_RW);
   if (fabs(gain - 1.0f) > 0.05f)
   {
      int t;
      for (t=0; t<file_tracks; t++) {
         _batch_imul_value(sbuf[t]+offs, sizeof(int32_t), no_samples, gain);
      }
   }
   res = handle->fmt->cvt_to_intl(handle->fmt->id, handle->scratch,
                                  (const int32_t**)sbuf, offs, file_tracks,
                                  no_samples, scratch, handle->buf_len);
   rb->release_tracks_ptr(rb);
   handle->bytes_avail = res;

   _aaxMutexUnLock(handle->thread.signal.mutex);
   _aaxSignalTrigger(&handle->thread.signal);

   return (res >= 0) ? (res-res) : -1; // (res - no_samples);
}

static size_t
_aaxFileDriverCapture(const void *id, void **tracks, ssize_t *offset, size_t *frames, void *scratch, size_t scratchlen, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   ssize_t offs = *offset;
   size_t bytes = 0;

   assert(*frames);

   *offset = 0;
   if (frames && tracks)
   {
      int file_tracks = handle->fmt->get_param(handle->fmt->id, __F_TRACKS);
      int file_bits = handle->fmt->get_param(handle->fmt->id, __F_BITS);
      int file_block = handle->fmt->get_param(handle->fmt->id, __F_BLOCK);
      unsigned int frame_bits = file_tracks*file_bits;
      int32_t **sbuf = (int32_t**)tracks;
      size_t no_samples, bufsize;
      ssize_t res, samples;
      void *data;

      no_samples = *frames;
      if (!file_block) {
         bufsize = no_samples*frame_bits/8;
      }
      else {
         bufsize = file_block;
      }

      if (bufsize > scratchlen) {
         bufsize = (scratchlen/frame_bits)*frame_bits;
      }

      bytes = 0;
      data = NULL;
      samples = no_samples;
      do
      {
         /* convert data still in the buffer */
         if (handle->fmt->cvt_endianness) {
            handle->fmt->cvt_endianness(handle->fmt->id, data, samples);
         }
         if (handle->fmt->cvt_to_signed) {
            handle->fmt->cvt_to_signed(handle->fmt->id, data, samples);
         }
         res = handle->fmt->cvt_from_intl(handle->fmt->id, sbuf, data,
                                          offs, file_tracks, samples);

         /* res holds the number of samples that are actually converted */
         /* or -2 if the next chunk can be processed                    */
         /* or -1 if an error occured, or end of file                   */
         if (res == __F_PROCESS)
         {
            data = NULL;
            samples = no_samples;
         }
         else if (samples > 0)
         {
            size_t res_bytes = res*frame_bits/8;

            offs += res;
            no_samples -= res;
            bytes += res_bytes;

            if (no_samples)
            {
               ssize_t ret;

               if (!file_block)
               {
                  bufsize = no_samples*frame_bits/8;
                  if (bufsize > scratchlen) {
                     bufsize = (scratchlen/frame_bits)*frame_bits;
                  }
               }

               /* more data is requested */
               data = scratch;

               _aaxMutexLock(handle->thread.signal.mutex);
               ret = _MIN(handle->bufpos, bufsize);
               memcpy(data, handle->buf, ret);
               memcpy(handle->buf, handle->buf+ret, handle->bufpos-ret);
               handle->bufpos -= ret;

               _aaxMutexUnLock(handle->thread.signal.mutex);
               _aaxSignalTrigger(&handle->thread.signal);

               if (ret <= 0)
               {
                  bytes = -1;
                  break;
               }
               samples = ret*8/frame_bits;
            }
         }
         else
         {
            bytes = -1;
            break;
         }
      } while (no_samples);
 
      if (bytes > 0)
      {
         /* gain is netagive for auto-gain mode */
         gain = fabsf(gain);
         if (fabs(gain - 1.0f) > 0.05f)
         {
            int t;
            for (t=0; t<file_tracks; t++) {
               _batch_imul_value(sbuf[t] + *offset, sizeof(int32_t), *frames, gain);
            }
         }
      }
   }

   return bytes;
}

static char *
_aaxFileDriverGetName(const void *id, int playback)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   if (handle)
   {
      if (handle->name) {
         ret = _aax_strdup(handle->name);
      } else {
         ret = _aax_strdup("default");
      }
   }

   return ret;
}

_aaxRenderer*
_aaxFileDriverRender(const void* config)
{
   _driver_t *handle = (_driver_t *)config;
   return handle->render;
}


static int
_aaxFileDriverState(const void *id, enum _aaxDriverState state)
{
   int rv = AAX_FALSE;
   switch(state)
   {
   case DRIVER_AVAILABLE:
   case DRIVER_PAUSE:
   case DRIVER_RESUME:
   case DRIVER_SUPPORTS_PLAYBACK:
   case DRIVER_SUPPORTS_CAPTURE:
      rv = AAX_TRUE;
      break;
   case DRIVER_SHARED_MIXER:
   case DRIVER_NEED_REINIT:
   default:
      break;
   }
   return rv;
}

static float
_aaxFileDriverParam(const void *id, enum _aaxDriverParam param)
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
      case DRIVER_MIN_PERIODS:
      case DRIVER_MAX_PERIODS:
         rv = 1.0f;
         break;
      case DRIVER_MAX_SAMPLES:
         rv = (float)handle->fmt->get_param(handle->fmt->id, __F_SAMPLES);
         if (rv == UINT_MAX) rv = AAX_FPINFINITE;
         break;
      case DRIVER_SAMPLE_DELAY:
         rv = (float)handle->no_samples;
         break;

		/* boolean */
      case DRIVER_TIMER_MODE:
          rv = (float)AAX_TRUE;
          break;
      case DRIVER_SHARED_MODE:
      default:
         break;
      }
   }
   return rv;
}

static char *
_aaxFileDriverGetDevices(const void *id, int mode)
{
   static const char *rd[2] = { BACKEND_NAME"\0\0", BACKEND_NAME"\0\0" };
   return (char *)rd[mode];
}

static char *
_aaxFileDriverGetInterfaces(const void *id, const char *devname, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *rv = NULL;

   if (handle && !handle->interfaces)
   {
      _aaxExtensionDetect* ftype;
      char interfaces[2048];
      size_t buflen;
      char *ptr;
      int i = 0;

      ptr = interfaces;
      buflen = 2048;

      while ((ftype = _aaxFileTypes[i++]) != NULL)
      {
         _aaxFmtHandle* type = ftype();
         if (type)
         {
            if (type->detect(mode))
            {
               char *ifs = type->interfaces(mode);
               size_t len = strlen(ifs);
               if (ifs && len)
               {
                  snprintf(ptr, buflen, "%s ", ifs);
                  buflen -= len+1;
                  ptr += len+1;
               }
            }
            free(type);
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
_aaxFileDriverLog(const void *id, int prio, int type, const char *str)
{
   static char _errstr[256];

   snprintf(_errstr, 256, "File: %s\n", str);
   _errstr[255] = '\0';  /* always null terminated */

   __aaxErrorSet(AAX_BACKEND_ERROR, (char*)&_errstr);
   _AAX_SYSLOG(_errstr);

   return (char*)&_errstr;
}

/*-------------------------------------------------------------------------- */

static _aaxFmtHandle*
_aaxGetFormat(const char *fname, enum aaxRenderMode mode)
{
   char *ext = strrchr(fname, '.');
   _aaxFmtHandle *rv = NULL;

   if (ext)
   {
      _aaxExtensionDetect* ftype;
      int i = 0;

      ext++;
      while ((ftype = _aaxFileTypes[i++]) != NULL)
      {
         _aaxFmtHandle* type = ftype();
         if (type && type->detect(mode) && type->supported(ext))
         {
            rv = type;
            break;
         }
         free(type);
      }
   }

   return rv;
}

static void*
_aaxFileDriverWriteThread(void *id)
{
   _driver_t *handle = (_driver_t*)id;
   ssize_t avail;
   int bits;

   _aaxThreadSetPriority(handle->thread.ptr, AAX_LOW_PRIORITY);

   _aaxMutexLock(handle->thread.signal.mutex);
   bits = handle->bits_sample;
   do
   {
      ssize_t buffer_avail;
      size_t usize;
      char *data;

      _aaxSignalWait(&handle->thread.signal);
      data = (char*)handle->scratch;
      avail = handle->bytes_avail;

      if (handle->fmt->cvt_from_signed) {
         handle->fmt->cvt_from_signed(handle->fmt->id, data, avail*8/bits);
      }
      if (handle->fmt->cvt_endianness) {
         handle->fmt->cvt_endianness(handle->fmt->id, data, avail*8/bits);
      }

      if TEST_FOR_FALSE(handle->thread.started) {
         break;
      }
      if (!avail) continue;

      buffer_avail = avail;
      do
      {
         usize = _MIN(buffer_avail, IOBUF_SIZE);
         memcpy((char*)handle->buf+handle->bufpos, data, usize);
         buffer_avail -= usize;
         data += usize;

         handle->bufpos += usize;
         if (handle->bufpos >= PERIOD_SIZE)
         {
            size_t wsize = (handle->bufpos/PERIOD_SIZE)*PERIOD_SIZE;
            ssize_t res;

            res = write(handle->fd, handle->buf, wsize);
            if (res > 0)
            {
               void *buf;

               memcpy(handle->buf, handle->buf+res, handle->bufpos-res);
               handle->bufpos -= res;

               if (handle->fmt->update)
               {
                  size_t spos = 0;
                  buf = handle->fmt->update(handle->fmt->id, &spos, &usize,
                                            AAX_FALSE);
                  if (buf)
                  {
                     off_t floc = lseek(handle->fd, 0L, SEEK_CUR);
                     lseek(handle->fd, spos, SEEK_SET);
                     res = write(handle->fd, buf, usize);
                     lseek(handle->fd, floc, SEEK_SET);
                  }
               }
            }
            else {
               break;
            }
         }
         else {
            break;
         }
      }
      while (buffer_avail >= 0);
      handle->bytes_avail = 0;
   }
   while(handle->thread.started);

   avail = write(handle->fd, handle->buf, handle->bufpos);
   handle->bufpos -= avail;
   _aaxMutexUnLock(handle->thread.signal.mutex);

   return handle; 
}

static void*
_aaxFileDriverReadThread(void *id)
{
   _driver_t *handle = (_driver_t*)id;

   _aaxThreadSetPriority(handle->thread.ptr, AAX_LOW_PRIORITY);

   _aaxMutexLock(handle->thread.signal.mutex);
   do
   {
      size_t size = IOBUF_SIZE - handle->bufpos;
      ssize_t res = read(handle->fd, handle->buf+handle->bufpos, size);
      if (res < 0) break;

      handle->bufpos += res;
      _aaxSignalWait(&handle->thread.signal);
   }
   while(handle->thread.started);

   _aaxMutexUnLock(handle->thread.signal.mutex);

   return handle;
}

#if 0
/**
 * Write a canonical WAVE file from memory to a file.
 *
 * @param a pointer to the exact ascii file location
 * @param no_samples number of samples per audio track
 * @param fs sample frequency of the audio tracks
 * @param no_tracks number of audio tracks in the buffer
 * @param format audio format
 */
#include <fcntl.h>		/* SEEK_*, O_* */
int
_aaxFileDriverWrite(const char *file, enum aaxProcessingType type,
                          const int32_t **sbuf, size_t no_samples,
                          unsigned int freq, char no_tracks,
                          enum aaxFormat format)
{
   _aaxFmtHandle *fmt = _aaxGetFormat(file);
   size_t rv = AAX_FALSE;
   if (fmt)
   {
      char *buf, *data, *scratch;
      size_t res, size, scratchlen;
      int mode, fd, oflag;
      unsigned int bits;
      off_t floc, offs;

      mode = AAX_MODE_WRITE_STEREO;
      fmt->id = fmt->setup(mode, &size, freq, no_tracks, format, no_samples, 256);
      if (!fmt->id)
      {
         printf("Error: Unable to setup the file stream handler,\n");
         return rv;
      }

      oflag = O_CREAT|O_WRONLY|O_BINARY;
      if (type == AAX_OVERWRITE) oflag |= O_TRUNC;
      else if (type == AAX_APPEND) oflag |= O_APPEND;

      fd = open(file, oflag, 0644);
      if (fd < 0)
      {
         printf("Error: Unable to write to file.\n");
         return rv;
      }

      floc = lseek(fd, 0L, SEEK_END);
      lseek(fd, 0L, SEEK_SET);

      buf = fmt->open(fmt->id, NULL, &size);
      if (buf && size)
      {
         res = write(fd, buf, size);
         if (res == -1) {
            _AAX_FILEDRVLOG(strerror(errno));
         }
      }

      if (type == AAX_APPEND) {
         lseek(fd, floc, SEEK_SET);
      }

      offs = 0;
      bits = fmt->get_param(fmt->id, __F_BITS);
      no_tracks = fmt->get_param(fmt->id, __F_TRACKS);
      size = (no_samples*no_tracks*bits)/8;

      scratchlen = (IOBUF_SIZE*no_tracks*sizeof(int32_t)*8)/bits;
      scratch = _aax_aligned_alloc16(scratchlen);

      data = _aax_aligned_alloc16(IOBUF_SIZE);
      do
      {
         ssize_t cvt = _MIN(size, IOBUF_SIZE)*8/(no_tracks*bits);

         /* returns the no. bytes that are ready for writing */
         res = fmt->cvt_to_intl(fmt->id, data, sbuf, offs, no_tracks, cvt,
                                scratch, scratchlen);
         size -= res;
         offs += cvt;

         if (fmt->cvt_from_signed) {
            fmt->cvt_from_signed(fmt->id, data, res*8/bits);
         }
         if (fmt->cvt_endianness) {
            fmt->cvt_endianness(fmt->id, data, res*8/bits);
         }

         res = write(fd, data, res);
      }
      while ((res > 0) && size);
      _aax_aligned_free(scratch);
      _aax_aligned_free(data);

      if (res >= 0 && fmt->update)
      {
         size_t offs;
         handle->io.write.no_samples = no_samples * no_tracks;
         void *buf = fmt->update(fmt->id, &offs, &size, AAX_TRUE);
         if (buf)
         {
            lseek(fd, offs, SEEK_SET);
            res = write(fd, buf, size);
         }
      }
      rv = (res >= 0) ? AAX_TRUE : AAX_FALSE;

      close(fd);
      fmt->close(fmt->id);
      free(fmt);
   }
   return rv;
}
#endif

