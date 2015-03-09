/*
 * Copyright 2007-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
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

#include <dsp/filters.h>
#include <dsp/effects.h>

#include "api.h"

#define WRITEFN		0
#define EPS		1e-5
#define READFN		!WRITEFN

AAX_API aaxFilter AAX_APIENTRY
aaxFilterCreate(aaxConfig config, enum aaxFilterType type)
{
   _handle_t *handle = get_handle(config);
   aaxFilter rv = NULL;
   if (handle && (type < AAX_FILTER_MAX))
   {
      _flt_function_tbl *flt = _aaxFilters[type-1];
      rv = flt->create(handle, type);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxFilterDestroy(aaxFilter f)
{
   _filter_t* filter = get_filter(f);
   int rv = AAX_FALSE;
   if (filter)
   {
      _flt_function_tbl *flt = _aaxFilters[filter->type-1];
      rv = flt->destroy(filter);
   }
   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxFilterSetSlot(aaxFilter f, unsigned slot, int ptype, float p1, float p2, float p3, float p4)
{
   aaxVec4f v = { p1, p2, p3, p4 };
   return aaxFilterSetSlotParams(f, slot, ptype, v);
}

AAX_API aaxFilter AAX_APIENTRY
aaxFilterSetSlotParams(aaxFilter f, unsigned slot, int ptype, aaxVec4f p)
{
   _filter_t* filter = get_filter(f);
   aaxFilter rv = AAX_FALSE;
   if (filter && p)
   {
      if ((slot < _MAX_FE_SLOTS) && filter->slot[slot])
      {
         int i, type = filter->type;
         for (i=0; i<4; i++)
         {
            if (!is_nan(p[i]))
            {
               float min = _flt_minmax_tbl[slot][type].min[i];
               float max = _flt_minmax_tbl[slot][type].max[i];
               cvtfn_t cvtfn = filter_get_cvtfn(filter->type, ptype, WRITEFN, i);
               filter->slot[slot]->param[i] = _MINMAX(cvtfn(p[i]), min, max);
            }
         }
         if TEST_FOR_TRUE(filter->state) {
            rv = aaxFilterSetState(filter, filter->state);
         } else {
            rv = filter;
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxFilterSetParam(const aaxFilter f, int param, int ptype, float value)
{
   _filter_t* filter = get_filter(f);
   int rv = AAX_FALSE;
   if (filter && !is_nan(value))
   {
      unsigned slot = param >> 4;
      if ((slot < _MAX_FE_SLOTS) && filter->slot[slot])
      {
         param &= 0xF;
         if ((param >= 0) && (param < 4))
         {
            cvtfn_t cvtfn = filter_get_cvtfn(filter->type, ptype, WRITEFN, param);
            filter->slot[slot]->param[param] = cvtfn(value);
            if TEST_FOR_TRUE(filter->state) {
               aaxFilterSetState(filter, filter->state);
            }
            rv = AAX_TRUE;
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxFilterSetState(aaxFilter f, int state)
{
   _filter_t* filter = get_filter(f);
   aaxFilter rv = NULL;
   if (filter)
   {
      _flt_function_tbl *flt = _aaxFilters[filter->type-1];
      if (flt->lite || EBF_VALID(filter))
      {
         unsigned slot;

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
                  cvtfn_t cvtfn;

                  cvtfn = filter_get_cvtfn(filter->type,AAX_LINEAR, WRITEFN, i);
                  filter->slot[slot]->param[i] =
                         _MINMAX(cvtfn(filter->slot[slot]->param[i]), min, max);
               }
            }
            slot++;
         }

         rv = flt->state(filter, state);
      }
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxFilterGetState(aaxFilter f)
{
   _filter_t* filter = get_filter(f);
   int rv = AAX_FALSE;
   if (filter) {
      rv = filter->state;
   }
   return rv;
}

AAX_API float AAX_APIENTRY
aaxFilterGetParam(const aaxFilter f, int param, int ptype)
{
   _filter_t* filter = get_filter(f);
   float rv = 0.0f;
   if (filter)
   {
      int slot = param >> 4;
      if ((slot < _MAX_FE_SLOTS) && filter->slot[slot])
      {
         param &= 0xF;
         if ((param >= 0) && (param < 4))
         {
            cvtfn_t cvtfn = filter_get_cvtfn(filter->type, ptype, READFN, param);
            rv = cvtfn(filter->slot[slot]->param[param]);
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxFilterGetSlot(const aaxFilter f, unsigned slot, int ptype, float* p1, float* p2, float* p3, float* p4)
{
   aaxVec4f v;
   aaxFilter rv = aaxEffectGetSlotParams(f, slot, ptype, v);
   if(p1) *p1 = v[0];
   if(p2) *p2 = v[1];
   if(p3) *p3 = v[2];
   if(p4) *p4 = v[3];
   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxFilterGetSlotParams(const aaxFilter f, unsigned slot, int ptype, aaxVec4f p)
{
   aaxFilter rv = AAX_FALSE;
   _filter_t* filter = get_filter(f);
   if (filter && p)
   {
      if ((slot < _MAX_FE_SLOTS) && filter->slot[slot])
      {
         int i;
         for (i=0; i<4; i++)
         {
            cvtfn_t cvtfn = filter_get_cvtfn(filter->type, ptype, READFN, i);
            p[i] = cvtfn(filter->slot[slot]->param[i]);
         }
         rv = filter;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

/* internal use only, used by aaxdefs.h */
AAX_API aaxFilter AAX_APIENTRY
aaxFilterApply(aaxFilterFn fn, void *handle, aaxFilter f)
{
   if (f)
   {
      if (!fn(handle, f))
      {
         aaxFilterDestroy(f);
         f = NULL;
      }
   }
   return f;
}

AAX_API float AAX_APIENTRY
aaxFilterApplyParam(const aaxFilter f, int s, int p, int ptype)
{
   float rv = 0.0f;
   if ((p >= 0) && (p < 4))
   {
      _filter_t* filter = get_filter(f);
      if (filter)
      {
         cvtfn_t cvtfn = filter_get_cvtfn(filter->type, ptype, READFN, p);
         rv = cvtfn(filter->slot[0]->param[p]);
         free(filter);
      }
   }
   return rv;
}

_filter_t*
new_filter_handle(_aaxMixerInfo* info, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   _filter_t* rv = NULL;
   if (type <= AAX_FILTER_MAX)
   {
      _flt_function_tbl *flt = _aaxFilters[type-1];
      rv = flt->handle(info, type, p2d, p3d);
   }
   return rv;
}

_filter_t*
get_filter(aaxFilter f)
{
   _filter_t* rv = (_filter_t*)f;
   if (rv && rv->id == FILTER_ID) {
      return rv;
   }
   return NULL;
}

