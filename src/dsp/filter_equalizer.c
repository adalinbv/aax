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
_aaxEqualizerCreate(_handle_t *handle, enum aaxFilterType type)
{
   unsigned int size = sizeof(_filter_t);
   _filter_t* flt;
   aaxFilter rv = NULL;

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
   return rv;
}

static int
_aaxEqualizerDestroy(_filter_t* filter)
{
   filter->slot[1]->data = NULL;
   free(filter->slot[0]->data);
   filter->slot[0]->data = NULL;
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxEqualizerSetState(_filter_t* filter, int state)
{
   aaxFilter rv = NULL;

   if (state == AAX_12DB_OCT)
   {
      _aaxRingBufferFreqFilterData *flt_lf = filter->slot[EQUALIZER_LF]->data;
      _aaxRingBufferFreqFilterData *flt_hf = filter->slot[EQUALIZER_HF]->data;
      if (flt_lf == NULL)
      {
         char *ptr;
         flt_lf = calloc(EQUALIZER_MAX,sizeof(_aaxRingBufferFreqFilterData));
         flt_lf->no_stages = 1;
         filter->slot[EQUALIZER_LF]->data = flt_lf;

         ptr = (char*)flt_lf + sizeof(_aaxRingBufferFreqFilterData);
         flt_hf = (_aaxRingBufferFreqFilterData*)ptr;
         flt_lf->no_stages = 1;
         filter->slot[EQUALIZER_HF]->data = flt_hf;
      }

      if (flt_lf && flt_hf)
      {
         float lf_gain, mf_gain, hf_gain;
         float *cptr, fs, fcl, fch, k, Q;

         fs = filter->info->frequency;

         /* frequencies */
         fcl = filter->slot[EQUALIZER_LF]->param[AAX_CUTOFF_FREQUENCY];
         fch = filter->slot[EQUALIZER_HF]->param[AAX_CUTOFF_FREQUENCY];
         if (fabsf(fch - fcl) < 200.0f) {
            fcl *= 0.9f; fch *= 1.1f;
         } else if (fch < fcl) {
            float f = fch; fch = fcl; fcl = f;
         }
         filter->slot[EQUALIZER_LF]->param[AAX_CUTOFF_FREQUENCY] = fcl;
         filter->slot[EQUALIZER_HF]->param[AAX_CUTOFF_FREQUENCY] = fch;

         /* gains */
         lf_gain = fabs(filter->slot[EQUALIZER_LF]->param[AAX_LF_GAIN]);
         if (fabs(lf_gain - 1.0f) < GMATH_128DB) lf_gain = 1.0f;
         else if (lf_gain < GMATH_128DB) lf_gain = 0.0f;

         mf_gain = fabs(filter->slot[EQUALIZER_LF]->param[AAX_HF_GAIN] +
                        filter->slot[EQUALIZER_HF]->param[AAX_LF_GAIN])*0.5f;
         if (fabs(mf_gain - 1.0f) < GMATH_128DB) mf_gain = 1.0f;
         else if (mf_gain < GMATH_128DB) mf_gain = 0.0f;

         hf_gain = fabs(filter->slot[EQUALIZER_HF]->param[AAX_HF_GAIN]);
         if (fabs(hf_gain - 1.0f) < GMATH_128DB) hf_gain = 1.0f;
         else if (hf_gain < GMATH_128DB) hf_gain = 0.0f;

         /* LF frequency setup */
         Q = filter->slot[EQUALIZER_LF]->param[AAX_RESONANCE];
         cptr = flt_lf->coeff;
         if (lf_gain >= mf_gain)
         {
            flt_lf->type = LOWPASS;
            flt_lf->lf_gain = lf_gain;
            flt_lf->hf_gain = mf_gain;
         }
         else
         {
            flt_lf->type = HIGHPASS;
            flt_lf->lf_gain = mf_gain;
            flt_lf->hf_gain = lf_gain;
         }
         k = flt_lf->hf_gain/flt_lf->lf_gain;
         _aax_butterworth_compute(fcl, fs, cptr, &k, Q, 1, flt_lf->type);
         flt_lf->hf_gain = 0.0f;
         flt_lf->k = k;

         /* HF frequency setup */
         Q = filter->slot[EQUALIZER_HF]->param[AAX_RESONANCE];
         cptr = flt_hf->coeff;
         if (mf_gain >= hf_gain)
         {
            flt_hf->type = LOWPASS;
            flt_hf->lf_gain = mf_gain;
            flt_hf->hf_gain = hf_gain;
         }
         else
         {
            flt_hf->type = HIGHPASS;
            flt_hf->lf_gain = hf_gain;
            flt_hf->hf_gain = mf_gain;
         }
         k = flt_hf->hf_gain/flt_hf->lf_gain;
         _aax_butterworth_compute(fch, fs, cptr, &k, Q, 1, flt_hf->type);
         flt_hf->hf_gain = 0.0f;
         flt_hf->k = k;
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      rv = filter;
   }
   else if (state == AAX_FALSE)
   {
      free(filter->slot[EQUALIZER_LF]->data);
      filter->slot[EQUALIZER_LF]->data = NULL;
      rv = filter;
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }

   return rv;
}

static _filter_t*
_aaxNewEqualizerHandle(_aaxMixerInfo* info, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   unsigned int size = sizeof(_filter_t);
   _filter_t* rv = NULL;

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
   return rv;
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxEqualizer =
{
   AAX_FALSE,
   "AAX_equalizer",
   (_aaxFilterCreate*)&_aaxEqualizerCreate,
   (_aaxFilterDestroy*)&_aaxEqualizerDestroy,
   (_aaxFilterSetState*)&_aaxEqualizerSetState,
   (_aaxNewFilterHandle*)&_aaxNewEqualizerHandle
};

