/*
 * Copyright 2007-2015 by Erik Hofman.
 * Copyright 2009-2015 by Adalin B.V.
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
_aaxDistanceFilterCreate(_handle_t *handle, enum aaxFilterType type)
{
   unsigned int size = sizeof(_filter_t) + sizeof(_aaxFilterInfo);
   _filter_t* flt = calloc(1, size);
   aaxFilter rv = NULL;

   if (flt)
   {
      char *ptr;

      flt->id = FILTER_ID;
      flt->state = AAX_FALSE;
      flt->info = handle->info ? handle->info : _info;

      ptr = (char*)flt + sizeof(_filter_t);
      flt->slot[0] = (_aaxFilterInfo*)ptr;
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
   aaxFilter rv = NULL;

   if ((state & ~AAX_DISTANCE_DELAY) < AAX_AL_DISTANCE_MODEL_MAX)
   {
      int pos = state & ~AAX_DISTANCE_DELAY;
      if ((pos >= AAX_AL_INVERSE_DISTANCE)
          && (pos < AAX_AL_DISTANCE_MODEL_MAX))
      {
         pos -= AAX_AL_INVERSE_DISTANCE;
         filter->slot[0]->state = state;
         filter->slot[0]->data = _aaxRingBufferALDistanceFn[pos];
      }
      else if (pos < AAX_DISTANCE_MODEL_MAX)
      {
         filter->slot[0]->state = state;
         filter->slot[0]->data = _aaxRingBufferDistanceFn[pos];
      }
      else _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   else _aaxErrorSet(AAX_INVALID_PARAMETER);
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewDistanceFilterHandle(_aaxMixerInfo* info, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   unsigned int size = sizeof(_filter_t) + sizeof(_aaxFilterInfo);
   _filter_t* rv = calloc(1, size);

   rv = calloc(1, size);
   if (rv)
   {
      char *ptr = (char*)rv + sizeof(_filter_t);

      rv->id = FILTER_ID;
      rv->info = info ? info : _info;
      rv->slot[0] = (_aaxFilterInfo*)ptr;
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
_aaxDistanceFilterSet(float val, int ptype, unsigned char param)
{
   float rv = val;
   return rv;
}

static float
_aaxDistanceFilterGet(float val, int ptype, unsigned char param)
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


