/*
 * Copyright 2022 by Erik Hofman.
 * Copyright 2022 by Adalin B.V.
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

#if HAVE_PIPEWIRE_H

# ifdef HAVE_RMALLOC_H
#  include <rmalloc.h>
# else
#  if HAVE_STRINGS_H
#   include <strings.h>
#  endif
#  include <string.h>		/* strstr, strncmp */
# endif

#include <aax/aax.h>
#include <xml.h>

// #include <base/types.h>
// #include <base/logging.h>
#include <base/memory.h>
#include <base/dlsym.h>
// #include <base/timer.h>

#include <backends/driver.h>
#include <ringbuffer.h>
// #include <arch.h>
#include <api.h>

#include <dsp/effects.h>
#include <software/renderer.h>
#include "pipewire.h"
#include "audio.h"


#define DEFAULT_RENDERER	"PipeWire"
#define MAX_ID_STRLEN		96

#define DEFAULT_OUTPUT_RATE	48000
#define DEFAULT_DEVNAME		"default"
#define DEFAULT_REFRESH		25.0

#define USE_PID			AAX_TRUE
#define USE_PIPEWIRE_THREAD	AAX_TRUE
#define CAPTURE_CALLBACK	AAX_TRUE
#define FILL_FACTOR		4.0f

#define PERIODS			2
#define CAPTURE_BUFFER_SIZE	(PERIODS*1024)
#define MAX_DEVICES_LIST	1024

#define _AAX_DRVLOG(a)         _aaxPipeWireDriverLog(id, 0, 0, a)
#define HW_VOLUME_SUPPORT(a)	((a->mixfd >= 0) && a->volumeMax)

#define PW_POD_BUFFER_LENGTH         1024
#define PW_THREAD_NAME_BUFFER_LENGTH 128

_aaxDriverDetect _aaxPipeWireDriverDetect;
static _aaxDriverNewHandle _aaxPipeWireDriverNewHandle;
static _aaxDriverGetDevices _aaxPipeWireDriverGetDevices;
static _aaxDriverGetInterfaces _aaxPipeWireDriverGetInterfaces;
static _aaxDriverConnect _aaxPipeWireDriverConnect;
static _aaxDriverDisconnect _aaxPipeWireDriverDisconnect;
static _aaxDriverSetup _aaxPipeWireDriverSetup;
static _aaxDriverCaptureCallback _aaxPipeWireDriverCapture;
static _aaxDriverPlaybackCallback _aaxPipeWireDriverPlayback;
static _aaxDriverSetName _aaxPipeWireDriverSetName;
static _aaxDriverGetName _aaxPipeWireDriverGetName;
static _aaxDriverRender _aaxPipeWireDriverRender;
static _aaxDriverState _aaxPipeWireDriverState;
static _aaxDriverParam _aaxPipeWireDriverParam;
static _aaxDriverLog _aaxPipeWireDriverLog;
#if USE_PIPEWIRE_THREAD
static _aaxDriverThread _aaxPipeWireDriverThread;
#endif

static char _pipewire_id_str[MAX_ID_STRLEN] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxPipeWireDriverBackend =
{
   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_pipewire_id_str,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxPipeWireDriverDetect,
   (_aaxDriverNewHandle *)&_aaxPipeWireDriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxPipeWireDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxPipeWireDriverGetInterfaces,

   (_aaxDriverSetName *)&_aaxPipeWireDriverSetName,
   (_aaxDriverGetName *)&_aaxPipeWireDriverGetName,
   (_aaxDriverRender *)&_aaxPipeWireDriverRender,
#if USE_PIPEWIRE_THREAD
   (_aaxDriverThread *)&_aaxPipeWireDriverThread,
#else
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,
#endif

   (_aaxDriverConnect *)&_aaxPipeWireDriverConnect,
   (_aaxDriverDisconnect *)&_aaxPipeWireDriverDisconnect,
   (_aaxDriverSetup *)&_aaxPipeWireDriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxPipeWireDriverCapture,
   (_aaxDriverPlaybackCallback *)&_aaxPipeWireDriverPlayback,
   NULL,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,
   NULL,

   ( _aaxDriverGetSetSources*)_aaxSoftwareDriverGetSetSources,

   (_aaxDriverState *)&_aaxPipeWireDriverState,
   (_aaxDriverParam *)&_aaxPipeWireDriverParam,
   (_aaxDriverLog *)&_aaxPipeWireDriverLog
};

