/*
 * Copyright 2018 by Erik Hofman.
 * Copyright 2018 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

// General SDL help:
// http://osdl.sourceforge.net/main/documentation/rendering/SDL-audio.html

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#if HAVE_STRINGS_H
# include <strings.h>	/* strcasecmp */
#endif

#include <aax/aax.h>
#include <xml.h>

#include <base/dlsym.h>
#include <base/logging.h>
#include <base/gmath.h>

#include <ringbuffer.h>
#include <arch.h>
#include <api.h>

#include <dsp/effects.h>
#include <software/renderer.h>
#include "audio.h"
#include "device.h"


#define SDL_ID_STRING	"SDL"
#define MAX_ID_STRLEN		96

#define USE_CALLBACK		0
#define DEFAULT_OUTPUT_RATE	48000
#define DEFAULT_DEVNAME		"Default"
#define DEFAULT_RENDERER	"SDL"
#define DEFAULT_REFRESH		25.0f
#define FILL_FACTOR		1.5f

#define _AAX_DRVLOG(a)		_aaxSDLDriverLog(NULL, 0, 0, a)

static _aaxDriverDetect _aaxSDLDriverDetect;
static _aaxDriverNewHandle _aaxSDLDriverNewHandle;
static _aaxDriverGetDevices _aaxSDLDriverGetDevices;
static _aaxDriverGetInterfaces _aaxSDLDriverGetInterfaces;
static _aaxDriverConnect _aaxSDLDriverConnect;
static _aaxDriverDisconnect _aaxSDLDriverDisconnect;
static _aaxDriverSetup _aaxSDLDriverSetup;
static _aaxDriverCaptureCallback _aaxSDLDriverCapture;
static _aaxDriverPlaybackCallback _aaxSDLDriverPlayback;
static _aaxDriverGetName _aaxSDLGetName;
static _aaxDriverRender _aaxSDLDriverRender;
static _aaxDriverThread _aaxSDLDriverThread;
static _aaxDriverState _aaxSDLDriverState;
static _aaxDriverParam _aaxSDLDriverParam;
static _aaxDriverLog _aaxSDLDriverLog;

static char _sdl_id_str[MAX_ID_STRLEN+1] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxSDLDriverBackend =
{
   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_sdl_id_str,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxSDLDriverDetect,
   (_aaxDriverNewHandle *)&_aaxSDLDriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxSDLDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxSDLDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxSDLGetName,
   (_aaxDriverRender *)&_aaxSDLDriverRender,
   (_aaxDriverThread *)&_aaxSDLDriverThread,

   (_aaxDriverConnect *)&_aaxSDLDriverConnect,
   (_aaxDriverDisconnect *)&_aaxSDLDriverDisconnect,
   (_aaxDriverSetup *)&_aaxSDLDriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxSDLDriverCapture,
   (_aaxDriverPlaybackCallback *)&_aaxSDLDriverPlayback,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,
   NULL,

   (_aaxDriverState *)&_aaxSDLDriverState,
   (_aaxDriverParam *)&_aaxSDLDriverParam,
   (_aaxDriverLog *)&_aaxSDLDriverLog
};

typedef struct
{
   void *handle;
   char *driver;
   char *devname;
   _aaxRenderer *render;
   enum aaxRenderMode mode;

   uint32_t devnum;
   SDL_AudioSpec spec;

   unsigned int format;
   unsigned int period_frames;
   char bits_sample;
   char no_periods;

   size_t threshold;            /* sensor buffer threshold for padding */
   float padding;               /* for sensor clock drift correction   */

   float latency;

   void **ptr;
   char **scratch;
   ssize_t scratch_ptr;
   ssize_t buf_len;

   char *ifname[2];

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
   unsigned int min_tracks;
   unsigned int max_tracks;

} _driver_t;

