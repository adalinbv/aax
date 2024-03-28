/*
 * SPDX-FileCopyrightText: Copyright © 2022-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2022-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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
#include <stdarg.h>		/* va_start */
#include <stdio.h>		/* snprintf */

#include <pipewire/pipewire.h>
#include <pipewire/extensions/metadata.h>
#include <pipewire/properties.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>

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
#include "pipewire.h"
#include "audio.h"


#define DEFAULT_RENDERER	"PipeWire"
#define DEFAULT_DEVNAME		"default"
#define MAX_ID_STRLEN		96

#define DEFAULT_PERIODS		1
#define DEFAULT_OUTPUT_RATE	48000
#define DEFAULT_REFRESH		25.0

#define USE_PIPEWIRE_THREAD	true
#define TIMING_DEBUG		false

#define BUFFER_SIZE_FACTOR	4.0f
#define CAPTURE_BUFFER_SIZE	(DEFAULT_PERIODS*8192)
#define MAX_DEVICES_LIST	4096

#define _AAX_DRVLOG(a)         _aaxPipeWireDriverLog(id, 0, 0, a)


_aaxDriverDetect _aaxPipeWireDriverDetect;
static _aaxDriverNewHandle _aaxPipeWireDriverNewHandle;
static _aaxDriverFreeHandle _aaxPipeWireDriverFreeHandle;
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
   (_aaxDriverFreeHandle *)&_aaxPipeWireDriverFreeHandle,
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
   struct pw_thread_loop *ml;

   struct spa_hook listener;
   struct spa_audio_info_raw spec;
   int stream_init_status;
   size_t buf_len;

   char bits_sample;
   unsigned int format;
   unsigned int period_frames;
   unsigned int no_periods;
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
      float aim; // in bytes
   } fill;

   float volumeCur, volumeInit;
   float volumeMin, volumeMax;
   float volumeStep, hwgain;

   _aaxTimer *callback_timer;
   size_t callback_avail;
   float callback_dt;

   struct _meta_t meta;

} _driver_t;

#undef DECL_FUNCTION
#define DECL_FUNCTION(f) static __typeof__(f) * p##f
DECL_FUNCTION(pw_init);
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
DECL_FUNCTION(pw_stream_set_active);
DECL_FUNCTION(pw_stream_get_state);
DECL_FUNCTION(pw_stream_dequeue_buffer);
DECL_FUNCTION(pw_stream_queue_buffer);
DECL_FUNCTION(pw_stream_get_control);
DECL_FUNCTION(pw_stream_set_control);
DECL_FUNCTION(pw_stream_update_properties);
DECL_FUNCTION(pw_properties_new);
DECL_FUNCTION(pw_properties_get);
DECL_FUNCTION(pw_properties_set);
DECL_FUNCTION(pw_properties_setf);
DECL_FUNCTION(pw_get_library_version);
DECL_FUNCTION(pw_get_application_name);
DECL_FUNCTION(pw_get_prgname);

static int _aaxPipeWireAudioStreamConnect(_driver_t*, enum pw_stream_flags, const char **error);
static float _pipewire_set_volume(_driver_t*, _aaxRingBuffer*, ssize_t, uint32_t, unsigned int, float);
static uint32_t _pipewire_get_id_by_name(_driver_t*, const char*);
static void _pipewire_detect_devices(char[2][MAX_DEVICES_LIST]);

static char* _aaxPipeWireDriverLogVar(const void *, const char *, ...);
static enum aaxFormat _aaxPipeWireGetFormat(unsigned int);

static int spa_audio_info_raw_valid(const struct spa_audio_info_raw*);
static void hotplug_loop_destroy();
static int hotplug_loop_init();

static char pipewire_initialized = false;
static const char *_const_pipewire_default_name = DEFAULT_DEVNAME;
const char *_const_pipewire_default_device = NULL;

static const enum spa_audio_channel channel_map[] = {
  SPA_AUDIO_CHANNEL_FL,
  SPA_AUDIO_CHANNEL_FR,
  SPA_AUDIO_CHANNEL_RL,
  SPA_AUDIO_CHANNEL_RR,
  SPA_AUDIO_CHANNEL_FC,
  SPA_AUDIO_CHANNEL_LFE,
  SPA_AUDIO_CHANNEL_SL,
  SPA_AUDIO_CHANNEL_SR
};

static void *audio = NULL;
static char pulseaudio = -1;

int
_aaxPipeWireDriverDetect(UNUSED(int mode))
{
   static int rv = false;
   char *error = NULL;
#if HAVE_PULSEAUDIO_H
   const char *env = getenv("AAX_SHOW_PIPEWIRE_DEVICES");
   if (env && _aax_getbool(env)) {
      pulseaudio = 0;
   }
#else
   pulseaudio = 0;
#endif

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
         TIE_FUNCTION(pw_stream_set_active);
         TIE_FUNCTION(pw_stream_get_state);
         TIE_FUNCTION(pw_stream_dequeue_buffer);
         TIE_FUNCTION(pw_stream_queue_buffer);
         TIE_FUNCTION(pw_stream_get_control);
         TIE_FUNCTION(pw_stream_set_control);
         TIE_FUNCTION(pw_stream_update_properties);
         TIE_FUNCTION(pw_properties_new);
         TIE_FUNCTION(pw_properties_get);
         TIE_FUNCTION(pw_properties_set);
         TIE_FUNCTION(pw_properties_setf);
      }

      error = _aaxGetSymError(0);
      if (!error)
      {
         /* useful but not required */
         TIE_FUNCTION(pw_get_library_version);
         TIE_FUNCTION(pw_get_application_name);
         TIE_FUNCTION(pw_get_prgname);
         rv = true;
      }
   }

   return rv;
}


