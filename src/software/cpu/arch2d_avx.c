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

#include <math.h>	/* for floorf */

#include "base/random.h"
#include "software/rbuf_int.h"
#include "arch2d_simd.h"

#ifdef __AVX__

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
hsum256_ps_avx(__m256 v)
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
_mm256_testz_ps_avx(__m256 x)
{
   __m256i zero = _mm256_setzero_si256();
   return _mm256_testz_si256(_mm256_castps_si256(x), zero);
}

static inline __m256    // range -1.0f .. 1.0f
fast_sin8_avx(__m256 x)
{
   const __m256 four = _mm256_set1_ps(-4.0f);
   return _mm256_mul_ps(four,_mm256_sub_ps(x, _mm256_mul_ps(x, _mm256_abs_ps(x))));
}

// Use the slower, more accurate algorithm:
//    M_PI_4*x - x*(fabs(x) - 1)*(0.2447 + 0.0663*fabs(x)); // -1 < x < 1
//    which equals to: x*(1.03 - 0.1784*abs(x) - 0.0663*x*x)
static inline __m256
fast_atan8_avx(__m256 x)
{
   const __m256 pi_4_mul = _mm256_set1_ps(1.03f);
   const __m256 add = _mm256_set1_ps(-0.1784f);
   const __m256 mul = _mm256_set1_ps(-0.0663f);

   return _mm256_mul_ps(x, _mm256_add_ps(pi_4_mul,
                             _mm256_add_ps(_mm256_mul_ps(add, _mm256_abs_ps(x)),
                                    _mm256_mul_ps(mul, _mm256_mul_ps(x, x)))));
}

static inline FN_PREALIGN __m256
_mm256_fmadd_ps(__m256 a, __m256 b, __m256 c) {
   return _mm256_add_ps(_mm256_mul_ps(a, b), c);
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
   __m256 inv_a = _mm256_rcp_ps(a); // -1 / a
   __m256 w_part1 = _mm256_and_ps(w, inv_a);
   __m256 w_part2 = _mm256_and_ps(z, _mm256_mul_ps(_mm256_sub_ps(a, one), _mm256_rcp_ps(_mm256_add_ps(a, one))));
   __m256 w_final = _mm256_or_ps(w_part1, w_part2);

   // a = (~x & a) | w
   __m256 adjusted_a = _mm256_or_ps(_mm256_andnot_ps(x, abs_a), w_final);

   // Polynomial approximation for arctangent
   __m256 poly, a2 = _mm256_mul_ps(adjusted_a, adjusted_a);
   poly = _mm256_fmadd_ps(_mm256_set1_ps(ATAN_COEF1), a2, _mm256_set1_ps(ATAN_COEF2));
   poly = _mm256_fmadd_ps(poly, a2, _mm256_set1_ps(ATAN_COEF3));
   poly = _mm256_fmadd_ps(poly, a2, _mm256_set1_ps(ATAN_COEF4));
   __m256 result = _mm256_fmadd_ps(poly, _mm256_mul_ps(a2, adjusted_a), adjusted_a);

   return _mm256_or_ps(result, sign);
}

static inline __m256
copysign_avx(__m256 x, __m256 y)
{
    const __m256 sign_mask = _mm256_set1_ps(-0.0f);
    __m256 y_sign = _mm256_and_ps(y, sign_mask);
    __m256 abs_x = _mm256_andnot_ps(sign_mask, x);
    return _mm256_or_ps(abs_x, y_sign);
}

