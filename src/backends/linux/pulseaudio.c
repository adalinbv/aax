/*
 * Copyright 2019-2020 by Erik Hofman.
 * Copyright 2019-2020 by Adalin B.V.
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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
# include <string.h>		/* strstr, strncmp */
#endif
#include <stdarg.h>		/* va_start */

#if HAVE_PULSE_PULSEAUDIO_H
// #include <pulse/pulseaudio.h>
#include <pulse/def.h>
#include <pulse/util.h>
#include <pulse/error.h>
#include <pulse/stream.h>
#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/thread-mainloop.h>

#include <aax/aax.h>
#include <xml.h>

#include <base/types.h>
#include <base/logging.h>
#include <base/memory.h>
#include <base/dlsym.h>
#include <base/timer.h>

#include <backends/driver.h>
#include <ringbuffer.h>
#include <arch.h>
#include <api.h>

#include <software/renderer.h>
#include "audio.h"

#define DEFAULT_RENDERER	"PulseAudio"
#define MAX_ID_STRLEN		96

#define DEFAULT_OUTPUT_RATE	48000
#define DEFAULT_DEVNAME		NULL
#define DEFAULT_REFRESH		25.0

#define USE_PID			AAX_TRUE
#define FILL_FACTOR		4.0f

#define MAX_DEVICES_LIST	4096

#define _AAX_DRVLOG(a)         _aaxPulseAudioDriverLog(id, 0, 0, a)
#define HW_VOLUME_SUPPORT(a)	((a->mixfd >= 0) && a->volumeMax)

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
   char *driver;
   char *devname;
   _aaxRenderer *render;

   pa_stream *pa;
   pa_context *ctx;
   pa_threaded_mainloop *ml;

   pa_sample_spec spec;
   pa_buffer_attr attr;

// char no_periods;
   char bits_sample;
   unsigned int format;
   unsigned int period_frames;
   enum aaxRenderMode mode;
   float refresh_rate;
   float latency;

   _data_t *dataBuffer;
   _aaxMutex *mutex;

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

   char descriptions[2][MAX_DEVICES_LIST];
   char names[2][MAX_DEVICES_LIST];

} _driver_t;

