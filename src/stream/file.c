/*
 * Copyright 2005-2013 by Erik Hofman.
 * Copyright 2009-2013 by Adalin B.V.
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
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <errno.h>		/* for ETIMEDOUT, errno */
#include <fcntl.h>		/* SEEK_*, O_* */
#include <assert.h>		/* assert */

#include <xml.h>

#include <base/types.h>
#include <base/threads.h>

#include <api.h>
#include <arch.h>

#include "file.h"
#include "filetype.h"
#include "audio.h"

#define BACKEND_NAME_OLD	"File"
#define BACKEND_NAME		"Audio Files"
#define DEFAULT_RENDERER	AAX_NAME_STR""

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
static _aaxDriverState _aaxFileDriverState;
static _aaxDriverParam _aaxFileDriverParam;

char _file_default_renderer[100] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxFileDriverBackend =
{
   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_file_default_renderer,

   (_aaxCodec **)&_oalRingBufferCodecs,

   (_aaxDriverDetect *)&_aaxFileDriverDetect,
   (_aaxDriverNewHandle *)&_aaxFileDriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxFileDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxFileDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxFileDriverGetName,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxFileDriverConnect,
   (_aaxDriverDisconnect *)&_aaxFileDriverDisconnect,
   (_aaxDriverSetup *)&_aaxFileDriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxFileDriverCapture,
   (_aaxDriverCallback *)&_aaxFileDriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxSoftwareDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxSoftwareDriver3dMixer,
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
   char sse_level;

   char *ptr, *scratch;
   unsigned int buf_len;

   struct threat_t thread;
   uint8_t buf[IOBUF_SIZE];
   unsigned int bufpos;
   unsigned int no_samples;

   _aaxFmtHandle* fmt;
   char *interfaces;

} _driver_t;

const char *default_renderer = BACKEND_NAME": /tmp/AeonWaveOut.wav";
static void* _aaxFileDriverWriteThread(void*);
static void* _aaxFileDriverReadThread(void*);

static int
_aaxFileDriverDetect(int mode)
{
   _aaxExtensionDetect* ftype;
   int i, rv = AAX_FALSE;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   i = 0;
   do
   {
      if ((ftype = _aaxFileTypes[i++]) != NULL)
      {
         _aaxFmtHandle* type = ftype();
         if (type)
         {
            rv = type->detect(mode);
            free(type);
            if (rv) break;
         }
      }
   }
   while (ftype);

   return rv;
}



