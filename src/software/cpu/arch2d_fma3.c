/*
 * SPDX-FileCopyrightText: Copyright © 2005-2024 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2024 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "base/random.h"
#include "software/rbuf_int.h"
#include "software/cpu/arch2d_simd.h"

#ifdef __FMA__

//
static inline float
hsum_ps_sse3(__m128 v)
{
   __m128 shuf = _mm_movehdup_ps(v);
   __m128 sums = _mm_add_ps(v, shuf);
   shuf        = _mm_movehl_ps(shuf, sums);
   sums        = _mm_add_ss(sums, shuf);
   return        _mm_cvtss_f32(sums);
}

static inline float
hsum256_ps_fma3(__m256 v)
{
   __m128 vlow  = _mm256_castps256_ps128(v);
   __m128 vhigh = _mm256_extractf128_ps(v, 1);
   vlow  = _mm_add_ps(vlow, vhigh);
   return hsum_ps_sse3(vlow);
}

static inline __m256
_mm256_abs_ps(__m256 x)
{
   const __m256 sign_mask = _mm256_set1_ps(-0.0f);
   return _mm256_andnot_ps(sign_mask, x);
}

static inline int
_mm256_testz_ps_fma3(__m256 x)
{
   const __m256i zero = _mm256_setzero_si256();
   return _mm256_testz_si256(_mm256_castps_si256(x), zero);
}

static inline __m256		// range -1.0f .. 1.0f
fast_sin8_fma3(__m256 x)	// -4.0f*(-x*fabsf(x) + x)
{
   const __m256 four = _mm256_set1_ps(-4.0f);
   return _mm256_mul_ps(four, _mm256_fmadd_ps(-x, _mm256_abs_ps(x), x));
}

// Use the slower, more accurate algorithm:
//    M_PI_4*x - x*(fabs(x) - 1)*(0.2447 + 0.0663*fabs(x)); // -1 < x < 1
//    which equals to: x*(1.03 - 0.1784*abs(x) - 0.0663*x*x)
static inline __m256
fast_atan8_fma3(__m256 x)
{
   const __m256 pi_4_mul = _mm256_set1_ps(1.03f);
   const __m256 add = _mm256_set1_ps(-0.1784f);
   const __m256 mull = _mm256_set1_ps(-0.0663f);

   return _mm256_mul_ps(x, _mm256_add_ps(pi_4_mul,
                             _mm256_add_ps(_mm256_mul_ps(add, _mm256_abs_ps(x)),
                                    _mm256_mul_ps(mull, _mm256_mul_ps(x, x)))));
}

static inline FN_PREALIGN __m256
_mm256_atan_ps(__m256 a)
{
   // Preserve sign and take absolute value of input
   const __m256 sign_mask = _mm256_set1_ps(-0.0f);
   __m256 sign = _mm256_and_ps(a, sign_mask); // Preserve sign
   __m256 abs_a = _mm256_andnot_ps(sign_mask, a); // Absolute value

   // w = a > tan(PI / 8)
   const __m256 tan_pi_8 = _mm256_set1_ps(GMATH_TAN_PI_8);
   __m256 w = _mm256_cmp_ps(abs_a, tan_pi_8, _CMP_GT_OQ);

   // x = a > tan(3 * PI / 8)
   const __m256 tan_3pi_8 = _mm256_set1_ps(GMATH_TAN_3PI_8);
   __m256 x = _mm256_cmp_ps(abs_a, tan_3pi_8, _CMP_GT_OQ);

   // z = ~w & x
   __m256 z = _mm256_andnot_ps(w, x);

   // y = (~w & PI/2) | (z & PI/4)
   __m256 y = _mm256_or_ps(_mm256_andnot_ps(w, _mm256_set1_ps(GMATH_PI_2)),
                           _mm256_and_ps(z, _mm256_set1_ps(GMATH_PI_4)));

   // w = (w & -1/a) | (z & (a - 1) * 1/(a + 1))
   const __m256 one = _mm256_set1_ps(1.0f);
   __m256 inv_a = _mm256_rcp_ps(abs_a); // -1 / a
   __m256 w_part1 = _mm256_and_ps(w, inv_a);
   __m256 w_part2 = _mm256_and_ps(z, _mm256_mul_ps(_mm256_sub_ps(abs_a, one), _mm256_rcp_ps(_mm256_add_ps(abs_a, one))));
   __m256 w_final = _mm256_or_ps(w_part1, w_part2);

   // a = (~x & a) | w
   __m256 adjusted_a = _mm256_or_ps(_mm256_andnot_ps(x, abs_a), w_final);

   // Polynomial approximation for arctangent
    __m256 poly, a2 = _mm256_mul_ps(adjusted_a, adjusted_a);
    poly = _mm256_fmadd_ps(_mm256_set1_ps(ATAN_COEF1), a2, _mm256_set1_ps(ATAN_COEF2));
    poly = _mm256_fmadd_ps(poly, a2, _mm256_set1_ps(ATAN_COEF3));
    poly = _mm256_fmadd_ps(poly, a2, _mm256_set1_ps(ATAN_COEF4));
   __m256 result = _mm256_fmadd_ps(poly, adjusted_a, adjusted_a);
   result = _mm256_add_ps(y, result);

   return _mm256_or_ps(result, sign);
}


void
_batch_get_average_rms_fma3(const_float32_ptr s, size_t num, float *rms, float *peak)
{
   size_t stmp, step, total;
   double rms_total = 0.0;
   float peak_cur = 0.0f;
   int i;

   *rms = *peak = 0;

   if (!num) return;

   total = num;
   stmp = (size_t)s & MEMMASK;
   if (stmp)
   {
      i = (MEMALIGN - stmp)/sizeof(float);
      if (i <= num)
      {
         do
         {
            float samp = *s++;            // rms
            float val = samp*samp;
            rms_total += val;
            if (val > peak_cur) peak_cur = val;
         }
         while (--i);
      }
   }

   if (num)
   {
      __m256* sptr = (__m256*)s;

      step = 3*sizeof(__m256)/sizeof(float);

      i = num/step;
      if (i)
      {
         union {
             __m256 ps;
             float f[8];
         } rms1, rms2, rms3, peak1, peak2, peak3;

         peak1.ps = peak2.ps = peak3.ps = _mm256_setzero_ps();
         rms1.ps = rms2.ps = rms3.ps = _mm256_setzero_ps();

         s += i*step;
         num -= i*step;
         do
         {
            __m256 smp1 = _mm256_load_ps((const float*)sptr++);
            __m256 smp2 = _mm256_load_ps((const float*)sptr++);
            __m256 smp3 = _mm256_load_ps((const float*)sptr++);
            __m256 val1, val2, val3;

            // Can't use fmadd since peakX uses valX too
            val1 = _mm256_mul_ps(smp1, smp1);
            val2 = _mm256_mul_ps(smp2, smp2);
            val3 = _mm256_mul_ps(smp3, smp3);

            rms1.ps = _mm256_add_ps(rms1.ps, val1);
            rms2.ps = _mm256_add_ps(rms2.ps, val2);
            rms3.ps = _mm256_add_ps(rms3.ps, val3);

            peak1.ps = _mm256_max_ps(peak1.ps, val1);
            peak2.ps = _mm256_max_ps(peak2.ps, val2);
            peak3.ps = _mm256_max_ps(peak3.ps, val3);
         }
         while(--i);

         rms_total += hsum256_ps_fma3(rms1.ps);
         rms_total += hsum256_ps_fma3(rms2.ps);
         rms_total += hsum256_ps_fma3(rms3.ps);

         peak1.ps = _mm256_max_ps(peak1.ps, peak2.ps);
         peak1.ps = _mm256_max_ps(peak1.ps, peak3.ps);
         _mm256_zeroupper();

         if (peak1.f[0] > peak_cur) peak_cur = peak1.f[0];
         if (peak1.f[1] > peak_cur) peak_cur = peak1.f[1];
         if (peak1.f[2] > peak_cur) peak_cur = peak1.f[2];
         if (peak1.f[3] > peak_cur) peak_cur = peak1.f[3];
         if (peak1.f[4] > peak_cur) peak_cur = peak1.f[4];
         if (peak1.f[5] > peak_cur) peak_cur = peak1.f[5];
         if (peak1.f[6] > peak_cur) peak_cur = peak1.f[6];
         if (peak1.f[7] > peak_cur) peak_cur = peak1.f[7];
      }

      if (num)
      {
         i = num;
         do
         {
            float samp = *s++;            // rms
            float val = samp*samp;
            rms_total += val;
            if (val > peak_cur) peak_cur = val;
         }
         while (--i);
      }
   }

   *rms = (float)sqrt(rms_total/total);
   *peak = sqrtf(peak_cur);
}

FN_PREALIGN void
_batch_fmadd_fma3(float32_ptr dst, const_float32_ptr src, size_t num, float v, float vstep)
{
   int need_step = (fabsf(vstep) <= LEVEL_90DB) ? 0 : 1;
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   if (!num || (fabsf(v) <= LEVEL_128DB && !need_step)) return;

   // volume ~= 1.0f and no change requested: just add both buffers
   if (fabsf(v - 1.0f) < LEVEL_90DB && !need_step)
   {
      _batch_fadd_avx(dst, src, num);
      return;
   }

   /*
    * Always assume need_step since it doesn't make any difference
    * in rendering speed.
    */

   /* work towards a 32-byte aligned d (and hence 32-byte aligned s) */
   dtmp = (size_t)d & MEMMASK;
   if (dtmp && num)
   {
      i = (MEMALIGN - dtmp)/sizeof(float);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ += *s++ * v;
            v += vstep;
         } while(--i);
      }
   }
   stmp = (size_t)s & MEMMASK;

   step = 2*sizeof(__m256)/sizeof(float);
   i = num/step;
   if (i)
   {
      const __m256 dvstep0 = _mm256_set_ps(7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f);
      const __m256 dvstep1 = _mm256_set_ps(15.0f, 14.0f, 13.0f, 12.0f, 11.0f, 10.0f, 9.0f, 8.0f);
      __m256 ymm0, ymm1, dv, tv0, tv1;
      __m256 *sptr = (__m256 *)s;
      __m256 *dptr = (__m256 *)d;

      assert(step == 2*8);
      dv = _mm256_set1_ps(vstep*step);
      tv0 = _mm256_fmadd_ps(dvstep0, _mm256_set1_ps(vstep), _mm256_set1_ps(v));
      tv1 = _mm256_fmadd_ps(dvstep1, _mm256_set1_ps(vstep), _mm256_set1_ps(v));
      v += i*step*vstep;

      num -= i*step;
      s += i*step;
      d += i*step;
      if (stmp)
      {
         do
         {
            ymm0 = _mm256_loadu_ps((const float*)sptr++);
            ymm1 = _mm256_loadu_ps((const float*)sptr++);

            ymm0 = _mm256_fmadd_ps(ymm0, tv0, _mm256_load_ps((const float*)dptr));
            ymm1 = _mm256_fmadd_ps(ymm1, tv1, _mm256_load_ps((const float*)(dptr+1)));

            tv0 = _mm256_add_ps(tv0, dv);
            tv1 = _mm256_add_ps(tv1, dv);

            _mm256_store_ps((float*)dptr++, ymm0);
            _mm256_store_ps((float*)dptr++, ymm1);
         }
         while(--i);
      }
      else
      {
         do
         {
            ymm0 = _mm256_load_ps((const float*)sptr++);
            ymm1 = _mm256_load_ps((const float*)sptr++);

            ymm0 = _mm256_fmadd_ps(ymm0, tv0, _mm256_load_ps((const float*)dptr));
            ymm1 = _mm256_fmadd_ps(ymm1, tv1, _mm256_load_ps((const float*)(dptr+1)));

            tv0 = _mm256_add_ps(tv0, dv);
            tv1 = _mm256_add_ps(tv1, dv);

            _mm256_store_ps((float*)dptr++, ymm0);
            _mm256_store_ps((float*)dptr++, ymm1);
         }
         while(--i);
      }
      _mm256_zeroupper();
   }

   if (num)
   {
      i = num;
      do {
         *d++ += *s++ * v;
         v += vstep;
      } while(--i);
   }
}

