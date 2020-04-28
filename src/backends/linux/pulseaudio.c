/*
 * Copyright 2019 by Erik Hofman.
 * Copyright 2019 by Adalin B.V.
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_PULSE_PULSEAUDIO_H
# ifdef HAVE_RMALLOC_H
#  include <rmalloc.h>
# else
#  if HAVE_STRINGS_H
#   include <strings.h>
#  endif
#  include <string.h>		/* strstr, strncmp */
# endif
#include <stdarg.h>		/* va_start */

# include <pulse/simple.h>
# include <pulse/error.h>

# include <aax/aax.h>
# include <xml.h>

# include <base/types.h>
# include <base/logging.h>
# include <base/dlsym.h>

# include <ringbuffer.h>
# include <arch.h>
# include <api.h>

# include <software/renderer.h>
# include "audio.h"

#define TIMER_BASED 		AAX_FALSE

# define MAX_NAME		40
# define MAX_DEVICES		16
# define DEFAULT_PERIODS	2
# define DEFAULT_DEVNAME	"default"
# define DEFAULT_PCM_NUM	0
# define MAX_ID_STRLEN 		96

# define DEFAULT_RENDERER	"PulseAudio"

static char *_aaxPulseAudioDriverLogVar(const void *, const char *, ...);

# define FILL_FACTOR		1.5f
# define DEFAULT_REFRESH	25.0f
# define _AAX_DRVLOG(a)		_aaxPulseAudioDriverLog(id, 0, 0, a)
# define HW_VOLUME_SUPPORT(a)	((a->mixfd >= 0) && a->volumeMax)

_aaxDriverDetect _aaxPulseAudioDriverDetect;
static _aaxDriverNewHandle _aaxPulseAudioDriverNewHandle;
static _aaxDriverGetDevices _aaxPulseAudioDriverGetDevices;
static _aaxDriverGetInterfaces _aaxPulseAudioDriverGetInterfaces;
static _aaxDriverConnect _aaxPulseAudioDriverConnect;
static _aaxDriverDisconnect _aaxPulseAudioDriverDisconnect;
static _aaxDriverSetup _aaxPulseAudioDriverSetup;
static _aaxDriverCaptureCallback _aaxPulseAudioDriverCapture;
static _aaxDriverPlaybackCallback _aaxPulseAudioDriverPlayback;
static _aaxDriverGetName _aaxPulseAudioDriverGetName;
static _aaxDriverRender _aaxPulseAudioDriverRender;
static _aaxDriverState _aaxPulseAudioDriverState;
static _aaxDriverParam _aaxPulseAudioDriverParam;
static _aaxDriverLog _aaxPulseAudioDriverLog;

static char _pulseaudio_id_str[MAX_ID_STRLEN] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxPulseAudioDriverBackend =
{
   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_pulseaudio_id_str,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxPulseAudioDriverDetect,
   (_aaxDriverNewHandle *)&_aaxPulseAudioDriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxPulseAudioDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxPulseAudioDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxPulseAudioDriverGetName,
   (_aaxDriverRender *)&_aaxPulseAudioDriverRender,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxPulseAudioDriverConnect,
   (_aaxDriverDisconnect *)&_aaxPulseAudioDriverDisconnect,
   (_aaxDriverSetup *)&_aaxPulseAudioDriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxPulseAudioDriverCapture,
   (_aaxDriverPlaybackCallback *)&_aaxPulseAudioDriverPlayback,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,
   NULL,

   ( _aaxDriverGetSetSources*)_aaxSoftwareDriverGetSetSources,

   (_aaxDriverState *)&_aaxPulseAudioDriverState,
   (_aaxDriverParam *)&_aaxPulseAudioDriverParam,
   (_aaxDriverLog *)&_aaxPulseAudioDriverLog
};

