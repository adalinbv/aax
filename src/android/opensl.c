/*
 * Copyright 2014 by Erik Hofman.
 * Copyright 2014 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

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

#include "SLES/OpenSLES.h"
#include "SLES/OpenSLES_Android.h"
#include "SLES/OpenSLES_AndroidMetadata.h"

#include "audio.h"
#include "opensl.h"

#define NO_FRAGMENTS		3
#define NUM_INTERFACES		3
#define NO_SAMPLES		512
#define MAX_ID_STRLEN		64
#define DEFAULT_RENDERER	"SLES"
#define	DEFAULT_INPUT_ID	SL_DEFAULTDEVICEID_AUDIOINPUT
#define DEFAULT_OUTPUT_ID	SL_DEFAULTDEVICEID_AUDIOOUTPUT

#if 0
static _aaxDriverDetect _aaxSLESDriverDetect;
static _aaxDriverNewHandle _aaxSLESDriverNewHandle;
static _aaxDriverGetDevices _aaxSLESDriverGetDevices;
static _aaxDriverGetInterfaces _aaxSLESDriverGetInterfaces;
static _aaxDriverConnect _aaxSLESDriverConnect;
static _aaxDriverDisconnect _aaxSLESDriverDisconnect;
static _aaxDriverSetup _aaxSLESDriverSetup;
static _aaxDriverCaptureCallback _aaxSLESDriverCapture;
static _aaxDriverCallback _aaxSLESDriverPlayback;
static _aaxDriverGetName _aaxSLESDriverGetName;
static _aaxDriverRender _aaxSLESDriverRender;
static _aaxDriverState _aaxSLESDriverState;
static _aaxDriverParam _aaxSLESDriverParam;
static _aaxDriverLog _aaxSLESDriverLog;
static _aaxDriverThread _aaxSLESDriverThread;

static char _sles_id_str[MAX_ID_STRLEN+1] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxSLESDriverBackend =
{
   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_sles_id_str,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxSLESDriverDetect,
   (_aaxDriverNewHandle *)&_aaxSLESDriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxSLESDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxSLESDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxSLESDriverGetName,
   (_aaxDriverRender *)&_aaxSLESDriverRender,
   (_aaxDriverThread *)&_aaxSLESDriverThread,

   (_aaxDriverConnect *)&_aaxSLESDriverConnect,
   (_aaxDriverDisconnect *)&_aaxSLESDriverDisconnect,
   (_aaxDriverSetup *)&_aaxSLESDriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxSLESDriverCapture,
   (_aaxDriverCallback *)&_aaxSLESDriverPlayback,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)&_aaxSLESDriverState,
   (_aaxDriverParam *)&_aaxSLESDriverParam,
   (_aaxDriverLog *)&_aaxSLESDriverLog
};

typedef struct
{
   SLEngineItf engineItf;
   SLObjectItf engineObjItf;
   SLObjectItf mixerObjItf;
   SLMetadataExtractionItf metadataItf;
 
   SLObjectItf playerObjItf;
   SLVolumeItf volumeItf;
   SLPlayItf playItf;
   SLAndroidSimpleBufferQueueItf bufferQItf;

   SLDataFormat_PCM pcm;
   SLDataLocator_AndroidSimpleBufferQueue queue;
   SLDataLocator_OutputMix mixer;
   SLDataSource source;
   SLDataSink sink;

   _aaxSemaphore *worker_start;
   _aaxRenderer *render;
   _handle_t *handle;

   char *name;
   unsigned int mode;
   unsigned int bits_sample;
   unsigned int no_tracks;
   unsigned int no_frames;
   float frequency_hz;
   float latency;

   int16_t *buffer[NO_FRAGMENTS];
   int buffer_pos, max_buffers;
   size_t buf_len;

   float volumeHW, hwgain;
   float volumeCur, volumeInit;
   float volumeMin, volumeMax;

   /* capabilities */
   unsigned int min_frequency;
   unsigned int max_frequency;
   unsigned int min_tracks;
   unsigned int max_tracks;

} _driver_t;

static int _sles_get_hw_settings(_driver_t*);
static int _sles_get_volume_range(_driver_t*);
static float _sles_set_volume(_driver_t*, _aaxRingBuffer*, ssize_t, ssize_t, unsigned int, float);
static void _sles_set_pcm(_driver_t*, int, int, int);
static void _aaxSLESDriverCallback(SLAndroidSimpleBufferQueueItf, void*);

