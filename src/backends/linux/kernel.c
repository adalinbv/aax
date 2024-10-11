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
#include "kernel.h"
#include "device.h"
#include "audio.h"

#define TIMER_BASED		true

#define MAX_NAME		40
#define MAX_DEVICES		16
#define DEFAULT_PERIODS		2
#define DEFAULT_DEVNAME		"default"
#define DEFAULT_PCM_NUM		0
#define	DEFAULT_PCM_NAME	"/dev/snd/pcmC0D0p"
#define DEFAULT_MIXER_NAME	"/dev/snd/controlC0"
#define MAX_ID_STRLEN		96

#ifdef __ANDROID__
# define DEFAULT_RENDERER	"Android"
#else
# define DEFAULT_RENDERER	"Linux"
#endif

static char *_aaxLinuxDriverLogVar(const void *, const char *, ...);

#define FILL_FACTOR		1.5f
#define DEFAULT_REFRESH		25.0f
#define _AAX_DRVLOG(a)		_aaxLinuxDriverLog(id, __LINE__, 0, a)

static _aaxDriverDetect _aaxLinuxDriverDetect;
static _aaxDriverNewHandle _aaxLinuxDriverNewHandle;
static _aaxDriverFreeHandle _aaxLinuxDriverFreeHandle;
static _aaxDriverGetDevices _aaxLinuxDriverGetDevices;
static _aaxDriverGetInterfaces _aaxLinuxDriverGetInterfaces;
static _aaxDriverConnect _aaxLinuxDriverConnect;
static _aaxDriverDisconnect _aaxLinuxDriverDisconnect;
static _aaxDriverSetup _aaxLinuxDriverSetup;
static _aaxDriverCaptureCallback _aaxLinuxDriverCapture;
static _aaxDriverPlaybackCallback _aaxLinuxDriverPlayback;
static _aaxDriverSetName _aaxLinuxDriverSetName;
static _aaxDriverGetName _aaxLinuxDriverGetName;
static _aaxDriverRender _aaxLinuxDriverRender;
static _aaxDriverThread _aaxLinuxDriverThread;
static _aaxDriverState _aaxLinuxDriverState;
static _aaxDriverParam _aaxLinuxDriverParam;
static _aaxDriverLog _aaxLinuxDriverLog;

static char _kernel_id_str[MAX_ID_STRLEN] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxLinuxDriverBackend =
{
   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_kernel_id_str,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxLinuxDriverDetect,
   (_aaxDriverNewHandle *)&_aaxLinuxDriverNewHandle,
   (_aaxDriverFreeHandle *)&_aaxLinuxDriverFreeHandle,
   (_aaxDriverGetDevices *)&_aaxLinuxDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxLinuxDriverGetInterfaces,

   (_aaxDriverSetName *)&_aaxLinuxDriverSetName,
   (_aaxDriverGetName *)&_aaxLinuxDriverGetName,
   (_aaxDriverRender *)&_aaxLinuxDriverRender,
   (_aaxDriverThread *)&_aaxLinuxDriverThread,

   (_aaxDriverConnect *)&_aaxLinuxDriverConnect,
   (_aaxDriverDisconnect *)&_aaxLinuxDriverDisconnect,
   (_aaxDriverSetup *)&_aaxLinuxDriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxLinuxDriverCapture,
   (_aaxDriverPlaybackCallback *)&_aaxLinuxDriverPlayback,
   NULL,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,
   NULL,

   ( _aaxDriverGetSetSources*)_aaxSoftwareDriverGetSetSources,

   (_aaxDriverState *)&_aaxLinuxDriverState,
   (_aaxDriverParam *)&_aaxLinuxDriverParam,
   (_aaxDriverLog *)&_aaxLinuxDriverLog
};

