/*
 * SPDX-FileCopyrightText: Copyright © 2018-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2018-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

#include <base/databuffer.h>
#include <base/dlsym.h>
#include <base/logging.h>
#include <base/memory.h>
#include <base/gmath.h>

#include <ringbuffer.h>
#include <arch.h>
#include <api.h>

#include <dsp/effects.h>
#include <software/renderer.h>
#include "audio.h"
#include "device.h"

#if defined(WIN32)
# include <Windows.h>
# include <VersionHelpers.h>
#endif


#define SDL_ID_STRING		"SDL"
#define MAX_ID_STRLEN		96

#define DEFAULT_OUTPUT_RATE	48000
#define DEFAULT_DEVNAME		NULL
#define DEFAULT_RENDERER	"SDL"
#define DEFAULT_REFRESH		25.0f

#define USE_PID			AAX_TRUE
#define USE_SDL_THREAD		AAX_TRUE
#define FILL_FACTOR		4.0f

#define _AAX_DRVLOG(a)		_aaxSDLDriverLog(id, 0, 0, a)

static _aaxDriverDetect _aaxSDLDriverDetect;
static _aaxDriverNewHandle _aaxSDLDriverNewHandle;
static _aaxDriverFreeHandle _aaxSDLDriverFreeHandle;
static _aaxDriverGetDevices _aaxSDLDriverGetDevices;
static _aaxDriverGetInterfaces _aaxSDLDriverGetInterfaces;
static _aaxDriverConnect _aaxSDLDriverConnect;
static _aaxDriverDisconnect _aaxSDLDriverDisconnect;
static _aaxDriverSetup _aaxSDLDriverSetup;
static _aaxDriverCaptureCallback _aaxSDLDriverCapture;
static _aaxDriverPlaybackCallback _aaxSDLDriverPlayback;
static _aaxDriverSetName _aaxSDLDriverSetName;
static _aaxDriverGetName _aaxSDLDriverGetName;
static _aaxDriverRender _aaxSDLDriverRender;
static _aaxDriverState _aaxSDLDriverState;
static _aaxDriverParam _aaxSDLDriverParam;
static _aaxDriverLog _aaxSDLDriverLog;
#if USE_SDL_THREAD
static _aaxDriverThread _aaxSDLDriverThread;
#endif

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
   (_aaxDriverFreeHandle *)&_aaxSDLDriverFreeHandle,
   (_aaxDriverGetDevices *)&_aaxSDLDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxSDLDriverGetInterfaces,

   (_aaxDriverSetName *)&_aaxSDLDriverSetName,
   (_aaxDriverGetName *)&_aaxSDLDriverGetName,
   (_aaxDriverRender *)&_aaxSDLDriverRender,
#if USE_SDL_THREAD
   (_aaxDriverThread *)&_aaxSDLDriverThread,
#else
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,
#endif

   (_aaxDriverConnect *)&_aaxSDLDriverConnect,
   (_aaxDriverDisconnect *)&_aaxSDLDriverDisconnect,
   (_aaxDriverSetup *)&_aaxSDLDriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxSDLDriverCapture,
   (_aaxDriverPlaybackCallback *)&_aaxSDLDriverPlayback,
   NULL,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,
   NULL,

   ( _aaxDriverGetSetSources*)_aaxSoftwareDriverGetSetSources,

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
   char bits_sample;
   float refresh_rate;
   float latency;

   _data_t *dataBuffer;
   _aaxMutex *mutex;

   char *ifname[2];

    /* capabilities */
   unsigned int min_frequency;
   unsigned int max_frequency;
   unsigned int min_tracks;
   unsigned int max_tracks;

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
DECL_FUNCTION(SDL_DequeueAudio);
DECL_FUNCTION(SDL_GetQueuedAudioSize);
DECL_FUNCTION(SDL_ClearQueuedAudio);
DECL_FUNCTION(SDL_GetError);
DECL_FUNCTION(SDL_ClearError);


static void _sdl_callback_write(void*, uint8_t*, int);
static float _sdl_set_volume(_driver_t*, _aaxRingBuffer*, ssize_t, uint32_t, unsigned int, float);

const char *_const_sdl_default_driver = DEFAULT_DEVNAME;
const char *_const_sdl_default_device = NULL;

static void *audio = NULL;