#define _AAX_DRVLOG(a)		_aaxSLESDriverLog(id, __LINE__, 0, a)


static int
_aaxSLESDriverDetect(int mode)
{
   static void *audio = NULL;
   static int rv = AAX_FALSE;
// char *error = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   if TEST_FOR_FALSE(rv) {
      audio = _aaxIsLibraryPresent("OpenSLES", 0);
   }

   if (audio)
   {
      snprintf(_sles_id_str, MAX_ID_STRLEN, "%s", DEFAULT_RENDERER);
      rv = AAX_TRUE;
   }

   return rv;
}

static void *
_aaxSLESDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle;
   size_t size;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   size = sizeof(_driver_t);
   handle = (_driver_t *)calloc(1, size);
   if (handle)
   {
      handle->frequency_hz = 48000.0f;
      handle->bits_sample = 16;
      handle->no_tracks = 2;
      handle->mode = mode;
   }

   return handle;
}

static void *
_aaxSLESDriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   uint32_t res;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      handle = _aaxSLESDriverNewHandle(mode);
   }

   if (handle)
   {
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
               _AAX_DRVLOG("frequency too small.");
               f = (float)_AAX_MIN_MIXER_FREQUENCY;
            }
            else if (f > (float)_AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_DRVLOG("frequency too large.");
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
                  _AAX_DRVLOG("no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_DRVLOG("no. tracks too great.");
                  i = _AAX_MAX_SPEAKERS;
               }
               handle->no_tracks = i;
            }
         }

         i = xmlNodeGetInt(xid, "bits-per-sample");
         if (i)
         {
            if (i != 16)
            {
               _AAX_DRVLOG("unsopported bits-per-sample");
               i = 16;
            }
            handle->bits_sample = i;
         }
      }
   }

   if (handle)
   {
      res = slCreateEngine(&handle->engineObjItf, 0, NULL, 0, NULL, NULL);
      if (res != SL_RESULT_SUCCESS) { 
         res = (*handle->engineObjItf)->Realize(handle->engineObjItf,
                                                SL_BOOLEAN_FALSE);
      }
      if (res == SL_RESULT_SUCCESS) {
         res = (*handle->engineObjItf)->GetInterface(handle->engineObjItf,
                                                     SL_IID_ENGINE,
                                                     &handle->engineItf);
      }
      if (res  == SL_RESULT_SUCCESS) {
         res = (*handle->engineItf)->CreateOutputMix(handle->engineItf,
                                                 &handle->mixerObjItf, 0, 0, 0);
      }
      if (res == SL_RESULT_SUCCESS) {
         res = (*handle->mixerObjItf)->Realize(handle->mixerObjItf,
                                               SL_BOOLEAN_FALSE);
      }
      if (res == SL_RESULT_SUCCESS)
      {
         handle->queue.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;

         handle->source.pLocator = &handle->queue;
         handle->source.pFormat = &handle->pcm;

         handle->mixer.locatorType = SL_DATALOCATOR_OUTPUTMIX;
         handle->mixer.outputMix = handle->mixerObjItf;

         handle->sink.pLocator = &handle->mixer; 
         handle->sink.pFormat = NULL;

         _sles_set_pcm(handle, handle->no_tracks, handle->bits_sample,
                               handle->frequency_hz);
      }
      else {
         handle = NULL;
      }
   }

   return (void *)handle;
}

static int
_aaxSLESDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   if (handle)
   {
      int i;

      for (i=0; i<handle->max_buffers; i++) {
         _aax_aligned_free(handle->buffer[i]);
      }

      if (handle->playerObjItf != NULL) {
         (*handle->playerObjItf)->Destroy(handle->playerObjItf);
      }
      if (handle->mixerObjItf != NULL) {
         (*handle->mixerObjItf)->Destroy(handle->mixerObjItf);
      }
      if (handle->engineObjItf != NULL) {
         (*handle->engineObjItf)->Destroy(handle->engineObjItf);
      }

      if (handle->render) {
         handle->render->close(handle->render->id);
      }

      _aaxSemaphoreDestroy(handle->worker_start);
      free(handle);
   }

   return AAX_TRUE;
}