// https://gitlab.freedesktop.org/pipewire/pipewire/-/wikis/FAQ#pipewire-buffering-explained
//
// Filters and real-time clients must use float-32
//
// The period_frames on the server is controlled by the clients node.latency property.
// It it always set to the lowest requested latency. If you start a PipeWire
// app with PIPEWIRE_LATENCY=128/48000 it will use a 2.6ms quantum and the
// latency between a client waking up and the sink read pointer will be 2.6ms.
static void *
_aaxPipeWireDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (handle)
   {
      int m = (mode == AAX_MODE_READ) ? 1 : 0;
      int frame_sz;

      handle->id = PW_ID_ANY;
      handle->driver = (char*)_const_pipewire_default_name;
      handle->mode = mode;

      handle->spec.channels = 2;
      handle->spec.rate = DEFAULT_OUTPUT_RATE;
      handle->spec.format = is_bigendian() ? SPA_AUDIO_FORMAT_S16_BE
                                           : SPA_AUDIO_FORMAT_S16_LE;
      memcpy(handle->spec.position, channel_map, sizeof(channel_map));

      handle->format = _aaxPipeWireGetFormat(handle->spec.format);
      handle->bits_sample = aaxGetBitsPerSample(handle->format);

      frame_sz = handle->spec.channels*handle->bits_sample/8;
      handle->period_frames = get_pow2(handle->spec.rate/DEFAULT_REFRESH);
      handle->fill.aim = (float)handle->period_frames/handle->spec.rate;
      handle->latency = handle->fill.aim/frame_sz;
      handle->no_periods = DEFAULT_PERIODS;

      if (!m) {
         handle->mutex = _aaxMutexCreate(handle->mutex);
      }

      handle->volumeInit = handle->volumeCur  = 1.0f;
      handle->volumeMin = 0.0f;
      handle->volumeMax = 1.0f;

      handle->min_tracks = 1;
      handle->max_tracks = _AAX_MAX_SPEAKERS;
      handle->min_frequency = _AAX_MIN_MIXER_FREQUENCY;
      handle->max_frequency = _AAX_MAX_MIXER_FREQUENCY;

      handle->callback_timer = _aaxTimerCreate();

      if (!pipewire_initialized)
      {
         int err;

         ppw_init(NULL, NULL);
         pipewire_initialized = true;

         err = hotplug_loop_init();
         if (err != 0)
         {
            _aaxPipeWireDriverLogVar(NULL, "PipeWire: hotplug loop error: %s",
                                     strerror(errno));
            _aaxPipeWireDriverDisconnect(handle);
            handle = NULL;
         }
      }
   }

   return handle;
}

static int
_aaxPipeWireDriverFreeHandle(UNUSED(void *id))
{
   _aaxCloseLibrary(audio);
   audio = NULL;

   return true;
}

static void *
_aaxPipeWireDriverConnect(void *config, const void *id, xmlId *xid, const char *renderer, enum aaxRenderMode mode)
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
               _AAX_SYSLOG("pipewire; frequency too small.");
               f = (float)_AAX_MIN_MIXER_FREQUENCY;
            }
            else if (f > (float)_AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_SYSLOG("pipewire; frequency too large.");
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
                  _AAX_SYSLOG("pipewire; no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_SYSLOG("pipewire; no. tracks too great.");
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
               _AAX_SYSLOG("pipewire; unsopported bits-per-sample");
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
            else if (i > 4)
            {
               _AAX_DRVLOG("no. periods too larget.");
               i = 4;
            }

            // pipewire doesn't work well with period numbers larger than 1
            handle->no_periods = 1; // i
         }
      }
   }
#if 0
 printf("\nrenderer: %s\n", handle->name);
 printf("frequency-hz: %f\n", handle->frequency_hz);
 printf("channels: %i\n", handle->no_tracks);
 printf("bits-per-sample: %i\n", handle->bits_sample);
 printf("periods: %i\n", handle->no_periods);
 printf("\n");
#endif

   return (void *)handle;
}

static int
_aaxPipeWireDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = false;

   if (handle)
   {
      if (handle->ml) {
         ppw_thread_loop_stop(handle->ml);
      }

      if (handle->pw)
      {
         ppw_stream_destroy(handle->pw);
         handle->pw = NULL;
      }

      if (handle->ml)
      {
         ppw_thread_loop_destroy(handle->ml);
         handle->ml = NULL;
      }

      _aax_free_meta(&handle->meta);

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

      _aaxTimerDestroy(handle->callback_timer);

      free(handle);

      rv = true;
   }

   if (pipewire_initialized)
   {
      pipewire_initialized = false;
      hotplug_loop_destroy();
   }

   return rv;
}