// http://wiki.libsdl.org/CategoryAudio
DECL_FUNCTION(SDL_GetVersion);
DECL_FUNCTION(SDL_GetNumAudioDrivers);
DECL_FUNCTION(SDL_GetAudioDriver);
DECL_FUNCTION(SDL_GetNumAudioDevices);
DECL_FUNCTION(SDL_GetAudioDeviceName);
DECL_FUNCTION(SDL_Init);
DECL_FUNCTION(SDL_Quit);
DECL_FUNCTION(SDL_AudioInit);
DECL_FUNCTION(SDL_AudioQuit);
DECL_FUNCTION(SDL_OpenAudioDevice);
DECL_FUNCTION(SDL_CloseAudioDevice);
DECL_FUNCTION(SDL_PauseAudioDevice);
DECL_FUNCTION(SDL_QueueAudio);
DECL_FUNCTION(SDL_GetQueuedAudioSize);
DECL_FUNCTION(SDL_ClearQueuedAudio);
DECL_FUNCTION(SDL_GetError);
DECL_FUNCTION(SDL_ClearError);


#if USE_CALLBACK
static void _sdl_callback(void*, uint8_t*, int);
#endif
static float _sdl_set_volume(_driver_t*, _aaxRingBuffer*, ssize_t, uint32_t, unsigned int, float);

const char *_const_sdl_default_driver = DEFAULT_DEVNAME;
const char *_const_sdl_default_device = NULL;

static int
_aaxSDLDriverDetect(UNUSED(int mode))
{
   static int rv = AAX_FALSE;
   void *audio = NULL;

   _AAX_LOG(LOG_DEBUG, __func__);

   if (TEST_FOR_FALSE(rv) && !audio) {
     audio = _aaxIsLibraryPresent("SDL2-2.0", "0");
   }

   if (audio)
   {
      char *error;

      snprintf(_sdl_id_str, MAX_ID_STRLEN, "%s %s",
                               DEFAULT_RENDERER, SDL_ID_STRING);

      _aaxGetSymError(0);

      TIE_FUNCTION(SDL_Init);
      if (pSDL_Init)
      {
         TIE_FUNCTION(SDL_Quit);
         TIE_FUNCTION(SDL_GetVersion);
         TIE_FUNCTION(SDL_GetNumAudioDrivers);
         TIE_FUNCTION(SDL_GetAudioDriver);
         TIE_FUNCTION(SDL_GetNumAudioDevices);
         TIE_FUNCTION(SDL_GetAudioDeviceName);
         TIE_FUNCTION(SDL_AudioQuit);
         TIE_FUNCTION(SDL_AudioInit);
         TIE_FUNCTION(SDL_OpenAudioDevice);
         TIE_FUNCTION(SDL_CloseAudioDevice);
         TIE_FUNCTION(SDL_PauseAudioDevice);
         TIE_FUNCTION(SDL_QueueAudio);
         TIE_FUNCTION(SDL_GetQueuedAudioSize);
         TIE_FUNCTION(SDL_ClearQueuedAudio);
         TIE_FUNCTION(SDL_GetError);
         TIE_FUNCTION(SDL_ClearError);
      }

      error = _aaxGetSymError(0);
      if (!error) {
         rv = AAX_TRUE;
      }
   }

   return rv;
}

static void *
_aaxSDLDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   handle = (_driver_t *)calloc(1, sizeof(_driver_t));
   if (handle)
   {
//    char m = (mode == AAX_MODE_READ) ? 0 : 1;

      handle->driver = (char*)_const_sdl_default_driver;
      handle->devname = (char*)_const_sdl_default_device;
      handle->mode = mode;
      handle->no_periods = 2;
      handle->bits_sample = 16;
      handle->spec.freq = 48000;
      handle->spec.channels = 2;
      handle->spec.format = AUDIO_S16SYS;
#if USE_CALLBACK
      handle->spec.callback = _sdl_callback;
      handle->spec.userdata = handle;
#endif
      handle->period_frames = handle->spec.freq/DEFAULT_REFRESH;
      handle->spec.samples = get_pow2(handle->period_frames*handle->spec.channels);
      handle->buf_len = handle->spec.channels*handle->period_frames*handle->bits_sample/8;
      handle->fill.aim = FILL_FACTOR * DEFAULT_REFRESH;
      pSDL_Init(SDL_INIT_AUDIO);
   }

   return handle;
}

