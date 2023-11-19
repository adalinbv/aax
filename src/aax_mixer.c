/*
 * SPDX-FileCopyrightText: Copyright © 2007-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif
#include <stdio.h>              /* for NULL */
#include <assert.h>
#include <math.h>		/* for ceif */

#include <xml.h>

#include <base/gmath.h>
#include <base/threads.h>
#include <base/timer.h>		/* for msecSleep, etc */

#include <dsp/filters.h>
#include <dsp/effects.h>
#include <dsp/lfo.h>

#include "api.h"
#include "arch.h"
#include "stream/device.h"
#include "ringbuffer.h"

static int _aaxMixerInit(_handle_t*);
static int _aaxMixerStart(_handle_t*);
static int _aaxMixerStop(_handle_t*);
static int _aaxMixerUpdate(_handle_t*);
static int _mixerCreateEFFromAAXS(aaxConfig, _buffer_t*);
static aaxBuffer _aaxCreateBufferFromAAXS(aaxConfig, _buffer_t*, char*);

AAX_API int AAX_APIENTRY
aaxMixerSetSetup(aaxConfig config, enum aaxSetupType type, int64_t setup)
{
   int rv = false;

   if (config == NULL)
   {
      switch(type)
      {
      case AAX_STEREO_EMITTERS:
         setup *= 2;
         // intentional fallthrough
      case AAX_MONO_EMITTERS:
         rv = (setup <= _aaxGetNoEmitters(NULL)) ? true : false;
         break;
      case AAX_RELEASE_MODE:
         __release_mode = setup;
         rv = true;
         break;
      default:
         break;
      }
   }
   else
   {
      _handle_t *handle = get_handle(config, __func__);
      if (handle && !handle->parent)
      {
         _aaxMixerInfo* info = handle->info;
         switch(type)
         {
         case AAX_BALANCE:
             info->balance = ((float)setup-64.0f)/64.0f;
             rv = true;
             break;
         case AAX_STEREO_EMITTERS:
            setup *= 2;
            // intentional fallthrough
         case AAX_MONO_EMITTERS:
            info->max_emitters = setup;
            rv = true;
            break;
         case AAX_FREQUENCY:
            if ((setup > 1000) && (setup <= _AAX_MAX_MIXER_FREQUENCY))
            {
               float iv = info->refresh_rate;
               iv = setup / INTERVAL(setup / iv);
               info->frequency = (float)setup;
               info->refresh_rate = iv;
               info->period_rate = iv;
               rv = true;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_REFRESH_RATE:
            if (((setup <= _AAX_MAX_MIXER_REFRESH_RATE)
                         && (handle->valid & HANDLE_ID)))
            {
               float update_hz = info->refresh_rate/info->update_rate;
               float fq = info->frequency;
               float iv = (float)setup;
               if (iv <= 5.0f) iv = 5.0f;

               iv = fq / INTERVAL(fq / iv);
               info->refresh_rate = iv;
               info->period_rate = iv;

               info->update_rate = (uint8_t)rintf(iv/update_hz);
               rv = true;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_UPDATE_RATE:
            if (((setup <= _AAX_MAX_MIXER_REFRESH_RATE)
                         && (handle->valid & HANDLE_ID)))
            {
               info->update_rate = (uint8_t)rintf(info->refresh_rate/setup);
               rv = true;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_TRACK_SIZE:
            {
               float fq = info->frequency;
               float iv = fq*sizeof(int32_t)/setup;
               if (iv <= 5.0f) iv = 5.0f;

               iv = fq / INTERVAL(fq / iv);
               info->refresh_rate = iv;
               info->period_rate = iv;
               rv = true;
            }
            break;
         case AAX_BIT_RATE:
            if (setup > 0)
            {
                info->bitrate = setup;
                rv = true;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_FORMAT:
            if (setup < AAX_FORMAT_MAX)
            {
               info->format = setup;
               rv = true;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_TRACKS:
            if (setup < _AAX_MAX_SPEAKERS)
            {
               info->no_tracks = setup;
               rv = true;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_TRACK_LAYOUT:
            if ((setup < _AAX_MAX_SPEAKERS) || (setup == AAX_TRACK_ALL) ||
                (setup == AAX_TRACK_MIX))
            {
               info->track = setup;
               if (setup != AAX_TRACK_ALL) {
                  info->no_tracks = 1;
               }
               rv = true;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_RELEASE_MODE:
            __release_mode = setup;
            rv = true;
            break;
         case AAX_CAPABILITIES:
            switch(setup)
            {
            case AAX_RENDER_NORMAL:
               info->midi_mode = AAX_RENDER_NORMAL;
               _aaxMixerSetRendering(handle);
               rv = true;
               break;
            case AAX_RENDER_SYNTHESIZER:
               info->midi_mode = AAX_RENDER_SYNTHESIZER;
               _aaxMixerSetRendering(handle);
               rv = true;
               break;
            case AAX_RENDER_ARCADE:
               info->midi_mode = AAX_RENDER_ARCADE;
               _aaxMixerSetRendering(handle);
               rv = true;
               break;
            default:
               _aaxErrorSet(AAX_INVALID_PARAMETER);
               break;
            }
            break;
         default:
            _aaxErrorSet(AAX_INVALID_ENUM);
            break;
         }
      }
      else if (handle)	/* registered sensors */
      {
         _aaxMixerInfo* info = handle->info;
         switch(type)
         {
         case AAX_TRACK_LAYOUT:
            if ((setup < _AAX_MAX_SPEAKERS) || (setup == AAX_TRACK_ALL) ||
                (setup == AAX_TRACK_MIX))
            {
               info->track = setup;
               if (setup != AAX_TRACK_ALL) {
                  info->no_tracks = 1;
               }
               rv = true;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
        case AAX_BIT_RATE:
            if (setup > 0)
            {
                info->bitrate = setup;
                rv = true;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
        default:
            _aaxErrorSet(AAX_INVALID_ENUM);
            break;
         }
      }
   }
   return rv;
}

AAX_API int64_t AAX_APIENTRY
aaxMixerGetSetup(const aaxConfig config, enum aaxSetupType type)
{
   int64_t rv = false;

   if (type == AAX_CAPABILITIES) {
         return _aaxGetCapabilities(config);
   }

   if (config == NULL)
   {
      switch(type)
      {
      case AAX_MONO_EMITTERS:
         rv = _aaxGetNoEmitters(NULL);
         break;
      case AAX_STEREO_EMITTERS:
         rv = _aaxGetNoEmitters(NULL)/2;
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
      _handle_t *handle = get_handle(config, __func__);
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
            case AAX_REFRESH_RATE:
               rv = (unsigned int)info->period_rate;
               break;
            case AAX_UPDATE_RATE:
                rv=(unsigned int)(info->refresh_rate/handle->info->update_rate);
                break;
            case AAX_FRAME_TIMING:
            {
               const _intBufferData* dptr;
               dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
               if (dptr)
               {
                  _sensor_t* sensor = _intBufGetDataPtr(dptr);
                  if (sensor->mixer->capturing) {
                     rv = AAX_TO_INT(1.0f/info->period_rate);
                  } else {
                     rv = AAX_TO_INT(handle->elapsed);
                  }
                  _intBufReleaseData(dptr, _AAX_SENSOR);
               } else {
                  rv = AAX_TO_INT(handle->elapsed);
               }
               break;
            }
            case AAX_FORMAT:
               rv = info->format;
               break;
            case AAX_TRACK_SIZE:
            {
               _aaxRingBuffer *rb = handle->ringbuffer;
               int bps = rb->get_parami(rb, RB_BYTES_SAMPLE);

               rv = (unsigned int)(info->frequency*bps/info->period_rate);
               break;
            }
            case AAX_TRACKS:
               rv = info->no_tracks;
               break;
            case AAX_LATENCY:
            case AAX_BIT_RATE:
            case AAX_BUFFER_FILL:
            {
               const _aaxDriverBackend *be = handle->backend.ptr;
               float f;
               switch(type)
               {
               case AAX_BIT_RATE:
                  rv = be->param(handle->backend.handle, DRIVER_BITRATE);
                  if (!rv)
                  {
                     rv = aaxGetBitsPerSample(info->format);
                     rv *= info->frequency*info->no_tracks;
                  }
                  break;
               case AAX_LATENCY:
                  f = be->param(handle->backend.handle, DRIVER_LATENCY);
                  rv = (int)(f*1e6);
                  break;
               case AAX_BUFFER_FILL:
                  f = be->param(handle->backend.handle, DRIVER_BUFFER_FILL);
                  rv = (int)(f*100.0f);
                  break;
               default:
                  break;
               }
            }
            default:
               break;
            }
         }
         else if (type & AAX_SHARED_MODE)
         {
            if (handle->backend.driver)
            {
               const _aaxDriverBackend *be = handle->backend.ptr;
               float f;

               switch(type)
               {
               case AAX_LATENCY:
                  f = be->param(handle->backend.handle, DRIVER_LATENCY);
                  rv = (int)(f*1e6);
                  break;
               case AAX_NO_SAMPLES:
                  rv= be->param(handle->backend.handle, DRIVER_SAMPLE_DELAY);
                  break;
               case AAX_SHARED_MODE:
                  f = be->param(handle->backend.handle, DRIVER_SHARED_MODE);
                  rv = f ? true : false;
                  break;
               case AAX_TIMER_MODE:
                  f = be->param(handle->backend.handle, DRIVER_TIMER_MODE);
                  rv = f ? true : false;
                  break;
               case AAX_BATCHED_MODE:
                  f = be->param(handle->backend.handle, DRIVER_BATCHED_MODE);
                  rv = f ? true : false;
                  break;
               case AAX_SEEKABLE_SUPPORT:
                  f = be->param(handle->backend.handle,DRIVER_SEEKABLE_SUPPORT);
                  rv = f ? true : false;
                  break;
               case AAX_TRACKS_MIN:
                  f = be->param(handle->backend.handle, DRIVER_MIN_TRACKS);
                  rv = (int)f;
                  break;
               case AAX_TRACKS_MAX:
                  f = be->param(handle->backend.handle, DRIVER_MAX_TRACKS);
                  rv = (int)f;
                  break;
               case AAX_PERIODS_MIN:
                  f = be->param(handle->backend.handle, DRIVER_MIN_PERIODS);
                  rv = (int)f;
                  break;
               case AAX_PERIODS_MAX:
                  f = be->param(handle->backend.handle, DRIVER_MAX_PERIODS);
                  rv = (int)f;
                  break;
               case AAX_FREQUENCY_MIN:
                  f = be->param(handle->backend.handle,DRIVER_MIN_FREQUENCY);
                  rv = (int)f;
                  break;
               case AAX_FREQUENCY_MAX:
                  f = be->param(handle->backend.handle,DRIVER_MAX_FREQUENCY);
                  rv = (int)f;
                  break;
               case AAX_SAMPLES_MAX:
                  f = be->param(handle->backend.handle, DRIVER_MAX_SAMPLES);
                  rv = rintf(f);
                  break;
               default:
                  _aaxErrorSet(AAX_INVALID_ENUM);
                  break;
               }
            }
            else {
               _aaxErrorSet(AAX_INVALID_STATE);
            }
         }
         else if ((type & AAX_PEAK_VALUE) || (type & AAX_AVERAGE_VALUE))
         {
            unsigned int band = (type >> 8) & 0xF;
            unsigned int track = type & 0x3F;
            if (track < AAX_TRACK_MAX && band < AAX_MAX_BANDS)
            {
               if (band == 0)
               {
                  _aaxRingBuffer *rb = handle->ringbuffer;
                  if (rb)
                  {
                     if (type & AAX_TRACK_ALL) {
                        track = AAX_TRACK_MAX;
                     }

                     if (type & AAX_PEAK_VALUE) {
                        rv = rb->get_parami(rb, RB_PEAK_VALUE+track);
                     } else if (type & AAX_AVERAGE_VALUE) {
                        rv = rb->get_parami(rb, RB_AVERAGE_VALUE+track);
                     }
                  }
               }
               else
               {
                     const _intBufferData* dptr;
                  dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
                  if (dptr)
                  {
                     _sensor_t* sensor = _intBufGetDataPtr(dptr);
                     if (type & AAX_PEAK_VALUE) {
                        rv = sensor->peak[track][band];
                     } else if (type & AAX_AVERAGE_VALUE) {
                        rv = sensor->rms[track][band];
                     }
                     _intBufReleaseData(dptr, _AAX_SENSOR);
                  }
               }
            }
            else {
               _aaxErrorSet(AAX_INVALID_ENUM);
            }
         }
         else if (type & AAX_COMPRESSION_VALUE)
         {
            unsigned int track = type & 0x3F;
            if (track < _AAX_MAX_SPEAKERS)
            {
               const _intBufferData* dptr;
               dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
               if (dptr)
               {
                  _sensor_t* sensor = _intBufGetDataPtr(dptr);
                  _aaxAudioFrame *mixer = sensor->mixer;
                  _aaxLFOData *lfo;

                  lfo = _FILTER_GET2D_DATA(mixer, DYNAMIC_GAIN_FILTER);
                  if (lfo) {
                     rv = AAX_TO_INT(lfo->compression[track]);
                  }

                  _intBufReleaseData(dptr, _AAX_SENSOR);
               }
            }
         }
         else if (type & AAX_GATE_ENABLED)
         {
            unsigned int track = type & 0x3F;
            if (track < _AAX_MAX_SPEAKERS)
            {
               const _intBufferData* dptr;
               dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
               if (dptr)
               {
                  _sensor_t* sensor = _intBufGetDataPtr(dptr);
                  _aaxAudioFrame *mixer = sensor->mixer;
                  _aaxLFOData *lfo;
                  int state;

                  lfo = _FILTER_GET2D_DATA(mixer, DYNAMIC_GAIN_FILTER);
                  state =_FILTER_GET_STATE(mixer->props2d, DYNAMIC_GAIN_FILTER);
                  if (lfo && (state & AAX_SOURCE_MASK) == AAX_ENVELOPE_FOLLOW)
                  {
                     if (lfo->average[track] <= lfo->gate_threshold) {
                        rv = true;
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
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerSetMode(UNUSED(aaxConfig config), UNUSED(enum aaxModeType type), UNUSED(int mode))
{
   int rv = false;
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerGetMode(const aaxConfig config, UNUSED(enum aaxModeType type))
{
   _handle_t* handle = get_handle(config, __func__);
   int rv = __release_mode;
   if (handle) rv = handle->info->mode;
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerSetFilter(aaxConfig config, aaxFilter f)
{
   _handle_t* handle = get_handle(config, __func__);
   _filter_t* filter = get_filter(f);
   int rv = __release_mode;

   if (!rv)
   {
      if (!filter) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      const _intBufferData* dptr;
      dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
      if (dptr)
      {
         _sensor_t* sensor = _intBufGetDataPtr(dptr);
         _aaxAudioFrame *mixer = sensor->mixer;
         _aax2dProps *p2d = mixer->props2d;
         int type = filter->pos;

         switch (filter->type)
         {
         case AAX_VOLUME_FILTER:
            _FILTER_SET(p2d, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
                /* gain min and gain max are read-only for the mixer      */
            /* _FILTER_SET(p2d, type, 1, _FILTER_GET_SLOT(filter, 0, 1)); */
            /* _FILTER_SET(p2d, type, 2, _FILTER_GET_SLOT(filter, 0, 2)); */
            _FILTER_SET(p2d, type, 3, _FILTER_GET_SLOT(filter, 0, 3));
            _FILTER_SET_STATE(p2d, type, _FILTER_GET_SLOT_STATE(filter));
            _FILTER_SWAP_SLOT_DATA(p2d, type, filter, 0);
            break;
         case AAX_COMPRESSOR:
         case AAX_DYNAMIC_GAIN_FILTER:
         case AAX_BITCRUSHER_FILTER:
         case AAX_TIMED_GAIN_FILTER:
            _FILTER_SWAP_SLOT(p2d, type, filter, 0);
            if (filter->type == AAX_DYNAMIC_GAIN_FILTER ||
                filter->type == AAX_COMPRESSOR)
            {
               p2d->final.gain_lfo = 1.0f;
            }
            break;
         case AAX_EQUALIZER:
            _FILTER_SWAP_SLOT(sensor->mixer, EQUALIZER_LF, filter, 0);
            _FILTER_SWAP_SLOT(sensor->mixer, EQUALIZER_MF, filter, 1);
            _FILTER_SWAP_SLOT(sensor->mixer, EQUALIZER_HF, filter, 2);
            break;
         case AAX_GRAPHIC_EQUALIZER:
            _FILTER_SWAP_SLOT(sensor->mixer, EQUALIZER_LF, filter, 0);
            _FILTER_SWAP_SLOT(sensor->mixer, EQUALIZER_HF, filter, 2);
            break;
         default:
            _aaxErrorSet(AAX_INVALID_ENUM);
            rv = false;
         }
         _intBufReleaseData(dptr, _AAX_SENSOR);
      }
      else
      {
         _aaxErrorSet(AAX_INVALID_STATE);
         rv = false;
      }
   }

   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxMixerGetFilter(const aaxConfig config, enum aaxFilterType type)
{
   _handle_t* handle = get_handle(config, __func__);
   aaxFilter rv = false;
   if (handle)
   {
      const _intBufferData* dptr;
      switch(type)
      {
      case AAX_COMPRESSOR:
      case AAX_VOLUME_FILTER:
      case AAX_TIMED_GAIN_FILTER:
      case AAX_DYNAMIC_GAIN_FILTER:
      case AAX_BITCRUSHER_FILTER:
      case AAX_GRAPHIC_EQUALIZER:
      case AAX_EQUALIZER:
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = sensor->mixer;
            rv = new_filter_handle(config, type, mixer->props2d,
                                                 mixer->props3d);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   return rv;
}


AAX_API int AAX_APIENTRY
aaxMixerSetEffect(aaxConfig config, aaxEffect e)
{
   _handle_t* handle = get_handle(config, __func__);
   _effect_t* effect = get_effect(e);
   int rv = __release_mode;

   if (!rv)
   {
      if (!effect) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = true;
      }
   }

   if (rv && handle->sensors)
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
      case AAX_DELAY_EFFECT:
      case AAX_REVERB_EFFECT:
      case AAX_CONVOLUTION_EFFECT:
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = sensor->mixer;
            _aax2dProps *p2d = mixer->props2d;
            int type = effect->pos;
            _EFFECT_SWAP_SLOT(p2d, type, effect, 0);
            if ((enum aaxEffectType)effect->type == AAX_DYNAMIC_PITCH_EFFECT)
            {
               p2d->final.pitch_lfo = 1.0f;
            }
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
         else
         {
            _aaxErrorSet(AAX_INVALID_STATE);
            rv = false;
         }
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
         rv = false;
      }
   }
   return rv;
}

AAX_API aaxEffect AAX_APIENTRY
aaxMixerGetEffect(const aaxConfig config, enum aaxEffectType type)
{
   _handle_t* handle = get_handle(config, __func__);
   aaxEffect rv = false;
   if (handle)
   {
      const _intBufferData* dptr;
      switch(type)
      {
      case AAX_PITCH_EFFECT:
      case AAX_DYNAMIC_PITCH_EFFECT:
      case AAX_DISTORTION_EFFECT:
      case AAX_PHASING_EFFECT:
      case AAX_CHORUS_EFFECT:
      case AAX_FLANGING_EFFECT:
      case AAX_DELAY_EFFECT:
      case AAX_REVERB_EFFECT:
      case AAX_CONVOLUTION_EFFECT:
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = sensor->mixer;
            rv = new_effect_handle(config, type, mixer->props2d,
                                                 mixer->props3d);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerSetState(aaxConfig config, enum aaxState state)
{
   int rv = false;
   _handle_t* handle;

   if (state == AAX_INITIALIZED) {
      handle = get_handle(config, __func__);
   } else {
      handle = get_valid_handle(config, __func__);
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
         rv = true;
         break;
      case AAX_STANDBY:
         _SET_STANDBY(handle);
         rv = true;
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
   return rv;
}

AAX_API enum aaxState AAX_APIENTRY
aaxMixerGetState(aaxConfig config)
{
   enum aaxState rv = AAX_INITIALIZED;
   _handle_t* handle;

   handle = get_valid_handle(config, __func__);
   if (handle)
   {
      if (_IS_STOPPED(handle)) rv = AAX_STOPPED;
      else if (_IS_PROCESSED(handle)) rv = AAX_PROCESSED;
      else if (_IS_STANDBY(handle)) rv = AAX_STANDBY;
      else if (_IS_PAUSED(handle)) rv = AAX_SUSPENDED;
      else if (_IS_PLAYING(handle)) rv = AAX_PLAYING;
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerAddBuffer(aaxConfig config, aaxBuffer buf)
{
   _handle_t* handle = get_valid_handle(config, __func__);
   _buffer_t* buffer = get_buffer(buf, __func__);
   int rv = __release_mode;

   if (!rv && handle)
   {
      if (!buffer) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
      else if (!buffer->aaxs) {
         _aaxErrorSet(AAX_INVALID_STATE);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _intBufferData *dptr = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
      if (dptr)
      {
         _sensor_t* sensor = _intBufGetDataPtr(dptr);
         _aaxAudioFrame *mixer = sensor->mixer;
         if (!mixer->info->midi_mode) {
            rv = _mixerCreateEFFromAAXS(config, buffer);
         }
      }
      if (!buffer->root) {
         buffer->root = handle->root;
      }
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerRegisterSensor(const aaxConfig config, const aaxConfig s)
{
   _handle_t* handle = get_write_handle(config, __func__);
   int rv = false;
   if (handle && (VALID_MIXER(handle) || handle->registered_sensors <= 1))
   {
      _handle_t* sframe = get_read_handle(s, __func__);
      if (sframe && !sframe->thread.started && (sframe != handle))
      {
         if (sframe->mixer_pos == UINT_MAX)
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
                  _aaxErrorSet(AAX_ERROR_NONE);
                  pos = _intBufAddData(hs, _AAX_DEVICE, sframe);
                  mixer->no_registered++;
                  handle->registered_sensors++;
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
                  _aax3dProps *mp3d, *sp3d;
                  _aaxAudioFrame *mixer, *submix;
                  _aaxRingBuffer *rb;

                  dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
                  if (dptr)
                  {
                     _sensor_t *sensor = _intBufGetDataPtr(dptr);
                     _aaxMixerInfo *info;

                     mixer = sensor->mixer;
                     submix = sframe_sensor->mixer;
                     info = submix->info;

                     mp3d = mixer->props3d;
                     sp3d = submix->props3d;

                     info->frequency = mixer->info->frequency;
                     while (info->frequency > 48000.0f) {
                        info->frequency /= 2.0f;
                     }
                     info->update_rate = mixer->info->update_rate;
                     info->period_rate = mixer->info->period_rate;
                     info->refresh_rate = mixer->info->refresh_rate;
                     info->unit_m = mixer->info->unit_m;
                     if (_FILTER_GET_STATE(sp3d, DISTANCE_FILTER) == false)
                     {
                        _FILTER_COPY_STATE(sp3d, mp3d, DISTANCE_FILTER);
//                      _FILTER_COPY_DATA(sp3d, mp3d, DISTANCE_FILTER);
                        memcpy(sp3d->filter[DISTANCE_FILTER].data,
                               mp3d->filter[DISTANCE_FILTER].data,
                               sizeof(_aaxRingBufferDistanceData));
                     }
                     _aaxAudioFrameResetDistDelay(submix, mixer);

                     if (_EFFECT_GET_DATA(sp3d, VELOCITY_EFFECT) == NULL)
                     {
                        _EFFECT_COPY(sp3d, mp3d, VELOCITY_EFFECT,
                                                 AAX_SOUND_VELOCITY);
                        _EFFECT_COPY(sp3d, mp3d, VELOCITY_EFFECT,
                                                 AAX_DOPPLER_FACTOR);
                        _EFFECT_COPY_DATA(sp3d, mp3d, VELOCITY_EFFECT);
                     }
                     _EFFECT_COPY(sp3d, mp3d, VELOCITY_EFFECT,
                                              AAX_LIGHT_VELOCITY);
                     _intBufReleaseData(dptr, _AAX_SENSOR);

                     sframe->root = handle;
                     sframe->parent = handle;
                     sframe->mixer_pos = pos;
                     submix->refctr++;

                     rb = submix->ringbuffer;
                     if (!rb)
                     {
                        const _aaxDriverBackend *be = handle->backend.ptr;
                        float dt = DELAY_EFFECTS_TIME;

                        rb = be->get_ringbuffer(dt, info->mode);
                        submix->ringbuffer = rb;
                     }

                     if (rb)
                     {
                        float delay_sec = 1.0f / info->period_rate;
                        float duration;

                        rb->set_format(rb, AAX_PCM24S, true);
                        rb->set_paramf(rb, RB_FREQUENCY, info->frequency);
                        rb->set_parami(rb, RB_NO_TRACKS, info->no_tracks);

                        /* create a ringbuffer with a bit of overrun space */
                        duration = rb->get_paramf(rb, RB_DURATION_SEC);
                        rb->set_paramf(rb, RB_DURATION_SEC, delay_sec*1.0f);

                        /* Do not initialize the RinBuffer if it wasn't already,
                         * this would assign memory to rb->tracks before the
                         * final ringbuffer setup is know
                         */
                        if (duration > 0.0f) {
                           rb->init(rb, true);
                        }

                        /*
                         * Now set the actual duration, this will not alter the
                         * allocated space since it is lower that the initial
                         * duration.
                         */
                        rb->set_paramf(rb, RB_DURATION_SEC, delay_sec);
                        rb->set_state(rb, RB_STARTED);
                     }

                     _intBufReleaseData(dptr_sframe, _AAX_SENSOR);
                     rv = true;
                  }
               }
            }
            else {
               _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            }
         }
      }
      else if (handle->file.ptr == NULL)
      {
         sframe = get_write_handle(s, __func__);
         if (sframe && !sframe->thread.started && (sframe != handle) &&
             (sframe->backend.ptr == &_aaxStreamDriverBackend))
         {
            _intBufferData *dptr;

            dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               _aaxAudioFrame *mixer = sensor->mixer;
               _intBufferData *dptr_sframe;

               dptr_sframe = _intBufGet(sframe->sensors, _AAX_SENSOR, 0);
               if (dptr_sframe)
               {
                  _sensor_t *sframe_sensor = _intBufGetDataPtr(dptr_sframe);
                  _aaxAudioFrame *submix = sframe_sensor->mixer;

                  submix->info->frequency = mixer->info->frequency;
                  submix->info->period_rate = mixer->info->period_rate;
                  submix->info->refresh_rate = mixer->info->refresh_rate;
                  submix->info->update_rate = mixer->info->update_rate;
                  submix->info->no_tracks = mixer->info->no_tracks;
                  submix->info->format = mixer->info->format;
                  submix->info->unit_m = mixer->info->unit_m;

                  _intBufReleaseData(dptr_sframe, _AAX_SENSOR);
               }
               _intBufReleaseData(dptr, _AAX_SENSOR);
            }

            sframe->root = handle;
            sframe->parent = handle;
            handle->file.driver = (char*)sframe;
            handle->file.handle = sframe->backend.handle;
            handle->file.ptr = sframe->backend.ptr;
            rv = true;
         }
         else {
            _aaxErrorSet(AAX_INVALID_STATE);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_STATE);
      }
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerDeregisterSensor(const aaxConfig config, const aaxConfig s)
{
   _handle_t* handle = get_write_handle(config, __func__);
   int rv = false;
   if (handle)
   {
      _handle_t* sframe = get_read_handle(s, __func__);

      aaxSensorSetState(s, AAX_SUSPENDED);
      if (sframe && sframe->mixer_pos != UINT_MAX && sframe->parent == handle)
      {
         _intBufferData *dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = sensor->mixer;
            _intBuffers *hs = mixer->devices;
            _intBufferData *dptr_sframe;

            _intBufRemove(hs, _AAX_DEVICE, sframe->mixer_pos, false);
            mixer->no_registered--;
            handle->registered_sensors--;
            _intBufReleaseData(dptr, _AAX_SENSOR);

            dptr_sframe = _intBufGet(sframe->sensors, _AAX_SENSOR, 0);
            if (dptr_sframe)
            {
               _sensor_t* sframe_sensor = _intBufGetDataPtr(dptr_sframe);
               sframe_sensor->mixer->refctr--;
               _intBufReleaseData(dptr_sframe, _AAX_SENSOR);
            }
            sframe->root = NULL;
            sframe->parent = NULL;
            sframe->mixer_pos = UINT_MAX;
            rv = true;
         }
      }
      else if (handle->file.ptr != NULL)
      {
         sframe = get_write_handle(s, __func__);
         if (sframe && (handle->file.ptr == sframe->backend.ptr))
         {
            handle->file.ptr = NULL;
            handle->file.driver = NULL;
            handle->file.handle = NULL;
            sframe->parent = NULL;
            sframe->root = NULL;
            rv = true;
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   return rv;
}


AAX_API int AAX_APIENTRY
aaxMixerRegisterEmitter(const aaxConfig config, const aaxEmitter em)
{
   _emitter_t* emitter = get_emitter_unregistered(em, __func__);
   _handle_t* handle = get_write_handle(config, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!emitter) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (emitter->parent || emitter->mixer_pos < UINT_MAX) {
          _aaxErrorSet(AAX_INVALID_STATE+1);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _aaxEmitter *src = emitter->source;
      unsigned int pos, positional;
      _intBufferData *dptr;

      positional = _IS_POSITIONAL(src->props3d);

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
            const _aaxDriverBackend *be = handle->backend.ptr;
            if (_aaxIncreaseEmitterCounter(be))
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

         emitter->root = handle;
         emitter->parent = handle;
         emitter->mixer_pos = pos;

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
            aaxFilterDestroy(f);
         }

         src->info = handle->info;
         if (!emitter->midi.mode) {
            emitter->midi.mode = src->info->midi_mode;
         }
         if (src->update_rate == 0) {
            src->update_rate = handle->info->update_rate;
         }
         src->update_ctr = 1;

         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _aax3dProps *mp3d, *ep3d = src->props3d;
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = sensor->mixer;
            _aaxRingBuffer *rb = handle->ringbuffer;

            if (!src->p3dq && VALID_HANDLE(handle)) {
               _intBufCreate(&src->p3dq, _AAX_DELAYED3D);
            }

            if (rb && rb->get_state(rb, RB_IS_VALID)) {
               src->props2d->dist_delay_sec = 0.0f;
            }

            if (_IS_RELATIVE(ep3d)) {
               ep3d->dprops3d->matrix.m4[LOCATION][3] = 0.0;
               ep3d->dprops3d->velocity.m4[LOCATION][3] = 0.0;
            } else {
               ep3d->dprops3d->matrix.m4[LOCATION][3] = 1.0;
               ep3d->dprops3d->velocity.m4[LOCATION][3] = 1.0;
            }

            if (positional)
            {
               mp3d = mixer->props3d;
               if (_FILTER_GET_STATE(ep3d, DISTANCE_FILTER) == false)
               {
                  _FILTER_COPY_STATE(ep3d, mp3d, DISTANCE_FILTER);
//                _FILTER_COPY_DATA(ep3d, mp3d, DISTANCE_FILTER);
                  memcpy(ep3d->filter[DISTANCE_FILTER].data,
                         mp3d->filter[DISTANCE_FILTER].data,
                         sizeof(_aaxRingBufferDistanceData));
               }
               _aaxEMitterResetDistDelay(src, mixer);

               if (_EFFECT_GET_DATA(ep3d, VELOCITY_EFFECT) == NULL)
               {
                  _EFFECT_COPY(ep3d,mp3d,VELOCITY_EFFECT,AAX_SOUND_VELOCITY);
                  _EFFECT_COPY(ep3d,mp3d,VELOCITY_EFFECT,AAX_DOPPLER_FACTOR);
                  _EFFECT_COPY_DATA(ep3d, mp3d, VELOCITY_EFFECT);
               }
               _EFFECT_COPY(ep3d, mp3d, VELOCITY_EFFECT, AAX_LIGHT_VELOCITY);
            }
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
         rv = true;
      }
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerDeregisterEmitter(const aaxConfig config, const aaxEmitter em)
{
   _emitter_t* emitter = get_emitter(em, _LOCK, __func__);
   _handle_t* handle = get_write_handle(config, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!emitter) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (emitter->mixer_pos == UINT_MAX || emitter->parent != handle) {
         _aaxErrorSet(AAX_INVALID_STATE+1);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _aaxEmitter *src = emitter->source;
      _intBufferData *dptr;

      rv = false;
      dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
      if (dptr)
      {
         _sensor_t* sensor = _intBufGetDataPtr(dptr);
         _aaxAudioFrame *mixer = sensor->mixer;
         _intBuffers *he;
         void *ptr;

         if (_IS_POSITIONAL(src->props3d))
         {
            he = mixer->emitters_3d;
            _PROP_DISTQUEUE_CLEAR_DEFINED(src->props3d);
         }
         else {
            he = mixer->emitters_2d;
         }

         /* Unlock the frame again to make sure locking is done in the  */
         /* proper order by _intBufRemove                               */
         if (_intBufGetNumNoLock(he, _AAX_EMITTER))
         {
            _intBufRelease(he, _AAX_EMITTER, emitter->mixer_pos);
            ptr = _intBufRemove(he, _AAX_EMITTER, emitter->mixer_pos, false);
            if (ptr)
            {
               const _aaxDriverBackend *be = handle->backend.ptr;
               _aaxDecreaseEmitterCounter(be);
               mixer->no_registered--;
               emitter->root = NULL;
               emitter->parent = NULL;
               emitter->mixer_pos = UINT_MAX;
               rv = true;
            }
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
         else {
            _aaxErrorSet(AAX_INVALID_STATE+1);
         }
      }
   }
   put_emitter(emitter);
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerRegisterAudioFrame(const aaxConfig config, const aaxFrame f)
{
   _handle_t* handle = get_write_handle(config, __func__);
   _frame_t* frame = get_frame(f, _LOCK, __func__);
   char put = (frame && frame->parent[0]) ? true : false;
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!frame) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (frame->mixer_pos[0] != UINT_MAX) {
         _aaxErrorSet(AAX_INVALID_STATE+1);
      } else if (frame->parent[0]) {
         _aaxErrorSet(AAX_INVALID_STATE+1);
      } else {
         rv = true;
      }
   }

   if (rv)
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

            put_frame(frame);
            while ((buf = aaxAudioFrameGetBuffer(frame)) != NULL) {
               aaxBufferDestroy(buf);
            }
            frame = get_frame(f, _LOCK, __func__);

            _aaxErrorSet(AAX_ERROR_NONE);
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
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t *sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *smixer = sensor->mixer;
            _aaxAudioFrame *fmixer = frame->submix;
            _aax3dProps *mp3d = smixer->props3d;
            _aax3dProps *fp3d = fmixer->props3d;
            _aaxDelayed3dProps *fdp3d = fp3d->dprops3d;
            _aax2dProps *mp2d = smixer->props2d;
            _aax2dProps *fp2d = fmixer->props2d;

            fp2d->parent = mp2d;
            fp3d->parent = mp3d;
            if (_IS_RELATIVE(fp3d)) {
               fdp3d->matrix.m4[LOCATION][3] = 0.0;
               fdp3d->velocity.m4[LOCATION][3] = 0.0;
            } else {
               fdp3d->matrix.m4[LOCATION][3] = 1.0;
               fdp3d->velocity.m4[LOCATION][3] = 1.0;
            }

            fmixer->info->period_rate = smixer->info->period_rate;
            fmixer->info->refresh_rate = smixer->info->refresh_rate;
            fmixer->info->update_rate = smixer->info->update_rate;
            fmixer->info->unit_m = smixer->info->unit_m;
            if (_FILTER_GET_STATE(fp3d, DISTANCE_FILTER) == false)
            {
               _FILTER_COPY_STATE(fp3d, mp3d, DISTANCE_FILTER);
//             _FILTER_COPY_DATA(fp3d, mp3d, DISTANCE_FILTER);
               memcpy(fp3d->filter[DISTANCE_FILTER].data,
                      mp3d->filter[DISTANCE_FILTER].data,
                      sizeof(_aaxRingBufferDistanceData));
            }
            _aaxAudioFrameResetDistDelay(fmixer, smixer);

            if (_EFFECT_GET_DATA(fp3d, VELOCITY_EFFECT) == NULL)
            {
               _EFFECT_COPY(fp3d,mp3d,VELOCITY_EFFECT,AAX_SOUND_VELOCITY);
               _EFFECT_COPY(fp3d,mp3d,VELOCITY_EFFECT,AAX_DOPPLER_FACTOR);
               _EFFECT_COPY_DATA(fp3d, mp3d, VELOCITY_EFFECT);
            }
            _EFFECT_COPY(fp3d, mp3d, VELOCITY_EFFECT, AAX_LIGHT_VELOCITY);
            _intBufReleaseData(dptr, _AAX_SENSOR);
            rv = true;

            fmixer->refctr++;
            frame->root = handle->root;
            frame->parent[0] = handle;
            frame->mixer_pos[0] = pos;
         }
      }
   }
   if (put) put_frame(frame);

   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerDeregisterAudioFrame(const aaxConfig config, const aaxFrame f)
{
   _handle_t* handle = get_write_handle(config, __func__);
   _frame_t* frame = get_frame(f, _LOCK, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!handle) {
         _aaxErrorSet(AAX_INVALID_HANDLE);
      } else if (!frame || frame->parent[0] != handle) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (frame->mixer_pos[0] == UINT_MAX) {
         _aaxErrorSet(AAX_INVALID_STATE+1);
      } else {
         rv = true;
      }
   }

   if (rv)
   {
      _intBufferData *dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
      if (dptr)
      {
         _sensor_t* sensor = _intBufGetDataPtr(dptr);
         _aaxAudioFrame *mixer = sensor->mixer;
         _intBuffers *hf = mixer->frames;

         /* Unlock the frame again to make sure locking is done in the */
         /* proper order by _intBufRemove                              */
         if (_intBufGetNumNoLock(hf, _AAX_FRAME))
         {
            _aaxAudioFrame *submix = frame->submix;
            _intBufRelease(hf, _AAX_FRAME, frame->mixer_pos[0]);
            _intBufRemove(hf, _AAX_FRAME, frame->mixer_pos[0], false);
            mixer->no_registered--;

            submix->refctr--;
            frame->root = NULL;
            frame->parent[0] = NULL;
            frame->mixer_pos[0] = UINT_MAX;
            rv = true;
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
         _intBufReleaseData(dptr, _AAX_SENSOR);
      }
   }
   put_frame(frame);

   return rv;
}


/* -------------------------------------------------------------------------- */

int
_aaxGetCapabilities(const aaxConfig config)
{
   static int rv = -1;

   if (rv < 0)
   {
      rv = _MINMAX(_aaxGetNoCores()-1, 0, 63);

      if (sizeof(size_t) >= 8) {
         rv |= AAX_64BIT;
      }

      if (_aaxArchDetectSSE2() || _aaxArchDetectNeon()) {
         rv |= AAX_SIMD;
      }

      if (_aaxArchDetectAVX() || _aaxArchDetectHelium()) {
         rv |= AAX_SIMD256;
      }
      if (_aaxArchDetectAVX2()) {
         rv |= AAX_SIMD512;
      }
   }

   if (config)
   {
      _handle_t *handle = get_handle(config, __func__);
      _aaxMixerInfo* info = handle->info;
      rv |= info->midi_mode;
   }

   return rv;
}

static int
_aaxMixerInit(_handle_t *handle)
{
   int res = false;
   _aaxMixerInfo* info = handle->info;
   float refrate = info->refresh_rate;
   float ms = rintf(1000.0f/refrate);

   /* test if we have enough privileges to set the requested priority */
   if (ms < 11.0) {
      res = _aaxThreadSetPriority(NULL, AAX_HIGHEST_PRIORITY);
   } else if (ms < 16.0) {
      res = _aaxThreadSetPriority(NULL, AAX_HIGH_PRIORITY);
   }
   if (res != 0) {
      _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
   }
   _aaxThreadSetPriority(NULL, AAX_NORMAL_PRIORITY);

   if ((handle->valid & true) == 0)
   {
      const _aaxDriverBackend *be = handle->backend.ptr;
      void *be_handle = handle->backend.handle;
      float periodrate = info->period_rate;
      unsigned ch = info->no_tracks;
      float freq = info->frequency;
      int brate = info->bitrate;
      int fmt = info->format;
      int rssr;

      assert(be != 0);

      /* is this a registered sensor? */
      rssr = (handle->parent) ? true : false;
      res = be->setup(be_handle, &refrate, &fmt, &ch, &freq, &brate,
                      rssr, periodrate);

      if (TEST_FOR_TRUE(res))
      {
         if (VALID_HANDLE(handle) && (ch >= 1 && ch <= RB_MAX_TRACKS) &&
             (freq >= 4000 && freq <= _AAX_MAX_MIXER_FREQUENCY))
         {
            const _intBufferData* dptr;
            float old_rate, periods;

            handle->valid |= true;
            info->bitrate = brate;
            info->frequency = freq;
            info->no_tracks = ch;
            info->format = fmt;

            info->period_rate = refrate;
            old_rate = info->refresh_rate/info->update_rate;
            info->update_rate = (uint8_t)rintf(refrate/old_rate);

            // recalculate refresh_rate based on the number of periods
            // and the new refresh-rate.
            periods = _MAX(rintf(refrate/info->refresh_rate), 1.0f);
            info->refresh_rate = refrate/periods;
            info->no_samples = TIME_TO_SAMPLES(freq, info->refresh_rate);

            /* copy the hardware volume from the backend */
            dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               _aaxAudioFrame *mixer = sensor->mixer;
               _aax2dProps *p2d = mixer->props2d;
               float hw, cur;

               cur = _FILTER_GET(p2d, VOLUME_FILTER, AAX_GAIN);
               hw = be->param(be_handle, DRIVER_VOLUME);
               if (hw < LEVEL_32DB) hw = 1.0f;
               _FILTER_SET(p2d, VOLUME_FILTER, AAX_GAIN, hw*cur);
               _intBufReleaseData(dptr, _AAX_SENSOR);
            }
         }
         else {
            _aaxErrorSet(AAX_INVALID_SETUP);
         }
      } // be->setup() sets it's own error
   }
   else {
      _aaxErrorSet(AAX_INVALID_STATE);
   }

#if 0
 int q;
 printf("Mixer info:\n");
 printf(" HRTF: [0]:  %9.6f %9.6f %9.6f %9.6f\n",
         info->hrtf[0].v4[0], info->hrtf[0].v4[1],
         info->hrtf[0].v4[2], info->hrtf[0].v4[3]);
 printf("       [1]:  %9.6f %9.6f %9.6f %9.6f\n",
         info->hrtf[1].v4[0], info->hrtf[1].v4[1],
         info->hrtf[1].v4[2], info->hrtf[1].v4[3]);
 for (q=0; q<2*_AAX_MAX_SPEAKERS; ++q) {
   printf(" Speaker %2i: ", q);
   printf("% 9.6lf % 9.6lf % 9.6lf % 9.6lf\n",
     info->speaker[q].v4[0], info->speaker[q].v4[1],
     info->speaker[q].v4[2], info->speaker[q].v4[3]);
 }
 if (info->delay) {
   printf(" delay: %lf %lf %lf %lf\n",
     info->delay->v4[0], info->delay->v4[1],
     info->delay->v4[2], info->delay->v4[3]);
 }
 printf(" no. samples: %i\n", info->no_samples);
 printf(" no. tracks: %i\n", info->no_tracks);
 printf(" bitrate: %i\n", info->bitrate);
 printf(" track: %i\n", info->track);
 printf(" pitch: %f\n", info->pitch);
 printf(" frequency: %f\n", info->frequency);
 printf(" preioid rate: %f\n", info->period_rate);
 printf(" refresh rate: %f\n", info->refresh_rate);
 printf(" unit_m: %f\n", info->unit_m);
 printf(" format: %x\n", info->format);
 printf(" mode: %i\n", info->mode);
 printf(" midi mode: %i\n", info->midi_mode);
 printf(" max. emitters: %i\n", info->max_emitters);
 printf(" max. registered: %i\n", info->max_registered);
 printf(" capabilities: %i\n", info->capabilities);
 printf(" update rate: %i\n", info->update_rate);
 printf(" id: %x\n", info->id);
#endif
   return res;
}

static int
_aaxMixerStart(_handle_t *handle)
{
   int rv = false;

   if (handle && TEST_FOR_FALSE(handle->thread.started)
       && !handle->parent)
   {
      unsigned int ms;
      int r;

      handle->thread.ptr = _aaxThreadCreate();
      assert(handle->thread.ptr != 0);

      _aaxSignalInit(&handle->thread.signal);
      assert(handle->thread.signal.condition != 0);
      assert(handle->thread.signal.mutex != 0);

      handle->thread.started = true;
      ms = rintf(1000/handle->info->period_rate);
      r = _aaxThreadStart(handle->thread.ptr, handle->backend.ptr->thread,
                          handle, ms, "aaxMixer");
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
            handle->thread.started = false;
         }
         else
         {
            if (ms < 11) {
               _aaxThreadSetPriority(handle->thread.ptr, AAX_HIGHEST_PRIORITY);
            } else if (ms < 16) {
               _aaxThreadSetPriority(handle->thread.ptr, AAX_HIGH_PRIORITY);
            } else {
               _aaxThreadSetPriority(handle->thread.ptr, AAX_NORMAL_PRIORITY);
            }
            rv = true;
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_STATE);
      }
   }
   else if (_IS_PAUSED(handle) || _IS_STANDBY(handle) || handle->parent) {
      rv = true;
   }
   return rv;
}

static int
_aaxMixerStop(_handle_t *handle)
{
   int rv = false;
   if (!handle->parent && TEST_FOR_TRUE(handle->thread.started))
   {
      handle->thread.started = false;

      _aaxSignalTrigger(&handle->thread.signal);
      _aaxThreadJoin(handle->thread.ptr);

      _aaxSignalFree(&handle->thread.signal);
      _aaxThreadDestroy(handle->thread.ptr);

      if (handle->finished)
      {
         _aaxSemaphoreDestroy(handle->finished);
         handle->finished = NULL;
      }

      rv = true;
   }
   else if (handle->parent) {
      rv = true;
   }

   return rv;
}

static int
_aaxMixerUpdate(_handle_t *handle)
{
   int rv = false;
   if (!handle->parent && TEST_FOR_TRUE(handle->thread.started))
   {
      int playing = _IS_PLAYING(handle);
      if (!handle->finished) {
         handle->finished = _aaxSemaphoreCreate(0);
      }

      if (!playing) {
         _SET_PLAYING(handle);
      }
      _aaxSignalTrigger(&handle->thread.signal);
      _aaxSemaphoreWait(handle->finished);
      if (!playing) {
         _SET_PAUSED(handle);
      }
      rv = true;
   }
   else if (handle->parent) {
      rv = true;
   }

   return rv;
}

void
_aaxMixerSetRendering(_handle_t *handle)
{
   if (handle->info->midi_mode == AAX_RENDER_SYNTHESIZER)
   {
      aaxEffect eff = aaxEffectCreate(handle, AAX_PHASING_EFFECT);
      aaxFilter flt = aaxFilterCreate(handle, AAX_FREQUENCY_FILTER);
      if (flt)
      {
         aaxFilterSetSlot(flt, 0, AAX_LINEAR, 4400.0f, 1.0f, 0.3f, 1.0f);
         aaxFilterSetState(flt, true);
         aaxScenerySetFilter(handle, flt);
         aaxFilterDestroy(flt);
      }
      if (eff)
      {
         aaxEffectSetSlot(eff, 0, AAX_LINEAR, 0.7f, 0.1f, 0.05f, 0.7f);
         aaxEffectSetState(eff, AAX_SINE);
         aaxMixerSetEffect(handle, eff);
         aaxEffectDestroy(eff);
      }
   }
   if (handle->info->midi_mode)
   {
      if (handle->data_dir) free(handle->data_dir);
         handle->data_dir = systemDataFile("");
   }
}

static int
_mixerCreateEFFromAAXS(aaxConfig config, _buffer_t *buffer)
{
   _handle_t *handle = get_handle(config, __func__);
   const char *aaxs = buffer->aaxs;
   int rv = true;
   xmlId *xid;

   xid = xmlInitBuffer(aaxs, strlen(aaxs));
   if (xid)
   {
      float freq = buffer->info.base_frequency;
      xmlId * xmid = xmlNodeGet(xid, "aeonwave/mixer");
      if (xmid)
      {
         unsigned int i, num = xmlNodeGetNum(xmid, "filter");
         int clear = false;
         xmlId *xeid, *xfid;

         if (xmlAttributeExists(xmid, "mode")) {
            clear = xmlAttributeCompareString(xmid, "mode", "append");
         }

         if (clear)
         {
            const _intBufferData* dptr;
            dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               _aaxAudioFrame* smixer = sensor->mixer;

               _aaxSetDefault2dFiltersEffects(smixer->props2d);

               _intBufReleaseData(dptr, _AAX_SENSOR);
            }
         }

         xfid = xmlMarkId(xmid);
         if (xfid)
         {
            for (i=0; i<num; i++)
            {
               if (xmlNodeGetPos(xmid, xfid, "filter", i) != 0)
               {
                  aaxFilter flt = _aaxGetFilterFromAAXS(config, xfid, freq, 0.0f, 0.0f, NULL);
                  if (flt)
                  {
                     aaxMixerSetFilter(handle, flt);
                     aaxFilterDestroy(flt);
                  }
               }
            }
            xmlFree(xfid);
         }

         xeid = xmlMarkId(xmid);
         if (xeid)
         {
            num = xmlNodeGetNum(xmid, "effect");
            for (i=0; i<num; i++)
            {
               if (xmlNodeGetPos(xmid, xeid, "effect", i) != 0)
               {
                  aaxEffect eff = _aaxGetEffectFromAAXS(config, xeid, freq, 0.0f, 0.0f, NULL);
                  if (eff)
                  {
                     _effect_t* effect = get_effect(eff);
                     if (effect->type == AAX_CONVOLUTION_EFFECT)
                     {
                        char *file = xmlAttributeGetString(xeid, "file");
                        if (file)
                        {
                           aaxBuffer buf;

                           buf = _aaxCreateBufferFromAAXS(handle, buffer, file);
                           if (buf)
                           {
                              aaxEffectAddBuffer(eff, buf);
                              handle->buffer = buf;
                           }
                           else {
                              _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
                           }
                           xmlFree(file);
                        }
                     }
                     aaxMixerSetEffect(handle, eff);
                     aaxEffectDestroy(eff);
                  }
               }
            }
            xmlFree(xeid);
         }
         xmlFree(xmid);
      }
      xmlClose(xid);
   }
   else
   {
      _aaxErrorSet(AAX_INVALID_STATE);
      rv = false;
   }
   return rv;
}

static aaxBuffer
_aaxCreateBufferFromAAXS(aaxConfig config, _buffer_t *buffer, char *file)
{
   _buffer_t *rv = NULL;
   char *s, *url, **ptr = NULL;
   _buffer_info_t info;

   url = _aaxURLConstruct(buffer->url, file);
   s = strrchr(url, '.');
   if (!s || strcasecmp(s, ".aaxs"))
   {
      _handle_t *handle = (_handle_t*)config;
      ptr = _bufGetDataFromStream(config, url, &info, handle->info);
   }

   if (ptr)
   {
      rv = aaxBufferCreate(config, info.no_samples, info.no_tracks, info.fmt);
      if (rv)
      {
          free(rv->url);
          rv->url = url;

          aaxBufferSetSetup(rv, AAX_FREQUENCY, info.rate);
          aaxBufferSetSetup(rv, AAX_BLOCK_ALIGNMENT, info.blocksize);
          if ((aaxBufferSetData(rv, ptr[0])) == false) {
             aaxBufferDestroy(rv);
             rv  = NULL;
          }
      }
      else {
         free(url);
      }
      free(ptr);
   }
   else {
      free(url);
   }

   return rv;
}
