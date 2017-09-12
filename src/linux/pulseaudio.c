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

#include <aax/aax.h>
#include <xml.h>

#include <base/types.h>
#include <base/logging.h>
#include <base/dlsym.h>

#include <arch.h>
#include <api.h>
#include <driver.h>
#include <devices.h>
#include <ringbuffer.h>

#include <software/renderer.h>
#include "audio.h"
#include "pulseaudio.h"

#define DEFAULT_RENDERER	"PulseAudio"
#define DEFAULT_DEVNAME		"default"
#define MAX_ID_STRLEN		64
#define NO_FRAGMENTS		2

#define _AAX_DRVLOG(a)         _aaxPulseAudioDriverLog(id, 0, 0, a)
#define HW_VOLUME_SUPPORT(a)	((a->mixfd >= 0) && a->volumeMax)

static _aaxDriverDetect _aaxPulseAudioDriverDetect;
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
   void *ctx;
   void *str;
   void *ml;

   char *name;
   void *handle;

   _aaxRenderer *render;

   int mode;
   unsigned int format;
   size_t buffer_size;
   pa_sample_spec pa_spec;

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

DECL_FUNCTION(pa_get_binary_name);
DECL_FUNCTION(pa_path_get_filename);
DECL_FUNCTION(pa_get_library_version);
DECL_FUNCTION(pa_strerror);
DECL_FUNCTION(pa_xfree);
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
DECL_FUNCTION(pa_context_unref);
DECL_FUNCTION(pa_context_errno);
DECL_FUNCTION(pa_stream_new);
DECL_FUNCTION(pa_stream_set_state_callback);
DECL_FUNCTION(pa_stream_connect_playback);
DECL_FUNCTION(pa_stream_disconnect);
DECL_FUNCTION(pa_stream_unref);
DECL_FUNCTION(pa_stream_begin_write);
DECL_FUNCTION(pa_stream_write);
DECL_FUNCTION(pa_stream_get_state);
DECL_FUNCTION(pa_signal_new);
DECL_FUNCTION(pa_signal_done);

static void* _aaxContextConnect(void*);

static const char *_const_pulseaudio_default_name = DEFAULT_DEVNAME;

static int
_aaxPulseAudioDriverDetect(int mode)
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

      TIE_FUNCTION(pa_stream_new);
      if (ppa_stream_new)
      {
         TIE_FUNCTION(pa_xfree);
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
         TIE_FUNCTION(pa_context_unref);
         TIE_FUNCTION(pa_context_errno);
         TIE_FUNCTION(pa_stream_new);
         TIE_FUNCTION(pa_stream_set_state_callback);
         TIE_FUNCTION(pa_stream_connect_playback);
         TIE_FUNCTION(pa_stream_disconnect);
         TIE_FUNCTION(pa_stream_unref);
         TIE_FUNCTION(pa_stream_write);
         TIE_FUNCTION(pa_stream_get_state);
         TIE_FUNCTION(pa_signal_new);
         TIE_FUNCTION(pa_signal_done);
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
            handle.ctx = _aaxContextConnect(&handle);
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
      handle->pa_spec.format = PA_SAMPLE_S16LE;
      handle->pa_spec.rate = 44100;
      handle->pa_spec.channels = 2;
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
            handle->pa_spec.rate = f;
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
               handle->pa_spec.channels = i;
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
 printf("frequency-hz: %f\n", handle->pa_spec.rate);
 printf("channels: %i\n", handle->pa_spec.channels);
 printf("device number: %i\n", handle->nodenum);
#endif
   }

   if (handle)
   {
      handle->handle = config;
      snprintf(_pulseaudio_id_str, MAX_ID_STRLEN ,"%s", DEFAULT_RENDERER);

      handle->ml = ppa_threaded_mainloop_new();
      if (handle->ml && ppa_threaded_mainloop_start(handle->ml) >= 0)
      {
         ppa_threaded_mainloop_lock(handle->ml);
         handle->ctx = _aaxContextConnect(handle);
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
   unsigned int rate;
   int rv = AAX_FALSE;

   assert(handle);

   handle->pa_spec.rate = rate = *speed;
   if (!registered) {
      period_frames = rate/(*refresh_rate);
   } else {
      period_frames = get_pow2((size_t)rintf((rate*NO_FRAGMENTS)/period_rate));
      period_frames = rate/period_rate;
   }

   if (handle->pa_spec.channels > *tracks) {
      handle->pa_spec.channels = *tracks;
   }
   *tracks = handle->pa_spec.channels;

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
   *speed = (float)handle->pa_spec.rate;
   *tracks = handle->pa_spec.channels;

   period_rate = (float)rate/period_frames;
   *refresh_rate = period_rate;

   handle->str = ppa_stream_new(handle->ctx, AAX_LIBRARY_STR,
                                &handle->pa_spec, NULL);
   if (handle->str)
   {
      const char *dev = NULL; // use the default device (for now)
      if (handle->mode)
      {
         pa_stream_state_t state;

         ppa_stream_connect_playback(handle->str, dev, NULL, 0, NULL, NULL);
         while ((state = ppa_stream_get_state(handle->str)) != PA_STREAM_READY)
         {
            if (state != PA_STREAM_CREATING)
            {
               if (ppa_strerror) {
                  _AAX_DRVLOG(ppa_strerror(ppa_context_errno(handle->ctx)));
               }
               break;
            }
            ppa_threaded_mainloop_wait(handle->ml);
         }

         if (state == PA_STREAM_READY)
         {
            handle->render = _aaxSoftwareInitRenderer(handle->latency, handle->mode, registered);
            if (handle->render)
            {
               const char *rstr = handle->render->info(handle->render->id);
               snprintf(_pulseaudio_id_str, MAX_ID_STRLEN ,"%s %s %s", DEFAULT_RENDERER, rstr, ppa_get_library_version());
               rv = AAX_TRUE;
            } 
         }
      }
   }

#if 0
 printf("Latency: %3.1f ms\n", handle->latency*1e3f);
 printf("Refresh rate: %f\n", *refresh_rate);
 printf("Sample rate: %i (reuqested: %.1f)\n", handle->pa_spec.rate, *speed);
 printf("No. channels: %i (reuqested: %i)\n", handle->pa_spec.channels, *tracks);
#endif

   return rv;
}


static ssize_t
_aaxPulseAudioDriverCapture(const void *id, void **data, ssize_t *offset, size_t *frames, void *scratch, size_t scratchlen, float gain, UNUSED(char batched))
{
   return AAX_FALSE;
}

static size_t
_aaxPulseAudioDriverPlayback(const void *id, void *src, UNUSED(float pitch), float gain, UNUSED(char batched))
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
#if 0
         close(handle->fd);
         handle->fd = -1;
#endif
         rv = AAX_TRUE;
      }
      break;
   case DRIVER_RESUME:
      if (handle) 
      {
#if 0
         handle->fd = open(handle->devnode, handle->mode|handle->exclusive);
         if (handle->fd)
         {
            int err, frag, fd = handle->fd;
            unsigned int param;

            if (handle->oss_version >= PulseAudio_VERSION_4)
            {
               int enable = 0;
               err = pioctl(fd, SNDCTL_DSP_COOKEDMODE, &enable);
            }

            frag = log2i(handle->buffer_size);
            frag |= NO_FRAGMENTS << 16;
            pioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag);

            param = handle->format;
            err = pioctl(fd, SNDCTL_DSP_SETFMT, &param);
            if (err >= 0)
            {
               param = handle->pa_spec.channels;
               err = pioctl(fd, SNDCTL_DSP_CHANNELS, &param);
            }
            if (err >= 0)
            {
               param = (unsigned int)handle->pa_spec.rate;
               err = pioctl(fd, SNDCTL_DSP_SPEED, &param);
            }
            if (err >= 0) {
               rv = AAX_TRUE;
            }
         }