#define NUM_BUTTERWORTH	4
#define NUM_BESSEL	8
void
_batch_freqfilter_float_fma3(float32_ptr dptr, const_float32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   const_float32_ptr s = sptr;

   if (num)
   {
      float k, *cptr, *hist;
      float h0, h1;
      int stage;

      cptr = filter->coeff;
      hist = filter->freqfilter->history[t];
      stage = filter->no_stages;
      if (!stage) stage++;

      assert(((size_t)cptr & MEMMASK16) == 0);

      h0 = hist[0];
      h1 = hist[1];

      k = filter->k;
      if (filter->state == AAX_BUTTERWORTH)
      {
         float32_ptr d = dptr;
         int j, i = num/NUM_BUTTERWORTH;
         int rest = num-i*NUM_BUTTERWORTH;

         if (i)
         {
            do
            {
               for (j=0; j<NUM_BUTTERWORTH; ++j)
               {
                  float nsmp = (*s++ * k) + h0 * cptr[0] + h1 * cptr[1];
                  *d++ = nsmp             + h0 * cptr[2] + h1 * cptr[3];

                  h1 = h0;
                  h0 = nsmp;
               }
            }
            while (--i);
         }

         if (rest)
         {
            i = rest;
            do
            {
               float nsmp = (*s++ * k) + h0 * cptr[0] + h1 * cptr[1];
               *d++ = nsmp             + h0 * cptr[2] + h1 * cptr[3];

               h1 = h0;
               h0 = nsmp;
            }
            while (--i);
         }
      }
      else // AAX_BESSEL
      {
         float32_ptr d = dptr;
         int j, i = num/NUM_BESSEL;
         int rest = num-i*NUM_BESSEL;

         if (i)
         {
            float smp;
            do
            {
               for (j=0; j<NUM_BESSEL; ++j)
               {
                  smp = (*s++ * k) + h0 * cptr[0] + h1 * cptr[1];
                  *d++ = smp;

                  h1 = h0;
                  h0 = smp;
               }
            }
            while (--i);
         }

         if (rest)
         {
            i = rest;
            do
            {
               float smp = (*s++ * k) + ((h0 * cptr[0]) + (h1 * cptr[1]));
               *d++ = smp;

               h1 = h0;
               h0 = smp;
            }
            while (--i);
         }
      }

      *hist++ = h0;
      *hist++ = h1;

      while(--stage)
      {
         cptr += 4;

         h0 = hist[0];
         h1 = hist[1];

         if (filter->state == AAX_BUTTERWORTH)
         {
            float32_ptr d = dptr;
            int j, i = num/NUM_BUTTERWORTH;
            int rest = num-i*NUM_BUTTERWORTH;

            if (i)
            {
               do
               {
                  for (j=0; j<NUM_BUTTERWORTH; ++j)
                  {
                     float nsmp = *d + h0 * cptr[0] + h1 * cptr[1];
                     *d++ = nsmp     + h0 * cptr[2] + h1 * cptr[3];

                     h1 = h0;
                     h0 = nsmp;
                  }
               }
               while (--i);
            }

            if (rest)
            {
               i = rest;
               do
               {
                  float nsmp = *d + h0 * cptr[0] + h1 * cptr[1];
                  *d++ = nsmp     + h0 * cptr[2] + h1 * cptr[3];

                  h1 = h0;
                  h0 = nsmp;
               }
               while (--i);
            }
         }
         else
         {
            float32_ptr d = dptr;
            int j, i = num/NUM_BESSEL;
            int rest = num-i*NUM_BESSEL;

            if (i)
            {
               do
               {
                  for (j=0; j<NUM_BESSEL; ++j)
                  {
                     float smp = *d + h0 * cptr[0] + h1 * cptr[1];
                     *d++ = smp;

                     h1 = h0;
                     h0 = smp;
                  }
               }
               while (--i);
            }

            if (rest)
            {
               i = rest;
               do
               {
                  float smp = *d + h0 * cptr[0] + h1 * cptr[1];
                  *d++ = smp;

                  h1 = h0;
                  h0 = smp;
               }
               while (--i);
            }
         }

         *hist++ = h0;
         *hist++ = h1;
      }
      _batch_fmul_value(dptr, dptr, num, filter->gain, 1.0f);
   }
}