void
_batch_dc_shift_avx(float32_ptr d, const_float32_ptr s, size_t num, float offset)
{
   size_t i, step;
   size_t dtmp, stmp;

   if (!num || offset == 0.0f)
   {
      if (num && d != s) {
         memcpy(d, s, num*sizeof(float));
      }
      return;
   }

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if (dtmp || stmp)                    /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      _batch_dc_shift_cpu(d, s, num, offset);
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
         const __m256 one = _mm256_set1_ps(1.0f);
         __m256 xoffs = _mm256_set1_ps(offset);

         num -= i*step;
         d += i*step;
         s += i*step;
         do
         {
             __m256 xsamp = _mm256_load_ps((const float*)sptr++);
             __m256 xdptr, xfact;

             xfact = copysign_avx(xoffs, xsamp);
             xfact = _mm256_sub_ps(one, xfact);

             xdptr = _mm256_add_ps(xoffs, _mm256_mul_ps(xsamp, xfact));

             _mm256_store_ps((float*)dptr++, xdptr);
         } while(--i);
         _mm256_zeroupper();

         if (num)
         {
            i = num;
            do
            {
                float samp = *s++;
                float fact = 1.0f-copysignf(offset, samp);
                *d++ = offset + samp*fact;

            } while(--i);
         }
      }
   }
}

void
_batch_wavefold_avx(float32_ptr d, const_float32_ptr s, size_t num, float threshold)
{
   size_t i, step;
   size_t dtmp, stmp;

   if (!num || threshold == 0.0f)
   {
      if (num && d != s) {
         memcpy(d, s, num*sizeof(float));
      }
      return;
   }

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if (dtmp || stmp)                    /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      _batch_wavefold_cpu(d, s, num, threshold);
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
         static const float max = (float)(1 << 23);
         __m256 xthresh, xthresh2;
         float threshold2;

         threshold = max*threshold;
         threshold2 = 2.0f*threshold;

         xthresh = _mm256_set1_ps(threshold);
         xthresh2 = _mm256_set1_ps(threshold2);

         num -= i*step;
         d += i*step;
         s += i*step;
         do
         {
             __m256 xsamp = _mm256_load_ps((const float*)sptr++);
             __m256 xasamp = _mm256_abs_ps(xsamp);
             __m256 xmask = _mm256_cmp_ps(xasamp, xthresh, _CMP_GT_OS);

             xasamp = copysign_avx(_mm256_sub_ps(xthresh2, xasamp), xsamp);

             _mm256_store_ps((float*)dptr++, _mm256_blendv_ps(xsamp, xasamp, xmask));
         } while(--i);
         _mm256_zeroupper();

         if (num)
         {
            i = num;
            do
            {
               float samp = *s++;
               float asamp = fabsf(samp);
               if (asamp > threshold) {
                  samp = copysignf(threshold2 - asamp, samp);
               }
               *d++ = samp;
            } while(--i);
         }
      }
   }
}

