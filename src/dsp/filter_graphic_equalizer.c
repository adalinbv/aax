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
_aaxGraphicEqualizerCreate(_handle_t *handle, enum aaxFilterType type)
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
      flt->slot[0]->param[0] = 1.0f; flt->slot[1]->param[0] = 1.0f;
      flt->slot[0]->param[1] = 1.0f; flt->slot[1]->param[1] = 1.0f;
      flt->slot[0]->param[2] = 1.0f; flt->slot[1]->param[2] = 1.0f;
      flt->slot[0]->param[3] = 1.0f; flt->slot[1]->param[3] = 1.0f;
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxGraphicEqualizerDestroy(_filter_t* filter)
{
   free(filter->slot[1]->data);
   filter->slot[1]->data = NULL;
   free(filter->slot[0]->data);
   filter->slot[0]->data = NULL;
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxGraphicEqualizerSetState(_filter_t* filter, int state)
{
   aaxFilter rv = NULL;

   if (state == AAX_FILTER_12DB_OCT || state == AAX_FILTER_24DB_OCT ||
       state == AAX_FILTER_36DB_OCT)
   {
      _aaxRingBufferEqualizerData *eq = filter->slot[EQUALIZER_HF]->data;

      /*
       * use EQUALIZER_HF to distinquish between GRAPHIC_EQUALIZER
       * and FREQ_FILTER (which only uses EQUALIZER_LF)
       */
      if (eq == NULL)
      {
         int stages;

         if (state == AAX_FILTER_36DB_OCT) stages = 3;
         else if (state == AAX_FILTER_24DB_OCT) stages = 2;
         else stages = 1;

         eq = calloc(1, sizeof(_aaxRingBufferEqualizerData));
         filter->slot[EQUALIZER_LF]->data = NULL;
         filter->slot[EQUALIZER_HF]->data = eq;

         if (eq)	/* fill in the fixed frequencies */
         {
            float fband = logf(44000.0f/67.0f)/8.0f;
            int pos = 7;
            do
            {
               _aaxRingBufferFreqFilterData *flt;
               float *cptr, fc, k, Q;

               flt = &eq->band[pos];
               cptr = flt->coeff;

               k = 1.0f;
               Q = 1.0f;
               fc = expf((float)pos*fband)*67.0f;
               _aax_butterworth_iir_compute(fc,filter->info->frequency,cptr,&k,Q,stages);
               flt->no_stages = stages;
               flt->k = k;
            }
            while (--pos >= 0);
         }
      }

      if (eq)		/* fill in the gains */
      {
         float gain_hf=filter->slot[EQUALIZER_HF]->param[AAX_GAIN_BAND3];
         float gain_lf=filter->slot[EQUALIZER_HF]->param[AAX_GAIN_BAND2];
         _aaxRingBufferFreqFilterData *flt = &eq->band[6];
         int s = EQUALIZER_HF, b = AAX_GAIN_BAND2;

         eq = filter->slot[EQUALIZER_HF]->data;
         flt->hf_gain = (fabs(gain_hf) > GMATH_128DB) ? gain_hf : 0.0f;
         flt->lf_gain = (fabs(gain_lf) > GMATH_128DB) ? gain_lf : 0.0f;
         flt->hf_gain_prev = 1.0f;
         do
         {
            int pos = s*4+b;
            float gain;

            flt = &eq->band[pos-1];	// next band

            gain_hf = gain_lf;
            gain_lf = filter->slot[s]->param[--b];

            /* gain_hf can never get below 0.001f */
            gain = gain_lf - gain_hf;
            flt->hf_gain = 0.0f;
            flt->lf_gain = (fabs(gain) > GMATH_128DB) ? gain : 0.0f;
            flt->hf_gain_prev = 1.0f;

            if (b == 0)
            {
               b += 4;
               s--;
            }
         }
         while (s >= 0);
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      rv = filter;
   }
   else if (state == AAX_FALSE)
   {
      free(filter->slot[EQUALIZER_HF]->data);
      filter->slot[EQUALIZER_HF]->data = NULL;
      rv = filter;
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }

   return rv;
}

/* -------------------------------------------------------------------------- */

static _filter_t*
_aaxNewGraphicEqualizerHandle(_aaxMixerInfo* info, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
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
      rv->slot[0]->param[0] = 1.0f; rv->slot[1]->param[0] = 1.0f;
      rv->slot[0]->param[1] = 1.0f; rv->slot[1]->param[1] = 1.0f;
      rv->slot[0]->param[2] = 1.0f; rv->slot[1]->param[2] = 1.0f;
      rv->slot[0]->param[3] = 1.0f; rv->slot[1]->param[3] = 1.0f;
      rv->slot[0]->data = NULL;     rv->slot[1]->data = NULL;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxGraphicEqualizer =
{
   AAX_TRUE,
   "AAX_graphic_equalizer",
   (_aaxFilterCreate*)&_aaxGraphicEqualizerCreate,
   (_aaxFilterDestroy*)&_aaxGraphicEqualizerDestroy,
   (_aaxFilterSetState*)&_aaxGraphicEqualizerSetState,
   (_aaxNewFilterHandle*)&_aaxNewGraphicEqualizerHandle
};