static int
_aaxSLESDriverSetup(const void *id, float *refresh_rate, int *fmt,
                    unsigned int *tracks, float *speed, int *bitrate,
                    int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
   ssize_t period_frames;
   SLuint32 num;
   SLresult res;
  
   period_frames = (size_t)rintf(freq / *refresh_rate);

   assert(handle);

   _sles_get_hw_settings(handle);

   if (handle->no_tracks > *tracks) {
      handle->no_tracks = *tracks;
   }
   *tracks = handle->no_tracks;
   *fmt = AAX_PCM16S;

   // Use the sample rate provided by
   //  AudioManager.getProperty(PROPERTY_OUTPUT_SAMPLE_RATE).
   //  Otherwise your buffers take a detour through the system resampler.
   *speed = handle->pcm.samplesPerSec;

   // Make your buffer size a multiple of
   // AudioManager.getProperty(PROPERTY_OUTPUT_FRAMES_PER_BUFFER).
   // Otherwise your callback will occasionally get two calls per timeslice
   // rather than one. Unless your CPU usage is really light, this will
   // probably end up glitching.
// handle->no_frames = AudioManager.getProperty(PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
   *refresh_rate = freq/(float)period_frames;

   // TODO: for now
   if (*tracks > 2)
   {
      char str[255];
      snprintf((char *)&str, 255, "Unable to output to %i speakers in "
                "this setup (2 is the maximum)", *tracks);
      _AAX_SYSLOG(str);
      return AAX_FALSE;
   }

   handle->min_frequency = 8000.0f;
   handle->max_frequency = _AAX_MAX_MIXER_FREQUENCY;
   handle->min_tracks = 1;
   handle->max_tracks = _AAX_MAX_SPEAKERS;

   // Early versions of OpenSL required at least two buffers but this was later
   // relaxed to just one. Go for the least required number fo buffers for
   // lowest latency.
   num = 1;
   do
   {
      const SLInterfaceID _aax_itf_ids[NUM_INTERFACES] = {
         SL_IID_BUFFERQUEUE,
         SL_IID_VOLUME,
         SL_IID_METADATAEXTRACTION
      };
      const SLboolean _aax_itf_req[NUM_INTERFACES] = {
         SL_BOOLEAN_TRUE,
         SL_BOOLEAN_TRUE,
         SL_BOOLEAN_TRUE
      };

      handle->max_buffers = num;
      handle->queue.numBuffers = num;
      handle->sink.pFormat = NULL;
      res = (*handle->engineItf)->CreateAudioPlayer(handle->engineItf,
                                                    &handle->playerObjItf,
                                                    &handle->source,
                                                    &handle->sink,
                                                    NUM_INTERFACES,
                                                    _aax_itf_ids,
                                                    _aax_itf_req);
   }
   while ((res != SL_RESULT_SUCCESS) && (++num < NO_FRAGMENTS));

   if (res == SL_RESULT_SUCCESS) {
      res = (*handle->playerObjItf)->Realize(handle->playerObjItf,
                                             SL_BOOLEAN_FALSE);
   }
   if (res == SL_RESULT_SUCCESS) {
      res = (*handle->playerObjItf)->GetInterface(handle->playerObjItf,
                                                  SL_IID_PLAY,
                                                  &handle->playItf);
   }
   if (res == SL_RESULT_SUCCESS) {
      res = (*handle->playerObjItf)->GetInterface(handle->playerObjItf,
                                                  SL_IID_BUFFERQUEUE,
                                                  &handle->bufferQItf);
   }
   if (res == SL_RESULT_SUCCESS) {
      res = (*handle->playerObjItf)->GetInterface(handle->playerObjItf,
                                                  SL_IID_METADATAEXTRACTION,
                                                  &handle->metadataItf);
   }
   if (res == SL_RESULT_SUCCESS) {
      res =(*handle->bufferQItf)->RegisterCallback(handle->bufferQItf,
                                                   _aaxSLESDriverCallback,
                                                   handle);
   }
   if (res == SL_RESULT_SUCCESS) {
      res = (*handle->playerObjItf)->GetInterface(handle->playerObjItf,
                                                  SL_IID_VOLUME,
                                                  &handle->volumeItf);
   }

   _sles_get_volume_range(handle);

   if (res == SL_RESULT_SUCCESS)
   {
      handle->worker_start = _aaxSemaphoreCreate(0);
      handle->render = _aaxSoftwareInitRenderer(handle->latency, handle->mode);
      if (handle->render)
      {
         const char *rstr = handle->render->info(handle->render->id);
         snprintf(_sles_id_str, MAX_ID_STRLEN, "%s %s", DEFAULT_RENDERER, rstr);
      }
   }

   return (res == SL_RESULT_SUCCESS) ? AAX_TRUE : AAX_FALSE;
}