typedef struct
{
   void *handle;
   char *name;
   _aaxRenderer *render;
   enum aaxRenderMode mode;

   pa_simple *str;
   pa_sample_spec spec;

   size_t threshold;		/* sensor buffer threshold for padding */
   float padding;		/* for sensor clock drift correction   */

   float latency;
// float frequency_hz;		/* located int spec */
   float refresh_rate;
// unsigned int format;		/* located int spec */
// unsigned int no_tracks;	/* located int spec */
   ssize_t period_frames;
   ssize_t period_frames_actual;
   char no_periods;
   char bits_sample;

   char use_timer;
   char prepared;
   char pause;
   char can_pause;

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

   /* capabilities */
   unsigned int min_frequency;
   unsigned int max_frequency;
   unsigned int min_periods;
   unsigned int max_periods;
   unsigned int min_tracks;
   unsigned int max_tracks;

} _driver_t;

#if 0
typedef struct
{
   char *devices;
   pa_threaded_mainloop *loop;
} _sink_info_t;
#endif

#undef DECL_FUNCTION
#define DECL_FUNCTION(f) static __typeof__(f) * p##f
DECL_FUNCTION(pa_simple_new);
DECL_FUNCTION(pa_simple_get_latency);
DECL_FUNCTION(pa_simple_write);
DECL_FUNCTION(pa_simple_drain);
DECL_FUNCTION(pa_simple_free);
DECL_FUNCTION(pa_strerror);

#define MAKE_FUNC(a) DECL_FUNCTION(a)
// MAKE_FUNC(a_simple_new);
// MAKE_FUNC(a_simple_get_latency);
// MAKE_FUNC(pa_simple_write);
// MAKE_FUNC(a_simple_drain);
// MAKE_FUNC(pa_simple_free);
// MAKE_FUNC(pa_strerror);

static int detect_pcm(_driver_t*, char);
static float _pulseaudio_set_volume(_driver_t*, _aaxRingBuffer*, ssize_t, size_t, unsigned int, float);
static int _pulseaudio_get_volume(_driver_t*);


static const char *_const_pulseaudio_default_name = DEFAULT_DEVNAME;

int
_aaxPulseAudioDriverDetect(UNUSED(int mode))
{
   static void *audio = NULL;
   static int rv = AAX_FALSE;
   char *error = NULL;

   _AAX_LOG(LOG_DEBUG, __func__);

   if (TEST_FOR_FALSE(rv) && !audio) {
      audio = _aaxIsLibraryPresent("pulse-simple", "0");
      _aaxGetSymError(0);
   }

   if (audio && mode != -1)
   {
      TIE_FUNCTION(pa_simple_new);
      if (ppa_simple_new)
      {
         TIE_FUNCTION(pa_simple_get_latency);
         TIE_FUNCTION(pa_simple_write);
         TIE_FUNCTION(pa_simple_drain);
         TIE_FUNCTION(pa_simple_free);
         TIE_FUNCTION(pa_strerror);
      }

      error = _aaxGetSymError(0);
      if (!error) {
         rv = AAX_TRUE;
      }
   }
   else {
      rv = AAX_FALSE;
   }

   return rv;
}

static void *
_aaxPulseAudioDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (handle)
   {
//    char m = (mode == AAX_MODE_READ) ? 0 : 1;

      handle->name = NULL; // (char*)_const_pulseaudio_default_name;
      handle->spec.format = PA_SAMPLE_S16LE;
      handle->spec.rate = 44100;
      handle->spec.channels = 2;
      handle->bits_sample = 16;
      handle->no_periods = DEFAULT_PERIODS;
      handle->period_frames = handle->spec.rate/DEFAULT_REFRESH;
      handle->period_frames_actual = handle->period_frames;
      handle->buf_len = handle->spec.channels*handle->period_frames*handle->bits_sample/8;
      handle->mode = mode;

      // default period size is for 25Hz
      handle->fill.aim = FILL_FACTOR * DEFAULT_REFRESH;
#if 0
      handle->min_tracks = 1;
      handle->max_tracks = _AAX_MAX_SPEAKERS;
      handle->min_frequency = _AAX_MIN_MIXER_FREQUENCY;
      handle->max_frequency = _AAX_MAX_MIXER_FREQUENCY;
      handle->latency = 0;
#endif
   }

   return handle;
}