typedef struct
{
   uint32_t id;
   void *handle;
   char *driver;
   char *devname;
   _aaxRenderer *render;

   struct pw_stream *pw;
   struct pw_context *ctx;
   struct pw_thread_loop *ml;
   int stream_init_status;

   struct {
      int32_t rate;
      uint8_t channels;
      unsigned int format;
   } spec;

// char no_periods;
   char bits_sample;
   unsigned int format;
   unsigned int samples;
   enum aaxRenderMode mode;
   float refresh_rate;
   float latency;

   _data_t *dataBuffer;
   _aaxMutex *mutex;

   _batch_cvt_to_intl_proc cvt_to_intl;
   _batch_cvt_from_intl_proc cvt_from_intl;

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

#undef DECL_FUNCTION
#define DECL_FUNCTION(f) static __typeof__(f) * p##f
DECL_FUNCTION(pw_init);
DECL_FUNCTION(pw_deinit);
DECL_FUNCTION(pw_thread_loop_new);
DECL_FUNCTION(pw_thread_loop_destroy);
DECL_FUNCTION(pw_thread_loop_stop);
DECL_FUNCTION(pw_thread_loop_get_loop);
DECL_FUNCTION(pw_thread_loop_lock);
DECL_FUNCTION(pw_thread_loop_unlock);
DECL_FUNCTION(pw_thread_loop_signal);
DECL_FUNCTION(pw_thread_loop_wait);
DECL_FUNCTION(pw_thread_loop_start);
DECL_FUNCTION(pw_context_new);
DECL_FUNCTION(pw_context_destroy);
DECL_FUNCTION(pw_context_connect);
DECL_FUNCTION(pw_proxy_add_object_listener);
DECL_FUNCTION(pw_proxy_get_user_data);
DECL_FUNCTION(pw_proxy_destroy);
DECL_FUNCTION(pw_core_disconnect);
DECL_FUNCTION(pw_stream_new_simple);
DECL_FUNCTION(pw_stream_destroy);
DECL_FUNCTION(pw_stream_connect);
DECL_FUNCTION(pw_stream_get_state);
DECL_FUNCTION(pw_stream_dequeue_buffer);
DECL_FUNCTION(pw_stream_queue_buffer);
DECL_FUNCTION(pw_properties_new);
DECL_FUNCTION(pw_properties_set);
DECL_FUNCTION(pw_properties_setf);
DECL_FUNCTION(pw_get_library_version);

static int hotplug_loop_init();
static void hotplug_loop_destroy();
static void _pipewire_detect_devices(char[2][MAX_DEVICES_LIST]);
static uint32_t _pipewire_get_id_by_name(const char*);
static int _pipewire_open(_driver_t*);

static char* _aaxPipeWireDriverLogVar(const void *, const char *, ...);
static enum aaxFormat _aaxPipeWireGetAAXFormat(enum spa_audio_format);
// static enum spa_audio_format _aaxPipeWireGetFormat(enum aaxFormat);

static char pipewire_initialized = AAX_FALSE;
static const char *_const_pipewire_default_name = DEFAULT_DEVNAME;
const char *_const_pipewire_default_device = NULL;

int
_aaxPipeWireDriverDetect(UNUSED(int mode))
{
   static void *audio = NULL;
   static int rv = AAX_FALSE;
   char *error = NULL;

   _AAX_LOG(LOG_DEBUG, __func__);

   if (TEST_FOR_FALSE(rv) && !audio) {
      audio = _aaxIsLibraryPresent(PIPEWIRE_LIBRARY, "0");
   }
   if (audio)
   {
      _aaxGetSymError(0);

      TIE_FUNCTION(pw_stream_new_simple);
      if (ppw_stream_new_simple)
      {
         TIE_FUNCTION(pw_init);
         TIE_FUNCTION(pw_deinit);
         TIE_FUNCTION(pw_thread_loop_new);
         TIE_FUNCTION(pw_thread_loop_destroy);
         TIE_FUNCTION(pw_thread_loop_stop);
         TIE_FUNCTION(pw_thread_loop_get_loop);
         TIE_FUNCTION(pw_thread_loop_lock);
         TIE_FUNCTION(pw_thread_loop_unlock);
         TIE_FUNCTION(pw_thread_loop_signal);
         TIE_FUNCTION(pw_thread_loop_wait);
         TIE_FUNCTION(pw_thread_loop_start);
         TIE_FUNCTION(pw_context_new);
         TIE_FUNCTION(pw_context_destroy);
         TIE_FUNCTION(pw_context_connect);
         TIE_FUNCTION(pw_proxy_add_object_listener);
         TIE_FUNCTION(pw_proxy_get_user_data);
         TIE_FUNCTION(pw_proxy_destroy);
         TIE_FUNCTION(pw_core_disconnect);
         TIE_FUNCTION(pw_stream_destroy);
         TIE_FUNCTION(pw_stream_connect);
         TIE_FUNCTION(pw_stream_get_state);
         TIE_FUNCTION(pw_stream_dequeue_buffer);
         TIE_FUNCTION(pw_stream_queue_buffer);
         TIE_FUNCTION(pw_properties_new);
         TIE_FUNCTION(pw_properties_set);
         TIE_FUNCTION(pw_properties_setf);
         TIE_FUNCTION(pw_get_library_version);
      }

      error = _aaxGetSymError(0);
      if (!error)
      {
         rv = AAX_TRUE;
      }
   }

   return rv;
}

static void *
_aaxPipeWireDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (handle)
   {
      int m = (mode == AAX_MODE_READ) ? 1 : 0;
      int err, frame_sz;

      handle->id = PW_ID_ANY;
      handle->driver = (char*)_const_pipewire_default_name;
      handle->mode = mode;
      handle->bits_sample = 16;
      handle->spec.rate = 44100;
      handle->spec.channels = 2;
      handle->spec.format = SPA_AUDIO_FORMAT_F32;
      handle->samples = get_pow2(handle->spec.rate*handle->spec.channels/DEFAULT_REFRESH);

      frame_sz = handle->spec.channels*handle->bits_sample/8;
#if USE_PID
      handle->fill.aim = FILL_FACTOR*handle->samples/handle->spec.rate;
      handle->latency = (float)handle->fill.aim/(float)frame_sz;
#else
      handle->latency = (float)handle->samples/(float)handle->spec.rate*frame_sz;
#endif

      if (!m) {
         handle->mutex = _aaxMutexCreate(handle->mutex);
      }

#if 1
      handle->min_tracks = 1;
      handle->max_tracks = _AAX_MAX_SPEAKERS;
      handle->min_frequency = _AAX_MIN_MIXER_FREQUENCY;
      handle->max_frequency = _AAX_MAX_MIXER_FREQUENCY;
#endif

      if (!pipewire_initialized)
      {
         ppw_init(NULL, NULL);
         pipewire_initialized = AAX_TRUE;

         err = hotplug_loop_init();
         if (err != 0)
         {
            _aaxPipeWireDriverLogVar(NULL, "Pipewire: hotplug loop error: %s",
                                     strerror(errno));
            _aaxPipeWireDriverDisconnect(handle);
         }
      }
   }

   return handle;
}


static void *
_aaxPipeWireDriverConnect(void *config, const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      id = handle = _aaxPipeWireDriverNewHandle(mode);
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
               _AAX_SYSLOG("pulse; frequency too small.");
               f = (float)_AAX_MIN_MIXER_FREQUENCY;
            }
            else if (f > (float)_AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_SYSLOG("pulse; frequency too large.");
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
                  _AAX_SYSLOG("pulse; no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_SYSLOG("pulse; no. tracks too great.");
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
               _AAX_SYSLOG("pulse; unsopported bits-per-sample");
               i = 16;
            }
         }
      }
   }

   return (void *)handle;
}

static int
_aaxPipeWireDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

   if (pipewire_initialized)
   {
      pipewire_initialized = AAX_FALSE;
      hotplug_loop_destroy();
      ppw_deinit();
   }

   if (handle)
   {
      if (handle->driver != _const_pipewire_default_name) {
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

      free(handle);

      return AAX_TRUE;
   }
   return AAX_FALSE;
}