static void *
_aaxSDLDriverConnect(void *config, const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   int m = (mode == AAX_MODE_READ) ? 1 : 0;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      id = handle = _aaxSDLDriverNewHandle(mode);
   }

   if (handle)
   {
      handle->handle = config;
      if (renderer)
      {
         handle->driver = _aax_strdup(renderer);
         if (handle->driver)
         {
            handle->devname = strstr(handle->driver, ": ");
            if (handle->devname)
            {
               handle->devname[0] = 0;
               handle->devname += strlen(": ");
            }
         }
      }

      if (xid)
      {
         float f;
         char *s;
         int i;

         if (!handle->driver)
         {
            s = xmlAttributeGetString(xid, "name");
            if (s)
            {
               if (strcasecmp(s, "default")) {
                  handle->driver = _aax_strdup(s);
               }
               xmlFree(s);
            }
         }

         f = (float)xmlNodeGetDouble(xid, "frequency-hz");
         if (f)
         {
            if (f < (float)_AAX_MIN_MIXER_FREQUENCY)
            {
               _AAX_DRVLOG("sdl; frequency too small.");
               f = (float)_AAX_MIN_MIXER_FREQUENCY;
            }
            else if (f > (float)_AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_DRVLOG("sdl; frequency too large.");
               f = (float)_AAX_MAX_MIXER_FREQUENCY;
            }
            handle->spec.freq = f;
         }

         if (mode != AAX_MODE_READ)
         {
            i = xmlNodeGetInt(xid, "channels");
            if (i)
            {
               if (i < 1)
               {
                  _AAX_DRVLOG("sdl; no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_DRVLOG("sdl; no. tracks too great.");
                  i = _AAX_MAX_SPEAKERS;
               }
               handle->spec.channels = i;
            }
         }

         i = xmlNodeGetInt(xid, "bits-per-sample");
         if (i)
         {
            if (i != 16)
            {
               _AAX_DRVLOG("sdl; unsopported bits-per-sample");
               i = 16;
            }
            handle->bits_sample = i;
         }
      }
   }

   if (handle && !pSDL_AudioInit(handle->driver))
   {
     
      SDL_AudioSpec req, avail;
      uint32_t device;

      memcpy(&req, &handle->spec, sizeof(SDL_AudioSpec));
      device = pSDL_OpenAudioDevice(handle->devname, m, &req, &handle->spec,
              SDL_AUDIO_ALLOW_FREQUENCY_CHANGE|SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
      if (device != 0)
      {
         pSDL_CloseAudioDevice(device);

         req.freq = 0;
         req.channels = 1;
         device = pSDL_OpenAudioDevice(handle->devname, m, &req, &avail,
                                       SDL_AUDIO_ALLOW_ANY_CHANGE);
         handle->min_frequency = avail.freq;
         handle->min_tracks = avail.channels;
         pSDL_CloseAudioDevice(device);

         req.freq = _AAX_MAX_MIXER_FREQUENCY;
         req.channels = _AAX_MAX_SPEAKERS;
         device = pSDL_OpenAudioDevice(handle->devname, m, &req, &avail,
                                       SDL_AUDIO_ALLOW_ANY_CHANGE);
         handle->max_frequency = avail.freq;
         handle->max_tracks = avail.channels;
         pSDL_CloseAudioDevice(device);
      }
      else
      {
         /* free memory allocations */
         _aaxSDLDriverDisconnect(handle);
         handle = NULL;
      }
      pSDL_AudioQuit();
   }

   return (void *)handle;
}

static int
_aaxSDLDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __func__);

   if (handle)
   {
      if (handle->devnum)
      {
         pSDL_CloseAudioDevice(handle->devnum);
         pSDL_AudioQuit();
      }
      pSDL_Quit();

      if (handle->driver != _const_sdl_default_driver) {
         free(handle->driver);
      }

      if (handle->render)
      {
         handle->render->close(handle->render->id);
         free(handle->render);
      }

      free(handle->ifname[(handle->mode == AAX_MODE_READ) ? 1 : 0]);
      free(handle);
   }

   return AAX_TRUE;
}

