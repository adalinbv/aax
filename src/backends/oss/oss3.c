/*
 * SPDX-FileCopyrightText: Copyright © 2005-2024 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2024 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>		// exit
#if HAVE_SYS_UTSNAME_H
# include <sys/utsname.h>
#endif
#include <stdio.h>
#include <sys/stat.h>
#ifdef HAVE_IO_H
# include <io.h>
#endif
#include <fcntl.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_TIME_H
# include <time.h>
#endif
#include <errno.h>
#include <assert.h>
#include <stdarg.h>	// va_start
#if HAVE_STRINGS_H
# include <strings.h>
#endif

#include <aax/aax.h>
#include <xml.h>

#include <base/memory.h>
#include <base/types.h>
#include <base/logging.h>
#include <base/dlsym.h>

#include <ringbuffer.h>
#include <arch.h>
#include <api.h>

#include <dsp/effects.h>
#include <software/renderer.h>
#include "device.h"
#include "device.h"
#include "audio.h"

#define TIMER_BASED		true

#define MAX_NAME		40
#define MAX_DEVICES		16
#define DEFAULT_PERIODS		1
#define DEFAULT_DEVNAME		"default"
#define	DEFAULT_PCM_NAME	"/dev/dsp"
#define DEFAULT_MIXER_NAME	"/dev/mixer"
#define MAX_ID_STRLEN		96

#define OSS_VERSION_3_MAX	0x030999
#define DEFAULT_RENDERER	"OSS3"

static char *_aaxOSS3DriverLogVar(const void *, const char *, ...);

#define FILL_FACTOR		1.5f
#define DEFAULT_REFRESH		46.875f
#define _AAX_DRVLOG(a)		_aaxOSS3DriverLog(id, __LINE__, 0, a)

static _aaxDriverDetect _aaxOSS3DriverDetect;
static _aaxDriverNewHandle _aaxOSS3DriverNewHandle;
static _aaxDriverFreeHandle _aaxOSS3DriverFreeHandle;
static _aaxDriverGetDevices _aaxOSS3DriverGetDevices;
static _aaxDriverGetInterfaces _aaxOSS3DriverGetInterfaces;
static _aaxDriverConnect _aaxOSS3DriverConnect;
static _aaxDriverDisconnect _aaxOSS3DriverDisconnect;
static _aaxDriverSetup _aaxOSS3DriverSetup;
static _aaxDriverCaptureCallback _aaxOSS3DriverCapture;
static _aaxDriverPlaybackCallback _aaxOSS3DriverPlayback;
static _aaxDriverSetName _aaxOSS3DriverSetName;
static _aaxDriverGetName _aaxOSS3DriverGetName;
static _aaxDriverRender _aaxOSS3DriverRender;
static _aaxDriverThread _aaxOSS3DriverThread;
static _aaxDriverState _aaxOSS3DriverState;
static _aaxDriverParam _aaxOSS3DriverParam;
static _aaxDriverLog _aaxOSS3DriverLog;

static char _oss3_id_str[MAX_ID_STRLEN] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxOSS3DriverBackend =
{
   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_oss3_id_str,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxOSS3DriverDetect,
   (_aaxDriverNewHandle *)&_aaxOSS3DriverNewHandle,
   (_aaxDriverFreeHandle *)&_aaxOSS3DriverFreeHandle,
   (_aaxDriverGetDevices *)&_aaxOSS3DriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxOSS3DriverGetInterfaces,

   (_aaxDriverSetName *)&_aaxOSS3DriverSetName,
   (_aaxDriverGetName *)&_aaxOSS3DriverGetName,
   (_aaxDriverRender *)&_aaxOSS3DriverRender,
   (_aaxDriverThread *)&_aaxOSS3DriverThread,

   (_aaxDriverConnect *)&_aaxOSS3DriverConnect,
   (_aaxDriverDisconnect *)&_aaxOSS3DriverDisconnect,
   (_aaxDriverSetup *)&_aaxOSS3DriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxOSS3DriverCapture,
   (_aaxDriverPlaybackCallback *)&_aaxOSS3DriverPlayback,
   NULL,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,
   NULL,

   ( _aaxDriverGetSetSources*)_aaxSoftwareDriverGetSetSources,

   (_aaxDriverState *)&_aaxOSS3DriverState,
   (_aaxDriverParam *)&_aaxOSS3DriverParam,
   (_aaxDriverLog *)&_aaxOSS3DriverLog
};

typedef struct
{
   void *handle;
   char *name;
   _aaxRenderer *render;
   enum aaxRenderMode mode;

   char *pcm;
   int fd;

   size_t threshold;		/* sensor buffer threshold for padding */
   float padding;		/* for sensor clock drift correction   */

   float latency;
   float frequency_hz;
   float refresh_rate;
   unsigned int format;
   unsigned int no_tracks;
   ssize_t period_frames;
   int exclusive;
   char no_periods;
   char bits_sample;

   char use_timer;
   char prepared;
   char pause;

   void **ptr;
   char **scratch;
   ssize_t buf_len;

   char *ifname[2];

   _batch_cvt_to_intl_proc cvt_to_intl;

   struct {
      float I;
      float err;
   } PID;
   struct {
      float aim;
   } fill;

   /* initial values, reset them when exiting */
   int mixfd;
   long volumeCur, volumeMin, volumeMax;
   float volumeInit, hwgain;

   /* capabilities */
   unsigned int min_frequency;
   unsigned int max_frequency;
   unsigned int min_periods;
   unsigned int max_periods;
   unsigned int min_tracks;
   unsigned int max_tracks;

} _driver_t;