static int
_aaxPipeWireDriverSetup(const void *id, float *refresh_rate, int *fmt,
                   unsigned int *tracks, float *speed, UNUSED(int *bitrate),
                   int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int period_samples;
   int32_t rate;
   uint8_t channels;
   unsigned int format;
   int frame_sz;
   int rv = AAX_FALSE;

   *fmt = AAX_PCM16S;

   rate = (unsigned int)*speed;
   channels = *tracks;
   if (channels > handle->spec.channels) {
      channels = handle->spec.channels;
   }

   if (*refresh_rate > 100) {
      *refresh_rate = 100;
   }

   if (!registered) {
      period_samples = get_pow2((size_t)rintf(rate/(*refresh_rate)));
   } else {
      period_samples = get_pow2((size_t)rintf(rate/period_rate));
   }
   format = SPA_AUDIO_FORMAT_F32;
   frame_sz = channels*handle->bits_sample/8;

   handle->spec.rate = rate;
   handle->spec.channels = channels;
   handle->spec.format = format;
   handle->samples = period_samples*frame_sz;
   handle->id = _pipewire_get_id_by_name(handle->driver);
   rv = _pipewire_open(handle);
   if (rv)
   {
      handle->samples = period_samples*frame_sz;
      handle->format = _aaxPipeWireGetAAXFormat(handle->spec.format);
      handle->bits_sample = aaxGetBitsPerSample(handle->spec.format);

      switch(handle->spec.format & AAX_FORMAT_NATIVE)
      {
      case AAX_PCM16S:
         handle->cvt_to_intl = _batch_cvt16_intl_24;
         handle->cvt_from_intl = _batch_cvt24_16_intl;
         break;
      case AAX_FLOAT:
         handle->cvt_to_intl = _batch_cvtps_intl_24;
         handle->cvt_from_intl = _batch_cvt24_ps_intl;
         break;
      case AAX_PCM32S:
         handle->cvt_to_intl = _batch_cvt24_intl_24;
         handle->cvt_from_intl = _batch_cvt24_24_intl;
         break;
      case AAX_PCM24S:
         handle->cvt_to_intl = _batch_cvt32_intl_24;
         handle->cvt_from_intl = _batch_cvt24_32_intl;
         break;
      case AAX_PCM24S_PACKED:
         handle->cvt_to_intl = _batch_cvt24_intl_24;
         handle->cvt_from_intl = _batch_cvt24_24_intl;
         break;
      case AAX_PCM8S:
         handle->cvt_to_intl = _batch_cvt8_intl_24;
         handle->cvt_from_intl = _batch_cvt24_8_intl;
         break;
      default:
         rv = AAX_FALSE;
         break;
      }
#if 0
 printf("spec:\n");
 printf("   frequency: %i\n", handle->spec.rate);
 printf("   format:    %x\n", handle->spec.format);
 printf("   channels:  %i\n", handle->spec.channels);
 printf("   samples:   %i\n", handle->samples);
#endif

      *speed = handle->spec.rate;
      *tracks = handle->spec.channels;
      if (!registered) {
         *refresh_rate = handle->spec.rate*frame_sz/(float)handle->samples;
      } else {
         *refresh_rate = period_rate;
      }
      handle->refresh_rate = *refresh_rate;

#if USE_PID
      handle->fill.aim = FILL_FACTOR*handle->samples/handle->spec.rate;
      handle->latency = (float)handle->fill.aim/(float)frame_sz;
      handle->latency *= handle->spec.channels/2;
#else
      handle->latency = (float)handle->samples/(float)handle->spec.rate*frame_sz;
#endif

      handle->render = _aaxSoftwareInitRenderer(handle->latency,
                                                handle->mode, registered);
      if (handle->render)
      {
         const char *rstr = handle->render->info(handle->render->id);

         snprintf(_pipewire_id_str, MAX_ID_STRLEN ,"%s %s %s",
                  DEFAULT_RENDERER, ppw_get_library_version(), rstr);
         rv = AAX_TRUE;
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
      snprintf(str,255, "  playback rate: %5i hz", handle->spec.rate);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  buffer size: %i bytes", handle->samples*handle->bits_sample/8);
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
 printf("  playback rate: %5i Hz\n", handle->spec.rate);
 printf("  buffer size: %u bytes\n", handle->samples*handle->bits_sample/8);
 printf("  latency:  %5.2f ms\n", 1e3f*handle->latency);
 printf("  timer based: yes\n");
 printf("  channels: %i, bytes/sample: %i\n", handle->spec.channels, handle->bits_sample/8);
#endif
   }
   while (0);

   return rv;
}


static ssize_t
_aaxPipeWireDriverCapture(const void *id, void **data, ssize_t *offset, size_t *frames, UNUSED(void *scratch), UNUSED(size_t scratchlen), float gain, UNUSED(char batched))
{
   return AAX_FALSE;
}

static size_t
_aaxPipeWireDriverPlayback(const void *id, void *src, UNUSED(float pitch), UNUSED(float gain), UNUSED(char batched))
{
   return AAX_FALSE;
}

static int
_aaxPipeWireDriverSetName(const void *id, int type, const char *name)
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
_aaxPipeWireDriverGetName(const void *id, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   /* TODO: distinguish between playback and record */
   if (handle && handle->driver && (mode < AAX_MODE_WRITE_MAX))
      ret = _aax_strdup(handle->driver);

   return ret;
}

_aaxRenderer*
_aaxPipeWireDriverRender(const void* config)
{
   _driver_t *handle = (_driver_t *)config;
   return handle->render;
}

static int
_aaxPipeWireDriverState(const void *id, enum _aaxDriverState state)
{
// _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

   switch(state)
   {
   case DRIVER_PAUSE:
      break;
   case DRIVER_RESUME:
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
_aaxPipeWireDriverParam(const void *id, enum _aaxDriverParam param)
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
         rv = (float)handle->samples/handle->spec.channels;
         break;

		/* boolean */
      case DRIVER_SHARED_MODE:
         rv = AAX_TRUE;
         break;
      case DRIVER_TIMER_MODE:
      case DRIVER_BATCHED_MODE:
      default:
         break;
      }
   }
   return rv;
}

static char *
_aaxPipeWireDriverGetDevices(const void *id, int mode)
{
   static char names[2][MAX_DEVICES_LIST] = {
     DEFAULT_DEVNAME"\0\0", DEFAULT_DEVNAME"\0\0"
   };
   static time_t t_previous = 0;
// _driver_t *handle = (_driver_t *)id;
   int m = (mode == AAX_MODE_READ) ? 0 : 1;
   char *rv = (char*)&names[m];
   time_t t_now;

   t_now = time(NULL);
   if (t_now > (t_previous+5))
   {
      t_previous = t_now;
      _pipewire_detect_devices(names);
   }

   return rv;
}

static char *
_aaxPipeWireDriverGetInterfaces(UNUSED(const void *id), UNUSED(const char *driver), UNUSED(int mode))
{
// _driver_t *handle = (_driver_t *)id;
   return NULL;
}

static char *
_aaxPipeWireDriverLogVar(const void *id, const char *fmt, ...)
{
   char _errstr[1024];
   va_list ap;

   _errstr[0] = '\0';
   va_start(ap, fmt);
   vsnprintf(_errstr, 1024, fmt, ap);

   // Always null terminate the string
   _errstr[1023] = '\0';
   va_end(ap);

   return _aaxPipeWireDriverLog(id, 0, -1, _errstr);
}

static char *
_aaxPipeWireDriverLog(const void *id, UNUSED(int prio), UNUSED(int type), const char *str)
{
   _driver_t *handle = id ? ((_driver_t *)id)->handle : NULL;
   static char _errstr[256];

   snprintf(_errstr, 256, "pipewire: %s\n", str);
   _errstr[255] = '\0';  /* always null terminated */

   __aaxDriverErrorSet(handle, AAX_BACKEND_ERROR, (char*)&_errstr);
   _AAX_SYSLOG(_errstr);
#ifndef NDEBUG
   printf("%s", _errstr);
#endif

   return (char*)&_errstr;
}

#if 0
static float
_pipewire_set_volume(_driver_t *handle, _aaxRingBuffer *rb, ssize_t offset, uint32_t period_frames, unsigned int tracks, float volume)
{
   float gain = fabsf(volume);
   float rv = 0;

   /* software volume fallback */
   if (rb && fabsf(gain - 1.0f) > LEVEL_32DB) {
      rb->data_multiply(rb, offset, period_frames, gain);
   }

   return rv;
}
#endif

/* ----------------------------------------------------------------------- */
static struct pw_core *hotplug_core = NULL;
static struct pw_thread_loop *hotplug_loop = NULL;
static struct pw_context *hotplug_context = NULL;
static struct pw_registry *hotplug_registry = NULL;
static struct spa_hook hotplug_registry_listener;
static struct spa_hook hotplug_core_listener;
static struct spa_list hotplug_pending_list;
static struct spa_list hotplug_io_list;
static int hotplug_init_seq_val = 0;
static char hotplug_init_complete = AAX_FALSE;
static char hotplug_events_enabled = AAX_FALSE;