static int
_aaxSDLDriverSetup(const void *id, UNUSED(float *refresh_rate), int *fmt,
                      unsigned int *tracks, float *speed, UNUSED(int *bitrate),
                      int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
   int m = (handle->mode == AAX_MODE_READ) ? 1 : 0;
   int rv = AAX_FALSE;

   *fmt = AAX_PCM16S;

   if (handle->devnum == 0 && !pSDL_AudioInit(handle->driver))
   {
      uint32_t period_frames;
      unsigned int periods;
      SDL_AudioSpec req;

      req.freq = (unsigned int)*speed;
      req.channels = *tracks;
      if (req.channels > handle->spec.channels) {
         req.channels = handle->spec.channels;
      }

      periods = handle->no_periods;
      if (!registered) {
         period_frames = get_pow2((size_t)rintf(req.freq/(*refresh_rate*periods)));
      } else {
         period_frames = get_pow2((size_t)rintf((req.freq*periods)/period_rate));
      }

      handle->latency = 1.0f / *refresh_rate;

      memcpy(&req, &handle->spec, sizeof(SDL_AudioSpec));
      req.format = AUDIO_S16SYS;
      req.samples = get_pow2(period_frames*req.channels);

      handle->devnum = pSDL_OpenAudioDevice(handle->devname, m, &req, &handle->spec, 0);
      if (handle->devnum != 0)
      {
         handle->period_frames = handle->spec.samples/handle->spec.channels;
         handle->buf_len = handle->spec.channels*handle->period_frames*handle->bits_sample/8;

         *speed = handle->spec.freq;
         *tracks = handle->spec.channels;
         if (!registered) {
            *refresh_rate = handle->spec.freq/(float)handle->period_frames;
         } else {
            *refresh_rate = period_rate;
         }

         handle->fill.aim = (float)period_frames/(float)handle->spec.freq;
         if (handle->fill.aim > 0.02f) {
            handle->fill.aim += 0.01f; // add 10ms
         } else {
            handle->fill.aim *= FILL_FACTOR;
         }
         handle->latency = handle->fill.aim;

         handle->render = _aaxSoftwareInitRenderer(handle->latency,
                                                handle->mode, registered);
         if (handle->render)
         {
            const char *rstr = handle->render->info(handle->render->id);
            SDL_version v;

            pSDL_GetVersion(&v);
            snprintf(_sdl_id_str, MAX_ID_STRLEN, "%s %i.%i.%i %s",
                     DEFAULT_RENDERER, v.major, v.minor, v.patch, rstr);
            rv = AAX_TRUE;
         }
         else {
            _AAX_DRVLOG("unable to get the renderer");
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
         snprintf(str,255,"  output renderer: '%s'", handle->driver);
      } else {
         snprintf(str,255,"  input renderer: '%s'", handle->driver);
      }
      _AAX_SYSLOG(str);
      snprintf(str,255, "  devname: '%s'", handle->devname);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  playback rate: %5i hz", handle->spec.freq);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  buffer size: %i bytes", handle->period_frames*handle->spec.channels*handle->bits_sample/8);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  latency: %3.2f ms",  1e3*handle->latency);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  no. periods: %i", handle->no_periods);
      _AAX_SYSLOG(str);
      snprintf(str,255,"  timer based: yes");
      _AAX_SYSLOG(str);
      snprintf(str,255,"  channels: %i, bytes/sample: %i\n", handle->spec.channels, handle->bits_sample/8);
      _AAX_SYSLOG(str);

#if 0
 printf("driver settings:\n");
 if (handle->mode != AAX_MODE_READ) {
    printf("  output renderer: '%s'\n", handle->driver);
 } else {
    printf("  input renderer: '%s'\n", handle->driver);
 }
 printf("  device: '%s'\n", handle->devname);
 printf("  playback rate: %5i Hz\n", handle->spec.freq);
 printf("  buffer size: %u bytes\n", handle->period_frames*handle->spec.channels*handle->bits_sample/8);
 printf("  latency:  %5.2f ms\n", 1e3*handle->latency);
 printf("  no_periods: %i\n", handle->no_periods);
 printf("  timer based: yes\n");
 printf("  channels: %i, bytes/sample: %i\n", handle->spec.channels, handle->bits_sample/8);
#endif
   }
   while (0);

   return rv;
}

