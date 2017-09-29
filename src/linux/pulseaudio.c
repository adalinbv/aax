/*
 * Copyright 2017 by Erik Hofman.
 * Copyright 2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
#include <base/dlsym.h>
#include <base/timer.h>

#include <arch.h>
#include <api.h>
#include <driver.h>
#include <devices.h>
#include <ringbuffer.h>

#include <software/renderer.h>
#include "audio.h"

#define DEFAULT_RENDERER	"PulseAudio"
#define DEFAULT_DEVNAME		"default"
#define MAX_DEVICES_LIST	4096
#define MAX_ID_STRLEN		64
#define NO_FRAGMENTS		2

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

   (_aaxDriverState *)&_aaxPulseAudioDriverState,
   (_aaxDriverParam *)&_aaxPulseAudioDriverParam,
   (_aaxDriverLog *)&_aaxPulseAudioDriverLog
};

typedef struct
{
   pa_stream *str;
   pa_context *ctx;
   pa_threaded_mainloop *ml;
   pa_channel_map *router;
   pa_proplist *props;

   char *name;
   void *handle;

   _aaxRenderer *render;

   int mode;
   unsigned int format;
   size_t buffer_size;

   pa_sample_spec spec;
   pa_buffer_attr attr;

   /* capabilities */
   unsigned int min_frequency;
   unsigned int max_frequency;
   unsigned int min_tracks;
   unsigned int max_tracks;
   float latency;

   int16_t *ptr, *scratch;
   size_t scratch_size;
#ifndef NDEBUG
   size_t buf_len;
#endif

} _driver_t;