static inline void
_aaxBufResampleDecimate_float_fma3(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float32_ptr s = (float32_ptr)sptr;
   float32_ptr d = dptr;
   float samp, dsamp;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor >= 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   d += dmin;

   samp = *s++;                 // n+(step-1)
   dsamp = *s - samp;           // (n+1) - n

   i = dmax-dmin;
   if (i)
   {
      if (freq_factor == 2.0f)
      {
         do {
            *d++ = (*s + *(s+1))*0.5f;
            s += 2;
         }
         while (--i);
      }
      else
      {
         do
         {
            size_t step;

            *d++ = samp + (dsamp * smu);

            smu += freq_factor;
            step = (size_t)floorf(smu);

            smu -= step;
            s += step-1;
            samp = *s++;
            dsamp = *s - samp;
         }
         while (--i);
      }
   }
}

static inline void
_aaxBufResampleLinear_float_fma3(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float32_ptr sptr = (float32_ptr)s;
   float32_ptr dptr = d;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor < 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   dptr += dmin;

   i = dmax-dmin;
   if (i)
   {
      __m128 samp = _mm_load_ss(sptr++);       // n
      __m128 nsamp = _mm_load_ss(sptr++);      // (n+1)
      __m128 dsamp = _mm_sub_ss(nsamp, samp);  // (n+1) - n

      do
      {
         __m128 tau = _mm_set_ss(smu);
         __m128 dout = samp;

         smu += freq_factor;

         // fmadd
         dout = _mm_fmadd_ss(tau, dsamp, dout);

         if (smu >= 1.0)
         {
            samp = nsamp;
            nsamp = _mm_load_ss(sptr++);

            smu -= 1.0;;

            dsamp = _mm_sub_ss(nsamp, samp);
         }
         _mm_store_ss(dptr++, dout);
      }
      while (--i);
   }
}


