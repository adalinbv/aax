/*
 * SPDX-FileCopyrightText: Copyright © 2019-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2019-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_PULSEAUDIO_H

# ifdef HAVE_RMALLOC_H
#  include <rmalloc.h>
# else
#  if HAVE_STRINGS_H
#   include <strings.h>
#  endif
#  include <string.h>		/* strstr, strncmp */
# endif
#include <stdarg.h>		/* va_start */
#include <stdio.h>		/* snprintf */

#include <pulse/util.h>
#include <pulse/error.h>
#include <pulse/stream.h>
#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/thread-mainloop.h>

#include <aax/aax.h>
#include <xml.h>

#include <base/databuffer.h>
#include <base/memory.h>
#include <base/dlsym.h>

#include <backends/driver.h>
#include <ringbuffer.h>
#include <api.h>

#include <dsp/effects.h>
#include <software/renderer.h>
#include "audio.h"

#define DEFAULT_RENDERER	"PulseAudio"
#define DEFAULT_DEVNAME		"default"
#define MAX_ID_STRLEN		96

#define DEFAULT_PERIODS		2
#define DEFAULT_OUTPUT_RATE	48000
#define DEFAULT_REFRESH		25.0

#define USE_PULSE_THREAD	true
#define CAPTURE_CALLBACK	true

#define BUFFER_SIZE_FACTOR	4.0f
#define CAPTURE_BUFFER_SIZE	(DEFAULT_PERIODS*8192)
#define MAX_DEVICES_LIST	4096

#define _AAX_DRVLOG(a)         _aaxPulseAudioDriverLog(id, 0, 0, a)
#define HW_VOLUME_SUPPORT(a)	((a->mixfd >= 0) && a->volumeMax)

_aaxDriverDetect _aaxPulseAudioDriverDetect;
static _aaxDriverNewHandle _aaxPulseAudioDriverNewHandle;
static _aaxDriverFreeHandle _aaxPulseAudioDriverFreeHandle;
static _aaxDriverGetDevices _aaxPulseAudioDriverGetDevices;
static _aaxDriverGetInterfaces _aaxPulseAudioDriverGetInterfaces;
static _aaxDriverConnect _aaxPulseAudioDriverConnect;
static _aaxDriverDisconnect _aaxPulseAudioDriverDisconnect;
static _aaxDriverSetup _aaxPulseAudioDriverSetup;
static _aaxDriverCaptureCallback _aaxPulseAudioDriverCapture;
static _aaxDriverPlaybackCallback _aaxPulseAudioDriverPlayback;
static _aaxDriverSetName _aaxPulseAudioDriverSetName;
static _aaxDriverGetName _aaxPulseAudioDriverGetName;
static _aaxDriverRender _aaxPulseAudioDriverRender;
static _aaxDriverState _aaxPulseAudioDriverState;
static _aaxDriverParam _aaxPulseAudioDriverParam;
static _aaxDriverLog _aaxPulseAudioDriverLog;
#if USE_PULSE_THREAD
static _aaxDriverThread _aaxPulseAudioDriverThread;
#endif

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
   (_aaxDriverFreeHandle *)&_aaxPulseAudioDriverFreeHandle,
   (_aaxDriverGetDevices *)&_aaxPulseAudioDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxPulseAudioDriverGetInterfaces,

   (_aaxDriverSetName *)&_aaxPulseAudioDriverSetName,
   (_aaxDriverGetName *)&_aaxPulseAudioDriverGetName,
   (_aaxDriverRender *)&_aaxPulseAudioDriverRender,
#if USE_PULSE_THREAD
   (_aaxDriverThread *)&_aaxPulseAudioDriverThread,
#else
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,
#endif

   (_aaxDriverConnect *)&_aaxPulseAudioDriverConnect,
   (_aaxDriverDisconnect *)&_aaxPulseAudioDriverDisconnect,
   (_aaxDriverSetup *)&_aaxPulseAudioDriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxPulseAudioDriverCapture,
   (_aaxDriverPlaybackCallback *)&_aaxPulseAudioDriverPlayback,
   NULL,

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
   char *driver;
   char *devname;
   _aaxRenderer *render;

   pa_stream *pa;
   pa_context *ctx;
   pa_threaded_mainloop *ml;
   pa_mainloop_api *ml_api;

   pa_sample_spec spec;
   pa_buffer_attr attr;
   pa_volume_t volume;

   char bits_sample;
   unsigned int format;
   unsigned int period_frames;
   enum aaxRenderMode mode;
   float refresh_rate;
   float latency;

   _batch_cvt_to_intl_proc cvt_to_intl;
   _batch_cvt_from_intl_proc cvt_from_intl;

   _data_t *dataBuffer;
   _aaxMutex *mutex;

   /* capabilities */
   unsigned int min_frequency;
   unsigned int max_frequency;
   unsigned int min_tracks;
   unsigned int max_tracks;

   struct {
      float I;
      float err;
   } PID;
   struct {
      float aim; // in bytes
   } fill;

   char descriptions[2][MAX_DEVICES_LIST];
   char names[2][MAX_DEVICES_LIST];

} _driver_t;

typedef struct
{
   char *names;
   char *descriptions;
   pa_threaded_mainloop *loop;
} _sink_info_t;