static uint32_t pipewire_default_sink_id = SPA_ID_INVALID;
static uint32_t pipewire_default_source_id = SPA_ID_INVALID;

/* A generic Pipewire node object used for enumeration. */
struct node_object
{
   struct spa_list link;

   int32_t id;
   int seq;

   /*
    * NOTE: If used, this is *must* be allocated with _aax_malloc() or similar
    * as _aax_free() will be called on it when the node_object is destroyed.
    *
    * If ownership of the referenced memory is transferred, this must be set
    * to NULL or the memory will be freed when the node_object is destroyed.
    */
   void *userdata;

   struct pw_proxy *proxy;
   struct spa_hook node_listener;
   struct spa_hook core_listener;
};

/* A sink/source node used for stream I/O. */
struct io_node
{
   struct spa_list link;

   uint32_t id;
   char is_capture;

   struct {
      int32_t rate;
      uint8_t channels;
      unsigned int format;
   } spec;

   char name[];
};

/* Helpers for retrieving values from params */
static char
get_range_param(const struct spa_pod *param, uint32_t key, int *def, int *min, int *max)
{
   const struct spa_pod_prop *prop;
   struct spa_pod *value;
   uint32_t n_values, choice;

   prop = spa_pod_find_prop(param, NULL, key);

   if (prop && prop->value.type == SPA_TYPE_Choice)
   {
      value = spa_pod_get_values(&prop->value, &n_values, &choice);

      if (n_values == 3 && choice == SPA_CHOICE_Range)
      {
         uint32_t *v = SPA_POD_BODY(value);

         if (v)
         {
             if (def) {
                 *def = (int)v[0];
             }
             if (min) {
                 *min = (int)v[1];
             }
             if (max) {
                 *max = (int)v[2];
            }

            return AAX_TRUE;
         }
      }
   }

   return AAX_FALSE;
}

static char
get_int_param(const struct spa_pod *param, uint32_t key, int *val)
{
   const struct spa_pod_prop *prop;
   int32_t v;

   prop = spa_pod_find_prop(param, NULL, key);

   if (prop && spa_pod_get_int(&prop->value, &v) == 0)
   {
      if (val) {
         *val = (int)v;
      }
      return AAX_TRUE;
   }

   return AAX_FALSE;
}

/* The active node list */
static char
io_list_check_add(struct io_node *node)
{
   struct io_node *n;
   char ret = AAX_TRUE;

   /* See if the node is already in the list */
   spa_list_for_each (n, &hotplug_io_list, link)
   {
      if (n->id == node->id)
      {
         ret = AAX_FALSE;
         goto dup_found;
      }
   }

   /* Add to the list if the node doesn't already exist */
   spa_list_append(&hotplug_io_list, &node->link);

   if (hotplug_events_enabled) {
//      SDL_AddAudioDevice(node->is_capture, node->name, &node->spec, PW_ID_TO_HANDLE(node->id));
   }

dup_found:
   return ret;
}

static void
io_list_remove(uint32_t id)
{
   struct io_node *n, *temp;

   /* Find and remove the node from the list */
   spa_list_for_each_safe (n, temp, &hotplug_io_list, link)
   {
      if (n->id == id)
      {
         spa_list_remove(&n->link);

         if (hotplug_events_enabled) {
//          SDL_RemoveAudioDevice(n->is_capture, PW_ID_TO_HANDLE(id));
         }

         _aax_free(n);
         break;
      }
   }
}

static void
io_list_sort()
{
   struct io_node *default_sink = NULL, *default_source = NULL;
   struct io_node *n, *temp;

   /* Find and move the default nodes to the beginning of the list */
   spa_list_for_each_safe (n, temp, &hotplug_io_list, link)
   {
      if (n->id == pipewire_default_sink_id)
      {
         default_sink = n;
         spa_list_remove(&n->link);
      }
      else if (n->id == pipewire_default_source_id)
      {
         default_source = n;
         spa_list_remove(&n->link);
      }
   }

   if (default_source) {
      spa_list_prepend(&hotplug_io_list, &default_source->link);
   }

   if (default_sink) {
      spa_list_prepend(&hotplug_io_list, &default_sink->link);
   }
}

static void
io_list_clear()
{
   struct io_node *n, *temp;

   spa_list_for_each_safe (n, temp, &hotplug_io_list, link)
   {
      spa_list_remove(&n->link);
      _aax_free(n);
   }
}

static void
node_object_destroy(struct node_object *node)
{
   assert(node);

   spa_list_remove(&node->link);
   spa_hook_remove(&node->node_listener);
   spa_hook_remove(&node->core_listener);
   _aax_free(node->userdata);
   ppw_proxy_destroy(node->proxy);
}

/* The pending node list */
static void
pending_list_add(struct node_object *node)
{
   assert(node);
   spa_list_append(&hotplug_pending_list, &node->link);
}

static void
pending_list_remove(uint32_t id)
{
   struct node_object *node, *temp;

   spa_list_for_each_safe (node, temp, &hotplug_pending_list, link)
   {
      if (node->id == id) {
         node_object_destroy(node);
      }
   }
}

static void
pending_list_clear()
{
   struct node_object *node, *temp;

   spa_list_for_each_safe (node, temp, &hotplug_pending_list, link) {
      node_object_destroy(node);
   }
}

static void *
node_object_new(uint32_t id, const char *type, uint32_t version, const void *funcs, const struct pw_core_events *core_events)
{
   struct node_object *node;
   struct pw_proxy *proxy;

   /* Create the proxy object */
   proxy = pw_registry_bind(hotplug_registry, id, type, version, sizeof(struct node_object));
   if (proxy == NULL)
   {
      _aaxPipeWireDriverLogVar(NULL, "Pipewire: proxy object error: %s", 
                               strerror(errno));
      return NULL;
   }

   node = ppw_proxy_get_user_data(proxy);
   memset(node, 0, sizeof(struct node_object));

   node->id = id;
   node->proxy = proxy;

   /* Add the callbacks */
   pw_core_add_listener(hotplug_core, &node->core_listener, core_events, node);
   ppw_proxy_add_object_listener(node->proxy, &node->node_listener, funcs, node);

   /* Add the node to the active list */
   pending_list_add(node);

   return node;
}

/* Core sync points */
static void
core_events_hotplug_init_callback(void *object, uint32_t id, int seq)
{
   if (id == PW_ID_CORE && seq == hotplug_init_seq_val)
   {
      /* This core listener is no longer needed. */
      spa_hook_remove(&hotplug_core_listener);

      /* Signal that the initial I/O list is populated */
      hotplug_init_complete = AAX_TRUE;
      ppw_thread_loop_signal(hotplug_loop, false);
   }
}

static void
core_events_interface_callback(void *object, uint32_t id, int seq)
{
   struct node_object *node = object;
   struct io_node *io = node->userdata;

   if (id == PW_ID_CORE && seq == node->seq)
   {
      /*
       * Move the I/O node to the connected list.
       * On success, the list owns the I/O node object.
       */
      if (io_list_check_add(io)) {
         node->userdata = NULL;
      }
      node_object_destroy(node);
   }
}

