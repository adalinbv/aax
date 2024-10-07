/*
 * SPDX-FileCopyrightText: Copyright © 2018-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2018-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

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

#include <arch.h>
#include <api.h>
#include <devices.h>
#include <ringbuffer.h>

#include <software/renderer.h>
#include "audio.h"
#include "device.h"


#define COREAUDIO_ID_STRING	"CoreAudio"
#define MAX_ID_STRLEN		32

#define DEFAULT_OUTPUT_RATE	48000
#define DEFAULT_DEVNAME		"default"
#define DEFAULT_RENDERER	"CoreAudio"

#define _AAX_DRVLOG(a)		_aaxCoreAudioDriverLog(NULL, 0, 0, a)

static _aaxDriverDetect _aaxCoreAudioDriverDetect;
static _aaxDriverNewHandle _aaxCoreAudioDriverNewHandle;
static _aaxDriverFreeHandle _aaxCodeAudioDriverFreeHandle;
static _aaxDriverGetDevices _aaxCoreAudioDriverGetDevices;
static _aaxDriverGetInterfaces _aaxCoreAudioDriverGetInterfaces;
static _aaxDriverConnect _aaxCoreAudioDriverConnect;
static _aaxDriverDisconnect _aaxCoreAudioDriverDisconnect;
static _aaxDriverSetup _aaxCoreAudioDriverSetup;
static _aaxDriverCaptureCallback _aaxCoreAudioDriverCapture;
static _aaxDriverPlaybackCallback _aaxCoreAudioDriverPlayback;
static _aaxDriverSetName _aaxCoreAudioDriverSetName;
static _aaxDriverGetName _aaxCoreAudioDriverGetName;
static _aaxDriverRender _aaxCoreAudioDriverRender;
static _aaxDriverState _aaxCoreAudioDriverState;
static _aaxDriverParam _aaxCoreAudioDriverParam;
static _aaxDriverLog _aaxCoreAudioDriverLog;

static char _coreaudio_id_str[MAX_ID_STRLEN+1] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxCoreAudioDriverBackend =
{
   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_coreaudio_id_str,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxCoreAudioDriverDetect,
   (_aaxDriverNewHandle *)&_aaxCoreAudioDriverNewHandle,
   (_aaxDriverFreeHandle *)&_aaxCodeAudioDriverFreeHandle,
   (_aaxDriverGetDevices *)&_aaxCoreAudioDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxCoreAudioDriverGetInterfaces,

   (_aaxDriverSetName *)&_aaxCoreAudioDriverSetName,
   (_aaxDriverGetName *)&_aaxCoreAudioDriverGetName,
   (_aaxDriverRender *)&_aaxCoreAudioDriverRender,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxCoreAudioDriverConnect,
   (_aaxDriverDisconnect *)&_aaxCoreAudioDriverDisconnect,
   (_aaxDriverSetup *)&_aaxCoreAudioDriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxCoreAudioDriverCapture,
   (_aaxDriverPlaybackCallback *)&_aaxCoreAudioDriverPlayback,
   NULL,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,
   NULL,

   ( _aaxDriverGetSetSources*)_aaxSoftwareDriverGetSetSources,

   (_aaxDriverState *)&_aaxCoreAudioDriverState,
   (_aaxDriverParam *)&_aaxCoreAudioDriverParam,
   (_aaxDriverLog *)&_aaxCoreAudioDriverLog
};


const char *_coreaudio_default_name = DEFAULT_DEVNAME;
static const int __rate[] = { AL_INPUT_RATE, AL_OUTPUT_RATE };

static void *audio = NULL;

static int
_aaxCoreAudioDriverDetect(UNUSED(int mode))
{
   static int rv = false;

   _AAX_LOG(LOG_DEBUG, __func__);

   if (TEST_FOR_FALSE(rv) && !audio) {
     audio = _aaxIsLibraryPresent("audio", 0);
   }

   if (audio)
   {
      char *error;

      snprintf(_coreaudio_id_str, MAX_ID_STRLEN, "%s %s",
                               DEFAULT_RENDERER, COREAUDIO_ID_STRING);

      _aaxGetSymError(0);

      TIE_FUNCTION(alSetParams);
      if (palSetParams)
      {
         TIE_FUNCTION(alGetErrorString);
      }

      error = _aaxGetSymError(0);
      if (!error) {
      {
         rv = true;
      }
   }

   return rv;
}

static void *
_aaxCoreAudioDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle;
   size_t size;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   size = sizeof(_driver_t) + MAX_PORTS*sizeof(_port_t);
   handle = (_driver_t *)calloc(1, size);
   if (handle)
   {
   }

   return handle;
}

static int
_aaxCodeAudioDriverFreeHandle(UNUSED(void *id))
{
   _aaxCloseLibrary(audio);
   audio = NULL;

   return true;
}

static void *
_aaxCoreAudioDriverConnect(void *config, const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int i;
   int res;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      id = handle = _aaxCoreAudioDriverNewHandle(mode);
   }

   if (handle)
   {
      handle->handle = config;
      if (renderer) {
         handle->name = (char *)renderer;
      }

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
               if (strcasecmp(s, DEFAULT_DEVNAME)) {
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
               _AAX_DRVLOG("coreaudio; frequency too small.");
               f = (float)_AAX_MIN_MIXER_FREQUENCY;
            }
            else if (f > (float)_AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_DRVLOG("coreaudio; frequency too large.");
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
                  _AAX_DRVLOG("coreaudio; no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_DRVLOG("coreaudio; no. tracks too great.");
                  i = _AAX_MAX_SPEAKERS;
               }
               handle->no_channels = i;
            }
         }

         i = xmlNodeGetInt(xid, "bits-per-sample");
         if (i)
         {
            if (i != 16)
            {
               _AAX_DRVLOG("coreaudio; unsopported bits-per-sample");
               i = 16;
            }
            handle->bytes_sample = i/8;
         }
      }
   }

   if (handle)
   {
      handle->device = detect_devnum(handle->name);
      if (handle->device <= 0)
      {
         if (handle->device < 0)
         {
            /* free memory allocations */
            _aaxCoreAudioDriverDisconnect(handle);
            return NULL;
         }

         handle->device = AL_DEFAULT_OUTPUT;
      }
   }

   return (void *)handle;
}