typedef struct
{
   void *handle;
   char *name;
   _aaxRenderer *render;
   enum aaxRenderMode mode;

   char *pcm;
   unsigned int cardnum;
   unsigned int devnum;
   int fd;

   size_t threshold;		/* sensor buffer threshold for padding */
   float padding;		/* for sensor clock drift correction   */

   float latency;
   float frequency_hz;
   float refresh_rate;
   unsigned int format;
   unsigned int no_tracks;
   ssize_t period_frames;
   ssize_t period_frames_actual;
   char no_periods;
   char bits_sample;

   char use_mmap;
   char use_timer;
   char interleaved;
   char prepared;
   char pause;
   char can_pause;

   void **ptr;
   char **scratch;
   ssize_t buf_len;

   char *ifname[2];

   struct snd_pcm_mmap_status *status;
   struct snd_pcm_mmap_control *control;
   struct snd_pcm_sync_ptr *sync;
   void *mmap_buffer;

   _batch_cvt_to_intl_proc cvt_to_intl;

   struct {
      float I;
      float err;
   } PID;
   struct {
      float aim;
   } fill;

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
DECL_FUNCTION(mmap);
DECL_FUNCTION(munmap);

static int detect_cardnum(const char*);
static int detect_pcm(_driver_t*, char);
static const char* detect_devname(int, int, char);
static void _init_params(struct snd_pcm_hw_params*);
static void _set_mask(struct snd_pcm_hw_params*, int, unsigned int);
static void _set_min(struct snd_pcm_hw_params*, int, unsigned int);
static unsigned int _get_int(struct snd_pcm_hw_params*, int);
static unsigned int _set_int(struct snd_pcm_hw_params*, int, unsigned int);
static unsigned int _get_minmax(struct snd_pcm_hw_params*, int, unsigned int, unsigned int*, unsigned int*);
static void _kernel_set_volume(_driver_t*, _aaxRingBuffer*, ssize_t, snd_pcm_sframes_t, unsigned int, float);


static const char *_const_kernel_default_pcm = DEFAULT_PCM_NAME;
static const char *_const_kernel_default_name = DEFAULT_DEVNAME;
static int _kernel_default_cardnum = DEFAULT_PCM_NUM;
static int _get_pagesize();

#ifndef O_NONBLOCK
# define O_NONBLOCK	0
#endif

static void *audio = NULL;

static int
_aaxLinuxDriverDetect(int mode)
{
   static int rv = false;
   char *s, *error = 0;

   _AAX_LOG(LOG_DEBUG, __func__);

#if RELEASE
   if TEST_FOR_FALSE(rv) {
      rv = _aaxALSADriverDetect(mode);
   }
# if HAVE_PULSEAUDIO_H
   if TEST_FOR_FALSE(rv) {
      rv = _aaxPulseAudioDriverDetect(mode);
   }
# endif
# if HAVE_PIPEWIRE_H
   if TEST_FOR_FALSE(rv) {
      rv = _aaxPipeWireDriverDetect(mode);
   }
# endif

#endif

   s = getenv("AAX_USE_KERNEL_DRIVER");
   if (s && _aax_getbool(s)) {
      rv = false;
   }

   if (TEST_FOR_FALSE(rv) && !audio)
   {
      audio = _aaxIsLibraryPresent(NULL, 0);
      _aaxGetSymError(0);
   }
     
   if (audio && mode != 0)
   {
      TIE_FUNCTION(ioctl);
      TIE_FUNCTION(mmap);
      TIE_FUNCTION(munmap);
      TIE_FUNCTION(poll);

      error = _aaxGetSymError(0);
      if (!error && (access(DEFAULT_PCM_NAME, F_OK) != -1)) {
         rv = true;
      }
   }
   else {
      rv = false;
   }

   return rv;
}

static void *
_aaxLinuxDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (handle)
   {
      char m = (mode == AAX_MODE_READ) ? 0 : 1;
      const char *name;

      handle->pcm = (char*)_const_kernel_default_pcm;
      handle->name = (char*)_const_kernel_default_name;
      handle->frequency_hz = (float)48000.0f;
      handle->bits_sample = 16;
      handle->no_tracks = 2;
      handle->no_periods = DEFAULT_PERIODS;
      handle->period_frames = handle->frequency_hz/DEFAULT_REFRESH;
      handle->period_frames_actual = handle->period_frames;
      handle->buf_len = handle->no_tracks*handle->period_frames*handle->bits_sample/8;
      handle->use_mmap = false;
      handle->mode = mode;
      handle->interleaved = true;
      handle->fd = -1;
      if (handle->mode != AAX_MODE_READ) { // Always interupt based for capture
         handle->use_timer = TIMER_BASED;
      }

      detect_pcm(handle, m);
      name = detect_devname(handle->cardnum, handle->devnum, m);
      handle->name = _aax_strdup(name);

      // default period size is for 25Hz
      handle->fill.aim = FILL_FACTOR * DEFAULT_REFRESH;
   }

   return handle;
}

static int
_aaxLinuxDriverFreeHandle(UNUSED(void *id))
{
   _aaxCloseLibrary(audio);
   audio = NULL;

   return true;
}

static void *
_aaxLinuxDriverConnect(void *config, const void *id, xmlId *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      id = handle = _aaxLinuxDriverNewHandle(mode);
   }

   if (handle)
   {
      if (xid)
      {
         float f;
         char *s;
         int i;

         if (!handle->pcm)
         {
            s = xmlAttributeGetString(xid, "name");
            if (s)
            {
               handle->cardnum = detect_cardnum(s);
               if (handle->name != _const_kernel_default_name) {
                  free(handle->name);
               }
               handle->name = _aax_strdup(s);
               xmlFree(s);
            }
         }

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
            if (i < 1)
            {
               _AAX_DRVLOG("no periods too small.");
               i = 1;
            }
            else if (i > 16)
            {
               _AAX_DRVLOG("no. periods too great.");
               i = 16;
            }
            handle->no_periods = i;
         }
      }

      if (renderer)
      {
         handle->cardnum = detect_cardnum(renderer);
         if (handle->name != _const_kernel_default_name) {
            free(handle->name);
         }
         handle->name = _aax_strdup(renderer);
      }
   }

   if (handle)
   {
      char m = (handle->mode == AAX_MODE_READ) ? 0 : 1;
      if (detect_pcm(handle, m))
      {
         handle->handle = config;
         handle->fd = open(handle->pcm, O_RDWR|O_NONBLOCK);
         if (handle->fd >= 0)
         {
            if (!handle->use_timer)
            {
               close(handle->fd);
               handle->fd = open(handle->pcm, O_RDWR);
            }
         }
         else
         {
            _aaxLinuxDriverLogVar(id, "open: %s", strerror(errno));
            free(handle);
            handle = NULL;
         }
      }
   }

   return (void *)handle;
}