static inline void
_aaxBufResampleCubic_float_fma3(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float32_ptr sptr = (float32_ptr)s;
   float32_ptr dptr = d;
   vec4f_t y, a;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

   dptr += dmin;

   y.v4[0] = *sptr++;
   y.v4[1] = *sptr++;
   y.v4[2] = *sptr++;
   y.v4[3] = *sptr++;

   a.v4[0] = -y.v4[0] + y.v4[1] - y.v4[2] + y.v4[3];
   a.v4[1] =  y.v4[0] - y.v4[1];
   a.v4[2] = -y.v4[0] + y.v4[2];

   a.v4[1] -= a.v4[0];

   i = dmax-dmin;
   if (i)
   {
      do
      {
         float smu2, ftmp;

         smu2 = smu*smu;
         ftmp = (a.v4[0]*smu*smu2 + a.v4[1]*smu2 + a.v4[2]*smu + y.v4[1]);
         *dptr++ = ftmp;

         smu += freq_factor;
         if (smu >= 1.0)
         {
            smu--;
            a.v4[0] += y.v4[0];

            y.v4[0] = y.v4[1];
            y.v4[1] = y.v4[2];
            y.v4[2] = y.v4[3];
            y.v4[3] = *sptr++;

            a.v4[0] = y.v4[3] - a.v4[0];
            a.v4[1] = y.v4[0] - y.v4[1] - a.v4[0];
            a.v4[2] = -y.v4[0] + y.v4[2];
         }
      }
      while (--i);
   }
}

