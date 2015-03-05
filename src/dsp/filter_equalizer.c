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
_aaxEqualizerCreate(aaxConfig config, enum aaxFilterType type)
{
   _handle_t *handle = get_handle(config);
   aaxFilter rv = NULL;
   if (handle)
   {
      unsigned int size = sizeof(_filter_t);
     _filter_t* flt;

      size += EQUALIZER_MAX*sizeof(_aaxFilterInfo);
      flt = calloc(1, size);
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
         flt->slot[1] = (_aaxFilterInfo*)(ptr + size);
         _aaxSetDefaultFilter2d(flt->slot[1], flt->pos);
         _aaxSetDefaultFilter2d(flt->slot[0], flt->pos);
         rv = (aaxFilter)flt;
      }
   }
   return rv;
}

static int
_aaxEqualizerDestroy(aaxFilter f)
{
   _filter_t* filter = get_filter(f);
   int rv = AAX_FALSE;
   if (filter)
   {
      filter->slot[1]->data = NULL;
      free(filter->slot[0]->data);
      filter->slot[0]->data = NULL;
      free(filter);
      rv = AAX_TRUE;
   }
   return rv;
}

static aaxFilter
_aaxEqualizerSetState(aaxFilter f, int state)
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

#if !ENABLE_LITE
   if EBF_VALID(filter)
   {
      if TEST_FOR_TRUE(state)
      {
         _aaxRingBufferFreqFilterData *flt = filter->slot[EQUALIZER_LF]->data;
         if (flt == NULL)
         {
            char *ptr;
            flt=calloc(EQUALIZER_MAX,sizeof(_aaxRingBufferFreqFilterData));
            filter->slot[EQUALIZER_LF]->data = flt;

            ptr = (char*)flt + sizeof(_aaxRingBufferFreqFilterData);
            flt = (_aaxRingBufferFreqFilterData*)ptr;
            filter->slot[EQUALIZER_HF]->data = flt;
         }

         if (flt)
         {
            float *cptr, fcl, fch, k, Q;

            fcl = filter->slot[EQUALIZER_LF]->param[AAX_CUTOFF_FREQUENCY];
            fch = filter->slot[EQUALIZER_HF]->param[AAX_CUTOFF_FREQUENCY];
            if (fabsf(fch - fcl) < 200.0f) {
               fcl *= 0.9f; fch *= 1.1f;
            } else if (fch < fcl) {
               float f = fch; fch = fcl; fcl = f;
            }
            filter->slot[EQUALIZER_LF]->param[AAX_CUTOFF_FREQUENCY] = fcl;

            /* LF frequency setup */
            flt = filter->slot[EQUALIZER_LF]->data;
            cptr = flt->coeff;
            k = 1.0f;
            Q = filter->slot[EQUALIZER_LF]->param[AAX_RESONANCE];
            iir_compute_coefs(fcl, filter->info->frequency, cptr, &k, Q);
            flt->lf_gain = filter->slot[EQUALIZER_LF]->param[AAX_LF_GAIN];
            flt->hf_gain = filter->slot[EQUALIZER_LF]->param[AAX_HF_GAIN];
            flt->k = k;

            /* HF frequency setup */
            flt = filter->slot[EQUALIZER_HF]->data;
            cptr = flt->coeff;
            k = 1.0f;
            Q = filter->slot[EQUALIZER_HF]->param[AAX_RESONANCE];
            iir_compute_coefs(fch, filter->info->frequency, cptr, &k, Q);
            flt->lf_gain = filter->slot[EQUALIZER_HF]->param[AAX_LF_GAIN];
            flt->hf_gain = filter->slot[EQUALIZER_HF]->param[AAX_HF_GAIN];
            flt->k = k;
         }
         else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      }
      else
      {
         free(filter->slot[EQUALIZER_LF]->data);
         filter->slot[EQUALIZER_LF]->data = NULL;
      }
   }
#endif
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewEqualizerHandle(_aaxMixerInfo* info, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   _filter_t* rv = NULL;
   if (type < AAX_FILTER_MAX)
   {
      unsigned int size = sizeof(_filter_t);

      size += EQUALIZER_MAX*sizeof(_aaxFilterInfo);
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
         rv->slot[1] = (_aaxFilterInfo*)(ptr + size);
         memcpy(rv->slot[1], &p2d->filter[rv->pos], size);
         rv->slot[1]->data = NULL;
         memcpy(rv->slot[0], &p2d->filter[rv->pos], size);
        rv->slot[0]->data = NULL;
      }
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxEqualizer =
{
   "AAX_equalizer",
   (_aaxFilterCreate*)&_aaxEqualizerCreate,
   (_aaxFilterDestroy*)&_aaxEqualizerDestroy,
   (_aaxFilterSetState*)&_aaxEqualizerSetState,
   (_aaxNewFilterHandle*)&_aaxNewEqualizerHandle
};

