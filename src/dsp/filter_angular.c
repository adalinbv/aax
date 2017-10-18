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
_aaxAngularFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, 1);
   aaxFilter rv = NULL;

   if (flt)
   {
      _aaxSetDefaultFilter3d(flt->slot[0], flt->pos);
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxAngularFilterDestroy(_filter_t* filter)
{
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxAngularFilterSetState(UNUSED(_filter_t* filter), UNUSED(int state))
{
   return  filter;
}

static _filter_t*
_aaxNewAngularFilterHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   unsigned int size = sizeof(_filter_t) + sizeof(_aaxFilterInfo);
   _filter_t* rv = calloc(1, size);

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
      memcpy(rv->slot[0], &p3d->filter[rv->pos], size);
   }
   return rv;
}

static float
_aaxAngularFilterGet(float val, int ptype, unsigned char param)
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
_aaxAngularFilterSet(float val, int ptype, unsigned char param)
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
_aaxAngularFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxAngularRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { -1.0f, -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  0.0f,  0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { {  0.0f,  0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxAngularRange[slot].min[param],
                       _aaxAngularRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxAngularFilter =
{
   AAX_TRUE,
   "AAX_angular_filter_1.01", 1.01f,
   (_aaxFilterCreate*)&_aaxAngularFilterCreate,
   (_aaxFilterDestroy*)&_aaxAngularFilterDestroy,
   (_aaxFilterSetState*)&_aaxAngularFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewAngularFilterHandle,
   (_aaxFilterConvert*)&_aaxAngularFilterSet,
   (_aaxFilterConvert*)&_aaxAngularFilterGet,
   (_aaxFilterConvert*)&_aaxAngularFilterMinMax
};

