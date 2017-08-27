/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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
_aaxVolumeFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
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
      flt->pos = _flt_cvt_tbl[type].pos;
      flt->type = type;

      size = sizeof(_aaxFilterInfo);
      _aaxSetDefaultFilter2d(flt->slot[0], flt->pos);
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxVolumeFilterDestroy(_filter_t* filter)
{
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxVolumeFilterSetState(_filter_t* filter, UNUSED(int state))
{
   return filter;
}

static _filter_t*
_aaxNewVolumeFilterHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
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
      rv->pos = _flt_cvt_tbl[type].pos;
      rv->state = p2d->filter[rv->pos].state;
      rv->type = type;

      size = sizeof(_aaxFilterInfo);
      memcpy(rv->slot[0], &p2d->filter[rv->pos], size);
      rv->slot[0]->data = NULL;
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
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 10.0f, 1.0f, 10.0f, 10.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {  0.0f, 0.0f,  0.0f,  0.0f } }, 
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {  0.0f, 0.0f,  0.0f,  0.0f } }
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
   "AAX_volume_filter", 1.0f,
   (_aaxFilterCreate*)&_aaxVolumeFilterCreate,
   (_aaxFilterDestroy*)&_aaxVolumeFilterDestroy,
   (_aaxFilterSetState*)&_aaxVolumeFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewVolumeFilterHandle,
   (_aaxFilterConvert*)&_aaxVolumeFilterSet,
   (_aaxFilterConvert*)&_aaxVolumeFilterGet,
    (_aaxFilterConvert*)&_aaxVolumeFilterMinMax
};