#undef DECL_FUNCTION
#define DECL_FUNCTION(f) static __typeof__(f) * p##f
DECL_FUNCTION(pa_get_binary_name);
DECL_FUNCTION(pa_path_get_filename);
DECL_FUNCTION(pa_get_library_version);
DECL_FUNCTION(pa_strerror);
DECL_FUNCTION(pa_threaded_mainloop_new);
DECL_FUNCTION(pa_threaded_mainloop_free);
DECL_FUNCTION(pa_threaded_mainloop_start);
DECL_FUNCTION(pa_threaded_mainloop_stop);
DECL_FUNCTION(pa_threaded_mainloop_lock);
DECL_FUNCTION(pa_threaded_mainloop_unlock);
DECL_FUNCTION(pa_threaded_mainloop_wait);
DECL_FUNCTION(pa_threaded_mainloop_signal);
DECL_FUNCTION(pa_threaded_mainloop_get_api);
DECL_FUNCTION(pa_context_new);
DECL_FUNCTION(pa_context_connect);
DECL_FUNCTION(pa_context_disconnect);
DECL_FUNCTION(pa_context_set_state_callback);
DECL_FUNCTION(pa_context_get_state);
DECL_FUNCTION(pa_context_get_sink_info_list);
DECL_FUNCTION(pa_context_get_source_info_list);
DECL_FUNCTION(pa_context_unref);
DECL_FUNCTION(pa_context_errno);
DECL_FUNCTION(pa_stream_new);
DECL_FUNCTION(pa_stream_set_state_callback);
DECL_FUNCTION(pa_stream_connect_playback);
DECL_FUNCTION(pa_stream_connect_record);
DECL_FUNCTION(pa_stream_unref);
DECL_FUNCTION(pa_stream_write);
DECL_FUNCTION(pa_stream_set_write_callback);
DECL_FUNCTION(pa_stream_peek);
DECL_FUNCTION(pa_stream_drop);
DECL_FUNCTION(pa_stream_readable_size);
DECL_FUNCTION(pa_stream_set_read_callback);
DECL_FUNCTION(pa_stream_get_latency);
DECL_FUNCTION(pa_stream_set_latency_update_callback);
DECL_FUNCTION(pa_stream_get_state);
DECL_FUNCTION(pa_stream_get_index);
DECL_FUNCTION(pa_stream_get_device_name);
DECL_FUNCTION(pa_stream_cork);
DECL_FUNCTION(pa_stream_is_corked);
DECL_FUNCTION(pa_channel_map_init_auto);
DECL_FUNCTION(pa_operation_get_state);
DECL_FUNCTION(pa_operation_unref);

DECL_FUNCTION(pa_stream_get_buffer_attr);
DECL_FUNCTION(pa_stream_get_context);
DECL_FUNCTION(pa_sample_spec_valid);
DECL_FUNCTION(pa_sample_spec_snprint);
DECL_FUNCTION(pa_channel_map_snprint);
DECL_FUNCTION(pa_stream_get_sample_spec);
DECL_FUNCTION(pa_stream_get_channel_map);
DECL_FUNCTION(pa_stream_get_device_name);
DECL_FUNCTION(pa_stream_get_device_index);
DECL_FUNCTION(pa_stream_is_suspended);

DECL_FUNCTION(pa_cvolume_set);
DECL_FUNCTION(pa_context_set_sink_input_volume);
DECL_FUNCTION(pa_context_set_source_output_volume);

static void stream_state_cb(pa_stream*, void*);
static void stream_latency_update_cb(pa_stream*, void*);
static void stream_playback_cb(pa_stream*, size_t, void*);
static void stream_capture_cb(pa_stream*, size_t, void*);
static void sink_device_cb(pa_context*, const pa_sink_info*, int, void*);
static void source_device_cb(pa_context*, const pa_source_info*, int, void*);
static float _pulseaudio_set_volume(_driver_t*, _aaxRingBuffer*, ssize_t, uint32_t, unsigned int, float);

static const char* detect_name(_driver_t*);
static void _aaxPulseAudioContextConnect(_driver_t*);
static void _aaxPulseAudioStreamConnect(_driver_t*,  pa_stream_flags_t flags, int*);
static char *_aaxPulseAudioDriverLogVar(const void *, const char *, ...);
static enum aaxFormat _aaxPulseAudioGetFormat(pa_sample_format_t);

static const char *_const_pulseaudio_default_name = DEFAULT_DEVNAME;
const char *_const_pulseaudio_default_device = NULL;
static void *audio = NULL;


static const char *env = "true";