static ssize_t
_aaxSDLDriverCapture(const void *id, void **data, ssize_t *offset, size_t *frames, void *scratch, size_t scratchlen, float gain, UNUSED(char batched))
{
   _driver_t *handle = (_driver_t *)id;
   size_t nframes = *frames;
   int tracks;

   *offset = 0;
   if ((handle->mode != 0) || (frames == 0) || (data == 0)) {
     return AAX_FALSE;
   }

   if (nframes == 0) {
      return AAX_TRUE;
   }

   *frames = 0;
   tracks = handle->spec.channels;


   *frames = nframes;
   return AAX_TRUE;
}

static size_t
_aaxSDLDriverPlayback(const void *id, void *s, UNUSED(float pitch), float gain,
                         UNUSED(char batched))
{
   _aaxRingBuffer *rb = (_aaxRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   ssize_t offs, outbuf_size, period_frames;
   unsigned int no_tracks, frame_sz;
   const int32_t **sbuf;
   char *data;
   int rv = 0;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rb);
   assert(id != 0);

   if (handle->mode == 0)
      return 0;

   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
   period_frames = rb->get_parami(rb, RB_NO_SAMPLES);
   frame_sz = no_tracks*handle->bits_sample/8;

   outbuf_size = period_frames*frame_sz;
   if (handle->ptr == 0 || (handle->buf_len < outbuf_size))
   {
      char *p = 0;

      _aax_free(handle->ptr);
      handle->buf_len = outbuf_size;

      handle->ptr = (void**)_aax_malloc(&p, outbuf_size);
      handle->scratch = (char**)p;
   }

   offs = rb->get_parami(rb, RB_OFFSET_SAMPLES);
   period_frames -= offs;

   _sdl_set_volume(handle, rb, offs, period_frames, no_tracks, gain);

   data = (char*)handle->scratch;
   sbuf = (const int32_t**)rb->get_tracks_ptr(rb, RB_READ);
   _batch_cvt16_intl_24(data, sbuf, offs, no_tracks, period_frames);
   rb->release_tracks_ptr(rb);

#if !USE_CALLBACK
   rv = pSDL_QueueAudio(handle->devnum, data, period_frames*frame_sz);
   if (!rv) rv = period_frames; // *frame_sz;
#else
   rv = period_frames;
#endif

   return rv;
}

static char *
_aaxSDLGetName(const void *id, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   /* TODO: distinguish between playback and record */
   if (handle && handle->devname && (mode < AAX_MODE_WRITE_MAX)) {
      ret = _aax_strdup(handle->devname);
   }

   return ret;
}

_aaxRenderer*
_aaxSDLDriverRender(const void* config)
{
   _driver_t *handle = (_driver_t *)config;
   return handle->render;
}


static int
_aaxSDLDriverState(const void *id, enum _aaxDriverState state)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

   switch(state)
   {
   case DRIVER_PAUSE:
      if (handle)
      {
         pSDL_PauseAudioDevice(handle->devnum, 1);
         rv = AAX_TRUE;
      }
      break;
   case DRIVER_RESUME:
      if (handle)
      {
         pSDL_PauseAudioDevice(handle->devnum, 0);
         rv = AAX_TRUE;
      }
      break;
   case DRIVER_AVAILABLE:
   case DRIVER_SHARED_MIXER:
   case DRIVER_SUPPORTS_PLAYBACK:
   case DRIVER_SUPPORTS_CAPTURE:
      rv = AAX_TRUE;
      break; 
   case DRIVER_NEED_REINIT:
   default:
      break;
   }
   return rv;
}