static void
core_events_metadata_callback(void *object, uint32_t id, int seq)
{
   struct node_object *node = object;

   if (id == PW_ID_CORE && seq == node->seq) {
      node_object_destroy(node);
   }
}

static const struct pw_core_events hotplug_init_core_events = {
  PW_VERSION_CORE_EVENTS,
  .done = core_events_hotplug_init_callback
};
static const struct pw_core_events interface_core_events = {
  PW_VERSION_CORE_EVENTS,
  .done = core_events_interface_callback
};
static const struct pw_core_events metadata_core_events = {
  PW_VERSION_CORE_EVENTS,
  .done = core_events_metadata_callback
};

static void
hotplug_core_sync(struct node_object *node)
{
   /*
    * Node sync events *must* come before the hotplug init sync events or the
    * initial I/O list will be incomplete when the main hotplug sync point is
    * hit.
    */
   if (node) {
      node->seq = pw_core_sync(hotplug_core, PW_ID_CORE, node->seq);
   }

   if (!hotplug_init_complete) {
      hotplug_init_seq_val = pw_core_sync(hotplug_core, PW_ID_CORE, hotplug_init_seq_val);
   }
}

/* Interface node callbacks */
static void
node_event_info(void *object, const struct pw_node_info *info)
{
   struct node_object *node = object;
   struct io_node *io = node->userdata;
   const char *prop_val;
   uint32_t i;

   if (info)
   {
      prop_val = spa_dict_lookup(info->props, PW_KEY_AUDIO_CHANNELS);
      if (prop_val) {
         io->spec.channels = (uint8_t)atoi(prop_val);
      }

      /* Need to parse the parameters to get the sample rate */
      for (i = 0; i < info->n_params; ++i) {
         pw_node_enum_params(node->proxy, 0, info->params[i].id, 0, 0, NULL);
      }

      hotplug_core_sync(node);
   }
}

static void
node_event_param(void *object, int seq, uint32_t id, uint32_t index, uint32_t next, const struct spa_pod *param)
{
   struct node_object *node = object;
   struct io_node *io = node->userdata;

   /* Get the default frequency */
   if (io->spec.rate == 0) {
      get_range_param(param, SPA_FORMAT_AUDIO_rate, &io->spec.rate, NULL, NULL);
   }

   /*
    * The channel count should have come from the node properties,
    * but it is stored here as well. If one failed, try the other.
    */
   if (io->spec.channels == 0)
   {
      int channels;
      if (get_int_param(param, SPA_FORMAT_AUDIO_channels, &channels)) {
         io->spec.channels = (uint8_t)channels;
      }
   }
}

static const struct pw_node_events interface_node_events = {
  PW_VERSION_NODE_EVENTS,
  .info = node_event_info,
  .param = node_event_param
};

/* Metadata node callback */
static int
metadata_property(void *object, uint32_t subject, const char *key, const char *type, const char *value)
{
   if (subject == PW_ID_CORE && key != NULL && value != NULL)
   {
      uint32_t val = atoi(value);

      if (!strcmp(key, "default.audio.sink")) {
         pipewire_default_sink_id = val;
      } else if (!strcmp(key, "default.audio.source")) {
         pipewire_default_source_id = val;
      }
   }

   return 0;
}

static const struct pw_metadata_events metadata_node_events = {
  PW_VERSION_METADATA_EVENTS,
  .property = metadata_property
};

/* Global registry callbacks */
static void
registry_event_global_callback(void *object, uint32_t id, uint32_t permissions, const char *type, uint32_t version, const struct spa_dict *props)
{
   struct node_object *node;

   /* We're only interested in interface and metadata nodes. */
   if (!strcmp(type, PW_TYPE_INTERFACE_Node))
   {
      const char *media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);

      if (media_class)
      {
         const char *node_desc;
         struct io_node *io;
         char is_capture;
         int str_buffer_len;

         /* Just want sink and capture */
         if (!strcasecmp(media_class, "Audio/Sink")) {
            is_capture = AAX_FALSE;
         } else if (!strcasecmp(media_class, "Audio/Source")) {
            is_capture = AAX_TRUE;
         } else {
            return;
         }

         node_desc = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);

         if (node_desc)
         {
            node = node_object_new(id, type, version, &interface_node_events, &interface_core_events);
            if (node == NULL)
            {
               _AAX_SYSLOG("Pipewire: Failed to allocate interface node");
               return;
            }

            /* Allocate and initialize the I/O node information struct */
            str_buffer_len = strlen(node_desc) + 1;
            node->userdata = io = calloc(1, sizeof(struct io_node) + str_buffer_len);
            if (io == NULL)
            {
               node_object_destroy(node);
               _AAX_SYSLOG("Pipewire: out of memory");
               return;
            }

            /* Begin setting the node properties */
            io->id = id;
            io->is_capture = is_capture;
            io->spec.format = SPA_AUDIO_FORMAT_F32; /* Pipewire uses floats internally. */
            strncpy(io->name, node_desc, str_buffer_len);

            /* Update sync points */
            hotplug_core_sync(node);
         }
      }
   }
   else if (!strcmp(type, PW_TYPE_INTERFACE_Metadata))
   {
      node = node_object_new(id, type, version, &metadata_node_events, &metadata_core_events);
      if (node == NULL)
      {
         _AAX_SYSLOG("Pipewire: Failed to allocate metadata node");
         return;
      }

      /* Update sync points */
      hotplug_core_sync(node);
   }
}

static void
registry_event_remove_callback(void *object, uint32_t id)
{
   io_list_remove(id);
   pending_list_remove(id);
}

static const struct pw_registry_events registry_events = {
  PW_VERSION_REGISTRY_EVENTS,
  .global = registry_event_global_callback,
  .global_remove = registry_event_remove_callback
};

int
hotplug_loop_init()
{
   int res;

   spa_list_init(&hotplug_pending_list);
   spa_list_init(&hotplug_io_list);

   hotplug_loop = ppw_thread_loop_new("AeonWaveHotplug", NULL);
   if (hotplug_loop == NULL)
   {
      _AAX_SYSLOG("Pipewire: Failed to create hotplug detection loop");
      return errno;
   }

   hotplug_context = ppw_context_new(ppw_thread_loop_get_loop(hotplug_loop), NULL, 0);
   if (hotplug_context == NULL)
   {
      _AAX_SYSLOG("Pipewire: Failed to create hotplug detection context");
      return errno;
   }

   hotplug_core = ppw_context_connect(hotplug_context, NULL, 0);
   if (hotplug_core == NULL)
   {
      _AAX_SYSLOG("Pipewire: Failed to connect hotplug detection context");
      return errno;
   }

   hotplug_registry = pw_core_get_registry(hotplug_core, PW_VERSION_REGISTRY, 0);
   if (hotplug_registry == NULL)
   {
      _AAX_SYSLOG("Pipewire: Failed to acquire hotplug detection registry");
      return errno;
   }

   spa_zero(hotplug_registry_listener);
   pw_registry_add_listener(hotplug_registry, &hotplug_registry_listener, &registry_events, NULL);

   spa_zero(hotplug_core_listener);
   pw_core_add_listener(hotplug_core, &hotplug_core_listener, &hotplug_init_core_events, NULL);

   hotplug_init_seq_val = pw_core_sync(hotplug_core, PW_ID_CORE, 0);

   res = ppw_thread_loop_start(hotplug_loop);
   if (res != 0) {
      _AAX_SYSLOG("Pipewire: Failed to start hotplug detection loop");
   }

   return res;
}