int
_aaxPulseAudioDriverDetect(UNUSED(int mode))
{
   static int rv = false;
   char *error = NULL;

   _AAX_LOG(LOG_DEBUG, __func__);

#if HAVE_PIPEWIRE_H
   if (_aaxPipeWireDriverDetect(mode)) {
      env = getenv("AAX_SHOW_PULSEAUDIO_DEVICES");
   }
#endif

   if (TEST_FOR_FALSE(rv) && !audio) {
      audio = _aaxIsLibraryPresent("pulse", "0");
   }
   if (audio)
   {
      _aaxGetSymError(0);

      TIE_FUNCTION(pa_stream_new);
      if (ppa_stream_new)
      {
         TIE_FUNCTION(pa_threaded_mainloop_new);
         TIE_FUNCTION(pa_threaded_mainloop_free);
         TIE_FUNCTION(pa_threaded_mainloop_start);
         TIE_FUNCTION(pa_threaded_mainloop_stop);
         TIE_FUNCTION(pa_threaded_mainloop_lock);
         TIE_FUNCTION(pa_threaded_mainloop_unlock);
         TIE_FUNCTION(pa_threaded_mainloop_wait);
         TIE_FUNCTION(pa_threaded_mainloop_signal);
         TIE_FUNCTION(pa_threaded_mainloop_get_api);
         TIE_FUNCTION(pa_context_new);
         TIE_FUNCTION(pa_context_connect);
         TIE_FUNCTION(pa_context_disconnect);
         TIE_FUNCTION(pa_context_set_state_callback);
         TIE_FUNCTION(pa_context_get_state);
         TIE_FUNCTION(pa_context_get_sink_info_list);
         TIE_FUNCTION(pa_context_get_source_info_list);
         TIE_FUNCTION(pa_context_unref);
         TIE_FUNCTION(pa_context_errno);
         TIE_FUNCTION(pa_stream_set_state_callback);
         TIE_FUNCTION(pa_stream_connect_playback);
         TIE_FUNCTION(pa_stream_connect_record);
         TIE_FUNCTION(pa_stream_unref);
         TIE_FUNCTION(pa_stream_write);
         TIE_FUNCTION(pa_stream_set_write_callback);
         TIE_FUNCTION(pa_stream_peek);
         TIE_FUNCTION(pa_stream_drop);
         TIE_FUNCTION(pa_stream_readable_size);
         TIE_FUNCTION(pa_stream_set_read_callback);
         TIE_FUNCTION(pa_stream_get_latency);
         TIE_FUNCTION(pa_stream_set_latency_update_callback);
         TIE_FUNCTION(pa_stream_get_buffer_attr);
         TIE_FUNCTION(pa_stream_get_context);
         TIE_FUNCTION(pa_stream_get_state);
         TIE_FUNCTION(pa_stream_get_index);
         TIE_FUNCTION(pa_stream_get_device_name);
         TIE_FUNCTION(pa_stream_cork);
         TIE_FUNCTION(pa_stream_is_corked);
         TIE_FUNCTION(pa_stream_get_sample_spec);
         TIE_FUNCTION(pa_stream_get_channel_map);
         TIE_FUNCTION(pa_stream_get_device_name);
         TIE_FUNCTION(pa_stream_get_device_index);
         TIE_FUNCTION(pa_stream_is_suspended);
         TIE_FUNCTION(pa_channel_map_init_auto);
         TIE_FUNCTION(pa_operation_get_state);
         TIE_FUNCTION(pa_operation_unref);
         TIE_FUNCTION(pa_sample_spec_valid);
         TIE_FUNCTION(pa_sample_spec_snprint);
         TIE_FUNCTION(pa_channel_map_snprint);
      }

      error = _aaxGetSymError(0);
      if (!error)
      {
         /* useful but not required */
         TIE_FUNCTION(pa_strerror);
         TIE_FUNCTION(pa_get_binary_name);
         TIE_FUNCTION(pa_path_get_filename);
         TIE_FUNCTION(pa_get_library_version);

         TIE_FUNCTION(pa_cvolume_set);
         TIE_FUNCTION(pa_context_set_sink_input_volume);
         TIE_FUNCTION(pa_context_set_source_output_volume);
         rv = true;
      }
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
      int m = (mode == AAX_MODE_READ) ? 1 : 0;
      int frame_sz;

      handle->driver = (char*)_const_pulseaudio_default_name;
      handle->mode = mode;

      handle->spec.channels = 2;
      handle->spec.rate = DEFAULT_OUTPUT_RATE;
      handle->spec.format = is_bigendian() ? PA_SAMPLE_S16BE : PA_SAMPLE_S16LE;

      handle->format = _aaxPulseAudioGetFormat(handle->spec.format);
      handle->bits_sample = aaxGetBitsPerSample(handle->format);

      frame_sz = handle->spec.channels*handle->bits_sample/8;
      handle->period_frames = get_pow2(handle->spec.rate/DEFAULT_REFRESH);
      handle->fill.aim = (float)DEFAULT_PERIODS*handle->period_frames/handle->spec.rate;
      handle->latency = handle->fill.aim/frame_sz;

      if (!m) {
         handle->mutex = _aaxMutexCreate(handle->mutex);
      }

      handle->min_tracks = 1;
      handle->max_tracks = _AAX_MAX_SPEAKERS;
      handle->min_frequency = _AAX_MIN_MIXER_FREQUENCY;
      handle->max_frequency = _AAX_MAX_MIXER_FREQUENCY;

      _aaxPulseAudioContextConnect(handle);
      if (handle->ctx)
      {
         pa_operation *opr;
         _sink_info_t si;

         si.names = handle->names[m];
         si.descriptions = handle->descriptions[m];
         si.loop = handle->ml;

         ppa_threaded_mainloop_lock(handle->ml);
         if (mode == AAX_MODE_READ) {
            opr = ppa_context_get_source_info_list(handle->ctx,
                                                   source_device_cb, &si);
         } else {
            opr = ppa_context_get_sink_info_list(handle->ctx,
                                                 sink_device_cb, &si);
         }

         if (opr)
         {
            while(ppa_operation_get_state(opr) == PA_OPERATION_RUNNING) {
               ppa_threaded_mainloop_wait(handle->ml);
            }
            ppa_operation_unref(opr);
         }
         ppa_threaded_mainloop_unlock(handle->ml);
      }
      else
      {
         ppa_threaded_mainloop_free(handle->ml);
         free(handle);
         handle = NULL;
      }
   }

   return handle;
}

static int
_aaxPulseAudioDriverFreeHandle(UNUSED(void *id))
{
   _aaxCloseLibrary(audio);
   audio = NULL;

   return true;
}