typedef struct
{
   _driver_t *handle;
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
DECL_FUNCTION(pa_context_get_sink_info_by_name);
DECL_FUNCTION(pa_context_get_sink_info_list);
DECL_FUNCTION(pa_context_get_source_info_by_name);
DECL_FUNCTION(pa_context_get_source_info_list);
DECL_FUNCTION(pa_context_unref);
DECL_FUNCTION(pa_context_errno);
DECL_FUNCTION(pa_stream_new);
DECL_FUNCTION(pa_stream_set_state_callback);
DECL_FUNCTION(pa_stream_connect_playback);
DECL_FUNCTION(pa_stream_connect_record);
DECL_FUNCTION(pa_stream_disconnect);
DECL_FUNCTION(pa_stream_unref);
DECL_FUNCTION(pa_stream_write);
DECL_FUNCTION(pa_stream_writable_size);
DECL_FUNCTION(pa_stream_set_write_callback);
DECL_FUNCTION(pa_stream_set_read_callback);
DECL_FUNCTION(pa_stream_get_latency);
DECL_FUNCTION(pa_stream_set_latency_update_callback);
DECL_FUNCTION(pa_stream_get_state);
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


static void stream_state_cb(pa_stream*, void*);
static void stream_latency_update_cb(pa_stream*, void*);
static void stream_write_cb(pa_stream*, size_t, void*);
// static void stream_record_cb(pa_stream*, size_t, void*);
static void sink_device_cb(pa_context*, const pa_sink_info*, int, void*);
static void source_device_cb(pa_context*, const pa_source_info*, int, void*);

static const char* detect_name(_driver_t*);
static float _aaxGetLatency(_driver_t*);
static void _aaxContextConnect(_driver_t*);
static void _aaxStreamConnect(_driver_t*,  pa_stream_flags_t flags, int*);
static char *_aaxPulseAudioDriverLogVar(const void *, const char *, ...);

static const char *_const_pulseaudio_default_name = DEFAULT_DEVNAME;
const char *_const_pulseaudio_default_device = NULL;

int
_aaxPulseAudioDriverDetect(UNUSED(int mode))
{
   static void *audio = NULL;
   static int rv = AAX_FALSE;
   char *error = NULL;

   _AAX_LOG(LOG_DEBUG, __func__);

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
         TIE_FUNCTION(pa_context_get_sink_info_by_name);
         TIE_FUNCTION(pa_context_get_sink_info_list);
         TIE_FUNCTION(pa_context_get_source_info_by_name);
         TIE_FUNCTION(pa_context_get_source_info_list);
         TIE_FUNCTION(pa_context_unref);
         TIE_FUNCTION(pa_context_errno);
         TIE_FUNCTION(pa_stream_set_state_callback);
         TIE_FUNCTION(pa_stream_connect_playback);
         TIE_FUNCTION(pa_stream_connect_record);
         TIE_FUNCTION(pa_stream_disconnect);
         TIE_FUNCTION(pa_stream_unref);
         TIE_FUNCTION(pa_stream_write);
         TIE_FUNCTION(pa_stream_writable_size);
         TIE_FUNCTION(pa_stream_set_write_callback);
         TIE_FUNCTION(pa_stream_set_read_callback);
         TIE_FUNCTION(pa_stream_get_latency);
         TIE_FUNCTION(pa_stream_set_latency_update_callback);
         TIE_FUNCTION(pa_stream_get_state);
         TIE_FUNCTION(pa_stream_get_device_name);
         TIE_FUNCTION(pa_stream_cork);
         TIE_FUNCTION(pa_stream_is_corked);
         TIE_FUNCTION(pa_channel_map_init_auto);
         TIE_FUNCTION(pa_operation_get_state);
         TIE_FUNCTION(pa_operation_unref);
//       TIE_FUNCTION(pa_signal_new);
//       TIE_FUNCTION(pa_signal_done);
//       TIE_FUNCTION(pa_xfree);

         TIE_FUNCTION(pa_stream_get_buffer_attr);
         TIE_FUNCTION(pa_stream_get_context);
         TIE_FUNCTION(pa_sample_spec_valid);
         TIE_FUNCTION(pa_sample_spec_snprint);
         TIE_FUNCTION(pa_channel_map_snprint);
         TIE_FUNCTION(pa_stream_get_sample_spec);
         TIE_FUNCTION(pa_stream_get_channel_map);
         TIE_FUNCTION(pa_stream_get_device_name);
         TIE_FUNCTION(pa_stream_get_device_index);
         TIE_FUNCTION(pa_stream_is_suspended);
      }

      error = _aaxGetSymError(0);
      if (!error)
      {
         /* useful but not required */
         TIE_FUNCTION(pa_strerror);
         TIE_FUNCTION(pa_get_binary_name);
         TIE_FUNCTION(pa_path_get_filename);
         TIE_FUNCTION(pa_get_library_version);
         rv = AAX_TRUE;
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
      char m = (mode == AAX_MODE_READ) ? 1 : 0;
      int frame_sz;

      handle->driver = (char*)_const_pulseaudio_default_name;
      handle->mode = mode;
      handle->bits_sample = 16;
      handle->spec.rate = 44100;
      handle->spec.channels = 2;
      handle->spec.format = is_bigendian()? PA_SAMPLE_S16BE:PA_SAMPLE_S16LE;
      handle->period_frames = get_pow2(handle->spec.rate/DEFAULT_REFRESH);

      frame_sz = handle->spec.channels*handle->bits_sample/8;
#if USE_PID
      handle->fill.aim = FILL_FACTOR*handle->period_frames*frame_sz/handle->spec.rate;
      handle->latency = (float)handle->fill.aim/(float)frame_sz;
#else
      handle->latency = (float)handle->period_frames/(float)handle->spec.rate;
#endif

      if (!m) {
         handle->mutex = _aaxMutexCreate(handle->mutex);
      }

#if 0
      handle->min_tracks = 1;
      handle->max_tracks = _AAX_MAX_SPEAKERS;
      handle->min_frequency = _AAX_MIN_MIXER_FREQUENCY;
      handle->max_frequency = _AAX_MAX_MIXER_FREQUENCY;
#endif

      _aaxContextConnect(handle);
      if (!handle->ctx)
      {
         ppa_threaded_mainloop_free(handle->ml);
         free(handle);
         handle = NULL;
      }
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

   if (handle)
   {
      pa_stream_flags_t flags;
      int error;

      flags = PA_STREAM_FIX_FORMAT | PA_STREAM_FIX_RATE |
              PA_STREAM_FIX_CHANNELS | PA_STREAM_DONT_MOVE |
              PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_START_CORKED;

      _aaxStreamConnect(handle, flags, &error);
      if (!handle->pa || error != PA_STREAM_READY) {
         _aaxPulseAudioDriverLogVar(id, "connect: %s", ppa_strerror(error));
      }
   }

   return (void *)handle;
}

static int
_aaxPulseAudioDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

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

      return AAX_TRUE;
   }
   return AAX_FALSE;
}

