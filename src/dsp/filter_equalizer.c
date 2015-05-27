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

   if (state == AAX_12DB_OCT || state == AAX_24DB_OCT ||
       state == AAX_36DB_OCT || state == AAX_48DB_OCT)
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
         int stages;

         if (state == AAX_48DB_OCT) stages = 4;
         else if (state == AAX_36DB_OCT) stages = 3;
         else if (state == AAX_24DB_OCT) stages = 2;
         else stages = 1;

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
         Q = filter->slot[EQUALIZER_LF]->param[AAX_RESONANCE];
         cptr = flt->coeff;

         flt->lf_gain = fabs(filter->slot[EQUALIZER_LF]->param[AAX_LF_GAIN]);
         if (flt->lf_gain < GMATH_128DB) flt->lf_gain = 0.0f;
         else if (fabs(flt->lf_gain - 1.0f) < GMATH_128DB) flt->lf_gain = 1.0f;

         flt->hf_gain = fabs(filter->slot[EQUALIZER_LF]->param[AAX_HF_GAIN]);
         if (flt->hf_gain < GMATH_128DB) flt->hf_gain = 0.0f;
         else if (fabs(flt->hf_gain - 1.0f) < GMATH_128DB) flt->hf_gain = 1.0f;

         flt->type = (flt->lf_gain >= flt->hf_gain) ? LOWPASS : HIGHPASS;
         if (flt->type == HIGHPASS)
         {
            float f = flt->lf_gain;
            flt->lf_gain = flt->hf_gain;
            flt->hf_gain = f;
         }

         k = 0.0f; // for now
         _aax_butterworth_compute(fcl, filter->info->frequency, cptr, &k, Q, stages, flt->type);

         flt->no_stages = stages;
         flt->k = k;

         /* HF frequency setup */
         flt = filter->slot[EQUALIZER_HF]->data;
         Q = filter->slot[EQUALIZER_HF]->param[AAX_RESONANCE];
         cptr = flt->coeff;
         k = 1.0f;

         flt->lf_gain = fabs(filter->slot[EQUALIZER_HF]->param[AAX_LF_GAIN]);
         if (flt->lf_gain < GMATH_128DB) flt->lf_gain = 0.0f;
         else if (fabs(flt->lf_gain - 1.0f) < GMATH_128DB) flt->lf_gain = 1.0f;

         flt->hf_gain = fabs(filter->slot[EQUALIZER_HF]->param[AAX_HF_GAIN]);
         if (flt->hf_gain < GMATH_128DB) flt->hf_gain = 0.0f;
         else if (fabs(flt->hf_gain - 1.0f) < GMATH_128DB) flt->hf_gain = 1.0f;

         flt->type = (flt->lf_gain >= flt->hf_gain) ? LOWPASS : HIGHPASS;
         if (flt->type == HIGHPASS)
         {
            float f = flt->lf_gain;
            flt->lf_gain = flt->hf_gain;
            flt->hf_gain = f;
         }
         _aax_butterworth_compute(fch, filter->info->frequency, cptr, &k, Q, stages, flt->type);

         flt->no_stages = stages;
         flt->k = k;
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