static float
_aaxSDLDriverParam(const void *id, enum _aaxDriverParam param)
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
      case DRIVER_MAX_PERIODS:
         rv = 2.0f;
         break;
      case DRIVER_MAX_SAMPLES:
         rv = AAX_FPINFINITE;
         break;
      case DRIVER_SAMPLE_DELAY:
         rv = (float)handle->period_frames;
         break;

		/* boolean */
      case DRIVER_TIMER_MODE:
      case DRIVER_BATCHED_MODE:
      case DRIVER_SHARED_MODE:
      default:
         break;
      }
   }
   return rv;
}

static char *
_aaxSDLDriverGetDevices(UNUSED(const void *id), int mode)
{
   static char names[2][1024] = { "\0\0", "\0\0" };
   static time_t t_previous[2] = { 0, 0 };
   unsigned char m = (mode == AAX_MODE_READ) ? 1 : 0;
   time_t t_now;

   t_now = time(NULL);
   if (t_now > (t_previous[m]+5))
   {
      int i, count, len = 1024;
      char *ptr;

      t_previous[m] = t_now;

      ptr = (char *)&names[m];
      *ptr = 0; *(ptr+1) = 0;

      count = pSDL_GetNumAudioDrivers();
      for (i=0; i<count; ++i)
      {
         const char* driver = pSDL_GetAudioDriver(i);
         int slen = strlen(driver)+1;

         if (slen > (len-1)) break;

         // We already provide a file and none backend
         if (!strcmp(driver, "disk") || !strcmp(driver, "dummy")) continue;

         // We already provide the alsa backend
         if (!strcmp(driver, "alsa")) continue;
         // We already provide the oss backend
         if (!strcmp(driver, "dsp") || !strcmp(driver, "dma")) continue;

         // We already provide the windows backend
         if (!strcmp(driver, "directsound") || !strcmp(driver, "winmm")) {
            continue;
         }

         snprintf(ptr, len, "%s", pSDL_GetAudioDriver(i));
         len -= slen;
         ptr += slen;
      }

      /* always end with "\0\0" no matter what */
      names[m][1022] = 0;
      names[m][1023] = 0;
   }

   return (char *)names[m];
}