DECL_STATIC_FUNCTION(ioctl);
DECL_STATIC_FUNCTION(poll);

static int _oss3_open(_driver_t*);
static int _oss3_get_version(void);
static void _oss3_set_volume(_driver_t*, _aaxRingBuffer*, ssize_t, size_t, unsigned int, float);

static const char *_const_oss3_default_pcm = DEFAULT_PCM_NAME;
static const char *_const_oss3_default_mixer = DEFAULT_MIXER_NAME;
static const char *_const_oss3_default_name = DEFAULT_DEVNAME;

#ifndef O_NONBLOCK
# define O_NONBLOCK	0
#endif

static void *audio = NULL;

static int
_aaxOSS3DriverDetect(int mode)
{
   static int rv = false;
   char *error = 0;

   _AAX_LOG(LOG_DEBUG, __func__);

   if (TEST_FOR_FALSE(rv) && !audio)
   {
      audio = _aaxIsLibraryPresent(NULL, 0);
      _aaxGetSymError(0);
   }

   if (audio)
   {
      TIE_FUNCTION(ioctl);
      TIE_FUNCTION(poll);

      if (_oss3_get_version() <= OSS_VERSION_3_MAX)
      {
         struct stat buffer;
         if (stat(DEFAULT_PCM_NAME, &buffer) == 0)
         {
            error = _aaxGetSymError(0);
            if (!error && (access(DEFAULT_PCM_NAME, F_OK) != -1)) {
               rv = true;
            }
         }
      }
   }
   else {
      rv = false;
   }

   return rv;
}

static void *
_aaxOSS3DriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (handle)
   {
//    char m = (mode == AAX_MODE_READ) ? 0 : 1;
//    const char *name;

      handle->pcm = (char*)_const_oss3_default_pcm;
      handle->name = (char*)_const_oss3_default_name;
      handle->frequency_hz = (float)48000.0f;
      handle->bits_sample = 16;
      handle->no_tracks = 2;
      handle->min_tracks = 1;
      handle->max_tracks = _AAX_MAX_SPEAKERS;
      handle->min_frequency = _AAX_MIN_MIXER_FREQUENCY;
      handle->max_frequency = _AAX_MAX_MIXER_FREQUENCY;
      handle->no_periods = DEFAULT_PERIODS;
      handle->min_periods = 1;
      handle->max_periods = 4;
      handle->period_frames = handle->frequency_hz/(handle->no_tracks*handle->bits_sample/8);
      handle->buf_len = handle->no_tracks*handle->period_frames*handle->bits_sample/8;
     if (is_bigendian()) {
         handle->format = AFMT_S16_BE;
      } else {
         handle->format = AFMT_S16_LE;
      }
      handle->mode = mode;
      handle->exclusive = O_EXCL;
      handle->fd = -1;
      if (handle->mode != AAX_MODE_READ) { // Always interupt based for capture
         handle->use_timer = TIMER_BASED;
      }

      handle->cvt_to_intl = _batch_cvt16_intl_24;

      // default period size is for 25Hz
      handle->fill.aim = FILL_FACTOR * DEFAULT_REFRESH;
   }

   return handle;
}