void
_batch_resample_float_fma3(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);
   assert(d != s);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_float_fma3(d, s, dmin, dmax, smu, fact);
   } else if (fact < 1.0f) {
      _aaxBufResampleLinear_float_fma3(d, s, dmin, dmax, smu, fact);
   } else if (fact >= 1.0f) {
      _aaxBufResampleDecimate_float_fma3(d, s, dmin, dmax, smu, fact);
   } else {
//    _aaxBufResampleNearest_float_fma3(d, s, dmin, dmax, smu, fact);
      memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}

float *
_aax_generate_waveform_fma3(float32_ptr rv, size_t no_samples, float freq, float phase, enum aaxSourceType wtype)
{
   const_float32_ptr phases = _harmonic_phases[wtype-AAX_1ST_WAVE];
   const_float32_ptr harmonics = _harmonics[wtype-AAX_1ST_WAVE];

   switch(wtype)
   {
   case AAX_SINE:
      rv = _aax_generate_waveform_cpu(rv, no_samples, freq, phase, wtype);
      break;
   case AAX_SAWTOOTH:
   case AAX_SQUARE:
   case AAX_TRIANGLE:
   case AAX_CYCLOID:
   case AAX_IMPULSE:
   case AAX_PULSE:
      if (rv)
      {
         const __m256 one = _mm256_set1_ps(1.0f);
         const __m256 two = _mm256_set1_ps(2.0f);
         const __m256 eight = _mm256_set1_ps(8.0f);
         __m256 phase8, freq8, h8;
         __m256 ngain, nfreq;
         __m256 hdt, s, mask;
         int i, h;
         float *ptr;

         assert(MAX_HARMONICS % 8 == 0);

         phase8 = _mm256_set1_ps(-1.0f + phase/GMATH_PI);
         freq8 = _mm256_set1_ps(freq);
         h8 = _mm256_set_ps(8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f);

         nfreq = _mm256_div_ps(freq8, h8);
         ngain = _mm256_and_ps(_mm256_cmp_ps(two, nfreq, _CMP_LT_OS), _mm256_load_ps(harmonics));
         hdt = _mm256_div_ps(two, nfreq);

         ptr = rv;
         i = no_samples;
         s = _mm256_add_ps(phase8, _mm256_load_ps(phases));

         mask = _mm256_cmp_ps(s, one, _CMP_GT_OS);
         s = _mm256_sub_ps(s, _mm256_and_ps(mask, one));
         do
         {
            __m256 rv = fast_sin8_fma3(s);

            *ptr++ = hsum256_ps_fma3(_mm256_mul_ps(ngain, rv));

            s = _mm256_add_ps(s, hdt);
            s = _mm256_sub_ps(s, _mm256_and_ps(two, _mm256_cmp_ps(s, one, _CMP_GE_OS)));
         }
         while (--i);

         h8 = _mm256_add_ps(h8, eight);
         for(h=8; h<MAX_HARMONICS; h += 8)
         {
            nfreq = _mm256_div_ps(freq8, h8);
            ngain = _mm256_and_ps(_mm256_cmp_ps(two, nfreq, _CMP_LT_OS), _mm256_load_ps(harmonics+h));
            if (_mm256_testz_ps_fma3(ngain))
            {
               hdt = _mm256_div_ps(two, nfreq);

               ptr = rv;
               i = no_samples;
               s = _mm256_add_ps(phase8, _mm256_load_ps(phases+h));

               mask = _mm256_cmp_ps(s, one, _CMP_GT_OS);
               s = _mm256_sub_ps(s, _mm256_and_ps(mask, one));
               do
               {
                  __m256 rv = fast_sin8_fma3(s);

                  *ptr++ += hsum256_ps_fma3(_mm256_mul_ps(ngain, rv));

                  s = _mm256_add_ps(s, hdt);
                  s = _mm256_sub_ps(s, _mm256_and_ps(two, _mm256_cmp_ps(s, one, _CMP_GE_OS)));
               }
               while (--i);
            }
            h8 = _mm256_add_ps(h8, eight);
         }
         _mm256_zeroupper();
      }
      break;
   default:
      break;
   }
   return rv;
}