static void *
_aaxPulseAudioDriverConnect(void *config, const void *id, xmlId *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      id = handle = _aaxPulseAudioDriverNewHandle(mode);
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
_aaxPulseAudioDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = false;

   if (handle)
   {
      if (handle->ml) {
         ppa_threaded_mainloop_stop(handle->ml);
      }

      if (handle->pa) {
         ppa_stream_unref(handle->pa);
      }

      if (handle->ctx)
      {
         ppa_context_disconnect(handle->ctx);
         ppa_context_unref(handle->ctx);
         handle->ctx = NULL;
      }

      if (handle->ml) {
         ppa_threaded_mainloop_free(handle->ml);
      }

      if (handle->driver != _const_pulseaudio_default_name) {
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

      rv = true;
   }

   return rv;
}

static int
_aaxPulseAudioDriverSetup(const void *id, float *refresh_rate, int *fmt,
                   unsigned int *tracks, float *speed, UNUSED(int *bitrate),
                   int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int period_samples;
   int periods, samples, frame_sz;
   pa_sample_spec req;
   int rv = false;

   *fmt = AAX_PCM16S;

   req = handle->spec;
   req.rate = (unsigned int)*speed;
   req.channels = *tracks;
   if (req.channels > handle->spec.channels) {
      req.channels = handle->spec.channels;
   }

   if (*refresh_rate > 100) {
      *refresh_rate = 100;
   }

   periods = DEFAULT_PERIODS;

   if (!registered) {
      period_samples = get_pow2(req.rate/(*refresh_rate));
   } else {
      period_samples = get_pow2(req.rate/period_rate);
   }
   req.format = is_bigendian() ? PA_SAMPLE_S16BE : PA_SAMPLE_S16LE;
   frame_sz = req.channels*handle->bits_sample/8;
   samples = period_samples*frame_sz;

   if (ppa_sample_spec_valid(&req))
   {
      pa_stream_flags_t flags;
      int error;

      handle->spec = req;
      handle->period_frames = samples;

      flags = PA_STREAM_FIX_FORMAT | PA_STREAM_FIX_RATE |
              PA_STREAM_FIX_CHANNELS | PA_STREAM_DONT_MOVE |
              PA_STREAM_AUTO_TIMING_UPDATE;
      if (handle->mode != AAX_MODE_READ) {
         flags |= PA_STREAM_START_CORKED;
      }

      _aaxPulseAudioStreamConnect(handle, flags, &error);
      if (!handle->pa || error != PA_STREAM_READY) {
         _aaxPulseAudioDriverLogVar(id, "connect: %s", ppa_strerror(error));
      }

      handle->format = _aaxPulseAudioGetFormat(handle->spec.format);
      switch(handle->format & AAX_FORMAT_NATIVE)
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
         rv = false;
         break;
      }

      handle->bits_sample = aaxGetBitsPerSample(handle->format);
      handle->period_frames = period_samples;

      *fmt = handle->format;
      *speed = handle->spec.rate;
      *tracks = handle->spec.channels;
      if (!registered) {
         *refresh_rate = handle->spec.rate/(float)handle->period_frames;
      } else {
         *refresh_rate = period_rate;
      }
      handle->refresh_rate = *refresh_rate;

      frame_sz = handle->spec.channels*handle->bits_sample/8;
      handle->fill.aim = (float)periods*frame_sz*period_samples/handle->spec.rate;
      handle->latency = handle->fill.aim/frame_sz;

#if 0
 printf("spec:\n");
 printf("   frequency: %i\n", handle->spec.rate);
 printf("   format:    %x\n", handle->format);
 printf("   channels:  %i\n", handle->spec.channels);
 printf("   samples:   %i\n", handle->period_frames);
#endif

      handle->render = _aaxSoftwareInitRenderer(handle->latency,
                                                handle->mode, registered);
      if (handle->render)
      {
         const char *rstr = handle->render->info(handle->render->id);

         snprintf(_pulseaudio_id_str, MAX_ID_STRLEN ,"%s %s %s",
                  DEFAULT_RENDERER, ppa_get_library_version(), rstr);
         rv = true;
      }
      else {
            _AAX_DRVLOG("unable to get the renderer");
      }
   }
   else {
      _AAX_DRVLOG("incompatible hardware configuration");
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
      snprintf(str,255, "  buffer size: %i bytes", handle->period_frames*handle->spec.channels*handle->bits_sample/8);
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
 printf("  buffer size: %u bytes\n", handle->period_frames*handle->bits_sample/8);
 printf("  latency:  %5.2f ms\n", 1e3f*handle->latency);
 printf("  timer based: yes\n");
 printf("  channels: %i, bytes/sample: %i\n", handle->spec.channels, handle->bits_sample/8);
#endif
   }
   while (0);

   return rv;
}


static ssize_t
_aaxPulseAudioDriverCapture(const void *id, void **data, ssize_t *offset, size_t *frames, UNUSED(void *scratch), UNUSED(size_t scratchlen), float gain, UNUSED(char batched))
{
   _driver_t *handle = (_driver_t *)id;
   size_t len, nframes = *frames;
   int tracks, frame_sz;
   size_t offs = *offset;

   *offset = 0;
   if ((handle->mode != 0) || (frames == 0) || (data == 0)) {
     return false;
   }

   if (nframes == 0) {
      return true;
   }

   if (handle->dataBuffer == 0)
   {
      size_t size = CAPTURE_BUFFER_SIZE;
      const pa_buffer_attr *a;

      a = ppa_stream_get_buffer_attr(handle->pa);
      if (a) size = DEFAULT_PERIODS*a->maxlength;

      handle->dataBuffer = _aaxDataCreate(1, size, 1);
      if (handle->dataBuffer == 0) return false;

#if CAPTURE_CALLBACK
      ppa_stream_set_read_callback(handle->pa, stream_capture_cb, handle);
#endif
   }

   *frames = 0;
   tracks = handle->spec.channels;
   frame_sz = tracks*handle->bits_sample/8;

#if !CAPTURE_CALLBACK
   req = nframes*frame_sz;
   if (_aaxDataGetDataAvail(handle->dataBuffer, 0) < req)
   {
      int ctr = 10;

      ppa_threaded_mainloop_lock(handle->ml);

      do
      {
         const void *buf;
         size_t len = 0;

         ppa_stream_peek(handle->pa, &buf, &len);
         if (len <= 0) {
            ppa_threaded_mainloop_wait(handle->ml);
         }
         else if (buf)
         {
            if (_aaxDataGetOffset(handle->dataBuffer, 0)+len < _aaxDataGetSize(handle->dataBuffer))
            {
               int res = _aaxDataAdd(handle->dataBuffer, 0, buf, len);
               if (res) ppa_stream_drop(handle->pa);
            }
         } else {
            ppa_stream_drop(handle->pa);
         }

         req -= len;
      }
      while(req > 0 && --ctr);

      ppa_threaded_mainloop_unlock(handle->ml);
   }
#endif

   len = _aaxDataGetDataAvail(handle->dataBuffer, 0);
   if (len > nframes*frame_sz) len = nframes*frame_sz;
   if (len)
   {
      void *buf = _aaxDataGetData(handle->dataBuffer, 0);

      nframes = len/frame_sz;
      handle->cvt_from_intl((int32_t**)data, buf, offs, tracks, nframes);
      _aaxDataMove(handle->dataBuffer, 0, NULL, len);

      gain = _pulseaudio_set_volume(handle, NULL, offs, nframes, tracks, gain);
      if (gain > LEVEL_96DB && fabsf(gain-1.0f) > LEVEL_96DB)
      {
         unsigned int t;
         for (t=0; t<tracks; t++)
         {
            int32_t *ptr = (int32_t*)data[t]+offs;
            _batch_imul_value(ptr, ptr, sizeof(int32_t), nframes, gain);
         }
      }
      *frames = nframes;
   }

   return true;
}