static int
_aaxOSS3DriverFreeHandle(UNUSED(void *id))
{
   _aaxCloseLibrary(audio);
   audio = NULL;

   return true;
}

static void *
_aaxOSS3DriverConnect(void *config, const void *id, xmlId *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      id = handle = _aaxOSS3DriverNewHandle(mode);
   }

   if (handle)
   {
      if (xid)
      {
         float f;
         char *s;
         int i;

         s = getenv("AAX_USE_TIMER");
         if (s) {
            handle->use_timer = _aax_getbool(s);
         } else if (xmlNodeTest(xid, "timed")) {
            handle->use_timer = xmlNodeGetBool(xid, "timed");
         }
         if (handle->mode == AAX_MODE_READ) {
            handle->use_timer = false;
         }

         f = (float)xmlNodeGetDouble(xid, "frequency-hz");
         if (f)
         {
            if (f < (float)_AAX_MIN_MIXER_FREQUENCY)
            {
               _AAX_DRVLOG("sampel rate too small.");
               f = (float)_AAX_MIN_MIXER_FREQUENCY;
            }
            else if (f > (float)_AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_DRVLOG("sample rate too large.");
               f = (float)_AAX_MAX_MIXER_FREQUENCY;
            }
            handle->frequency_hz = f;
         }

         if (mode != AAX_MODE_READ)
         {
            i = xmlNodeGetInt(xid, "channels");
            if (i)
            {
               if (i < 1)
               {
                  _AAX_DRVLOG("no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_DRVLOG("no. tracks too great.");
                  i = _AAX_MAX_SPEAKERS;
               }
               handle->no_tracks = i;
            }
         }

         i = xmlNodeGetInt(xid, "bits-per-sample");
         if (i)
         {
            if (i != 16)
            {
               _AAX_DRVLOG("unsopported bits-per-sample");
               i = 16;
            }
         }

         i = xmlNodeGetInt(xid, "periods");
         if (i)
         {
            if (i < handle->min_periods)
            {
               _AAX_DRVLOG("no periods too small.");
               i = 1;
            }
            else if (i > handle->max_periods)
            {
               _AAX_DRVLOG("no. periods too great.");
               i = 16;
            }
            else {
               handle->no_periods = i;
            }
         }
      }

      if (renderer)
      {
         if (handle->name != _const_oss3_default_name) {
            free(handle->name);
         }
         handle->name = _aax_strdup(renderer);
      }
   }

   if (handle)
   {
//    char m = (handle->mode == AAX_MODE_READ) ? 0 : 1;
      {
         handle->handle = config;
         handle->fd = _oss3_open(handle);
         if (handle->fd < 0)
         {
            _aaxOSS3DriverLogVar(id, "open: %s", strerror(errno));
            free(handle);
            handle = NULL;
         }
      }
   }

   return (void *)handle;
}

static int
_aaxOSS3DriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = false;

   if (handle)
   {
      char *ifname;

      if (handle->fd >= 0) {
         close(handle->fd);
      }

      ifname = handle->ifname[handle->mode ? 1 : 0];
      if (ifname) free(ifname);

      if (handle->name != _const_oss3_default_name) {
         free(handle->name);
      }

      if (handle->pcm != _const_oss3_default_pcm) {
         free(handle->pcm);
      }

      if (handle->render)
      {
         handle->render->close(handle->render->id);
         free(handle->render);
      }

      if (handle->ptr) free(handle->ptr);
      free(handle);

      rv = true;
   }

   return rv;
}

