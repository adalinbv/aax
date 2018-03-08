/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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
# include <string.h>
#endif
#ifdef HAVE_LIBIO_H
#include <libio.h>              /* for NULL */
#endif
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
#include "devices.h"
#include "stream/device.h"
#include "ringbuffer.h"

static int _aaxMixerInit(_handle_t*);
static int _aaxMixerStart(_handle_t*);
static int _aaxMixerStop(_handle_t*);
static int _aaxMixerUpdate(_handle_t*);
static int _mixerCreateEFFromAAXS(aaxConfig, _buffer_t*);
static aaxBuffer _aaxCreateBufferFromAAXS(aaxConfig, _buffer_t*, char*);

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
         // intentional fallthrough
      case AAX_MONO_EMITTERS:
         rv = (setup <= _aaxGetNoEmitters()) ? AAX_TRUE : AAX_FALSE;
         break;
      case AAX_RELEASE_MODE:
         __release_mode = setup ? AAX_TRUE : AAX_FALSE;
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
         case AAX_STEREO_EMITTERS:
            setup *= 2;
            // intentional fallthrough
         case AAX_MONO_EMITTERS:
            rv = (setup <= info->max_emitters) ? AAX_TRUE : AAX_FALSE;
            break;
         case AAX_FREQUENCY:
            if ((setup > 1000) && (setup <= _AAX_MAX_MIXER_FREQUENCY))
            {
               float iv = info->refresh_rate;
               iv = setup / INTERVAL(setup / iv);
               info->frequency = (float)setup;
               info->refresh_rate = iv;
               info->period_rate = iv;
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_REFRESHRATE:
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
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_UPDATE_RATE:
            if (((setup <= _AAX_MAX_MIXER_REFRESH_RATE)
                         && (handle->valid & HANDLE_ID)))
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

               iv = fq / INTERVAL(fq / iv);
               info->refresh_rate = iv;
               info->period_rate = iv;
               rv = AAX_TRUE;
            }
            break;
         case AAX_BITRATE:
            if (setup > 0)
            {
                info->bitrate = setup;
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
         case AAX_TRACKS:
            if (setup < _AAX_MAX_SPEAKERS)
            {
               info->no_tracks = setup;
               rv = AAX_TRUE;
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
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         case AAX_RELEASE_MODE:
            __release_mode = setup ? AAX_TRUE : AAX_FALSE;
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
               rv = AAX_TRUE;
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
        case AAX_BITRATE:
            if (setup > 0)
            {
                info->bitrate = setup;
                rv = AAX_TRUE;
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

AAX_API unsigned int AAX_APIENTRY
aaxMixerGetSetup(const aaxConfig config, enum aaxSetupType type)
{
   unsigned int rv = AAX_FALSE;

   if (config == NULL)
   {
      switch(type)
      {
      case AAX_MONO_EMITTERS:
         rv = _aaxGetNoEmitters();
         break;
      case AAX_STEREO_EMITTERS:
         rv = _aaxGetNoEmitters()/2;
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
            case AAX_REFRESHRATE:
               rv = (unsigned int)info->period_rate;
               break;
            case AAX_UPDATERATE:
                rv=(unsigned int)(info->refresh_rate/handle->info->update_rate);
                break;
            case AAX_FRAME_TIMING:
               rv = (int)(handle->elapsed*1000000.0f);
               break;
            case AAX_BITRATE:
               rv = info->bitrate;
            break;
            case AAX_TRACKSIZE:
            {
               _aaxRingBuffer *rb = handle->ringbuffer;
               int bps = rb->get_parami(rb, RB_BYTES_SAMPLE);

               rv = (unsigned int)(info->frequency*bps/info->period_rate);
               break;
            }
            case AAX_TRACKS:
               rv = info->no_tracks;
               break;
            case AAX_FORMAT:
               rv = info->format;
               break;
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
                  rv = f ? AAX_TRUE : AAX_FALSE;
                  break;
               case AAX_TIMER_MODE:
                  f = be->param(handle->backend.handle, DRIVER_TIMER_MODE);
                  rv = f ? AAX_TRUE : AAX_FALSE;
                  break;
               case AAX_BATCHED_MODE:
                  f = be->param(handle->backend.handle, DRIVER_BATCHED_MODE);
                  rv = f ? AAX_TRUE : AAX_FALSE;
                  break;
               case AAX_SEEKABLE_SUPPORT:
                  f = be->param(handle->backend.handle,DRIVER_SEEKABLE_SUPPORT);
                  rv = f ? AAX_TRUE : AAX_FALSE;
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
            unsigned int track = type & 0x3F;
            if (track <= _AAX_MAX_SPEAKERS)
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
                     rv = 256*32768*lfo->compression[track];
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
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerSetMode(UNUSED(aaxConfig config), UNUSED(enum aaxModeType type), UNUSED(int mode))
{
   int rv = AAX_FALSE;
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerGetMode(UNUSED(const aaxConfig config), UNUSED(enum aaxModeType type))
{
   int rv = AAX_FALSE;
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
         rv = AAX_TRUE;
      }
   }

   if (rv)
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
            _aax2dProps *p2d = mixer->props2d;
            _FILTER_SET(p2d, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
                /* gain min and gain max are read-only for the mixer      */
            /* _FILTER_SET(p2d, type, 1, _FILTER_GET_SLOT(filter, 0, 1)); */
            /* _FILTER_SET(p2d, type, 2, _FILTER_GET_SLOT(filter, 0, 2)); */
            _FILTER_SET(p2d, type, 3, _FILTER_GET_SLOT(filter, 0, 3));
            _FILTER_SET_STATE(p2d, type, _FILTER_GET_SLOT_STATE(filter));
            _FILTER_SWAP_SLOT_DATA(p2d, type, filter, 0);
            break;
         }
         case AAX_COMPRESSOR:
         case AAX_DYNAMIC_GAIN_FILTER:
         case AAX_TIMED_GAIN_FILTER:
         {
            _aaxAudioFrame *mixer = sensor->mixer;
            _aax2dProps *p2d = mixer->props2d;
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

            /* frees both EQUALIZER_LF and EQUALIZER_HF */
            type = EQUALIZER_LF;
            _FILTER_SET(sensor, type, 0, _FILTER_GET_SLOT(filter, 0, 0));
            _FILTER_SET(sensor, type, 1, _FILTER_GET_SLOT(filter, 0, 1));
            _FILTER_SET(sensor, type, 2, _FILTER_GET_SLOT(filter, 0, 2));
            _FILTER_SET(sensor, type, 3, _FILTER_GET_SLOT(filter, 0, 3));
            _FILTER_SET_STATE(sensor, type, _FILTER_GET_SLOT_STATE(filter));
            _FILTER_SWAP_SLOT_DATA(sensor, type, filter, 0);
            break;
         default:
            _aaxErrorSet(AAX_INVALID_ENUM);
            rv = AAX_FALSE;
         }
         _intBufReleaseData(dptr, _AAX_SENSOR);
      }
      else
      {
         _aaxErrorSet(AAX_INVALID_STATE);
         rv = AAX_FALSE;
      }
   }

   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxMixerGetFilter(const aaxConfig config, enum aaxFilterType type)
{
   _handle_t* handle = get_handle(config, __func__);
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
         rv = AAX_TRUE;
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
      case AAX_REVERB_EFFECT:
      case AAX_CONVOLUTION_EFFECT:
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = sensor->mixer;
            _aax2dProps *p2d = mixer->props2d;
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
         }
         else
         {
            _aaxErrorSet(AAX_INVALID_STATE);
            rv = AAX_FALSE;
         }
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
         rv = AAX_FALSE;
      }
   }
   return rv;
}

AAX_API aaxEffect AAX_APIENTRY
aaxMixerGetEffect(const aaxConfig config, enum aaxEffectType type)
{
   _handle_t* handle = get_handle(config, __func__);
   aaxEffect rv = AAX_FALSE;
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
   int rv = AAX_FALSE;
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
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      rv = _mixerCreateEFFromAAXS(config, buffer);
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
   int rv = AAX_FALSE;
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
                     if (_FILTER_GET_STATE(sp3d, DISTANCE_FILTER) == AAX_FALSE)
                     {
                        _FILTER_COPY_STATE(sp3d, mp3d, DISTANCE_FILTER);
                        _FILTER_COPY_DATA(sp3d, mp3d, DISTANCE_FILTER);
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
                     _intBufReleaseData(dptr, _AAX_SENSOR);

                     sframe->root = handle;
                     sframe->parent = handle;
                     sframe->mixer_pos = pos;
                     submix->refcount++;

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

                        rb->set_format(rb, AAX_PCM24S, AAX_TRUE);
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
                           rb->init(rb, AAX_TRUE);
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
                     rv = AAX_TRUE;
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

                  _intBufReleaseData(dptr_sframe, _AAX_SENSOR);
               }
               _intBufReleaseData(dptr, _AAX_SENSOR);
            }

            sframe->root = handle;
            sframe->parent = handle;
            handle->file.driver = (char*)sframe;
            handle->file.handle = sframe->backend.handle;
            handle->file.ptr = sframe->backend.ptr;
            rv = AAX_TRUE;
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
   int rv = AAX_FALSE;
   if (handle)
   {
      _handle_t* sframe = get_read_handle(s, __func__);

      aaxSensorSetState(s, AAX_SUSPENDED);
      if (sframe && sframe->mixer_pos != UINT_MAX)
      {
         _intBufferData *dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = sensor->mixer;
            _intBuffers *hs = mixer->devices;
            _intBufferData *dptr_sframe;

            _intBufRemove(hs, _AAX_DEVICE, sframe->mixer_pos, AAX_FALSE);
            mixer->no_registered--;
            handle->registered_sensors--;
            _intBufReleaseData(dptr, _AAX_SENSOR);

            dptr_sframe = _intBufGet(sframe->sensors, _AAX_SENSOR, 0);
            if (dptr_sframe)
            {
               _sensor_t* sframe_sensor = _intBufGetDataPtr(dptr_sframe);
               sframe_sensor->mixer->refcount--;
               _intBufReleaseData(dptr_sframe, _AAX_SENSOR);

               sframe->root = NULL;
               sframe->parent = NULL;
               sframe->mixer_pos = UINT_MAX;
               rv = AAX_TRUE;
            }
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
            rv = AAX_TRUE;
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
   _handle_t* handle = get_write_handle(config, __func__);
   int rv = AAX_FALSE;
   if (handle)
   {
      _emitter_t* emitter = get_emitter_unregistered(em, __func__);
      if (emitter && emitter->mixer_pos == UINT_MAX)
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
               if (_aaxIncreaseEmitterCounter())
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
            if (src->update_rate == 0) {
               src->update_rate = handle->info->update_rate;
            }
            src->update_ctr = src->update_rate;

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
               }

               if (positional)
               {
                  mp3d = mixer->props3d;
                  if (_FILTER_GET_STATE(ep3d, DISTANCE_FILTER) == AAX_FALSE)
                  {
                     _FILTER_COPY_STATE(ep3d, mp3d, DISTANCE_FILTER);
                     _FILTER_COPY_DATA(ep3d, mp3d, DISTANCE_FILTER);
                  }
                  _aaxEMitterResetDistDelay(src, mixer);

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
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerDeregisterEmitter(const aaxConfig config, const aaxEmitter em)
{
   _handle_t* handle = get_write_handle(config, __func__);
   int rv = AAX_FALSE;
   if (handle)
   {
      _emitter_t* emitter = get_emitter(em, __func__);
      if (emitter && emitter->mixer_pos != UINT_MAX)
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
               ptr = _intBufRemove(he, _AAX_EMITTER, emitter->mixer_pos, AAX_FALSE);
               if (ptr)
               {
                  _aaxDecreaseEmitterCounter();
                  mixer->no_registered--;
                  emitter->root = NULL;
                  emitter->parent = NULL;
                  emitter->mixer_pos = UINT_MAX;
                  rv = AAX_TRUE;
               }
               _intBufReleaseData(dptr, _AAX_SENSOR);
            }
            else {
               _aaxErrorSet(AAX_INVALID_PARAMETER);
            }
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
      put_emitter(emitter);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerRegisterAudioFrame(const aaxConfig config, const aaxFrame f)
{
   _handle_t* handle = get_write_handle(config, __func__);
   int rv = AAX_FALSE;
   if (handle && VALID_MIXER(handle))
   {
      _frame_t* frame = get_frame(f, __func__);
      if (frame && !frame->parent)
      {
         if (frame->mixer_pos == UINT_MAX)
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
               _aax3dProps *mp3d, *fp3d;
               _aaxAudioFrame *smixer, *fmixer;

               dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
               if (dptr)
               {
                  _sensor_t *sensor = _intBufGetDataPtr(dptr);
                  smixer = sensor->mixer;
                  fmixer = frame->submix;

                  mp3d = smixer->props3d;
                  fp3d = fmixer->props3d;
                  fp3d->parent = mp3d;

                  if (_IS_RELATIVE(fp3d)) {
                     fp3d->dprops3d->matrix.m4[LOCATION][3] = 0.0;
                     fp3d->dprops3d->velocity.m4[LOCATION][3] = 0.0;
                  }

                  fmixer->info->period_rate = smixer->info->period_rate;
                  fmixer->info->refresh_rate = smixer->info->refresh_rate;
                  fmixer->info->update_rate = smixer->info->update_rate;
                  if (_FILTER_GET_STATE(fp3d, DISTANCE_FILTER) == AAX_FALSE)
                  {
                     _FILTER_COPY_STATE(fp3d, mp3d, DISTANCE_FILTER);
                     _FILTER_COPY_DATA(fp3d, mp3d, DISTANCE_FILTER);
                  }
                  _aaxAudioFrameResetDistDelay(fmixer, smixer);

                  if (_EFFECT_GET_DATA(fp3d, VELOCITY_EFFECT) == NULL)
                  {
                     _EFFECT_COPY(fp3d,mp3d,VELOCITY_EFFECT,AAX_SOUND_VELOCITY);
                     _EFFECT_COPY(fp3d,mp3d,VELOCITY_EFFECT,AAX_DOPPLER_FACTOR);
                     _EFFECT_COPY_DATA(fp3d, mp3d, VELOCITY_EFFECT);
                  }
                  _intBufReleaseData(dptr, _AAX_SENSOR);
                  rv = AAX_TRUE;

                  fmixer->refcount++;

                  frame->root = handle->root;
                  frame->parent = handle;
                  frame->mixer_pos = pos;
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
      else if (frame)
      {
         if (frame->parent) put_frame(frame);
         _aaxErrorSet(AAX_INVALID_STATE);
      }
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxMixerDeregisterAudioFrame(const aaxConfig config, const aaxFrame f)
{
   _handle_t* handle = get_write_handle(config, __func__);
   int rv = AAX_FALSE;
   if (handle)
   {
      _frame_t* frame = get_frame(f, __func__);
      if (frame && frame->mixer_pos != UINT_MAX)
      {
         _intBufferData *dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *submix = frame->submix;
            _aaxAudioFrame *mixer = sensor->mixer;
            _intBuffers *hf = mixer->frames;

            /* Unlock the frame again to make sure locking is done in the */
            /* proper order by _intBufRemove                              */
            if (_intBufGetNumNoLock(hf, _AAX_FRAME))
            {
               _intBufRelease(hf, _AAX_FRAME, frame->mixer_pos);
               _intBufRemove(hf, _AAX_FRAME, frame->mixer_pos, AAX_FALSE);
               mixer->no_registered--;
               _intBufReleaseData(dptr, _AAX_SENSOR);

               submix->refcount--;
               frame->root = NULL;
               frame->parent = NULL;
               frame->mixer_pos = UINT_MAX;
               rv = AAX_TRUE;
            }
            else {
               _aaxErrorSet(AAX_INVALID_PARAMETER);
            }
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
      put_frame(frame);
   }
   return rv;
}


/* -------------------------------------------------------------------------- */

static int
_aaxMixerInit(_handle_t *handle)
{
   int res = AAX_FALSE;
   _aaxMixerInfo* info = handle->info;
   float refrate = info->refresh_rate;
   float ms = rintf(1000.0f/refrate);

   /* test if we have enough privileges to set the requested priority */
   if (ms < 5) {
      res = _aaxThreadSetPriority(NULL, AAX_HIGHEST_PRIORITY);
   } else if (ms < 11) {
      res = _aaxThreadSetPriority(NULL, AAX_HIGH_PRIORITY);
   }
   if (res != 0) {
      _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
   }
   _aaxThreadSetPriority(NULL, AAX_NORMAL_PRIORITY);

   if ((handle->valid & AAX_TRUE) == 0)
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
      rssr = (handle->parent) ? AAX_TRUE : AAX_FALSE;
      res = be->setup(be_handle, &refrate, &fmt, &ch, &freq, &brate,
                      rssr, periodrate);

      if (TEST_FOR_TRUE(res))
      {
         if ((VALID_HANDLE(handle) && freq <= _AAX_MAX_MIXER_FREQUENCY))
         {
            const _intBufferData* dptr;
            float old_rate, periods;

            handle->valid |= AAX_TRUE;
            info->bitrate = brate;
            info->frequency = freq;
            info->no_tracks = ch;
            info->format = fmt;

            info->period_rate = refrate;
            old_rate = info->refresh_rate/info->update_rate;
            info->update_rate = (uint8_t)rintf(refrate/old_rate);

            // recalculate refresh_rate based on the number of periods
            // and the new refresh-rate.
            periods = rintf(refrate/info->refresh_rate);
            info->refresh_rate = refrate/periods;
            info->no_samples = TIME_TO_SAMPLES(freq, info->refresh_rate);

            /* copy the hardware volume from the backend */
            dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr)
            {
               _sensor_t* sensor = _intBufGetDataPtr(dptr);
               _aaxAudioFrame *mixer = sensor->mixer;
               _aax2dProps *p2d = mixer->props2d;
               float cur;

               cur = be->param(be_handle, DRIVER_VOLUME);
               if (cur < 0.05f) cur = 1.0f;
               _FILTER_SET(p2d, VOLUME_FILTER, AAX_GAIN, cur);
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
   return res;
}

static int
_aaxMixerStart(_handle_t *handle)
{
   int rv = AAX_FALSE;

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

      handle->thread.started = AAX_TRUE;
      ms = rintf(1000/handle->info->period_rate);
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
         else
         {
            if (ms < 5) {
               _aaxThreadSetPriority(handle->thread.ptr, AAX_HIGHEST_PRIORITY);
            } else if (ms < 11) {
               _aaxThreadSetPriority(handle->thread.ptr, AAX_HIGH_PRIORITY);
            } else {
               _aaxThreadSetPriority(handle->thread.ptr, AAX_NORMAL_PRIORITY);
            }
            rv = AAX_TRUE;
         }
      }   
      else {
         _aaxErrorSet(AAX_INVALID_STATE);
      }
   }
   else if (_IS_PAUSED(handle) || _IS_STANDBY(handle) || handle->parent) {
      rv = AAX_TRUE;
   }
   return rv;
}

static int
_aaxMixerStop(_handle_t *handle)
{
   int rv = AAX_FALSE;
   if (!handle->parent && TEST_FOR_TRUE(handle->thread.started))
   {
      handle->thread.started = AAX_FALSE;

      _aaxSignalTrigger(&handle->thread.signal);
      _aaxThreadJoin(handle->thread.ptr);

      _aaxSignalFree(&handle->thread.signal);
      _aaxThreadDestroy(handle->thread.ptr);

      if (handle->finished)
      {
         _aaxSemaphoreDestroy(handle->finished);
         handle->finished = NULL;
      }

      rv = AAX_TRUE;
   }
   else if (handle->parent) {
      rv = AAX_TRUE;
   }

   return rv;
}

static int
_aaxMixerUpdate(_handle_t *handle)
{
   int rv = AAX_FALSE;
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
      rv = AAX_TRUE;
   }
   else if (handle->parent) {
      rv = AAX_TRUE;
   }

   return rv;
}

static int
_mixerCreateEFFromAAXS(aaxConfig config, _buffer_t *buffer)
{
   _handle_t *handle = get_handle(config, __func__);
   const char *aaxs = buffer->aaxs;
   int rv = AAX_TRUE;
   void *xid;

   put_emitter(handle);

   xid = xmlInitBuffer(aaxs, strlen(aaxs));
   if (xid)
   {
      void *xmid = xmlNodeGet(xid, "aeonwave/sound");
      float freq = 0.0f;

      if (xmid)
      {
         freq = xmlAttributeGetDouble(xmid, "frequency");
         xmlFree(xmid);
      }

      xmid = xmlNodeGet(xid, "aeonwave/mixer");
      if (xmid)
      {
         int clear = xmlAttributeCompareString(xmid, "mode", "append");
         unsigned int i, num = xmlNodeGetNum(xmid, "filter");
         void *xeid, *xfid = xmlMarkId(xmid);

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

         for (i=0; i<num; i++)
         {
            if (xmlNodeGetPos(xmid, xfid, "filter", i) != 0)
            {
               aaxFilter flt = _aaxGetFilterFromAAXS(config, xfid, freq);
               if (flt)
               {
                  aaxMixerSetFilter(handle, flt);
                  aaxFilterDestroy(flt);
               }
            }
         }
         xmlFree(xfid);

         xeid = xmlMarkId(xmid);
         num = xmlNodeGetNum(xmid, "effect");
         for (i=0; i<num; i++)
         {
            if (xmlNodeGetPos(xmid, xeid, "effect", i) != 0)
            {
               char *file = xmlAttributeGetString(xeid, "file");
               aaxEffect eff = _aaxGetEffectFromAAXS(config, xeid, freq);
               if (eff)
               {
                  _effect_t* effect = get_effect(eff);
                  if (file && effect->type == AAX_CONVOLUTION_EFFECT)
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
                  }
                  aaxMixerSetEffect(handle, eff);
                  aaxEffectDestroy(eff);
               }
               xmlFree(file);
            }
         }
         xmlFree(xeid);
         xmlFree(xmid);
      }
      xmlClose(xid);
   }
   else
   {
      _aaxErrorSet(AAX_INVALID_STATE);
      rv = AAX_FALSE;
   }
   return rv;
}

static aaxBuffer
_aaxCreateBufferFromAAXS(aaxConfig config, _buffer_t *buffer, char *file)
{
   _buffer_t *rv = NULL;
   char *s, *u, *url, **ptr = NULL;
   size_t no_samples, blocksize;
   unsigned int tracks;
   float freq;
   int fmt;

   u = strdup(buffer->url);
   url = _aaxURLConstruct(u, file);
   free(u);

   s = strrchr(url, '.');
   if (!s || strcasecmp(s, ".aaxs")) {
      ptr = _bufGetDataFromStream(url, &fmt, &tracks, &freq,
                                       &no_samples, &blocksize);
   }

   if (ptr)
   {
      rv = aaxBufferCreate(config, no_samples, tracks, fmt);
      if (rv)
      {
          free(rv->url);
          rv->url = url;

          aaxBufferSetSetup(rv, AAX_FREQUENCY, freq);
          aaxBufferSetSetup(rv, AAX_BLOCK_ALIGNMENT, blocksize);
          if ((aaxBufferSetData(rv, ptr[0])) == AAX_FALSE) {
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