static void *
_aaxPulseAudioDriverConnect(void *config, const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      id = handle = _aaxPulseAudioDriverNewHandle(mode);
   }

   if (handle)
   {
      if (xid)
      {
         float f;
         char *s;
         int i;

         if (!handle->name)
         {
            s = xmlAttributeGetString(xid, "name");
            if (s)
            {
               if (strcasecmp(s, "default")) {
                  handle->name = _aax_strdup(s);
               }
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
            handle->use_timer = AAX_FALSE;
         }

         f = (float)xmlNodeGetDouble(xid, "frequency-hz");
         if (f)
         {
            if (f < (float)_AAX_MIN_MIXER_FREQUENCY)
            {
               _AAX_SYSLOG("PulseAudio: frequency too small.");
               f = (float)_AAX_MIN_MIXER_FREQUENCY;
            }
            else if (f > (float)_AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_SYSLOG("PulseAudio: frequency too large.");
               f = (float)_AAX_MAX_MIXER_FREQUENCY;
            }
            handle->spec.rate = f;
         }

         if (mode != AAX_MODE_READ)
         {
            i = xmlNodeGetInt(xid, "channels");
            if (i)
            {
               if (i < 1)
               {
                  _AAX_SYSLOG("no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_SYSLOG("no. tracks too great.");
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
               _AAX_SYSLOG("PulseAudio: unsopported bits-per-sample");
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

      if (renderer) {
         if (handle->name != _const_pulseaudio_default_name) {
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
         const char *agent = aaxGetVersionString((aaxConfig)id);
         int error;

         handle->handle = config;
         handle->str = ppa_simple_new(NULL,		// default server
                              agent,
                              PA_STREAM_PLAYBACK,
                              handle->name,
                              "playback",
                              &handle->spec,
                              NULL,			// default channel map
                              NULL,			// default attributes
                              &error);
printf("handle->str: %p, name: '%s'\n", handle->str, handle->name);
         if (handle->str == NULL)
         {
            _aaxPulseAudioDriverLogVar(id, "open: %s", ppa_strerror(error));
            free(handle);
            handle = NULL;
         }
      }
   }

printf("Detect: %p\n", handle);
   return (void *)handle;
}

static int
_aaxPulseAudioDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

   if (handle)
   {
      if (handle->str)
      {
         ppa_simple_drain(handle->str, NULL);
         ppa_simple_free(handle->str);
      }

      if (handle->name != _const_pulseaudio_default_name) {
         free(handle->name);
      }

      if (handle->render)
      {
         handle->render->close(handle->render->id);
         free(handle->render);
      }

      if (handle->ptr) free(handle->ptr);
      free(handle);

      return AAX_TRUE;
   }
   return AAX_FALSE;
}

static int
_aaxPulseAudioDriverSetup(const void *id, float *refresh_rate, int *fmt,
                   unsigned int *channels, float *speed, UNUSED(int *bitrate),
                   int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

   assert(handle);

   if (handle->str)
   {
      unsigned int tracks, rate, bits, periods; // format;
      unsigned int period_frames, period_frames_actual;

      rate = (unsigned int)*speed;
      tracks = *channels;
      if (tracks > handle->spec.channels) {
         tracks = handle->spec.channels;
      }

      periods = handle->no_periods;
      if (!registered) {
         period_frames = get_pow2((size_t)rintf(rate/(*refresh_rate)));
      } else {
         period_frames = get_pow2((size_t)rintf((rate*periods)/period_rate));
      }
      period_frames_actual = period_frames;
      bits = aaxGetBitsPerSample(*fmt);

      handle->cvt_to_intl = _batch_cvt16_intl_24;

      handle->latency = 1.0f / *refresh_rate;
      if (handle->latency < 0.010f) {
         handle->use_timer = AAX_FALSE;
      }

      _pulseaudio_get_volume(handle);

      handle->spec.rate = rate;
      handle->spec.channels = tracks;
      handle->bits_sample = bits;
      handle->no_periods = periods;
      handle->period_frames = period_frames;
      handle->period_frames_actual = period_frames_actual;
      handle->threshold = 5*period_frames/4;
#if 1
 printf("   frequency           : %i\n", handle->spec.rate);
 printf("   no. channels:         %i\n", handle->spec.channels);
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
            pa_usec_t latency;
            _aaxRingBuffer *rb;
            int i, error;

            rb = _aaxRingBufferCreate(0.0f, m);
            if (rb)
            {
               rb->set_format(rb, AAX_PCM24S, AAX_TRUE);
               rb->set_parami(rb, RB_NO_TRACKS, handle->spec.channels);
               rb->set_paramf(rb, RB_FREQUENCY, handle->spec.rate);
               rb->set_parami(rb, RB_NO_SAMPLES, handle->period_frames);
               rb->init(rb, AAX_TRUE);
               rb->set_state(rb, RB_STARTED);

               for (i=0; i<handle->no_periods; i++) {
                  _aaxPulseAudioDriverPlayback(handle, rb, 1.0f, 0.0f, 0);
               }
               _aaxRingBufferFree(rb);
            }

            latency = ppa_simple_get_latency(handle->str, &error);
            if (latency != (pa_usec_t) -1) {
               handle->latency = (float)latency*1e-6f;
            }
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
         snprintf(_pulseaudio_id_str, MAX_ID_STRLEN, "%s %s",
                                      DEFAULT_RENDERER, rstr);
         rv = AAX_TRUE;
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
      snprintf(str,255, "  playback rate: %5i hz", handle->spec.rate);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  buffer size: %zu bytes", handle->period_frames*handle->spec.channels*handle->bits_sample/8);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  latency: %3.2f ms",  1e3*handle->latency);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  no. periods: %i", handle->no_periods);
      _AAX_SYSLOG(str);
      snprintf(str,255,"  timer based: %s",handle->use_timer?"yes":"no");
      _AAX_SYSLOG(str);
      snprintf(str,255,"  channels: %i, bytes/sample: %i\n", handle->spec.channels, handle->bits_sample/8);
      _AAX_SYSLOG(str);
      snprintf(str,255,"  can pause: %i\n", handle->can_pause);
      _AAX_SYSLOG(str);

#if 1
 printf("driver settings:\n");
 if (handle->mode != AAX_MODE_READ) {
    printf("  output renderer: '%s'\n", handle->name);
 } else {
    printf("  input renderer: '%s'\n", handle->name);
 }
 printf("  playback rate: %5i Hz\n", handle->spec.rate);
 printf("  buffer size: %zu bytes\n", handle->period_frames*handle->spec.channels*handle->bits_sample/8);
 printf("  latency:  %5.2f ms\n", 1e3*handle->latency);
 printf("  no_periods: %i\n", handle->no_periods);
 printf("  timer based: %s\n",handle->use_timer?"yes":"no");
 printf("  channels: %i, bytes/sample: %i\n", handle->spec.channels, handle->bits_sample/8);
 printf("  can pause: %i\n", handle->can_pause);
#endif
   }
   while (0);

   return rv;
}


static ssize_t
_aaxPulseAudioDriverCapture(UNUSED(const void *id), UNUSED(void **data), UNUSED(ssize_t *offset), UNUSED(size_t *frames), UNUSED(void *scratch), UNUSED(size_t scratchlen), UNUSED(float gain), UNUSED(char batched))
{
   return AAX_FALSE;
}

static size_t
_aaxPulseAudioDriverPlayback(const void *id, void *s, UNUSED(float pitch), float gain, UNUSED(char batched))
{
   _aaxRingBuffer *rb = (_aaxRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   ssize_t ret, offs, outbuf_size, period_frames;
   unsigned int no_tracks; // count;
   size_t bufsize; // avail
   const int32_t **sbuf;
   char *data;
   int error;
   int rv = 0;

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

   _pulseaudio_set_volume(handle, rb, offs, period_frames, no_tracks, gain);

   data = (char*)handle->scratch;
   sbuf = (const int32_t**)rb->get_tracks_ptr(rb, RB_READ);
   handle->cvt_to_intl(data, sbuf, offs, no_tracks, period_frames);
   rb->release_tracks_ptr(rb);

   bufsize = no_tracks*period_frames*handle->bits_sample/8;
   ret = ppa_simple_write(handle->str, data, bufsize, &error);
   if (ret == 0) {
      rv = bufsize;
   } else {
      _aaxPulseAudioDriverLogVar(id, "write: %s", ppa_strerror(error));
   }

   return rv;
}

static char *
_aaxPulseAudioDriverGetName(const void *id, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   if (handle && (mode < AAX_MODE_WRITE_MAX))
      ret = _aax_strdup(handle->name);

   return ret;
}

_aaxRenderer*
_aaxPulseAudioDriverRender(const void* config)
{
   _driver_t *handle = (_driver_t *)config;
   return handle->render;
}

static int
_aaxPulseAudioDriverState(const void *id, enum _aaxDriverState state)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

   switch(state)
   {
   case DRIVER_PAUSE:
      if (handle)
      {
#if 0
         if (!ppa_stream_is_corked(handle->str)) {
            ppa_stream_cork(handle->str, 1, NULL, NULL);
         }
#endif
         rv = AAX_TRUE;
      }
      break;
   case DRIVER_RESUME:
      if (handle)
      {
#if 0
         if (ppa_stream_is_corked(handle->str)) {
            ppa_stream_cork(handle->str, 0, NULL, NULL);
         }
#endif
         rv = AAX_TRUE;
      }
      break;
   case DRIVER_AVAILABLE:
      rv = AAX_TRUE;
      break;
   case DRIVER_SHARED_MIXER:
      rv = AAX_TRUE;
      break;
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
_aaxPulseAudioDriverParam(const void *id, enum _aaxDriverParam param)
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
         pa_usec_t latency = ppa_simple_get_latency(handle->str, NULL);
         if (latency != (pa_usec_t) -1) rv = latency*1e-6f;
         break;
      }

		/* boolean */
      case DRIVER_SHARED_MODE:
      case DRIVER_TIMER_MODE:
         rv = (float)AAX_TRUE;
         break;
      case DRIVER_BATCHED_MODE:
      default:
         break;
      }
   }
   return rv;
}

static char *
_aaxPulseAudioDriverGetDevices(UNUSED(const void *id), int mode)
{
   static char names[2][1024] = { "\0\0", "\0\0" };
   int m = (mode == AAX_MODE_READ) ? 0 : 1;
#if 0
   pa_threaded_mainloop *loop;

   loop = ppa_threaded_mainloop_new();
   if (loop && ppa_threaded_mainloop_start(loop) >= 0)
   {
      pa_context *context;

      ppa_threaded_mainloop_lock(loop);
      context = _aaxContextConnect(loop);
      if (context)
      {
         _sink_info_t si;
         pa_operation *opr;
#if 0
         pa_stream_flags_t flags;
         pa_sample_spec spec;
         pa_stream *stream;

         flags = PA_STREAM_FIX_FORMAT | PA_STREAM_FIX_RATE |
                 PA_STREAM_FIX_CHANNELS | PA_STREAM_DONT_MOVE;

         spec.format = PA_SAMPLE_S16NE;
         spec.rate = 44100;
         spec.channels = (mode == AAX_MODE_READ) ? 1 : 2;
#endif

         si.devices = (char *)&names[m];
         si.loop = loop;

#if 0
         stream = _aaxStreamConnect(NULL, context, loop, &spec, flags, NULL, NULL, mode);
         if (stream)
         {
             if (mode == AAX_MODE_READ) {
                opr = ppa_context_get_source_info_by_name(context, ppa_stream_get_device_name(stream), source_device_callback, &si);
             } else {
                opr = ppa_context_get_sink_info_by_name(context, ppa_stream_get_device_name(stream), sink_device_callback, &si);
             }

             if (opr)
             {
                while(ppa_operation_get_state(opr) == PA_OPERATION_RUNNING) {
                   ppa_threaded_mainloop_wait(loop);
                }
                ppa_operation_unref(opr);
             }
             ppa_stream_disconnect(stream);
             ppa_stream_unref(stream);
             stream = NULL;
         }
#else
         if (mode == AAX_MODE_READ) {
            opr = ppa_context_get_source_info_list(context, source_device_callback, &si);
         } else {
            opr = ppa_context_get_sink_info_list(context, sink_device_callback, &si);
         }

         if (opr)
         {
            while(ppa_operation_get_state(opr) == PA_OPERATION_RUNNING) {
               ppa_threaded_mainloop_wait(loop);
            }
            ppa_operation_unref(opr);
         }
#endif

         ppa_context_disconnect(context);
         ppa_context_unref(context);
      }
      ppa_threaded_mainloop_unlock(loop);
      ppa_threaded_mainloop_stop(loop);
   }
   if (loop) {
      ppa_threaded_mainloop_free(loop);
   }
#endif

   return (char *)&names[m];
}

static char *
_aaxPulseAudioDriverGetInterfaces(UNUSED(const void *id), UNUSED(const char *devname), UNUSED(int mode))
{
// _driver_t *handle = (_driver_t *)id;
   return NULL;
}

static char *
_aaxPulseAudioDriverLogVar(const void *id, const char *fmt, ...)
{
   char _errstr[1024];
   va_list ap;

   _errstr[0] = '\0';
   va_start(ap, fmt);
   vsnprintf(_errstr, 1024, fmt, ap);

   // Whatever happen in vsnprintf, what i'll do is just to null terminate it
   _errstr[1023] = '\0';
   va_end(ap);

   return _aaxPulseAudioDriverLog(id, 0, -1, _errstr);
}

static char *
_aaxPulseAudioDriverLog(const void *id, UNUSED(int prio), UNUSED(int type), const char *str)
{
   _driver_t *handle = id ? ((_driver_t *)id)->handle : NULL;
   static char _errstr[256];

   snprintf(_errstr, 256, "pulse audio: %s\n", str);
   _errstr[255] = '\0';  /* always null terminated */

   __aaxDriverErrorSet(handle, AAX_BACKEND_ERROR, (char*)&_errstr);
   _AAX_SYSLOG(_errstr);
#ifndef NDEBUG
   printf("%s", _errstr);
#endif

   return (char*)&_errstr;
}

static int
detect_pcm(_driver_t *handle, char m)
{
   int rv = AAX_TRUE;
   return rv;
}

static int
_pulseaudio_get_volume(UNUSED(_driver_t *handle))
{
   int rv = 0;
   return rv;
}

static float
_pulseaudio_set_volume(UNUSED(_driver_t *handle), _aaxRingBuffer *rb, ssize_t offset, size_t period_frames, UNUSED(unsigned int no_tracks), float volume)
{
   float gain = fabsf(volume);
   float rv = 0;

// TODO: hardware volume if available

      /* software volume fallback */
   if (rb && fabsf(gain - 1.0f) > LEVEL_32DB) {
      rb->data_multiply(rb, offset, period_frames, gain);
   }

   return rv;
}

/* ----------------------------------------------------------------------- */

#if 0
static void
context_state_callback(UNUSED(pa_context *context), void *ml)
{
   pa_threaded_mainloop *loop = ml;
   ppa_threaded_mainloop_signal(loop, 0);
}

static void
stream_state_callback(pa_stream *stream, void *ml)
{
    pa_threaded_mainloop *loop = ml;
    pa_stream_state_t state;

    state = ppa_stream_get_state(stream);
    if (state == PA_STREAM_READY || !PA_STREAM_IS_GOOD(state))
        ppa_threaded_mainloop_signal(loop, 0);
}
#endif

#if 0
static void
stream_buffer_attr_callback(pa_stream *stream, void *id)
{
   _driver_t *handle = (_driver_t*)id;
   handle->attr = *ppa_stream_get_buffer_attr(stream);
}
#endif

#if 0
static void
stream_success_callback(UNUSED(pa_stream *stream), UNUSED(int success), void *id)
{
   _driver_t *handle = (_driver_t*)id;
   ppa_threaded_mainloop_signal(handle->ml, 0);
}
#endif

#if 0
static void
sink_info_callback(UNUSED(pa_context *context), const pa_sink_info *info, int eol, void *id)
{
   _driver_t *handle = (_driver_t*)id;
   char chanmap_str[256] = "";
   const struct {
      const char *str;
      enum DevFmtChannels chans;
   } chanmaps[] = {
      { "front-left,front-right,front-center,lfe,rear-left,rear-right,side-left,side-right",
        DevFmtX71 },
      { "front-left,front-right,front-center,lfe,rear-center,side-left,side-right",
        DevFmtX61 },
      { "front-left,front-right,front-center,lfe,rear-left,rear-right",
        DevFmtX51 },
      { "front-left,front-right,front-center,lfe,side-left,side-right",
        DevFmtX51Side },
      { "front-left,front-right,rear-left,rear-right", DevFmtQuad },
      { "front-left,front-right", DevFmtStereo },
      { "mono", DevFmtMono },
      { NULL, 0 }
   };
   int i;

   if (eol)
   {
      ppa_threaded_mainloop_signal(handle->ml, 0);
      return;
   }

   for(i = 0;chanmaps[i].str;i++)
   {
      pa_channel_map map;
      if (!pa_channel_map_parse(&map, chanmaps[i].str))
         continue;

      if (pa_channel_map_equal(&info->channel_map, &map)
         || (pa_channel_map_superset &&
             ppa_channel_map_superset(&info->channel_map, &map))
          )
      {
         device->FmtChans = chanmaps[i].chans;
         return;
      }
   }

// ppa_channel_map_snprint(chanmap_str, sizeof(chanmap_str), &info->channel_map);
// ERR("Failed to find format for channel map:\n    %s\n", chanmap_str);
   _AAX_DRVLOG("Failed to find format for channel map.");
}
#endif

#if 0
static void
sink_device_callback(UNUSED(pa_context *context), const pa_sink_info *info, int eol, void *si)
{
   _sink_info_t *sink = si;
   size_t slen, len;
   char *ptr;

   if (eol)
   {
      ppa_threaded_mainloop_signal(sink->loop, 0);
      return;
   }

   ptr = memmem(sink->devices, MAX_DEVICES_LIST, "\0\0", 2);
   assert(ptr);

   slen = ptr-sink->devices;
   if (slen) ptr++;

   len = MAX_DEVICES_LIST - slen;
   slen = strlen(info->description)+1;
   if (len > slen+1)
   {
      snprintf(ptr, len, "%s", info->description);
      ptr += slen;
      *ptr = '\0';
   }
}

static void
source_device_callback(UNUSED(pa_context *context), const pa_source_info *info, int eol, void *si)
{
   _sink_info_t *source = si;
   size_t slen, len;
   char *ptr;

   if (eol)
   {
      ppa_threaded_mainloop_signal(source->loop, 0);
      return;
   }

   ptr = memmem(source->devices, MAX_DEVICES_LIST, "\0\0", 2);
   assert(ptr);

   slen = ptr-source->devices;
   if (slen) ptr++;

   len = MAX_DEVICES_LIST - slen;
   slen = strlen(info->description)+1;
   if (len > slen+1)
   {
      snprintf(ptr, len, "%s", info->description);
      ptr += slen;
      *ptr = '\0';
   }
}
#endif

#if 0
static void
source_device_callback(UNUSED(pa_context *context), const pa_source_info *info, int eol, void *id)
{
   _driver_t *handle = (_driver_t*)id;
// void *temp;
// unsigned i;

   if (eol)
   {
      ppa_threaded_mainloop_signal(handle->ml, 0);
      return;
   }

#if 0
   for(i=0; i<numCaptureDevNames; ++i)
   {
      if (strcmp(info->name, allCaptureDevNameMap[i].device_name) == 0) {
         return;
      }
   }

   temp = realloc(allCaptureDevNameMap, (numCaptureDevNames+1) * sizeof(*allCaptureDevNameMap));
   if (temp)
   {
      allCaptureDevNameMap = temp;
      allCaptureDevNameMap[numCaptureDevNames].name = strdup(info->description);
      allCaptureDevNameMap[numCaptureDevNames].device_name = strdup(info->name);
      numCaptureDevNames++;
   }
#endif
}
#endif

#if 0
static void
sink_name_callback(UNUSED(pa_context *context), const pa_sink_info *info, int eol, void *id)
{
   _driver_t *handle = (_driver_t*)id;

   if (eol)
   {
      ppa_threaded_mainloop_signal(handle->ml, 0);
      return;
   }

   free(handle->name);
   handle->name = strdup(info->description);
}
#endif

#if 0
static void
source_name_callback(UNUSED(pa_context *context), const pa_source_info *info, int eol, void *id)
{
   _driver_t *handle = (_driver_t*)id;

   if (eol)
   {
      ppa_threaded_mainloop_signal(handle->ml, 0);
      return;
   }

   free(handle->name);
   handle->name = strdup(info->description);
}
#endif

#if 0
static pa_context *
_aaxContextConnect(pa_threaded_mainloop *loop)
{
   static char pulse_avail = AAX_TRUE;
   const char *name = AAX_LIBRARY_STR;
   char buf[PATH_MAX];
   pa_context  *rv = NULL;

   if (!pulse_avail)
   {
      static time_t t_previous = 0;
      time_t t_now = time(NULL);
      if (t_now > (t_previous+1))
      {
         t_previous = t_now;
         pulse_avail = AAX_TRUE;
      }
      return rv;
   }

   assert(loop);

   if (ppa_get_binary_name && ppa_get_binary_name(buf, PATH_MAX)) {
      name = ppa_path_get_filename(buf);
   }

   rv = ppa_context_new(ppa_threaded_mainloop_get_api(loop), name);
   if (rv)
   {
      char *srv = NULL; // connect to the default server (for now)
      ppa_context_set_state_callback(rv, context_state_callback, loop);
      if (ppa_context_connect(rv, srv, 0, NULL) >= 0)
      {
         int state;
         while ((state = ppa_context_get_state(rv)) != PA_CONTEXT_READY)
         {
            if (!PA_CONTEXT_IS_GOOD(state)) {
               break;
            }
            ppa_threaded_mainloop_wait(loop);
         }
         ppa_context_set_state_callback(rv, NULL, NULL);
      }
      else
      {
         pulse_avail = AAX_FALSE;
         ppa_context_unref(rv);
      }
   }
   return rv;
}


static pa_stream*
_aaxStreamConnect(const char *devname, pa_context *ctx, pa_threaded_mainloop *ml, pa_sample_spec *spec, pa_stream_flags_t flags, pa_channel_map *router, pa_proplist *props, int mode)
{
   pa_stream *rv;

   rv = ppa_stream_new_with_proplist(ctx, AAX_LIBRARY_STR, spec, router, props);
   if (rv)
   {
      if (mode)
      {
         pa_stream_state_t state;

         ppa_stream_set_state_callback(rv, stream_state_callback, ml);

         if (mode == AAX_MODE_READ) {
            ppa_stream_connect_record(rv, devname, NULL, flags);
         } else {
            ppa_stream_connect_playback(rv, devname, NULL, flags, NULL, NULL);
         }

         while ((state = ppa_stream_get_state(rv)) != PA_STREAM_READY)
         {
            if (!PA_STREAM_IS_GOOD(state))
            {
               ppa_stream_unref(rv);
               break;
            }
            ppa_threaded_mainloop_wait(ml);
         }

         ppa_stream_set_state_callback(rv, NULL, NULL);
      }
   }
   return rv;
}
#endif

#endif /* HAVE_PULSE_PULSEAUDIO_H */