static void
hotplug_loop_destroy()
{
   if (hotplug_loop) {
      ppw_thread_loop_stop(hotplug_loop);
   }

   pending_list_clear();
   io_list_clear();

   hotplug_init_complete = AAX_FALSE;
   hotplug_events_enabled = AAX_FALSE;

   pipewire_default_sink_id = SPA_ID_INVALID;
   pipewire_default_source_id = SPA_ID_INVALID;

   if (hotplug_registry)
   {
      ppw_proxy_destroy((struct pw_proxy *)hotplug_registry);
      hotplug_registry = NULL;
   }

   if (hotplug_core)
   {
      ppw_core_disconnect(hotplug_core);
      hotplug_core = NULL;
   }

   if (hotplug_context)
   {
      ppw_context_destroy(hotplug_context);
      hotplug_context = NULL;
   }

   if (hotplug_loop)
   {
      ppw_thread_loop_destroy(hotplug_loop);
      hotplug_loop = NULL;
   }
}

static void
driver_list_add(char description[MAX_DEVICES_LIST], char *name)
{
   char *ptr = description;

   if (ptr[0] != 0)
   {
      ptr = memmem(ptr, MAX_DEVICES_LIST, "\0\0", 2);
      if (ptr) ptr++;
   }

   if (ptr)
   {
      off_t offs = ptr-description;
      size_t avail = (MAX_DEVICES_LIST-2) - offs;
      size_t slen = strlen(name);
      if (avail >= slen)
      {
         memcpy(ptr, name, slen);
         ptr += slen;
         *ptr++ = '\0';
         *ptr = '\0';
      }
   }
}

static void
_pipewire_detect_devices(char description[2][MAX_DEVICES_LIST])
{
   if (!pipewire_initialized)
   {
      int err;

      ppw_init(NULL, NULL);
      pipewire_initialized = AAX_TRUE;

      err = hotplug_loop_init();
      if (err != 0)
      {
         _aaxPipeWireDriverLogVar(NULL, "Pipewire: hotplug loop error: %s",
                                  strerror(errno));
         hotplug_loop_destroy();
         pipewire_initialized = AAX_FALSE;
      }
   }

   if (hotplug_loop)
   {
      struct io_node *io;

      ppw_thread_loop_lock(hotplug_loop);

      /* Wait until the initial registry enumeration is complete */
      if (!hotplug_init_complete) {
         ppw_thread_loop_wait(hotplug_loop);
      }

      /* Sort the I/O list so the default source/sink are listed first */
      io_list_sort();

      memset(description[0], 0, MAX_DEVICES_LIST);
      memset(description[1], 0, MAX_DEVICES_LIST);

      spa_list_for_each (io, &hotplug_io_list, link)
      {
         int m = io->is_capture ? 0 : 1;
         driver_list_add(description[m], io->name);
      }

      hotplug_events_enabled = AAX_TRUE;

      ppw_thread_loop_unlock(hotplug_loop);
   }
}

static enum aaxFormat
_aaxPipeWireGetAAXFormat(enum spa_audio_format format)
{
   enum aaxFormat rv = AAX_PCM16S;
   switch(format)
   {
   case SPA_AUDIO_FORMAT_U8 :
      rv = AAX_PCM8U;
      break;
   case SPA_AUDIO_FORMAT_ALAW:
      rv = AAX_ALAW;
      break;
   case SPA_AUDIO_FORMAT_ULAW:
      rv = AAX_MULAW;
      break;
#if __BYTE_ORDER == __BIG_ENDIAN
   case SPA_AUDIO_FORMAT_S16_LE:
      rv = AAX_PCM16S_LE;
      break;
   case SPA_AUDIO_FORMAT_S16_BE:
      rv = AAX_PCM16S;
      break;
   case SPA_AUDIO_FORMAT_F32_LE:
      rv = AAX_FLOAT_LE;
      break;
   case SPA_AUDIO_FORMAT_F32_BE:
      rv = AAX_FLOAT;
      break;
   case SPA_AUDIO_FORMAT_S32_LE:
      rv = AAX_PCM32S_LE;
      break;
   case SPA_AUDIO_FORMAT_S32_BE:
      rv = AAX_PCM32S;
      break;
   case SPA_AUDIO_FORMAT_S24_32LE:
      rv = AAX_PCM24S_LE;
      break;
   case SPA_AUDIO_FORMAT_S24_32BE:
      rv = AAX_PCM24S;
      break;
#else
   case SPA_AUDIO_FORMAT_S16_LE:
      rv = AAX_PCM16S;
      break;
   case SPA_AUDIO_FORMAT_S16_BE:
      rv = AAX_PCM16S_BE;
      break;
   case SPA_AUDIO_FORMAT_F32_LE:
      rv = AAX_FLOAT;
      break;
   case SPA_AUDIO_FORMAT_F32_BE:
      rv = AAX_FLOAT_BE;
      break;
   case SPA_AUDIO_FORMAT_S32_LE:
      rv = AAX_PCM32S;
      break;
   case SPA_AUDIO_FORMAT_S32_BE:
      rv = AAX_PCM32S_BE;
      break;
   case SPA_AUDIO_FORMAT_S24_LE:
      rv = AAX_PCM24S_PACKED;
      break;
   case SPA_AUDIO_FORMAT_S24_32_LE:
      rv = AAX_PCM24S;
      break;
   case SPA_AUDIO_FORMAT_S24_32_BE:
      rv = AAX_PCM24S_BE;
      break;
#endif
   default:
      break;
   }
   return rv;
}

#if 0
static enum spa_audio_format
_aaxPipeWireGetFormat(enum aaxFormat format)
{
   enum spa_audio_format rv = SPA_AUDIO_FORMAT_S16_LE;
   switch(format)
   {
   case AAX_PCM8S:
      rv = SPA_AUDIO_FORMAT_S8;
      break;
   case AAX_PCM16S:
#if __BYTE_ORDER == __BIG_ENDIAN
      rv = SPA_AUDIO_FORMAT_S16_LE;
#else
      rv = SPA_AUDIO_FORMAT_S16_BE;
#endif
      break;
   case AAX_PCM24S:
#if __BYTE_ORDER == __BIG_ENDIAN
      rv = SPA_AUDIO_FORMAT_S24_LE;
#else
      rv = SPA_AUDIO_FORMAT_S24_BE;
#endif
      break;
   case AAX_PCM32S:
#if __BYTE_ORDER == __BIG_ENDIAN
      rv = SPA_AUDIO_FORMAT_S32_LE;
#else
      rv = SPA_AUDIO_FORMAT_S32_BE;
#endif
      break;
   case AAX_FLOAT:
#if __BYTE_ORDER == __BIG_ENDIAN
      rv = SPA_AUDIO_FORMAT_F32_LE;
#else
      rv = SPA_AUDIO_FORMAT_F32_BE;
#endif
      break;
   default:
      break;
   }
   return rv;
}
#endif