#endif
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
_aaxPulseAudioDriverGetDevices(const void *id, int mode)
{
   static char names[2][1024] = { "default\0\0", "default\0\0" };
   return (char *)&names[mode];
}

static char *
_aaxPulseAudioDriverGetInterfaces(const void *id, const char *devname, int mode)
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

static void context_state_callback(void *context, void *id)
{
   _driver_t *handle = (_driver_t*)id;
   int state = ppa_context_get_state(context);
   if(state == PA_CONTEXT_READY ||
      (state != PA_CONTEXT_CONNECTING &&
       state != PA_CONTEXT_AUTHORIZING &&
       state != PA_CONTEXT_SETTING_NAME))
   {
     ppa_threaded_mainloop_signal(handle->ml, 0);
   }
}

static void*
_aaxContextConnect(void *id)
{
   _driver_t *handle = (_driver_t*)id;
   const char *name = AAX_LIBRARY_STR;
   char buf[PATH_MAX];
   void *rv = NULL;

   assert(id);
   assert(handle->ml);
   assert(!handle->ctx);

   if (ppa_get_binary_name && ppa_get_binary_name(buf, PATH_MAX)) {
      name = ppa_path_get_filename(buf);
   }

   rv = ppa_context_new(ppa_threaded_mainloop_get_api(handle->ml), name);
   if (rv)
   {
      char *srv = NULL; // connect to the default server (for now)
      ppa_context_set_state_callback(rv, context_state_callback, handle);
      if (ppa_context_connect(rv, srv, 0, NULL) >= 0)
      {
         int state;
         while ((state = ppa_context_get_state(rv)) != PA_CONTEXT_READY)
         {
            if(state != PA_CONTEXT_CONNECTING &&
               state != PA_CONTEXT_AUTHORIZING &&
               state != PA_CONTEXT_SETTING_NAME)
            {
               if (ppa_strerror) {
                  _AAX_DRVLOG(ppa_strerror(ppa_context_errno(rv)));
               }
               break;
            }
            ppa_threaded_mainloop_wait(handle->ml);
         }
         ppa_context_set_state_callback(rv, NULL, NULL);
      }
      else {
         ppa_context_unref(rv);
         if (ppa_strerror) {
            _AAX_DRVLOG(ppa_strerror(ppa_context_errno(rv)));
         }
      }
   }
   return rv;
}