void
_batch_get_average_rms_avx(const_float32_ptr s, size_t num, float *rms, float *peak)
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
         __m256 rms1, rms2, rms3;
         union {
             __m256 ps;
             float f[8];
         } peak1, peak2, peak3;

         peak1.ps = peak2.ps = peak3.ps = _mm256_setzero_ps();
         rms1 = rms2 = rms3 = _mm256_setzero_ps();

         s += i*step;
         num -= i*step;
         do
         {
            __m256 smp1 = _mm256_load_ps((const float*)sptr++);
            __m256 smp2 = _mm256_load_ps((const float*)sptr++);
            __m256 smp3 = _mm256_load_ps((const float*)sptr++);
            __m256 val1, val2, val3;

            val1 = _mm256_mul_ps(smp1, smp1);
            val2 = _mm256_mul_ps(smp2, smp2);
            val3 = _mm256_mul_ps(smp3, smp3);

            rms1 = _mm256_add_ps(rms1, val1);
            rms2 = _mm256_add_ps(rms2, val2);
            rms3 = _mm256_add_ps(rms3, val3);

            peak1.ps = _mm256_max_ps(peak1.ps, val1);
            peak2.ps = _mm256_max_ps(peak2.ps, val2);
            peak3.ps = _mm256_max_ps(peak3.ps, val3);
         }
         while(--i);

         rms_total += hsum256_ps_avx(rms1);
         rms_total += hsum256_ps_avx(rms2);
         rms_total += hsum256_ps_avx(rms3);

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
_batch_cvt24_ps_avx(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *d = (int32_t*)dst;
   float *s = (float*)src;

   if (((size_t)d & MEMMASK) != 0 || ((size_t)s & MEMMASK) != 0)
   {
      if (((size_t)d & MEMMASK16) == 0 || ((size_t)s & MEMMASK16) == 0)
      {
         _batch_cvt24_ps_sse_vex(dst, src, num);
         return;
      }
      else
      {
         float mul = MUL;
         size_t i = num;
         do {
            *d++ = (int32_t)(*s++ * mul);
         } while (--i);
         return;
      }
   }

   assert(((size_t)d & MEMMASK) == 0);
   assert(((size_t)s & MEMMASK) == 0);

   if (num)
   {
      size_t i, step;

      step = 6*sizeof(__m256)/sizeof(float);

      i = num/step;
      if (i)
      {
         const __m256 mul = _mm256_set1_ps(MUL);
         __m256 ymm0, ymm1, ymm2, ymm3, ymm4, ymm5;
         __m256i *dptr = (__m256i*)d;
         __m256* sptr = (__m256*)s;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            ymm0 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);
            ymm1 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);
            ymm2 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);
            ymm3 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);
            ymm4 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);
            ymm5 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);

            _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm0));
            _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm1));
            _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm2));
            _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm3));
            _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm4));
            _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm5));
         }
         while(--i);

         step = 2*sizeof(__m256)/sizeof(float);
         i = num/step;
         if (i)
         {
            num -= i*step;
            s += i*step;
            d += i*step;
            do
            {
               ymm0 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);
               ymm1 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);

               _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm0));
               _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm1));
            }
            while(--i);
         }
         _mm256_zeroupper();
      }

      if (num)
      {
         float mul = MUL;
         i = num;
         do {
            *d++ = (int32_t)(*s++ * mul);
         } while (--i);
      }
   }
}

FN_PREALIGN void
_batch_cvt24_ps24_avx(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *d = (int32_t*)dst;
   float *s = (float*)src;
   size_t i, step;
   size_t dtmp, stmp;

   if (!num) return;

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if ((dtmp || stmp) && dtmp != stmp)
   {
      _batch_cvt24_ps24_sse_vex(dst, src, num);
      return;
   }

   /* work towards a 32-byte aligned d (and hence 32-byte aligned sptr) */
   if (dtmp && num)
   {
      i = (MEMALIGN - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ = (int32_t)*s++;
         } while(--i);
      }
   }

   if (num)
   {
      step = 6*sizeof(__m256)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m256i ymm2i, ymm3i, ymm4i, ymm5i, ymm6i, ymm7i;
         __m256i *dptr = (__m256i*)d;
         __m256* sptr = (__m256*)s;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            ymm2i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));
            ymm3i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));
            ymm4i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));
            ymm5i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));
            ymm6i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));
            ymm7i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));

            _mm256_store_si256(dptr++, ymm2i);
            _mm256_store_si256(dptr++, ymm3i);
            _mm256_store_si256(dptr++, ymm4i);
            _mm256_store_si256(dptr++, ymm5i);
            _mm256_store_si256(dptr++, ymm6i);
            _mm256_store_si256(dptr++, ymm7i);
         }
         while(--i);

         step = 2*sizeof(__m256)/sizeof(float);
         i = num/step;
         if (i)
         {
            num -= i*step;
            s += i*step;
            d += i*step;
            do
            {
               ymm2i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));
               ymm3i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));

               _mm256_store_si256(dptr++, ymm2i);
               _mm256_store_si256(dptr++, ymm3i);
            }
            while(--i);
         }
         _mm256_zeroupper();
      }

      if (num)
      {
         i = num;
         do {
            *d++ = (int32_t)*s++;
         } while (--i);
      }
   }
}

