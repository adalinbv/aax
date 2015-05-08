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
_aaxFrequencyFilterCreate(_handle_t *handle, enum aaxFilterType type)
{
   unsigned int size = sizeof(_filter_t) + 2*sizeof(_aaxFilterInfo);
   _filter_t* flt = calloc(1, size);
   aaxFilter rv = NULL;

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
   return rv;
}

static int
_aaxFrequencyFilterDestroy(_filter_t* filter)
{
   filter->slot[1]->data = NULL;
   free(filter->slot[0]->data);
   filter->slot[0]->data = NULL;
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxFrequencyFilterSetState(_filter_t* filter, int state)
{
   int mask, istate, wstate;
   aaxFilter rv = NULL;

   mask = AAX_TRIANGLE_WAVE|AAX_SINE_WAVE|AAX_SQUARE_WAVE|AAX_SAWTOOTH_WAVE |
          AAX_ENVELOPE_FOLLOW;

   istate = state & ~AAX_INVERSE;
   wstate = istate & mask;

   switch (wstate || state == AAX_FILTER_6DB_OCT || state == AAX_FILTER_12DB_OCT
                || state == AAX_FILTER_24DB_OCT || state == AAX_FILTER_36DB_OCT)
   {
   case AAX_FILTER_6DB_OCT:
   case AAX_FILTER_12DB_OCT:
   case AAX_FILTER_24DB_OCT:
   case AAX_FILTER_36DB_OCT:
   case AAX_TRIANGLE_WAVE:
   case AAX_SINE_WAVE:
   case AAX_SQUARE_WAVE:
   case AAX_SAWTOOTH_WAVE:
   case AAX_ENVELOPE_FOLLOW:
   {
      _aaxRingBufferFreqFilterData *flt = filter->slot[0]->data;
      int stages;

      if (flt == NULL)
      {
         flt = calloc(1, sizeof(_aaxRingBufferFreqFilterData));
         flt->fs = filter->info ? filter->info->frequency : 48000.0f;
         filter->slot[0]->data = flt;
      }

      if (state & AAX_FILTER_36DB_OCT) stages = 3;
      else if (state & AAX_FILTER_24DB_OCT) stages = 2;
      else if (state & AAX_FILTER_6DB_OCT) stages = 0;
      else stages = 1;

      if (flt)
      {
         float fc = filter->slot[0]->param[AAX_CUTOFF_FREQUENCY];
         float Q = filter->slot[0]->param[AAX_RESONANCE];
         float *cptr = flt->coeff;
         float fs = flt->fs; 
         float k = 1.0f;

         flt->lf_gain = filter->slot[0]->param[AAX_LF_GAIN];
         if (flt->lf_gain < GMATH_128DB) flt->lf_gain = 0.0f;
         flt->hf_gain = filter->slot[0]->param[AAX_HF_GAIN];
         if (flt->hf_gain < GMATH_128DB) flt->hf_gain = 0.0f;
         flt->hf_gain_prev = 1.0f;
         flt->no_stages = stages;
         flt->Q = Q;
         if (stages) // 2nd, 4th or 8th order filter
         {
            _aax_butterworth_iir_compute(fc, fs, cptr, &k, Q, stages);
            flt->k = k;
         }
         else // 1st order filter
         {
            _aax_movingaverage_fir_compute(fc, fs, &k);
            flt->k = k;
         }

         // Non-Manual only
         if (wstate && EBF_VALID(filter) && filter->slot[1])
         {
            _aaxRingBufferLFOData* lfo = flt->lfo;

            if (lfo == NULL) {
               lfo = flt->lfo = malloc(sizeof(_aaxRingBufferLFOData));
            }

            if (lfo)
            {
               int t;

               lfo->min = filter->slot[0]->param[AAX_CUTOFF_FREQUENCY];
               lfo->max = filter->slot[1]->param[AAX_CUTOFF_FREQUENCY];
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
                  switch (wstate)
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
                  switch (wstate)
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
         else if (wstate == AAX_FALSE)
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
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewFrequencyFilterHandle(_aaxMixerInfo* info, enum aaxFilterType type, _aax2dProps* p2d, _aax3dProps* p3d)
{
   unsigned int size = sizeof(_filter_t) + 2*sizeof(_aaxFilterInfo);
   _filter_t* rv = calloc(1, size);

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
   return rv;
}

_flt_function_tbl _aaxFrequencyFilter =
{
   AAX_TRUE,
   "AAX_frequency_filter",
   (_aaxFilterCreate*)&_aaxFrequencyFilterCreate,
   (_aaxFilterDestroy*)&_aaxFrequencyFilterDestroy,
   (_aaxFilterSetState*)&_aaxFrequencyFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewFrequencyFilterHandle
};

/* -------------------------------------------------------------------------- */

/* 1st order: 6dB/oct (X[k] = a*X[k-1] + (1-a)*Y[k]) */
/* http://lorien.ncl.ac.uk/ming/filter/fillpass.htm  */
void
_batch_movingavg_cpu(int32_ptr d, const_int32_ptr sptr, size_t num, float *hist, float a1)
{
   if (num)
   {
      int32_ptr s = (int32_ptr)sptr;
      float smp, a0 = 1.0f - a1;
      size_t i = num;

      smp = *hist;
      do
      {
         smp = a0*smp + a1*(*s++);
         *d++ = smp;
      }
      while (--i);
      *hist = smp;
   }
}

/* 2nd order: 12dB/oct */
void
_batch_freqfilter_cpu(int32_ptr d, const_int32_ptr sptr, size_t num, float *hist, float k, const float *cptr)
{
   if (num)
   {
      int32_ptr s = (int32_ptr)sptr;
      float smp, nsmp, h0, h1;
      size_t i = num;

      h0 = hist[0];
      h1 = hist[1];
      do
      {
         smp = *s++ * k;
         smp = smp + h0 * cptr[0];
         nsmp = smp + h1 * cptr[1];
         smp = nsmp + h0 * cptr[2];
         smp = smp + h1 * cptr[3];

         h1 = h0;
         h0 = nsmp;
         *d++ = smp;
      }
      while (--i);

      hist[0] = h0;
      hist[1] = h1;
   }
}

void
_batch_movingavg_float_cpu(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float a1)
{
   if (num)
   {
      float32_ptr s = (float32_ptr)sptr;
      float smp, a0 = 1.0f - a1;
      size_t i = num;

      smp = *hist;
      do
      {
         smp = a0*smp + a1*(*s++);
         *d++ = smp;
      }
      while (--i);
      *hist = smp;
   }
}

void
_batch_freqfilter_float_cpu(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float k, const float *cptr)
{
   if (num)
   {
      float32_ptr s = (float32_ptr)sptr;
      float smp, h0, h1;
      size_t i = num;
      float c0, c1, c2, c3;

      // for original code see _batch_freqfilter_cpu
      c0 = cptr[0];
      c1 = cptr[1];
      c2 = cptr[2];
      c3 = cptr[3];

      h0 = hist[0];
      h1 = hist[1];

      do
      {
         smp = (*s++ * k) + ((h0 * c0) + (h1 * c1));
         *d++ = smp       + ((h0 * c2) + (h1 * c3));

         h1 = h0;
         h0 = smp;
      }
      while (--i);

      hist[0] = h0;
      hist[1] = h1;
   }
}

void
_aax_movingaverage_fir_compute(float fc, float fs, float *a)
{
   fc *= GMATH_2PI;
   *a = fc/(fc+fs);
}

/* Calculate a 2nd order (12 dB/oct) Butterworth IIR filter
 *
 * A common practice is to chain several 2nd order sections in order to achieve
 * a higher order filter. So, for a 4th order (24dB/oct) filter  we need 2 of
 * those sections in series.
 *
 * From:
 *   http://www.gamedev.net/reference/articles/article846.asp
 *   http://www.gamedev.net/reference/articles/article845.asp
 */
static void
_aax_butterworth_iir_bilinear(float a0, float a1, float a2, float b0, float b1, float b2,
             float *k, float fs, float *coef)
{
   float ad, bd;

   a2 *= (4.0f * fs*fs);
   b2 *= (4.0f * fs*fs);
   a1 *= (2.0f * fs);
   b1 *= (2.0f * fs);

   ad = a2 + a1 + a0;
   bd = b2 + b1 + b0;

   *k *= ad/bd;

   // modified: coef[0] and coef[1] are negated to prevent this should be
   //           done every time the filter is applied.
   *coef++ = -1.0f * (-2.0f*b2 + 2.0f*b0) / bd;
   *coef++ = -1.0f * (b2 - b1 + b0) / bd;
   *coef++ =         (-2.0f*a2 + 2.0f*a0) / ad;
   *coef   =         (a2 - a1 + a0) / ad;
}

static void // pre-warp
_aax_butterworth_iir_prewarp(float *a0, float *a1, float *a2, float *b0, float *b1, float *b2,
        float fc, float fs, float *k, float *coef)
{
   float wp;

   // http://unicorn.us.com/trading/allpolefilters.html
   /* To get a highpass filter, use ω0 = 1 ⁄ tan[pi*fc⁄(c*fs)] as the adjusted
    * digital cutoff frequency (that is, invert c before applying to fc and
    * invert the tangent to get ω0), and calculate the lowpass coefficients.
    * Then, negate the coefficients a1 and b1 — but be sure you calculate b2
    * before negating those coefficients! Applying these coefficients in the
    * final filter formula above, will result in a highpass filter with a 3 dB
    * cutoff at fc
    */

   // highass: wp = 2.0f*fs * 1.0f/tanf(GMATH_PI * -fc/fs);
   // lowpass: wp = 2.0f*fs * tanf(GMATH_PI * fc/fs);
   wp = 2.0f*fs * tanf(GMATH_PI * fc/fs);
   *a2 /= wp*wp;
   *b2 /= wp*wp;
   *a1 /= wp;
   *b1 /= wp;

   _aax_butterworth_iir_bilinear(*a0, *a1, *a2, *b0, *b1, *b2, k, fs, coef);
}

void
_aax_butterworth_iir_compute(float fc, float fs, float *coef, float *gain, float Q, int stages)
{
   // http://www.electronics-tutorials.ws/filter/filter_8.html
   static const float _b1[3][3] = {
      { 1.4142f,    0.0f,      0.0f      },     // 2nd order
      { 0.765367,   1.847759,  0.0f      },     // 4th order
      { 0.5176387f, 1.414214f, 1.931852f }      // 6th roder
   };
   int i, pos = stages-1;
   float k = 1.0f;

   assert(stages <= _AAX_FILTER_SECTIONS);
   assert(stages <= 3);

   for (i=0; i<stages; i++)
   {
      float a0 = 1.0f;
      float a1 = 0.0f;
      float a2 = 0.0f;
      float b0 = 1.0f;
      float b1 = _b1[pos][i] / Q;
      float b2 = 1.0f;

      _aax_butterworth_iir_prewarp(&a0, &a1, &a2, &b0, &b1, &b2, fc, fs, &k, coef);
      coef += 4;
   }
   *gain = k;
}