static size_t
_aaxPulseAudioDriverPlayback(const void *id, void *src, UNUSED(float pitch), UNUSED(float gain), UNUSED(char batched))
{
   _aaxRingBuffer *rb = (_aaxRingBuffer *)src;
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

   size = BUFFER_SIZE_FACTOR*DEFAULT_PERIODS*period_frames;
   if (handle->dataBuffer == 0 || (_aaxDataGetSize(handle->dataBuffer) < size*frame_sz))
   {
      _aaxDataDestroy(handle->dataBuffer);
      handle->dataBuffer = _aaxDataCreate(1, size, frame_sz);
      if (handle->dataBuffer == 0) return -1;

      ppa_stream_set_write_callback(handle->pa, stream_playback_cb, handle);
   }

   offs = rb->get_parami(rb, RB_OFFSET_SAMPLES);
   period_frames -= offs;
   size = period_frames*frame_sz;

   free = _aaxDataGetFreeSpace(handle->dataBuffer, 0);
   if (free > size)
   {
      unsigned char *data;

      _pulseaudio_set_volume(handle, rb, offs, period_frames, no_tracks, gain);

      _aaxMutexLock(handle->mutex);

      data = _aaxDataGetPtr(handle->dataBuffer, 0);
      sbuf = (const int32_t**)rb->get_tracks_ptr(rb, RB_READ);
      handle->cvt_to_intl(data, sbuf, offs, no_tracks, period_frames);
      rb->release_tracks_ptr(rb);

      _aaxDataIncreaseOffset(handle->dataBuffer, 0, size);

      _aaxMutexUnLock(handle->mutex);

      rv = period_frames;
   }

   return rv;
}

static int
_aaxPulseAudioDriverSetName(const void *id, int type, const char *name)
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
_aaxPulseAudioDriverGetName(const void *id, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   /* TODO: distinguish between playback and record */
   if (handle && handle->driver && (mode < AAX_MODE_WRITE_MAX))
      ret = _aax_strdup(handle->driver);

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
   int rv = false;

   switch(state)
   {
   case DRIVER_PAUSE:
      if (handle)
      {
         ppa_threaded_mainloop_lock(handle->ml);
         if (!ppa_stream_is_corked(handle->pa)) {
            ppa_stream_cork(handle->pa, 1, NULL, NULL);
         }
         ppa_threaded_mainloop_unlock(handle->ml);
         rv = true;
      }
      break;
   case DRIVER_RESUME:
      if (handle)
      {
         ppa_threaded_mainloop_lock(handle->ml);
         if (ppa_stream_is_corked(handle->pa)) {
            ppa_stream_cork(handle->pa, 0, NULL, NULL);
         }
         ppa_threaded_mainloop_unlock(handle->ml);
         rv = true;
      }
      break;
   case DRIVER_AVAILABLE:
   case DRIVER_SHARED_MIXER:
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
         rv = (float)handle->period_frames/handle->spec.channels;
         break;

		/* boolean */
      case DRIVER_SHARED_MODE:
      case DRIVER_TIMER_MODE:
         rv = true;
         break;
      case DRIVER_BATCHED_MODE:
      default:
         break;
      }
   }
   return rv;
}

static char *
_aaxPulseAudioDriverGetDevices(const void *id, int mode)
{
   static char names[2][MAX_DEVICES_LIST] = {
     DEFAULT_DEVNAME"\0\0", DEFAULT_DEVNAME"\0\0"
   };
   static time_t t_previous[2] = { 0, 0 };
   int m = (mode == AAX_MODE_READ) ? 1 : 0;
   char *rv = (char*)&names[m];
   time_t t_now;

   t_now = time(NULL);
   if (t_now > (t_previous[m]+5))
   {
      _driver_t *handle = (_driver_t *)id;
      _driver_t *ptr = NULL;

      t_previous[m] = t_now;
      if (handle) {
         rv = handle->descriptions[m];
      }
      else
      {
         ptr = handle = calloc(1, sizeof(_driver_t));
         if (handle) {
            _aaxPulseAudioContextConnect(handle);
         }
      }

      if (handle && handle->ctx)
      {
         pa_operation *opr;
         _sink_info_t si;
         size_t sl;
         char *s;

         t_previous[m] = t_now;

         si.names = handle->names[m];
         si.descriptions = rv;
         si.loop = handle->ml;

         ppa_threaded_mainloop_lock(handle->ml);
         if (m) {
            opr = ppa_context_get_source_info_list(handle->ctx, source_device_cb, &si);
         } else {
            opr = ppa_context_get_sink_info_list(handle->ctx, sink_device_cb, &si);
         }

         if (opr)
         {
            while(ppa_operation_get_state(opr) == PA_OPERATION_RUNNING) {
               ppa_threaded_mainloop_wait(handle->ml);
            }
            ppa_operation_unref(opr);
         }
         ppa_threaded_mainloop_unlock(handle->ml);

         if (ptr) {
            _aaxPulseAudioDriverDisconnect(handle);
         }

         sl = strlen(rv);
         s = rv + sl+1;
         if (*s != '\0') {
            memmove(rv, s, MAX_DEVICES_LIST-sl);;
         }
      }
   }

   return rv;
}

static char *
_aaxPulseAudioDriverGetInterfaces(UNUSED(const void *id), UNUSED(const char *driver), UNUSED(int mode))
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

   // Always null terminate the string
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

