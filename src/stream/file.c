/*
 * Copyright 2005-2015 by Erik Hofman.
 * Copyright 2009-2015 by Adalin B.V.
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

#ifdef HAVE_UNISTD_H
# include <unistd.h>           /* read, write, close, lseek */
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <sys/stat.h>
#include <ctype.h>		/* toupper */
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
#include "format.h"
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

#define USE_BATCHED_SEMAPHORE	0

static _aaxDriverDetect _aaxFileDriverDetect;
static _aaxDriverNewHandle _aaxFileDriverNewHandle;
static _aaxDriverGetDevices _aaxFileDriverGetDevices;
static _aaxDriverGetInterfaces _aaxFileDriverGetInterfaces;
static _aaxDriverConnect _aaxFileDriverConnect;
static _aaxDriverDisconnect _aaxFileDriverDisconnect;
static _aaxDriverSetup _aaxFileDriverSetup;
static _aaxDriverCaptureCallback _aaxFileDriverCapture;
static _aaxDriverPlaybackCallback _aaxFileDriverPlayback;
static _aaxDriverSetPosition _aaxFileDriverSetPosition;
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
   (_aaxDriverPlaybackCallback *)&_aaxFileDriverPlayback,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,
   (_aaxDriverSetPosition *)&_aaxFileDriverSetPosition,

   (_aaxDriverState *)&_aaxFileDriverState,
   (_aaxDriverParam *)&_aaxFileDriverParam,
   (_aaxDriverLog *)&_aaxFileDriverLog
};

typedef struct
{
   int fd;
   int fmode;
   char *name;
   char *station;
   char *description;
   char *artist;
   char *title;
   char *genre;
   char *website;
   char metadata_changed;

   uint8_t no_channels;
   uint8_t bits_sample;
   enum aaxFormat format;
   int mode;
   float latency;
   float frequency;
   size_t no_samples;
   size_t no_bytes;

   size_t meta_pos;
   size_t meta_interval;

   char *ptr, *scratch;
   size_t buf_len;

   struct threat_t thread;
#if USE_BATCHED_SEMAPHORE
   _aaxSemaphore *finished;
#endif
   uint8_t buf[IOBUF_SIZE];
   size_t bufpos;
   size_t bytes_avail;

   _aaxFmtHandle* fmt;
   char *interfaces;
   _aaxRenderer *render;

   void *out_header;
   size_t out_hdr_size;

   _io_t io;

} _driver_t;

const char *default_renderer = BACKEND_NAME": /tmp/AeonWaveOut.wav";
static _aaxFmtHandle* _aaxGetFormat(const char*, enum aaxRenderMode);
static void* _aaxFileDriverWriteThread(void*);
static void* _aaxFileDriverReadThread(void*);
static void _aaxFileDriverWriteChunk(const void*);
static ssize_t _aaxFileDriverReadChunk(const void*);
static const char *_get_json(const char*, const char*);