#define FC      50.0f // 50Hz high-pass EMA filter cutoff frequency
float *
_aax_generate_noise_fma3(float32_ptr rv, size_t no_samples, uint64_t seed, unsigned char skip, float fs)
{
   if (rv)
   {
      float (*rnd_fn)() = _aax_random;
      float rnd_skip = skip ? skip : 1.0f;
      float ds, prev, alpha;
      float *end = rv + no_samples;
      float *ptr = rv;

      if (seed)
      {
          _aax_srand(seed);
          rnd_fn = _aax_seeded_random;
      }

      prev = 0.0f;
      alpha = 1.0f;
      // exponential moving average (ema) filter
      // to filter frequencies below FC (50Hz)
      _aax_ema_compute(FC, fs, &alpha);

      ds = FC/fs;

      memset(rv, 0, no_samples*sizeof(float));
      do
      {
         float rnd = 0.5f*rnd_fn();
         rnd = rnd - _MINMAX(rnd, -ds, ds);

         // exponential moving average filter
         prev = (1.0f-alpha)*prev + alpha*rnd;
         *ptr += rnd - prev; // high-pass

         ptr += (int)rnd_skip;
         if (skip > 1) {
            rnd_skip = 1.0f + fabsf((2*skip-rnd_skip)*rnd_fn());
         }
      }
      while (ptr < end);
   }
   return rv;
}