FN_PREALIGN void
_batch_cvtps_24_avx(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *s = (int32_t*)src;
   float *d = (float*)dst;

   assert(((size_t)d & MEMMASK) == 0);
   assert(((size_t)s & MEMMASK) == 0);

   if (num)
   {
      size_t i, step;

      step = 6*sizeof(__m256i)/sizeof(int32_t);

      i = num/step;
      if (i)
      {
         const __m256 mul = _mm256_rcp_ps(_mm256_set1_ps(MUL));
         __m256 ymm0, ymm1, ymm2, ymm3, ymm4, ymm5;
         __m256i* sptr = (__m256i*)s;
         __m256 *dptr = (__m256*)d;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            ymm0 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm1 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm2 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm3 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm4 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm5 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));

            _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm0, mul));
            _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm1, mul));
            _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm2, mul));
            _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm3, mul));
            _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm4, mul));
            _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm5, mul));
         }
         while(--i);

         step = 2*sizeof(__m256i)/sizeof(int32_t);
         i = num/step;
         if (i)
         {
            num -= i*step;
            s += i*step;
            d += i*step;
            do
            {
               ymm0 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
               ymm1 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));

               _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm0, mul));
               _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm1, mul));
            }
            while(--i);
         }
         _mm256_zeroupper();
      }

      if (num)
      {
         float mul = IMUL;
         i = num;
         do {
            *d++ = (float)(*s++) * mul;
         } while (--i);
      }
   }
}

FN_PREALIGN void
_batch_cvtps24_24_avx(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *s = (int32_t*)src;
   float *d = (float*)dst;
   size_t i, step;
   size_t dtmp, stmp;

   assert(s != 0);
   assert(d != 0);

   if (!num) return;

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if ((dtmp || stmp) && dtmp != stmp)
   {
      _batch_cvtps24_24_sse_vex(dst, src, num);
      return;
   }

   /* work towards a 32-byte aligned d (and hence 32-byte aligned sptr) */
   if (dtmp && num)
   {
      i = (MEMALIGN - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ = (float)*s++;
         } while(--i);
      }
   }

   if (num)
   {
      step = 6*sizeof(__m256i)/sizeof(int32_t);

      i = num/step;
      if (i)
      {
         __m256 ymm0, ymm1, ymm2, ymm3, ymm4, ymm5;
         __m256i* sptr = (__m256i*)s;
         __m256 *dptr = (__m256*)d;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            ymm0 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm1 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm2 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm3 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm4 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm5 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));

            _mm256_store_ps((float*)dptr++, ymm0);
            _mm256_store_ps((float*)dptr++, ymm1);
            _mm256_store_ps((float*)dptr++, ymm2);
            _mm256_store_ps((float*)dptr++, ymm3);
            _mm256_store_ps((float*)dptr++, ymm4);
            _mm256_store_ps((float*)dptr++, ymm5);
         }
         while(--i);

         step = 2*sizeof(__m256i)/sizeof(int32_t);
         i = num/step;
         if (i)
         {
            i = num/step;
            num -= i*step;
            s += i*step;
            d += i*step;
            if (i)
            {
               do
               {
                  ymm4 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
                  ymm5 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));

                  _mm256_store_ps((float*)dptr++, ymm4);
                  _mm256_store_ps((float*)dptr++, ymm5);
               }
               while(--i);
            }
         }
         _mm256_zeroupper();
      }

      if (num)
      {
         i = num;
         do {
            *d++ = (float)*s++;
         } while (--i);
      }
   }
}

