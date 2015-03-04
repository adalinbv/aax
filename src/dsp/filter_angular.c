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
#include "api.h"

static aaxFilter
_aaxAngularFilterCreate(aaxConfig config, enum aaxFilterType type)
{
   _handle_t *handle = get_handle(config);
   aaxFilter rv = NULL;
   if (handle)
   {
      unsigned int size = sizeof(_filter_t) + sizeof(_aaxFilterInfo);
     _filter_t* flt = calloc(1, size);

      if (flt)
      {
         char *ptr;
         int i;

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
   }
   return rv;
}

static int
_aaxAngularFilterDestroy(aaxFilter f)
{
   _filter_t* filter = get_filter(f);
   int rv = AAX_FALSE;
   if (filter)
   {
      free(filter);
      rv = AAX_TRUE;
   }
   return rv;
}

static aaxFilter
_aaxAngularFilterSetState(aaxFilter f, int state)
{
   _filter_t* filter = get_filter(f);
   aaxFilter rv = NULL;
   unsigned slot;

   assert(f);

   filter->state = state;
   filter->slot[0]->state = state;

   /*
    * Make sure parameters are actually within their expected boundaries.
    */
   slot = 0;
   while ((slot < _MAX_FE_SLOTS) && filter->slot[slot])
   {
      int i, type = filter->type;
      for(i=0; i<4; i++)
      {
         if (!is_nan(filter->slot[slot]->param[i]))
         {
            float min = _flt_minmax_tbl[slot][type].min[i];
            float max = _flt_minmax_tbl[slot][type].max[i];
            cvtfn_t cvtfn = filter_get_cvtfn(filter->type, AAX_LINEAR, WRITEFN, i);
            filter->slot[slot]->param[i] =
                      _MINMAX(cvtfn(filter->slot[slot]->param[i]), min, max);
         }
      }
      slot++;
   }
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewAngularFilterHandle(_aaxMixerInfo* info, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   _filter_t* rv = NULL;
   if (type < AAX_FILTER_MAX)
   {
      unsigned int size = sizeof(_filter_t) + sizeof(_aaxFilterInfo);

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
      }
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxAngularFilter =
{
   "AAX_angular_filter",
   _aaxAngularFilterCreate,
   _aaxAngularFilterDestroy,
   _aaxAngularFilterSetState,
   _aaxNewAngularFilterHandle
};