static void *
_aaxFileDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));
   if (handle)
   {
      _aaxExtensionDetect *ftype = _aaxFileTypes[0];

      handle->mode = mode;
      handle->sse_level = _aaxGetSSELevel();
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

         if (!strcasecmp(renderer, "default")) {
            s = (char *)default_renderer;
         }
         else {
            s = _aax_strdup(renderer);
         }
      }
      else if (xid) {
         s = xmlNodeGetString(xid, "renderer");
      }

      if (s && (*s == '~'))
      {
         const char *home = userHomeDir();
         if (home)
         {
            int hlen = strlen(home);
            int slen = strlen(s);
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

      if (s)
      {
         char *ext = strrchr(s, '.');
         if (ext)
         {
            _aaxExtensionDetect* ftype;
            int i = 0;

            ext++;
            do
            {
               if ((ftype = _aaxFileTypes[i++]) != NULL)
               {
                  _aaxFmtHandle* type = ftype();
                  if (type && type->detect(mode) && type->supported(ext))
                  {
                     free(handle->fmt);
                     handle->fmt = type;
                     break;
                  }
                  free(type);
               }
            }
            while (ftype);
         }
      }

      if (handle->fmt)
      {
         handle->name = s;

         s = (char*)_aaxGetSIMDSupportString();
         snprintf(_file_default_renderer, 99, "%s %s", DEFAULT_RENDERER, s);

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
      unsigned int offs, size;
      void *buf = NULL;

      if (handle->thread.started)
      {
         handle->thread.started = AAX_FALSE;
         _aaxConditionSignal(handle->thread.condition);
         _aaxThreadJoin(handle->thread.ptr);
      }

      if (handle->thread.condition) {
         _aaxConditionDestroy(handle->thread.condition);
      }
      if (handle->thread.mutex) {
         _aaxMutexDestroy(handle->thread.mutex);
      }
      if (handle->thread.ptr) {
         _aaxThreadDestroy(handle->thread.ptr);
      }

      free(handle->ptr);
      if (handle->name && handle->name != default_renderer) {
         free(handle->name);
      }

      if (handle->fmt->update && handle->fmt->id) {
         buf = handle->fmt->update(handle->fmt->id, &offs, &size, AAX_TRUE);
      }
      if (buf)
      {
         lseek(handle->fd, offs, SEEK_SET);
         ret = write(handle->fd, buf, size);
      }
      handle->fmt->close(handle->fmt->id);
      close(handle->fd);

      free(handle->fmt);
      free(handle->interfaces);
      free(handle);
   }

   return ret;
}

static int
_aaxFileDriverSetup(const void *id, size_t *frames, int *fmt,
                        unsigned int *tracks, float *speed, int *bitrate)
{
   _driver_t *handle = (_driver_t *)id;
   int freq, rv = AAX_FALSE;
   unsigned int bufsize;
   float period_ms;

   assert(handle);

   handle->format = *fmt;
   handle->bits_sample = aaxGetBitsPerSample(*fmt);
   handle->frequency = *speed;
   freq = (int)handle->frequency;

   period_ms = cailf(1000.0f*(*frames)/(*speed));
   if (period_ms < 4.0f) period_ms = 4.0f;
   *frames = period_ms*(*speed)/1000.0f;

   handle->fmt->id = handle->fmt->setup(handle->mode, &bufsize, freq,
                                        *tracks, *fmt, *frames, *bitrate);
   if (handle->fmt->id)
   {
      handle->fd = open(handle->name, handle->fmode, 0644);
      if (handle->fd >= 0)
      {
         unsigned int no_samples = *frames;
         void *buf, *header = NULL;
         int res = AAX_TRUE;

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
            buf = handle->fmt->open(handle->fmt->id, header, &bufsize);
            res = bufsize;

            if (bufsize)
            {
               if (handle->mode != AAX_MODE_READ && buf) {
                  res = write(handle->fd, buf, bufsize);
               } else if (handle->mode == AAX_MODE_READ && !buf) {
                  res = lseek(handle->fd, bufsize, SEEK_SET);
               }
            }
         }
         while (handle->mode == AAX_MODE_READ && buf && bufsize);

         free(header);

         if (res == bufsize)
         {
            freq = handle->fmt->get_param(handle->fmt->id, __F_FREQ);

            handle->frequency = (float)freq;
            handle->format = handle->fmt->get_param(handle->fmt->id, __F_FMT);
            handle->no_channels = handle->fmt->get_param(handle->fmt->id, __F_TRACKS);

            *fmt = handle->format;
            *speed = handle->frequency;
            *tracks = handle->no_channels;
            *frames = no_samples;

            handle->latency = (float)_MAX(no_samples, (PERIOD_SIZE*8/(handle->no_channels*handle->bits_sample))) / (float)handle->frequency;

            handle->thread.ptr = _aaxThreadCreate();
            handle->thread.mutex = _aaxMutexCreate(handle->thread.mutex);
            handle->thread.condition = _aaxConditionCreate();
            if (handle->mode == AAX_MODE_READ) {
               res =_aaxThreadStart(handle->thread.ptr,
                                  _aaxFileDriverReadThread, handle, 20);
            } else {
               res =_aaxThreadStart(handle->thread.ptr,
                                  _aaxFileDriverWriteThread, handle, 20);
            }

            if (res == 0)
            {
               handle->thread.started = AAX_TRUE;
               rv = AAX_TRUE;
            }
         }
      }
      else
      {
//       _AAX_FILEDRVLOG("File: Unable to open the requested file");
         free(handle->fmt->id);
         handle->fmt->id = 0;
      }
   }
   else {
      _AAX_FILEDRVLOG("File: Unable to intiialize the handler");
   }

   return rv;
}

