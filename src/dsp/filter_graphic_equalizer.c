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
_aaxGraphicEqualizerCreate(aaxConfig config, enum aaxFilterType type)
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
         flt->slot[0]->param[0] = 1.0f; flt->slot[1]->param[0] = 1.0f;
         flt->slot[0]->param[1] = 1.0f; flt->slot[1]->param[1] = 1.0f;
         flt->slot[0]->param[2] = 1.0f; flt->slot[1]->param[2] = 1.0f;
         flt->slot[0]->param[3] = 1.0f; flt->slot[1]->param[3] = 1.0f;
         rv = (aaxFilter)flt;
      }
   }
   return rv;
}

static int
_aaxGraphicEqualizerDestroy(aaxFilter f)
{
   _filter_t* filter = get_filter(f);
   int rv = AAX_FALSE;
   if (filter)
   {
      free(filter->slot[1]->data);
      filter->slot[1]->data = NULL;
      free(filter->slot[0]->data);
      filter->slot[0]->data = NULL;
      free(filter);
      rv = AAX_TRUE;
   }
   return rv;
}

static aaxFilter
_aaxGraphicEqualizerSetState(aaxFilter f, int state)
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
         _aaxRingBufferEqualizerData *eq = filter->slot[EQUALIZER_HF]->data;

         /*
          * use EQUALIZER_HF to distinquish between GRAPHIC_EQUALIZER
          * and FREQ_FILTER (which only uses EQUALIZER_LF)
          */
         if (eq == NULL)
         {
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
                  Q = 2.0f;
                  fc = expf((float)pos*fband)*67.0f;
                  iir_compute_coefs(fc,filter->info->frequency,cptr,&k,Q);
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
            flt->hf_gain = gain_hf;
            flt->lf_gain = gain_lf;
            do
            {
               int pos = s*4+b;
               float gain;

               flt = &eq->band[pos-1];

               gain_hf = gain_lf;
               gain_lf = filter->slot[s]->param[--b];

               /* gain_hf can never get below 0.001f */
               gain = gain_lf - gain_hf;
               flt->hf_gain = 0.0f;
               flt->lf_gain = gain;

               if (b == 0)
               {
                  b += 4;
                  s--;
               }
            }
            while (s >= 0);
         }
         else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      }
      else
      {
         free(filter->slot[EQUALIZER_HF]->data);
         filter->slot[EQUALIZER_HF]->data = NULL;
      }
   }
#endif
   rv = filter;
   return rv;
}

/* -------------------------------------------------------------------------- */

static _filter_t*
_aaxNewGraphicEqualizerHandle(_aaxMixerInfo* info, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
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
         rv->slot[0]->param[0] = 1.0f; rv->slot[1]->param[0] = 1.0f;
         rv->slot[0]->param[1] = 1.0f; rv->slot[1]->param[1] = 1.0f;
         rv->slot[0]->param[2] = 1.0f; rv->slot[1]->param[2] = 1.0f;
         rv->slot[0]->param[3] = 1.0f; rv->slot[1]->param[3] = 1.0f;
         rv->slot[0]->data = NULL;     rv->slot[1]->data = NULL;
      }
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxGraphicEqualizer =
{
   AAX_FALSE,
   "AAX_graphic_equalizer",
   (_aaxFilterCreate*)&_aaxGraphicEqualizerCreate,
   (_aaxFilterDestroy*)&_aaxGraphicEqualizerDestroy,
   (_aaxFilterSetState*)&_aaxGraphicEqualizerSetState,
   (_aaxNewFilterHandle*)&_aaxNewGraphicEqualizerHandle
};