static int
_aaxOSS3DriverSetup(const void *id, float *refresh_rate, int *fmt,
                     unsigned int *channels, float *speed, UNUSED(int *bitrate),
                     int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
// int m = (handle->mode == AAX_MODE_READ) ? 1 : 0;
   int rv = false;

   *fmt = AAX_PCM16S;

   if (handle->fd >= 0)
   {
      unsigned int tracks, rate, periods, format;
      size_t frame_sz, frag_size, period_frames;
      int res, fd = handle->fd;

      format = handle->format;
      periods = handle->no_periods;
      rate = (unsigned int)*speed;
      tracks = *channels;
      if (tracks > handle->no_tracks) {
         tracks = handle->no_tracks;
      }
      if (tracks > 2) {
         tracks = 2;
      }

      if (*refresh_rate > 100) {
         *refresh_rate = 100;
      }

      if (!registered) {
         period_frames = get_pow2((size_t)rintf(rate/(*refresh_rate)));
      } else {
         period_frames = get_pow2((size_t)rintf(rate/period_rate));
      }

      handle->latency = 1.0f / *refresh_rate;
      if (handle->latency < 0.010f) {
         handle->use_timer = false;
      }

      res = pioctl(fd, SNDCTL_DSP_SETFMT, &format);
      if (res >= 0)
      {
         handle->format = format;
         res = pioctl(fd, SNDCTL_DSP_CHANNELS, &tracks);
      }
      if (res >= 0) {
         res = pioctl(fd, SNDCTL_DSP_SPEED, &rate);
      }

      frame_sz = tracks*handle->bits_sample/8;
      frag_size = period_frames*frame_sz;
      if (res >= 0)
      {
         res = pioctl(handle->fd, SNDCTL_DSP_GETBLKSIZE, &frag_size);
         if (res >= 0) {
             period_frames = frag_size/(tracks*handle->bits_sample/8);
         }
      }

      handle->frequency_hz = rate;
      handle->no_tracks = tracks;
      handle->no_periods = periods;
      handle->period_frames = period_frames;
      handle->threshold = 5*period_frames/4;

      *speed = (float)rate;
      *channels = tracks;
      if (!registered) {
         *refresh_rate = rate*frame_sz/(float)period_frames;
      } else {
         *refresh_rate = period_rate;
      }
      handle->refresh_rate = *refresh_rate;

      handle->fill.aim = FILL_FACTOR*period_frames/rate;
      handle->latency = (float)handle->fill.aim/(float)(handle->no_tracks*handle->bits_sample/8);

      handle->render = _aaxSoftwareInitRenderer(handle->latency,
                                             handle->mode, registered);
      if (handle->render)
      {
         int version = _oss3_get_version();
         const char *rstr = handle->render->info(handle->render->id);
         char *os_name = "";
#if HAVE_SYS_UTSNAME_H
         struct utsname utsname;
         uname(&utsname);
         os_name = utsname.sysname;
#endif
         snprintf(_oss3_id_str, MAX_ID_STRLEN, "%s %x.%x.%x %s %s",
                              DEFAULT_RENDERER, (version>>16),
                              (version>>8 & 0xFF), (version & 0xFF),
                              os_name, rstr);
         rv = true;
      }
      else {
         _AAX_DRVLOG("unable to get the renderer");
      }
   }

   do
   {
      char str[255];

      _AAX_SYSLOG("driver settings:");

      if (handle->mode != AAX_MODE_READ) {
         snprintf(str,255,"  output renderer: '%s'", handle->name);
      } else {
         snprintf(str,255,"  input renderer: '%s'", handle->name);
      }
      _AAX_SYSLOG(str);
      snprintf(str,255, "  devname: '%s'", handle->pcm);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  refresh_rate: %5.0f hz", handle->refresh_rate);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  playback rate: %5.0f hz", handle->frequency_hz);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  buffer size: %zu bytes", handle->period_frames*handle->no_tracks*handle->bits_sample/8);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  latency: %3.2f ms",  1e3*handle->latency);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  no. periods: %i", handle->no_periods);
      _AAX_SYSLOG(str);
      snprintf(str,255,"  timer based: %s", handle->use_timer ? "yes" : "no");
      _AAX_SYSLOG(str);
      snprintf(str,255,"  channels: %i, bytes/sample: %i\n", handle->no_tracks, handle->bits_sample/8);
      _AAX_SYSLOG(str);
      _AAX_SYSLOG(str);

#if 0
 printf("driver settings:\n");
 if (handle->mode != AAX_MODE_READ) {
    printf("  output renderer: '%s'\n", handle->name);
 } else {
    printf("  input renderer: '%s'\n", handle->name);
 }
 printf("  pcm: '%s'\n", handle->pcm);
 printf("  refresh rate: %5.0f Hz\n", handle->refresh_rate);
 printf("  playback rate: %5.0f Hz\n", handle->frequency_hz);
 printf("  buffer size: %zu bytes\n", handle->period_frames*handle->no_tracks*handle->bits_sample/8);
 printf("  latency:  %5.2f ms\n", 1e3*handle->latency);
 printf("  no_periods: %i\n", handle->no_periods);
 printf("  timer based: %s\n",handle->use_timer?"yes":"no");
 printf("  channels: %i, bytes/sample: %i\n", handle->no_tracks, handle->bits_sample/8);