FN_PREALIGN void
_batch_fmul_avx(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   const_float32_ptr s = (float32_ptr)sptr;
   float32_ptr d = (float32_ptr)dptr;
   size_t i, step, dtmp, stmp;

   if (!num) return;

   /* work towards a 16-byte aligned d (and hence 16-byte aligned s) */
   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if (dtmp && num)
   {
      i = (MEMALIGN16 - dtmp)/sizeof(float);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ *= *s++;
         } while(--i);
      }
   }

   step = 4*sizeof(__m256)/sizeof(float);

   i = num/step;
   if (i)
   {
      __m256* sptr = (__m256*)s;
      __m256* dptr = (__m256*)d;
      __m256 ymm0, ymm1, ymm2, ymm3;
      __m256 ymm4, ymm5, ymm6, ymm7;

      num -= i*step;
      s += i*step;
      d += i*step;
      if (stmp)
      {
         do
         {
            ymm0 = _mm256_loadu_ps((const float*)(sptr++));
            ymm1 = _mm256_loadu_ps((const float*)(sptr++));
            ymm2 = _mm256_loadu_ps((const float*)(sptr++));
            ymm3 = _mm256_loadu_ps((const float*)(sptr++));

            ymm4 = _mm256_load_ps((const float*)(dptr+0));
            ymm5 = _mm256_load_ps((const float*)(dptr+1));
            ymm6 = _mm256_load_ps((const float*)(dptr+2));
            ymm7 = _mm256_load_ps((const float*)(dptr+3));

            ymm0 = _mm256_mul_ps(ymm0, ymm4);
            ymm1 = _mm256_mul_ps(ymm1, ymm5);
            ymm2 = _mm256_mul_ps(ymm2, ymm6);
            ymm3 = _mm256_mul_ps(ymm3, ymm7);

           _mm256_store_ps((float*)dptr++, ymm0);
           _mm256_store_ps((float*)dptr++, ymm1);
           _mm256_store_ps((float*)dptr++, ymm2);
           _mm256_store_ps((float*)dptr++, ymm3);
        }
        while(--i);
     }
     else
     {
        do
        {
           ymm0 = _mm256_load_ps((const float*)(sptr++));
           ymm1 = _mm256_load_ps((const float*)(sptr++));
           ymm2 = _mm256_load_ps((const float*)(sptr++));
           ymm3 = _mm256_load_ps((const float*)(sptr++));

           ymm4 = _mm256_load_ps((const float*)(dptr+0));
           ymm5 = _mm256_load_ps((const float*)(dptr+1));
           ymm6 = _mm256_load_ps((const float*)(dptr+2));
           ymm7 = _mm256_load_ps((const float*)(dptr+3));

           ymm0 = _mm256_mul_ps(ymm0, ymm4);
           ymm1 = _mm256_mul_ps(ymm1, ymm5);
           ymm2 = _mm256_mul_ps(ymm2, ymm6);
           ymm3 = _mm256_mul_ps(ymm3, ymm7);

           _mm256_store_ps((float*)dptr++, ymm0);
           _mm256_store_ps((float*)dptr++, ymm1);
           _mm256_store_ps((float*)dptr++, ymm2);
           _mm256_store_ps((float*)dptr++, ymm3);
        }
        while(--i);
     }
  }

  if (num)
  {
     i = num;
     do {
        *d++ *= *s++;
     } while(--i);
  }
}

FN_PREALIGN void
_batch_fmul_value_avx(float32_ptr dptr, const_float32_ptr sptr, size_t num, float numerator, float denomerator)
{
   const_float32_ptr s = (float32_ptr)sptr;
   float32_ptr d = (float32_ptr)dptr;
   size_t i, step, dtmp, stmp;
   float f = numerator/denomerator;

   if (!num) return;

   if (fabsf(f - 1.0f) < LEVEL_128DB) {
      if (sptr != dptr) memcpy(dptr, sptr, num*sizeof(float));
   } else if  (fabsf(f) <= LEVEL_128DB) {
      memset(dptr, 0, num*sizeof(float));
   }

   stmp = (size_t)s & MEMMASK;
   dtmp = (size_t)d & MEMMASK;
   /* work towards a 32-byte aligned d (and hence 32-byte aligned s) */
   if (dtmp && num)
   {
      i = (MEMALIGN - dtmp)/sizeof(float);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ = *s++ * f;
         } while(--i);
      }
   }

   step = sizeof(__m256)/sizeof(float);

   i = num/step;
   if (i)
   {
      __m256* sptr = (__m256*)s;
      __m256* dptr = (__m256*)d;
      __m256 tv, ymm0;

      num -= i*step;
      s += i*step;
      d += i*step;

      tv = _mm256_mul_ps(_mm256_set1_ps(numerator), _mm256_rcp_ps(_mm256_set1_ps(denomerator)));
      if (stmp)
      {
         do
         {
            ymm0 =_mm256_mul_ps(tv, _mm256_loadu_ps((const float*)(sptr++)));
            _mm256_store_ps((float*)dptr++, ymm0);
         }
         while(--i);
      }
      else
      {
         do
         {
            ymm0 = _mm256_mul_ps(tv, _mm256_load_ps((const float*)(sptr++)));
            _mm256_store_ps((float*)dptr++, ymm0);
         }
         while(--i);
      }
   }
   _mm256_zeroupper();

   if (num)
   {
      i = num;
      do {
         *d++ = *s++ * f;
      } while(--i);
   }
}