static float
_pulseaudio_set_volume(_driver_t *handle, _aaxRingBuffer *rb, ssize_t offset, uint32_t period_frames, unsigned int tracks, float volume)
{
   float gain = fabsf(volume);
   pa_operation *op = NULL;
   float rv = 0;

   if (ppa_cvolume_set)
   {
      pa_volume_t vol;

      vol = lroundf(gain*PA_VOLUME_NORM);
      if (vol > PA_VOLUME_MAX) {
         vol = PA_VOLUME_MAX;
      }

      gain = (float)vol/PA_VOLUME_NORM / fabsf(volume);

      if (fabsf((float)handle->volume - vol) > 0)
      {
         pa_cvolume cvol;

         handle->volume = vol;

         ppa_threaded_mainloop_lock(handle->ml);

         ppa_cvolume_set(&cvol, tracks, vol);
         if (handle->mode == AAX_MODE_READ) {
            op = ppa_context_set_source_output_volume(handle->ctx,
                                              ppa_stream_get_index(handle->pa),
                                              &cvol, NULL, NULL);
         } else {
            op = ppa_context_set_sink_input_volume(handle->ctx,
                                              ppa_stream_get_index(handle->pa),
                                              &cvol, NULL, NULL);
         }

         if (op) {
            ppa_operation_unref(op);
         }
         ppa_threaded_mainloop_unlock(handle->ml);
      }
   }

   /* software volume fallback */
   if (rb && fabsf(gain - 1.0f) > LEVEL_32DB) {
      rb->data_multiply(rb, offset, period_frames, gain);
   }

   return rv;
}

/* ----------------------------------------------------------------------- */

static void
stream_playback_cb(pa_stream *stream, size_t len, void *be_ptr)
{
   _driver_t *be_handle = (_driver_t *)be_ptr;
   _handle_t *handle = (_handle_t *)be_handle->handle;
   void *id = be_handle;

   if (_IS_PLAYING(handle))
   {
      _data_t *buf = be_handle->dataBuffer;
      void *data;
      int res;

      assert(be_handle->mode != AAX_MODE_READ);
      assert(be_handle->dataBuffer);

      _aaxMutexLock(be_handle->mutex);

      if (_aaxDataGetDataAvail(buf, 0) < len) {
         len = _aaxDataGetDataAvail(buf, 0);
      }

      data = _aaxDataGetData(buf, 0);
      res = ppa_stream_write(be_handle->pa, data, len, NULL, 0LL,
                             PA_SEEK_RELATIVE);
      if (res >= 0) {
         _aaxDataMove(buf, 0, NULL, len);
      } else if (ppa_strerror) {
         _AAX_DRVLOG(ppa_strerror(res));
      }

      _aaxMutexUnLock(be_handle->mutex);
   }
// ppa_threaded_mainloop_signal(be_handle->ml, 0);
}

#if CAPTURE_CALLBACK
static void
stream_capture_cb(pa_stream *p, size_t nbytes, void *be_ptr)
{
   _driver_t *be_handle = (_driver_t *)be_ptr;
   const void *buf;
   size_t len = 0;

   len = _aaxDataGetFreeSpace(be_handle->dataBuffer, 0);
   ppa_stream_peek(be_handle->pa, &buf, &len);
   if (buf)
   {
      if (_aaxDataGetOffset(be_handle->dataBuffer, 0)+len < _aaxDataGetSize(be_handle->dataBuffer))
      {
         int res = _aaxDataAdd(be_handle->dataBuffer, 0, buf, len);
         if (res) ppa_stream_drop(be_handle->pa);
      }
   } else {
      ppa_stream_drop(be_handle->pa);
   }
}
#endif

#if USE_PULSE_THREAD
static int
_aaxPulseAudioDriverThread(void* config)
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
      return false;
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
         dest_rb->set_format(dest_rb, AAX_PCM24S, true);
         dest_rb->set_paramf(dest_rb, RB_FREQUENCY, freq);
         dest_rb->set_paramf(dest_rb, RB_DURATION_SEC, delay_sec);
         dest_rb->init(dest_rb, true);
         dest_rb->set_state(dest_rb, RB_STARTED);

         handle->ringbuffer = dest_rb;
         _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
      }
   }

   dest_rb = handle->ringbuffer;
   if (!dest_rb) {
      return false;
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

         if (handle->batch_finished) { // batched mode
            _aaxSemaphoreRelease(handle->batch_finished);
         }
      }

      res = _aaxSignalWaitTimed(&handle->thread.signal, dt);
   }
   while (res == AAX_TIMEOUT || res == true);

   _aaxMutexUnLock(handle->thread.signal.mutex);

   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      be->destroy_ringbuffer(handle->ringbuffer);
      handle->ringbuffer = NULL;
   }

   return handle ? true : false;
}
#endif

static const char*
detect_name(_driver_t *handle)
{
   int m = (handle->mode == AAX_MODE_READ) ? 1 : 0;
   const char *ptr = handle->descriptions[m];
   const char *rv = NULL;
   int di = 0, ni = 0;

   if (handle->driver)
   {
      while (strlen(ptr) && strcmp(handle->driver, ptr)) {
         ptr += strlen(ptr)+1;
         di++;
      }

      if (strlen(ptr) > 0)
      {
         ptr = handle->names[m];
         for(ni=0; ni<di; ++ni) {
            ptr += strlen(ptr)+1;
         }
         rv = ptr;
      }
   }

   return rv;
}

static void
context_state_cb(pa_context *context, void *id)
{
   _driver_t *handle = (_driver_t *)id;
   switch (ppa_context_get_state(context))
   {
   case PA_CONTEXT_READY:
   case PA_CONTEXT_TERMINATED:
   case PA_CONTEXT_FAILED:
      ppa_threaded_mainloop_signal(handle->ml, 0);
      break;
   case PA_CONTEXT_UNCONNECTED:
   case PA_CONTEXT_CONNECTING:
   case PA_CONTEXT_AUTHORIZING:
   case PA_CONTEXT_SETTING_NAME:
      break;
   }
}