#endif
   }
   while (0);

   return rv;
}


static ssize_t
_aaxOSS3DriverCapture(const void *id, void **data, ssize_t *offset, size_t *req_frames, void *scratch, size_t scratchlen, float gain, UNUSED(char batched))
{
   _driver_t *handle = (_driver_t *)id;
   ssize_t offs = *offset;
   ssize_t init_offs = offs;
   ssize_t rv = false;

   assert(handle->mode == AAX_MODE_READ);

   *offset = 0;
   if ((req_frames == 0) || (data == 0))
      return rv;

   if (*req_frames)
   {
      unsigned int tracks = handle->no_tracks;
      size_t period_frames;
//    float diff;

      period_frames = *req_frames;

#if 0
      /* try to keep the buffer padding at the threshold level at all times */
      diff = (float)avail-(float)handle->threshold;
      handle->padding = (handle->padding + diff/(float)period_frames)/2;
      corr = _MINMAX(roundf(handle->padding), -1, 1);
      period_frames += corr;
      offs -= corr;
      *offset = -corr;
#endif
#if 0
if (corr)
 printf("avail: %4i (%4i), fetch: %6i\r", avail, handle->threshold, period_frames);
#endif

      if (*req_frames)
      {
         unsigned int frame_size = tracks*handle->bits_sample/8;
         size_t buflen;
         size_t res;

         buflen = _MIN(period_frames*frame_size, scratchlen);

         res = read(handle->fd, scratch, buflen);
         if (res > 0)
         {
            int32_t **sbuf = (int32_t**)data;

            res /= frame_size;

            _batch_cvt24_16_intl(sbuf, scratch, offs, tracks, res);

            *req_frames = _MAX(offs, 0);
            _oss3_set_volume(handle, NULL, init_offs, offs, tracks, gain);

            rv = true;
         }
         else {
            _AAX_SYSLOG(strerror(errno));
         }
      }
   }

   return rv;
}

static size_t
_aaxOSS3DriverPlayback(const void *id, void *s, UNUSED(float pitch), float gain, UNUSED(char batched))
{
   _aaxRingBuffer *rb = (_aaxRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   ssize_t offs, outbuf_size, period_frames;
   unsigned int no_tracks;
   const int32_t **sbuf;
   char *data;
   int res;

   assert(rb);
   assert(id != 0);

   if (handle->mode == AAX_MODE_READ)
      return 0;

   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
   period_frames = rb->get_parami(rb, RB_NO_SAMPLES);

   outbuf_size = no_tracks*period_frames*handle->bits_sample/8;
   if (handle->ptr == 0 || (handle->buf_len < outbuf_size))
   {
      char *p;

      if (handle->ptr) _aax_free(handle->ptr);
      handle->buf_len = outbuf_size;

      handle->ptr = (void**)_aax_malloc(&p, 0, outbuf_size);
      handle->scratch = (char**)p;
   }

   offs = rb->get_parami(rb, RB_OFFSET_SAMPLES);
   period_frames -= offs;

   _oss3_set_volume(handle, rb, offs, period_frames, no_tracks, gain);

   data = (char*)handle->scratch;
   sbuf = (const int32_t**)rb->get_tracks_ptr(rb, RB_READ);
// handle->cvt_to_intl(data, sbuf, offs, no_tracks, period_frames);
   _batch_cvt16_intl_24(data, sbuf, offs, no_tracks, period_frames);
   rb->release_tracks_ptr(rb);

   res = write(handle->fd, data, outbuf_size);
   if (res < 0)
   {
      char errstr[1024];
      snprintf(errstr, 1024, "oss: %s", strerror(errno));
      _AAX_SYSLOG(errstr);
   }
   else {
      outbuf_size -= res;
   }

   return outbuf_size;
}

static int
_aaxOSS3DriverSetName(const void *id, int type, const char *name)
{
   _driver_t *handle = (_driver_t *)id;
   int ret = false;
   if (handle)
   {
      switch (type)
      {
      default:
         break;
      }
   }
   return ret;
}

static char *
_aaxOSS3DriverGetName(const void *id, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   if (handle && handle->name && (mode < AAX_MODE_WRITE_MAX)) {
      ret = _aax_strdup(handle->name);
   }

   return ret;
}

_aaxRenderer*
_aaxOSS3DriverRender(const void* config)
{
   _driver_t *handle = (_driver_t *)config;
   return handle->render;
}

static int
_aaxOSS3DriverState(const void *id, enum _aaxDriverState state)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = false;

   switch(state)
   {
   case DRIVER_PAUSE:
      if (handle && !handle->pause)
      {
         handle->pause = 1;
         close(handle->fd);
         handle->fd = -1;
         rv = true;
      }
      break;
   case DRIVER_RESUME:
      if (handle && handle->pause)
      {
         handle->pause = 0;
         handle->fd = _oss3_open(handle);
         if (handle->fd >= 0) {
            rv = true;
         }
      }
      break;
   case DRIVER_AVAILABLE:
      rv = true;
      break;
   case DRIVER_SHARED_MIXER:
      rv = handle->exclusive ? false : true;
      break;
   case DRIVER_SUPPORTS_PLAYBACK:
   case DRIVER_SUPPORTS_CAPTURE:
      rv = true;
      break;
   case DRIVER_NEED_REINIT:
   default:
      break;
   }

   return rv;
}

