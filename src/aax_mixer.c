/*
 * Copyright 2007-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
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

#ifdef HAVE_LIBIO_H
#include <libio.h>              /* for NULL */
#endif
#include <string.h>		/* for calloc */
#include <assert.h>
#include <math.h>		/* for ceif */

#include <signal.h>
#ifdef SOLARIS /* needed with at least Solaris 8 */
# include <siginfo.h>
#endif


#include <arch.h>
#include <ringbuffer.h>

#include <base/gmath.h>
#include <base/threads.h>
#include <base/timer.h>		/* for msecSleep, etc */

#include "api.h"

static int _aaxMixerInit(_handle_t*);
static int _aaxMixerStart(_handle_t*);
static int _aaxMixerStop(_handle_t*);
static int _aaxMixerUpdate(_handle_t*);

AAX_API int AAX_APIENTRY
aaxMixerSetSetup(aaxConfig config, enum aaxSetupType type, unsigned int setup)
{
   int rv = AAX_FALSE;

   if (config == NULL)
   {
      switch(type)
      {
      case AAX_STEREO_EMITTERS:
         setup *= 2;
         /* break not needed */
      case AAX_MONO_EMITTERS:
         rv = (setup <= _oalRingBufferGetNoSources()) ? AAX_TRUE : AAX_FALSE;
         break;
      default:
         break;
      }
   }
   else
   {
      _handle_t *handle = get_handle(config);
      if (handle && !handle->handle)
      {
         _aaxMixerInfo* info = handle->info;
         switch(type)
         {
         case AAX_STEREO_EMITTERS:
            setup *= 2;
            /* break not needed */
         case AAX_MONO_EMITTERS:
            rv = (setup <= info->max_emitters) ? AAX_TRUE : AAX_FALSE;
            break;
         case AAX_FREQUENCY:
            if ((setup > 1000) && (setup <= _AAX_MAX_MIXER_FREQUENCY))
            {
               float iv = info->refresh_rate;
               iv = setup / (float)get_pow2((unsigned int)ceilf(setup / iv));
               info->frequency = (float)setup;
               info->refresh_rate = iv;
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_REFRESHRATE:
            if (((setup <= _AAX_MAX_MIXER_REFRESH_RATE)
                         && (handle->valid & HANDLE_ID))
                || (setup <= _AAX_MAX_MIXER_REFRESH_RATE_LT))
            {
               float update_hz = info->refresh_rate/info->update_rate;
               float fq = info->frequency;
               float iv = (float)setup;
               if (iv <= 5.0f) iv = 5.0f;

               iv = fq / (float)get_pow2((unsigned)ceilf(fq / iv));
               info->refresh_rate = iv;

               info->update_rate = (uint8_t)rintf(iv/update_hz);
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_UPDATERATE:
            if (((setup <= _AAX_MAX_MIXER_REFRESH_RATE)
                         && (handle->valid & HANDLE_ID))
                || (setup <= _AAX_MAX_MIXER_REFRESH_RATE_LT))
            {
               info->update_rate = (uint8_t)rintf(info->refresh_rate/setup);
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_TRACKSIZE:
            {
               float fq = info->frequency;
               float iv = fq*sizeof(int32_t)/setup;
               if (iv <= 5.0f) iv = 5.0f;

               iv = fq / (float)get_pow2((unsigned)ceilf(fq / iv));
               info->refresh_rate = iv;
               rv = AAX_TRUE;
            }
            break;
         case AAX_TRACKS:
            if (setup < _AAX_MAX_SPEAKERS)
            {
               info->no_tracks = setup;
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_FORMAT:
            if (setup < AAX_FORMAT_MAX)
            {
               info->format = setup;
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         default:
            _aaxErrorSet(AAX_INVALID_ENUM);
            break;
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   return rv;
}

AAX_API unsigned int AAX_APIENTRY
aaxMixerGetSetup(const aaxConfig config, enum aaxSetupType type)
{
   unsigned int rv = AAX_FALSE;

   if (config == NULL)
   {
      switch(type)
      {
      case AAX_MONO_EMITTERS:
         rv = _oalRingBufferGetNoSources();
         break;
      case AAX_STEREO_EMITTERS:
         rv = _oalRingBufferGetNoSources()/2;
         break;
      case AAX_AUDIO_FRAMES:
         rv = 0;
         break;
      default:
         break;
      }
   }
   else
   {
      _handle_t *handle = get_handle(config);
      if (handle)
      {
         if (type < AAX_SETUP_TYPE_MAX)
         {
            _aaxMixerInfo* info = handle->info;
            switch(type)
            {
            case AAX_MONO_EMITTERS:
               rv = info->max_emitters;
               break;
            case AAX_STEREO_EMITTERS:
               rv = info->max_emitters/2;
               break;
            case AAX_AUDIO_FRAMES:
               rv = VALID_HANDLE(handle) ? info->max_emitters : 0;
               break;
            case AAX_FREQUENCY:
               rv = (unsigned int)info->frequency;
               break;
            case AAX_REFRESHRATE:
               rv = (unsigned int)info->refresh_rate;
               break;
            case AAX_UPDATERATE:
                rv=(unsigned int)(info->refresh_rate/handle->info->update_rate);
                break;
            case AAX_LATENCY:
               if (handle->backend.driver)
               {
                  const _aaxDriverBackend *be = handle->backend.ptr;
                  float ltc = be->param(handle->backend.handle, DRIVER_LATENCY);
                  rv = (int)(ltc*1e6);
               }
               break;
            case AAX_TRACKSIZE:
            {
               _oalRingBuffer *rb = handle->ringbuffer;
               int bps = _oalRingBufferGetParami(rb, RB_BYTES_SAMPLE);

               rv = (unsigned int)(info->frequency*bps/info->refresh_rate);
               break;
            }
            case AAX_TRACKS:
               rv = info->no_tracks;
               break;
            case AAX_FORMAT:
               rv = info->format;
               break;
            default:
               _aaxErrorSet(AAX_INVALID_ENUM);
            }
         }
         else if ((type >= AAX_PEAK_VALUE_TRACK0
                    && type <= AAX_PEAK_VALUE_TRACK7)
                  || (type >= AAX_AVERAGE_VALUE_TRACK0
                       && type <= AAX_AVERAGE_VALUE_TRACK7))
         {
            unsigned int track = type & 0xFF;
            if (track < _AAX_MAX_SPEAKERS)
            {
               _oalRingBuffer *rb = handle->ringbuffer;
               if (rb)
               {
                  if (type <= AAX_PEAK_VALUE_TRACK7) {
                     rv = rb->peak[track];
                  } else {
                     rv = rb->average[track];
                  }
               }
            }
            else {
               _aaxErrorSet(AAX_INVALID_ENUM);
            }
         }
         else if ((type >= AAX_COMPRESSION_VALUE_TRACK0
                       && type <= AAX_COMPRESSION_VALUE_TRACK7))
         {
            unsigned int track = type & 0xFF;
            if (track < _AAX_MAX_SPEAKERS)
            {
               const _intBufferData* dptr;
               dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
               if (dptr)
               {
                  _sensor_t* sensor = _intBufGetDataPtr(dptr);
                  _aaxAudioFrame *mixer = sensor->mixer;
                  _oalRingBufferLFOInfo *lfo;

                  lfo = _FILTER_GET2D_DATA(mixer, DYNAMIC_GAIN_FILTER);
                  if (lfo) {
                     rv = 256*32768*lfo->compression[track];
                  }
               
                  _intBufReleaseData(dptr, _AAX_SENSOR);
               }
            }
         }
         else if ((type >= AAX_GATE_ENABLED_TRACK0)
               && (type <= AAX_GATE_ENABLED_TRACK7))
         {
            unsigned int track = type & 0xFF;
            if (track < _AAX_MAX_SPEAKERS)
            {
               const _intBufferData* dptr;
               dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
               if (dptr)
               {
                  _sensor_t* sensor = _intBufGetDataPtr(dptr);
                  _aaxAudioFrame *mixer = sensor->mixer;
                  _oalRingBufferLFOInfo *lfo;
                  int state;

                  lfo = _FILTER_GET2D_DATA(mixer, DYNAMIC_GAIN_FILTER);
                  state =_FILTER_GET_STATE(mixer->props2d, DYNAMIC_GAIN_FILTER);
                  if (lfo && ((state & ~AAX_INVERSE) == AAX_ENVELOPE_FOLLOW))
                  {
                     if (lfo->average[track] <= lfo->gate_threshold) {
                        rv = AAX_TRUE;
                     }
                  }

                  _intBufReleaseData(dptr, _AAX_SENSOR);
               }
            }
         }
         else {
            _aaxErrorSet(AAX_INVALID_ENUM);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      }
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerSetFilter(aaxConfig config, aaxFilter f)
{
   _handle_t* handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle)
   {
      _filter_t* filter = get_filter(f);
      if (filter)
      {
         const _intBufferData* dptr;
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            int type = filter->pos;
            switch (filter->type)
            {
            case AAX_VOLUME_FILTER:
            {
               _aaxAudioFrame *mixer = sensor->mixer;
               _oalRingBuffer2dProps *p2d = mixer->props2d;
               _FILTER_SET(p2d, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
                   /* gain min and gain max are fixed values for the mixer   */
               /* _FILTER_SET(p2d, type, 1, _FILTER_GET_SLOT(filter, 0, 1)); */
               /* _FILTER_SET(p2d, type, 2, _FILTER_GET_SLOT(filter, 0, 2)); */
               /* _FILTER_SET(p2d, type, 3, _FILTER_GET_SLOT(filter, 0, 3)); */
               _FILTER_SET_STATE(p2d, type, _FILTER_GET_SLOT_STATE(filter));
               _FILTER_SWAP_SLOT_DATA(p2d, type, filter, 0);
               rv = AAX_TRUE;
               break;
            }
            case AAX_COMPRESSOR:
            case AAX_DYNAMIC_GAIN_FILTER:
            case AAX_TIMED_GAIN_FILTER:
            {
               _aaxAudioFrame *mixer = sensor->mixer;
               _oalRingBuffer2dProps *p2d = mixer->props2d;
               _FILTER_SET(p2d, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
               _FILTER_SET(p2d, type, 1, _FILTER_GET_SLOT(filter, 0, 1));
               _FILTER_SET(p2d, type, 2, _FILTER_GET_SLOT(filter, 0, 2));
               _FILTER_SET(p2d, type, 3, _FILTER_GET_SLOT(filter, 0, 3));
               _FILTER_SET_STATE(p2d, type, _FILTER_GET_SLOT_STATE(filter));
               _FILTER_SWAP_SLOT_DATA(p2d, type, filter, 0);
               if (filter->type == AAX_DYNAMIC_GAIN_FILTER ||
                   filter->type == AAX_COMPRESSOR)
               {
                  p2d->final.gain_lfo = 1.0f;
               }
               rv = AAX_TRUE;
               break;
            }
            case AAX_EQUALIZER:
            case AAX_GRAPHIC_EQUALIZER:
               type = EQUALIZER_HF;
               _FILTER_SET(sensor, type, 0, _FILTER_GET_SLOT(filter, 1, 0));
               _FILTER_SET(sensor, type, 1, _FILTER_GET_SLOT(filter, 1, 1));
               _FILTER_SET(sensor, type, 2, _FILTER_GET_SLOT(filter, 1, 2));
               _FILTER_SET(sensor, type, 3, _FILTER_GET_SLOT(filter, 1, 3));
               _FILTER_SWAP_SLOT_DATA(sensor, type, filter, 1);

               type = EQUALIZER_LF;
               /* frees both EQUALIZER_LF and EQUALIZER_HF */
               _FILTER_SET(sensor, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
               _FILTER_SET(sensor, type, 1, _FILTER_GET_SLOT(filter, 0, 1));
               _FILTER_SET(sensor, type, 2, _FILTER_GET_SLOT(filter, 0, 2));
               _FILTER_SET(sensor, type, 3, _FILTER_GET_SLOT(filter, 0, 3));
               _FILTER_SWAP_SLOT_DATA(sensor, type, filter, 0);
               _FILTER_SET_STATE(sensor, type, _FILTER_GET_SLOT_STATE(filter));
               rv = AAX_TRUE;
               break;
            default:
               _aaxErrorSet(AAX_INVALID_ENUM);
            }
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API const aaxFilter AAX_APIENTRY
aaxMixerGetFilter(const aaxConfig config, enum aaxFilterType type)
{
   _handle_t* handle = get_handle(config);
   aaxFilter rv = AAX_FALSE;
   if (handle)
   {
      const _intBufferData* dptr;
      switch(type)
      {
      case AAX_COMPRESSOR:
      case AAX_VOLUME_FILTER:
      case AAX_TIMED_GAIN_FILTER:
      case AAX_DYNAMIC_GAIN_FILTER:
      case AAX_GRAPHIC_EQUALIZER:
      case AAX_EQUALIZER:
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = sensor->mixer;
            rv = new_filter_handle(handle->info, type, mixer->props2d,
                                                       mixer->props3d);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}


AAX_API int AAX_APIENTRY
aaxMixerSetEffect(aaxConfig config, aaxEffect e)
{
   _handle_t* handle = get_handle(config);
   int rv = AAX_FALSE;
   if (handle)
   {
      _filter_t* effect = get_effect(e);
      if (effect)
      {
         const _intBufferData* dptr;
         switch (effect->type)
         {
         case AAX_PITCH_EFFECT:
         case AAX_DYNAMIC_PITCH_EFFECT:
         case AAX_DISTORTION_EFFECT:
         case AAX_PHASING_EFFECT:
         case AAX_CHORUS_EFFECT:
         case AAX_FLANGING_EFFECT:
         case AAX_REVERB_EFFECT:
            dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               _aaxAudioFrame *mixer = sensor->mixer;
               _oalRingBuffer2dProps *p2d = mixer->props2d;
               int type = effect->pos;
               _EFFECT_SET(p2d, type, 0, _EFFECT_GET_SLOT(effect, 0, 0));
               _EFFECT_SET(p2d, type, 1, _EFFECT_GET_SLOT(effect, 0, 1));
               _EFFECT_SET(p2d, type, 2, _EFFECT_GET_SLOT(effect, 0, 2));
               _EFFECT_SET(p2d, type, 3, _EFFECT_GET_SLOT(effect, 0, 3));
               _EFFECT_SET_STATE(p2d, type, _EFFECT_GET_SLOT_STATE(effect));
               _EFFECT_SWAP_SLOT_DATA(p2d, type, effect, 0);
               if ((enum aaxEffectType)effect->type == AAX_DYNAMIC_PITCH_EFFECT)
               {
                  p2d->final.pitch_lfo = 1.0f;
               }
               _intBufReleaseData(dptr, _AAX_SENSOR);
               rv = AAX_TRUE;
            }
            break;
         default:
            _aaxErrorSet(AAX_INVALID_ENUM);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API const aaxEffect AAX_APIENTRY
aaxMixerGetEffect(const aaxConfig config, enum aaxEffectType type)
{
   _handle_t* handle = get_handle(config);
   aaxEffect rv = AAX_FALSE;
   if (handle)
   {
      const _intBufferData* dptr;
      switch(type)
      {
      case AAX_DYNAMIC_PITCH_EFFECT:
      case AAX_DISTORTION_EFFECT:
      case AAX_PHASING_EFFECT:
      case AAX_CHORUS_EFFECT:
      case AAX_FLANGING_EFFECT:
      case AAX_PITCH_EFFECT:
      case AAX_REVERB_EFFECT:
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = sensor->mixer;
            rv = new_effect_handle(handle->info, type, mixer->props2d,
                                                       mixer->props3d);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerSetState(aaxConfig config, enum aaxState state)
{
   int rv = AAX_FALSE;
   _handle_t* handle;

   if (state == AAX_INITIALIZED) {
      handle = get_handle(config);
   } else {
      handle = get_valid_handle(config);
   }

   if (handle)
   {
      switch (state)
      {
      case AAX_UPDATE:
         rv = _aaxMixerUpdate(handle);
         break;
      case AAX_SUSPENDED:
         _SET_PAUSED(handle);
         rv = AAX_TRUE;
         break;
      case AAX_STANDBY:
         _SET_STANDBY(handle);
         rv = AAX_TRUE;
         break;
      case AAX_INITIALIZED:
         rv = _aaxMixerInit(handle);
         break;
      case AAX_PLAYING:
         rv = _aaxMixerStart(handle);
         if (rv) _SET_PLAYING(handle);
         break;
      case AAX_STOPPED:
         rv = _aaxMixerStop(handle);
         if (rv) _SET_PROCESSED(handle);
         break;
      default:
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API enum aaxState AAX_APIENTRY
aaxMixerGetState(aaxConfig config)
{
   enum aaxState rv = AAX_INITIALIZED;
   _handle_t* handle;

   handle = get_valid_handle(config);
   if (handle)
   {
      if (_IS_PLAYING(handle)) rv = AAX_PLAYING;
      else if (_IS_PROCESSED(handle)) rv = AAX_STOPPED;
      else if (_IS_STANDBY(handle)) rv = AAX_STANDBY;
      else if (_IS_PAUSED(handle)) rv = AAX_SUSPENDED;
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerRegisterSensor(const aaxConfig config, const aaxConfig s)
{
   _handle_t* handle = get_write_handle(config);
   int rv = AAX_FALSE;
   if (handle && VALID_HANDLE(handle))
   {
      _handle_t* sframe = get_read_handle(s);
      if (sframe && !sframe->thread.started && (sframe != handle))
      {
         if (sframe->pos == UINT_MAX)
         {
            unsigned int pos = UINT_MAX;
            _intBufferData *dptr;

            dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               _aaxAudioFrame *mixer = sensor->mixer;
               _intBuffers *hs = mixer->devices;

               if (hs == NULL)
               {
                  unsigned int res;
                  res = _intBufCreate(&mixer->devices, _AAX_DEVICE);
                  if (res != UINT_MAX) {
                     hs = mixer->devices;
                  }
               }

               if (hs && (mixer->no_registered < mixer->info->max_registered))
               {
                  aaxBuffer buf; /* clear the sensors buffer queue */
                  while ((buf = aaxSensorGetBuffer(sframe)) != NULL) {
                     aaxBufferDestroy(buf);
                  }
                  pos = _intBufAddData(hs, _AAX_DEVICE, sframe);
                  mixer->no_registered++;
               }
               else {
                  _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
               }
               _intBufReleaseData(dptr, _AAX_SENSOR);
            }

            if (dptr && pos != UINT_MAX)
            {
               _intBufferData *dptr_sframe;

               dptr_sframe = _intBufGet(sframe->sensors, _AAX_SENSOR, 0);
               if (dptr_sframe)
               {
                  _sensor_t *sframe_sensor = _intBufGetDataPtr(dptr_sframe);
                  _oalRingBuffer3dProps *mp3d, *sp3d;
                  _aaxAudioFrame *mixer, *submix;
                  _oalRingBuffer *rb;

                  dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
                  if (dptr)
                  {
                     _sensor_t *sensor = _intBufGetDataPtr(dptr);
                     mixer = sensor->mixer;
                     submix = sframe_sensor->mixer;

                     mp3d = mixer->props3d;
                     sp3d = submix->props3d;

                     submix->info->frequency = mixer->info->frequency;
                     while (submix->info->frequency > 48000.0f) {
                        submix->info->frequency /= 2.0f;
                     }
                     submix->info->refresh_rate = mixer->info->refresh_rate;
                     submix->info->update_rate = mixer->info->update_rate;
                     submix->dist_delaying = mixer->dist_delaying;
                     if (_FILTER_GET_DATA(sp3d, DISTANCE_FILTER) == NULL) {
                        _FILTER_COPY_DATA(sp3d, mp3d, DISTANCE_FILTER);
                     }

                     if (_EFFECT_GET_DATA(sp3d, VELOCITY_EFFECT) == NULL)
                     {
                        _EFFECT_COPY(sp3d, mp3d, VELOCITY_EFFECT,
                                                 AAX_SOUND_VELOCITY);
                        _EFFECT_COPY(sp3d, mp3d, VELOCITY_EFFECT,
                                                 AAX_DOPPLER_FACTOR);
                        _EFFECT_COPY_DATA(sp3d, mp3d, VELOCITY_EFFECT);
                     }
                     _intBufReleaseData(dptr, _AAX_SENSOR);

                     sframe->handle = handle;
                     sframe->pos = pos;
                     submix->refcount++;
                     submix->thread = AAX_TRUE;

                     rb = submix->ringbuffer;
                     if (!rb)
                     {
                        rb = _oalRingBufferCreate(DELAY_EFFECTS_TIME);
                        submix->ringbuffer = rb;
                     }

                     if (rb)
                     {
                        _aaxMixerInfo* info = submix->info;
                        const _aaxDriverBackend *be;
                        unsigned int pos;
                        float delay_sec;

                        be = _aaxGetDriverBackendLoopback(&pos);
                        delay_sec = 1.0f / info->refresh_rate;

                        _oalRingBufferSetFormat(rb, be->codecs, AAX_PCM24S);
                        _oalRingBufferSetParamf(rb, RB_FREQUENCY, info->frequency);
                        _oalRingBufferSetParami(rb, RB_NO_TRACKS, info->no_tracks);

                        /* create a ringbuffer with a bit of overrun space */
                        _oalRingBufferSetParamf(rb, RB_DURATION_SEC, delay_sec*1.0f);

                        /* Do not initialize the RinBuffer yet, this would
                         * assign memory to rb->tracks before the final
                         * ringbuffer setup is know 
                        _oalRingBufferInit(rb, AAX_TRUE);
                         */

                        /* 
                         * Now set the actual duration, this will not alter the
                         * allocated space since it is lower that the initial
                         * duration.
                         */
                        _oalRingBufferSetParamf(rb, RB_DURATION_SEC, delay_sec);
                        _oalRingBufferStart(rb);
                     }

                     _intBufReleaseData(dptr_sframe, _AAX_SENSOR);
                     rv = AAX_TRUE;
                  }
               }
            }
            else {
               _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            }
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_STATE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerDeregisterSensor(const aaxConfig config, const aaxConfig s)
{
   _handle_t* handle = get_write_handle(config);
   int rv = AAX_FALSE;
   if (handle)
   {
      _handle_t* sframe = get_handle(s);
      if (sframe && sframe->pos != UINT_MAX)
      {
         _intBufferData *dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         _intBufferData *dptr_sframe;
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = sensor->mixer;
            _intBuffers *hs = mixer->devices;
            _intBufRemove(hs, _AAX_DEVICE, sframe->pos, AAX_FALSE);
            mixer->no_registered--;
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }

         dptr_sframe = _intBufGet(sframe->sensors, _AAX_SENSOR, 0);
         if (dptr_sframe)
         {
            _sensor_t* sframe_sensor = _intBufGetDataPtr(dptr_sframe);
            sframe_sensor->mixer->refcount--;
            _intBufReleaseData(dptr_sframe, _AAX_SENSOR);

            sframe->handle = NULL;
            sframe->pos = UINT_MAX;
            rv = AAX_TRUE;
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}


AAX_API int AAX_APIENTRY
aaxMixerRegisterEmitter(const aaxConfig config, const aaxEmitter em)
{
   _handle_t* handle = get_write_handle(config);
   int rv = AAX_FALSE;
   if (handle)
   {
      _emitter_t* emitter = get_emitter_unregistered(em);
      if (emitter && emitter->pos == UINT_MAX)
      {
         _aaxEmitter *src = emitter->source;
         unsigned int pos, positional;
         _intBufferData *dptr;

         positional = _IS_POSITIONAL(src);

         pos = UINT_MAX;
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = sensor->mixer;
            _intBuffers *he;

            if (!positional) {
               he = mixer->emitters_2d;
            } else {
               he = mixer->emitters_3d;
            }

            if (mixer->no_registered < mixer->info->max_registered)
            {
               if (_oalRingBufferGetSource())
               {
                  mixer->no_registered++;
                  pos = _intBufAddData(he, _AAX_EMITTER, emitter);
               }
            }
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }

         if (pos != UINT_MAX)
         {
            _aaxEmitter *src = emitter->source;
            _intBufferData *dptr;

            emitter->handle = handle;
            emitter->pos = pos;

            /*
             * The mixer frequency is not know by the frequency filter
             * (which needs it to properly compute the filter coefficients)
             * until it has been registered to the mixer.
             * So we need to update the filter coefficients one more time.
             */
            if (_FILTER_GET2D_DATA(emitter->source, FREQUENCY_FILTER)) 
            {
               aaxFilter f = aaxEmitterGetFilter(emitter, AAX_FREQUENCY_FILTER);
               _filter_t *filter = (_filter_t *)f;
               aaxFilterSetState(f, filter->slot[0]->state);
               aaxEmitterSetFilter(emitter, f);
            }

            src->info = handle->info;
            if (src->update_rate == 0) {
               src->update_rate = handle->info->update_rate;
            }
            src->update_ctr = src->update_rate;

            dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _oalRingBuffer3dProps *mp3d, *ep3d = src->props3d;
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               _aaxAudioFrame *mixer = sensor->mixer;

               if (_oalRingBufferIsValid(handle->ringbuffer)) {
                  src->props2d->delay_sec =
                   _oalRingBufferGetParamf(handle->ringbuffer, RB_DURATION_SEC);
               }

               if (positional)
               {
                  mp3d = mixer->props3d;
                  if (mixer->dist_delaying) {
                     _PROP_DISTDELAY_SET_DEFINED(src->props3d);
                  }

                  if (_FILTER_GET_DATA(ep3d, DISTANCE_FILTER) == NULL) {
                     _FILTER_COPY_DATA(ep3d, mp3d, DISTANCE_FILTER);
                  }

                  if (_EFFECT_GET_DATA(ep3d, VELOCITY_EFFECT) == NULL)
                  {
                     _EFFECT_COPY(ep3d,mp3d,VELOCITY_EFFECT,AAX_SOUND_VELOCITY);
                     _EFFECT_COPY(ep3d,mp3d,VELOCITY_EFFECT,AAX_DOPPLER_FACTOR);
                     _EFFECT_COPY_DATA(ep3d, mp3d, VELOCITY_EFFECT);
                  }
               }
               _intBufReleaseData(dptr, _AAX_SENSOR);
            }
            rv = AAX_TRUE;
         }
         else {
            _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerDeregisterEmitter(const aaxConfig config, const aaxEmitter em)
{
   _handle_t* handle = get_write_handle(config);
   int rv = AAX_FALSE;
   if (handle)
   {
      _emitter_t* emitter = get_emitter(em);
      if (emitter && emitter->pos != UINT_MAX)
      {
         _aaxEmitter *src = emitter->source;
         _intBufferData *dptr;

         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = sensor->mixer;
            _intBuffers *he;
            void *ptr;

            if (_IS_POSITIONAL(src))
            {
               he = mixer->emitters_3d;
               _PROP_DISTDELAY_CLEAR_DEFINED(src->props3d);
            }
            else {
               he = mixer->emitters_2d;
            }

            ptr = _intBufRemove(he, _AAX_EMITTER, emitter->pos, AAX_FALSE);
            if (ptr)
            {
               _oalRingBufferPutSource();
               mixer->no_registered--;
               emitter->handle = NULL;
               emitter->pos = UINT_MAX;
               rv = AAX_TRUE;
            }
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
      put_emitter(emitter);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerRegisterAudioFrame(const aaxConfig config, const aaxFrame f)
{
   _handle_t* handle = get_write_handle(config);
   int rv = AAX_FALSE;
   if (handle && VALID_HANDLE(handle))
   {
      _frame_t* frame = get_frame(f);
      if (frame && !frame->handle && !frame->thread.started)
      {
         if (frame->pos == UINT_MAX)
         {
            unsigned int pos = UINT_MAX;
            _intBufferData *dptr;

            dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               _aaxAudioFrame *mixer = sensor->mixer;
               _intBuffers *hf = mixer->frames;

               if (hf == NULL)
               {
                  unsigned int res = _intBufCreate(&mixer->frames, _AAX_FRAME);
                  if (res != UINT_MAX) {
                     hf = mixer->frames;
                  }
               }

               if (hf && (mixer->no_registered < mixer->info->max_registered))
               {
                  aaxBuffer buf; /* clear the frames buffer queue */
                  while ((buf = aaxAudioFrameGetBuffer(frame)) != NULL) {
                     aaxBufferDestroy(buf);
                  }
                  pos = _intBufAddData(hf, _AAX_FRAME, frame);
                  mixer->no_registered++;
               }
               else {
                  _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
               }
               _intBufReleaseData(dptr, _AAX_SENSOR);
            }
            
            if (dptr && pos != UINT_MAX)
            {
               _oalRingBuffer3dProps *mp3d, *fp3d;
               _aaxAudioFrame *mixer, *submix;

               dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
               if (dptr)
               {
                  _sensor_t *sensor = _intBufGetDataPtr(dptr);
                  mixer = sensor->mixer;
                  submix = frame->submix;

                  mp3d = mixer->props3d;
                  fp3d = submix->props3d;

                  submix->dist_delaying = mixer->dist_delaying;
                  if (_FILTER_GET_DATA(fp3d, DISTANCE_FILTER) == NULL) {
                     _FILTER_COPY_DATA(fp3d, mp3d, DISTANCE_FILTER);
                  }

                  if (_EFFECT_GET_DATA(fp3d, VELOCITY_EFFECT) == NULL)
                  {
                     _EFFECT_COPY(fp3d, mp3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY);
                     _EFFECT_COPY(fp3d, mp3d, VELOCITY_EFFECT, AAX_DOPPLER_FACTOR);
                     _EFFECT_COPY_DATA(fp3d, mp3d, VELOCITY_EFFECT);
                  }
                  _intBufReleaseData(dptr, _AAX_SENSOR);
                  rv = AAX_TRUE;

                  submix->refcount++;
#if THREADED_FRAMES
                  submix->thread = AAX_TRUE;
#else
                  submix->thread = AAX_FALSE;
#endif
                  frame->handle = handle;
                  frame->pos = pos;
               }
            }
            else {
               _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            }
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }

         /* No need to put the frame since it was not registered yet.
          * This means there is no lock to unlock 
          * put_frame(frame);
          */
      }
      else
      {
         if (frame->handle) put_frame(frame);
         _aaxErrorSet(AAX_INVALID_STATE);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerDeregisterAudioFrame(const aaxConfig config, const aaxFrame f)
{
   _handle_t* handle = get_write_handle(config);
   int rv = AAX_FALSE;
   if (handle)
   {
      _frame_t* frame = get_frame(f);
      if (frame && frame->pos != UINT_MAX)
      {
         _intBufferData *dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         _aaxAudioFrame *submix = frame->submix;
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = sensor->mixer;
            _intBuffers *hf = mixer->frames;
            _intBufRemove(hf, _AAX_FRAME, frame->pos, AAX_FALSE);
            mixer->no_registered--;
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }

         submix->refcount--;
         submix->thread = AAX_FALSE;
         frame->handle = NULL;
         frame->pos = UINT_MAX;
         rv = AAX_TRUE;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
      put_frame(frame);
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}


/* -------------------------------------------------------------------------- */

static int
_aaxMixerInit(_handle_t *handle)
{
   int res = AAX_FALSE;
   const _aaxDriverBackend *be = handle->backend.ptr;
   _aaxMixerInfo* info = handle->info;
   float refrate = info->refresh_rate;
   unsigned ch = info->no_tracks;
   float freq = info->frequency;
   int fmt = info->format;
   size_t frames;

   assert(be != 0);

   frames = (size_t)(freq/refrate);

   res = be->setup(handle->backend.handle, &frames, &fmt, &ch, &freq);
   if TEST_FOR_TRUE(res)
   {
      if (handle->valid || (freq <= _AAX_MAX_MIXER_FREQUENCY_LT))
      {
         handle->valid |= AAX_TRUE;
//       info->pitch = freq/info->frequency;
         info->frequency = freq;
         info->no_tracks = ch;
         info->format = fmt;

         /* only alter the refresh rate when not registered */
         if (!handle->handle) {
            info->refresh_rate = freq/frames;
         }
      }
      else {
         __aaxErrorSet(AAX_INVALID_SETUP, "aaxMixerSetState");
      }
   }
// else {
//    __aaxErrorSet(AAX_INVALID_SETUP, "aaxMixerSetState");
///}
   return res;
}

static int
_aaxMixerStart(_handle_t *handle)
{
   int rv = AAX_FALSE;

   if (handle && TEST_FOR_FALSE(handle->thread.started))
   {
      unsigned int ms;
      int r;

#if SET_PROCESS_PRIORITY
      _aaxProcessSetPriority(AAX_HIGH_PRIORITY);
#endif
#if SET_THREAD_PRIORITY
      _aaxThreadSetPriority(AAX_HIGH_PRIORITY);
#endif

      handle->thread.ptr = _aaxThreadCreate();
      assert(handle->thread.ptr != 0);

      handle->thread.condition = _aaxConditionCreate();
      assert(handle->thread.condition != 0);

      handle->thread.mutex = _aaxMutexCreate(0);
      assert(handle->thread.mutex != 0);

      handle->thread.started = AAX_TRUE;
      ms = rintf(1000/handle->info->refresh_rate);
      r = _aaxThreadStart(handle->thread.ptr, handle->backend.ptr->thread,
                          handle, ms);
      if (r == 0)
      {
         int p = 0;
         do
         {
            _intBufferData *dptr_sensor;

            msecSleep(100);
            dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr_sensor)
            {
//             _sensor_t* sensor = _intBufGetDataPtr(dptr_sensor);
               r = (handle->ringbuffer != 0);
               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
            }

            if (p++ > 500) break;
         }
         while (r == 0);

         if (r == 0)
         {
            _aaxErrorSet(AAX_TIMEOUT);
            handle->thread.started = AAX_FALSE;
         }
         else {
            rv = AAX_TRUE;
         }
      }   
      else {
         _aaxErrorSet(AAX_INVALID_STATE);
      }
   }
   else if (_IS_STANDBY(handle)) {
      rv = AAX_TRUE;
   }
   return rv;
}

static int
_aaxMixerStop(_handle_t *handle)
{
   int rv = AAX_FALSE;
   if TEST_FOR_TRUE(handle->thread.started)
   {
      handle->thread.started = AAX_FALSE;
      _aaxConditionSignal(handle->thread.condition);
      _aaxThreadJoin(handle->thread.ptr);

      _aaxConditionDestroy(handle->thread.condition);
      _aaxMutexDestroy(handle->thread.mutex);
      _aaxThreadDestroy(handle->thread.ptr);

      rv = AAX_TRUE;
   }

   return rv;
}

static int
_aaxMixerUpdate(_handle_t *handle)
{
   int rv = AAX_FALSE;
   if TEST_FOR_TRUE(handle->thread.started)
   {
      _aaxConditionSignal(handle->thread.condition);
      rv = AAX_TRUE;
   }

   return rv;
}