#if 0
FN_PREALIGN void
_batch_hadd_avx(float32_ptr dst, const_float16_ptr src, size_t num)
{
   float16_ptr s = (float16_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if ((dtmp || stmp) && dtmp != stmp)
   {
      i = num;                          /* improperly aligned,            */
      do                                /* let the compiler figure it out */
      {
         *d++ += HALF2FLOAT(*s);
          s++;
      }
      while (--i);
      return;
   }

   /* work towards a 32-byte aligned d (and hence 32-byte aligned s) */
   if (dtmp && num)
   {
      i = (MEMALIGN - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ += HALF2FLOAT(*s);
            s++;
         } while(--i);
      }
   }

   step = 8*sizeof(__m256)/sizeof(float);

   i = num/step;
   if (i)
   {
      __m256 ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
      __m128i* sptr = (__m128i*)s;
      __m256* dptr = (__m256*)d;

      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         ymm0 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
         ymm1 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
         ymm2 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
         ymm3 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
         ymm4 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
         ymm5 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
         ymm6 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
         ymm7 = _mm256_cvtph_ps(_mm_load_si128(sptr++));

         ymm0 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+0)), ymm0);
         ymm1 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+1)), ymm1);
         ymm2 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+2)), ymm2);
         ymm3 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+3)), ymm3);
         ymm4 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+4)), ymm4);
         ymm5 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+5)), ymm5);
         ymm6 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+6)), ymm6);
         ymm7 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+7)), ymm7);

         _mm256_store_ps((float*)dptr++, ymm0);
         _mm256_store_ps((float*)dptr++, ymm1);
         _mm256_store_ps((float*)dptr++, ymm2);
         _mm256_store_ps((float*)dptr++, ymm3);
         _mm256_store_ps((float*)dptr++, ymm4);
         _mm256_store_ps((float*)dptr++, ymm5);
         _mm256_store_ps((float*)dptr++, ymm6);
         _mm256_store_ps((float*)dptr++, ymm7);
      }
      while(--i);

      step = 2*sizeof(__m256)/sizeof(float);
      i = num/step;
      if (i)
      {
         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            ymm0 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
            ymm1 = _mm256_cvtph_ps(_mm_load_si128(sptr++));

            ymm0 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+0)), ymm0);
            ymm1 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+1)), ymm1);

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
         *d++ += HALF2FLOAT(*s);
         s++;
      } while(--i);
   }
}
#endif

