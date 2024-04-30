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

#include <dsp/filters.h>
#include <dsp/effects.h>

#include "api.h"

AAX_API bool AAX_APIENTRY
aaxScenerySetSetup(UNUSED(aaxConfig config), UNUSED(enum aaxSetupType type), UNUSED(int64_t setup))
{
   bool rv = false;
   return rv;
}

AAX_API int64_t AAX_APIENTRY
aaxSceneryGetSetup(UNUSED(const aaxConfig config), UNUSED(enum aaxSetupType type))
{
   int64_t rv = false;
   return rv;
}

AAX_API bool AAX_APIENTRY
aaxScenerySetFilter(aaxConfig config, aaxFilter f)
{
   _handle_t* handle = get_handle(config, __func__);
   bool rv = false;
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
            _aaxAudioFrame* mixer = sensor->mixer;
            _aax2dProps *p2d = mixer->props2d;
            _aax3dProps *p3d = mixer->props3d;
            int type = filter->pos;
            switch (filter->type)
            {
            case AAX_FREQUENCY_FILTER:
               _FILTER_SWAP_SLOT(p2d, type, filter, 0);
               rv = true;
               break;
            case AAX_DISTANCE_FILTER:
               _FILTER_SWAP_SLOT(p3d, type, filter, 0);
               if (_EFFECT_GET_UPDATED(p3d, VELOCITY_EFFECT) == false)
               {
                  _aaxRingBufferDistanceData *data;
                  data = _FILTER_GET_DATA(p3d, DISTANCE_FILTER);
                  if (data->next.T_K != 0.0f && data->next.hr_pct != 0.0f)
                  {
                     if (_FILTER_GET_SLOT_STATE(filter) & AAX_ISO9613_DISTANCE)
                     {
                        float vs = _velocity_calculcate_vs(&data->next);
                        _EFFECT_SET(p3d, VELOCITY_EFFECT, AAX_SOUND_VELOCITY, vs);
                     }
                  }
               }
               rv = true;
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
   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxSceneryGetFilter(aaxConfig config, enum aaxFilterType type)
{
   _handle_t* handle = get_handle(config, __func__);
   aaxFilter rv = false;
   if (handle)
   {
      const _intBufferData* dptr;
      switch (type)
      {
      case AAX_FREQUENCY_FILTER:
      case AAX_DISTANCE_FILTER:
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            rv = new_filter_handle(config, type, mixer->props2d,
                                                 mixer->props3d);
            _intBufReleaseData(dptr, _AAX_SENSOR);
         }
         break;
      default:
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   return rv;
}

AAX_API bool AAX_APIENTRY
aaxScenerySetEffect(aaxConfig config, aaxEffect e)
{
   _handle_t* handle = get_handle(config, __func__);
   bool rv = false;
   if (handle)
   {
      _effect_t* effect = get_effect(e);
      if (effect)
      {
         const _intBufferData* dptr;
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            _aax3dProps *p3d = mixer->props3d;
            int type = effect->pos;
            switch (effect->type)
            {
            case AAX_VELOCITY_EFFECT:
            {
               float c, vs;

               _EFFECT_SWAP_SLOT(p3d, type, effect, 0);

               c = _EFFECT_GET(p3d, type, AAX_LIGHT_VELOCITY);
               vs = _EFFECT_GET(p3d, type, AAX_SOUND_VELOCITY);
               if (c < 800000.0f*vs)
               {
                  static const double c_mps = 299792458.0;
                  double unit_m = vs/343.0;

                  handle->info->unit_m = unit_m;
                  _EFFECT_SET(p3d, type, AAX_LIGHT_VELOCITY, c_mps*unit_m);
               }
               rv = true;
               break;
            }
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
   return rv;
}

AAX_API aaxEffect AAX_APIENTRY
aaxSceneryGetEffect(aaxConfig config, enum aaxEffectType type)
{
   _handle_t* handle = get_handle(config, __func__);
   aaxEffect rv = false;
   if (handle)
   {
      const _intBufferData* dptr;
      switch(type)
      {
      case AAX_VELOCITY_EFFECT:
         dptr = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
         if (dptr)
         {
            _sensor_t* sensor = _intBufGetDataPtr(dptr);
            _aaxAudioFrame* mixer = sensor->mixer;
            rv = new_effect_handle(config, type, mixer->props2d,
                                                 mixer->props3d);
            _intBufReleaseData(dptr, _AAX_SENSOR);
            if (rv)
            {
                _effect_t *eff = (_effect_t*)rv;
                float unit_m = handle->info->unit_m;
                float c_mps = _FILTER_GET_SLOT(eff, 0, AAX_LIGHT_VELOCITY);
               _FILTER_SET_SLOT(eff, 0, AAX_LIGHT_VELOCITY, c_mps/unit_m);
            }
         }
         break;
      default:
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   return rv;
}