static int
_aaxFileDriverPlayback(const void *id, void *s, float pitch, float gain)
{
   _driver_t *handle = (_driver_t *)id;  
   _oalRingBuffer *rb = (_oalRingBuffer *)s;
   unsigned int bps, no_samples, offs, bufsize;
   unsigned int file_bps, file_tracks;
   _oalRingBufferSample *rbd;
   const int32_t** sbuf;
   char *scratch, *data;
   int res;

   assert(rb);
   assert(rb->sample);
   assert(id != 0);

   rbd = rb->sample;
   offs = _oalRingBufferGetParami(rb, RB_OFFSET_SAMPLES);
   no_samples = _oalRingBufferGetParami(rb, RB_NO_SAMPLES) - offs;
   bps = _oalRingBufferGetParami(rb, RB_BYTES_SAMPLE);

   file_bps = handle->fmt->get_param(handle->fmt->id, __F_BITS)/8;
   file_tracks = handle->fmt->get_param(handle->fmt->id, __F_TRACKS);
   assert(file_tracks == handle->no_channels);

   _aaxMutexLock(handle->thread.mutex);

   bufsize = no_samples * (file_tracks*_MAX(file_bps, bps));
   if ((handle->ptr == 0) || (handle->buf_len < bufsize))
   {
      char *p = 0;

      _aax_free(handle->ptr);
      handle->ptr = (char *)_aax_malloc(&p, 2*bufsize);
      handle->scratch = (char *)p;
      handle->buf_len = bufsize;
   }
   data = (char *)handle->scratch;
   scratch = data + bufsize;
   assert(bufsize <= handle->buf_len);

   if (fabs(gain - 1.0f) > 0.05f)
   {
      int t;
      for (t=0; t<file_tracks; t++) {
         _batch_mul_value(rbd->track[t]+offs, bps, no_samples, gain);
      }
   }

   sbuf = (const int32_t**)rbd->track;
   res = handle->fmt->cvt_to_intl(handle->fmt->id, data, sbuf,
                                  offs, file_tracks, no_samples,
                                  scratch, handle->buf_len);
   handle->no_samples = res;

   _aaxMutexUnLock(handle->thread.mutex);
   _aaxConditionSignal(handle->thread.condition);

   return (res >= 0) ? (res-res) : INT_MAX; // (res - no_samples);
}