static int
_aaxLinuxDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = false;

   if (handle)
   {
      int page_size = _get_pagesize();
      char *ifname;

      if (handle->fd >= 0) {
         close(handle->fd);
      }

      if (handle->sync) {
         free(handle->sync);
      }
      else
      {
         if (handle->status) {
            munmap(handle->status, page_size);
         }
         if (handle->control) {
            munmap(handle->control, page_size);
         }
      }

      ifname = handle->ifname[handle->mode ? 1 : 0];
      if (ifname) free(ifname);

      if (handle->name != _const_kernel_default_name) {
         free(handle->name);
      }

      if (handle->pcm != _const_kernel_default_pcm) {
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
_aaxLinuxDriverSetup(const void *id, float *refresh_rate, int *fmt,
                     unsigned int *channels, float *speed, UNUSED(int *bitrate),
                     int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = false;

   if (handle->fd >= 0)
   {
      snd_pcm_uframes_t period_frames, period_frames_actual;
      unsigned int tracks, rate, bits, periods, format;
      struct snd_pcm_hw_params hwparams;
      struct snd_pcm_sw_params swparams;
      int res, fd = handle->fd;
 
      rate = (unsigned int)*speed;
      tracks = *channels;
      if (tracks > handle->no_tracks) {
         tracks = handle->no_tracks;
      }

      periods = handle->no_periods;
      if (!registered) {
         period_frames = get_pow2((size_t)rintf(rate/(*refresh_rate*periods)));
      } else {
         period_frames = get_pow2((size_t)rintf((rate*periods)/period_rate));
      }
      period_frames_actual = period_frames;
      bits = aaxGetBitsPerSample(*fmt);

      handle->latency = 1.0f / *refresh_rate;
      if (handle->latency < 0.010f) {
         handle->use_timer = false;
      }

      _init_params(&hwparams);
      hwparams.flags |= SNDRV_PCM_HW_PARAMS_NORESAMPLE;
      if (handle->use_timer) {
          hwparams.flags |= SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP;
      }

      res = pioctl(fd, SNDRV_PCM_IOCTL_HW_REFINE, &hwparams);
      if (res >= 0)
      {
         unsigned int min, max;

         rate = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_RATE, rate,
                            &handle->min_frequency, &handle->max_frequency);
         bits = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, bits,
                            &min, &max);
         tracks = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_CHANNELS, tracks,
                              &handle->min_tracks, &handle->max_tracks);
         periods = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_PERIODS, periods,
                               &handle->min_periods, &handle->max_periods);

         /* recalculate the no. frames to match the refresh_rate */
         if (!registered) {
            period_frames = get_pow2((size_t)rintf(rate/(*refresh_rate*periods)));
         } else {
            period_frames = get_pow2((size_t)rintf((rate*periods)/period_rate));
         }
         period_frames = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
                                                period_frames, &min, &max);
         if (handle->mode == AAX_MODE_READ) {
            period_frames_actual = 2*period_frames;
         } else {
            period_frames_actual = period_frames;
         }
      }

      switch (bits)
      {
      case 8:
         handle->cvt_to_intl = _batch_cvt8_intl_24;
         format = SND_PCM_FORMAT_S8;
         *fmt = AAX_PCM8S;
         break;
      case 24:
         handle->cvt_to_intl = _batch_cvt24_intl_24;
         format = SND_PCM_FORMAT_S24_LE;
         *fmt = AAX_PCM24S;
         break;
      case 32:
         handle->cvt_to_intl = _batch_cvt32_intl_24;
         format = SND_PCM_FORMAT_S32_LE;
         *fmt = AAX_PCM32S;
         break;
      case 16:
      default:
         handle->cvt_to_intl = _batch_cvt16_intl_24;
         format = SND_PCM_FORMAT_S16_LE;
         *fmt = AAX_PCM16S;
      }
      _set_mask(&hwparams, SNDRV_PCM_HW_PARAM_FORMAT, format);
      _set_mask(&hwparams, SNDRV_PCM_HW_PARAM_SUBFORMAT,
                           SNDRV_PCM_SUBFORMAT_STD);
      _set_int(&hwparams, SNDRV_PCM_HW_PARAM_RATE, rate);
      _set_int(&hwparams, SNDRV_PCM_HW_PARAM_CHANNELS, tracks);
      _set_min(&hwparams, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, period_frames_actual);
      _set_int(&hwparams, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, bits);
      _set_int(&hwparams, SNDRV_PCM_HW_PARAM_FRAME_BITS, bits*tracks);
      _set_int(&hwparams, SNDRV_PCM_HW_PARAM_PERIODS, periods);

      if (handle->use_mmap) {
         _set_mask(&hwparams, SNDRV_PCM_HW_PARAM_ACCESS,
                              SND_PCM_ACCESS_MMAP_INTERLEAVED);
      } else {
         _set_mask(&hwparams, SNDRV_PCM_HW_PARAM_ACCESS,
                              SND_PCM_ACCESS_RW_INTERLEAVED);
      }

      res = pioctl(handle->fd, SNDRV_PCM_IOCTL_HW_PARAMS, &hwparams);
      if (res >= 0)
      {
         if (hwparams.info & SNDRV_PCM_INFO_PAUSE) {
            handle->can_pause = true;
         }

         memset(&swparams, 0, sizeof(struct snd_pcm_sw_params));
         swparams.tstamp_mode = SNDRV_PCM_TSTAMP_ENABLE;
         swparams.period_step = 1;
         swparams.avail_min = 1;
         swparams.silence_size = 0;
         swparams.silence_threshold = periods*period_frames_actual;
         swparams.boundary = periods*period_frames_actual;
         swparams.xfer_align = period_frames/2;
         if (handle->mode == AAX_MODE_READ)
         {
            swparams.start_threshold = 1;
            swparams.stop_threshold = periods*period_frames_actual;
         }
         else
         {
            swparams.start_threshold = period_frames*(periods-1);
            swparams.stop_threshold = periods*period_frames;
         }
         res = pioctl(handle->fd, SNDRV_PCM_IOCTL_SW_PARAMS, &swparams);
         if (res >= 0)
         {
            int page_size = _get_pagesize();
            handle->status = pmmap(NULL, page_size, PROT_READ,
                                         MAP_FILE|MAP_SHARED, handle->fd,
                                         SNDRV_PCM_MMAP_OFFSET_STATUS);
            if (handle->status != MAP_FAILED)
            {
               handle->control = pmmap(NULL, page_size, PROT_READ|PROT_WRITE,
                                       MAP_FILE|MAP_SHARED, handle->fd,
                                       SNDRV_PCM_MMAP_OFFSET_CONTROL);
               if (handle->control == MAP_FAILED)
               {
                  pmunmap(handle->status, page_size);
                  handle->status = NULL;
               }
            }
            else {
               handle->status = NULL;
            }

            if (handle->status == NULL)
            {
               handle->sync = calloc(1, sizeof(struct snd_pcm_sync_ptr));
               if (handle->sync)
               {
                  handle->status = &handle->sync->s.status;
                  handle->control = &handle->sync->c.control;
                  handle->sync->flags = 0;
                  pioctl(handle->fd, SNDRV_PCM_IOCTL_SYNC_PTR, handle->sync);
               }
            }

            if (handle->status)
            {
               handle->control->avail_min = 1;

               rate = _get_int(&hwparams, SNDRV_PCM_HW_PARAM_RATE);
               tracks = _get_int(&hwparams, SNDRV_PCM_HW_PARAM_CHANNELS);
               periods = _get_int(&hwparams, SNDRV_PCM_HW_PARAM_PERIODS);
               period_frames_actual = _get_int(&hwparams, SNDRV_PCM_HW_PARAM_PERIOD_SIZE);

               handle->frequency_hz = rate;
               handle->no_tracks = tracks;
               handle->bits_sample = bits;
               handle->no_periods = periods;
               handle->period_frames = period_frames;
               handle->period_frames_actual = period_frames_actual;
               handle->threshold = 5*period_frames/4;
#if 0
 printf("   frequency           : %f\n", handle->frequency_hz);
 printf("   no. channels:         %i\n", handle->no_tracks);
 printf("   bits per sample:      %i\n", handle->bits_sample);
 printf("   no. periods:          %i\n", handle->no_periods);
 printf("   period frames:        %zi\n", handle->period_frames);
 printf("   actial period frames: %zi\n", handle->period_frames_actual);
#endif

               *speed = rate;
               *channels = tracks;
               if (!registered) {
                  *refresh_rate = rate/(float)period_frames;
               } else {
                  *refresh_rate = period_rate;
               }
               handle->refresh_rate = *refresh_rate;

               if (!handle->use_timer)
               {
                  handle->latency = 1.0f / *refresh_rate;
                  if (handle->mode != AAX_MODE_READ) // && !handle->use_timer)
                  {
                     char m = (handle->mode == AAX_MODE_READ) ? 0 : 1;
                     snd_pcm_sframes_t delay;
                     _aaxRingBuffer *rb;
                     int i;

                     rb = _aaxRingBufferCreate(0.0f, m);
                     if (rb)
                     {
                        rb->set_format(rb, AAX_PCM24S, true);
                        rb->set_parami(rb, RB_NO_TRACKS, handle->no_tracks);
                        rb->set_paramf(rb, RB_FREQUENCY, handle->frequency_hz);
                        rb->set_parami(rb, RB_NO_SAMPLES, handle->period_frames);
                        rb->init(rb, true);
                        rb->set_state(rb, RB_STARTED);

                        for (i=0; i<handle->no_periods; i++) {
                           _aaxLinuxDriverPlayback(handle, rb, 1.0f, 0.0f, 0);
                        }
                        _aaxRingBufferFree(rb);
                     }

                     res = pioctl(handle->fd, SNDRV_PCM_IOCTL_DELAY, &delay);
                     if (res >= 0) {
                        handle->latency = (float)delay/(float)rate;
                     }
                     res = 0;
                  }
               }
               else
               {
                  handle->fill.aim = (float)period_frames/(float)rate;
                  if (handle->fill.aim > 0.02f) {
                     handle->fill.aim += 0.01f; // add 10ms
                  } else {
                     handle->fill.aim *= FILL_FACTOR;
                  }
                  handle->latency = handle->fill.aim;
               }

               handle->render = _aaxSoftwareInitRenderer(handle->latency,
                                                      handle->mode, registered);
               if (handle->render)
               {
                  const char *rstr = handle->render->info(handle->render->id);
                  char *os_name = "";
#if HAVE_SYS_UTSNAME_H
                  struct utsname utsname;
                  uname(&utsname);
                  os_name = utsname.sysname;
#endif
                  snprintf(_kernel_id_str, MAX_ID_STRLEN, "%s %s %s",
                                       DEFAULT_RENDERER, os_name, rstr);
                  rv = true;
               }
               else {
                  _AAX_DRVLOG("unable to get the renderer");
               }
            }
            else {
               _AAX_DRVLOG("status information unavailable");
            }
         }
         else {
            _AAX_DRVLOG("invalid software configuration");
         }
      }
      else {
         _AAX_DRVLOG("incompatible hardware configuration");
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
      snprintf(str,255, "  playback rate: %5.0f hz", handle->frequency_hz);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  buffer size: %zu bytes", handle->period_frames*handle->no_tracks*handle->bits_sample/8);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  latency: %3.2f ms",  1e3*handle->latency);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  no. periods: %i", handle->no_periods);
      _AAX_SYSLOG(str);
      snprintf(str,255,"  use mmap: %s", handle->use_mmap?"yes":"no");
      _AAX_SYSLOG(str);
      snprintf(str,255,"  interleaved: %s",handle->interleaved?"yes":"no");
      _AAX_SYSLOG(str);
      snprintf(str,255,"  timer based: %s",handle->use_timer?"yes":"no");
      _AAX_SYSLOG(str);
      snprintf(str,255,"  channels: %i, bytes/sample: %i\n", handle->no_tracks, handle->bits_sample/8);
      _AAX_SYSLOG(str);
      snprintf(str,255,"  can pause: %i\n", handle->can_pause);
      _AAX_SYSLOG(str);