static void
stream_state_cb(pa_stream *stream, void *id)
{
    _driver_t *handle = (_driver_t *)id;
    switch (ppa_stream_get_state(stream))
    {
    case PA_STREAM_READY:
    {
#if 0
       pa_stream *s = stream;
       const pa_buffer_attr *a;
       char cmt[PA_CHANNEL_MAP_SNPRINT_MAX], sst[PA_SAMPLE_SPEC_SNPRINT_MAX];

       fprintf(stderr, "Stream successfully created.\n");

       if (!(a = ppa_stream_get_buffer_attr(s)))
          fprintf(stderr, "pa_stream_get_buffer_attr() failed: %s\n", ppa_strerror(ppa_context_errno(ppa_stream_get_context(s))));
       else {
          fprintf(stderr, "Buffer metrics: maxlength=%u, tlength=%u, prebuf=%u, minreq=%u\n", a->maxlength, a->tlength, a->prebuf, a->minreq);
       }

       fprintf(stderr, "Using sample spec '%s', channel map '%s'.\n",
              ppa_sample_spec_snprint(sst, sizeof(sst), ppa_stream_get_sample_spec(s)),
              ppa_channel_map_snprint(cmt, sizeof(cmt), ppa_stream_get_channel_map(s)));

      fprintf(stderr, "Connected to device %s (%u, %ssuspended).\n",
              ppa_stream_get_device_name(s),
              ppa_stream_get_device_index(s),
              ppa_stream_is_suspended(s) ? "" : "not ");
#endif
    }
    case PA_STREAM_FAILED:
    case PA_STREAM_TERMINATED:
       ppa_threaded_mainloop_signal(handle->ml, 0);
       break;
    case PA_STREAM_UNCONNECTED:
    case PA_STREAM_CREATING:
       break;
    }
}

static void
stream_latency_update_cb(pa_stream *stream, void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int negative = 0;
   pa_usec_t rv;

   if (ppa_stream_get_latency(handle->pa, &rv, &negative) >= 0)
   {
      if (rv && !negative) {
         handle->latency = (float)rv*1e-6f;
      }
   }
   ppa_threaded_mainloop_signal(handle->ml, 0);
}

static void
sink_device_cb(UNUSED(pa_context *context), const pa_sink_info *info, int eol, void *si)
{
   _sink_info_t *handle = si;
   char *sptr, *ptr;
   size_t slen, len;
   char is_lazy;

   if (eol)
   {
      ppa_threaded_mainloop_signal(handle->loop, 0);
      return;
   }

   is_lazy = false;
   if ((env && _aax_getbool(env)) ||
       (info->name && (!strncmp(info->name, "alsa_output.usb", 15) ||
                       !strncmp(info->name, "alsa_output.bluetooth", 21))))
   {
      is_lazy = true;
   }

   if (is_lazy)
   {
      sptr = ptr = handle->descriptions;
      slen = strlen(sptr);
      if (slen)
      {
         char *p = memmem(ptr, MAX_DEVICES_LIST, "\0\0", 2);
         if (p)
         {
            slen = p - sptr;
            ptr = p+1;
         }
      }

      len = (MAX_DEVICES_LIST-2) - slen;
      slen = strlen(info->description);
      if (len > slen)
      {
         sprintf(ptr, "%s", info->description);
//       printf("# '%s'\n", info->description);
         ptr += slen;
         *ptr++ = '\0';
         *ptr = '\0';
      }

      sptr = ptr = handle->names;
      if (sptr)
      {
         slen = strlen(ptr);
         if (slen)
         {
            char *p = memmem(ptr, MAX_DEVICES_LIST, "\0\0", 2);
            if (p)
            {
               slen = p - sptr;
               ptr = p+1;
            }
         }

         len = (MAX_DEVICES_LIST-2) - slen;
         slen = strlen(info->name);
         if (len > slen)
         {
            sprintf(ptr, "%s", info->name);
            ptr += slen;
            *ptr++ = '\0';
            *ptr = '\0';
         }
      }
   }
}

static void
source_device_cb(UNUSED(pa_context *context), const pa_source_info *info, int eol, void *si)
{
    _sink_info_t *handle = si;
   char *sptr, *ptr;
   size_t slen, len;
   char is_lazy;

   if (eol)
   {
      ppa_threaded_mainloop_signal(handle->loop, 0);
      return;
   }

   is_lazy = false;
   if ((env && _aax_getbool(env)) ||
       (info->name && (!strncmp(info->name, "alsa_output.usb", 15) ||
                       !strncmp(info->name, "alsa_output.bluetooth", 21))))
   {
      is_lazy = true;
   }

   if (is_lazy)
   {
      sptr = ptr = handle->descriptions;
      slen = strlen(sptr);
      if (slen)
      {
         char *p = memmem(ptr, MAX_DEVICES_LIST, "\0\0", 2);
         if (p)
         {
            slen = p - sptr;
            ptr = p+1;
         }
      }

      len = (MAX_DEVICES_LIST-2) - slen;
      slen = strlen(info->description);
      if (len > slen)
      {
         sprintf(ptr, "%s", info->description);
         ptr += slen;
         *ptr++ = '\0';
         *ptr = '\0';
      }

      sptr = ptr = handle->names;
      if (sptr)
      {
         slen = strlen(ptr);
         if (slen)
         {
            char *p = memmem(ptr, MAX_DEVICES_LIST, "\0\0", 2);
            if (p)
            {
               slen = p - sptr;
               ptr = p+1;
            }
         }

         len = (MAX_DEVICES_LIST-2) - slen;
         slen = strlen(info->name);
         if (len > slen)
         {
            sprintf(ptr, "%s", info->name);
            ptr += slen;
            *ptr++ = '\0';
            *ptr = '\0';
         }
      }
   }
}