static int
_aaxPulseAudioDriverSetup(const void *id, float *refresh_rate, int *fmt,
                   unsigned int *tracks, float *speed, UNUSED(int *bitrate),
                   int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int period_frames;
   pa_sample_spec req;
   int rv = AAX_FALSE;

   *fmt = AAX_PCM16S;

   memcpy(&req, &handle->spec, sizeof(pa_sample_spec));

   req.rate = (unsigned int)*speed;
   req.channels = *tracks;
   if (req.channels > handle->spec.channels) {
      req.channels = handle->spec.channels;
   }

   if (*refresh_rate > 100) {
      *refresh_rate = 100;
   }

   if (!registered) {
      period_frames = get_pow2((size_t)rintf(req.rate/(*refresh_rate)));
   } else {
      period_frames = get_pow2((size_t)rintf(req.rate/period_rate));
   }
   handle->period_frames = period_frames;

   if (ppa_sample_spec_valid(&req))
   {
      int frame_sz;

      memcpy(&handle->spec, &req, sizeof(pa_sample_spec));

      frame_sz = handle->spec.channels*handle->bits_sample/8;
#if 0
 printf("spec:\n");
 printf("   frequency: %i\n", handle->spec.rate);
 printf("   format:    %x\n", handle->format);
 printf("   channels:  %i\n", handle->spec.channels);
 printf("   samples:   %i\n", handle->period_frames);
#endif

      *speed = handle->spec.rate;
      *tracks = handle->spec.channels;
      if (!registered) {
         *refresh_rate = handle->spec.rate/(float)handle->period_frames;
      } else {
         *refresh_rate = period_rate;
      }
      handle->refresh_rate = *refresh_rate;

#if USE_PID
      handle->fill.aim = FILL_FACTOR*handle->period_frames*frame_sz/handle->spec.rate;
      handle->latency = (float)handle->fill.aim/(float)frame_sz;
#else
      handle->latency = (float)handle->period_frames/(float)handle->spec.rate;
#endif

      handle->render = _aaxSoftwareInitRenderer(handle->latency,
                                                handle->mode, registered);
      if (handle->render)
      {
         const char *rstr = handle->render->info(handle->render->id);

         snprintf(_pulseaudio_id_str, MAX_ID_STRLEN ,"%s %s %s",
                  DEFAULT_RENDERER, ppa_get_library_version(), rstr);
         rv = AAX_TRUE;
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
      snprintf(str,255, "  buffer size: %i bytes", handle->spec.channels*handle->period_frames*handle->bits_sample/8);
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
 printf("  buffer size: %u bytes\n", handle->spec.channels*handle->period_frames*handle->bits_sample/8);
 printf("  latency:  %5.2f ms\n", 1e3f*handle->latency);
 printf("  timer based: yes\n");
 printf("  channels: %i, bytes/sample: %i\n", handle->spec.channels, handle->bits_sample/8);
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

   size = period_frames*frame_sz;
   if (handle->dataBuffer == 0 || (handle->dataBuffer->size < 8*size))
   {
      _aaxDataDestroy(handle->dataBuffer);
      handle->dataBuffer = _aaxDataCreate(2*FILL_FACTOR*size, no_tracks*handle->bits_sample/8);
      if (handle->dataBuffer == 0) return -1;

      ppa_stream_set_write_callback(handle->pa, stream_write_cb, handle);
   }

   offs = rb->get_parami(rb, RB_OFFSET_SAMPLES);
   period_frames -= offs;
   size = period_frames*frame_sz;

   free = handle->dataBuffer->size - handle->dataBuffer->avail;
   if (free > size)
   {
      unsigned char *data;

//    _pulseaudio_set_volume(handle, rb, offs, period_frames, no_tracks, gain);

      _aaxMutexLock(handle->mutex);
      data = handle->dataBuffer->data + handle->dataBuffer->avail;
      sbuf = (const int32_t**)rb->get_tracks_ptr(rb, RB_READ);
      _batch_cvt16_intl_24(data, sbuf, offs, no_tracks, period_frames);
      rb->release_tracks_ptr(rb);

      handle->dataBuffer->avail += size;
      _aaxMutexUnLock(handle->mutex);

      assert(handle->dataBuffer->avail <= handle->dataBuffer->size);

      rv = period_frames;
   }

   return rv;
}

static char *
_aaxPulseAudioDriverGetName(const void *id, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   /* TODO: distinguish between playback and record */
   if (handle && handle->devname && (mode < AAX_MODE_WRITE_MAX))
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
   int rv = AAX_FALSE;

   switch(state)
   {
   case DRIVER_PAUSE:
      if (handle)
      {
         if (!ppa_stream_is_corked(handle->pa)) {
            ppa_stream_cork(handle->pa, 1, NULL, NULL);
         }
         rv = AAX_TRUE;
      }
      break;
   case DRIVER_RESUME:
      if (handle)
      {
         if (ppa_stream_is_corked(handle->pa)) {
            ppa_stream_cork(handle->pa, 0, NULL, NULL);
         }
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
 //      rv = _aaxGetLatency(handle);
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
_aaxPulseAudioDriverGetDevices(const void *id, int mode)
{
   int m = (mode == AAX_MODE_READ) ? 0 : 1;
   _driver_t *handle = (_driver_t *)id;

   if (!id)
   {
      handle = calloc(1, sizeof(_driver_t));
      _aaxContextConnect(handle);
   }

   if (handle->ctx)
   {
      pa_operation *opr;
      _sink_info_t si;

      si.handle = handle;
      si.loop = handle->ml;

      if (mode == AAX_MODE_READ) {
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

      if (!id) {
         _aaxPulseAudioDriverDisconnect(handle);
      }
   }

   return handle->descriptions[m];
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

/* ----------------------------------------------------------------------- */

static const char*
detect_name(_driver_t *handle)
{
   int m = (handle->mode == AAX_MODE_READ) ? 1 : 0;
   const char *ptr = handle->descriptions[m];
   const char *rv = NULL;
   int di = 0, ni = 0;

   while (strlen(ptr) && strcmp(handle->driver, ptr)) {
      ptr += strlen(ptr)+1;
   }

   if (strlen(ptr) > 0)
   {
      ptr = handle->names[m];
      for(ni=0; ni<di; ++ni) {
         ptr += strlen(ptr)+1;
      }
      rv = ptr;
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

static float
_aaxGetLatency(_driver_t *handle)
{
   int negative = 0;
   pa_usec_t rv;

   ppa_threaded_mainloop_lock(handle->ml);

   if (ppa_stream_get_latency(handle->pa, &rv, &negative) >= 0) {
      if (negative) rv = 0;
   }

   ppa_threaded_mainloop_unlock(handle->ml);

   return (float)rv*1e-6f;
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
stream_write_cb(pa_stream *stream, size_t len, void *be_ptr)
{
   _driver_t *be_handle = (_driver_t *)be_ptr;
   _handle_t *handle = (_handle_t *)be_handle->handle;
   void *id = be_handle;

   if (_IS_PLAYING(handle))
   {
      assert(be_handle->mode != AAX_MODE_READ);
      assert(be_handle->dataBuffer);

      _aaxMutexLock(be_handle->mutex);

      // assert(be_handle->dataBuffer->avail >= len);
      if (be_handle->dataBuffer->avail < len) {
         len = be_handle->dataBuffer->avail;
      }

      {
         int res = -1;

         res = ppa_stream_write(be_handle->pa, be_handle->dataBuffer->data,len,
                                NULL, 0LL, PA_SEEK_RELATIVE);
         if (res >= 0) {
            _aaxDataMove(be_handle->dataBuffer, NULL, len);
         } else if (ppa_strerror) {
            _AAX_DRVLOG(ppa_strerror(res));
         }
      }

      _aaxMutexUnLock(be_handle->mutex);
   }
// ppa_threaded_mainloop_signal(be_handle->ml, 0);
}

static void
stream_latency_update_cb(pa_stream *stream, void *id)
{
   _driver_t *handle = (_driver_t *)id;
   ppa_threaded_mainloop_signal(handle->ml, 0);
}

#if 0
static void
sink_info_cb(UNUSED(pa_context *context), const pa_sink_info *info, int eol, void *id)
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

static void
sink_device_cb(UNUSED(pa_context *context), const pa_sink_info *info, int eol, void *si)
{
   _sink_info_t *sink = si;
   _driver_t *handle = sink->handle;
   char *sptr, *ptr;
   size_t slen, len;
   int m = 1;

   if (eol)
   {
      ppa_threaded_mainloop_signal(sink->loop, 0);
      return;
   }

   sptr = ptr = (char*)&handle->descriptions[m];
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
//    printf("# '%s'\n", info->description);
      ptr += slen;
      *ptr++ = '\0';
      *ptr = '\0';
   }

   sptr = ptr = handle->names[m];
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

static void
source_device_cb(UNUSED(pa_context *context), const pa_source_info *info, int eol, void *si)
{
    _sink_info_t *sink = si;
   _driver_t *handle = sink->handle;
   char *sptr, *ptr;
   size_t slen, len;
   int m = 0;

   if (eol)
   {
      ppa_threaded_mainloop_signal(sink->loop, 0);
      return;
   }

   sptr = ptr = (char*)&handle->descriptions[m];
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

   sptr = ptr = handle->names[m];
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

#if 0
static void
source_device_cb(UNUSED(pa_context *context), const pa_source_info *info, int eol, void *id)
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
sink_name_cb(UNUSED(pa_context *context), const pa_sink_info *info, int eol, void *id)
{
   _driver_t *handle = (_driver_t*)id;

   if (eol)
   {
      ppa_threaded_mainloop_signal(handle->ml, 0);
      return;
   }

   free(handle->driver);
   handle->driver = strdup(info->description);
}
#endif

#if 0
static void
source_name_cb(UNUSED(pa_context *context), const pa_source_info *info, int eol, void *id)
{
   _driver_t *handle = (_driver_t*)id;

   if (eol)
   {
      ppa_threaded_mainloop_signal(handle->ml, 0);
      return;
   }

   free(handle->driver);
   handle->driver = strdup(info->description);
}
#endif

static void
_aaxContextConnect(_driver_t *handle)
{
   static char pulse_avail = AAX_TRUE;
   const char *name = AAX_LIBRARY_STR;
   char buf[PATH_MAX];

   if (!pulse_avail)
   {
      static time_t t_previous = 0;
      time_t t_now = time(NULL);
      if (t_now > (t_previous+1))
      {
         t_previous = t_now;
         pulse_avail = AAX_TRUE;
      }
   }

   assert(loop);

   if (ppa_get_binary_name && ppa_get_binary_name(buf, PATH_MAX)) {
      name = ppa_path_get_filename(buf);
   }

   handle->ml = ppa_threaded_mainloop_new();
   if (handle->ml)
   {
      handle->ctx = ppa_context_new(ppa_threaded_mainloop_get_api(handle->ml), name);
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
            pulse_avail = AAX_FALSE;
            ppa_context_unref(handle->ctx);
         }
      }
      else
      {
         ppa_threaded_mainloop_free(handle->ml);
         handle->ml = NULL;
      }
   }
}


static void
_aaxStreamConnect(_driver_t *handle, pa_stream_flags_t flags, int *error)
{
   const char *agent = aaxGetVersionString((aaxConfig)handle);
   pa_channel_map map;

   *error = PA_STREAM_READY;

   ppa_channel_map_init_auto(&map, handle->spec.channels, PA_CHANNEL_MAP_WAVEEX);
   handle->pa = ppa_stream_new(handle->ctx, agent, &handle->spec, &map);
   if (handle->pa)
   {
      int mode = (handle->mode = AAX_MODE_READ) ? 1 : 0;
      if (mode) // write
      {
         pa_stream * pa = handle->pa;
         pa_buffer_attr attr;
         unsigned int buflen;
         const char *name;
         int res;

         ppa_stream_set_state_callback(pa, stream_state_cb, handle);
//       ppa_stream_set_read_callback(pa, stream_record_cb, handle);
//       ppa_stream_set_write_callback(pa, stream_write_cb, handle);
         ppa_stream_set_latency_update_callback(pa, stream_latency_update_cb, handle);

         buflen = handle->period_frames*handle->spec.channels*handle->bits_sample/8;

         attr.fragsize = (uint32_t)-1;
         attr.maxlength = (uint32_t)-1; // buflen*handle->no_periods;
         attr.minreq = (uint32_t)-1; // buflen;
         attr.prebuf = 0; // buflen;
         attr.tlength = 2*buflen;
         flags |= PA_STREAM_ADJUST_LATENCY;

         name = detect_name(handle);
         if (mode == AAX_MODE_READ) {
            res = ppa_stream_connect_record(pa, name, NULL, flags);
         } else {
            res = ppa_stream_connect_playback(pa, name, &attr, flags, NULL, NULL);
         }

         if (res >= 0)
         {
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
         }
         else {
            *error = ppa_context_errno(handle->ctx);
         }
      }
   }
}

#endif /* HAVE_PULSE_PULSEAUDIO_H */