static void
stream_playback_cb(void *be_ptr)
{
   _driver_t *be_handle = (_driver_t *)be_ptr;
   _handle_t *handle = (_handle_t *)be_handle->handle;
// void *id = be_handle;

   if (_IS_PLAYING(handle))
   {
      struct pw_buffer *pw_buf;

      assert(be_handle->mode != AAX_MODE_READ);
      assert(be_handle->dataBuffer);

      _aaxMutexLock(be_handle->mutex);

      pw_buf = ppw_stream_dequeue_buffer(be_handle->pw);
      if (pw_buf)
      {
         struct spa_buffer *spa_buf = pw_buf->buffer;
         uint64_t len = pw_buf->size;
         if (spa_buf)
         {
            void *data = _aaxDataGetData(be_handle->dataBuffer);
            if (_aaxDataGetDataAvail(be_handle->dataBuffer) < len) {
               len = _aaxDataGetDataAvail(be_handle->dataBuffer);
            }

            memcpy(spa_buf->datas[0].data, data, len);
            ppw_stream_queue_buffer(be_handle->pw, pw_buf);

            _aaxDataMove(be_handle->dataBuffer, NULL, len);
         }
         else {
            memset(spa_buf->datas[0].data, 0, len);
         }
         ppw_stream_queue_buffer(be_handle->pw, pw_buf);
      }

      _aaxMutexUnLock(be_handle->mutex);
   }
}

static void
stream_capture_cb(void *be_ptr)
{
   _driver_t *be_handle = (_driver_t *)be_ptr;
   struct pw_buffer  *pw_buf;

   pw_buf = ppw_stream_dequeue_buffer(be_handle->pw);
   if (pw_buf)
   {
      struct spa_buffer *spa_buf = pw_buf->buffer;
      if (spa_buf->datas[0].data)
      {
         uint32_t offs = SPA_MIN(spa_buf->datas[0].chunk->offset,
                                 spa_buf->datas[0].maxsize);
         uint32_t len = SPA_MIN(spa_buf->datas[0].chunk->size,
                                spa_buf->datas[0].maxsize - offs);

         if (_aaxDataGetOffset(be_handle->dataBuffer)+len < _aaxDataGetSize(be_handle->dataBuffer))
         {
            uint8_t *buf = (uint8_t*)spa_buf->datas[0].data + offs;
            _aaxDataAdd(be_handle->dataBuffer, buf, len);
         }
      }
      ppw_stream_queue_buffer(be_handle->pw, pw_buf);
   }
}

static void
stream_add_buffer_cb(void *be_ptr, struct pw_buffer *buffer)
{
   _driver_t *be_handle = (_driver_t *)be_ptr;

#if 0
   if (handle->mode != AAX_MODE_READ)
   {
      /*
       * Clamp the output spec samples and size to the max size of the Pipewire buffer.
       * If they exceed the maximum size of the Pipewire buffer, double buffering will be used.
       */
      if (this->spec.size > buffer->buffer->datas[0].maxsize) { 
         this->spec.samples = buffer->buffer->datas[0].maxsize / this->hidden->stride;
         this->spec.size    = buffer->buffer->datas[0].maxsize;
      }
   } else if (this->hidden->buffer == NULL) {
       /*
        * The latency of source nodes can change, so buffering is always required.
        *
        * Ensure that the intermediate input buffer is large enough to hold the requested
        * application packet size or a full buffer of data from Pipewire, whichever is larger.
        *
        * A packet size of 2 periods should be more than is ever needed.
        */
      this->hidden->input_buffer_packet_size = SPA_MAX(this->spec.size, buffer->buffer->datas[0].maxsize) * 2;
      this->hidden->buffer                   = SDL_NewDataQueue(this->hidden->input_buffer_packet_size, this->hidden->input_buffer_packet_size);
   }
#endif

   be_handle->stream_init_status |= PW_READY_FLAG_BUFFER_ADDED;
   ppw_thread_loop_signal(be_handle->ml, false);
}

static void
stream_state_cb(void *be_ptr, enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
   _driver_t *be_handle = (_driver_t *)be_ptr;

   if (state == PW_STREAM_STATE_STREAMING) {
      be_handle->stream_init_status |= PW_READY_FLAG_STREAM_READY;
   }

   if (state == PW_STREAM_STATE_STREAMING || state == PW_STREAM_STATE_ERROR) {
      ppw_thread_loop_signal(be_handle->ml, false);
   }
}

static const struct pw_stream_events stream_output_events =
{
  PW_VERSION_STREAM_EVENTS,
  .state_changed = stream_state_cb,
  .add_buffer = stream_add_buffer_cb,
  .process = stream_playback_cb
};
static const struct pw_stream_events stream_input_events =
{
  PW_VERSION_STREAM_EVENTS,
  .state_changed = stream_state_cb,
  .add_buffer = stream_add_buffer_cb,
  .process = stream_capture_cb
};

static uint32_t
_pipewire_get_id_by_name(const char *name)
{
   uint32_t rv = PW_ID_ANY;
   struct io_node *n;

   if (hotplug_events_enabled)
   {
      spa_list_for_each (n, &hotplug_io_list, link)
      {
         if (!strcmp(n->name, name))
         {
            rv = n->id;
            break;
         }
      }
   }

   return rv;
}

static const enum spa_audio_channel pchannel_map_1[] = {
  SPA_AUDIO_CHANNEL_MONO
};
static const enum spa_audio_channel pchannel_map_2[] = {
  SPA_AUDIO_CHANNEL_FL,
  SPA_AUDIO_CHANNEL_FR
};
static const enum spa_audio_channel pchannel_map_3[] = {
  SPA_AUDIO_CHANNEL_FL,
  SPA_AUDIO_CHANNEL_FR,
  SPA_AUDIO_CHANNEL_LFE
};
static const enum spa_audio_channel pchannel_map_4[] = {
  SPA_AUDIO_CHANNEL_FL,
  SPA_AUDIO_CHANNEL_FR,
  SPA_AUDIO_CHANNEL_RL,
  SPA_AUDIO_CHANNEL_RR
};
static const enum spa_audio_channel pchannel_map_5[] = {
  SPA_AUDIO_CHANNEL_FL,
  SPA_AUDIO_CHANNEL_FR,
  SPA_AUDIO_CHANNEL_FC,
  SPA_AUDIO_CHANNEL_RL,
  SPA_AUDIO_CHANNEL_RR
};
static const enum spa_audio_channel pchannel_map_6[] = {
  SPA_AUDIO_CHANNEL_FL,
  SPA_AUDIO_CHANNEL_FR,
  SPA_AUDIO_CHANNEL_FC,
  SPA_AUDIO_CHANNEL_LFE,
  SPA_AUDIO_CHANNEL_RL,
  SPA_AUDIO_CHANNEL_RR };