#if 0
FN_PREALIGN void
_batch_hmadd_avx(float32_ptr dst, const_float16_ptr src, size_t num, float v, float vstep)
{
   float16_ptr s = (float16_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   if (!num || (v <= LEVEL_128DB && vstep <= LEVEL_128DB)) return;
   if (fabsf(v - 1.0f) < LEVEL_96DB && vstep <=  LEVEL_96DB)
   {
      _batch_hadd_avx(dst, src, num);
      return;
   }

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if ((dtmp || stmp) && dtmp != stmp) {
      return _batch_hmadd_sse_vex(dst, src, num, v, vstep);
   }

   /* work towards a 32-byte aligned d (and hence 32-byte aligned s) */
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

   step = 4*sizeof(__m256)/sizeof(float);

   i = num/step;
   if (i)
   {
      __m256 ymm0, ymm1, ymm2, ymm3;
      __m128i* sptr = (__m128i*)s;
      __m256 *dptr = (__m256*)d;

       vstep *= step;
      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         __m256 tv = _mm256_set1_ps(v);

         ymm0 = _mm256_mul_ps(_mm256_cvtph_ps(_mm_load_si128(sptr++)), tv);
         ymm1 = _mm256_mul_ps(_mm256_cvtph_ps(_mm_load_si128(sptr++)), tv);
         ymm2 = _mm256_mul_ps(_mm256_cvtph_ps(_mm_load_si128(sptr++)), tv);
         ymm3 = _mm256_mul_ps(_mm256_cvtph_ps(_mm_load_si128(sptr++)), tv);

         ymm0 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+0)), ymm0);
         ymm1 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+1)), ymm1);
         ymm2 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+2)), ymm2);
         ymm3 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+3)), ymm3);

         v += vstep;

         _mm256_store_ps((float*)dptr++, ymm0);
         _mm256_store_ps((float*)dptr++, ymm1);
         _mm256_store_ps((float*)dptr++, ymm2);
         _mm256_store_ps((float*)dptr++, ymm3);
      }
      while(--i);
      vstep /= step;


      step = 2*sizeof(__m256)/sizeof(float);
      i = num/step;
      if (i)
      {
         vstep *= step;
         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            __m256 tv = _mm256_set1_ps(v);

            ymm0 = _mm256_mul_ps(_mm256_cvtph_ps(_mm_load_si128(sptr++)), tv);
            ymm1 = _mm256_mul_ps(_mm256_cvtph_ps(_mm_load_si128(sptr++)), tv);

            ymm0 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+0)), ymm0);
            ymm1 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+1)), ymm1);

            v += vstep;

            _mm256_store_ps((float*)dptr++, ymm0);
            _mm256_store_ps((float*)dptr++, ymm1);
         }
         while(--i);
         vstep /= step;
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
#endif

FN_PREALIGN void
_batch_fadd_avx(float32_ptr dst, const_float32_ptr src, size_t num)
{
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

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
            *d++ += *s++;
         } while(--i);
      }
   }
   stmp = (size_t)s & MEMMASK;

   step = 2*sizeof(__m256)/sizeof(float);

   i = num/step;
   if (i)
   {
      __m256 ymm0, ymm1, ymm2, ymm3;
      __m256* sptr = (__m256*)s;
      __m256* dptr = (__m256*)d;

      assert(step == 2*8);

      num -= i*step;
      s += i*step;
      d += i*step;
      if (stmp)
      {
         do
         {
            ymm0 = _mm256_loadu_ps((const float*)sptr++);
            ymm1 = _mm256_loadu_ps((const float*)sptr++);

            ymm0 = _mm256_add_ps(ymm0, _mm256_load_ps((const float*)(dptr+0)));
            ymm1 = _mm256_add_ps(ymm1, _mm256_load_ps((const float*)(dptr+1)));

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

            ymm2 = _mm256_add_ps(ymm0, _mm256_load_ps((const float*)(dptr+0)));
            ymm3 = _mm256_add_ps(ymm1, _mm256_load_ps((const float*)(dptr+1)));

            _mm256_store_ps((float*)dptr++, ymm2);
            _mm256_store_ps((float*)dptr++, ymm3);
         }
         while(--i);
      }
      _mm256_zeroupper();
   }

   if (num)
   {
      i = num;
      do {
         *d++ += *s++;
      } while(--i);
   }
}

