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

#include <aax/aax.h>

#include <base/types.h>		/* for rintf */
#include <base/gmath.h>
#include <base/random.h>

#include <software/rbuf_int.h>

#include "filters.h"
#include "lfo.h"
#include "dsp.h"
#include "api.h"

#define VERSION	1.13
#define DSIZE	sizeof(_aaxRingBufferFreqFilterData)

void _freqfilter_swap(void*, void*);

static aaxFilter
_aaxFrequencyFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, 2, DSIZE);
   aaxFilter rv = NULL;

   if (flt)
   {
      _aaxSetDefaultFilter2d(flt->slot[0], flt->pos, 0);
      flt->slot[0]->destroy = _freqfilter_destroy;
      flt->slot[0]->swap = _freqfilter_swap;
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxFrequencyFilterDestroy(_filter_t* filter)
{
   if (filter->slot[0]->data)
   {
      filter->slot[0]->destroy(filter->slot[0]->data);
      filter->slot[0]->data = NULL;
      filter->slot[1]->data = NULL;
   }
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxFrequencyFilterSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = AAX_FALSE;
   int ostate, sstate;
   int resonance, stereo;

   assert(filter->info);

   stereo = (state & AAX_LFO_STEREO) ? AAX_TRUE : AAX_FALSE;
   state &= ~AAX_LFO_STEREO;

   sstate = state & AAX_SOURCE_MASK;
   ostate = state & AAX_ORDER_MASK;
   resonance = (ostate == AAX_RESONANCE_FACTOR) ? AAX_TRUE : AAX_FALSE;
   if (ostate == 0) ostate = AAX_12DB_OCT;

   if ((ostate >= AAX_1ST_ORDER && ostate <= AAX_LAST_ORDER) || resonance ||
       (sstate >= AAX_CONSTANT && sstate <= AAX_LAST_WAVE) ||
       (sstate >= AAX_1ST_SOURCE && sstate <= AAX_LAST_SOURCE))
   {
      _aaxRingBufferFreqFilterData *flt = filter->slot[0]->data;

      if (flt == NULL)
      {
         flt = _aax_aligned_alloc(DSIZE);
         filter->slot[0]->data = flt;
         if (flt) memset(flt, 0, DSIZE);
      }

      if (flt && !flt->freqfilter)
      {
         size_t dsize = sizeof(_aaxRingBufferFreqFilterHistoryData);

         flt->freqfilter = _aax_aligned_alloc(dsize);
         if (flt->freqfilter) {
            memset(flt->freqfilter, 0, dsize);
         }
         else
         {
            free(flt);
            flt = NULL;
         }
      }

      if (flt)
      {
         float fc, fmax;
         int stages;

         flt->fs = filter->info ? filter->info->frequency : 48000.0f;
         flt->run = _freqfilter_run;

         fc = filter->slot[0]->param[AAX_CUTOFF_FREQUENCY];
         fmax = filter->slot[1]->param[AAX_CUTOFF_FREQUENCY_HF & 0xF];
         fc = CLIP_FREQUENCY(fc, flt->fs);
         fmax = CLIP_FREQUENCY(fmax, flt->fs);
         flt->fc_low = fc;
         flt->fc_high = fmax;

         flt->high_gain = fabsf(filter->slot[0]->param[AAX_LF_GAIN]);
         if (flt->high_gain < LEVEL_128DB) flt->high_gain = 0.0f;

         flt->low_gain = fabsf(filter->slot[0]->param[AAX_HF_GAIN]);
         if (flt->low_gain < LEVEL_128DB) flt->low_gain = 0.0f;

         if (fabsf(flt->low_gain - flt->high_gain) < LEVEL_128DB) {
             flt->high_gain *= 0.9f;
         }

         if (ostate == AAX_48DB_OCT) stages = 4;
         else if (ostate == AAX_36DB_OCT) stages = 3;
         else if (ostate == AAX_24DB_OCT) stages = 2;
         else if (ostate == AAX_6DB_OCT) stages = 0;
         else stages = 1;

         flt->no_stages = stages;
         flt->state = (state & AAX_BESSEL) ? AAX_BESSEL : AAX_BUTTERWORTH;
         flt->Q = filter->slot[0]->param[AAX_RESONANCE];
         flt->type = (flt->high_gain >= flt->low_gain) ? LOWPASS : HIGHPASS;
         flt->resonance = resonance ? flt->Q/fmax : 0.0f;

         if ((state & AAX_SOURCE_MASK) == AAX_RANDOM_SELECT)
         {
            float lfc2 = _lin2log(fmax);
            float lfc1 = _lin2log(fc);

            flt->fc_low = fc;
            flt->fc_high = fmax;
            flt->random = 1;

            lfc1 += (lfc2 - lfc1)*_aax_random();
            fc = _log2lin(lfc1);
         }

         if (flt->state == AAX_BESSEL) {
             _aax_bessel_compute(fc, flt);
         }
         else
         {
            if (flt->type == HIGHPASS)
            {
               float g = flt->high_gain;
               flt->high_gain = flt->low_gain;
               flt->low_gain = g;
            }
            _aax_butterworth_compute(fc, flt);
         }

#if 0
  printf("Filter, type: %s\n", flt->type == LOWPASS ? "low-pass" : "high-pass");
  printf(" Fc: % 7.1f Hz, ", flt->fc);
  printf(" k: %5.4f, ", flt->k);
  printf(" Q: %3.1f\n", flt->Q);
  printf(" low gain:  %5.4f\n high gain: %5.4f\n", flt->low_gain, flt->high_gain);
#endif

         // Non-Manual only
         if (sstate && EBF_VALID(filter) && filter->slot[1])
         {
            _aaxLFOData* lfo = flt->lfo;

            if (lfo == NULL) {
               lfo = flt->lfo = _lfo_create();
            }

            if (lfo)
            {
               int constant;

               /* sweeprate */
               lfo->state = sstate;
               lfo->fs = filter->info->frequency;
               lfo->period_rate = filter->info->period_rate;

               lfo->min = fc;
               lfo->max = fmax;

               if (state & AAX_LFO_EXPONENTIAL)
               {
                  lfo->convert = _logarithmic;
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
                     state ^= AAX_INVERSE;
                  }
                  lfo->min = _lin2log(lfo->min);
                  lfo->max = _lin2log(lfo->max);
               }
               else
               {
                  lfo->convert = _linear;
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
                     state ^= AAX_INVERSE;
                  }
               }

               lfo->depth = 1.0f;
               lfo->offset = 0.0f;
               lfo->min_sec = lfo->min/lfo->fs;
               lfo->max_sec = lfo->max/lfo->fs;

               lfo->f = filter->slot[1]->param[AAX_SWEEP_RATE & 0xF];
               lfo->inverse = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;
               lfo->stereo_link = !stereo;

               constant = _lfo_set_timing(lfo);
               lfo->envelope = AAX_FALSE;

               if (!_lfo_set_function(lfo, constant)) {
                  _aaxErrorSet(AAX_INVALID_PARAMETER);
               }
            } /* flt->lfo */
         } /* flt */
         else if (sstate == AAX_FALSE)
         {
            _lfo_destroy(flt->lfo);
            flt->lfo = NULL;
         }
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
   }
   else if (sstate == AAX_FALSE)
   {
      if (filter->slot[0]->data)
      {
         filter->slot[0]->destroy(filter->slot[0]->data);
         filter->slot[0]->data = NULL;
         filter->slot[1]->data = NULL;
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewFrequencyFilterHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _filter_t* rv = _aaxFilterCreateHandle(info, type, 2, 0);

   if (rv)
   {
      _aaxRingBufferFreqFilterData *freq;

      _aax_dsp_copy(rv->slot[0], &p2d->filter[rv->pos]);
      rv->slot[0]->destroy = _freqfilter_destroy;
      rv->slot[0]->swap = _freqfilter_swap;

      /* reconstruct rv->slot[1] */
      freq = (_aaxRingBufferFreqFilterData*)p2d->filter[rv->pos].data;
      if (freq && freq->lfo)
      {
         rv->slot[1]->param[AAX_SWEEP_RATE & 0xF] = freq->lfo->f;
         if (freq->lfo->convert == _logarithmic) {
            rv->slot[1]->param[AAX_CUTOFF_FREQUENCY_HF & 0xF] = _log2lin(freq->lfo->max);
         } else {
            rv->slot[1]->param[AAX_CUTOFF_FREQUENCY_HF & 0xF] = freq->lfo->max;
         }
      }
      else
      {
         rv->slot[1]->param[AAX_SWEEP_RATE & 0xF] = 1.0f;
         rv->slot[1]->param[AAX_CUTOFF_FREQUENCY_HF & 0xF] = MAX_CUTOFF;
      }

      rv->state = p2d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxFrequencyFilterSet(float val, int ptype, unsigned char param)
{
   float rv = val;
   if (param > 0 && ptype == AAX_DECIBEL) {
      rv = _lin2db(val);
   }
   return rv;
}

static float
_aaxFrequencyFilterGet(float val, int ptype, unsigned char param)
{
   float rv = val;
   if ((param == AAX_LF_GAIN || param == AAX_HF_GAIN) && ptype == AAX_DECIBEL) {
      rv = _db2lin(val);
   }
   return rv;
}

static float
_aaxFrequencyFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxFrequencyRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { MIN_CUTOFF, 0.0f, 0.0f, 1.0f  }, { MAX_CUTOFF, 10.0f, 10.0f, 100.0f } },
    { { MIN_CUTOFF, 0.0f, 0.0f, 0.01f }, { MAX_CUTOFF,  1.0f,  1.0f,  50.0f } },
    { {       0.0f, 0.0f, 0.0f, 0.0f  }, {       0.0f,  0.0f,  0.0f,   0.0f } },
    { {       0.0f, 0.0f, 0.0f, 0.0f  }, {       0.0f,  0.0f,  0.0f,   0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxFrequencyRange[slot].min[param],
                       _aaxFrequencyRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxFrequencyFilter =
{
   AAX_TRUE,
   "AAX_frequency_filter_"AAX_MKSTR(VERSION), VERSION,
   (_aaxFilterCreate*)&_aaxFrequencyFilterCreate,
   (_aaxFilterDestroy*)&_aaxFrequencyFilterDestroy,
   (_aaxFilterReset*)&_freqfilter_reset,
   (_aaxFilterSetState*)&_aaxFrequencyFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewFrequencyFilterHandle,
   (_aaxFilterConvert*)&_aaxFrequencyFilterSet,
   (_aaxFilterConvert*)&_aaxFrequencyFilterGet,
   (_aaxFilterConvert*)&_aaxFrequencyFilterMinMax
};

void
_freqfilter_reset(void *data)
{
   _aaxRingBufferFreqFilterData *flt = data;

   if (flt->lfo) {
      _lfo_reset(flt->lfo);
   }
}

void
_freqfilter_data_swap( _aaxRingBufferFreqFilterData *dflt, _aaxRingBufferFreqFilterData *sflt)
{
   _lfo_swap(dflt->lfo, sflt->lfo);
   memcpy(dflt->coeff, sflt->coeff, sizeof(float[4*_AAX_MAX_STAGES]));
   dflt->Q = sflt->Q;
   dflt->k = sflt->k;
   dflt->fs = sflt->fs;
   dflt->high_gain = sflt->high_gain;
   dflt->low_gain = sflt->low_gain;
   dflt->state = sflt->state;
   dflt->no_stages = sflt->no_stages;
   dflt->type = sflt->type;
}

void
_freqfilter_swap(void *d, void *s)
{
   _aaxFilterInfo *dst = d, *src = s;

   if (src->data && src->data_size)
   {
      if (!dst->data) {
          dst->data = _aaxAtomicPointerSwap(&src->data, dst->data);
          dst->data_size = src->data_size;
      }
      else
      {
         _aaxRingBufferFreqFilterData *dflt = dst->data;
         _aaxRingBufferFreqFilterData *sflt = src->data;

         _freqfilter_data_swap(dflt, sflt);
      }
   }
   dst->destroy = src->destroy;
   dst->swap = src->swap;
}

void
_freqfilter_destroy(void *ptr)
{
   _aaxRingBufferFreqFilterData *data = ptr;
   if (data)
   {
      _lfo_destroy(data->lfo);
      _aax_aligned_free(data->freqfilter);
   }
   _aax_dsp_destroy(ptr);
}

/**
 * 1st order, 6dB/octave exponential moving average Butterwordth FIR filter
 *
 * X[k] = a*X[k-1] + (1-a)*Y[k]
 * http://lorien.ncl.ac.uk/ming/filter/fillpass.htm
 *
 * Used for:
 *  - frequency filtering (frames and emitters)
 *  - HRTF head shadow filtering
 */
void
_batch_ema_iir_cpu(int32_ptr d, const_int32_ptr sptr, size_t num, float *hist, float a1)
{
   if (num)
   {
      int32_ptr s = (int32_ptr)sptr;
      float smp, b1 = 1.0f - a1;
      size_t i = num;

      smp = *hist;
      do
      {
         smp = a1*(*s++) + b1*smp;
         *d++ = smp;
      }
      while (--i);
      *hist = smp;
   }
}

#if 0
void
_batch_ema_iir_float_cpu(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float a1)
{
   if (num)
   {
      float32_ptr s = (float32_ptr)sptr;
      size_t i = num;
      float smp;

      smp = *hist;
      do
      {
//       smp = a1*(*s++) + (1.0f - a1)*smp;
         smp += a1*(*s++ - smp);	// smp = a1*(*s++ - smp) + smp;
         *d++ = smp;
      }
      while (--i);
      *hist = smp;
   }
}
#endif

// exponential moving average filter:  y[n] = a*x[n] + (1-a)*y[n-1]
// http://www.dsprelated.com/showarticle/182.php
// used for per emitter HRTF calculation
// and for surround crossover
void
_aax_EMA_compute(float fc, float fs, float *a)
{
   float n = *a;

   // exact
   float c = cosf(GMATH_2PI*fc/fs);
// *a = c - 1.0f + sqrtf(c*c - 4.0f*c + 3.0f);
   *a = c - 1.0f + sqrtf((0.5f*c*c - 2.0f*c + 1.5f)*n);
}


/**
 * 2nd order, 12dB/octave biquad IIR Butterworth filter
 * http://www.earlevel.com/main/2003/02/28/biquads/
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
#if 0
void
_batch_freqfilter_iir_cpu(int32_ptr dptr, const_int32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   const_int32_ptr s = sptr;

   if (num)
   {
      float k, *cptr, *hist;
      float smp, nsmp, h0, h1;
      int stage;

      cptr = filter->coeff;
      hist = filter->freqfilter->history[t];
      stage = filter->no_stages;
      if (!stage) stage++;

      if (filter->state == AAX_BESSEL) {
         k = filter->k * (filter->high_gain - filter->low_gain);
      } else {
         k = filter->k * filter->high_gain;
      }

      do
      {
         int32_ptr d = dptr;
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

         *hist++ = h0;
         *hist++ = h1;
         cptr += 4;
         k = 1.0f;
         s = dptr;
      }
      while (--stage);
   }
}

void
_batch_freqfilter_iir_float_cpu(float32_ptr dptr, const_float32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   if (num)
   {
      const_float32_ptr s = sptr;
      float k, *cptr, *hist;
      float c0, c1, c2, c3;
      float smp, h0, h1;
      int stage;

      if (filter->state == AAX_BESSEL) {
         k = filter->k * (filter->high_gain - filter->low_gain);
      } else {
         k = filter->k * filter->high_gain;
      }

      if (fabsf(k-1.0f) < LEVEL_96DB)
      {
         if (dptr != sptr) memcpy(dptr, sptr, num*sizeof(float));
         return;
      }
      if (fabsf(k) < LEVEL_96DB && filter->no_stages < 2)
      {
         memset(dptr, 0, num*sizeof(float));
         return;
      }

      cptr = filter->coeff;
      hist = filter->freqfilter->history[t];
      stage = filter->no_stages;
      if (!stage) stage++;

      do
      {
         float32_ptr d = dptr;
         size_t i = num;

         // for original code see _batch_freqfilter_iir_cpu
         c0 = *cptr++;
         c1 = *cptr++;
         c2 = *cptr++;
         c3 = *cptr++;

         h0 = hist[0];
         h1 = hist[1];

         // z[n] = k*x[n] + c0*x[n-1]  + c1*x[n-2] + c2*z[n-1] + c3*z[n-2];
         if (filter->state == AAX_BUTTERWORTH)
         {
            do
            {
               smp = (*s++ * k) + ((h0 * c0) + (h1 * c1));
               *d++ = smp       + ((h0 * c2) + (h1 * c3));

               h1 = h0;
               h0 = smp;
            }
            while (--i);
         }
         else
         {
            do
            {
               smp = (*s++ * k) + ((h0 * c0) + (h1 * c1));
               *d++ = smp;

               h1 = h0;
               h0 = smp;
            }
            while (--i);
         }

         *hist++ = h0;
         *hist++ = h1;
         k = 1.0f;
         s = dptr;
      }
      while (--stage);
   }
}
#endif

static inline void
_aax_bilinear(float a0, float a1, float a2, float b0, float b1, float b2,
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

static inline void // pre-warp
_aax_bilinear_s2z(float *a0, float *a1, float *a2,
                float *b0, float *b1, float *b2,
                float fc, float fs, float *k, float *coef)
{
   float wp;

   // prewarp
   wp = 2.0f*tanf(GMATH_PI * fc/fs);

   *a2 /= wp*wp;
   *b2 /= wp*wp;
   *a1 /= wp;
   *b1 /= wp;

   _aax_bilinear(*a0, *a1, *a2, *b0, *b1, *b2, k, coef);
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
_aax_butterworth_compute(float fc, void *flt)
{
   // http://www.ti.com/lit/an/sloa049b/sloa049b.pdf
   // double Q = -0.5/cos(pi/2.0*(1.0+(1.0+(2.0*phase+1.0)/order)));
   static const float _Q[_AAX_MAX_STAGES][_AAX_MAX_STAGES] = {
      { 0.7071f, 1.0f,    1.0f,    1.0f    },	// 2nd order
      { 0.5412f, 1.3605f, 1.0f,    1.0f    },	// 4th order
      { 0.5177f, 0.7071f, 1.9320f, 1.0f    },	// 6th roder
      { 0.5098f, 0.6013f, 0.8999f, 2.5628f }	// 8th order
   };
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   int i, pos, first_order, stages;
   float k = 1.0f, A = 1.0f;
   float fs, Q, *coef;
   float a2, a1, a0;

   filter->fc = fc;
   filter->state = AAX_BUTTERWORTH;
   stages = filter->no_stages;
   if (!stages) stages++;

   assert(stages <= _AAX_MAX_STAGES);

   Q = filter->Q;
   fs = filter->fs;
   coef = filter->coeff;
   first_order = stages ? AAX_FALSE : AAX_TRUE;

   A = 0.0f;
   if (filter->high_gain > LEVEL_128DB)
   {
      // it's a shelf filter, adjust for correct dB
      // derived from the audio EQ Cookbook
      A = powf(filter->low_gain/filter->high_gain, 0.5f);
   }

   pos = stages-1;
   for (i=0; i<stages; i++)
   {
      float b2, b1, b0;

      if (A > LEVEL_128DB)
      {
         switch (filter->type)
         {
         case BANDPASS:
            a2 = 1.0f;
            a1 = A/(_Q[pos][i] * Q);
            a0 = 1.0f;

            b2 = 1.0f;
            b1 = A/(_Q[pos][i] * Q);
            b0 = 1.0f;
            break;
         case HIGHPASS:
            a2 = 1.0f;
            a1 = sqrtf(A)/(_Q[pos][i] * Q);
            a0 = A;
            k *= A;

            b2 = A;
            b1 = sqrtf(A)/(_Q[pos][i] * Q);
            b0 = 1.0f;
            break;
         case LOWPASS:
         default:
            a2 = A;
            a1 = sqrtf(A)/(_Q[pos][i] * Q);
            a0 = 1.0f;
            k *= A;

            b2 = 1.0f;
            b1 = sqrtf(A)/(_Q[pos][i] * Q);
            b0 = A;
            break;
         }
      }
      else
      {
         switch (filter->type)
         {
         case BANDPASS:
            a2 = 0.0f;
            a1 = 1.0f;
            a0 = 0.0f;
            break;
         case HIGHPASS:
            a2 = 1.0f;
            a1 = first_order ? 1.0f : 0.0f;
            a0 = 0.0f;
            break;
         case LOWPASS:
         default:
            a2 = 0.0f;
            a1 = first_order ? 1.0f : 0.0f;
            a0 = 1.0f;
            break;
         }
         b2 = 1.0f;
         b1 = 1.0f/(_Q[pos][i] * Q);
         b0 = 1.0f;
      }

      _aax_bilinear_s2z(&a0, &a1, &a2, &b0, &b1, &b2, fc, fs, &k, coef);
      coef += 4;
   }

   filter->k = k;
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
 *
 * Note: The benefit of the Bessel filter is linear phase shifting in the
 *       cut-band. Unfortunately this is only true for analogue Bessel
 *       filters but digital bilinear filters use prewrapping which spoils
 *       the advantage.
 *
 * Note: Because of this Bessel is now implemented using cascading
 *       exponential moving average filters.
 *
 * Note: coef[0] and coef[1] are negate to prevent this is required every
 *       time the filter is applied.
 */
#if 1
void
_aax_bessel_compute(float fc, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   float k = 1.0f, alpha = 1.0f;
   float beta, fs, *coef;
   int stages, type;

   filter->fc = fc;
   filter->state = AAX_BESSEL;
   fs = filter->fs;
   coef = filter->coeff;
   stages = filter->no_stages;
   type = filter->type;

   // highpass filtering is more a high-shelve type.
   // fix this by using a lowpass filter and subtracting it from the source
   if (type == HIGHPASS) {
      type = LOWPASS;
   }

   if (stages > 0) alpha = 2.0f*stages;
   if (type == HIGHPASS) alpha = 1.0f/alpha;
   _aax_EMA_compute(_MIN(fc, 4800.0f), fs, &alpha);
   beta = 1.0f - alpha;

   if (stages == 0)	// 1st order exponential moving average filter
   {
      k = (type == HIGHPASS) ? beta : alpha;

      coef[0] = beta;
      coef[1] = 0.0f;
      coef[2] = (type == HIGHPASS) ? -1.0f : 0.0f;
      coef[3] = 0.0f;
   }
   else			// 2nd, 4th, 6th or 8th order exp. mov. avg. filter
   {
      float g = (type == HIGHPASS) ? beta : alpha;
      int i;

      k = powf(g, 2.0f*stages); // alpha*alpha for 2nd order
      if (type == BANDPASS) k *= powf(10000.0f/fc, stages);

      for (i=0; i<stages; i++)
      {
         coef[0] = 2.0f*beta;
         coef[1] = -beta*beta;

         if      (type == HIGHPASS) coef[2] = -2.0f;
         else if (type == BANDPASS) coef[2] = -1.0f;
         else                       coef[2] =  0.0f;

         if (type == HIGHPASS)      coef[3] =  1.0f;
         else                       coef[3] =  0.0f;

         coef += 4;
      }
   }

   filter->Q = 1.0f;
   filter->k = k;
}
#else
void
_aax_bessel_compute(float fc, float fs, float *coef, float *gain, float Q, int stages, char type)
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
   int i, pos, first_order;
   float a2, a1, a0;
   float A = *gain;
   float k = 1.0f;

   assert(stages <= _AAX_MAX_STAGES);

   first_order = stages ? AAX_FALSE : AAX_TRUE;
   if (!stages) stages++;

   pos = stages-1;
   for (i=0; i<stages; i++)
   {
      float b2, b1, b0;
      float nfc;

      if (A >  LEVEL_128DB)	// shelf or allpass filter
      {
         switch (type)
         {
         case BANDPASS:
            a2 = 1.0f;
            a1 = A/(_Q[pos][i] * Q);
            a0 = 1.0f;

            b2 = 1.0f;
            b1 = A/(_Q[pos][i] * Q);
            b0 = 1.0f;
            break;
         case HIGHPASS:
            a2 = 1.0f;
            a1 = sqrtf(A)/(_Q[pos][i] * Q);
            a0 = A;
            k *= A;

            b2 = A;
            b1 = sqrtf(A)/(_Q[pos][i] * Q);
            b0 = 1.0f;
            break;
         case LOWPASS:
         default:
            a2 = A;
            a1 = sqrtf(A)/(_Q[pos][i] * Q);
            a0 = 1.0f;
            k *= A;

            b2 = 1.0f;
            b1 = sqrtf(A)/(_Q[pos][i] * Q);
            b0 = A;
            break;
         }
      }
      else
      {
         switch (type)
         {
         case BANDPASS:
            a2 = 0.0f;
            a1 = 1.0f;
            a0 = 0.0f;
            break;
         case HIGHPASS:
            a2 = 1.0f;
            a1 = first_order ? 1.0f : 0.0f;
            a0 = 0.0f;
            break;
         case LOWPASS:
         default:
            a2 = 0.0f;
            a1 = first_order ? 1.0f : 0.0f;
            a0 = 1.0f;
            break;
         }
         b2 = 1.0f;
         b1 = 1.0f/(_Q[pos][i] * Q);
         b0 = 1.0f;
      }

      if (type == HIGHPASS) {
         nfc = fc / _FSF[pos][i];
      } else {
         nfc = fc * _FSF[pos][i];
      }
      _aax_bilinear_s2z(&a0, &a1, &a2, &b0, &b1, &b2, fc, fs, &k, coef);
      coef += 4;
   }
   *gain = k;
}
#endif

int
_freqfilter_run(void *rb, MIX_PTR_T d, CONST_MIX_PTR_T s,
                size_t dmin, size_t dmax, size_t ds,
                unsigned int track, void *data, void *env,
                float velocity, unsigned char ctr)
{
   _aaxRingBufferSample *rbd = (_aaxRingBufferSample*)rb;
   _aaxRingBufferFreqFilterData *filter = data;
   CONST_MIX_PTR_T sptr;
   MIX_T *dptr;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(s != 0);
   assert(d != 0);
   assert(data != NULL);
   assert(dmin < dmax);
   assert(track < _AAX_MAX_SPEAKERS);

   s += dmin;
   d += dmin;
   dmax -= dmin;

   if (filter->lfo && !ctr)
   {
      float fc = filter->fc;

      fc = _MINMAX(filter->lfo->get(filter->lfo, env, s, track, dmax),
                   20.0f, 0.9f*0.5f*filter->fs);

      if (filter->resonance > 0.0f) {
         if (filter->type > BANDPASS) { // HIGHPASS
             filter->Q = _MAX(filter->resonance*(filter->fc_high - fc), 1.0f);
         } else {
            filter->Q = filter->resonance*fc;
         }
      }

      if (filter->state == AAX_BESSEL) {
         _aax_bessel_compute(fc, filter);
      } else {
         _aax_butterworth_compute(fc, filter);
      }
   }

   dmax += ds;
   sptr = s - ds;
   dptr = d - ds;
   rbd->freqfilter(dptr, sptr, track, dmax, filter);
   if (filter->state == AAX_BESSEL && filter->low_gain > LEVEL_128DB) {
      rbd->add(dptr, sptr, dmax, filter->low_gain, 0.0f);
   }

   return AAX_TRUE;
}