static int
_aaxFileDriverDetect(int mode)
{
   _aaxExtensionDetect* ftype;
   int i, rv = AAX_FALSE;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   i = 0;
   while ((ftype = _aaxFormatTypes[i++]) != NULL)
   {
      _aaxFmtHandle* type = ftype();
      if (type)
      {
         rv = type->detect(type, mode);
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
      _aaxExtensionDetect *ftype = _aaxFormatTypes[0];

      handle->fd = -1;
      handle->mode = mode;
      if (ftype)
      {
         _aaxFmtHandle* type = ftype();
         if (type && type->detect(type, mode))
          {
            handle->fmt = type;
            handle->io.open = open;
            handle->io.close = close;
            handle->io.read = read;
            handle->io.write = write;
            handle->io.seek = lseek;
            handle->io.stat = fstat;
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

      free(handle->fmt);
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
         if (buf && (handle && handle->io.protocol == PROTOCOL_FILE))
         {
            handle->io.seek(handle->fd, offs, SEEK_SET);
            ret = handle->io.write(handle->fd, buf, size);
         }
         handle->fmt->close(handle->fmt->id);
      }
      handle->io.close(handle->fd);

      if (handle->render)
      {
         handle->render->close(handle->render->id);
         free(handle->render);
      }

#if USE_BATCHED_SEMAPHORE
      if (handle->finished)
      {
         _aaxSemaphoreDestroy(handle->finished);
         handle->finished = NULL;
      }
#endif

      free(handle->station);
      free(handle->description);
      free(handle->artist);
      free(handle->title);
      free(handle->genre);
      free(handle->website);
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
      char m = (handle->mode == AAX_MODE_READ) ? 0 : 1;
      if (!m)
      {
         char *s, *protocol, *server, *path;
         int port;

         s = strdup(handle->name);
         handle->io.protocol = _url_split(s, &protocol, &server, &path, &port);
#if 0
 printf("name: '%s'\n", handle->name);
 printf("protocol: '%s'\n", protocol);
 printf("server: '%s'\n", server);
 printf("path: '%s'\n", path);
 printf("port: %i\n", port);
#endif
         switch (handle->io.protocol)
         {
         case PROTOCOL_HTTP:
            handle->io.open = _socket_open;
            handle->io.close = _socket_close;
            handle->io.read = _socket_read;
            handle->io.write = _socket_write;
            handle->io.seek = _socket_seek;
            handle->io.stat = _socket_stat;
            handle->fd = handle->io.open(server, O_RDWR, port);
            if (handle->fd >= 0)
            {
               int res = http_send_request(&handle->io, handle->fd, "GET",
                                           server, path, "Icy-MetaData:1",
                                           aaxGetVersionString((aaxConfig)id));
               if (res > 0)
               {
                  char buf[4096];
                  res = http_get_response(&handle->io, handle->fd, buf, 4096);
                  if (res == 200)
                  {
                     const char *s;

                     handle->fmt->set_param(handle->fmt->id, __F_IS_STREAM, 1);

                     s = _get_json(buf, "content-length");
                     if (s) handle->no_bytes = atoi(s);

                     s = _get_json(buf, "content-type");
                     if (s && !strcasecmp(s, "audio/mpeg"))
                     {
                        s = _get_json(buf, "icy-name");
                        if (s)
                        {
                           handle->artist = strdup(s);
                           handle->station = strdup(s);
                        }

                        s = _get_json(buf, "icy-description");
                        if (s)
                        {
                           handle->title = strdup(s);
                           handle->description = strdup(s);
                        }

                        s = _get_json(buf, "icy-genre");
                        if (s) handle->genre = strdup(s);

                        s = _get_json(buf, "icy-url");
                        if (s) handle->website = strdup(s);

                        s = _get_json(buf, "icy-metaint");
                        if (s)
                        {
                           handle->meta_interval = atoi(s);
                           handle->meta_pos = 0;
                        }
                     }
                  }
                  else
                  {
                     handle->io.close(handle->fd);
                     handle->fd = -1;
                     _aaxFileDriverLog(id, 0, 0, buf+strlen("HTTP/1.0 "));
                     free(handle->fmt->id);
                     handle->fmt->id = 0;
                  }
               }
            }
            break;
         case PROTOCOL_FILE:
            handle->fd = handle->io.open(path, handle->fmode, 0644);
            if (handle->fd < 0)
            {
               if (handle->mode != AAX_MODE_READ) {
                  _aaxFileDriverLog(id, 0, 0, "File already exists");
               } else {
                  _aaxFileDriverLog(id, 0, 0, "File read error");
               }
               free(handle->fmt->id);
               handle->fmt->id = 0;
            }
            break;
         default:
            break;
         }
         free(s);
      }

      if ((handle->fd >= 0) || m)
      {
         size_t no_samples = period_frames;
         void *header = NULL;
         void *buf = NULL;
         int res = AAX_TRUE;

         if (bufsize) {
            header = malloc(bufsize);
         }

         do
         {
            if (!m && header && bufsize)
            {
               res = handle->io.read(handle->fd, header, bufsize);
               if (res > 0)
               {
                  handle->meta_pos += res;
                  switch (handle->io.protocol)
                  {
                  case PROTOCOL_FILE:
                  {
                     struct stat st;
                     handle->io.stat(handle->fd, &st);
                     handle->no_bytes = st.st_size;
                     break;
                  }
                  case PROTOCOL_HTTP:
                  default:
                     break;
                  }
               }
               else {
                  break;
               }
            }

            bufsize = res;
            buf = handle->fmt->open(handle->fmt->id, header, &bufsize,
                                    handle->no_bytes);
            if (m && buf)
            {
               handle->out_header = malloc(bufsize);
               memcpy(handle->out_header, buf, bufsize);
               handle->out_hdr_size = bufsize;
               res = bufsize;
               buf = NULL;
            }
            else {
               res = bufsize;
            }
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
            handle->latency = (float)_MAX(no_samples,(PERIOD_SIZE*8/(handle->no_channels*handle->bits_sample))) / (float)handle->frequency;

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
            handle->io.close(handle->fd);
            handle->fd = -1;
            free(handle->fmt->id);
            handle->fmt->id = 0;
         }
      }
   }
   else {
      _aaxFileDriverLog(id, 0, 0, " Unable to intialize the file format");
   }

   return rv;
}

static size_t
_aaxFileDriverPlayback(const void *id, void *src, float pitch, float gain,
                       char batched)
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

   /*
    * Opening the  file for playback is delayed until actual playback to
    * prevent setting the file size to zero just bij running aaxinfo -d
    * which always opens a file in playback mode.
    */
   if (handle->fd < 0) {
      handle->fd = handle->io.open(handle->name, handle->fmode, 0644);
   }

   if (handle->out_header)
   {
      if (handle->fd)
      {
         res = handle->io.write(handle->fd, handle->out_header,
                                            handle->out_hdr_size);
         if (res == handle->out_hdr_size)
         {
            free(handle->out_header);
            handle->out_header = NULL;
         }
         else {
            return -1;
         }
      }
      else {
         return -1;
      }
   }

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

#if !USE_BATCHED_SEMAPHORE
   if (batched) {
      _aaxFileDriverWriteChunk(id);
   } else {
      _aaxSignalTrigger(&handle->thread.signal);
   }
#else
   _aaxSignalTrigger(&handle->thread.signal);
   if (batched)
   {
      if (!handle->finished) {
         handle->finished = _aaxSemaphoreCreate(0);
      }
      _aaxSemaphoreWait(handle->finished);
   }
#endif

   return (res >= 0) ? (res-res) : -1; // (res - no_samples);
}

static size_t
_aaxFileDriverCapture(const void *id, void **tracks, ssize_t *offset, size_t *frames, void *scratch, size_t scratchlen, float gain, char batched)
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
               memmove(handle->buf, handle->buf+ret, handle->bufpos-ret);
               handle->bufpos -= ret;

               _aaxMutexUnLock(handle->thread.signal.mutex);
#if !USE_BATCHED_SEMAPHORE
               if (batched) {
                  _aaxFileDriverReadChunk(id);
               } else {
                  _aaxSignalTrigger(&handle->thread.signal);
               }
#else
               _aaxSignalTrigger(&handle->thread.signal);
               if (batched)
               {
                  if (!handle->finished) {
                     handle->finished = _aaxSemaphoreCreate(0);
                  }
                  _aaxSemaphoreWait(handle->finished);
               }
#endif

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
_aaxFileDriverGetName(const void *id, int type)
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
            ret = _aax_strdup(handle->name);
         } else {
            ret = _aax_strdup("default");
         }
         break;
      case AAX_MUSIC_PERFORMER_STRING:
         ret = handle->fmt->name(handle->fmt->id, __F_ARTIST);
         if (!ret) ret = handle->artist;
         break;
      case AAX_TRACK_TITLE_STRING:
         ret = handle->fmt->name(handle->fmt->id, __F_TITLE);
         if (!ret) ret = handle->title;
         break;
      case AAX_MUSIC_GENRE_STRING:
         ret = handle->fmt->name(handle->fmt->id, __F_GENRE);
         if (!ret) ret = handle->genre;
         break;
      case AAX_TRACK_NUMBER_STRING:
         ret = handle->fmt->name(handle->fmt->id, __F_TRACKNO);
         break;
      case AAX_ALBUM_NAME_STRING:
         ret = handle->fmt->name(handle->fmt->id, __F_ALBUM);
         if (!ret) ret = handle->description;
         break;
      case AAX_RELEASE_DATE_STRING:
         ret = handle->fmt->name(handle->fmt->id, __F_DATE);
         break;
      case AAX_SONG_COMPOSER_STRING:
         ret = handle->fmt->name(handle->fmt->id, __F_COMPOSER);
         if (!ret) ret = handle->station;
         break;
      case AAX_SONG_COPYRIGHT_STRING:
         ret = handle->fmt->name(handle->fmt->id, __F_COPYRIGHT);
         break;
      case AAX_SONG_COMMENT_STRING:
         ret = handle->fmt->name(handle->fmt->id, __F_COMMENT);
         break;
      case AAX_ORIGINAL_PERFORMER_STRING:
         ret = handle->fmt->name(handle->fmt->id, __F_ORIGINAL);
         break;
      case AAX_WEBSITE_STRING:
         ret = handle->fmt->name(handle->fmt->id, __F_WEBSITE);
         if (!ret) ret = handle->website;
         break;
      case AAX_COVER_IMAGE_DATA:
         ret = handle->fmt->name(handle->fmt->id, __F_IMAGE);
         break;
      default:
         break;
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
      case DRIVER_SEEKABLE_SUPPORT:
         if (handle->fmt->get_param(handle->fmt->id, __F_POSITION) != 0) {
            rv =  (float)AAX_TRUE;
         }
         break;
      case DRIVER_TIMER_MODE:
      case DRIVER_BATCHED_MODE:
         rv = (float)AAX_TRUE;
         break;
      case DRIVER_SHARED_MODE:
      default:
         break;
      }
   }
   return rv;
}