static int
_aaxPipeWireDriverSetup(const void *id, float *refresh_rate, int *fmt,
                   unsigned int *tracks, float *speed, UNUSED(int *bitrate),
                   int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int period_samples;
   struct spa_audio_info_raw req;
   int periods; // frame_sz;
   int rv = false;

   *fmt = AAX_PCM16S;

   req = handle->spec;
   req.rate = (unsigned int)*speed;
   req.channels = *tracks;
   if (req.channels > handle->spec.channels) {
      req.channels = handle->spec.channels;
   }

   if (*refresh_rate > 200) {
      *refresh_rate = 200;
   }

   periods = handle->no_periods;
   if (!registered) {
      period_samples = get_pow2((size_t)rintf(req.rate/(*refresh_rate*periods)));
   } else {
      period_samples = get_pow2((size_t)rintf((req.rate*periods)/period_rate));
   }
   req.format = is_bigendian() ? SPA_AUDIO_FORMAT_F32_BE
                               : SPA_AUDIO_FORMAT_F32_LE;
// frame_sz = req.channels*handle->bits_sample/8;

   if (spa_audio_info_raw_valid(&req))
   {
      enum pw_stream_flags flags;
      const char *error;

      handle->spec = req;
      handle->period_frames = period_samples;

      // NOTE: The PW_STREAM_FLAG_RT_PROCESS flag can be set to call the stream
      //       processing callback from the realtime thread. However, it comes
      //       with some caveats: no file IO, allocations, locking or other
      //       blocking operations must occur in the mixer callback.
      //
      // PW_STREAM_FLAG_EXCLUSIVE 		// require exclusive access
      flags = PW_STREAM_FLAG_AUTOCONNECT |	// try to automatically connect
              PW_STREAM_FLAG_MAP_BUFFERS;  	// mmap the buffers
      if (handle->mode != AAX_MODE_READ) {
//       flags |= PW_STREAM_FLAG_INACTIVE;	// start the stream inactive
      }

      _aaxPipeWireAudioStreamConnect(handle, flags, &error);
      if (!handle->pw || error) {
         _aaxPipeWireDriverLogVar(id, "connect: %s", error);
      }

      handle->format = _aaxPipeWireGetFormat(handle->spec.format);
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

      *fmt = handle->format;
      *speed = handle->spec.rate;
      *tracks = handle->spec.channels;
      if (!registered) {
         *refresh_rate = handle->spec.rate/(float)period_samples/periods;
      } else {
         *refresh_rate = period_rate;
      }

      handle->refresh_rate = *refresh_rate;
      handle->bits_sample = aaxGetBitsPerSample(handle->format);

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

         if (ppw_get_library_version) {
            snprintf(_pipewire_id_str, MAX_ID_STRLEN ,"%s %s %s",
                     DEFAULT_RENDERER, ppw_get_library_version(), rstr);
         } else {
            snprintf(_pipewire_id_str, MAX_ID_STRLEN ,"%s %s",
                     DEFAULT_RENDERER, rstr);
         }
         rv = true;
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
      snprintf(str,255, "  buffer size: %i bytes", handle->period_frames*handle->spec.channels*handle->bits_sample/8);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  latency: %3.2f ms", 1e3f*handle->latency);
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
 printf("  buffer size: %lu bytes\n", handle->buf_len);
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
      struct pw_buffer  *pw_buf;

      pw_buf = ppw_stream_dequeue_buffer(handle->pw);
      if (pw_buf)
      {
         struct spa_buffer *spa_buf = pw_buf->buffer;

         size = handle->no_periods*spa_buf->datas[0].chunk->size;
         ppw_stream_queue_buffer(handle->pw, pw_buf);
      }

      handle->dataBuffer = _aaxDataCreate(1, size, 1);
      if (handle->dataBuffer == 0) return false;

//    stream_set_read_callback
   }

   *frames = 0;
   tracks = handle->spec.channels;
   frame_sz = tracks*handle->bits_sample/8;

   len = _aaxDataGetDataAvail(handle->dataBuffer, 0);
   if (len > nframes*frame_sz) len = nframes*frame_sz;
   if (len)
   {
      void *buf = _aaxDataGetData(handle->dataBuffer, 0);

      nframes = len/frame_sz;
//    _batch_cvt24_16_intl((int32_t**)data, buf, offs, tracks, nframes);
      handle->cvt_from_intl((int32_t**)data, buf, offs, tracks, nframes);
      _aaxDataMove(handle->dataBuffer, 0, NULL, len);

      gain = _pipewire_set_volume(handle, NULL, offs, nframes, tracks, gain);
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
_aaxPipeWireDriverPlayback(const void *id, void *src, UNUSED(float pitch), UNUSED(float gain), UNUSED(char batched))
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

   size = BUFFER_SIZE_FACTOR*period_frames;
   if (handle->dataBuffer == 0 || (_aaxDataGetSize(handle->dataBuffer) < size*frame_sz))
   {
      _aaxDataDestroy(handle->dataBuffer);
      handle->dataBuffer = _aaxDataCreate(1, size, frame_sz);
      if (handle->dataBuffer == 0) return -1;

      // stream_set_write_callback
   }

   offs = rb->get_parami(rb, RB_OFFSET_SAMPLES);
   period_frames -= offs;
   size = period_frames*frame_sz;

   free = _aaxDataGetFreeSpace(handle->dataBuffer, 0);
   if (free > size)
   {
      unsigned char *data;

      _pipewire_set_volume(handle, rb, offs, period_frames, no_tracks, gain);

      // store audio data in handle->dataBuffer
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
_aaxPipeWireDriverSetName(const void *id, int type, const char *name)
{
   _driver_t *handle = (_driver_t *)id;
   int ret = false;
   if (handle)
   {
      struct spa_dict_item item[3];
      int nitems = 1;

      switch (type)
      {
      case AAX_MUSIC_PERFORMER_STRING:
      case AAX_MUSIC_PERFORMER_UPDATE:
         if (handle->meta.artist) free(handle->meta.artist);
         handle->meta.artist = strdup(name);
         item[0] = SPA_DICT_ITEM_INIT(PW_KEY_MEDIA_ARTIST, name);
         item[1] = SPA_DICT_ITEM_INIT(PW_KEY_NODE_NAME, name);
         nitems = 2;
         ret = true;
         break;
      case AAX_TRACK_TITLE_STRING:
      case AAX_TRACK_TITLE_UPDATE:
         if (handle->meta.title) free(handle->meta.title);
         handle->meta.title = strdup(name);
         item[0] = SPA_DICT_ITEM_INIT(PW_KEY_MEDIA_TITLE, name);
         item[1] = SPA_DICT_ITEM_INIT(PW_KEY_NODE_DESCRIPTION, name);
         nitems = 2;
         ret = true;
         break;
      case AAX_SONG_COPYRIGHT_STRING:
         if (handle->meta.copyright) free(handle->meta.copyright);
         handle->meta.copyright = strdup(name);
         item[0] = SPA_DICT_ITEM_INIT(PW_KEY_MEDIA_COPYRIGHT, name);
         ret = true;
         break;
      case AAX_SONG_COMMENT_STRING:
         if (handle->meta.comments) free(handle->meta.comments);
         handle->meta.comments = strdup(name);
         item[0] = SPA_DICT_ITEM_INIT(PW_KEY_MEDIA_COMMENT, name);
         ret = true;
         break;
      case AAX_RELEASE_DATE_STRING:
         if (handle->meta.date) free(handle->meta.date);
         handle->meta.date = strdup(name);
         item[0] = SPA_DICT_ITEM_INIT(PW_KEY_MEDIA_DATE, name);
         ret = true;
         break;
      case AAX_COVER_IMAGE_DATA:
      case AAX_MUSIC_GENRE_STRING:
      case AAX_TRACK_NUMBER_STRING:
      case AAX_ALBUM_NAME_STRING:
      case AAX_SONG_COMPOSER_STRING:
      case AAX_WEBSITE_STRING:
      default:
         break;
      }

      if (ret)
      {
         if (handle->pw)
         {
            ppw_thread_loop_lock(handle->ml);
            ppw_stream_update_properties(handle->pw, &SPA_DICT_INIT(item, nitems));
            ppw_thread_loop_unlock(handle->ml);
         }
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
   _driver_t *handle = (_driver_t *)id;
   int rv = false;

   switch(state)
   {
   case DRIVER_PAUSE:
      if (handle)
      {
         ppw_thread_loop_lock(handle->ml);
         ppw_stream_set_active(handle->pw, false);
         ppw_thread_loop_unlock(handle->ml);
         rv = true;
      }
      break;
   case DRIVER_RESUME:
      if (handle)
      {
         ppw_thread_loop_lock(handle->ml);
         ppw_stream_set_active(handle->pw, true);
         ppw_thread_loop_unlock(handle->ml);
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
         rv = 1.0f;
         break;
      case DRIVER_MAX_PERIODS:
         rv = 1.0f;
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
         rv = true;
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
   static time_t t_previous[2] = { 0, 0 };
   int m = (mode == AAX_MODE_READ) ? 0 : 1;
   char *rv = (char*)&names[m];
   time_t t_now;

   t_now = time(NULL);
   if (t_now > (t_previous[m]+5))
   {
      t_previous[m] = t_now;
      _pipewire_detect_devices(names);
   }

   return rv;
}

static char *
_aaxPipeWireDriverGetInterfaces(UNUSED(const void *id), UNUSED(const char *driver), UNUSED(int mode))
{
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

static float
_pipewire_set_volume(_driver_t *handle, _aaxRingBuffer *rb, ssize_t offset, uint32_t period_frames, unsigned int tracks, float volume)
{
   float gain = fabsf(volume);
   float hwgain;

   hwgain = _MINMAX(gain, handle->volumeMin, handle->volumeMax);

   /*
    * Slowly adjust volume to dampen volume slider movement.
    * If the volume step is large, don't dampen it.
    * volume is negative for auto-gain mode.
    */
   if ((volume < 0.0f) && (handle->mode == AAX_MODE_READ))
   {
      float dt = period_frames/handle->spec.rate;
      float rr = _MINMAX(dt/10.0f, 0.0f, 1.0f);      /* 10 sec average */

      /* Quickly adjust for a very large step in volume */
      if (fabsf(hwgain - handle->volumeCur) > 0.825f) rr = 0.9f;

      hwgain = (1.0f-rr)*handle->hwgain + (rr)*hwgain;
      handle->hwgain = hwgain;
   }

#if 0
   if (fabsf(hwgain - handle->volumeCur) >= handle->volumeStep)
   {
      if (ppw_stream_set_control &&
          !ppw_stream_set_control(handle->pw, SPA_PROP_volume, 1, &hwgain, 0))
      {
         handle->volumeCur = hwgain;
      }
   }
#endif

   if (hwgain) gain /= hwgain;
   else gain = 0.0f;

   /* software volume fallback */
   if (rb && fabsf(gain - 1.0f) > LEVEL_32DB) {
      rb->data_multiply(rb, offset, period_frames, gain);
   }

   return gain;
}

/* ----------------------------------------------------------------------- */
#define PW_POD_BUFFER_LENGTH		1024
#define PW_THREAD_NAME_BUFFER_LENGTH	128

static struct pw_core *hotplug_core = NULL;
static struct pw_thread_loop *hotplug_loop = NULL;
static struct pw_context *hotplug_context = NULL;
static struct pw_registry *hotplug_registry = NULL;
static struct spa_hook hotplug_registry_listener;
static struct spa_hook hotplug_core_listener;
static struct spa_list hotplug_pending_list;
static struct spa_list hotplug_io_list;
static int hotplug_init_seq_val = 0;
static char hotplug_init_complete = false;
static char hotplug_events_enabled = false;

static uint32_t pipewire_default_sink_id = SPA_ID_INVALID;
static uint32_t pipewire_default_source_id = SPA_ID_INVALID;

/* A generic PipeWire node object used for enumeration. */
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
   char is_lazy;

   struct {
      int32_t rate;
      uint8_t channels;
      unsigned int format;
   } spec;

   char name[];
};


static int
spa_sample_format_valid(unsigned format)
{
   int rv = false;
   switch(format)
   {
   case SPA_AUDIO_FORMAT_ALAW:
   case SPA_AUDIO_FORMAT_ULAW:
   case SPA_AUDIO_FORMAT_S8:
   case SPA_AUDIO_FORMAT_U8:
   case SPA_AUDIO_FORMAT_S16_LE:
   case SPA_AUDIO_FORMAT_S16_BE:
   case SPA_AUDIO_FORMAT_U16_LE:
   case SPA_AUDIO_FORMAT_U16_BE:
   case SPA_AUDIO_FORMAT_S24_32_LE:
   case SPA_AUDIO_FORMAT_S24_32_BE:
   case SPA_AUDIO_FORMAT_S32_LE:
   case SPA_AUDIO_FORMAT_S32_BE:
   case SPA_AUDIO_FORMAT_U32_LE:
   case SPA_AUDIO_FORMAT_U32_BE:
   case SPA_AUDIO_FORMAT_S24_LE:
   case SPA_AUDIO_FORMAT_S24_BE:
   case SPA_AUDIO_FORMAT_U24_LE:
   case SPA_AUDIO_FORMAT_U24_BE:
   case SPA_AUDIO_FORMAT_F32_LE:
   case SPA_AUDIO_FORMAT_F32_BE:
   case SPA_AUDIO_FORMAT_F64_LE:
   case SPA_AUDIO_FORMAT_F64_BE:
      rv = true;
      break;
   default:
      break;
   }
   return rv;
}

static int
spa_sample_rate_valid(uint32_t rate)
{
  return rate > 0 && rate <= _AAX_MAX_MIXER_FREQUENCY;
}

static int
spa_channels_valid(uint8_t channels)
{
  return channels > 0 && channels <= _AAX_MAX_SPEAKERS;
}

static int
spa_audio_info_raw_valid(const struct spa_audio_info_raw *spec)
{
   assert(spec);

   if (!spa_sample_rate_valid(spec->rate) ||
       !spa_channels_valid(spec->channels) ||
       !spa_sample_format_valid(spec->format))
      return 0;

   return 1;
}


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

            return true;
         }
      }
   }

   return false;
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
      return true;
   }

   return false;
}

/* The active node list */
static char
io_list_check_add(struct io_node *node)
{
   struct io_node *n;
   char ret = true;

   /* See if the node is already in the list */
   spa_list_for_each (n, &hotplug_io_list, link)
   {
      if (n->id == node->id)
      {
         ret = false;
         goto dup_found;
      }
   }

   /* Add to the list if the node doesn't already exist */
   spa_list_append(&hotplug_io_list, &node->link);

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
      _aaxPipeWireDriverLogVar(NULL, "PipeWire: proxy object error: %s",
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
      hotplug_init_complete = true;
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
         int str_buffer_len;
         char is_capture;
         char is_lazy;

         /* Just want sink and capture */
         if (!strcasecmp(media_class, "Audio/Sink")) {
            is_capture = false;
         } else if (!strcasecmp(media_class, "Audio/Source")) {
            is_capture = true;
         } else {
            return;
         }

         if (pulseaudio == -1) {
#if HAVE_PULSEAUDIO_H
             pulseaudio = _aaxPulseAudioDriverDetect(is_capture ? 0 : 1);
#endif
         }

         is_lazy = false;
         if (pulseaudio)
         {
            node_desc = spa_dict_lookup(props, PW_KEY_NODE_NAME);
            if (node_desc && (!strncmp(node_desc, "alsa_output.usb", 15) ||
                              !strncmp(node_desc, "alsa_output.bluetooth", 21)))
            {
               is_lazy = true;
            }
         }

         node_desc = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
         if (node_desc)
         {
            node = node_object_new(id, type, version, &interface_node_events, &interface_core_events);
            if (node == NULL)
            {
               _AAX_SYSLOG("PipeWire: Failed to allocate interface node");
               return;
            }

            /* Allocate and initialize the I/O node information struct */
            str_buffer_len = strlen(node_desc) + 1;
            node->userdata = io = calloc(1, sizeof(struct io_node) + str_buffer_len);
            if (io == NULL)
            {
               node_object_destroy(node);
               _AAX_SYSLOG("PipeWire: out of memory");
               return;
            }

            /* Begin setting the node properties */
            io->id = id;
            io->is_lazy = is_lazy;
            io->is_capture = is_capture;
            io->spec.format = SPA_AUDIO_FORMAT_S16;
            strlcpy(io->name, node_desc, str_buffer_len);

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
         _AAX_SYSLOG("PipeWire: Failed to allocate metadata node");
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
      _AAX_SYSLOG("PipeWire: Failed to create hotplug detection loop");
      return errno;
   }

   hotplug_context = ppw_context_new(ppw_thread_loop_get_loop(hotplug_loop), NULL, 0);
   if (hotplug_context == NULL)
   {
      _AAX_SYSLOG("PipeWire: Failed to create hotplug detection context");
      return errno;
   }

   hotplug_core = ppw_context_connect(hotplug_context, NULL, 0);
   if (hotplug_core == NULL)
   {
      _AAX_SYSLOG("PipeWire: Failed to connect hotplug detection context");
      return errno;
   }

   hotplug_registry = pw_core_get_registry(hotplug_core, PW_VERSION_REGISTRY, 0);
   if (hotplug_registry == NULL)
   {
      _AAX_SYSLOG("PipeWire: Failed to acquire hotplug detection registry");
      return errno;
   }

   spa_zero(hotplug_registry_listener);
   pw_registry_add_listener(hotplug_registry, &hotplug_registry_listener, &registry_events, NULL);

   spa_zero(hotplug_core_listener);
   pw_core_add_listener(hotplug_core, &hotplug_core_listener, &hotplug_init_core_events, NULL);

   hotplug_init_seq_val = pw_core_sync(hotplug_core, PW_ID_CORE, 0);

   res = ppw_thread_loop_start(hotplug_loop);
   if (res != 0) {
      _AAX_SYSLOG("PipeWire: Failed to start hotplug detection loop");
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

   hotplug_init_complete = false;
   hotplug_events_enabled = false;

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
      pipewire_initialized = true;

      err = hotplug_loop_init();
      if (err != 0)
      {
         _aaxPipeWireDriverLogVar(NULL, "PipeWire: hotplug loop error: %s",
                                  strerror(errno));
         hotplug_loop_destroy();
         pipewire_initialized = false;
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
         if (!io->is_lazy)
         {
            int m = io->is_capture ? 0 : 1;
            driver_list_add(description[m], io->name);
         }
      }

      hotplug_events_enabled = true;

      ppw_thread_loop_unlock(hotplug_loop);
   }
}

static void
stream_playback_cb(void *be_ptr)
{
   if (be_ptr)
   {
      _driver_t *be_handle = (_driver_t *)be_ptr;
      _handle_t *handle = (_handle_t *)be_handle->handle;

      if (handle->ringbuffer) {
         _aaxSoftwareMixerThreadUpdate(handle, handle->ringbuffer);
      }
      if (handle->batch_finished) { // batched mode
         _aaxSemaphoreRelease(handle->batch_finished);
      }

      be_handle->callback_avail = 0;
      be_handle->callback_dt = _aaxTimerElapsed(be_handle->callback_timer);

      if (_IS_PLAYING(handle) && be_handle->dataBuffer)
      {
         struct pw_buffer *pw_buf;

         assert(be_handle->mode != AAX_MODE_READ);
         assert(be_handle->dataBuffer);

         pw_buf = ppw_stream_dequeue_buffer(be_handle->pw);
         if (pw_buf)
         {
            struct spa_buffer *spa_buf = pw_buf->buffer;
            _data_t *buf = be_handle->dataBuffer;
            uint64_t len = _aaxDataGetDataAvail(buf, 0);
            int frame_sz;

            frame_sz = be_handle->spec.channels*be_handle->bits_sample/8;

            _aaxMutexLock(be_handle->mutex);

            if (_aaxDataGetDataAvail(be_handle->dataBuffer, 0) < len) {
               len = _aaxDataGetDataAvail(be_handle->dataBuffer, 0);
            }

            be_handle->callback_avail = len;
            len = _aaxDataMove(buf, 0, spa_buf->datas[0].data,
                               be_handle->period_frames*frame_sz);

            _aaxMutexUnLock(be_handle->mutex);

            spa_buf->datas[0].chunk->offset = 0;
            spa_buf->datas[0].chunk->size = len;
            spa_buf->datas[0].chunk->stride = frame_sz;

            ppw_stream_queue_buffer(be_handle->pw, pw_buf);
         }
      }
   }
}

static void
stream_capture_cb(void *be_ptr)
{
   if (be_ptr)
   {
      _driver_t *be_handle = (_driver_t *)be_ptr;
      if (be_handle->dataBuffer)
      {
         struct pw_buffer  *pw_buf;

         pw_buf = ppw_stream_dequeue_buffer(be_handle->pw);
         if (pw_buf)
         {
            struct spa_buffer *spa_buf = pw_buf->buffer;
            uint32_t offs = SPA_MIN(spa_buf->datas[0].chunk->offset,
                                    spa_buf->datas[0].maxsize);
            uint32_t len = SPA_MIN(spa_buf->datas[0].chunk->size,
                                   spa_buf->datas[0].maxsize - offs);

            if (_aaxDataGetOffset(be_handle->dataBuffer, 0)+len
                                   < _aaxDataGetSize(be_handle->dataBuffer))
            {
               uint8_t *buf = (uint8_t*)spa_buf->datas[0].data + offs;
               _aaxDataAdd(be_handle->dataBuffer, 0, buf, len);
            }
            ppw_stream_queue_buffer(be_handle->pw, pw_buf);
         }
      }
   }
}

static void
stream_add_buffer_cb(void *be_ptr, struct pw_buffer *buffer)
{
   if (be_ptr)
   {
      _driver_t *be_handle = (_driver_t *)be_ptr;
      if (be_handle->mode != AAX_MODE_READ)
      {
         size_t buf_len;
         int frame_sz;

         // Clamp the output spec samples and size to the max size of the
         // PipeWire buffer.
         frame_sz = be_handle->spec.channels*be_handle->bits_sample/8;
         buf_len = be_handle->period_frames*frame_sz;
         if (buf_len > buffer->buffer->datas[0].maxsize) {
            be_handle->period_frames = buffer->buffer->datas[0].maxsize/frame_sz;
         }
      }

      be_handle->stream_init_status |= PW_READY_FLAG_BUFFER_ADDED;
      ppw_thread_loop_signal(be_handle->ml, false);
   }
}

static void
stream_control_cb(void *be_ptr, uint32_t control, const struct pw_stream_control *pw)
{
   if (be_ptr)
   {
      _driver_t *be_handle = (_driver_t *)be_ptr;

      switch (control)
      {
      case SPA_PROP_volume:
         be_handle->hwgain = be_handle->volumeCur = pw->values[0];
         be_handle->volumeMin = pw->min;
         be_handle->volumeMax = pw->max;
         break;
      case SPA_PROP_mute:
      case SPA_PROP_channelVolumes:
      case SPA_PROP_monitorMute:
      case SPA_PROP_monitorVolumes:
      case SPA_PROP_softMute:
      case SPA_PROP_softVolumes:
      default:
         break;
      }
   }
}

static void
stream_state_cb(void *be_ptr, enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
   if (be_ptr)
   {
      _driver_t *be_handle = (_driver_t *)be_ptr;

      switch(state)
      {
      case PW_STREAM_STATE_STREAMING:
         be_handle->stream_init_status |= PW_READY_FLAG_STREAM_READY;
         // intentional fallthrough
      case PW_STREAM_STATE_ERROR:
         ppw_thread_loop_signal(be_handle->ml, false);
         break;
      case PW_STREAM_STATE_CONNECTING:
      case PW_STREAM_STATE_UNCONNECTED:
      case PW_STREAM_STATE_PAUSED:
      default:
         break;
      }
   }
}

static const struct pw_stream_events stream_output_events =
{
  PW_VERSION_STREAM_EVENTS,
  .control_info = stream_control_cb,
  .state_changed = stream_state_cb,
  .add_buffer = stream_add_buffer_cb,
  .process = stream_playback_cb
};
static const struct pw_stream_events stream_input_events =
{
  PW_VERSION_STREAM_EVENTS,
  .control_info = stream_control_cb,
  .state_changed = stream_state_cb,
  .add_buffer = stream_add_buffer_cb,
  .process = stream_capture_cb
};

static uint32_t
_pipewire_get_id_by_name(_driver_t *handle, const char *name)
{
   uint32_t rv = PW_ID_ANY;
   struct io_node *n;

   if (!hotplug_events_enabled) {
      _aaxPipeWireDriverGetDevices(handle, handle->mode);
   }

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

static int
_aaxPipeWireAudioStreamConnect(_driver_t *handle, enum pw_stream_flags flags, const char **error)
{
   char m = (handle->mode == AAX_MODE_READ) ? 1 : 0;
   uint8_t pod_buffer[PW_POD_BUFFER_LENGTH];
   struct spa_audio_info_raw spa_info = { 0 };
   const struct spa_pod *params = NULL;
   struct spa_pod_builder b;
   int rv = false;

   *error = NULL;

   handle->id = _pipewire_get_id_by_name(handle, handle->driver);

   spa_info = handle->spec;
   b = SPA_POD_BUILDER_INIT(pod_buffer, sizeof(pod_buffer));
   params = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &spa_info);
   if (params)
   {
      char thread_name[PW_THREAD_NAME_BUFFER_LENGTH];

      snprintf(thread_name, sizeof(thread_name), "aaxBackend");
      handle->ml = ppw_thread_loop_new(thread_name, NULL);
   }

   if (handle->ml)
   {
      struct pw_properties *props;

      props = ppw_properties_new(NULL, NULL);
      if (props)
      {
         const char *app_name = AAX_LIBRARY_STR;
         const char *role = (handle->latency < 0.010f) ? "Music" : "Game";
         const char *category = m ? "Capture" : "Playback";
         const char *stream_name = NULL;
         const char *stream_desc = NULL;

         if (ppw_get_application_name) {
            app_name = ppw_get_application_name();
         }
         if (!stream_name && ppw_get_prgname) {
            app_name = ppw_get_prgname();
         }

         if (!stream_name) {
            stream_name = handle->meta.artist;
         }
         if (!stream_desc)
         {
            stream_desc = handle->meta.title;
            if (!stream_desc) {
               stream_desc = stream_name;
            } else if (!stream_name) {
               stream_name = stream_desc;
            }
         }
         if (!stream_name) {
            stream_name = _aax_get_binary_name("Audio Stream");
         }
         if (!stream_desc) {
            stream_desc = AAX_LIBRARY_STR;
         }

         ppw_properties_set(props, PW_KEY_MEDIA_TYPE, "Audio");
         ppw_properties_set(props, PW_KEY_MEDIA_CATEGORY, category);
         ppw_properties_set(props, PW_KEY_MEDIA_ROLE, role);
         ppw_properties_set(props, PW_KEY_APP_NAME, app_name);
         ppw_properties_set(props, PW_KEY_NODE_NAME, stream_name);
         ppw_properties_set(props, PW_KEY_NODE_DESCRIPTION, stream_desc);
         ppw_properties_setf(props, PW_KEY_NODE_LATENCY, "%u/%i", handle->period_frames, handle->spec.rate);
         ppw_properties_setf(props, PW_KEY_NODE_RATE, "1/%u", handle->spec.rate);
         ppw_properties_set(props, PW_KEY_NODE_ALWAYS_PROCESS, "true");

         handle->pw = ppw_stream_new_simple(
                         ppw_thread_loop_get_loop(handle->ml),
                         stream_name, props,
                         m ? &stream_input_events : &stream_output_events,
                         handle);
         if (handle->pw)
         {
            uint32_t node_id = handle->id;
            int res;

            res = ppw_stream_connect(handle->pw,
                                   m ? PW_DIRECTION_INPUT : PW_DIRECTION_OUTPUT,
                                   node_id, flags, &params, 1);
            if (res == 0)
            {
               res = ppw_thread_loop_start(handle->ml);
               if (res == 0)
               {
                  ppw_thread_loop_lock(handle->ml);
                  while (handle->stream_init_status != PW_READY_FLAG_ALL_BITS &&
                         ppw_stream_get_state(handle->pw, NULL) != PW_STREAM_STATE_ERROR)
                  {
                     ppw_thread_loop_wait(handle->ml);
                  }
                  ppw_thread_loop_unlock(handle->ml);

                  if (ppw_stream_get_state(handle->pw, error) != PW_STREAM_STATE_ERROR)
                  {
                     const char *s, *p;

                     s = ppw_properties_get(props, PW_KEY_NODE_LATENCY);
                     p = strchr(s, '/');
                     if (p)
                     {
                        handle->period_frames = atoi(s);
                        handle->spec.rate = atoi(p+1);
                        handle->latency = (float)handle->period_frames/handle->spec.rate;
                     }
                     rv = true;
                  }
               }
               else {
                  *error = "PipeWire: Failed to start stream loop";
               }
            }
            else {
               *error = "PipeWire: Failed to connect stream";
            }
         }
      }
   }

   if (!params) {
      *error = "incompatible hardware configuration";
   } else if (!handle->ml) {
      *error = "failed to create a stream loop";
   } else if (!handle->pw) {
      *error = "failed to create a stream";
   }

   return rv;
}

static enum aaxFormat
_aaxPipeWireGetFormat(unsigned int format)
{
   enum aaxFormat rv = AAX_PCM16S;
   switch(format)
   {
   case SPA_AUDIO_FORMAT_ALAW:
      rv = AAX_ALAW;
      break;
   case SPA_AUDIO_FORMAT_ULAW:
      rv = AAX_MULAW;
      break;
   case SPA_AUDIO_FORMAT_S8:
      rv = AAX_PCM8S;
      break;
   case SPA_AUDIO_FORMAT_U8:
      rv = AAX_PCM8U;
      break;
#if __BYTE_ORDER == __BIG_ENDIAN
#else
   case SPA_AUDIO_FORMAT_S16_LE:
      rv = AAX_PCM16S;
      break;
   case SPA_AUDIO_FORMAT_S16_BE:
      rv = AAX_PCM16S_BE;
      break;
   case SPA_AUDIO_FORMAT_U16_LE:
      rv = AAX_PCM16U;
      break;
   case SPA_AUDIO_FORMAT_U16_BE:
      rv = AAX_PCM16U_BE;
      break;
   case SPA_AUDIO_FORMAT_S24_32_LE:
      rv = AAX_PCM24S;
      break;
   case SPA_AUDIO_FORMAT_S24_32_BE:
      rv = AAX_PCM24S_BE;
      break;
   case SPA_AUDIO_FORMAT_U24_32_LE:
      rv = AAX_PCM24U;
      break;
   case SPA_AUDIO_FORMAT_U24_32_BE:
      rv = AAX_PCM24U_BE;
      break;
   case SPA_AUDIO_FORMAT_S32_LE:
      rv = AAX_PCM32S;
      break;
   case SPA_AUDIO_FORMAT_S32_BE:
      rv = AAX_PCM32S_BE;
      break;
   case SPA_AUDIO_FORMAT_U32_LE:
      rv = AAX_PCM32U;
      break;
   case SPA_AUDIO_FORMAT_U32_BE:
      rv = AAX_PCM32U_BE;
      break;
   case SPA_AUDIO_FORMAT_S24_LE:
      rv = AAX_PCM24S_PACKED;
      break;
   case SPA_AUDIO_FORMAT_S24_BE:
      rv = AAX_PCM24S_PACKED_BE;
      break;
   case SPA_AUDIO_FORMAT_F32_LE:
      rv = AAX_FLOAT;
      break;
   case SPA_AUDIO_FORMAT_F32_BE:
      rv = AAX_FLOAT_BE;
      break;
   case SPA_AUDIO_FORMAT_F64_LE:
      rv = AAX_DOUBLE;
      break;
   case SPA_AUDIO_FORMAT_F64_BE:
      rv = AAX_DOUBLE_BE;
      break;
#endif
   default:
      break;
   }
   return rv;
}

#if USE_PIPEWIRE_THREAD
static int
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
      return false;
   }

   be = handle->backend.ptr;
   be_handle = (_driver_t *)handle->backend.handle;

   delay_sec = 1.0f/handle->info->period_rate;

   tracks = 2;
   freq = DEFAULT_OUTPUT_RATE;
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

   be_handle->dataBuffer = _aaxDataCreate(1, 1, 1);

   _aaxMutexLock(handle->thread.signal.mutex);
   do
   {
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

      res = _aaxSignalWaitTimed(&handle->thread.signal, delay_sec);
#if 0
 printf("dt: %5.4f, delay: %5.4f, avail: %lu\n",
        be_handle->callback_dt, delay_sec, be_handle->callback_avail);
#endif
   }
   while (res == AAX_TIMEOUT || res == true);

   _aaxMutexUnLock(handle->thread.signal.mutex);

#if 0
   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      be->destroy_ringbuffer(handle->ringbuffer);
      handle->ringbuffer = NULL;
   }
#endif

   return handle ? true : false;
}
#endif

#endif /* HAVE_PIPEWIRE_H */