#if 0
 printf("driver settings:\n");
 if (handle->mode != AAX_MODE_READ) {
    printf("  output renderer: '%s'\n", handle->name);
 } else {
    printf("  input renderer: '%s'\n", handle->name);
 }
 printf("  pcm: '%s', card: %i, device: %i\n", handle->pcm, handle->cardnum, handle->devnum);
 printf("  playback rate: %5.0f Hz\n", handle->frequency_hz);
 printf("  buffer size: %zu bytes\n", handle->period_frames*handle->no_tracks*handle->bits_sample/8);
 printf("  latency:  %5.2f ms\n", 1e3*handle->latency);
 printf("  no_periods: %i\n", handle->no_periods);
 printf("  use mmap: %s\n", handle->use_mmap?"yes":"no");
 printf("  interleaved: %s\n",handle->interleaved?"yes":"no");
 printf("  timer based: %s\n",handle->use_timer?"yes":"no");
 printf("  channels: %i, bytes/sample: %i\n", handle->no_tracks, handle->bits_sample/8);
 printf("  can pause: %i\n", handle->can_pause);
#endif
   }
   while (0);

   return rv;
}


static ssize_t
_aaxLinuxDriverCapture(const void *id, void **data, ssize_t *offset, size_t *req_frames, void *scratch, size_t scratchlen, float gain, UNUSED(char batched))
{
   _driver_t *handle = (_driver_t *)id;
   snd_pcm_sframes_t avail;
   ssize_t offs = *offset;
   ssize_t init_offs = offs;
   ssize_t rv = false;
   int ret;
 
   assert(handle->mode == AAX_MODE_READ);

   *offset = 0;
   if ((req_frames == 0) || (data == 0))
      return rv;

   /* synchronise capture and playback for registered sensors */
   if (handle->sync)
   {
      handle->sync->flags = SNDRV_PCM_SYNC_PTR_APPL |
                            SNDRV_PCM_SYNC_PTR_HWSYNC;
      ret = pioctl(handle->fd, SNDRV_PCM_IOCTL_SYNC_PTR, handle->sync);
   }

   avail = handle->status->hw_ptr;
   avail -= handle->control->appl_ptr;

   if (*req_frames && handle->status)
   {
      snd_pcm_sframes_t period_frames, count, corr;
      unsigned int tracks = handle->no_tracks;
      int32_t **sbuf = (int32_t**)data;
      struct snd_xferi x;
      float diff;

      period_frames = *req_frames;

      /* try to keep the buffer padding at the threshold level at all times */
      diff = (float)avail-(float)handle->threshold;
      handle->padding = (handle->padding + diff/(float)period_frames)/2;
      corr = _MINMAX(roundf(handle->padding), -1, 1);
      period_frames += corr;
      offs -= corr;
      *offset = -corr;
#if 0
if (corr)
 printf("avail: %4i (%4i), fetch: %6i\r", avail, handle->threshold, period_frames);
#endif

      count = 8;
      do
      {
         ret = 0;
         if (!handle->prepared)
         {
            ret = pioctl(handle->fd, SNDRV_PCM_IOCTL_PREPARE);
            if (ret >= 0) {
               pioctl(handle->fd, SNDRV_PCM_IOCTL_START);
               handle->prepared = true;
            }
            else {
               _aaxLinuxDriverLogVar(id, "read prepare: %s", strerror(errno));
            }
         }

         if (handle->sync)
         {
            handle->sync->flags = SNDRV_PCM_SYNC_PTR_APPL |
                                  SNDRV_PCM_SYNC_PTR_HWSYNC;
            ret = pioctl(handle->fd, SNDRV_PCM_IOCTL_SYNC_PTR, handle->sync);
         }

         avail = handle->status->hw_ptr;
         avail -= handle->control->appl_ptr;
         if (avail)
         {
            int scratchsz= scratchlen*8/(handle->no_tracks*handle->bits_sample);

            x.result = 0;
            x.buf = scratch;
            x.frames = _MIN(_MIN(period_frames, avail), scratchsz);
            ret = pioctl(handle->fd, SNDRV_PCM_IOCTL_READI_FRAMES, &x);
            if (ret >= 0)
            {
               _batch_cvt24_16_intl(sbuf, scratch, offs, tracks, x.frames);
               period_frames -= x.frames;
               offs += x.frames;
            }
            else
            {
               _aaxLinuxDriverLogVar(id, "read: %s", strerror(errno));
               handle->prepared = false;
            }
         }
      }
      while (period_frames && --count);

      if (ret >= 0)
      {
         *req_frames = _MAX(offs, 0);
         _kernel_set_volume(handle, NULL, init_offs, offs, tracks, gain);
         rv = true;
      }
   }

   return rv;
}