static int
_aaxFileDriverSetPosition(const void *id, off_t pos)
{
// _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

// TODO: we probably need a protocol dependend way to set a new position
#if 0
   int res = handle->io.seek(handle->fd, 0, SEEK_CUR);
   if (res != pos)
   {
//    handle->bufpos = 0;
      res = handle->io.seek(handle->fd, 0, SEEK_SET);
      if (res >= 0)
      {
         int file_tracks = handle->fmt->get_param(handle->fmt->id, __F_TRACKS);
         int file_bits = handle->fmt->get_param(handle->fmt->id, __F_BITS);
         unsigned int samples = IOBUF_SIZE*8/(file_tracks*file_bits);
         ssize_t seek;
         while ((seek = handle->fmt->set_param(handle->fmt->id, __F_POSITION, pos))
                   == __F_PROCESS)
         {
            unsigned char sbuf[IOBUF_SIZE][_AAX_MAX_SPEAKERS];
            unsigned char buf[IOBUF_SIZE];

            res = handle->io.read(handle->fd, buf, IOBUF_SIZE);
            if (res <= 0) break;

            res = handle->fmt->cvt_from_intl(handle->fmt->id, (int32_ptrptr)sbuf,
                                             buf, 0, file_tracks, samples);
         }
      
         if ((seek >= 0) && (res >= 0)) {
            rv = (handle->io.seek(handle->fd, seek, SEEK_SET) >= 0) ? AAX_TRUE : AAX_FALSE;
         }
      }
   }
#endif
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

      while ((ftype = _aaxFormatTypes[i++]) != NULL)
      {
         _aaxFmtHandle* type = ftype();
         if (type)
         {
            if (type->detect(type, mode))
            {
               char *ifs = type->interfaces(mode);
               size_t len = ifs ? strlen(ifs) : 0;
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
   _driver_t *handle = (_driver_t *)id;
   static char _errstr[256];

   if (handle && handle->io.protocol == PROTOCOL_HTTP) {
       snprintf(_errstr, 256, "HTTP: %s\n", str);
   } else {
      snprintf(_errstr, 256, "File: %s\n", str);
   }
   _errstr[255] = '\0';  /* always null terminated */

   __aaxErrorSet(AAX_BACKEND_ERROR, (char*)&_errstr);
   _AAX_SYSLOG(_errstr);

   return (char*)&_errstr;
}

/*-------------------------------------------------------------------------- */

static const char*
_get_json(const char *haystack, const char *needle)
{
   static char buf[64];
   char *start, *end;
   size_t pos;

   buf[0] = '\0';
   start = strcasestr(haystack, needle);
   if (start)
   {
      size_t haystacklen;

      start += strlen(needle);
      pos = start - haystack;

      haystacklen = strlen(haystack);
      while ((pos++ < haystacklen) && (*start == ':' || *start == ' ')) {
         start++;
      }

      if (pos < haystacklen)
      {
         end = start;
         while ((pos++ < haystacklen) &&
                (*end != '\0' && *end != '\n' && *end != '\r')) {
            end++;
         }

         if ((end-start) > 63) {
            end = start + 63;
         }
         memcpy(buf, start, (end-start));
         buf[end-start] = '\0';
      }
   }

   return (buf[0] != '\0') ? buf : NULL;
}

static _aaxFmtHandle*
_aaxGetFormat(const char *url, enum aaxRenderMode mode)
{
   char *s, *protocol, *server, *path;
   _aaxFmtHandle *rv = NULL;
   int port, res;
   char *ext;

   if (!url) return rv;

   ext = NULL;
   s = strdup(url);
   res = _url_split(s, &protocol, &server, &path, &port);
   switch (res)
   {
   case PROTOCOL_HTTP:
      if (path) ext = strrchr(path, '.');
      if (!ext) ext = ".mp3";
      break;
   case PROTOCOL_FILE:
      if (path) ext = strrchr(path, '.');
      break;
   default:
      break;
   }

   if (ext)
   {
      _aaxExtensionDetect* ftype;
      int i = 0;

      ext++;
      while ((ftype = _aaxFormatTypes[i++]) != NULL)
      {
         _aaxFmtHandle* type = ftype();
         if (type && type->detect(type, mode) && type->supported(ext))
         {
            rv = type;
            break;
         }
         free(type);
      }
   }
   free(s);

   return rv;
}

static void
_aaxFileDriverWriteChunk(const void *id)
{
   _driver_t *handle = (_driver_t*)id;
   ssize_t buffer_avail, avail;
   size_t usize;
   char *data;
   int bits;

   bits = handle->bits_sample;

   data = (char*)handle->scratch;
   avail = handle->bytes_avail;

   if (handle->fmt->cvt_from_signed) {
      handle->fmt->cvt_from_signed(handle->fmt->id, data, avail*8/bits);
   }
   if (handle->fmt->cvt_endianness) {
      handle->fmt->cvt_endianness(handle->fmt->id, data, avail*8/bits);
   }

   buffer_avail = avail;
   do
   {
      usize = _MIN(buffer_avail, IOBUF_SIZE);
      if (handle->bufpos+usize <= IOBUF_SIZE)
      {
         memcpy((char*)handle->buf+handle->bufpos, data, usize);
         buffer_avail -= usize;
         data += usize;

         handle->bufpos += usize;
         if (handle->bufpos >= PERIOD_SIZE)
         {
            size_t wsize = (handle->bufpos/PERIOD_SIZE)*PERIOD_SIZE;
            ssize_t res;

            res = handle->io.write(handle->fd, handle->buf, wsize);
            if (res > 0)
            {
               void *buf;

               memmove(handle->buf, handle->buf+res, handle->bufpos-res);
               handle->bufpos -= res;

               if (handle->fmt->update)
               {
                  size_t spos = 0;
                  buf = handle->fmt->update(handle->fmt->id, &spos, &usize,
                                            AAX_FALSE);
                  if (buf)
                  {
// TODO: Can't do seek for PROTOCOL_HTTP
                     off_t floc = handle->io.seek(handle->fd, 0L, SEEK_CUR);
                     handle->io.seek(handle->fd, spos, SEEK_SET);
                     res = handle->io.write(handle->fd, buf, usize);
                     handle->io.seek(handle->fd, floc, SEEK_SET);
                  }
               }
            }
            else
            {
               _AAX_FILEDRVLOG(strerror(errno));
               break;
            }
         }
         else {
            break;
         }
      }
      else
      {
         _AAX_FILEDRVLOG("Internal buffer overflow");
         break;
      }
   }
   while (buffer_avail >= 0);
   handle->bytes_avail = 0;

}  

static void*
_aaxFileDriverWriteThread(void *id)
{
   _driver_t *handle = (_driver_t*)id;

   _aaxThreadSetPriority(handle->thread.ptr, AAX_LOW_PRIORITY);

   _aaxMutexLock(handle->thread.signal.mutex);
   do
   {
      _aaxSignalWait(&handle->thread.signal);

      if TEST_FOR_FALSE(handle->thread.started) {
         break;
      }

      if (!handle->bytes_avail) {
         continue;
      }
   
      _aaxFileDriverWriteChunk(id);

#if USE_BATCHED_SEMAPHORE
      if (handle->finished) {
         _aaxSemaphoreRelease(handle->finished);
      }
#endif
   }
   while(handle->thread.started);

   if (handle->bytes_avail) {
      _aaxFileDriverWriteChunk(id);
   }

   _aaxMutexUnLock(handle->thread.signal.mutex);

   return handle; 
}

static ssize_t
_aaxFileDriverReadChunk(const void *id)
{
   _driver_t *handle = (_driver_t*)id;
   size_t size = IOBUF_SIZE - handle->bufpos;
   ssize_t res;

   res = handle->io.read(handle->fd, handle->buf+handle->bufpos, size);
   if (res > 0)
   {
      handle->bufpos += res;

      if (handle->meta_interval)
      {
         handle->meta_pos += res;
         while ((int)handle->meta_pos >= (int)handle->meta_interval)
         {
            size_t offs = handle->meta_pos - handle->meta_interval;
            char *ptr = (char*)handle->buf;
            int slen, blen;

            ptr += handle->bufpos;
            ptr -= offs;

            slen = *ptr * 16;

            if ((ptr+slen) >= ((char*)handle->buf+IOBUF_SIZE)) break;

            if (slen)
            {
               char *artist = ptr+1 + strlen("StreamTitle='");
               if (artist)
               {
                  char *title = strstr(artist, " - ");
                  char *end = strchr(artist, '\'');
                  if (title)
                  {
                     *title = '\0';
                     title += strlen(" - ");
                  }
                  if (end) {
                     *end = '\0';
                  }

                  free(handle->artist);
                  free(handle->title);

                  if (artist && end) {
                     handle->artist = strdup(artist);
                  } else {
                     handle->artist = NULL;
                  }

                  if (title && end) {
                     handle->title = strdup(title);
                  } else {
                     handle->title = NULL;
                  }
                  handle->metadata_changed = AAX_TRUE;
               }
            }

            slen++;	// dd the slen-byte itself
            handle->meta_pos -= (handle->meta_interval+slen);

            /* move the rest of the buffer len-bytes back */
            handle->bufpos -= slen;
            blen = handle->bufpos;
            blen -= (ptr - (char*)handle->buf);
            memmove(ptr, ptr+slen, blen);
         }
      }
   }

   return res;
}

static void*
_aaxFileDriverReadThread(void *id)
{
   _driver_t *handle = (_driver_t*)id;

   _aaxThreadSetPriority(handle->thread.ptr, AAX_LOW_PRIORITY);

   _aaxMutexLock(handle->thread.signal.mutex);
   do
   {
      ssize_t res = _aaxFileDriverReadChunk(id);
      if (res < 0) break;

#if USE_BATCHED_SEMAPHORE
      if (handle->finished) {
         _aaxSemaphoreRelease(handle->finished);
      }
#endif
      _aaxSignalWait(&handle->thread.signal);
   }
   while(handle->thread.started);

   _aaxMutexUnLock(handle->thread.signal.mutex);

   return handle;
}

