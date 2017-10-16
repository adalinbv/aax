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
#include "api.h"

static aaxFilter
_aaxDistanceFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   unsigned int size = sizeof(_filter_t) + sizeof(_aaxFilterInfo);
   _filter_t* flt = calloc(1, size);
   aaxFilter rv = NULL;

   if (flt)
   {
      char *ptr;

      flt->id = FILTER_ID;
      flt->state = AAX_FALSE;
      flt->info = info;

      ptr = (char*)flt + sizeof(_filter_t);
      flt->slot[0] = (_aaxFilterInfo*)ptr;
      flt->slot[0]->data = *(void**)&_aaxRingBufferDistanceFn[1];
      flt->pos = _flt_cvt_tbl[type].pos;
      flt->type = type;

      size = sizeof(_aaxFilterInfo);
      _aaxSetDefaultFilter3d(flt->slot[0], flt->pos);
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxDistanceFilterDestroy(_filter_t* filter)
{
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxDistanceFilterSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = NULL;

   if ((state & ~AAX_DISTANCE_DELAY) < AAX_AL_DISTANCE_MODEL_MAX)
   {
      int pos = state & ~AAX_DISTANCE_DELAY;
      if ((pos >= AAX_AL_INVERSE_DISTANCE)
          && (pos < AAX_AL_DISTANCE_MODEL_MAX))
      {
         pos -= AAX_AL_INVERSE_DISTANCE;
         filter->slot[0]->state = state;
         filter->slot[0]->data = *(void**)&_aaxRingBufferALDistanceFn[pos];
      }
      else if (pos < AAX_DISTANCE_MODEL_MAX)
      {
         filter->slot[0]->state = state;
         filter->slot[0]->data = *(void**)&_aaxRingBufferDistanceFn[pos];
      }
      else _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   else _aaxErrorSet(AAX_INVALID_PARAMETER);
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewDistanceFilterHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   unsigned int size = sizeof(_filter_t) + sizeof(_aaxFilterInfo);
   _filter_t* rv = calloc(1, size);

   rv = calloc(1, size);
   if (rv)
   {
      _handle_t *handle = get_driver_handle(config);
      _aaxMixerInfo* info = handle ? handle->info : _info;
      char *ptr = (char*)rv + sizeof(_filter_t);

      rv->id = FILTER_ID;
      rv->info = info;
      rv->handle = handle;
      rv->slot[0] = (_aaxFilterInfo*)ptr;
      rv->slot[0]->data = *(void**)&_aaxRingBufferDistanceFn[1];
      rv->pos = _flt_cvt_tbl[type].pos;
      rv->state = p2d->filter[rv->pos].state;
      rv->type = type;

      size = sizeof(_aaxFilterInfo);
      memcpy(rv->slot[0], &p3d->filter[rv->pos], size);
      rv->state = p3d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxDistanceFilterSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxDistanceFilterGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxDistanceFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxDistanceRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.1f, 0.0f, 0.0f }, { MAXFLOAT, MAXFLOAT, 1.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxDistanceRange[slot].min[param],
                       _aaxDistanceRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxDistanceFilter =
{
   AAX_TRUE,
   "AAX_distance_filter", 1.0f,
   (_aaxFilterCreate*)&_aaxDistanceFilterCreate,
   (_aaxFilterDestroy*)&_aaxDistanceFilterDestroy,
   (_aaxFilterSetState*)&_aaxDistanceFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewDistanceFilterHandle,
   (_aaxFilterConvert*)&_aaxDistanceFilterSet,
   (_aaxFilterConvert*)&_aaxDistanceFilterGet,
   (_aaxFilterConvert*)&_aaxDistanceFilterMinMax
};