static size_t
_aaxLinuxDriverPlayback(const void *id, void *s, UNUSED(float pitch), float gain, UNUSED(char batched))
{
   _aaxRingBuffer *rb = (_aaxRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   ssize_t ret, offs, outbuf_size, period_frames;
   snd_pcm_sframes_t avail, bufsize;
   unsigned int no_tracks, count;
   const int32_t **sbuf;
   char *data;
   int rv = 0;

   assert(rb);
   assert(id != 0);

   if (handle->mode == AAX_MODE_READ || handle->status == NULL)
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

   _kernel_set_volume(handle, rb, offs, period_frames, no_tracks, gain);

   data = (char*)handle->scratch;
   sbuf = (const int32_t**)rb->get_tracks_ptr(rb, RB_READ);
   handle->cvt_to_intl(data, sbuf, offs, no_tracks, period_frames);
   rb->release_tracks_ptr(rb);

   count = 8;
   bufsize = handle->no_periods*handle->period_frames;
   do
   {
      ret = 0;
      if (!handle->prepared)
      {
         ret = pioctl(handle->fd, SNDRV_PCM_IOCTL_PREPARE);
         if (ret >= 0) {
            handle->prepared = true;
         }
         else {
            _aaxLinuxDriverLogVar(id, "write prepare: %s", strerror(errno));
         }
      }

      if (handle->sync)
      {
         handle->sync->flags=SNDRV_PCM_SYNC_PTR_APPL|SNDRV_PCM_SYNC_PTR_HWSYNC;
         ret = pioctl(handle->fd, SNDRV_PCM_IOCTL_SYNC_PTR, handle->sync);
      }

      avail = handle->status->hw_ptr + bufsize;
      avail -= handle->control->appl_ptr;

      rv = bufsize - avail;
      if (handle->prepared)
      {
         struct snd_xferi xfer;

         xfer.result = 0;
         xfer.buf = data;
         xfer.frames = period_frames; // _MIN(period_frames, avail);
         ret = pioctl(handle->fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &xfer);
         if (ret >= 0) {
            period_frames -= xfer.frames;
         }
         else
         {
            _aaxLinuxDriverLogVar(id, "write: %s", strerror(errno));
            handle->prepared = false;
         }
      }
   }
   while (period_frames && (ret < 0) && --count);

   return rv;
}

static int
_aaxLinuxDriverSetName(const void *id, int type, const char *name)
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
_aaxLinuxDriverGetName(const void *id, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   if (handle && handle->name && (mode < AAX_MODE_WRITE_MAX)) {
      ret = _aax_strdup(handle->name);
   }

   return ret;
}

_aaxRenderer*
_aaxLinuxDriverRender(const void* config)
{
   _driver_t *handle = (_driver_t *)config;
   return handle->render;
}

static int
_aaxLinuxDriverState(const void *id, enum _aaxDriverState state)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = false;

   switch(state)
   {
   case DRIVER_PAUSE:
      if (handle && !handle->pause)
      {
         if (handle->can_pause)
         {
            int err = pioctl(handle->fd, SNDRV_PCM_IOCTL_PAUSE, 1);
            if (err >= 0) {
               rv = true;
            }
         }
         handle->pause = 1;
      }
      break;
   case DRIVER_RESUME:
      if (handle && handle->pause) 
      {
         if (handle->can_pause)
         {
            int err = pioctl(handle->fd, SNDRV_PCM_IOCTL_PAUSE, 0);
            if (err >= 0) {
               rv = true;
            }
         }
         handle->pause = 0;
      }
      break;
   case DRIVER_AVAILABLE:
#if 0
      if (handle)
      {
         int state;
         int err = pioctl(handle->fd, SNDRV_CTL_IOCTL_POWER_STATE, &state);
         if ((err >= 0) && (state != 0)) {
            rv = true;
         }
      }
#else
      rv = true;
#endif
      break;
   case DRIVER_SHARED_MIXER:
      rv = false;
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
_aaxLinuxDriverParam(const void *id, enum _aaxDriverParam param)
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
         snd_pcm_sframes_t avail;
         int err = pioctl(handle->fd, SNDRV_PCM_IOCTL_DELAY, &avail);
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
_aaxLinuxDriverGetDevices(UNUSED(const void *id), int mode)
{
   static char names[2][1024] = { "\0\0", "\0\0" };
   static time_t t_previous[2] = { 0, 0 };
   unsigned char m = (mode == AAX_MODE_READ) ? 0 : 1;
   time_t t_now;

   t_now = time(NULL);
   if (t_now > (t_previous[m]+5))
   {
//    _driver_t *handle = (_driver_t*)id;
      int fd, len = 1024;
      int card = 0;
      char *ptr;

      t_previous[m] = t_now;

      ptr = (char *)&names[m];
      *ptr = 0; *(ptr+1) = 0;
      do
      {
         char fn[256];

         snprintf(fn, 256, "/dev/snd/controlC%u", card++);
         fd = open(fn, O_RDONLY|O_NONBLOCK);
         if (fd >= 0)
         {
            struct snd_ctl_card_info card_info;
            if (pioctl(fd, SNDRV_CTL_IOCTL_CARD_INFO, &card_info) >= 0)
            {
               int slen;

               close(fd);

               slen = strlen((const char*)card_info.name)+1;
               if (slen > (len-1)) break;

               snprintf(ptr, len, "%s", (const char*)card_info.name);
               len -= slen;
               ptr += slen;
            }
         }
      }
      while (card < MAX_DEVICES);

      /* always end with "\0\0" no matter what */
      names[m][1022] = 0;
      names[m][1023] = 0;
   }

   return (char *)&names[m];
}

static char *
_aaxLinuxDriverGetInterfaces(const void *id, const char *devname, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned char m = (mode == AAX_MODE_READ) ? 0 : 1;
   char *rv = handle ? handle->ifname[m] : NULL;

   if (!rv)
   {
      char devlist[1024] = "\0\0";
      int card, device, fd;
      size_t len = 1024;
      char *ptr;

      card = detect_cardnum(devname);
      ptr = devlist;
      device = 0;
      do
      {
         char fn[256];
         snprintf(fn, 256, "/dev/snd/pcmC%uD%u%c", card, device, m ? 'p' : 'c');

         fd = open(fn, O_RDONLY|O_NONBLOCK);
         if (fd >= 0)
         {
            struct snd_pcm_info info;
            unsigned int slen;

            pioctl(fd, SNDRV_PCM_IOCTL_INFO, &info);
            close(fd);

            slen = strlen((const char*)info.name)+1;
            if (slen > (len-1)) break;

            snprintf(ptr, len, "%s", (const char*)info.name);
            len -= slen;
            ptr += slen;
         }
         device++;
      }
      while (device < MAX_DEVICES);

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

   return rv;
}

static char *
_aaxLinuxDriverLogVar(const void *id, const char *fmt, ...)
{
   char _errstr[1024];
   va_list ap;

   _errstr[0] = '\0';
   va_start(ap, fmt);
   vsnprintf(_errstr, 1024, fmt, ap);

   // Whatever happen in vsnprintf, what i'll do is just to null terminate it
   _errstr[1023] = '\0';
   va_end(ap);

   return _aaxLinuxDriverLog(id, 0, -1, _errstr);
}

static char *
_aaxLinuxDriverLog(const void *id, UNUSED(int prio), UNUSED(int type), const char *str)
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
_kernel_set_volume(UNUSED(_driver_t *handle), _aaxRingBuffer *rb, ssize_t offset, snd_pcm_sframes_t period_frames, UNUSED(unsigned int no_tracks), float volume)
{
   float gain = fabsf(volume);

// TODO: hardware volume if available

   /* software volume fallback */
   if (rb && fabsf(gain - 1.0f) > LEVEL_32DB) {
      rb->data_multiply(rb, offset, period_frames, gain);
   }
}


/* -------------------------------------------------------------------------- */

#ifdef WIN32
void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
   return 0;
}

int munmap(void *addr, size_t len)
{
   return 0;
}
#endif

static int
_get_pagesize()
{
#ifdef WIN32
   SYSTEM_INFO system_info;
   GetSystemInfo (&system_info);
   int page_size = system_info.dwPageSize;
#else
   int page_size = sysconf(_SC_PAGE_SIZE);
#endif

   return page_size;
}

static void
_init_params(struct snd_pcm_hw_params *params)
{
   if (params)
   {
      int n;

      memset(params, 0, sizeof(struct snd_pcm_hw_params));

      for (n = SNDRV_PCM_HW_PARAM_FIRST_MASK;
           n <= SNDRV_PCM_HW_PARAM_LAST_MASK; n++)
      {
         struct snd_mask *m = &params->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK];
         m->bits[0] = ~0;
         m->bits[1] = ~0;
      }

      for (n = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
           n <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; n++)
      {
         struct snd_interval *i;

         i = &params->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
         i->max = ~0;
      }
      params->rmask = ~0U;
      params->info = ~0U;
   }
}

static unsigned int
_get_minmax(struct snd_pcm_hw_params *params, int param, unsigned int value, unsigned int *min, unsigned int *max)
{
   int rv = value;
   if (param >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL &&
       param <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL)
   {
      struct snd_interval *itv;

      itv = &params->intervals[param - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
      rv = _MINMAX(value, itv->min, itv->max);
      *min = itv->min;
      *max = itv->max;
   }
   return rv;
}

static unsigned int
_set_int(struct snd_pcm_hw_params *params, int param, unsigned int value)
{
   int rv = value;
   if (param >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL &&
       param <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL)
   {
      struct snd_interval *itv;

      itv = &params->intervals[param - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
      itv->min = value;
      itv->max = value;
      itv->integer = 1;
   }
   return rv;
}

static unsigned int
_get_int(struct snd_pcm_hw_params *params, int param)
{
   int rv = 0;
   if (param >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL &&
       param <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL)
   {
      struct snd_interval *itv;

      itv = &params->intervals[param - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
      if (itv->integer) {
         rv = itv->max;
      }
   }
   return rv;
}

static void
_set_min(struct snd_pcm_hw_params *params, int param, unsigned int value)
{
   if (param >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL &&
       param <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL)
   {
      struct snd_interval *itv;

      itv = &params->intervals[param - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
      itv->min = value;
   }
}

static void
_set_mask(struct snd_pcm_hw_params *params, int param, unsigned int bit)
{
   if ((bit < SNDRV_MASK_MAX) &&
       (param >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
       (param <= SNDRV_PCM_HW_PARAM_LAST_MASK))
   {
      struct snd_mask *mask;

      mask = &params->masks[param - SNDRV_PCM_HW_PARAM_FIRST_MASK];
      mask->bits[0] = mask->bits[1] = 0;
      mask->bits[bit >> 5] |= (1 << (bit & 31));
   }
}

static int
detect_pcm(_driver_t *handle, char m)
{
   const char *devname = handle->name;
   int fd, rv = false;

   handle->pcm = (char*)_const_kernel_default_pcm;
   if (devname && strcasecmp(devname, DEFAULT_DEVNAME))
   {
      int card = 0, device = 0;
      char *ifname;

      ifname = strstr(devname, ": ");
      if (ifname)
      {
         char fn[256];

         *ifname = 0;
         ifname += 2;

         card = detect_cardnum(devname);
         device = 0;
         do
         {
            snprintf(fn, 256, "/dev/snd/pcmC%uD%u%c", card, device, m?'p':'c');

            fd = open(fn, O_RDONLY|O_NONBLOCK);
            if (fd >= 0)
            {
               struct snd_pcm_info info;
               const char *name;
               int found;

               pioctl(fd, SNDRV_PCM_IOCTL_INFO, &info);

               name = (const char*)info.name;
               found = !strcasecmp(ifname, name) ? true : false;
               close(fd);

               if (found) break;
            }
            device++;
         }
         while (device < MAX_DEVICES);

         ifname -= 2;
         *ifname = ':';

         handle->pcm = _aax_strdup(fn);
      }

      handle->cardnum = card;
      handle->devnum = device;
      rv = true;
   }
   else
   {
      handle->cardnum = 0;
      handle->devnum = 0;
      rv = true;
   }

   return rv;
}

static const char*
detect_devname(int card, int device, char m)
{
   static char rv[256];
   char fn[256];
   int fd;

   snprintf(fn, 256, "/dev/snd/controlC%u", card);

   fd = open(fn, O_RDONLY|O_NONBLOCK);
   if (fd >= 0)
   {
      struct snd_ctl_card_info card_info;
      if (pioctl(fd, SNDRV_CTL_IOCTL_CARD_INFO, &card_info) >= 0) {
         snprintf(rv, 256, "%s", card_info.name);
      }
      close(fd);

      snprintf(fn, 256, "/dev/snd/pcmC%uD%u%c", card, device, m ? 'p' : 'c');

      fd = open(fn, O_RDONLY|O_NONBLOCK);
      if (fd >= 0)
      {
         struct snd_pcm_info info;
         if (pioctl(fd, SNDRV_PCM_IOCTL_INFO, &info) >= 0)
         {
            char *ptr = strchr(rv, '\0');
            size_t len = 256 - (ptr - rv);

            if (len < 256) {
               snprintf(ptr, len, ": %s", info.name);
            }
         }
         close(fd);
      }
   }

   return (const char*)&rv;
}

static int
detect_cardnum(const char *devname)
{
   int rv = _kernel_default_cardnum;

   if (devname && !strncasecmp(devname, "/dev/snd/pcmC", 13) ) {
       rv = strtol(devname+13, NULL, 10);
   }
   else if (devname && strcasecmp(devname, "Linux") &&
                       strcasecmp(devname, DEFAULT_DEVNAME))
   {
      int fd, card = -1;
      do
      {
         char fn[256];

         snprintf(fn, sizeof(fn), "/dev/snd/controlC%u", ++card);
         fd = open(fn, O_RDONLY|O_NONBLOCK);
         if (fd >= 0)
         {
            struct snd_ctl_card_info card_info;
            if (pioctl(fd, SNDRV_CTL_IOCTL_CARD_INFO, &card_info) >= 0)
            {
               const char *name = (const char*)card_info.name;
               int res = strcasecmp(devname, name);
               close(fd);
   
               if (!res) 
               {
                  rv = card;
                  break;
               }
            }
         }
      }
      while (card < MAX_DEVICES);
   }

   return rv;
}

int
_aaxLinuxDriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   snd_pcm_sframes_t period_frames, bufsize;
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
   if (!be_handle->status) {
      return false;
   }

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

   bufsize = be_handle->no_periods*be_handle->period_frames;

   wait_us = delay_sec*1000000;
   period_frames = dest_rb->get_parami(dest_rb, RB_NO_SAMPLES);
   stdby_time = (int)(delay_sec*1000);
   _aaxMutexLock(handle->thread.signal.mutex);
   while TEST_FOR_TRUE(handle->thread.started)
   {
      snd_pcm_sframes_t avail;
      int ret;

      _aaxMutexUnLock(handle->thread.signal.mutex);

      avail = be_handle->status->hw_ptr + bufsize;
      avail -= be_handle->control->appl_ptr;
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
                  }
               }
               while (!(pfd.revents & (POLLIN|POLLOUT)));
               if (ret < 0) {
                  _aaxLinuxDriverLogVar(id, "poll: %s\n", strerror(errno));
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