static float
_aaxOSS3DriverParam(const void *id, enum _aaxDriverParam param)
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
         rv = 1.0f; // handle->hwgain;
         break;
      case DRIVER_REFRESHRATE:
         rv = handle->refresh_rate;
         break;

		/* int */
      case DRIVER_MIN_FREQUENCY:
         rv = (float)handle->min_frequency;
         break;
      case DRIVER_MAX_FREQUENCY:
         rv = (float)handle->max_frequency;
         break;
      case DRIVER_MIN_TRACKS:
         rv = (float)handle->min_tracks;
         break;
      case DRIVER_MAX_TRACKS:
         rv = (float)handle->max_tracks;
         break;
      case DRIVER_MIN_PERIODS:
         rv = (float)handle->min_periods;
         break;
      case DRIVER_MAX_PERIODS:
         rv = (float)handle->max_periods;
         break;
      case DRIVER_MAX_SOURCES:
         rv = ((_handle_t*)(handle->handle))->backend.ptr->getset_sources(0, 0);
         break;
      case DRIVER_MAX_SAMPLES:
         rv = AAX_FPINFINITE;
         break;
      case DRIVER_SAMPLE_DELAY:
      {
         unsigned int avail;
         int err = pioctl(handle->fd, SNDCTL_DSP_GETODELAY, &avail);
         if (err >= 0) rv = (float)avail;
         break;
      }

		/* boolean */
      case DRIVER_TIMER_MODE:
         rv = (float)true;
         break;
      case DRIVER_BATCHED_MODE:
      case DRIVER_SHARED_MODE:
      default:
         break;
      }
   }
   return rv;
}

static char *
_aaxOSS3DriverGetDevices(UNUSED(const void *id), int mode)
{
   static char names[2][1024] = { "\0\0", "\0\0" };
   static time_t t_previous[2] = { 0, 0 };
   unsigned char m = (mode == AAX_MODE_READ) ? 0 : 1;
   time_t t_now;

   t_now = time(NULL);
   if (t_now > (t_previous[m]+5))
   {
      _driver_t *handle = (_driver_t*)id;
      int fd;

      t_previous[m] = t_now;

      if (handle && handle->mixfd >= 0) {
         fd = handle->mixfd;
      }
      else {
         fd = open(_const_oss3_default_mixer, O_RDWR);
      }

      if (fd >= 0)
      {
         if (!handle)
         {
            close(fd);
            fd = -1;
         }
      }
   }

   return (char *)&names[m];
}

static char *
_aaxOSS3DriverGetInterfaces(const void *id, const char *devname, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned char m = (mode == AAX_MODE_READ) ? 0 : 1;
   char *rv = handle ? handle->ifname[m] : NULL;

   return rv;
}

static char *
_aaxOSS3DriverLogVar(const void *id, const char *fmt, ...)
{
   char _errstr[1024];
   va_list ap;

   _errstr[0] = '\0';
   va_start(ap, fmt);
   vsnprintf(_errstr, 1024, fmt, ap);

   // Whatever happen in vsnprintf, what i'll do is just to null terminate it
   _errstr[1023] = '\0';
   va_end(ap);

   return _aaxOSS3DriverLog(id, 0, -1, _errstr);
}

