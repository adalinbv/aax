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

#include <assert.h>

#include <aax/aax.h>

#include <base/types.h>		/* for rintf */
#include <base/gmath.h>

#include "common.h"
#include "filters.h"
#include "api.h"

#define VERSION	1.01

static aaxFilter
_aaxDirectionalFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, 1, 0);
   aaxFilter rv = NULL;

   if (flt)
   {
      _aaxSetDefaultFilter3d(flt->slot[0], flt->pos, 0);
      rv = (aaxFilter)flt;
   }
   return rv;
}

static aaxFilter
_aaxDirectionalFilterSetState(UNUSED(_filter_t* filter), UNUSED(int state))
{
   return  filter;
}

static _filter_t*
_aaxNewDirectionalFilterHandle(const aaxConfig config, enum aaxFilterType type, UNUSED(_aax2dProps* p2d), _aax3dProps* p3d)
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _filter_t* rv = _aaxFilterCreateHandle(info, type, 1, 0);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[0], &p3d->filter[rv->pos]);
      rv->state = p3d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxDirectionalFilterGet(float val, int ptype, unsigned char param)
{
   float rv = val;
   if (param < 2)
   {
      if (ptype == AAX_DEGREES) {
         rv = _cos_deg2rad_2(val);
      } else {
         rv = _cos_2(val);
      }
   }
   return rv;
}

static float
_aaxDirectionalFilterSet(float val, int ptype, unsigned char param)
{
   float rv = val;
   if (param < 4)
   {
      if (ptype == AAX_DEGREES) {
         rv = _2acos_rad2deg(val);
      } else {
         rv = _2acos(val);
      }
   }
   return rv;
}

static float
_aaxDirectionalFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxDirectionalRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { -1.0f, -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.0f,  0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { {  0.0f,  0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { {  0.0f,  0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxDirectionalRange[slot].min[param],
                       _aaxDirectionalRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxDirectionalFilter =
{
   "AAX_directional_filter_"AAX_MKSTR(VERSION), VERSION,
   (_aaxFilterCreateFn*)&_aaxDirectionalFilterCreate,
   (_aaxFilterDestroyFn*)&_aaxFilterDestroy,
   NULL,
   (_aaxFilterSetStateFn*)&_aaxDirectionalFilterSetState,
   (_aaxNewFilterHandleFn*)&_aaxNewDirectionalFilterHandle,
   (_aaxFilterConvertFn*)&_aaxDirectionalFilterSet,
   (_aaxFilterConvertFn*)&_aaxDirectionalFilterGet,
   (_aaxFilterConvertFn*)&_aaxDirectionalFilterMinMax
};

/*
 * Directional filter: audio cone support.
 *
 * Version 2.6 adds forward gain which allows for donut shaped cones.
 * TODO: Calculate cone relative to the frame position when indoors.
 *       For now it is assumed that indoor reflections render the cone
 *       pretty much useless, at least if the emitter is in another room.
 */
float
_directional_prepare(_aax3dProps *ep3d,  _aaxDelayed3dProps *edp3d_m, _aaxDelayed3dProps *fdp3d_m)
{
   float cone_volume = 1.0f;

   if (_PROP3D_CONE_IS_DEFINED(edp3d_m) && !_PROP3D_MONO_IS_DEFINED(fdp3d_m))
   {
      float tmp, forward_gain, inner_vec;

      forward_gain = _FILTER_GET(ep3d, DIRECTIONAL_FILTER, AAX_FORWARD_GAIN);
      inner_vec = _FILTER_GET(ep3d, DIRECTIONAL_FILTER, AAX_INNER_ANGLE);
      tmp = -edp3d_m->matrix.m4[DIR_BACK][2];

      if (tmp < inner_vec)
      {
         float outer_vec, outer_gain;

         outer_vec = _FILTER_GET(ep3d, DIRECTIONAL_FILTER, AAX_OUTER_ANGLE);
         outer_gain = _FILTER_GET(ep3d, DIRECTIONAL_FILTER, AAX_OUTER_GAIN);
         if (outer_vec < tmp)
         {
            tmp -= inner_vec;
            tmp *= (outer_gain - 1.0f);
            tmp /= (outer_vec - inner_vec);
            cone_volume = (1.0f + tmp);
         }
         else if (outer_vec < 1.0f) {
            cone_volume = outer_gain;
         }
      }
      else if (forward_gain != 1.0f)
      {
         tmp -= inner_vec;

         tmp /= (1.0f - inner_vec);
         cone_volume = (1.0f - tmp) + tmp*forward_gain;
      }
   }

   return cone_volume;
}