static void
_aaxPulseAudioContextConnect(_driver_t *handle)
{
   static char pulse_avail = true;
   const char *name = AAX_LIBRARY_STR;
   char buf[PATH_MAX] = "";

   if (!pulse_avail)
   {
      static time_t t_previous = 0;
      time_t t_now = time(NULL);
      if (t_now > (t_previous+1))
      {
         t_previous = t_now;
         pulse_avail = true;
      }
   }

   if (ppa_get_binary_name && ppa_get_binary_name(buf, sizeof(buf))) {
      name = ppa_path_get_filename(buf);
   }

   handle->ml = ppa_threaded_mainloop_new();
   if (handle->ml)
   {
      handle->ml_api = ppa_threaded_mainloop_get_api(handle->ml);
      handle->ctx = ppa_context_new(handle->ml_api, name);
      if (handle->ctx)
      {
         char *srv = NULL; // connect to the default server (for now)

         ppa_context_set_state_callback(handle->ctx, context_state_cb, handle);
         if (ppa_context_connect(handle->ctx, srv, 0, NULL) >= 0)
         {
            ppa_threaded_mainloop_lock(handle->ml);

            if (ppa_threaded_mainloop_start(handle->ml) >= 0)
            {
               pa_context_state_t state;
               while ((state = ppa_context_get_state(handle->ctx)) != PA_CONTEXT_READY)
               {
                  pa_context_state_t state;

                  state = ppa_context_get_state(handle->ctx);

                  if (!PA_CONTEXT_IS_GOOD(state))
                  {
//                   error = ppa_context_errno(handle->ctx);
                     break;
                  }
                  ppa_threaded_mainloop_wait(handle->ml);
               }
            }

            ppa_threaded_mainloop_unlock(handle->ml);
         }
         else
         {
            pulse_avail = false;
            ppa_context_unref(handle->ctx);
            handle->ctx = NULL;
         }
      }
      else
      {
         ppa_threaded_mainloop_free(handle->ml);
         handle->ml = NULL;
      }
   }
}

static enum aaxFormat
_aaxPulseAudioGetFormat(pa_sample_format_t format)
{
   enum aaxFormat rv = AAX_PCM16S;
   switch(format)
   {
      case PA_SAMPLE_U8 :
         rv = AAX_PCM8U;
         break;
      case PA_SAMPLE_ALAW:
         rv = AAX_ALAW;
         break;
      case PA_SAMPLE_ULAW:
         rv = AAX_MULAW;
         break;
#if __BYTE_ORDER == __BIG_ENDIAN
      case PA_SAMPLE_S16LE:
         rv = AAX_PCM16S_LE;
         break;
      case PA_SAMPLE_S16BE:
         rv = AAX_PCM16S;
         break;
      case PA_SAMPLE_FLOAT32LE:
         rv = AAX_FLOAT_LE;
         break;
      case PA_SAMPLE_FLOAT32BE:
         rv = AAX_FLOAT;
         break;
      case PA_SAMPLE_S32LE:
         rv = AAX_PCM32S_LE;
         break;
      case PA_SAMPLE_S32BE:
         rv = AAX_PCM32S;
         break;
      case PA_SAMPLE_S24_32LE:
         rv = AAX_PCM24S_LE;
         break;
      case PA_SAMPLE_S24_32BE:
         rv = AAX_PCM24S;
         break;
#else
      case PA_SAMPLE_S16LE:
         rv = AAX_PCM16S;
         break;
      case PA_SAMPLE_S16BE:
         rv = AAX_PCM16S_BE;
         break;
      case PA_SAMPLE_FLOAT32LE:
         rv = AAX_FLOAT;
         break;
      case PA_SAMPLE_FLOAT32BE:
         rv = AAX_FLOAT_BE;
         break;
      case PA_SAMPLE_S32LE:
         rv = AAX_PCM32S;
         break;
      case PA_SAMPLE_S32BE:
         rv = AAX_PCM32S_BE;
         break;
      case PA_SAMPLE_S24LE:
         rv = AAX_PCM24S_PACKED;
         break;
      case PA_SAMPLE_S24_32LE:
         rv = AAX_PCM24S;
         break;
      case PA_SAMPLE_S24_32BE:
         rv = AAX_PCM24S_BE;
         break;
#endif
      default:
         break;
   }
   return rv;
}

static void
_aaxPulseAudioStreamConnect(_driver_t *handle, pa_stream_flags_t flags, int *error)
{
   const char *agent = aaxGetString(AAX_VERSION_STRING);
   pa_channel_map map;

   *error = PA_STREAM_READY;

   if (!ppa_channel_map_init_auto(&map, handle->spec.channels, PA_CHANNEL_MAP_WAVEEX)) {
      _AAX_SYSLOG("pulse; unsupported channel map.");
   }

   ppa_threaded_mainloop_lock(handle->ml);
   handle->pa = ppa_stream_new(handle->ctx, agent, &handle->spec, &map);
   if (handle->pa)
   {
      pa_stream * pa = handle->pa;
      pa_buffer_attr attr;
      unsigned int buflen;
      const char *name;
      int res;

      ppa_stream_set_state_callback(pa, stream_state_cb, handle);
      ppa_stream_set_latency_update_callback(pa, stream_latency_update_cb, handle);

      buflen = handle->period_frames*handle->bits_sample/8;

      attr.fragsize = buflen;		// recording only
      attr.maxlength = 2*DEFAULT_PERIODS*buflen;
      attr.minreq = (uint32_t)-1;
      attr.prebuf = 0;			// playback only
      attr.tlength = DEFAULT_PERIODS*buflen;	// playback only
      flags |= PA_STREAM_ADJUST_LATENCY;

      name = detect_name(handle);
      if (handle->mode == AAX_MODE_READ) {
         res = ppa_stream_connect_record(pa, name, &attr, flags);
      } else {
         res = ppa_stream_connect_playback(pa, name, &attr, flags, NULL, NULL);
      }

      if (res >= 0)
      {
         const pa_sample_spec *spec;

         do
         {
            pa_stream_state_t state;

            state = ppa_stream_get_state(pa);
            if (state == PA_STREAM_READY) break;

            if (!PA_STREAM_IS_GOOD(state))
            {
               *error = ppa_context_errno(handle->ctx);
               break;
            }

            ppa_threaded_mainloop_wait(handle->ml);
         }
         while(1);

         spec = ppa_stream_get_sample_spec(handle->pa);
         if (spec) {
            memcpy(&handle->spec, spec, sizeof(pa_sample_spec));
         }
      }
      else {
         *error = ppa_context_errno(handle->ctx);
      }
   }
   ppa_threaded_mainloop_unlock(handle->ml);
}

#endif /* HAVE_PULSEAUDIO_H */