static int
_aaxFileDriverCapture(const void *id, void **tracks, int offset, size_t *frames, void *scratch, size_t scratchlen, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   int bytes = 0;

   assert(*frames);

   if (frames && tracks)
   {
      int file_tracks = handle->fmt->get_param(handle->fmt->id, __F_TRACKS);
      int file_bits = handle->fmt->get_param(handle->fmt->id, __F_BITS);
      int file_block = handle->fmt->get_param(handle->fmt->id, __F_BLOCK);
      unsigned int frame_bits = file_tracks*file_bits;
      int32_t **sbuf = (int32_t**)tracks;
      unsigned int no_samples, bufsize;
      int res, samples, offs = offset;
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
            unsigned int res_bytes = res*frame_bits/8;

            offs += res;
            no_samples -= res;
            bytes += res_bytes;

            if (no_samples)
            {
               int ret;

               if (!file_block)
               {
                  bufsize = no_samples*frame_bits/8;
                  if (bufsize > scratchlen) {
                     bufsize = (scratchlen/frame_bits)*frame_bits;
                  }
               }

               /* more data is requested */
               data = scratch;

               _aaxMutexLock(handle->thread.mutex);
               ret = _MIN(handle->bufpos, bufsize);
               memcpy(data, handle->buf, ret);
               memcpy(handle->buf, handle->buf+ret, handle->bufpos-ret);
               handle->bufpos -= ret;
               _aaxMutexUnLock(handle->thread.mutex);

               _aaxConditionSignal(handle->thread.condition);

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
               _batch_mul_value((void*)(sbuf[t]+offset), sizeof(int32_t),
                                *frames, gain);
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
      unsigned int buflen;
      char *ptr;
      int i = 0;

      ptr = interfaces;
      buflen = 2048;

      do
      {
         if ((ftype = _aaxFileTypes[i++]) != NULL)
         {
            _aaxFmtHandle* type = ftype();
            if (type)
            {
               if (type->detect(mode))
               {
                  char *ifs = type->interfaces(mode);
                  unsigned int len = strlen(ifs);
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
      }
      while (ftype);
      
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
   int len = _MIN(strlen(str)+1, 256);

   memcpy(_errstr, str, len);
   _errstr[255] = '\0';  /* always null terminated */

   __aaxErrorSet(AAX_BACKEND_ERROR, (char*)&_errstr);
   _AAX_SYSLOG(_errstr);

   return (char*)&_errstr;
}

/*-------------------------------------------------------------------------- */

static void*
_aaxFileDriverWriteThread(void *id)
{
   _driver_t *handle = (_driver_t*)id;
   int res;

   _aaxThreadSetPriority(AAX_LOW_PRIORITY);

   _aaxMutexLock(handle->thread.mutex);
   do
   {
      unsigned int usize, file_bufsize;
      char *data;

      _aaxConditionWait(handle->thread.condition, handle->thread.mutex);
      res = handle->no_samples;
      data = (char*)handle->scratch;

      if (handle->fmt->cvt_from_signed) {
         handle->fmt->cvt_from_signed(handle->fmt->id, data, res);
      }
      if (handle->fmt->cvt_endianness) {
         handle->fmt->cvt_endianness(handle->fmt->id, data, res);
      }

      file_bufsize = res;
      do
      {
         usize = file_bufsize;

         if (handle->bufpos+usize >= IOBUF_SIZE) {
            usize = IOBUF_SIZE - handle->bufpos;
         }
         memcpy(handle->buf+handle->bufpos, data, usize);

         handle->bufpos += usize;
         if (handle->bufpos >= PERIOD_SIZE)
         {
            unsigned int wsize = (handle->bufpos/PERIOD_SIZE)*PERIOD_SIZE;

            res = write(handle->fd, handle->buf, wsize);
            if (res > 0)
            {
               void *buf;

               memcpy(handle->buf, handle->buf+res, handle->bufpos-res);
               handle->bufpos -= res;
               file_bufsize -= _MIN(res, usize);

               if (handle->fmt->update)
               {
                  unsigned int spos = 0;
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
      while (file_bufsize);
      handle->no_samples = 0;
   }
   while(handle->thread.started);

   res = write(handle->fd, handle->buf, handle->bufpos);
   _aaxMutexUnLock(handle->thread.mutex);

   return handle; 
}

static void*
_aaxFileDriverReadThread(void *id)
{
   _driver_t *handle = (_driver_t*)id;

   _aaxThreadSetPriority(AAX_LOW_PRIORITY);

   _aaxMutexLock(handle->thread.mutex);
   do
   {
      unsigned int size = IOBUF_SIZE - handle->bufpos;
      unsigned int res = read(handle->fd, handle->buf+handle->bufpos, size);
      if (res < 0) break;

      handle->bufpos += res;
      _aaxConditionWait(handle->thread.condition, handle->thread.mutex);
   }
   while(handle->thread.started);

   return handle;
}

static uint32_t _aaxDefaultWaveHeader[WAVE_EXT_HEADER_SIZE] =
{
    0x46464952,                 /*  0. "RIFF"                                */
    0x00000024,                 /*  1. (file_length - 8)                     */
    0x45564157,                 /*  2. "WAVE"                                */

    0x20746d66,                 /*  3. "fmt "                                */
    0x00000010,                 /*  4.                                       */
    0x00020001,                 /*  5. PCM & stereo                          */
    DEFAULT_OUTPUT_RATE,        /*  6.                                       */
    0x0001f400,                 /*  7. (sample_rate*channels*bits_sample/8)  */
    0x00100004,                 /*  8. (channels*bits_sample/8)              *
                                 *     & 16 bits per sample                  */
/* used for both the extensible data section and data section */
    0x61746164,                 /*  9. "data"                                */
    0,                          /* 10. length of the data block              *
                                 *     (sampels*channels*bits_sample/8)      */
    0,0,
/* data section starts here in case of the extensible format */
    0x61746164,                 /* 15. "data"                                */
    0
};

/**
 * Write a canonical WAVE file from memory to a file.
 *
 * @param a pointer to the exact ascii file location
 * @param no_samples number of samples per audio track
 * @param fs sample frequency of the audio tracks
 * @param no_tracks number of audio tracks in the buffer
 * @param format audio format
 */
void
_aaxFileDriverWrite(const char *file, enum aaxProcessingType type,
                          void *buffer, unsigned int no_samples,
                          unsigned int freq, char no_tracks,
                          enum aaxFormat format)
{
   uint32_t waveHeader[WAVE_EXT_HEADER_SIZE];
   unsigned int size;
   int fd, res, oflag;
   int fmt, bps;
   uint32_t s;
   off_t floc;

   switch (format) {
   case AAX_PCM8S:
   case AAX_PCM16S:
   case AAX_PCM24S:
   case AAX_PCM32S:
      fmt = 0x1;
      break;
   case AAX_FLOAT:
   case AAX_DOUBLE:
      fmt = 0x3;
      break;
   case AAX_ALAW:
      fmt = 0x6;
      break;
   case AAX_MULAW:
      fmt = 0x7;
      break;
   default:
      _AAX_FILEDRVLOG("File: unsupported format");
      return;
   }

   oflag = O_CREAT|O_WRONLY|O_BINARY;
   if (type == AAX_OVERWRITE) oflag |= O_TRUNC;
// if (type == AAX_APPEND) oflag |= O_APPEND;
   fd = open(file, oflag, 0644);
   if (fd < 0)
   {
      printf("Error: Unable to write to file.\n");
      return;
   }

   memcpy(waveHeader, _aaxDefaultWaveHeader, WAVE_EXT_HEADER_SIZE*4);

   bps = _oalRingBufferFormat[format].bits/8;

   floc = lseek(fd, 0L, SEEK_END);
   size = floc - WAVE_EXT_HEADER_SIZE*4;
   size += no_samples * no_tracks * bps;
   s = WAVE_HEADER_SIZE*4 - 8 + size;
   waveHeader[1] = s;

   s = (no_tracks << 16) | fmt;
   waveHeader[5] = s;

   s = freq;
   waveHeader[6] = s;

   s *= no_tracks * bps;
   waveHeader[7] = s;

   s = size;
   waveHeader[10] = s;

   if (is_bigendian())
   {
      waveHeader[1] = _bswap32(waveHeader[1]);
      waveHeader[5] = _bswap32(waveHeader[5]);
      waveHeader[6] = _bswap32(waveHeader[6]);
      waveHeader[7] = _bswap32(waveHeader[7]);
      waveHeader[10] = _bswap32(waveHeader[10]);
   }

   lseek(fd, 0L, SEEK_SET);
   res = write(fd, &waveHeader, WAVE_HEADER_SIZE*4);
   if (res == -1) {
      _AAX_FILEDRVLOG(strerror(errno));
   }

   if (is_bigendian()) {
      _batch_endianswap16((uint16_t*)buffer, no_samples*no_tracks);
   }

   if (type == AAX_APPEND) {
      lseek(fd, floc, SEEK_SET);
   }
   size = no_samples * no_tracks * bps;
   res = write(fd, buffer, size);
   if (res == -1) {
      _AAX_FILEDRVLOG(strerror(errno));
   }

   close(fd);
}

