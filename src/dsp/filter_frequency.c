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

   istate = state & ~(AAX_INVERSE|AAX_BUTTERWORTH|AAX_BESSEL);
   wstate = istate & mask;

   switch (wstate || istate == AAX_6DB_OCT || istate == AAX_12DB_OCT
                  || istate == AAX_24DB_OCT || istate == AAX_36DB_OCT
                  || istate == AAX_48DB_OCT)
   {
   case AAX_6DB_OCT:
   case AAX_12DB_OCT:
   case AAX_24DB_OCT:
   case AAX_36DB_OCT:
   case AAX_48DB_OCT:
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

      if (state & AAX_48DB_OCT) stages = 4;
      else if (state & AAX_36DB_OCT) stages = 3;
      else if (state & AAX_24DB_OCT) stages = 2;
      else if (state & AAX_6DB_OCT) stages = 0;
      else stages = 1;

      if (flt)
      {
         float fc = filter->slot[0]->param[AAX_CUTOFF_FREQUENCY];
         float Q = filter->slot[0]->param[AAX_RESONANCE];
         float *cptr = flt->coeff;
         float fs = flt->fs; 
         float k = 1.0f;

         flt->lf_gain = fabs(filter->slot[0]->param[AAX_LF_GAIN]);
         if (flt->lf_gain < GMATH_128DB) flt->lf_gain = 0.0f;
         else if (fabs(flt->lf_gain - 1.0f) < GMATH_128DB) flt->lf_gain = 1.0f;

         flt->hf_gain = fabs(filter->slot[0]->param[AAX_HF_GAIN]);
         if (flt->hf_gain < GMATH_128DB) flt->hf_gain = 0.0f;
         else if (fabs(flt->hf_gain - 1.0f) < GMATH_128DB) flt->hf_gain = 1.0f;

         flt->hf_gain_prev = 1.0f;
         flt->Q = Q;

         flt->lp = (flt->lf_gain >= flt->hf_gain) ? AAX_TRUE : AAX_FALSE;
         if (flt->lp == AAX_FALSE)
         {
            float f = flt->lf_gain;
            flt->lf_gain = flt->hf_gain;
            flt->hf_gain = f;
         }

         if (state & AAX_BESSEL) {
             _aax_bessel_iir_compute(fc, fs, cptr, &k, Q, stages, flt->lp);
         } else {
            _aax_butterworth_iir_compute(fc, fs, cptr, &k, Q, stages, flt->lp);
         }
         flt->no_stages = stages;
         filter->state = state >> 24;
         flt->k = k;

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

/**
 * 1st order, 6dB/octave moving average Butterwordth FIR filter
 *
 * X[k] = a*X[k-1] + (1-a)*Y[k]
 * http://lorien.ncl.ac.uk/ming/filter/fillpass.htm
 *
 * Used for:
 *  - frequency filtering (frames and emitters)
 *  - HRTF head shadow filtering
 */
void
_batch_freqfilter_fir_cpu(int32_ptr d, const_int32_ptr sptr, size_t num, float *hist, float a1)
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

void
_batch_freqfilter_fir_float_cpu(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float a1)
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

// http://www.dsprelated.com/showarticle/182.php
void
_aax_movingaverage_fir_compute(float fc, float fs, float *a, char lowpass)
{
   // exact
   float c = cosf(GMATH_2PI*fc/fs);
   *a = c - 1.0f + sqrtf(c*c - 4.0f*c + 3.0f);

   // approx
// *a = 1.0f - expf(-GMATH_2PI*fc/fs);
}


/**
 * 2nd order, 12dB/octave biquad IIR Butterworth filter
 *
 * A common practice is to chain several 2nd order sections in order to achieve
 * a higher order filter. So, for a 4th order (24dB/oct) filter  we need 2 of
 * those sections in series.
 *
 * From:
 *   http://www.gamedev.net/reference/articles/article846.asp
 *   http://www.gamedev.net/reference/articles/article845.asp
 *
 * When we construct a 24 db/oct filter, we take to 2nd order
 * sections and compute the coefficients separately for each section.
 *
 *  n       Polinomials
 * --------------------------------------------------------------------
 *  2       s^2 + 1.4142s +1
 *  4       (s^2 + 0.765367s + 1) (s^2 + 1.847759s + 1)
 *  6       (s^2 + 0.5176387s + 1) (s^2 + 1.414214 + 1) (s^2 + 1.931852s + 1)
 *
 * Where n is a filter order.
 * For n=4, or two second order sections, we have following equasions for each
 * 2nd order stage:
 *
 * (1 / (s^2 + (1/Q) * 0.765367s + 1)) * (1 / (s^2 + (1/Q) * 1.847759s + 1))
 *
 * Where Q is filter quality factor in the range of 1 to 1000.
 * The overall filter Q is a product of all 2nd order stages.
 * For example, the 6th order filter (3 stages, or biquads) with individual
 * Q of 2 will have filter Q = 2 * 2 * 2 = 8.
 *
 * The nominator part is just 1.
 * The denominator coefficients for stage 1 of filter are:
 *  b2 = 1; b1 = 0.765367; b0 = 1;
 * numerator is
 *  a2 = 0; a1 = 0; a0 = 1;
 *
 * The denominator coefficients for stage 1 of filter are:
 *  b2 = 1; b1 = 1.847759; b0 = 1;
 * numerator is
 *  a2 = 0; a1 = 0; a0 = 1;
 *
 * Used for:
 * - frequency filtering (frames and emitters)
 * - equalizer (graphic and parametric)
 */
void
_batch_freqfilter_iir_cpu(int32_ptr d, const_int32_ptr sptr, size_t num, float *hist, float k, const float *cptr)
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
_batch_freqfilter_iir_float_cpu(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float k, const float *cptr)
{
   if (num)
   {
      float32_ptr s = (float32_ptr)sptr;
      float smp, h0, h1;
      size_t i = num;
      float c0, c1, c2, c3;

      // for original code see _batch_freqfilter_iir_cpu
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

# if 1
static void
_aax_iir_bilinear(float a0, float a1, float a2, float b0, float b1, float b2,
                  float *k, float *coef)
{
   float ad, bd;

   a2 *= 4.0f;
   b2 *= 4.0f;
   a1 *= 2.0f;
   b1 *= 2.0f;

   ad = a2 + a1 + a0;
   bd = b2 + b1 + b0;

   *k *= ad/bd;

   coef[0] = 2.0f*(-b2 + b0) / bd;
   coef[1] = (b2 - b1 + b0) / bd;
   coef[2] = 2.0f*(-a2 + a0) / ad;
   coef[3] = (a2 - a1 + a0) / ad;

   // negate to prevent this is required every time the filter is applied.
   coef[0] = -coef[0];
   coef[1] = -coef[1];
}

#else
static void	// original code
_aax_iir_bilinear(float a0, float a1, float a2, float b0, float b1, float b2, float *k, float fs, float *coef)
{
   float ad, bd;

                /* alpha (Numerator in s-domain) */
   ad = 4.f * a2 + 2.f * a1 + a0;
                /* beta (Denominator in s-domain) */
   bd = 4.f * b2 + 2.f * b1 + b0;

                /* update gain constant for this section */
   *k *= ad/bd;

                /* Denominator */
   coef[0] = (2.f * b0 - 8.f * b2) / bd;	/* beta1 */
   coef[1] = (4.f * b2 - 2.f * b1 + b0) / bd;	/* beta2 */

                /* Nominator */
   coef[2] = (2.f * a0 - 8.f * a2) / ad;	/* alpha1 */
   coef[3] = (4.f * a2 - 2.f * a1 + a0) / ad;	/* alpha2 */

   // negate to prevent this is required every time the filter is applied.
   coef[0] = -coef[0];
   coef[1] = -coef[1];
}
#endif

static void // pre-warp
_aax_iir_s_to_z(float *a0, float *a1, float *a2,
                float *b0, float *b1, float *b2,
                float fc, float fs, float *k, float *coef, char lowpass)
{
   float wp;

   // prewarp
   wp = 2.0f*tanf(GMATH_PI * fc/fs);

   *a2 /= wp*wp;
   *b2 /= wp*wp;
   *a1 /= wp;
   *b1 /= wp;

   _aax_iir_bilinear(*a0, *a1, *a2, *b0, *b1, *b2, k, coef);
}

/*
 * The Butterworth polynomial requires the least amount of work because the
 * frequency-scaling factor is always equal to one.
 * From a filter-table listing for Butterworth, we can find the zeroes of the
 * second-order Butterworth polynomial:
 *  z1 = -0.707 + j0.707, z1* = -0.707 -j0.707,
 *
 * which are used with the factored form of the polynomial. Alternately, we
 * find the coefficients of the polynomial:
 *  a0 = 1, a1 = 1.414
 *
 * It can be easily confirmed that
 *  (s+0.707 + j0.707) (s+0.707 -j0.707) = s2 + 1.414s + 1.
 */
void
_aax_butterworth_iir_compute(float fc, float fs, float *coef, float *gain, float Q, int stages, char lowpass)
{
   // http://www.ti.com/lit/an/sloa049b/sloa049b.pdf
   static const float _Q[_AAX_MAX_STAGES][_AAX_MAX_STAGES] = {
      { 0.7071f, 1.0f,    1.0f,    1.0f    },	// 2nd order
      { 0.5412f, 1.3605f, 1.0f,    1.0f    },	// 4th order
      { 0.5177f, 0.7071f, 1.9320f, 1.0f    },	// 6th roder
      { 0.5098f, 0.6013f, 0.8999f, 2.5628f }	// 8th order
   };
   float a2, a1, a0;
   float k = 1.0f;
   int i, pos;

   assert(stages <= _AAX_MAX_STAGES);

   a0 = lowpass ? 1.0f : 0.0f;
   a1 = stages ? 0.0f : 1.0f;
   a2 = lowpass ? 0.0f : 1.0f;
   if (!stages) stages++;

   pos = stages-1;
   for (i=0; i<stages; i++)
   {
      float b0 = 1.0f;
      float b1 = 1.0f/(_Q[pos][i] * Q);
      float b2 = 1.0f;

      _aax_iir_s_to_z(&a0, &a1, &a2, &b0, &b1, &b2, fc, fs, &k, coef, lowpass);
      coef += 4;
   }
   *gain = k;
}

/**
 * nth order, (n*6)dB/octave linear phase Bessel IIR filter
 *
 * 1 pass: y[k] = a0*x[k] + a1*x[k−1] + a2*x[k−2] + b1*y[k−1] + b2*y[k−2]
 * http://unicorn.us.com/trading/allpolefilters.html
 *
 * http://www.ti.com/lit/an/sloa049b/sloa049b.pdf
 * Referring to a table listing the zeros of the second-order Bessel polynomial,
 * we find:
 * z1 = -1.103 + j0.6368, z1* = -1.103 - j0.6368;
 *
 * a table of coefficients provides:
 *  a0 = 1.622 and a1 = 2.206.
 *
 * Again, using the coefficient form lends itself to our standard form, so that
 * the realization of a second-order low-pass Bessel filter is made by a
 * circuit with the transfer function:
 *
 * Butterworth: FSF = 1.0, Q = 1/0.7071
 * Bessel: FSF = 1/1.274, Q / 1/0.577
 */
void
_aax_bessel_iir_compute(float fc, float fs, float *coef, float *gain, float Q, int stages, char lowpass)
{
// http://www.eevblog.com/forum/projects/sallen-key-lpf-frequency-scaling-factor
   // http://www.ti.com/lit/an/sloa049b/sloa049b.pdf
   static const float _FSF[_AAX_MAX_STAGES][_AAX_MAX_STAGES] = {
      { 1.2736f, 1.0f,    1.0f,    1.0f    },	// 2nd order
      { 1.4192f, 1.5912f, 1.0f,    1.0f    },	// 4th order
      { 1.6060f, 1.6913f, 1.9071f, 1.0f    },	// 6th roder
      { 1.7837f, 2.1953f, 1.9591f, 1.8376f }	// 8th order
   };
   static const float _Q[_AAX_MAX_STAGES][_AAX_MAX_STAGES] = {
      { 0.5773f, 1.0f,    1.0f,    1.0f    },	// 2nd order
      { 0.5219f, 0.8055f, 1.0f,    1.0f    },	// 4th order
      { 0.5103f, 0.6112f, 1.0234f, 1.0f    }, 	// 6th roder
      { 0.5060f, 1.2258f, 0.7109f, 0.5596f }	// 8th order
   };
   float a2, a1, a0;
   float k = 1.0f;
   int i, pos;

   assert(stages <= _AAX_MAX_STAGES);

   a0 = lowpass ? 1.0f : 0.0f;
   a1 = stages ? 0.0f : 1.0f;
   a2 = lowpass ? 0.0f : 1.0f;
   if (!stages) stages++;

   pos = stages-1;
   for (i=0; i<stages; i++)
   {
      float b0 = 1.0f;
      float b1 = 1.0f/(_Q[pos][i] * Q);
      float b2 = 1.0f;
      float nfc;

      if (lowpass) {
         nfc = fc * _FSF[pos][i];
      } else {
         nfc = fc / _FSF[pos][i];
      }
      _aax_iir_s_to_z(&a0, &a1, &a2, &b0, &b1, &b2, nfc, fs, &k, coef, lowpass);
      coef += 4;
   }
   *gain = k;
}