static int
_aaxSDLDriverDetect(UNUSED(int mode))
{
   static int rv = AAX_FALSE;

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
         TIE_FUNCTION(SDL_DequeueAudio);
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
      char m = (mode == AAX_MODE_READ) ? 1 : 0;
      int frame_sz;

      handle->driver = (char*)_const_sdl_default_driver;
      handle->devname = (char*)_const_sdl_default_device;
      handle->mode = mode;
      handle->bits_sample = 16;
      handle->spec.freq = 48000;
      handle->spec.channels = 2;
      handle->spec.format = is_bigendian() ? AUDIO_S16MSB : AUDIO_S16LSB;
      handle->spec.callback = m ? NULL : _sdl_callback_write;
      handle->spec.userdata = handle;
      handle->spec.samples = get_pow2(handle->spec.freq*handle->spec.channels/DEFAULT_REFRESH);

      frame_sz = handle->spec.channels*handle->bits_sample/8;
#if USE_PID
      handle->fill.aim = FILL_FACTOR*handle->spec.samples/handle->spec.freq;
      handle->latency = (float)handle->fill.aim/(float)frame_sz;
#else
      handle->latency = (float)handle->spec.samples/((float)handle->spec.freq*frame_sz);
#endif
      if (!m) {
         handle->mutex = _aaxMutexCreate(handle->mutex);
      }

      pSDL_Init(SDL_INIT_AUDIO);
   }

   return handle;
}

static int
_aaxSDLDriverFreeHandle(UNUSED(void *id))
{
   _aaxCloseLibrary(audio);
   audio = NULL;

   return AAX_TRUE;
}

static void *
_aaxSDLDriverConnect(void *config, const void *id, xmlId *xid, const char *renderer, enum aaxRenderMode mode)
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
   int rv = AAX_FALSE;
   char *ifname;

   _AAX_LOG(LOG_DEBUG, __func__);

   if (handle)
   {
      if (handle->devnum) {
         pSDL_CloseAudioDevice(handle->devnum);
      }

      if (handle->driver != _const_sdl_default_driver) {
         free(handle->driver);
      }

      if (handle->render)
      {
         handle->render->close(handle->render->id);
         free(handle->render);
      }

      if (handle->mutex) {
         _aaxMutexDestroy(handle->mutex);
      }
      _aaxDataDestroy(handle->dataBuffer);

      ifname = handle->ifname[(handle->mode == AAX_MODE_READ) ? 1 : 0];
      if (ifname) free(ifname);
      free(handle);

      pSDL_AudioQuit();
      pSDL_Quit();

      rv = AAX_TRUE;
   }

   return rv;
}

