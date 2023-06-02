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

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
#endif

#include <aax/aax.h>

#include <base/memory.h>
#include <base/types.h>		/* for rintf */
#include <base/gmath.h>

#include <dsp/filters.h>
#include <dsp/effects.h>

#include "api.h"

#define WRITEFN		0
#define EPS		1e-5
#define READFN		!WRITEFN

AAX_API aaxFilter AAX_APIENTRY
aaxFilterCreate(aaxDSP config, enum aaxFilterType type)
{
   _handle_t *handle = get_handle(config, __func__);
   _aaxMixerInfo *info = (handle && handle->info) ? handle->info : _info;
   aaxFilter rv = NULL;

   if (info && (type > 0 && type < AAX_FILTER_MAX))
   {
      _flt_function_tbl *flt = _aaxFilters[type-1];
      rv = flt->create(info, type);
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

AAX_API int AAX_APIENTRY
aaxFilterSetSlot(aaxFilter f, unsigned slot, int ptype, float p1, float p2, float p3, float p4)
{
   aaxVec4f v = { p1, p2, p3, p4 };
   return aaxFilterSetSlotParams(f, slot, ptype, v);
}

AAX_API int AAX_APIENTRY
aaxFilterSetSlotParams(aaxFilter f, unsigned slot, int ptype, aaxVec4f p)
{
   _filter_t* filter = get_filter(f);
   int rv = AAX_FALSE;
   if (filter && p)
   {
      if ((slot < _MAX_FE_SLOTS) && filter->slot[slot])
      {
         int i;
         for (i=0; i<4; i++)
         {
            if (!is_nan(p[i]))
            {
               _flt_function_tbl *flt = _aaxFilters[filter->type-1];
               filter->slot[slot]->param[i] =
                                  flt->limit(flt->get(p[i], ptype, i), slot, i);
            }
         }
         if TEST_FOR_TRUE(filter->state) {
            rv = aaxFilterSetState(filter, filter->state);
         } else {
            rv = AAX_TRUE;
         }
      }
      else
      {
         void *handle = filter->handle;
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      __aaxErrorSet(AAX_INVALID_HANDLE, __func__);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxFilterSetParam(const aaxFilter f, int param, int ptype, float value)
{
   _filter_t* filter = get_filter(f);
   unsigned slot = param >> 4;
   int rv = __release_mode;

   param &= 0xF;
   if (!rv)
   {
      void *handle = filter ? filter->handle : NULL;
      if (!filter) {
         __aaxErrorSet(AAX_INVALID_HANDLE, __func__);
      } else if ((slot >=_MAX_FE_SLOTS) || !filter->slot[slot]) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (param < 0 || param >= 4) {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      } else if (is_nan(value)) {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 2);
      } else {
         rv = AAX_TRUE;
      }
   }

   if (rv)
   {
      _flt_function_tbl *flt = _aaxFilters[filter->type-1];
      filter->slot[slot]->param[param] = flt->get(value, ptype, param);
      if TEST_FOR_TRUE(filter->state) {
         aaxFilterSetState(filter, filter->state);
      }
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxFilterAddBuffer(UNUSED(aaxFilter f), UNUSED(aaxBuffer b))
{
   return AAX_FALSE;
}

AAX_API int AAX_APIENTRY
aaxFilterSetState(aaxFilter f, int state)
{
   _filter_t* filter = get_filter(f);
   int rv = AAX_FALSE;
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
            int i;
            for(i=0; i<4; i++)
            {
               if (!is_nan(filter->slot[slot]->param[i]))
               {
                  filter->slot[slot]->param[i] =
                              flt->limit(filter->slot[slot]->param[i], slot, i);
               }
            }
            slot++;
         }

         rv = flt->state(filter, state) ? AAX_TRUE : AAX_FALSE;
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
      void *handle = filter->handle;
      int slot = param >> 4;
      if ((slot < _MAX_FE_SLOTS) && filter->slot[slot])
      {
         param &= 0xF;
         if ((param >= 0) && (param < 4))
         {
            _flt_function_tbl *flt = _aaxFilters[filter->type-1];
            rv = flt->set(filter->slot[slot]->param[param], ptype, param);
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
      __aaxErrorSet(AAX_INVALID_HANDLE, __func__);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxFilterGetSlot(const aaxFilter f, unsigned slot, int ptype, float* p1, float* p2, float* p3, float* p4)
{
   aaxVec4f v;
   int rv = aaxEffectGetSlotParams(f, slot, ptype, v);
   if(p1) *p1 = v[0];
   if(p2) *p2 = v[1];
   if(p3) *p3 = v[2];
   if(p4) *p4 = v[3];
   return rv;
}

AAX_API int AAX_APIENTRY
aaxFilterGetSlotParams(const aaxFilter f, unsigned slot, int ptype, aaxVec4f p)
{
   int rv = AAX_FALSE;
   _filter_t* filter = get_filter(f);
   if (filter)
   {
      void *handle = filter->handle;
      if (p)
      {
         if ((slot < _MAX_FE_SLOTS) && filter->slot[slot])
         {
            int i;
            for (i=0; i<4; i++)
            {
               _flt_function_tbl *flt = _aaxFilters[filter->type-1];
               p[i] = flt->set(filter->slot[slot]->param[i], ptype, i);
            }
            rv = AAX_TRUE;
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
      } else {
         _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
      }
   }
   else {
      __aaxErrorSet(AAX_INVALID_HANDLE, __func__);
   }
   return rv;
}

AAX_API const char* AAX_APIENTRY
aaxFilterGetNameByType(aaxConfig handle, enum aaxFilterType type)
{
   const char *rv = NULL;
   if (type < AAX_FILTER_MAX) {
      rv = _aaxFilters[type-1]->name;
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   return rv;
}

