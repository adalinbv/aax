/*
 * SPDX-FileCopyrightText: Copyright © 2007-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

#define VERSION	1.14
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

static aaxFilter
_aaxFrequencyFilterSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = false;
   int ostate, sstate;
   int resonance, stereo;

   assert(filter->info);

   stereo = (state & AAX_LFO_STEREO) ? true : false;
   state &= ~AAX_LFO_STEREO;

   sstate = state & (AAX_SOURCE_MASK & ~AAX_PURE_WAVEFORM);
   ostate = state & AAX_ORDER_MASK;
   resonance = (ostate == AAX_RESONANCE_FACTOR) ? true : false;
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
         if ((state & AAX_SOURCE_MASK) == AAX_RANDOM_SELECT)
         {
            float lfc2 = _lin2log(fmax);
            float lfc1 = _lin2log(fc);

            lfc2 -= 0.1f*(lfc2 - lfc1)*_aax_random();
            fmax = _log2lin(lfc2);

            flt->fc_high = fmax;
            flt->random = true;
         }
         flt->fc_low = fc;
         flt->fc_high = fmax;

         flt->high_gain = filter->slot[0]->param[AAX_LF_GAIN];
         flt->low_gain = filter->slot[0]->param[AAX_HF_GAIN];
         _freqfilter_normalize_gains(flt);

         if (ostate == AAX_48DB_OCT) stages = 4;
         else if (ostate == AAX_36DB_OCT) stages = 3;
         else if (ostate == AAX_24DB_OCT) stages = 2;
         else if (ostate == AAX_6DB_OCT) stages = 0;
         else stages = 1;

         flt->no_stages = stages;
         flt->state = (state & AAX_BESSEL) ? AAX_BESSEL : AAX_BUTTERWORTH;
         flt->Q = filter->slot[0]->param[AAX_RESONANCE];
         flt->resonance = resonance ? flt->Q/fmax : 0.0f;

         if (fabsf(flt->high_gain - flt->low_gain) <= LEVEL_60DB) {
            flt->type = ALLPASS;
         } else if (flt->high_gain > flt->low_gain) {
            flt->type = LOWPASS;
         } else {
            flt->type = HIGHPASS;
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
               lfo->inverse = (state & AAX_INVERSE) ? true : false;
               lfo->stereo_link = !stereo;

               constant = _lfo_set_timing(lfo);
               lfo->envelope = false;

               if (!_lfo_set_function(lfo, constant)) {
                  _aaxErrorSet(AAX_INVALID_PARAMETER);
               }
            } /* flt->lfo */
         } /* flt */
         else if (sstate == false)
         {
            _lfo_destroy(flt->lfo);
            flt->lfo = NULL;
         }
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
   }
   else if (sstate == false)
   {
      if (filter->slot[0]->data)
      {
         filter->slot[0]->destroy(filter->slot[0]->data);
         filter->slot[0]->data_size = 0;
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
         rv->slot[1]->param[AAX_CUTOFF_FREQUENCY_HF & 0xF] = MAXIMUM_CUTOFF;
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

#define MINF	MINIMUM_CUTOFF
#define MAXF	MAXIMUM_CUTOFF
static float
_aaxFrequencyFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxFrequencyRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { MINF, 0.0f, 0.0f, 1.0f  }, { MAXF, 10.0f, 10.0f, 100.0f } },
    { { MINF, 0.0f, 0.0f, 0.01f }, { MAXF,  1.0f,  1.0f,  50.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f  }, { 0.0f,  0.0f,  0.0f,   0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f  }, { 0.0f,  0.0f,  0.0f,   0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxFrequencyRange[slot].min[param],
                       _aaxFrequencyRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxFrequencyFilter =
{
   "AAX_frequency_filter_"AAX_MKSTR(VERSION), VERSION,
   (_aaxFilterCreateFn*)&_aaxFrequencyFilterCreate,
   (_aaxFilterDestroyFn*)&_aaxFilterDestroy,
   (_aaxFilterResetFn*)&_freqfilter_reset,
   (_aaxFilterSetStateFn*)&_aaxFrequencyFilterSetState,
   (_aaxNewFilterHandleFn*)&_aaxNewFrequencyFilterHandle,
   (_aaxFilterConvertFn*)&_aaxFrequencyFilterSet,
   (_aaxFilterConvertFn*)&_aaxFrequencyFilterGet,
   (_aaxFilterConvertFn*)&_aaxFrequencyFilterMinMax
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
   if (dflt && sflt)
   {
      if (dflt->lfo && sflt->lfo) {
         _lfo_swap(dflt->lfo, sflt->lfo);
      }
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
}

void
_freqfilter_swap(void *d, void *s)
{
   _aaxFilterInfo *dst = d, *src = s;

   if (src->data && src->data_size)
   {
      if (!dst->data) {
          _aaxAtomicPointerSwap(&src->data, &dst->data);
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
      _aax_aligned_free(data);
   }
}

void
_freqfilter_normalize_gains(_aaxRingBufferFreqFilterData *flt)
{
   float high_gain = flt->high_gain;
   float low_gain = flt->low_gain;
   float gain = flt->gain;

   if (high_gain < LEVEL_128DB) high_gain = 0.0f;
   if (low_gain < LEVEL_128DB) low_gain = 0.0f;

   gain = 1.0f;
   if (high_gain > 1.0f)
   {
      gain = high_gain;
      low_gain /= high_gain;
      high_gain = 1.0f;
   }
   if (low_gain > 1.0f)
   {
      gain *= low_gain;
      high_gain /= low_gain;
      low_gain = 1.0f;
   }
   if (high_gain < 1.0f && low_gain < 1.0f && (low_gain > 0.0f || high_gain > 0.0f))
   {
       float fact = 1.0f/_MAX(high_gain, low_gain);
       high_gain *= fact;
       low_gain *= fact;
       gain /= fact;
   }

   flt->high_gain = high_gain;
   flt->low_gain = low_gain;
   flt->gain = gain;
}

// first order allpass:
// https://thewolfsound.com/allpass-filter/
void
_aax_allpass_compute(float fc, float fs, float *a)
{
   float c = tanf(GMATH_2PI*fc/fs);
   *a = (c - 1.0f)/(c + 1.0f);
}

// first order exponential moving average:
// http://www.dsprelated.com/showarticle/182.php
void
_aax_ema_compute(float fc, float fs, float *a)
{
   float n = *a;

   float c = cosf(GMATH_2PI*fc/fs);
   *a = c - 1.0f + sqrtf((0.5f*c*c - 2.0f*c + 1.5f)*n);
}

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
 *
 * To obtain an allpass biquad section, the numerator polynomial is simply the
 * "flip" of the denominator polynomial. To obtain unity gain, we set
 * k = a2, b1 = a1/a2, b2 = 1/a2
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
   first_order = stages ? false : true;

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

      if (A > LEVEL_128DB) // shelf filter
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
         case ALLPASS:
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
      else // not a shelf filter
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
         case ALLPASS:
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

      if (filter->type == ALLPASS)
      {
         k *= a2;
         b1 = a1/a2;
         b2 = 1.0f/a2;
      }

      _aax_bilinear_s2z(&a0, &a1, &a2, &b0, &b1, &b2, fc, fs, &k, coef);
      coef += 4;
   }

   filter->k = k * filter->high_gain;
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
   _aax_ema_compute(_MIN(fc, 4800.0f), fs, &alpha);
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
   filter->k = k * (filter->high_gain - filter->low_gain);
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

   first_order = stages ? false : true;
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
         case ALLPASS:
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
         case ALLPASS:
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
                size_t dmin, size_t dmax, size_t ds, unsigned int track,
                void *data, void *env, float velocity)
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

   if (filter->lfo)
   {
      float fc = filter->lfo->get(filter->lfo, env, s, track, dmax);
      fc = CLIP_FREQUENCY(fc, filter->fs);

      // if filter->resonance != 0.0f then the filter Q factor responds to
      // the LFO and the cutoff frequency remains the same
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

   return true;
}