static int
_aaxSDLDriverSetup(const void *id, float *refresh_rate, int *fmt,
                      unsigned int *tracks, float *speed, UNUSED(int *bitrate),
                      int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
   int m = (handle->mode == AAX_MODE_READ) ? 1 : 0;
   int rv = AAX_FALSE;

   *fmt = AAX_PCM16S;

   if (handle->devnum == 0 && !pSDL_AudioInit(handle->driver))
   {
      uint32_t period_samples;
      SDL_AudioSpec req;
      int frame_sz;

      memcpy(&req, &handle->spec, sizeof(SDL_AudioSpec));

      req.freq = (unsigned int)*speed;
      req.channels = *tracks;
      if (req.channels > handle->spec.channels) {
         req.channels = handle->spec.channels;
      }

      if (*refresh_rate > 100) {
         *refresh_rate = 100;
      }

      if (!registered) {
         period_samples = get_pow2((size_t)rintf(req.freq/(*refresh_rate)));
      } else {
         period_samples = get_pow2((size_t)rintf(req.freq/period_rate));
      }

      frame_sz = req.channels*handle->bits_sample/8;
      req.format = is_bigendian() ? AUDIO_S16MSB : AUDIO_S16LSB;
      req.samples = period_samples*frame_sz;
      req.size = req.samples*handle->bits_sample/8;

      handle->devnum = pSDL_OpenAudioDevice(handle->devname, m,
                                            &req, &handle->spec, 0);
      if (handle->devnum != 0)
      {
         frame_sz = handle->spec.channels*handle->bits_sample/8;
#if 0
 printf("spec:\n");
 printf("   frequency: %i\n", handle->spec.freq);
 printf("   format:    %x\n", handle->spec.format);
 printf("   channels:  %i\n", handle->spec.channels);
 printf("   silence:   %i\n", handle->spec.silence);
 printf("   samples:   %i\n", handle->spec.samples);
 printf("   padding:   %i\n", handle->spec.padding);
 printf("   size:      %i\n", handle->spec.size);
#endif

         *speed = handle->spec.freq;
         *tracks = handle->spec.channels;
         if (!registered) {
            *refresh_rate = handle->spec.freq*frame_sz/(float)handle->spec.samples;
         } else {
            *refresh_rate = period_rate;
         }
         handle->refresh_rate = *refresh_rate;

#if USE_PID
         handle->fill.aim = FILL_FACTOR*handle->spec.samples/handle->spec.freq;
         handle->latency = (float)handle->fill.aim/(float)frame_sz;
#else
         handle->latency = (float)handle->spec.samples/((float)handle->spec.freq*frame_sz);
#endif

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
      snprintf(str,255, "  buffer size: %i bytes", handle->spec.samples*handle->bits_sample/8);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  latency: %3.2f ms",  1e3f*handle->latency);
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
 printf("  buffer size: %u bytes\n", handle->spec.samples*handle->bits_sample/8);
 printf("  latency:  %5.2f ms\n", 1e3f*handle->latency);
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
   size_t res, nframes = *frames;
   ssize_t offs = *offset;
   int tracks, frame_sz;

   *offset = 0;
   if ((handle->mode != 0) || (frames == 0) || (data == 0)) {
     return AAX_FALSE;
   }

   if (nframes == 0) {
      return AAX_TRUE;
   }

   *frames = 0;
   tracks = handle->spec.channels;
   frame_sz = tracks*handle->bits_sample/8;

   res = _MIN(nframes*frame_sz, scratchlen);
   res = pSDL_DequeueAudio(handle->devnum, scratch, res);

   nframes = res/frame_sz;
   _batch_cvt24_16_intl((int32_t**)data, scratch, offs, tracks, nframes);

   /* gain is negative for auto-gain mode */
   gain = fabsf(gain);
   if (gain < 0.99f || gain > 1.01f)
   {
      int t;
      for (t=0; t<tracks; t++)
      {
         int32_t *ptr = (int32_t*)data[t]+offs;
         _batch_imul_value(ptr, ptr, sizeof(int32_t), nframes, gain);
      }
   }
   *frames = nframes;

   return AAX_TRUE;
}

static size_t
_aaxSDLDriverPlayback(const void *id, void *s, UNUSED(float pitch), float gain,
                         UNUSED(char batched))
{
   _aaxRingBuffer *rb = (_aaxRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   ssize_t offs, period_frames;
   unsigned int no_tracks, frame_sz;
   const int32_t **sbuf;
   size_t size, free;
   int rv = 0;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rb);
   assert(id != 0);

   if (handle->mode == 0)
      return 0;

   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
   period_frames = rb->get_parami(rb, RB_NO_SAMPLES);
   frame_sz = no_tracks*handle->bits_sample/8;

   size = period_frames*frame_sz;
   if (handle->dataBuffer == 0 || (_aaxDataGetSize(handle->dataBuffer) < 2*FILL_FACTOR*size*frame_sz))
   {
      _aaxDataDestroy(handle->dataBuffer);
      handle->dataBuffer = _aaxDataCreate(1, 2*FILL_FACTOR*size, frame_sz);
      if (handle->dataBuffer == 0) return -1;
   }

   offs = rb->get_parami(rb, RB_OFFSET_SAMPLES);
   period_frames -= offs;
   size = period_frames*frame_sz;

   free = _aaxDataGetFreeSpace(handle->dataBuffer, 0);
   if (free > size)
   {
      unsigned char *data;

      _sdl_set_volume(handle, rb, offs, period_frames, no_tracks, gain);

      _aaxMutexLock(handle->mutex);
      data = _aaxDataGetPtr(handle->dataBuffer, 0);
      sbuf = (const int32_t**)rb->get_tracks_ptr(rb, RB_READ);
      _batch_cvt16_intl_24(data, sbuf, offs, no_tracks, period_frames);
      rb->release_tracks_ptr(rb);

      _aaxDataIncreaseOffset(handle->dataBuffer, 0, size);
      _aaxMutexUnLock(handle->mutex);

      rv = period_frames;
   }

   return rv;
}

static int
_aaxSDLDriverSetName(const void *id, int type, const char *name)
{
   _driver_t *handle = (_driver_t *)id;
   int ret = AAX_FALSE;
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
_aaxSDLDriverGetName(const void *id, int mode)
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
      case DRIVER_MAX_PERIODS:
         rv = 2.0f;
         break;
      case DRIVER_MAX_SOURCES:
         rv = ((_handle_t*)(handle->handle))->backend.ptr->getset_sources(0, 0);
         break;
      case DRIVER_MAX_SAMPLES:
         rv = AAX_FPINFINITE;
         break;
      case DRIVER_SAMPLE_DELAY:
         rv = (float)handle->spec.samples/handle->spec.channels;
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
      const char *env = getenv("AAX_SHOW_ALL_SDL_DEVICES");
      int i, count, len = 1024;
      char show_all, *ptr;

      t_previous[m] = t_now;
      show_all = (env && _aax_getbool(env)) ? AAX_TRUE : AAX_FALSE;

      ptr = (char *)&names[m];
      *ptr = 0; *(ptr+1) = 0;

      count = pSDL_GetNumAudioDrivers();
      for (i=0; i<count; ++i)
      {
         const char* driver = pSDL_GetAudioDriver(i);
         int slen = strlen(driver)+1;

         if (slen > (len-1)) break;

         if (!show_all)
         {
            // We already provide a file and none backend
            if (!strcmp(driver, "disk") || !strcmp(driver, "dummy")) continue;

            // We already provide the pipewire backend
            if (!strcmp(driver, "pipewire")) continue;
            // We already provide the pulseaudio backend
            if (!strcmp(driver, "pulseaudio")) continue;
            // We already provide the alsa backend
            if (!strcmp(driver, "alsa")) continue;
            // We already provide the oss backend
            if (!strcmp(driver, "dsp") || !strcmp(driver, "dma")) continue;

            // We already provide the windows backend
            if (!strcmp(driver, "directsound") || !strcmp(driver, "winmm")) {
#if defined(WIN32)
              // for XP use the directsound SDL fallback
              // for Vista and later: skip it.
              if (IsWindowsVistaOrGreater())
#endif
               continue;
            }
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

   if (rb && fabsf(gain - 1.0f) > LEVEL_32DB) {
      rb->data_multiply(rb, offset, period_frames, gain);
   }

   return rv;
}

/*-------------------------------------------------------------------------- */

static void
_sdl_callback_write(void *be_ptr, uint8_t *dst, int len)
{
   _driver_t *be_handle = (_driver_t *)be_ptr;
   _handle_t *handle = (_handle_t *)be_handle->handle;
   void *id = be_handle;

   if (_IS_PLAYING(handle))
   {
      assert(be_handle->mode != AAX_MODE_READ);
      assert(be_handle->dataBuffer);

      _aaxMutexLock(be_handle->mutex);

      if (_aaxDataGetOffset(be_handle->dataBuffer, 0) >= (size_t)len) {
         _aaxDataMove(be_handle->dataBuffer, 0, dst, len);
      }
      else {
#if 0
 printf("buffer underrun: avail: %zi, len: %i\n", _aaxDataGetOffset(be_handle->dataBuffer, 0), len);
#endif
         _AAX_DRVLOG("buffer underrun\n");
      }

      _aaxMutexUnLock(be_handle->mutex);
   }
}

#if USE_SDL_THREAD
static void *
_aaxSDLDriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   _intBufferData *dptr_sensor;
   const _aaxDriverBackend *be;
   _aaxRingBuffer *dest_rb;
   _aaxAudioFrame *smixer;
   _driver_t *be_handle;
   int state, tracks;
   float delay_sec;
   float freq;
   int res;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
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
         dest_rb->set_format(dest_rb, AAX_PCM24S, AAX_TRUE);
         dest_rb->set_paramf(dest_rb, RB_FREQUENCY, freq);
         dest_rb->set_paramf(dest_rb, RB_DURATION_SEC, delay_sec);
         dest_rb->init(dest_rb, AAX_TRUE);
         dest_rb->set_state(dest_rb, RB_STARTED);

         handle->ringbuffer = dest_rb;
         _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
      }
   }

   dest_rb = handle->ringbuffer;
   if (!dest_rb) {
      return NULL;
   }

   /* get real duration, it might have been altered for better performance */
   delay_sec = dest_rb->get_paramf(dest_rb, RB_DURATION_SEC);

   be->state(handle->backend.handle, DRIVER_PAUSE);
   state = AAX_SUSPENDED;

   be_handle = (_driver_t *)handle->backend.handle;
   be_handle->dataBuffer = _aaxDataCreate(1, 1, 1);

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
         do
         {
            float target, input, err, P, I;

            target = be_handle->fill.aim;
            input = (float)_aaxDataGetOffset(be_handle->dataBuffer, 0)/freq;
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
#endif

         if (handle->finished) {
            _aaxSemaphoreRelease(handle->finished);
         }
      }

      res = _aaxSignalWaitTimed(&handle->thread.signal, dt);
   }
   while (res == AAX_TIMEOUT || res == AAX_TRUE);

   _aaxMutexUnLock(handle->thread.signal.mutex);

   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      be->destroy_ringbuffer(handle->ringbuffer);
      handle->ringbuffer = NULL;
   }

   return handle;
}
#endif