static char *
_aaxSDLDriverGetInterfaces(const void *id, const char*devname, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned char m = (mode == AAX_MODE_READ) ? 1 : 0;
   char *rv = handle->ifname[m];

   if (!rv)
   {
      int res = pSDL_AudioInit(devname);
      if (!res)
      {
         char devlist[1024] = "\0\0";
         int i, count, len = 1024;
         char *ptr;

         ptr = devlist;

         count = pSDL_GetNumAudioDevices(m);
         for (i=0; i<count; ++i)
         {
            const char *name = pSDL_GetAudioDeviceName(i, m);
            int slen = strlen(name)+1;

            if (slen > (len-1)) break;

            snprintf(ptr, len, "%s", name);
            len -= slen;
            ptr += slen;
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
         pSDL_AudioQuit();
      }
   }

   return rv;
}

static char *
_aaxSDLDriverLog(const void *id, UNUSED(int prio), UNUSED(int type), const char *str)
{
   _driver_t *handle = (_driver_t *)id;
   static char _errstr[256] = "\0";

   if (str)
   {
      size_t len = _MIN(strlen(str)+1, 256);

      memcpy(_errstr, str, len);
      _errstr[255] = '\0';  /* always null terminated */

      __aaxDriverErrorSet(handle->handle, AAX_BACKEND_ERROR, (char*)&_errstr);
      _AAX_SYSLOG(_errstr);
   }

   return (char*)&_errstr;
}

static float
_sdl_set_volume(UNUSED(_driver_t *handle), _aaxRingBuffer *rb, ssize_t offset, uint32_t period_frames, UNUSED(unsigned int no_tracks), float volume)
{
   float gain = fabsf(volume);
   float rv = 0;

// TODO: hardware volume if available

      /* software volume fallback */
   if (rb && fabsf(gain - 1.0f) > 0.05f) {
      rb->data_multiply(rb, offset, period_frames, gain);
   }

   return rv;
}

/*-------------------------------------------------------------------------- */

#if USE_CALLBACK
static void
_sdl_callback(void *handle, uint8_t *data, int len)
{
   void *data = (char*)handle->scratch;
}
#endif

void *
_aaxSDLDriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   _intBufferData *dptr_sensor;
   const _aaxDriverBackend *be;
   _aaxRingBuffer *dest_rb;
   _aaxAudioFrame *mixer;
   _driver_t *be_handle;
   size_t period_frames;
   unsigned int wait_us;
   unsigned int frame_sz;
   float delay_sec;
   const void *id;
   int stdby_time;
   char state;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
   }

   delay_sec = 1.0f/handle->info->period_rate;

   be = handle->backend.ptr;
   id = handle->backend.handle;         // Required for _AAX_DRVLOG
   be_handle = (_driver_t *)handle->backend.handle;
   frame_sz = be_handle->spec.channels*be_handle->bits_sample/8;

   dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _sensor_t* sensor = _intBufGetDataPtr(dptr_sensor);

      mixer = sensor->mixer;

      dest_rb = be->get_ringbuffer(REVERB_EFFECTS_TIME, mixer->info->mode);
      if (dest_rb)
      {
         dest_rb->set_format(dest_rb, AAX_PCM24S, AAX_TRUE);
         dest_rb->set_parami(dest_rb, RB_NO_TRACKS, mixer->info->no_tracks);
         dest_rb->set_paramf(dest_rb, RB_FREQUENCY, mixer->info->frequency);
         dest_rb->set_paramf(dest_rb, RB_DURATION_SEC, delay_sec);
         dest_rb->init(dest_rb, AAX_TRUE);
         dest_rb->set_state(dest_rb, RB_STARTED);

         handle->ringbuffer = dest_rb;
      }
      _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

      if (!dest_rb) {
         return NULL;
      }
   }
   else {
      return NULL;
   }

   be->state(handle->backend.handle, DRIVER_PAUSE);
   state = AAX_SUSPENDED;

   wait_us = delay_sec*1000000;
   period_frames = dest_rb->get_parami(dest_rb, RB_NO_SAMPLES);
   stdby_time = (int)(delay_sec*1000);
   _aaxMutexLock(handle->thread.signal.mutex);
   while TEST_FOR_TRUE(handle->thread.started)
   {
      _driver_t *be_handle = (_driver_t *)handle->backend.handle;
     uint32_t avail;

      _aaxMutexUnLock(handle->thread.signal.mutex);

      avail = pSDL_GetQueuedAudioSize(be_handle->devnum)/frame_sz;
      if (avail > period_frames)
      {
         if (_IS_PLAYING(handle))
         {
            // TIMER_BASED
            usecSleep(wait_us);
         }
         else {
            msecSleep(stdby_time);
         }
      }

      _aaxMutexLock(handle->thread.signal.mutex);

      avail = pSDL_GetQueuedAudioSize(be_handle->devnum)/frame_sz;
      if (avail <= period_frames)
      {
         if TEST_FOR_FALSE(handle->thread.started) {
            break;
         }

         if (be->state(be_handle, DRIVER_AVAILABLE) == AAX_FALSE) {
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

            err = 0.40f*P + 0.97f*I;
            wait_us = _MAX((delay_sec + err), 1e-6f) * 1000000.0f;
#if 0
 avail = pSDL_GetQueuedAudioSize(be_handle->devnum)/frame_sz;
 printf("target: %5.1f, res: %i, err: %- 5.1f, avail: %i\n", target*freq, res, err*freq, avail);
#endif
         }
#if 0
 printf("state: %i, paused: %i\n", state, _IS_PAUSED(handle));
 printf("playing: %i, standby: %i\n", _IS_PLAYING(handle), _IS_STANDBY(handle));
#endif
      }

      if (handle->finished) {
         _aaxSemaphoreRelease(handle->finished);
      }
   }

   handle->ringbuffer = NULL;
   be->destroy_ringbuffer(dest_rb);
   _aaxMutexUnLock(handle->thread.signal.mutex);

   return handle;
}