FN_PREALIGN void
_batch_fmadd_avx(float32_ptr dst, const_float32_ptr src, size_t num, float v, float vstep)
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

   step = sizeof(__m256)/sizeof(float);

   i = num/step;
   if (i)
   {
      __m256 dvstep = _mm256_set_ps(7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f);
      __m256 ymm0, ymm1, dv, tv;
      __m256* sptr = (__m256*)s;
      __m256* dptr = (__m256*)d;

      assert(step == 8);
      dvstep = _mm256_mul_ps(dvstep, _mm256_set1_ps(vstep));

      dv = _mm256_set1_ps(vstep*step);
      tv = _mm256_add_ps(_mm256_set1_ps(v), dvstep);
      v += step*vstep;

      num -= i*step;
      s += i*step;
      d += i*step;
      if (stmp)
      {
         do
         {
            ymm0 = _mm256_mul_ps(tv, _mm256_loadu_ps((const float*)sptr++));
            ymm1 = _mm256_add_ps(ymm0, _mm256_load_ps((const float*)dptr));

            tv = _mm256_add_ps(tv, dv);

            _mm256_store_ps((float*)dptr++, ymm0);
         }
         while(--i);
      }
      else
      {
         do
         {
            ymm0 = _mm256_mul_ps(tv, _mm256_load_ps((const float*)sptr++));
            ymm1 = _mm256_add_ps(ymm0, _mm256_load_ps((const float*)dptr));

            tv = _mm256_add_ps(tv, dv);

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

float *
_aax_generate_waveform_avx(float32_ptr rv, size_t no_samples, float freq, float phase, enum aaxSourceType wtype)
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
            __m256 rv = fast_sin8_avx(s);

            *ptr++ = hsum256_ps_avx(_mm256_mul_ps(ngain, rv));

            s = _mm256_add_ps(s, hdt);
            s = _mm256_sub_ps(s, _mm256_and_ps(two, _mm256_cmp_ps(s, one, _CMP_GE_OS)));
         }
         while (--i);

         h8 = _mm256_add_ps(h8, eight);
         for(h=8; h<MAX_HARMONICS; h += 8)
         {
            nfreq = _mm256_div_ps(freq8, h8);
            ngain = _mm256_and_ps(_mm256_cmp_ps(two, nfreq, _CMP_LT_OS), _mm256_load_ps(harmonics+h));
            if (_mm256_testz_ps_avx(ngain))
            {
               hdt = _mm256_div_ps(two, nfreq);

               ptr = rv;
               i = no_samples;
               s = _mm256_add_ps(phase8, _mm256_load_ps(phases+h));

               mask = _mm256_cmp_ps(s, one, _CMP_GT_OS);
               s = _mm256_sub_ps(s, _mm256_and_ps(mask, one));
               do
               {
                  __m256 rv = fast_sin8_avx(s);

                  *ptr++ += hsum256_ps_avx(_mm256_mul_ps(ngain, rv));

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
_aax_generate_noise_avx(float32_ptr rv, size_t no_samples, uint64_t seed, unsigned char skip, float fs)
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
_batch_limit_avx(void_ptr dst, const_void_ptr src, size_t num)
{
   float *d = (float*)dst;
   float *s = (float*)src;
   size_t i, step;
   size_t dtmp, stmp;

   if (!num) return;

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if (dtmp || stmp)  			/* improperly aligned,            */
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
         const __m256 xmin = _mm256_set1_ps(-1.55);
         const __m256 xmax = _mm256_set1_ps(1.55);
         const __m256 mul = _mm256_set1_ps(MUL*GMATH_1_PI_2);
         const __m256 imul = _mm256_set1_ps(IMUL);
         __m256 xmm0, xmm1;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xmm0 = _mm256_load_ps((const float*)sptr++);

            xmm0 = _mm256_mul_ps(xmm0,imul);
            xmm0 = _mm256_min_ps(_mm256_max_ps(xmm0, xmin), xmax);
            xmm1 = _mm256_mul_ps(mul, fast_atan8_avx(xmm0));

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
_batch_atanps_avx(void_ptr dst, const_void_ptr src, size_t num)
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
#endif /* AVX */

