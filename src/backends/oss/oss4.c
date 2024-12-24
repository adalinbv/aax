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
#include <string.h>
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
#define MAX_DEVNODE		16
#define MAX_DEVICES		16
#define DEFAULT_PERIODS		1
#define DEFAULT_DEVNUM		0
#define DEFAULT_DEVNAME		"default"
#define	DEFAULT_PCM_NAME	"/dev/dsp0"
#define DEFAULT_MIXER_NAME	"/dev/mixer0"
#define MAX_ID_STRLEN		96

#define OSS_VERSION_4		0x040000
#define DEFAULT_RENDERER	"OSS4"

static char *_aaxOSS4DriverLogVar(const void *, const char *, ...);

#define FILL_FACTOR		1.5f
#define DEFAULT_REFRESH		46.875f
#define _AAX_DRVLOG(a)		_aaxOSS4DriverLog(id, __LINE__, 0, a)
#define HW_VOLUME_SUPPORT(a)	((a->mixfd >= 0) && a->volumeMax)

static _aaxDriverDetect _aaxOSS4DriverDetect;
static _aaxDriverNewHandle _aaxOSS4DriverNewHandle;
static _aaxDriverFreeHandle _aaxOSS4DriverFreeHandle;
static _aaxDriverGetDevices _aaxOSS4DriverGetDevices;
static _aaxDriverGetInterfaces _aaxOSS4DriverGetInterfaces;
static _aaxDriverConnect _aaxOSS4DriverConnect;
static _aaxDriverDisconnect _aaxOSS4DriverDisconnect;
static _aaxDriverSetup _aaxOSS4DriverSetup;
static _aaxDriverCaptureCallback _aaxOSS4DriverCapture;
static _aaxDriverPlaybackCallback _aaxOSS4DriverPlayback;
static _aaxDriverSetName _aaxOSS4DriverSetName;
static _aaxDriverGetName _aaxOSS4DriverGetName;
static _aaxDriverRender _aaxOSS4DriverRender;
static _aaxDriverThread _aaxOSS4DriverThread;
static _aaxDriverState _aaxOSS4DriverState;
static _aaxDriverParam _aaxOSS4DriverParam;
static _aaxDriverLog _aaxOSS4DriverLog;

static char _oss4_id_str[MAX_ID_STRLEN] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxOSS4DriverBackend =
{
   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_oss4_id_str,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxOSS4DriverDetect,
   (_aaxDriverNewHandle *)&_aaxOSS4DriverNewHandle,
   (_aaxDriverFreeHandle *)&_aaxOSS4DriverFreeHandle,
   (_aaxDriverGetDevices *)&_aaxOSS4DriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxOSS4DriverGetInterfaces,

   (_aaxDriverSetName *)&_aaxOSS4DriverSetName,
   (_aaxDriverGetName *)&_aaxOSS4DriverGetName,
   (_aaxDriverRender *)&_aaxOSS4DriverRender,
   (_aaxDriverThread *)&_aaxOSS4DriverThread,

   (_aaxDriverConnect *)&_aaxOSS4DriverConnect,
   (_aaxDriverDisconnect *)&_aaxOSS4DriverDisconnect,
   (_aaxDriverSetup *)&_aaxOSS4DriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxOSS4DriverCapture,
   (_aaxDriverPlaybackCallback *)&_aaxOSS4DriverPlayback,
   NULL,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,
   NULL,

   ( _aaxDriverGetSetSources*)_aaxSoftwareDriverGetSetSources,

   (_aaxDriverState *)&_aaxOSS4DriverState,
   (_aaxDriverParam *)&_aaxOSS4DriverParam,
   (_aaxDriverLog *)&_aaxOSS4DriverLog
};