static size_t
_aaxSLESDriverCapture(const void *id, void **data, ssize_t *offset, size_t *frames, void *scratch, size_t scratchlen, float gain)
{
// _driver_t *handle = (_driver_t *)id;
// ssize_t offs = *offset;
   size_t rv = AAX_FALSE;

   assert(handle->mode == AAX_MODE_READ);

   *offset = 0;
   if ((frames == 0) || (data == 0))
      return rv;

   if (*frames)
   {
   }

   return AAX_FALSE;
}

// The documentation available is very unclear about how to best manage buffers.
// I've chosen to this approach: Instantly enqueue a buffer that was rendered
// to the last time, and then render the next. Hopefully it's okay to spend
// time in this callback after having enqueued
static size_t
_aaxSLESDriverPlayback(const void *id, void *s, float pitch, float gain)
{
   _aaxRingBuffer *rb = (_aaxRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   ssize_t offs, outbuf_size, no_samples;
   unsigned int no_tracks;
   const int32_t **sbuf;
   int16_t *data;

   assert(rb);
   assert(id != 0);

   if (handle->mode == 0)
      return 0;

   offs = rb->get_parami(rb, RB_OFFSET_SAMPLES);
   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES) - offs;

   assert(no_samples <= handle->no_frames);

   outbuf_size = no_tracks*no_samples*sizeof(int16_t);
   data = handle->buffer[handle->buffer_pos];
   if (data == NULL)
   {
      data = (int16_t*)_aax_aligned_alloc16(outbuf_size);
      handle->buffer[handle->buffer_pos] = data;
      handle->buf_len = outbuf_size;
   }
   assert(outbuf_size <= handle->buf_len);

   sbuf = (const int32_t **)rb->get_tracks_ptr(rb, RB_READ);
   _sles_set_volume(handle, rb, offs, no_samples, no_tracks, gain);
   _batch_cvt16_intl_24(data, sbuf, offs, no_tracks, no_samples);
   rb->release_tracks_ptr(rb);

   if (is_bigendian()) {
      _batch_endianswap16((uint16_t*)data, no_tracks*no_samples);
   }

   return 0;
}

_aaxRenderer*
_aaxSLESDriverRender(const void* id)
{
   _driver_t *handle = (_driver_t *)id;
   return handle->render;
}


static char *
_aaxSLESDriverGetName(const void *id, int playback)
{
// _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;
   return ret;
}

static int
_aaxSLESDriverState(const void *id, enum _aaxDriverState state)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;
   SLresult res;

   switch(state)
   {
   case DRIVER_PAUSE:
      res = (*handle->playItf)->SetPlayState(handle->playItf,
                                                 SL_PLAYSTATE_PAUSED);
      rv = (res == SL_RESULT_SUCCESS) ? AAX_TRUE : AAX_FALSE;
      break;
   case DRIVER_RESUME:
      res = (*handle->playItf)->SetPlayState(handle->playItf,
                                                 SL_PLAYSTATE_PLAYING);
      rv = (res == SL_RESULT_SUCCESS) ? AAX_TRUE : AAX_FALSE;
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
_aaxSLESDriverParam(const void *id, enum _aaxDriverParam param)
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
         rv = handle->volumeMax;
         break;
      case DRIVER_MIN_VOLUME:
         rv = handle->volumeMin;
         break;
      case DRIVER_VOLUME:
         rv = handle->volumeHW;
         break;
      case DRIVER_SAMPLE_DELAY:
         rv = (float)handle->no_frames;
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
         break
      case DRIVER_MAX_TRACKS:
         rv = (float)handle->max_tracks;
         break;
      case DRIVER_MIN_PERIODS:
      case DRIVER_MAX_PERIODS:
         rv = 2.0f;
         break;

		/* boolean */
      case DRIVER_TIMER_MODE:
          rv = (float)AAX_TRUE;
          break;
      case DRIVER_SHARED_MODE:
      default:
         break;
      }
   }
   return rv;
}
static char *
_aaxSLESDriverGetDevices(const void *id, int mode)
{
   static const char *rd[2] = {
    "\0\0",
    "\0\0"
   };
   return (char *)rd[mode];
}