typedef struct
{
   char *devices;
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
DECL_FUNCTION(pa_stream_new_with_proplist);
DECL_FUNCTION(pa_stream_set_state_callback);
DECL_FUNCTION(pa_stream_connect_playback);
DECL_FUNCTION(pa_stream_connect_record);
DECL_FUNCTION(pa_stream_disconnect);
DECL_FUNCTION(pa_stream_unref);
DECL_FUNCTION(pa_stream_begin_write);
DECL_FUNCTION(pa_stream_write);
DECL_FUNCTION(pa_stream_get_state);
DECL_FUNCTION(pa_stream_get_device_name);
DECL_FUNCTION(pa_stream_cork);
DECL_FUNCTION(pa_stream_is_corked);
DECL_FUNCTION(pa_operation_get_state);
DECL_FUNCTION(pa_operation_unref);

// DECL_FUNCTION(pa_signal_new);
// DECL_FUNCTION(pa_signal_done);
// DECL_FUNCTION(pa_xfree);

#define MAKE_FUNC(a) DECL_FUNCTION(a)
// MAKE_FUNC(pa_sample_spec_valid);
// MAKE_FUNC(pa_frame_size);
// MAKE_FUNC(pa_stream_drop);
// MAKE_FUNC(pa_stream_peek);
// MAKE_FUNC(pa_threaded_mainloop_in_thread);
// MAKE_FUNC(pa_stream_readable_size);
// MAKE_FUNC(pa_stream_writable_size);
// MAKE_FUNC(pa_stream_is_suspended);
// MAKE_FUNC(pa_xmalloc);
// MAKE_FUNC(pa_threaded_mainloop_accept);
// MAKE_FUNC(pa_stream_set_write_callback);
// MAKE_FUNC(pa_stream_set_buffer_attr);
// MAKE_FUNC(pa_stream_get_buffer_attr);
// MAKE_FUNC(pa_stream_get_sample_spec);
// MAKE_FUNC(pa_stream_get_time);
// MAKE_FUNC(pa_stream_set_read_callback);
// MAKE_FUNC(pa_stream_set_moved_callback);
// MAKE_FUNC(pa_stream_set_underflow_callback);
// MAKE_FUNC(pa_channel_map_init_auto);
// MAKE_FUNC(pa_channel_map_parse);
// MAKE_FUNC(pa_channel_map_snprint);
// MAKE_FUNC(pa_channel_map_equal);
// MAKE_FUNC(pa_context_get_server_info);
// MAKE_FUNC(pa_context_get_source_info_by_name);
// MAKE_FUNC(pa_context_get_source_info_list);
// MAKE_FUNC(pa_proplist_new);
// MAKE_FUNC(pa_proplist_free);
// MAKE_FUNC(pa_proplist_set);
// MAKE_FUNC(pa_sample_spec_valid);
// MAKE_FUNC(pa_frame_size);
// MAKE_FUNC(pa_stream_drop);
// MAKE_FUNC(pa_stream_peek);
// MAKE_FUNC(pa_threaded_mainloop_in_thread);

static void stream_state_callback(pa_stream*, void*);
static void sink_device_callback(pa_context*, const pa_sink_info*, int, void*);
static void source_device_callback(pa_context*, const pa_source_info*, int, void*);
static pa_stream* _aaxStreamConnect(const char*,  pa_context*, pa_threaded_mainloop*, pa_sample_spec*, pa_stream_flags_t flags, pa_channel_map*, pa_proplist*, int);
static pa_context* _aaxContextConnect(pa_threaded_mainloop*);

static const char *_const_pulseaudio_default_name = DEFAULT_DEVNAME;

int
_aaxPulseAudioDriverDetect(UNUSED(int mode))
{
   static void *audio = NULL;
   static int rv = AAX_FALSE;
   char *error = NULL;

   _AAX_LOG(LOG_DEBUG, __func__);
     
   if (TEST_FOR_FALSE(rv)) {
      audio = _aaxIsLibraryPresent("pulse", "0");
   }
   if (audio)
   {
      _aaxGetSymError(0);

      TIE_FUNCTION(pa_stream_new_with_proplist);
      if (ppa_stream_new_with_proplist)
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
         TIE_FUNCTION(pa_stream_get_state);
         TIE_FUNCTION(pa_stream_get_device_name);
         TIE_FUNCTION(pa_stream_cork);
         TIE_FUNCTION(pa_stream_is_corked);
         TIE_FUNCTION(pa_operation_get_state);
         TIE_FUNCTION(pa_operation_unref);
//       TIE_FUNCTION(pa_signal_new);
//       TIE_FUNCTION(pa_signal_done);
//       TIE_FUNCTION(pa_xfree);
      }

      error = _aaxGetSymError(0);
      if (!error)
      {
         _driver_t handle;
         handle.ctx = NULL;
         handle.ml = ppa_threaded_mainloop_new();
         if (handle.ml && ppa_threaded_mainloop_start(handle.ml) >= 0)
         {  
            ppa_threaded_mainloop_lock(handle.ml);
            handle.ctx = _aaxContextConnect(handle.ml);
            if (handle.ctx)
            {
               ppa_context_set_state_callback(handle.ctx, NULL, NULL);
               ppa_context_unref(handle.ctx);

               /* useful but not required */
               TIE_FUNCTION(pa_strerror);
               TIE_FUNCTION(pa_get_binary_name);
               TIE_FUNCTION(pa_path_get_filename);
               TIE_FUNCTION(pa_get_library_version);
               TIE_FUNCTION(pa_stream_begin_write);
               rv = AAX_TRUE;
            }
            ppa_threaded_mainloop_unlock(handle.ml);
            ppa_threaded_mainloop_stop(handle.ml);
            ppa_threaded_mainloop_free(handle.ml);
         }
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
      handle->name = (char*)_const_pulseaudio_default_name;
      handle->spec.format = PA_SAMPLE_S16LE;
      handle->spec.rate = 44100;
      handle->spec.channels = 2;
      handle->mode = mode;

      handle->min_tracks = 1;
      handle->max_tracks = _AAX_MAX_SPEAKERS;
      handle->min_frequency = _AAX_MIN_MIXER_FREQUENCY;
      handle->max_frequency = _AAX_MAX_MIXER_FREQUENCY;
      handle->latency = 0;
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
                  _AAX_SYSLOG("PulseAudio: no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_SYSLOG("PulseAudio: no. tracks too great.");
                  i = _AAX_MAX_SPEAKERS;
               }
               handle->spec.channels = i;
            }
         }
#if 0
         i = xmlNodeGetInt(xid, "bits-per-sample");
         if (i)
         {
            if (i != 16)
            {
               _AAX_SYSLOG("PulseAudio: unsopported bits-per-sample");
               i = 16;
            }
         }
#endif
      }

      if (renderer) {
         if (handle->name != _const_pulseaudio_default_name) {
            free(handle->name);
         }
         handle->name = _aax_strdup(renderer);
      }