typedef struct
{
   void *handle;
   char *name;
   _aaxRenderer *render;
   enum aaxRenderMode mode;

   char pcm[MAX_DEVNODE]; // "/dev/dspXX\0"
   int fd;

   size_t threshold;		/* sensor buffer threshold for padding */
   float padding;		/* for sensor clock drift correction   */

   float latency;
   float frequency_hz;
   float refresh_rate;
   unsigned int format;
   unsigned int no_tracks;
   ssize_t period_frames;
   ssize_t fragsize;
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
   int nodenum;
// int setup;

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

static int _oss4_open(_driver_t*);
static int _oss4_get_version(void);
static int _oss4_get_volume(_driver_t*);
static void _oss4_set_volume(_driver_t*, _aaxRingBuffer*, ssize_t, size_t, unsigned int, float);
static int _oss4_detect_devnode(_driver_t*, char);
static int _oss4_detect_nodenum(const char*);

static int _const_oss4_default_nodenum = DEFAULT_DEVNUM;
static const char *_const_oss4_default_pcm = DEFAULT_PCM_NAME;
static const char *_const_oss4_default_mixer = DEFAULT_MIXER_NAME;
static const char *_const_oss4_default_name = DEFAULT_DEVNAME;

#ifndef O_NONBLOCK
# define O_NONBLOCK	0
#endif

static void *audio = NULL;

static int
_aaxOSS4DriverDetect(int mode)
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

      if (_oss4_get_version() >= OSS_VERSION_4)
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
_aaxOSS4DriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (handle)
   {
//    char m = (mode == AAX_MODE_READ) ? 0 : 1;
//    const char *name;

      snprintf(handle->pcm , MAX_DEVNODE, "%s", _const_oss4_default_pcm);
      handle->name = (char*)_const_oss4_default_name;
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
_aaxOSS4DriverFreeHandle(UNUSED(void *id))
{
   _aaxCloseLibrary(audio);
   audio = NULL;

   return true;
}

static void *
_aaxOSS4DriverConnect(void *config, const void *id, xmlId *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      id = handle = _aaxOSS4DriverNewHandle(mode);
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

         if (xmlNodeTest(xid, "shared")) {
            handle->exclusive = xmlNodeGetBool(xid, "shared") ? O_EXCL : 0;
         }

         s = xmlAttributeGetString(xid, "name");
         if (s)
         {
            handle->nodenum = _oss4_detect_nodenum(s);
            if (handle->name != _const_oss4_default_name) {
               free(handle->name);
            }
            handle->name = _aax_strdup(s);
            xmlFree(s);
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
         handle->nodenum = _oss4_detect_nodenum(renderer);
         if (handle->name != _const_oss4_default_name) {
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
         handle->fd = _oss4_open(handle);
         if (handle->fd < 0)
         {
            _aaxOSS4DriverLogVar(id, "open: %s", strerror(errno));
            free(handle);
            handle = NULL;
         }
      }
   }

   return (void *)handle;
}

static int
_aaxOSS4DriverDisconnect(void *id)
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

      if (handle->name != _const_oss4_default_name) {
         free(handle->name);
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
_aaxOSS4DriverSetup(const void *id, float *refresh_rate, int *fmt,
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
      audio_buf_info info;
      int enable = 0;
      int delay;

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

      /* disable sample conversion */
      res = pioctl(fd, SNDCTL_DSP_COOKEDMODE, &enable);

      _oss4_get_volume(handle);

      info.fragsize = 0;
      if ((res >= 0) && (handle->mode == O_WRONLY)) {
         res = pioctl(fd, SNDCTL_DSP_GETOSPACE, &info);
      }

      if (res >= 0)
      {
         oss_audioinfo ainfo;

         handle->fragsize = info.fragsize;

         period_frames = info.fragsize/(tracks*handle->bits_sample/8);
         period_rate = (float)rate/period_frames;
         *refresh_rate = period_rate;

         ainfo.dev = handle->nodenum;
         res = pioctl(handle->fd, SNDCTL_AUDIOINFO_EX, &ainfo);
         if (res >= 0)
         {
            handle->min_tracks = ainfo.min_channels;
            handle->max_tracks = ainfo.max_channels;
            handle->min_frequency = ainfo.min_rate;
            handle->max_frequency = ainfo.max_rate;
         }
      }

      handle->latency = 0.0f;
      res = pioctl(fd, SNDCTL_DSP_GETODELAY, &delay);
      if (res >= 0)
      {
         handle->latency = (float)delay;
         handle->latency /= (float)(rate*tracks*handle->bits_sample/8);
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
         int version = _oss4_get_version();
         const char *rstr = handle->render->info(handle->render->id);
         char *os_name = "";
#if HAVE_SYS_UTSNAME_H
         struct utsname utsname;
         uname(&utsname);
         os_name = utsname.sysname;
#endif
         snprintf(_oss4_id_str, MAX_ID_STRLEN, "%s %x.%x.%x %s %s",
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
_aaxOSS4DriverCapture(const void *id, void **data, ssize_t *offset, size_t *req_frames, void *scratch, size_t scratchlen, float gain, UNUSED(char batched))
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
            _oss4_set_volume(handle, NULL, init_offs, offs, tracks, gain);

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
_aaxOSS4DriverPlayback(const void *id, void *s, UNUSED(float pitch), float gain, UNUSED(char batched))
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

   _oss4_set_volume(handle, rb, offs, period_frames, no_tracks, gain);

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
_aaxOSS4DriverSetName(const void *id, int type, const char *name)
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
_aaxOSS4DriverGetName(const void *id, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   if (handle && handle->name && (mode < AAX_MODE_WRITE_MAX)) {
      ret = _aax_strdup(handle->name);
   }

   return ret;
}

_aaxRenderer*
_aaxOSS4DriverRender(const void* config)
{
   _driver_t *handle = (_driver_t *)config;
   return handle->render;
}

static int
_aaxOSS4DriverState(const void *id, enum _aaxDriverState state)
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
         handle->fd = _oss4_open(handle);
         if (handle->fd >= 0) {
            rv = true;
         }
      }
      break;
   case DRIVER_AVAILABLE:
      if (handle)
      {
         oss_audioinfo ainfo;
         int err;

         ainfo.dev = handle->nodenum;
         err = pioctl(handle->fd, SNDCTL_AUDIOINFO_EX, &ainfo);
         if (err >= 0 && ainfo.enabled) {
            rv = true;
         }
      }
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
_aaxOSS4DriverParam(const void *id, enum _aaxDriverParam param)
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
_aaxOSS4DriverGetDevices(UNUSED(const void *id), int mode)
{
   static char names[2][1024] = { "\0\0", "\0\0" };
   static time_t t_previous[2] = { 0, 0 };
   unsigned char m = (mode == AAX_MODE_READ) ? 0 : 1;
   time_t t_now;

   t_now = time(NULL);
   if (t_now > (t_previous[m]+5))
   {
      int fd;

      t_previous[m] = t_now;

      fd = open(_const_oss4_default_mixer, O_RDWR);
      if (fd >= 0)
      {
         oss_sysinfo info;
         int len = 1024;
         char *ptr;

         ptr = (char *)&names[m];
         *ptr = 0; *(ptr+1) = 0;

         if (pioctl(fd, SNDCTL_SYSINFO, &info) >= 0)
         {
            int i;
            for (i=0; i<info.numaudios; ++i)
            {
               oss_card_info cinfo;
               size_t slen;

               cinfo.card = i;
               if (pioctl(fd, SNDCTL_CARDINFO, &cinfo) < 0) continue;

               slen = strlen(cinfo.shortname)+strlen(cinfo.longname)+2;
               if (slen > (len-1)) break;

               snprintf(ptr, len, "%s %s", cinfo.shortname, cinfo.longname);
               len -= slen;
               ptr += slen;
            }
         }
         close(fd);
      }

      /* always end with "\0\0" no matter what */
      names[m][1022] = 0;
      names[m][1023] = 0;
   }

   return (char *)&names[m];
}

static char *
_aaxOSS4DriverGetInterfaces(const void *id, const char *devname, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned char m = (mode == AAX_MODE_READ) ? 0 : 1;
   char *rv = handle ? handle->ifname[m] : NULL;

   if (!rv)
   {
      char devlist[1024] = "\0\0";
      char *ptr = devlist;
      size_t len = 1024;
      int fd;

      fd = open(_const_oss4_default_mixer, O_RDWR);
      if (fd >= 0)
      {
         oss_sysinfo info;
         if (pioctl(fd, SNDCTL_SYSINFO, &info) >= 0)
         {
            int i;
            for (i=0; i<info.numaudios; ++i)
            {
               oss_audioinfo ainfo;
               unsigned int slen;

               ainfo.dev = i;
               if (pioctl(fd, SNDCTL_AUDIOINFO_EX, &ainfo) < 0) continue;

               if (!ainfo.enabled) continue;
               if (ainfo.caps & PCM_CAP_VIRTUAL) continue;
               if (((ainfo.caps & PCM_CAP_OUTPUT) && !m) ||
                   ((ainfo.caps & PCM_CAP_INPUT) && m)) continue;

               if (!strncmp(ainfo.name, devname, strlen(devname)))
               {
                  slen = strlen(ainfo.name)-strlen(devname);
                  if (slen > (len-1)) break;

                  snprintf(ptr, len, "%s", ainfo.name+strlen(devname)+1);
                  len -= slen;
                  ptr += slen;
#if 0
 printf("    name: '%s'\n", ainfo.name);
 printf("    caps: %x\n", ainfo.caps);
 printf("    min_rate: %i, max_rate: %i\n", ainfo.min_rate, ainfo.max_rate);
 printf("    min_channels: %i, max_channels: %i\n", ainfo.min_channels, ainfo.max_channels);
 printf("    latency: %i\n", ainfo.latency);
 printf("    devnode: '%s'\n", ainfo.devnode);
#endif
               }
            }

            if (ptr != devlist)
            {
               *ptr++ = '\0';
               if (handle->ifname[m]) {
                  rv = realloc(handle->ifname[m], ptr-devlist);
               } else {
                  rv = malloc(ptr-devlist);
               }
               if (rv)
               {
                  handle->ifname[m] = rv;
                  memcpy(handle->ifname[m], devlist, ptr-devlist);
               }
            }
         }
      }
   }

   return rv;
}

static char *
_aaxOSS4DriverLogVar(const void *id, const char *fmt, ...)
{
   char _errstr[1024];
   va_list ap;

   _errstr[0] = '\0';
   va_start(ap, fmt);
   vsnprintf(_errstr, 1024, fmt, ap);

   // Whatever happen in vsnprintf, what i'll do is just to null terminate it
   _errstr[1023] = '\0';
   va_end(ap);

   return _aaxOSS4DriverLog(id, 0, -1, _errstr);
}

static char *
_aaxOSS4DriverLog(const void *id, UNUSED(int prio), UNUSED(int type), const char *str)
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

/* -------------------------------------------------------------------------- */

static int
_oss4_open(_driver_t *handle)
{
   int fd, m = handle->mode;

   _oss4_detect_devnode(handle, m);
   fd = open(handle->pcm, handle->mode|handle->exclusive);
   if (fd >= 0)
   {
      unsigned int param;
      int err, frag;
      int enable = 0;

      err = pioctl(fd, SNDCTL_DSP_COOKEDMODE, &enable);

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
_oss4_get_version(void)
{
   static int version = -1;

   if (version < 0)
   {
      int fd = open(_const_oss4_default_pcm, O_WRONLY);  /* open /dev/dsp0 */
      if (fd >= 0)
      {
         oss_sysinfo info;
         if (pioctl(fd, SNDCTL_SYSINFO, &info) >= 0) {
            version = info.versionnum;
         }
         else
         {
            int err = pioctl(fd, OSS_GETVERSION, &version);
#if __FreeBSD__
            if (err == EINVAL) version = SOUND_VERSION;
#else
            if (err < 0) version = 0;
#endif
         }
         close(fd);
         fd = -1;
      }
   }
   return version;
}

static int
_oss4_get_volume(_driver_t *handle)
{
   int rv = 0;

   if (handle && handle->fd >= 0)
   {
      int vlr = -1;

      handle->volumeMax = 100;

      errno = 0;
      if (handle->mode == O_RDONLY) {
         rv = pioctl(handle->fd, SNDCTL_DSP_GETRECVOL, &vlr);
      } else {
         rv = pioctl(handle->fd, SNDCTL_DSP_GETPLAYVOL, &vlr);
      }
      handle->volumeCur = ((vlr & 0xFF) + ((vlr >> 8) & 0xFF))/2;
      handle->volumeInit = (float)handle->volumeCur/(float)handle->volumeMax;

      handle->volumeMin = 0;
      if (rv == EINVAL) {
         handle->volumeMax = 0;
      }
   }
   return rv;
}


static void
_oss4_set_volume(UNUSED(_driver_t *handle), _aaxRingBuffer *rb, ssize_t offset, size_t period_frames, UNUSED(unsigned int no_tracks), float volume)
{
   float gain = fabsf(volume);
   float hwgain = gain;

   if (handle && HW_VOLUME_SUPPORT(handle))
   {
      long volume;

      hwgain = _MINMAX(gain, handle->volumeMin, handle->volumeMax);
      volume = (hwgain * handle->volumeMax);

      /*
       * Slowly adjust volume to dampen volume slider movement.
       * If the volume step is large, don't dampen it.
       * volume is negative for auto-gain mode.
       */
      if ((volume < 0.0f) && (handle->mode == O_RDONLY))
      {
         float dt = GMATH_E1*period_frames/handle->frequency_hz;
         float rr = _MINMAX(dt/5.0f, 0.0f, 1.0f);       /* 10 sec average */

         /* Quickly adjust for a very large step in volume */
         if (labs(volume - handle->volumeCur) > 82) rr = 0.9f;

         hwgain = (1.0f-rr)*handle->hwgain + (rr)*hwgain;
         handle->hwgain = hwgain;
      }

      volume = (hwgain * handle->volumeMax);
      if (volume != handle->volumeCur)
      {
         int vlr = volume | (volume << 8);
         int rv;

         handle->volumeCur = volume;

         if (handle->mode == O_RDONLY) {
            rv = pioctl(handle->fd, SNDCTL_DSP_SETRECVOL, &vlr);
         } else {
            rv = pioctl(handle->fd, SNDCTL_DSP_SETPLAYVOL, &vlr);
         }
         if (rv < 0) volume = handle->volumeMax;
      }

      hwgain = (float)volume/handle->volumeMax;
      if (hwgain) gain /= hwgain;
      else gain = 0.0f;
   }

   /* software volume fallback */
   if (rb && fabsf(hwgain - gain) > LEVEL_32DB) {
      rb->data_multiply(rb, offset, period_frames, gain, 1.0f);
   }
}

static int
_oss4_detect_devnode(_driver_t *handle, UNUSED(char mode))
{
   int fd, rv = true;
   snprintf(handle->pcm, MAX_DEVNODE, "/dev/dsp%i", handle->nodenum);

   return rv;
}

static int
_oss4_detect_nodenum(const char *devname)
{
   static int slen = strlen("/dev/dsp");
   int rv = _const_oss4_default_nodenum;

   if (devname && !strncasecmp(devname, "/dev/dsp", slen) ) {
      rv = strtol(devname+slen, NULL, 10);
   }
   else if (devname && strcasecmp(devname, "OSS4") &&
                       strcasecmp(devname, DEFAULT_DEVNAME))
   {
      rv = strtol(devname+strlen("pcm"), NULL, 10);
   }

   return rv;
}

int
_aaxOSS4DriverThread(void* config)
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
                  _aaxOSS4DriverLogVar(id, "poll: %s\n", strerror(errno));
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