static char *
_aaxSLESDriverGetInterfaces(const void *id, const char *devname, int mode)
{
   static const char *rd[2] = {
    "\0\0",
    "\0\0"
   };
   return (char *)rd[mode];
}

static char *
_aaxSLESDriverLog(const void *id, int prio, int type, const char *str)
{
   static char _errstr[256];

   snprintf(_errstr, 256, "sles: %s\n", str);
   _errstr[255] = '\0';  /* always null terminated */

   __aaxErrorSet(AAX_BACKEND_ERROR, (char*)&_errstr);
   _AAX_SYSLOG(_errstr);

   return (char*)&_errstr;
}

/* -------------------------------------------------------------------------- */

static int
_sles_get_volume_range(_driver_t *handle)
{
   int rv = 0;

   if (handle && handle->volumeItf)
   {
      SLmillibel vol;
      SLresult res;

      res = (*handle->volumeItf)->GetVolumeLevel(handle->volumeItf, &vol);
      if (res == SL_RESULT_SUCCESS)
      {
         handle->volumeInit = vol;
         handle->volumeCur = handle->volumeInit;
         handle->volumeHW = handle->volumeInit;
         rv = AAX_TRUE;
      }
      res = (*handle->volumeItf)->GetMaxVolumeLevel(handle->volumeItf, &vol);
      if (res == SL_RESULT_SUCCESS) {
         handle->volumeMax = vol;
      }
   }

   return rv;
}

static float 
_sles_set_volume(_driver_t *handle, _aaxRingBuffer *rb, ssize_t offset, ssize_t no_frames, unsigned int no_tracks, float volume)
{
   float gain = fabsf(volume);
   float hwgain = gain;
   float rv = 0;

   if (handle && handle->volumeItf && handle->volumeMax)
   {
      SLresult res;

      hwgain = _MINMAX(fabsf(gain), handle->volumeMin, handle->volumeMax);

      /*
       * Slowly adjust volume to dampen volume slider movement.
       * If the volume step is large, don't dampen it.
       * volume is negative for auto-gain mode.
       */
      if ((volume < 0.0f) && (handle->mode == AAX_MODE_READ))
      {
         float dt = no_frames/handle->frequency_hz;
         float rr = _MINMAX(dt/10.0f, 0.0f, 1.0f);      /* 10 sec average */

         /* Quickly adjust for a very large step in volume */
         if (fabsf(hwgain - handle->volumeCur) > 0.825f) rr = 0.9f;

         hwgain = (1.0f-rr)*handle->hwgain + (rr)*hwgain;
         handle->hwgain = hwgain;
      }

      res = (*handle->volumeItf)->SetVolumeLevel(handle->volumeItf, hwgain);
      if (res == SL_RESULT_SUCCESS)
      {
         if (hwgain) gain /= hwgain;
         else gain = 0.0f;
         rv = gain;
      }
   }

   /* software volume fallback */
   if (rb && fabsf(gain - 1.0f) > 0.05f) {
      rb->data_multiply(rb, offset, no_frames, gain);
   }

   return rv;
}

