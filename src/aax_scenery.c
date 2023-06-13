/*
 * Copyright 2007-2023 by Erik Hofman.
 * Copyright 2009-2023 by Adalin B.V.
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

#include <dsp/filters.h>
#include <dsp/effects.h>

#include "api.h"

#if 0
AAX_API int AAX_APIENTRY
aaxScenerySetMatrix64(aaxConfig config, aaxMtx4d mtx64)
{
   _handle_t *handle = get_handle(config, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx64 || detect_nan_mtx4d(mtx64)) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv) {
      mtx4dFill(handle->info->matrix.m4, mtx64);
   }

   return rv;
}

AAX_API int AAX_APIENTRY
aaxSceneryGetMatrix64(aaxConfig config, aaxMtx4d mtx64)
{
   _handle_t *handle = get_handle(config, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!mtx64) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv) {
      mtx4dFill(mtx64, handle->info->matrix.m4);
   }

   return rv;
}

AAX_API int AAX_APIENTRY
aaxScenerySetDimensions(aaxConfig config, aaxVec3f dimensions)
{
   _handle_t *handle = get_handle(config, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!dimensions || detect_nan_vec3(dimensions)) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      float radius;

      vec3fFill(handle->info->bounding.box.v3, dimensions);

      radius = fmaxf(dimensions[0], fmaxf(dimensions[1], dimensions[2]));
      handle->info->bounding.radius_sq = radius*radius;

      // Scenery does not set the dimensions property on purpose.
      // This tells the emitter to apply directional cues.
      // _PROP_DIMENSIONS_SET_DEFINED(p3d);
   }

   return rv;
}

AAX_API int AAX_APIENTRY
aaxSceneryGetDimensions(aaxConfig config, aaxVec3f dimensions)
{
   _handle_t *handle = get_handle(config, __func__);
   int rv = __release_mode;

   if (!rv)
   {
      if (!dimensions) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv) {
      vec3fFill(dimensions, handle->info->bounding.box.v3);
   }

   return rv;
}
#endif

AAX_API int AAX_APIENTRY
aaxScenerySetSetup(UNUSED(aaxConfig config), UNUSED(enum aaxSetupType type), UNUSED(unsigned int setup))
{
   int rv = AAX_FALSE;
   return rv;
}

AAX_API unsigned int AAX_APIENTRY
aaxSceneryGetSetup(UNUSED(const aaxConfig config), UNUSED(enum aaxSetupType type))
{
   unsigned int rv = AAX_FALSE;
   return rv;
}

AAX_API int AAX_APIENTRY
aaxScenerySetFilter(aaxConfig config, aaxFilter f)
{
   _handle_t* handle = get_handle(config, __func__);
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
            _aaxAudioFrame* mixer = sensor->mixer;
            _aax2dProps *p2d = mixer->props2d;
            _aax3dProps *p3d = mixer->props3d;
            int type = filter->pos;
            switch (filter->type)
            {
            case AAX_FREQUENCY_FILTER:
               _FILTER_SWAP_SLOT(p2d, type, filter, 0);
               rv = AAX_TRUE;
               break;
            case AAX_DISTANCE_FILTER:
               _FILTER_SWAP_SLOT(p3d, type, filter, 0);
               if (_EFFECT_GET_UPDATED(p3d, VELOCITY_EFFECT) == AAX_FALSE)
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
   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxSceneryGetFilter(aaxConfig config, enum aaxFilterType type)
{
   _handle_t* handle = get_handle(config, __func__);
   aaxFilter rv = AAX_FALSE;
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

AAX_API int AAX_APIENTRY
aaxScenerySetEffect(aaxConfig config, aaxEffect e)
{
   _handle_t* handle = get_handle(config, __func__);
   int rv = AAX_FALSE;
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
               rv = AAX_TRUE;
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
   aaxEffect rv = AAX_FALSE;
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
         }
         break;
      default:
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   return rv;
}