static char *
_aaxOSS3DriverLog(const void *id, UNUSED(int prio), UNUSED(int type), const char *str)
{
   _driver_t *handle = (_driver_t *)id;
   static char _errstr[256];

   snprintf(_errstr, 256, DEFAULT_RENDERER": %s\n", str);
   _errstr[255] = '\0';  /* always null terminated */

   __aaxDriverErrorSet(handle->handle, AAX_BACKEND_ERROR, (char*)&_errstr);
   _AAX_SYSLOG(_errstr);
#ifndef NDEBUG
   printf("%s", _errstr);
#endif

   return (char*)&_errstr;
}

static void
_oss3_set_volume(UNUSED(_driver_t *handle), _aaxRingBuffer *rb, ssize_t offset, size_t period_frames, UNUSED(unsigned int no_tracks), float volume)
{
   float gain = fabsf(volume);

   /* software volume fallback */
   if (rb && fabsf(gain - 1.0f) > LEVEL_32DB) {
      rb->data_multiply(rb, offset, period_frames, gain);
   }
}


/* -------------------------------------------------------------------------- */

static int
_oss3_open(_driver_t *handle)
{
   int fd;

   fd = open(handle->pcm, handle->mode|handle->exclusive);
   if (fd >= 0)
   {
      unsigned int param;
      int err, frag;

      frag = log2i(handle->period_frames);
      frag |= DEFAULT_PERIODS << 16;
      pioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag);

      param = handle->format;
      err = pioctl(fd, SNDCTL_DSP_SETFMT, &param);
      if (err >= 0)
      {
         param = handle->no_tracks;
         err = pioctl(fd, SNDCTL_DSP_CHANNELS, &param);
      }
      if (err >= 0)
      {
         param = (unsigned int)handle->frequency_hz;
         err = pioctl(fd, SNDCTL_DSP_SPEED, &param);
      }
   }
   return fd;
}


static int
_oss3_get_version(void)
{
   static int version = -1;

   if (version < 0)
   {
      int fd = open(_const_oss3_default_pcm, O_WRONLY);  /* open /dev/dsp */
      if (fd >= 0)
      {
         if (pioctl(fd, OSS_GETVERSION, &version) < 0) {
            version = 0;
         }
         close(fd);
         fd = -1;
      }
   }
   return version;
}

int
_aaxOSS3DriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   size_t period_frames; // bufsize;
   _intBufferData *dptr_sensor;
   const _aaxDriverBackend *be;
   _aaxRingBuffer *dest_rb;
   _aaxAudioFrame *mixer;
   _driver_t *be_handle;
   unsigned int wait_us;
   float delay_sec;
   const void *id;
   int stdby_time;
   char state;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return false;
   }

   delay_sec = 1.0f/handle->info->period_rate;

   be = handle->backend.ptr;
   id = handle->backend.handle;		// Required for _AAX_DRVLOG
   be_handle = (_driver_t *)handle->backend.handle;

   dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _sensor_t* sensor = _intBufGetDataPtr(dptr_sensor);

      mixer = sensor->mixer;

      dest_rb = be->get_ringbuffer(MAX_EFFECTS_TIME, mixer->info->mode);
      if (dest_rb)
      {
         dest_rb->set_format(dest_rb, AAX_PCM24S, true);
         dest_rb->set_parami(dest_rb, RB_NO_TRACKS, mixer->info->no_tracks);
         dest_rb->set_paramf(dest_rb, RB_FREQUENCY, mixer->info->frequency);
         dest_rb->set_paramf(dest_rb, RB_DURATION_SEC, delay_sec);
         dest_rb->init(dest_rb, true);
         dest_rb->set_state(dest_rb, RB_STARTED);

         handle->ringbuffer = dest_rb;
      }
      _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

      if (!dest_rb) {
         return false;
      }
   }
   else {
      return false;
   }

   be->state(handle->backend.handle, DRIVER_PAUSE);
   state = AAX_SUSPENDED;

   be_handle = (_driver_t *)handle->backend.handle;
   if (be_handle->use_timer) {
      _aaxProcessSetPriority(-20);
   }

