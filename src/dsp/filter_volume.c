/*
 * Copyright 2007-2018 by Erik Hofman.
 * Copyright 2009-2018 by Adalin B.V.
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

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
#endif

#include <aax/aax.h>

#include <base/types.h>		/* for rintf */
#include <base/gmath.h>

#include "common.h"
#include "filters.h"
#include "arch.h"
#include "api.h"

static aaxFilter
_aaxVolumeFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, 2);
   aaxFilter rv = NULL;

   if (flt)
   {
      _aaxSetDefaultFilter3d(flt->slot[0], flt->pos, 0);
      _aaxSetDefaultFilter3d(flt->slot[1], flt->pos, 1);
      flt->slot[0]->destroy = aligned_destroy;
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxVolumeFilterDestroy(_filter_t* filter)
{
   filter->slot[0]->destroy(filter->slot[0]->data);
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxVolumeFilterSetState(_filter_t* filter, int state)
{
   _aaxRingBufferOcclusionData *direct_path = filter->slot[0]->data;

   if (!direct_path)
   {
      direct_path = _aax_aligned_alloc(sizeof(_aaxRingBufferOcclusionData));
      filter->slot[0]->data = direct_path;
   }

   if (direct_path)
   {
      direct_path->occlusion.v4[0] = 0.5f*filter->slot[1]->param[0];
      direct_path->occlusion.v4[1] = 0.5f*filter->slot[1]->param[1];
      direct_path->occlusion.v4[2] = 0.5f*filter->slot[1]->param[2];
      direct_path->occlusion.v4[3] = 1.0f - filter->slot[1]->param[3];
      direct_path->radius_sq = vec3fMagnitudeSquared(&direct_path->occlusion.v3);
      direct_path->fc = 22000.0f;

      direct_path->level = 1.0f;
      direct_path->inverse = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;
   }

   return filter;
}

static _filter_t*
_aaxNewVolumeFilterHandle(const aaxConfig config, enum aaxFilterType type, UNUSED(_aax2dProps* p2d), _aax3dProps* p3d)
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _filter_t* rv = _aaxFilterCreateHandle(info, type, 2);

   if (rv)
   {
      unsigned int size = sizeof(_aaxFilterInfo);

      memcpy(rv->slot[0], &p2d->filter[rv->pos], size);
      memcpy(rv->slot[1], &p3d->filter[rv->pos], size);
      rv->slot[0]->data = NULL;

      rv->state = p3d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxVolumeFilterSet(float val, int ptype, UNUSED(unsigned char param))
{
   float rv = val;
   if (ptype == AAX_LOGARITHMIC) {
      rv = _lin2db(val);
   }
   return rv;
}

static float
_aaxVolumeFilterGet(float val, int ptype, UNUSED(unsigned char param))
{
   float rv = val;
   if (param < 3 && ptype == AAX_LOGARITHMIC) {
      rv = _db2lin(val);
   }
   return rv;
}

static float
_aaxVolumeFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxVolumeRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {   10.0f,    1.0f,    10.0f, 10.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { FLT_MAX, FLT_MAX,  FLT_MAX,  1.0f } }, 
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {    0.0f,    0.0f,     0.0f,  0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {    0.0f,    0.0f,     0.0f,  0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxVolumeRange[slot].min[param],
                       _aaxVolumeRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxVolumeFilter =
{
   AAX_TRUE,
   "AAX_volume_filter_1.1", 1.1f,
   (_aaxFilterCreate*)&_aaxVolumeFilterCreate,
   (_aaxFilterDestroy*)&_aaxVolumeFilterDestroy,
   (_aaxFilterSetState*)&_aaxVolumeFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewVolumeFilterHandle,
   (_aaxFilterConvert*)&_aaxVolumeFilterSet,
   (_aaxFilterConvert*)&_aaxVolumeFilterGet,
    (_aaxFilterConvert*)&_aaxVolumeFilterMinMax
};