void
_sles_set_pcm(_driver_t *handle, int tracks, int bits, int rate)
{
   handle->pcm.formatType = SL_DATAFORMAT_PCM;
   handle->pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;

   handle->pcm.numChannels = tracks;
   handle->pcm.channelMask = 0;
   switch (tracks)
   {
   case 8:
      handle->pcm.channelMask |= SL_SPEAKER_SIDE_LEFT|SL_SPEAKER_SIDE_RIGHT;
      // no break needed;
   case 6:
      handle->pcm.channelMask |= SL_SPEAKER_FRONT_CENTER;
      handle->pcm.channelMask |= SL_SPEAKER_LOW_FREQUENCY;
      // no break needed;
   case 4:
      handle->pcm.channelMask |= SL_SPEAKER_BACK_LEFT|SL_SPEAKER_BACK_RIGHT;
      // no break needed;
   case 2:
   default:
      handle->pcm.channelMask |= SL_SPEAKER_FRONT_LEFT|SL_SPEAKER_FRONT_RIGHT;
   break;
   }

   switch (bits)
   {
   case 8:
      handle->pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_8;
      handle->pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_8;
      break;
   case 24:
      handle->pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_24;
      handle->pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_24;
      break;
   case 32:
      handle->pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_32;
      handle->pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_32;
      break;
   case 16:
   default:
      handle->pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
      handle->pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
      break;
   }

   switch (rate)
   {
   case 8000:
      handle->pcm.samplesPerSec = SL_SAMPLINGRATE_8;
      break;
   case 11025:
      handle->pcm.samplesPerSec = SL_SAMPLINGRATE_11_025;
      break;
   case 12000:
      handle->pcm.samplesPerSec = SL_SAMPLINGRATE_12;
      break;
   case 16000:
      handle->pcm.samplesPerSec = SL_SAMPLINGRATE_16;
      break;
   case 22050:
      handle->pcm.samplesPerSec = SL_SAMPLINGRATE_22_05;
      break;
   case 24000:
      handle->pcm.samplesPerSec = SL_SAMPLINGRATE_24;
      break;
   case 32000:
      handle->pcm.samplesPerSec = SL_SAMPLINGRATE_32;
      break;
   case 48000:
      handle->pcm.samplesPerSec = SL_SAMPLINGRATE_48;
      break;
   case 64000:
      handle->pcm.samplesPerSec = SL_SAMPLINGRATE_64;
      break;
   case 88200:
      handle->pcm.samplesPerSec = SL_SAMPLINGRATE_88_2;
      break;
   case 96000:
      handle->pcm.samplesPerSec = SL_SAMPLINGRATE_96;
      break;
   case 192000:
      handle->pcm.samplesPerSec = SL_SAMPLINGRATE_192;
      break;
   case 44100:
   default:
      handle->pcm.samplesPerSec = SL_SAMPLINGRATE_44_1;
      break;
   }
}

#define PCM_METADATA_VALUE_SIZE		32
static int
_sles_get_hw_settings(_driver_t *handle)
{
   SLMetadataExtractionItf md;
   int rv = AAX_FALSE;
   SLuint32 num;
   SLresult res;

   // https://searchcode.com/codesearch/view/41578782/
   md = handle->metadataItf;
   res = (*md)->GetItemCount(md, &num);
   if (res == SL_RESULT_SUCCESS)
   {
      union {
         SLMetadataInfo pcmMetaData;
         char withData[PCM_METADATA_VALUE_SIZE];
      } u;
      SLuint32 i;
      for (i=0; i<num; i++)
      {
         SLMetadataInfo *key = NULL;
         SLuint32 isize = 0;

         res = (*md)->GetKeySize(md, i, &isize);
         if (res == SL_RESULT_SUCCESS)
         {
            key = (SLMetadataInfo*)malloc(isize);
            if (key)
            {
               SLMetadataInfo *value = NULL;
               SLuint32 vsize = 0;

               res = (*md)->GetValueSize(md, i, &vsize);
               value = malloc(vsize);
               if (value)
               {
                  res = (*md)->GetKey(md, i, isize, key);
                  if (res == SL_RESULT_SUCCESS)
                  {
                     char *p = (char*)key->data;
                     if (!strcmp(p, ANDROID_KEY_PCMFORMAT_NUMCHANNELS))
                     {
                        res = (*md)->GetValue(md, i, PCM_METADATA_VALUE_SIZE,
                                                  &u.pcmMetaData);
                        if (res == SL_RESULT_SUCCESS) {
                           handle->pcm.numChannels = *u.pcmMetaData.data;
                           handle->no_tracks = handle->pcm.numChannels;
                        }
                     }
                     else if (!strcmp(p, ANDROID_KEY_PCMFORMAT_SAMPLERATE))
                     {
                        res = (*md)->GetValue(md, i, PCM_METADATA_VALUE_SIZE,
                                                  &u.pcmMetaData);
                        if (res == SL_RESULT_SUCCESS) {
                           handle->pcm.samplesPerSec = *u.pcmMetaData.data;
                           handle->frequency_hz = handle->pcm.samplesPerSec;
                        }
                     }
                     else if (!strcmp(p, ANDROID_KEY_PCMFORMAT_BITSPERSAMPLE))
                     {
                        res = (*md)->GetValue(md, i, PCM_METADATA_VALUE_SIZE,
                                                  &u.pcmMetaData);
                        if (res == SL_RESULT_SUCCESS) {
                           handle->pcm.bitsPerSample = *u.pcmMetaData.data;
                           handle->bits_sample = handle->pcm.bitsPerSample;
                        }
                     }
                     else if (!strcmp(p, ANDROID_KEY_PCMFORMAT_CONTAINERSIZE))
                     {
                        res = (*md)->GetValue(md, i, PCM_METADATA_VALUE_SIZE,
                                                  &u.pcmMetaData);
                        if (res == SL_RESULT_SUCCESS) {
                           handle->pcm.containerSize = *u.pcmMetaData.data;
                        }
                     }
                     else if (!strcmp(p, ANDROID_KEY_PCMFORMAT_CHANNELMASK))
                     {
                        res = (*md)->GetValue(md, i, PCM_METADATA_VALUE_SIZE,
                                                  &u.pcmMetaData);
                        if (res == SL_RESULT_SUCCESS) {
                           handle->pcm.channelMask = *u.pcmMetaData.data;
                        }
                     }
                     else if (!strcmp(p, ANDROID_KEY_PCMFORMAT_ENDIANNESS))
                     {
                        res = (*md)->GetValue(md, i, PCM_METADATA_VALUE_SIZE,
                                                  &u.pcmMetaData);
                        if (res == SL_RESULT_SUCCESS) {
                           handle->pcm.endianness = *u.pcmMetaData.data;
                        }  
                     }
                  }
                  free(value);
               }
               free(key);
            }
         }
      }
   } 

   return rv;
}