static int
_aaxCoreAudioDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __func__);

   if (handle)
   {
   }

   return true;
}

static int
_aaxCoreAudioDriverSetup(const void *id, UNUSED(float *refresh_rate), int *fmt,
                      unsigned int *tracks, float *speed, UNUSED(int *bitrate),
                      int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int channels, data_format;
   ALpv params[2];
   unsigned int i;
   int result;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert (id != 0);

   return true;
}

static ssize_t
_aaxCoreAudioDriverCapture(const void *id, void **data, ssize_t *offset, size_t *frames, void *scratch, size_t scratchlen, float gain, UNUSED(char batched))
{
   _driver_t *handle = (_driver_t *)id;
   size_t scratchsz, nframes = *frames;
   ssize_t offs = *offset;
   int tracks;

   *offset = 0;
   if ((handle->mode != 0) || (frames == 0) || (data == 0)) {
     return false;
   }

   if (nframes == 0) {
      return true;
   }

   *frames = 0;
   tracks = handle->no_channels;


   *frames = nframes;
   return true;
}

static size_t
_aaxCoreAudioDriverPlayback(const void *id, void *s, UNUSED(float pitch), float gain,
                         UNUSED(char batched))
{
   _aaxRingBuffer *rb = (_aaxRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   ssize_t offs, no_samples;
   size_t outbuf_size;
   int no_tracks;
   const int32_t **sbuf;
   int16_t *data;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(rb);
   assert(id != 0);

   if (handle->mode == 0)
      return 0;

   offs = rb->get_parami(rb, RB_OFFSET_SAMPLES);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES) - offs;
   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);

   outbuf_size = no_tracks * no_samples*sizeof(int16_t);
   if (handle->scratch == 0)
   {
      char *p;
      handle->scratch = (void**)_aax_malloc(&p, 0, outbuf_size);
      handle->data = (int16_t**)p;
#ifndef NDEBUG
      handle->buf_len = outbuf_size;
#endif
   }
   data = (int16_t*)handle->data;
   assert(outbuf_size <= handle->buf_len);

   sbuf = (const int32_t**)rb->get_tracks_ptr(rb, RB_READ);





   free(data);

   return 0;
}

static int
_aaxCoreAudioDriverSetName(const void *id, int type, const char *name)
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
}

static char *
_aaxCoreAudioDriverGetName(const void *id, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   /* TODO: distinguish between playback and record */
   if (handle && handle->name && (mode < AAX_MODE_WRITE_MAX)) {
      ret = _aax_strdup(handle->name);
   }

   return ret;
#endif
}

_aaxRenderer*
_aaxCoreAudioDriverRender(const void* config)
{
   _driver_t *handle = (_driver_t *)config;
   return handle->render;
}


static int
_aaxCoreAudioDriverState(const void *id, enum _aaxDriverState state)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = false;
   unsigned int i;
   ALpv params;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert (id != 0);

   switch(state)
   {
   case DRIVER_PAUSE:
      if (handle)
      {
         rv = true;
      }
      break;
   case DRIVER_RESUME:
      if (handle)
      {
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
_aaxCoreAudioDriverParam(const void *id, enum _aaxDriverParam param)
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
         rv = 8000.0f;
         break;
      case DRIVER_MAX_FREQUENCY:
         rv = _AAX_MAX_MIXER_FREQUENCY;
         break;
      case DRIVER_MIN_TRACKS:
         rv = 1.0f;
         break;
      case DRIVER_MAX_TRACKS:
         rv = (float)_AAX_MAX_SPEAKERS;
         break;
      case DRIVER_MIN_PERIODS:
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
         rv = (float)handle->no_frames;
         break;

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
_aaxCoreAudioDriverGetDevices(UNUSED(const void *id), int mode)
{
   static char *renderers[2] = { "\0\0", "\0\0" };
   return (char *)renderers[mode ? 0 : 1];
}

static char *
_aaxCoreAudioDriverGetInterfaces(UNUSED(const void *id), UNUSED(const char*devname), int mode)
{
   static char *renderers[2] = { "\0\0", "\0\0" };
   return (char *)renderers[mode ? 0 : 1];
}

static char *
_aaxCoreAudioDriverLog(const void *id, UNUSED(int prio), UNUSED(int type), const char *str)
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

/*-------------------------------------------------------------------------- */

