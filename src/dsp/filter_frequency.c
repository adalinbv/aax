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
_aaxFrequencyFilterCreate(aaxConfig config, enum aaxFilterType type)
{
   _handle_t *handle = get_handle(config);
   aaxFilter rv = NULL;
   if (handle)
   {
      unsigned int size = sizeof(_filter_t) + 2*sizeof(_aaxFilterInfo);
     _filter_t* flt = calloc(1, size);

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
         _aaxSetDefaultFilter2d(flt->slot[0], flt->pos);
         rv = (aaxFilter)flt;
      }
   }
   return rv;
}

static int
_aaxFrequencyFilterDestroy(aaxFilter f)
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
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

static aaxFilter
_aaxFrequencyFilterSetState(aaxFilter f, int state)
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

   if (filter)
   {
      switch (state & ~AAX_INVERSE)
      {
      case AAX_TRUE:
      case AAX_TRIANGLE_WAVE:
      case AAX_SINE_WAVE:
      case AAX_SQUARE_WAVE:
      case AAX_SAWTOOTH_WAVE:
      case AAX_ENVELOPE_FOLLOW:
      {
         _aaxRingBufferFreqFilterData *flt = filter->slot[0]->data;
         if (flt == NULL)
         {
            flt = calloc(1, sizeof(_aaxRingBufferFreqFilterData));
            flt->fs = filter->info ? filter->info->frequency : 48000.0f;
            filter->slot[0]->data = flt;
         }

         if (flt)
         {
            float fc = filter->slot[0]->param[AAX_CUTOFF_FREQUENCY];
            float Q = filter->slot[0]->param[AAX_RESONANCE];
            float *cptr = flt->coeff;
            float fs = flt->fs; 
            float k = 1.0f;

//          flt->fs = fs = filter->info->frequency;
            iir_compute_coefs(fc, fs, cptr, &k, Q);
            flt->lf_gain = filter->slot[0]->param[AAX_LF_GAIN];
            flt->hf_gain = filter->slot[0]->param[AAX_HF_GAIN];
            flt->Q = Q;
            flt->k = k;

            // Non-Manual only
            if ((state & ~AAX_INVERSE) != AAX_TRUE && EBF_VALID(filter)
                && filter->slot[1])
            {
               _aaxRingBufferLFOData* lfo = flt->lfo;

               if (lfo == NULL) {
                  lfo = flt->lfo = malloc(sizeof(_aaxRingBufferLFOData));
               }

               if (lfo)
               {
                  int t;

                  lfo->min=filter->slot[0]->param[AAX_CUTOFF_FREQUENCY];
                  lfo->max=filter->slot[1]->param[AAX_CUTOFF_FREQUENCY];
                  if (fabsf(lfo->max - lfo->min) < 200.0f)
                  { 
                     lfo->min = 0.5f*(lfo->min + lfo->max);
                     lfo->max = lfo->min;
                  }
                  else if (lfo->max < lfo->min)
                  {
                     float f = lfo->max;
                     lfo->max = lfo->min;
                     lfo->min = f;
                  }

                  /* sweeprate */
                  lfo->f = filter->slot[1]->param[AAX_RESONANCE];
                  lfo->inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;
                  lfo->convert = _linear; // _log2lin;

                  for (t=0; t<_AAX_MAX_SPEAKERS; t++)
                  {
                     lfo->step[t] = 2.0f * lfo->f;
                     lfo->step[t] *= (lfo->max - lfo->min);
                     lfo->step[t] /= filter->info->period_rate;
                     lfo->value[t] = lfo->max;
                     switch (state & ~AAX_INVERSE)
                     {
                     case AAX_SAWTOOTH_WAVE:
                        lfo->step[t] *= 0.5f;
                        break;
                     case AAX_ENVELOPE_FOLLOW:
                        lfo->step[t] = ENVELOPE_FOLLOW_STEP_CVT(lfo->f);
                        break;
                     default:
                        break;
                     }
                  }

                  lfo->envelope = AAX_FALSE;
                  lfo->get = _aaxRingBufferLFOGetFixedValue;
                  if ((lfo->max - lfo->min) > 0.01f)
                  {
                     switch (state & ~AAX_INVERSE)
                     {
                     case AAX_TRIANGLE_WAVE:
                        lfo->get = _aaxRingBufferLFOGetTriangle;
                        break;
                     case AAX_SINE_WAVE:
                        lfo->get = _aaxRingBufferLFOGetSine;
                        break;
                     case AAX_SQUARE_WAVE:
                        lfo->get = _aaxRingBufferLFOGetSquare;
                        break;
                     case AAX_SAWTOOTH_WAVE:
                        lfo->get = _aaxRingBufferLFOGetSawtooth;
                        break;
                     case AAX_ENVELOPE_FOLLOW:
                        lfo->get = _aaxRingBufferLFOGetGainFollow;
                        lfo->envelope = AAX_TRUE;
                        break;
                     default:
                        _aaxErrorSet(AAX_INVALID_PARAMETER);
                        break;
                     }
                  }
               } /* flt->lfo */
            } /* flt */
            else if ((state & ~AAX_INVERSE) == AAX_TRUE)
            {
               free(flt->lfo);
               flt->lfo = NULL;
            }
         }
         else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
         break;
      }
      case AAX_FALSE:
         free(filter->slot[0]->data);
         filter->slot[0]->data = NULL;
         break;
      default:
         _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      }
   }
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewFrequencyFilterHandle(_aaxMixerInfo* info, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   _filter_t* rv = NULL;
   if (type < AAX_FILTER_MAX)
   {
      unsigned int size = sizeof(_filter_t) + 2*sizeof(_aaxFilterInfo);

      rv = calloc(1, size);
      if (rv)
      {
         char *ptr = (char*)rv + sizeof(_filter_t);
         _aaxRingBufferFreqFilterData *freq;

         rv->id = FILTER_ID;
         rv->info = info ? info : _info;
         rv->slot[0] = (_aaxFilterInfo*)ptr;
         rv->pos = _flt_cvt_tbl[type].pos;
         rv->state = p2d->filter[rv->pos].state;
         rv->type = type;

         size = sizeof(_aaxFilterInfo);
         freq = (_aaxRingBufferFreqFilterData *)p2d->filter[rv->pos].data;
         rv->slot[1] = (_aaxFilterInfo*)(ptr + size);
         /* reconstruct rv->slot[1] */
         if (freq && freq->lfo)
         {
            rv->slot[1]->param[AAX_RESONANCE] = freq->lfo->f;
            rv->slot[1]->param[AAX_CUTOFF_FREQUENCY] = freq->lfo->max;
         }
         else
         {
            int type = AAX_FREQUENCY_FILTER;
            memcpy(rv->slot[1], &_flt_minmax_tbl[1][type], size);
         }
         memcpy(rv->slot[0], &p2d->filter[rv->pos], size);
         rv->slot[0]->data = NULL;
      }
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxFrequencyFilter =
{
   "AAX_frequency_filter",
   (_aaxFilterCreate*)&_aaxFrequencyFilterCreate,
   (_aaxFilterDestroy*)&_aaxFrequencyFilterDestroy,
   (_aaxFilterSetState*)&_aaxFrequencyFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewFrequencyFilterHandle
};