void
_batch_limit_fma3(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   float *d = (float*)dptr;
   float *s = (float*)sptr;
   size_t i, step;
   size_t dtmp, stmp;

   if (!num) return;

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if (dtmp || stmp)                    /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      _batch_limit_sse2(d, s, num);
      return;
   }

   if (num)
   {
      __m256 *dptr = (__m256*)d;
      __m256* sptr = (__m256*)s;

      step = sizeof(__m256)/sizeof(float);

      i = num/step;
      if (i)
      {
         const __m256 xmin = _mm256_set1_ps(-1.55f);
         const __m256 xmax = _mm256_set1_ps(1.55f);
         const __m256 mul = _mm256_set1_ps(MUL*GMATH_1_PI_2);
         const __m256 imul = _mm256_set1_ps(IMUL);
         __m256 xmm0, xmm1;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xmm0 = _mm256_load_ps((const float*)sptr++);

            xmm0 = _mm256_mul_ps(xmm0, imul);
            xmm0 = _mm256_min_ps(_mm256_max_ps(xmm0, xmin), xmax);
            xmm1 = _mm256_mul_ps(mul, fast_atan8_fma3(xmm0));

            _mm256_store_ps((float*)dptr++, xmm1);
         }
         while(--i);
         _mm256_zeroupper();
      }

      if (num) {
         _batch_limit_sse2(d, s, num);
      }
   }
}

void
_batch_atanps_fma3(void_ptr dst, const_void_ptr src, size_t num)
{
   float *d = (float*)dst;
   float *s = (float*)src;
   size_t i, step;
   size_t dtmp, stmp;

   if (!num) return;

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if (dtmp || stmp)                    /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      _batch_atanps_sse2(d, s, num);
      return;
   }

   if (num)
   {
      __m256 *dptr = (__m256*)d;
      __m256* sptr = (__m256*)s;

      step = sizeof(__m256)/sizeof(float);

      i = num/step;
      if (i)
      {
//       const __m256 xmin = _mm256_set1_ps(-1.55);
//       const __m256 xmax = _mm256_set1_ps(1.55);
         const __m256 mul = _mm256_set1_ps(MUL*GMATH_1_PI_2);
         const __m256 imul = _mm256_set1_ps(IMUL);
         __m256 xmm0, xmm1;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xmm0 = _mm256_mul_ps(imul, _mm256_load_ps((const float*)sptr++));
            xmm1 = _mm256_atan_ps(xmm0);
            _mm256_store_ps((float*)dptr++, _mm256_mul_ps(xmm1, mul));
         }
         while(--i);
         _mm256_zeroupper();
      }

      if (num) {
         _batch_atanps_sse2(d, s, num);
      }
   }
}

#else
typedef int make_iso_compilers_happy;
#endif /* __FMA__ */