#if 0
 printf("frequency-hz: %f\n", handle->spec.rate);
 printf("channels: %i\n", handle->spec.channels);
 printf("device number: %i\n", handle->nodenum);
#endif
   }

   if (handle)
   {
      handle->handle = config;
      snprintf(_pulseaudio_id_str, MAX_ID_STRLEN ,"%s", DEFAULT_RENDERER);

      handle->ml = ppa_threaded_mainloop_new();
      ppa_threaded_mainloop_lock(handle->ml);
      if (handle->ml && ppa_threaded_mainloop_start(handle->ml) >= 0)
      {
         handle->ctx = _aaxContextConnect(handle->ml);
         if (handle->ctx)
         {
            ppa_threaded_mainloop_unlock(handle->ml);
         }
         else
         {
            _aaxPulseAudioDriverDisconnect(handle);
            handle = NULL;
            _AAX_DRVLOG("unable to create a context");
         }
      }
      else {
         _AAX_DRVLOG("unable to create the main loop");
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
      if (handle->name != _const_pulseaudio_default_name) {
         free(handle->name);
      }

      if (handle->render)
      {
         handle->render->close(handle->render->id);
         free(handle->render);
      }

      if (handle->ml) {
         ppa_threaded_mainloop_lock(handle->ml);
      }

      if (handle->str)
      {
         ppa_stream_disconnect(handle->str);
         ppa_stream_unref(handle->str);
      }

      if (handle->ctx)
      {
         ppa_context_disconnect(handle->ctx);
         ppa_context_unref(handle->ctx);
      }

      if (handle->ml)
      {
         ppa_threaded_mainloop_unlock(handle->ml);
         ppa_threaded_mainloop_stop(handle->ml);
         ppa_threaded_mainloop_free(handle->ml);
      }

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
   ssize_t period_frames = 1024;
   pa_stream_flags_t flags;
   unsigned int rate;
   int rv = AAX_FALSE;

   assert(handle);

   handle->spec.rate = rate = *speed;
   if (!registered) {
      period_frames = get_pow2((size_t)rintf(rate/(*refresh_rate*NO_FRAGMENTS)));
   } else {
      period_frames = get_pow2((size_t)rintf((rate*NO_FRAGMENTS)/period_rate));
   }

   if (handle->spec.channels > *tracks) {
      handle->spec.channels = *tracks;
   }
   *tracks = handle->spec.channels;

   if (*tracks > _AAX_MAX_SPEAKERS)
   {
      char str[255];
      snprintf((char *)&str, 255, "PulseAudio: Unable to output to %i speakers"
                                  " in this setup (%i is the maximum)", *tracks,
                                  _AAX_MAX_SPEAKERS);
      _AAX_SYSLOG(str);
      return AAX_FALSE;
   }

   *fmt = AAX_PCM16S;
   *speed = (float)handle->spec.rate;
   *tracks = handle->spec.channels;

   period_rate = (float)rate/period_frames;
   *refresh_rate = period_rate;

   flags = PA_STREAM_START_CORKED | PA_STREAM_INTERPOLATE_TIMING |
           PA_STREAM_NOT_MONOTONIC | PA_STREAM_AUTO_TIMING_UPDATE |
           PA_STREAM_ADJUST_LATENCY | PA_CONTEXT_NOAUTOSPAWN;
   handle->str = _aaxStreamConnect(handle->name, handle->ctx, handle->ml,
                                   &handle->spec, flags, handle->router,
                                   handle->props, handle->mode);
   if (handle->str)
   {
      handle->render = _aaxSoftwareInitRenderer(handle->latency, handle->mode, registered);
      if (handle->render)
      {
         const char *rstr = handle->render->info(handle->render->id);
         snprintf(_pulseaudio_id_str, MAX_ID_STRLEN ,"%s %s %s", DEFAULT_RENDERER, ppa_get_library_version(), rstr);
         rv = AAX_TRUE;
      } 
   }
   else {
      _AAX_DRVLOG(ppa_strerror(ppa_context_errno(handle->ctx)));
   }

#if 0
 printf("Latency: %3.1f ms\n", handle->latency*1e3f);
 printf("Refresh rate: %f\n", *refresh_rate);
 printf("Sample rate: %i (reuqested: %.1f)\n", handle->spec.rate, *speed);
 printf("No. channels: %i (reuqested: %i)\n", handle->spec.channels, *tracks);
#endif

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
   _driver_t *handle = (_driver_t *)id;
   _aaxRingBuffer *rb = (_aaxRingBuffer *)src;
   unsigned int blocksize, no_tracks;
   ssize_t offs, no_samples;
   size_t outbuf_size;
   int res = 0;
   int32_t **sbuf;
   void *data;

   offs = rb->get_parami(rb, RB_OFFSET_SAMPLES);
   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES) - offs;
   blocksize = no_tracks*sizeof(int16_t);
   outbuf_size = no_samples*blocksize;

   if (ppa_stream_begin_write)
   {
      size_t size = outbuf_size;
      res = ppa_stream_begin_write(handle->str, &data, &size);
      if (res >= 0)
      {
         no_samples = size/blocksize;
         outbuf_size = size;
      }
   }
   else
   {
      if (handle->ptr == 0)
      {
         char *p = 0;
         handle->ptr = (int16_t *)_aax_malloc(&p, outbuf_size);
         handle->scratch = (int16_t*)p;
#ifndef NDEBUG
         handle->buf_len = outbuf_size;
#endif
      }
      assert(outbuf_size <= handle->buf_len);

      data = handle->scratch;
      assert(outbuf_size <= handle->buf_len);
   }


   sbuf = (int32_t**)rb->get_tracks_ptr(rb, RB_READ);
   _batch_cvt16_intl_24(data, (const int32_t**)sbuf, offs, no_tracks, no_samples);
   rb->release_tracks_ptr(rb);
   res = ppa_stream_write(handle->str, data, outbuf_size, NULL, 0, PA_SEEK_RELATIVE);

   if (res < 0 && ppa_strerror) {
      _AAX_DRVLOG(ppa_strerror(res));
   }
   return 0; // (info.bytes-outbuf_size)/(no_tracks*sizeof(int16_t));
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
         if (!ppa_stream_is_corked(handle->str)) {
            ppa_stream_cork(handle->str, 1, NULL, NULL);
         }
         rv = AAX_TRUE;
      }
      break;
   case DRIVER_RESUME:
      if (handle) 
      {
         if (ppa_stream_is_corked(handle->str)) {
            ppa_stream_cork(handle->str, 0, NULL, NULL);
         }
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
         rv = (float)NO_FRAGMENTS;
         break;
      case DRIVER_MAX_SAMPLES:
         rv = AAX_FPINFINITE;
         break;
      case DRIVER_SAMPLE_DELAY:
      {
//       uint64_t latency_us = ppa_simple_get_latency(handle->pa, NULL);
//       rv = latency_us*1e-6f;
         rv = 0;
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
   static char names[2][MAX_DEVICES_LIST] = { "\0\0", "\0\0" };
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
#if 1
         pa_stream_flags_t flags;
         pa_sample_spec spec;
         pa_stream *stream;

         flags = PA_STREAM_FIX_FORMAT | PA_STREAM_FIX_RATE |
                 PA_STREAM_FIX_CHANNELS | PA_STREAM_DONT_MOVE;

         spec.format = PA_SAMPLE_S16NE;
         spec.rate = 44100;
         spec.channels = (mode == AAX_MODE_READ) ? 1 : 2;
#endif

         si.devices = (char *)&names[mode];
         si.loop = loop;

#if 1
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

   return (char *)&names[mode];
}

static char *
_aaxPulseAudioDriverGetInterfaces(UNUSED(const void *id), UNUSED(const char *devname), UNUSED(int mode))
{
// _driver_t *handle = (_driver_t *)id;
   return NULL;
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

#endif /* HAVE_PULSE_PULSEAUDIO_H */