static const enum spa_audio_channel pchannel_map_7[] = {
  SPA_AUDIO_CHANNEL_FL,
  SPA_AUDIO_CHANNEL_FR,
  SPA_AUDIO_CHANNEL_FC,
  SPA_AUDIO_CHANNEL_LFE,
  SPA_AUDIO_CHANNEL_RC,
  SPA_AUDIO_CHANNEL_RL,
  SPA_AUDIO_CHANNEL_RR
};
static const enum spa_audio_channel pchannel_map_8[] = {
  SPA_AUDIO_CHANNEL_FL,
  SPA_AUDIO_CHANNEL_FR,
  SPA_AUDIO_CHANNEL_FC,
  SPA_AUDIO_CHANNEL_LFE,
  SPA_AUDIO_CHANNEL_RL,
  SPA_AUDIO_CHANNEL_RR,
  SPA_AUDIO_CHANNEL_SL,
  SPA_AUDIO_CHANNEL_SR
};

#define COPY_CHANNEL_MAP(c) memcpy(&spa_info.position, pchannel_map_##c, sizeof(pchannel_map_##c))
static int
_pipewire_open(_driver_t *handle)
{
   void *id = handle;
   char m = (handle->mode == AAX_MODE_READ) ? 1 : 0;
   uint8_t pod_buffer[PW_POD_BUFFER_LENGTH];
   struct spa_pod_builder b = SPA_POD_BUILDER_INIT(pod_buffer, sizeof(pod_buffer));
   struct spa_audio_info_raw spa_info = { 0 };
   const struct spa_pod *params = NULL;
   int rv = AAX_FALSE;

   spa_info.rate = handle->spec.rate;
   spa_info.channels = handle->spec.channels;
   switch (handle->spec.channels) {
   case 1:
      COPY_CHANNEL_MAP(1);
      break;
   case 2:
      COPY_CHANNEL_MAP(2);
      break;
   case 3:
      COPY_CHANNEL_MAP(3);
      break;
   case 4:
      COPY_CHANNEL_MAP(4);
      break;
   case 5:
      COPY_CHANNEL_MAP(5);
      break;
   case 6:
      COPY_CHANNEL_MAP(6);
      break;
   case 7:
      COPY_CHANNEL_MAP(7);
      break;
   case 8:
      COPY_CHANNEL_MAP(8);
      break;
   }
   spa_info.format = handle->spec.format;
   params = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &spa_info);
   if (params)
   {
      char thread_name[PW_THREAD_NAME_BUFFER_LENGTH];

      snprintf(thread_name, sizeof(thread_name),
               "AAXAudio%c%ld", (m) ? 'C' : 'P', (long)handle->id);
      handle->ml = ppw_thread_loop_new(thread_name, NULL);
   }

   /*
    * Load the realtime module so Pipewire can set the loop thread to the
    * appropriate priority.
    *
    * NOTE: Pipewire versions 0.3.22 or higher require the PW_KEY_CONFIG_NAME
    *       property (with client-rt.conf), lower versions require explicitly
    *       specifying the 'rtkit' module.
    *
    *       PW_KEY_CONTEXT_PROFILE_MODULES is deprecated and can be safely
    *       removed if the minimum required Pipewire version is increased to
    *       0.3.22 or higher at some point.
    */
   if (handle->ml)
   {
      struct pw_properties *props;

      props = ppw_properties_new(PW_KEY_CONFIG_NAME, "client-rt.conf",
                                 PW_KEY_CONTEXT_PROFILE_MODULES,
                                 "default,rtkit", NULL);
      if (props) {
         handle->ctx = ppw_context_new(ppw_thread_loop_get_loop(handle->ml),                                           props, 0);
      }
   }

   if (handle->ctx)
   {
      struct pw_properties *props;

      props = ppw_properties_new(NULL, NULL);
      if (props)
      {
         const char *name = AAX_LIBRARY_STR;
         const char *stream_name = _aax_get_binary_name("Audio Stream");
         const char *stream_role = "Game";

         ppw_properties_set(props, PW_KEY_MEDIA_TYPE, "Audio");
         ppw_properties_set(props, PW_KEY_MEDIA_CATEGORY, m ? "Capture" : "Playback");
         ppw_properties_set(props, PW_KEY_MEDIA_ROLE, stream_role);
         ppw_properties_set(props, PW_KEY_APP_NAME, name);
         ppw_properties_set(props, PW_KEY_NODE_NAME, stream_name);
         ppw_properties_set(props, PW_KEY_NODE_DESCRIPTION, stream_name);
         ppw_properties_setf(props, PW_KEY_NODE_LATENCY, "%u/%i", handle->samples, handle->spec.rate);
         ppw_properties_setf(props, PW_KEY_NODE_RATE, "1/%u", handle->spec.rate);
         ppw_properties_set(props, PW_KEY_NODE_ALWAYS_PROCESS, "true");

         handle->pw = ppw_stream_new_simple(ppw_thread_loop_get_loop(handle->ml), stream_name, props, m ? &stream_input_events : &stream_output_events, handle);
         if (handle->pw)
         {
            /*
             * NOTE: The PW_STREAM_FLAG_RT_PROCESS flag can be set to call the
             *       stream processing callback from the realtime thread.
             *       However, it comes with some caveats: no file IO,
             *       allocations, locking or other blocking operations must
             *       occur in the mixer callback.  As this cannot be guaranteed
             *       when the  callback is in the calling application, this flag
             *       is omitted.
             */
            static const enum pw_stream_flags STREAM_FLAGS = PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS;
            uint32_t node_id = handle->id;
            int res;

            res = ppw_stream_connect(handle->pw,
                                   m ? PW_DIRECTION_INPUT : PW_DIRECTION_OUTPUT,
                                   node_id, STREAM_FLAGS, &params, 1);
            if (res == 0)
            {
               res = ppw_thread_loop_start(handle->ml);
               if (res == 0)
               {
                  const char *error;

                  ppw_thread_loop_lock(handle->ml);
                  while (handle->stream_init_status != PW_READY_FLAG_ALL_BITS &&
                         ppw_stream_get_state(handle->pw, NULL) != PW_STREAM_STATE_ERROR)
                  {
                     ppw_thread_loop_wait(handle->ml);
                  }
                  ppw_thread_loop_unlock(handle->ml);

                  if (ppw_stream_get_state(handle->pw, &error) == PW_STREAM_STATE_ERROR) {
                     _aaxPipeWireDriverLogVar(handle, "Pipewire: hotplug loop error: %s", error);
                  } else {
                     rv = AAX_TRUE;
                  }
               }
               else {
                  _AAX_DRVLOG("Pipewire: Failed to start stream loop");
               }
            }
            else {
               _AAX_DRVLOG("Pipewire: Failed to connect stream");
            }
         }
      }
   }

   if (!params) {
      _AAX_DRVLOG("incompatible hardware configuration");
   } else if (!handle->ml) {
      _AAX_DRVLOG("failed to create a stream loop");
   } else if (!handle->ctx) {
      _AAX_DRVLOG("failed to create a stream context");
   } else if (!handle->pw) {
      _AAX_DRVLOG("failed to create a stream");
   }

   return rv;
}

#if USE_PIPEWIRE_THREAD
static void *
_aaxPipeWireDriverThread(void* config)
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
   dest_rb = be->get_ringbuffer(REVERB_EFFECTS_TIME, handle->info->mode);
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
   be_handle->dataBuffer = _aaxDataCreate(1, 1);

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
            input = (float)_aaxDataGetOffset(be_handle->dataBuffer)/freq;
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

#endif /* HAVE_PIPEWIRE_H */
