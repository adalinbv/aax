/*
 * Copyright 2007-2018 by Erik Hofman.
 * Copyright 2009-2018 by Adalin B.V.
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

#include <assert.h>

#include <aax/aax.h>

#include <base/types.h>		/* for rintf */
#include <base/gmath.h>

#include "common.h"
#include "filters.h"
#include "api.h"

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

static int
_aaxDirectionalFilterDestroy(_filter_t* filter)
{
   free(filter);

   return AAX_TRUE;
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
      _aax_dsp_copy(rv->slot[0], &p2d->filter[rv->pos]);
      rv->slot[0]->data = NULL;

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
   AAX_TRUE,
   "AAX_directional_filter_1.01", 1.01f,
   (_aaxFilterCreate*)&_aaxDirectionalFilterCreate,
   (_aaxFilterDestroy*)&_aaxDirectionalFilterDestroy,
   (_aaxFilterSetState*)&_aaxDirectionalFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewDirectionalFilterHandle,
   (_aaxFilterConvert*)&_aaxDirectionalFilterSet,
   (_aaxFilterConvert*)&_aaxDirectionalFilterGet,
   (_aaxFilterConvert*)&_aaxDirectionalFilterMinMax
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
         } else {
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