/*
 * https://gist.github.com/hrydgard/3072540
 * This callback handler is called every time a buffer finishes playing
 * and is a replacement for the wait-for-enough-space threads like
 * _aaxALSADriverThread
 *
 * Never make a syscall or lock a synchronization object inside the buffer
 * callback. If you must synchronize, use a lock-free structure. For best
 * results, use a completely wait-free structure such as a single-reader
 * single-writer ring buffer. Loads of developers get this wrong and end
 * up with glitches that are unpredictable and hard to debug.
 */
static void
_aaxSLESDriverCallback(SLAndroidSimpleBufferQueueItf queue, void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int16_t *next;
   SLresult res;

   // First push the buffer
   next =  handle->buffer[handle->buffer_pos];
   res = (*handle->bufferQItf)->Enqueue(handle->bufferQItf, next,
                                        handle->no_frames*sizeof(int16_t));
   // Comment from sample code:
   // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
   // which for this code example would indicate a programming error
   if (res == SL_RESULT_SUCCESS)
   {
      if (++handle->buffer_pos == handle->max_buffers) {
         handle->buffer_pos = 0;
      }
   }

   // Then signal the thread to fill the next buffer
   _aaxSemaphoreRelease(handle->worker_start);
}

void *
_aaxSLESDriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   _intBufferData *dptr_sensor;
   const _aaxDriverBackend *be;
   _aaxRingBuffer *dest_rb;
   _aaxAudioFrame *mixer;
   _driver_t *be_handle;
   float delay_sec;
   char state;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
   }

   delay_sec = 1.0f/handle->info->period_rate;

   be = handle->backend.ptr;
   be_handle = (_driver_t *)handle->backend.handle;
   be_handle->handle = handle;

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

   _aaxMutexLock(handle->thread.signal.mutex);
   do
   {
      _aaxMutexUnLock(handle->thread.signal.mutex);
      _aaxSemaphoreWait(be_handle->worker_start);

      _aaxMutexLock(handle->thread.signal.mutex);
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

      if (TEST_FOR_TRUE(handle->thread.started) && _IS_PLAYING(handle)) {
         _aaxSoftwareMixerThreadUpdate(handle, dest_rb);
      }
   }
   while TEST_FOR_TRUE(handle->thread.started);

   handle->ringbuffer = NULL;
   be->destroy_ringbuffer(dest_rb);
   _aaxMutexUnLock(handle->thread.signal.mutex);

   return handle;
}

#endif // 0