// bufsize = be_handle->no_periods*be_handle->period_frames;

   wait_us = delay_sec*1000000;
   period_frames = dest_rb->get_parami(dest_rb, RB_NO_SAMPLES);
   stdby_time = (int)(delay_sec*1000);
   _aaxMutexLock(handle->thread.signal.mutex);
   while TEST_FOR_TRUE(handle->thread.started)
   {
      size_t avail;
      int ret;

      _aaxMutexUnLock(handle->thread.signal.mutex);

#if 0
      avail = be_handle->status->hw_ptr + bufsize;
      avail -= be_handle->control->appl_ptr;
#else
      avail = period_frames;
#endif
      if (avail < period_frames)
      {
         if (_IS_PLAYING(handle))
         {
            // TIMER_BASED
            if (be_handle->use_timer) {
               usecSleep(wait_us);
            }
				/* timeout is in ms */
            else
            {
               struct pollfd pfd;
               pfd.fd = be_handle->fd;
               pfd.events = POLLOUT|POLLERR|POLLNVAL;
               do
               {
                  errno = 0;
                  ret = ppoll(&pfd, 1, 2*stdby_time);
                  if (ret <= 0) break;
                  if (errno == -EINTR) continue;
                  if (pfd.revents & (POLLERR|POLLNVAL))
                  {
                     _AAX_DRVLOG("snd_pcm_wait polling error");
#if 0
                     switch(be_handle->status->state)
                     {
                     case SND_PCM_STATE_XRUN:
                         be_handle->prepared = false;
                         break;
                      case SND_PCM_STATE_SUSPENDED:
                      case SND_PCM_STATE_DISCONNECTED:
                         break;
                      default:
                         break;
                     }
#endif
                  }
               }
               while (!(pfd.revents & (POLLIN|POLLOUT)));
               if (ret < 0) {
                  _aaxOSS3DriverLogVar(id, "poll: %s\n", strerror(errno));
               }
            }
         }
         else {
            msecSleep(stdby_time);
         }
      }
      _aaxMutexLock(handle->thread.signal.mutex);
      if TEST_FOR_FALSE(handle->thread.started) {
         break;
      }

      if (be->state(be_handle, DRIVER_AVAILABLE) == false) {
         _SET_PROCESSED(handle);
      }

      if (state != handle->state)
      {
         if (_IS_PAUSED(handle) ||
             (!_IS_PLAYING(handle) && _IS_STANDBY(handle))) {
            be->state(handle->backend.handle, DRIVER_PAUSE);
         }
         else if (_IS_PLAYING(handle) || _IS_STANDBY(handle)) {
            be->state(handle->backend.handle, DRIVER_RESUME);
         }
         state = handle->state;
      }

      if (_IS_PLAYING(handle))
      {
         int res = _aaxSoftwareMixerThreadUpdate(handle, dest_rb);

         if (be_handle->use_timer)
         {
            float target, input, err, P, I; //, D;
            float freq = mixer->info->frequency;

            target = be_handle->fill.aim;
            input = (float)res/freq;
            err = input - target;

            /* present error */
            P = err;

            /*  accumulation of past errors */
            be_handle->PID.I += err*delay_sec;
            I = be_handle->PID.I;

            /* prediction of future errors, based on current rate of change */
//          D = (be_handle->PID.err - err)/delay_sec;
//          be_handle->PID.err = err;

//          err = 0.45f*P + 0.83f*I + 0.00125f*D;
            err = 0.40f*P + 0.97f*I;
            wait_us = _MAX((delay_sec + err), 1e-6f) * 1000000.0f;
#if 0
 printf("target: %5.1f, res: %i, err: %- 5.1f, delay: %5.2f\n", target*freq, res, err*freq, be_handle->fill.dt*1000.0f);
#endif
         }
      }
#if 0
 printf("state: %i, paused: %i\n", state, _IS_PAUSED(handle));
 printf("playing: %i, standby: %i\n", _IS_PLAYING(handle), _IS_STANDBY(handle));
#endif

      if (handle->batch_finished) { // batched mode
         _aaxSemaphoreRelease(handle->batch_finished);
      }
   }

   handle->ringbuffer = NULL;
   be->destroy_ringbuffer(dest_rb);
   _aaxMutexUnLock(handle->thread.signal.mutex);

   return handle ? true : false;
}
