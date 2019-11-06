/*
 * Copyright 2005-2018 by Erik Hofman.
 * Copyright 2009-2018 by Adalin B.V.
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

#include <stdio.h>
#include <math.h>	/* for floorf */


#include "software/rbuf_int.h"
#include "arch2d_simd.h"

#ifdef __AVX__

inline float	// range -1.0f .. 1.0f
fast_sin_sse_vex(float x)
{
   return -4.0f*(x - x*fabsf(x));
}

static inline FN_PREALIGN float
hsum_ps_sse_vex(__m128 v) {
   __m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
   __m128 sums = _mm_add_ps(v, shuf);
   shuf = _mm_movehl_ps(shuf, sums);
   sums = _mm_add_ss(sums, shuf);
   return _mm_cvtss_f32(sums);
}

static inline FN_PREALIGN float
hsum_half_ps_sse_vex(__m128 v) {
   __m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
   __m128 sums = _mm_add_ps(v, shuf);
   return _mm_cvtss_f32(sums);
}

static inline __m128
_mm_abs_ps(__m128 x) {
   return _mm_andnot_ps(_mm_set1_ps(-0.0f), x);
}

static inline int
_mm_testz_ps_sse_vex(__m128 x)
{
   __m128i zero = _mm_setzero_si128();
   return (_mm_movemask_epi8(_mm_cmpeq_epi32(_mm_castps_si128(x), zero)) != 0xFFFF);
}

static inline __m128	// range -1.0f .. 1.0f
fast_sin4_sse_vex(__m128 x)
{
   __m128 four = _mm_set1_ps(-4.0f);
   return _mm_mul_ps(four, _mm_sub_ps(x, _mm_mul_ps(x, _mm_abs_ps(x))));
}

float *
_aax_generate_waveform_sse_vex(float32_ptr rv, size_t no_samples, float freq, float phase, enum wave_types wtype)
{
   const_float32_ptr harmonics = _harmonics[wtype];
   if (wtype == _SINE_WAVE) {
      rv = _aax_generate_waveform_cpu(rv, no_samples, freq, phase, wtype);
   }
   else if (rv)
   {
      __m128 phase4, freq4, h4;
      __m128 one, two, four;
      __m128 ngain, nfreq;
      __m128 hdt, s;
      unsigned int i, h;
      float *ptr;

      assert(MAX_HARMONICS % 4 == 0);

      one = _mm_set1_ps(1.0f);
      two = _mm_set1_ps(2.0f);
      four = _mm_set1_ps(4.0f);

      phase4 = _mm_set1_ps(-1.0f + phase/GMATH_PI);
      freq4 = _mm_set1_ps(freq);
      h4 = _mm_set_ps(4.0f, 3.0f, 2.0f, 1.0f);

      nfreq = _mm_div_ps(freq4, h4);
      ngain = _mm_and_ps(_mm_cmplt_ps(two, nfreq), _mm_load_ps(harmonics));
      hdt = _mm_div_ps(two, nfreq);

      ptr = rv;
      i = no_samples;
      s = phase4;
      do
      {
         __m128 rv = fast_sin4_sse_vex(s);

         *ptr++ = hsum_ps_sse_vex(_mm_mul_ps(ngain, rv));

         s = _mm_add_ps(s, hdt);
         s = _mm_sub_ps(s, _mm_and_ps(two, _mm_cmpge_ps(s, one)));
      }
      while (--i);

      h4 = _mm_add_ps(h4, four);
      for(h=4; h<MAX_HARMONICS; h += 4)
      {
         nfreq = _mm_div_ps(freq4, h4);
         ngain = _mm_and_ps(_mm_cmplt_ps(two, nfreq), _mm_load_ps(harmonics+h));
         if (_mm_testz_ps_sse_vex(ngain))
         {
            hdt = _mm_div_ps(two, nfreq);

            ptr = rv;
            i = no_samples;
            s = phase4;
            do
            {
               __m128 rv = fast_sin4_sse_vex(s);

               *ptr++ += hsum_ps_sse_vex(_mm_mul_ps(ngain, rv));

               s = _mm_add_ps(s, hdt);
               s = _mm_sub_ps(s, _mm_and_ps(two, _mm_cmpge_ps(s, one)));
            }
            while (--i);
         }
         h4 = _mm_add_ps(h4, four);
      }
   }
   return rv;
}

FN_PREALIGN void
_batch_get_average_rms_sse_vex(const_float32_ptr s, size_t num, float *rms, float *peak)
{
   size_t stmp, step, total;
   double rms_total = 0.0;
   float peak_cur = 0.0f;
   unsigned int i;

   *rms = *peak = 0;

   if (!num) return;

   total = num;
   stmp = (size_t)s & MEMMASK16;
   if (stmp)
   {
      i = (MEMALIGN16 - stmp)/sizeof(float);
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
      __m128* sptr = (__m128*)s;

      step = 3*sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         union {
             __m128 ps;
             float f[4];
         } rms1, rms2, rms3, peak1, peak2, peak3;

         peak1.ps = peak2.ps = peak3.ps = _mm_setzero_ps();
         rms1.ps = rms2.ps = rms3.ps = _mm_setzero_ps();

         s += i*step;
         num -= i*step;
         do
         {
            __m128 smp1 = _mm_load_ps((const float*)sptr++);
            __m128 smp2 = _mm_load_ps((const float*)sptr++);
            __m128 smp3 = _mm_load_ps((const float*)sptr++);
            __m128 val1, val2, val3;

            val1 = _mm_mul_ps(smp1, smp1);
            val2 = _mm_mul_ps(smp2, smp2);
            val3 = _mm_mul_ps(smp3, smp3);

            rms1.ps = _mm_add_ps(rms1.ps, val1);
            rms2.ps = _mm_add_ps(rms2.ps, val2);
            rms3.ps = _mm_add_ps(rms3.ps, val3);

            peak1.ps = _mm_max_ps(peak1.ps, val1);
            peak2.ps = _mm_max_ps(peak2.ps, val2);
            peak3.ps = _mm_max_ps(peak3.ps, val3);
         }
         while(--i);

         rms_total += rms1.f[0] + rms1.f[1] + rms1.f[2] + rms1.f[3];
         rms_total += rms2.f[0] + rms2.f[1] + rms2.f[2] + rms2.f[3];
         rms_total += rms3.f[0] + rms3.f[1] + rms3.f[2] + rms3.f[3];

         peak1.ps = _mm_max_ps(peak1.ps, peak2.ps);
         peak1.ps = _mm_max_ps(peak1.ps, peak3.ps);

         if (peak1.f[0] > peak_cur) peak_cur = peak1.f[0];
         if (peak1.f[1] > peak_cur) peak_cur = peak1.f[1];
         if (peak1.f[2] > peak_cur) peak_cur = peak1.f[2];
         if (peak1.f[3] > peak_cur) peak_cur = peak1.f[3];
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

void
_batch_cvt24_ps_sse_vex(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *d = (int32_t*)dst;
   float *s = (float*)src;

   if (((size_t)d & MEMMASK16) != 0 || ((size_t)s & MEMMASK16) != 0)
   {
      float mul = (float)(1<<23);
      size_t i = num;
      do {
         *d++ = (int32_t)(*s++ * mul);
      } while (--i);
      return;
   }

   assert(((size_t)d & MEMMASK16) == 0);
   assert(((size_t)s & MEMMASK16) == 0);

   if (num)
   {
      __m128i *dptr = (__m128i*)d;
      __m128* sptr = (__m128*)s;
      size_t i, step;

      step = 6*sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m128 xmm0, xmm1, xmm2, xmm3, xmm4, xmm5;
         __m128 mul = _mm_set1_ps((float)(1<<23));

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xmm0 = _mm_mul_ps(mul, _mm_load_ps((const float*)sptr++));
            xmm1 = _mm_mul_ps(mul, _mm_load_ps((const float*)sptr++));
            xmm2 = _mm_mul_ps(mul, _mm_load_ps((const float*)sptr++));
            xmm3 = _mm_mul_ps(mul, _mm_load_ps((const float*)sptr++));
            xmm4 = _mm_mul_ps(mul, _mm_load_ps((const float*)sptr++));
            xmm5 = _mm_mul_ps(mul, _mm_load_ps((const float*)sptr++));

            _mm_store_si128(dptr++, _mm_cvtps_epi32(xmm0));
            _mm_store_si128(dptr++, _mm_cvtps_epi32(xmm1));
            _mm_store_si128(dptr++, _mm_cvtps_epi32(xmm2));
            _mm_store_si128(dptr++, _mm_cvtps_epi32(xmm3));
            _mm_store_si128(dptr++, _mm_cvtps_epi32(xmm4));
            _mm_store_si128(dptr++, _mm_cvtps_epi32(xmm5));
         }
         while(--i);
      }

      if (num)
      {
         float mul = (float)(1<<23);
         i = num;
         do {
            *d++ = (int32_t)(*s++ * mul);
         } while (--i);
      }
   }
}

void
_batch_cvt24_ps24_sse_vex(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *d = (int32_t*)dst;
   float *s = (float*)src;
   size_t i, step;
   size_t dtmp, stmp;

   if (!num) return;

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if ((dtmp || stmp) && dtmp != stmp)  /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      i = num;
      do {
         *d++ += (int32_t)*s++;
      }
      while (--i);
      return;
   }

   /* work towards a 16-byte aligned d (and hence 16-byte aligned sptr) */
   if (dtmp && num)
   {
      i = (MEMALIGN16 - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ += (int32_t)*s++;
         } while(--i);
      }
   }

   if (num)
   {
      __m128i *dptr = (__m128i*)d;
      __m128* sptr = (__m128*)s;

      step = 6*sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m128i xmm2i, xmm3i, xmm4i, xmm5i, xmm6i, xmm7i;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xmm2i = _mm_cvtps_epi32(_mm_load_ps((const float*)sptr++));
            xmm3i = _mm_cvtps_epi32(_mm_load_ps((const float*)sptr++));
            xmm4i = _mm_cvtps_epi32(_mm_load_ps((const float*)sptr++));
            xmm5i = _mm_cvtps_epi32(_mm_load_ps((const float*)sptr++));
            xmm6i = _mm_cvtps_epi32(_mm_load_ps((const float*)sptr++));
            xmm7i = _mm_cvtps_epi32(_mm_load_ps((const float*)sptr++));

            _mm_store_si128(dptr++, xmm2i);
            _mm_store_si128(dptr++, xmm3i);
            _mm_store_si128(dptr++, xmm4i);
            _mm_store_si128(dptr++, xmm5i);
            _mm_store_si128(dptr++, xmm6i);
            _mm_store_si128(dptr++, xmm7i);
         }
         while(--i);
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

void
_batch_cvtps24_24_sse_vex(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *s = (int32_t*)src;
   float *d = (float*)dst;
   size_t i, step;
   size_t dtmp, stmp;

   assert(s != 0);
   assert(d != 0);

   if (!num) return;

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if ((dtmp || stmp) && dtmp != stmp)  /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      i = num;
      do {
         *d++ += (float)*s++;
      }
      while (--i);
      return;
   }

   /* work towards a 16-byte aligned d (and hence 16-byte aligned sptr) */
   if (dtmp && num)
   {
      i = (MEMALIGN16 - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ += (float)*s++;
         } while(--i);
      }
   }

   if (num)
   {
      __m128i* sptr = (__m128i*)s;
      __m128 *dptr = (__m128*)d;

      step = 6*sizeof(__m128i)/sizeof(int32_t);

      i = num/step;
      if (i)
      {
         __m128 xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xmm2 = _mm_cvtepi32_ps(_mm_load_si128(sptr++));
            xmm3 = _mm_cvtepi32_ps(_mm_load_si128(sptr++));
            xmm4 = _mm_cvtepi32_ps(_mm_load_si128(sptr++));
            xmm5 = _mm_cvtepi32_ps(_mm_load_si128(sptr++));
            xmm6 = _mm_cvtepi32_ps(_mm_load_si128(sptr++));
            xmm7 = _mm_cvtepi32_ps(_mm_load_si128(sptr++));

            _mm_store_ps((float*)dptr++, xmm2);
            _mm_store_ps((float*)dptr++, xmm3);
            _mm_store_ps((float*)dptr++, xmm4);
            _mm_store_ps((float*)dptr++, xmm5);
            _mm_store_ps((float*)dptr++, xmm6);
            _mm_store_ps((float*)dptr++, xmm7);
         }
         while(--i);
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

static void
_batch_iadd_sse_vex(int32_ptr dst, const_int32_ptr src, size_t num)
{
   int32_ptr d = (int32_ptr)dst;
   int32_ptr s = (int32_ptr)src;
   size_t i, step, dtmp, stmp;

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if ((dtmp || stmp) && dtmp != stmp)  /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      i = num;
      do {
         *d++ += *s++;
      } while (--i);
      return;
   }

   /* work towards a 16-byte aligned d (and hence 16-byte aligned sptr) */
   if (dtmp && num)
   {
      i = (MEMALIGN16 - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do
         {
            *d++ += *s++;
         } while(--i);
      }
   }

   step = 2*sizeof(__m128i)/sizeof(int32_t);

   i = num/step;
   if (i)
   {
      __m128i *sptr = (__m128i *)s;
      __m128i *dptr = (__m128i *)d;
      __m128i xmm0i, xmm4i;

      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         xmm0i = _mm_load_si128(sptr++);
         xmm4i = _mm_load_si128(sptr++);

         xmm0i = _mm_add_epi32(xmm0i, _mm_load_si128(dptr+0));
         xmm4i = _mm_add_epi32(xmm4i, _mm_load_si128(dptr+1));

         _mm_store_si128(dptr++, xmm0i);
         _mm_store_si128(dptr++, xmm4i);
      }
      while(--i);
   }

   if (num)
   {
      i = num;
      do {
         *d++ += *s++;
      } while(--i);
   }
}

void
_batch_imadd_sse_vex(int32_ptr dst, const_int32_ptr src, size_t num, float v, float vstep)
{
   int32_ptr d = (int32_ptr)dst;
   int32_ptr s = (int32_ptr)src;
   size_t i, step, dtmp, stmp;

   if (!num || (v <= LEVEL_128DB && vstep <= LEVEL_128DB)) return;
   if (fabsf(v - 1.0f) < LEVEL_96DB && vstep <=  LEVEL_96DB) {
      _batch_iadd_sse_vex(dst, src, num);
      return;
   }

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if ((dtmp || stmp) && dtmp != stmp)	/* improperly aligned,            */
   {					/* let the compiler figure it out */
      i = num;
      do
      {
         *d++ += (int32_t)((float)*s++ * v);
         v += vstep;
      }
      while (--i);
      return;
   }

   /* work towards a 16-byte aligned d (and hence 16-byte aligned sptr) */
   if (dtmp && num)
   {
      i = (MEMALIGN16 - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do
         {
            *d++ += (int32_t)((float)*s++ * v);
            v += vstep;
         } while(--i);
      }
   }

   step = 2*sizeof(__m128i)/sizeof(int32_t);

   i = num/step;
   if (i)
   {
      __m128i *sptr = (__m128i *)s;
      __m128i *dptr = (__m128i *)d;
      __m128i xmm0i, xmm3i, xmm4i, xmm7i;
      __m128 xmm1, xmm5;

      vstep *= step;				/* 8 samples at a time */
      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         __m128 tv = _mm_set1_ps(v);

         xmm1 = _mm_cvtepi32_ps(_mm_load_si128(sptr++));
         xmm5 = _mm_cvtepi32_ps(_mm_load_si128(sptr++));

         xmm1 = _mm_mul_ps(xmm1, tv);
         xmm5 = _mm_mul_ps(xmm5, tv);

         xmm3i = _mm_cvtps_epi32(xmm1);
         xmm7i = _mm_cvtps_epi32(xmm5);

         xmm0i = _mm_add_epi32(xmm3i, _mm_load_si128(dptr+0));
         xmm4i = _mm_add_epi32(xmm7i, _mm_load_si128(dptr+1));

         v += vstep;

         _mm_store_si128(dptr++, xmm0i);
         _mm_store_si128(dptr++, xmm4i);
      }
      while(--i);
   }

   if (num)
   {
      vstep /= step;
      i = num;
      do {
         *d++ += (int32_t)((float)*s++ * v);
         v += vstep;
      } while(--i);
   }
}

void
_batch_cvt24_16_sse_vex(void_ptr dst, const_void_ptr src, size_t num)
{
   int16_t *s = (int16_t *)src;
   int32_t *d = (int32_t*)dst;
   size_t i, step;
   size_t tmp;

   if (!num) return;

   /*
    * work towards 16-byte aligned d
    */
   tmp = (size_t)d & MEMMASK16;
   if (tmp && num)
   {
      i = (MEMALIGN16 - tmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ = *s++ << 8;
         } while(--i);
      }
   }

#if 0
   step = 2*sizeof(__m128i)/sizeof(int16_t);

   tmp = (size_t)s & MEMMASK16;
   i = num/step;
   num -= i*step;
   if (i)
   {
      __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i zero = _mm_setzero_si128();
      __m128i *dptr = (__m128i *)d;
      __m128i *sptr = (__m128i *)s;

      do {
         if (tmp) {
            xmm0 = _mm_loadu_si128(sptr++);
            xmm4 = _mm_loadu_si128(sptr++);
         } else {
            xmm0 = _mm_load_si128(sptr++);
            xmm4 = _mm_load_si128(sptr++);
         }

         xmm1 = _mm_unpacklo_epi16(zero, xmm0);
         xmm3 = _mm_unpackhi_epi16(zero, xmm0);
         xmm5 = _mm_unpacklo_epi16(zero, xmm4);
         xmm7 = _mm_unpackhi_epi16(zero, xmm4);

         s += step;
         d += step;

         xmm0 = _mm_srai_epi32(xmm1, 8);
         xmm2 = _mm_srai_epi32(xmm3, 8);
         xmm4 = _mm_srai_epi32(xmm5, 8);
         xmm6 = _mm_srai_epi32(xmm7, 8);

         _mm_store_si128(dptr++, xmm0);
         _mm_store_si128(dptr++, xmm2);
         _mm_store_si128(dptr++, xmm4);
         _mm_store_si128(dptr++, xmm6);
      }
      while (--i);
   }
#else
   step = 2*sizeof(__m128i)/sizeof(int16_t);

   i = num/step;
   if (i)
   {
      __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5;
      __m128i zero = _mm_setzero_si128();
      __m128i *dptr = (__m128i *)d;
      __m128i *sptr = (__m128i *)s;

      tmp = (size_t)s & MEMMASK16;
      num -= i*step;

      s += i*step;
      d += i*step;
      do
      {
         if (tmp) {
            xmm0 = _mm_loadu_si128(sptr++);
            xmm1 = _mm_loadu_si128(sptr++);
         } else {
            xmm0 = _mm_load_si128(sptr++);
            xmm1 = _mm_load_si128(sptr++);
         }

         xmm2 = _mm_unpacklo_epi16(zero, xmm0);
         xmm3 = _mm_unpackhi_epi16(zero, xmm0);
         xmm4 = _mm_unpacklo_epi16(zero, xmm1);
         xmm5 = _mm_unpackhi_epi16(zero, xmm1);

         _mm_store_si128(dptr++, _mm_srai_epi32(xmm2, 8));
         _mm_store_si128(dptr++, _mm_srai_epi32(xmm3, 8));
         _mm_store_si128(dptr++, _mm_srai_epi32(xmm4, 8));
         _mm_store_si128(dptr++, _mm_srai_epi32(xmm5, 8));
      }
      while (--i);
   }
#endif

   if (num)
   {
      i = num;
      do {
         *d++ = *s++ << 8;
      } while (--i);
   }
}

void
_batch_cvt16_24_sse_vex(void_ptr dst, const_void_ptr src, size_t num)
{
   size_t i, step;
   int32_t* s = (int32_t*)src;
   int16_t* d = (int16_t*)dst;
   size_t tmp;

   if (!num) return;

   _batch_dither_cpu(s, 2, num);

   /*
    * work towards 16-byte aligned sptr
    */
   tmp = (size_t)s & MEMMASK16;
   if (tmp && num)
   {
      i = (MEMALIGN16 - tmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ = *s++ >> 8;
         } while(--i);
      }
   }

   assert(((size_t)s & MEMMASK16) == 0);
   tmp = (size_t)d & MEMMASK16;

   step = 4*sizeof(__m128i)/sizeof(int32_t);

   i = num/step;
   if (i)
   {
      __m128i xmm2, xmm3, xmm6, xmm7;
      __m128i *dptr = (__m128i *)d;
      __m128i *sptr = (__m128i *)s;

      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         xmm2 = _mm_srai_epi32(_mm_load_si128(sptr++), 8);
         xmm3 = _mm_srai_epi32(_mm_load_si128(sptr++), 8);
         xmm6 = _mm_srai_epi32(_mm_load_si128(sptr++), 8);
         xmm7 = _mm_srai_epi32(_mm_load_si128(sptr++), 8);

         if (tmp) {
            _mm_storeu_si128(dptr++, _mm_packs_epi32(xmm2, xmm3));
            _mm_storeu_si128(dptr++, _mm_packs_epi32(xmm6, xmm7));
         } else {
            _mm_store_si128(dptr++, _mm_packs_epi32(xmm2, xmm3));
            _mm_store_si128(dptr++, _mm_packs_epi32(xmm6, xmm7));
         }
      }
      while (--i);
   }

   if (num)
   {
      i = num;
      do {
         *d++ = *s++ >> 8;
      } while (--i);
   }
}

void
_batch_cvt16_intl_24_sse_vex(void_ptr dst, const_int32_ptrptr src,
                                size_t offset, unsigned int tracks,
                                size_t num)
{
   size_t i, step;
   int16_t *d = (int16_t*)dst;
   int32_t *s1, *s2;
   size_t t, tmp;

   if (!num) return;

   for (t=0; t<tracks; t++)
   {
      int32_t *s = (int32_t *)src[t] + offset;
      _batch_dither_cpu(s, 2, num);
   }

   if (tracks != 2)
   {
      for (t=0; t<tracks; t++)
      {
         int32_t *s = (int32_t *)src[t] + offset;
         int16_t *d = (int16_t *)dst + t;
         size_t i = num;

         do
         {
            *d = *s++ >> 8;
            d += tracks;
         }
         while (--i);
      }
      return;
   }

   s1 = (int32_t *)src[0] + offset;
   s2 = (int32_t *)src[1] + offset;

   step = 2*sizeof(__m128i)/sizeof(int32_t);

   /*
    * work towards 16-byte aligned sptr
    */
   tmp = (size_t)s1 & MEMMASK16;
   assert(tmp == ((size_t)s2 & MEMMASK16));

   i = num/step;
   if (tmp && i)
   {
      i = (MEMALIGN16 - tmp)/sizeof(int32_t);
      num -= i;
      do
      {
         *d++ = *s1++ >> 8;
         *d++ = *s2++ >> 8;
      }
      while (--i);
   }

   i = num/step;
   if (i)
   {
      __m128i mask, xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i *sptr1 = (__m128i*)s1;
      __m128i *sptr2 = (__m128i*)s2;
      __m128i *dptr = (__m128i *)d;


      mask = _mm_set_epi32(0x00FFFF00, 0x00FFFF00, 0x00FFFF00, 0x00FFFF00);
      tmp = (size_t)d & MEMMASK16;

      num -= i*step;
      s1 += i*step;
      s2 += i*step;
      s2 += 2*i*step;
      do
      {
         xmm2 = _mm_and_si128(mask, _mm_load_si128(sptr1++));
         xmm3 = _mm_and_si128(mask, _mm_load_si128(sptr2++));
         xmm6 = _mm_and_si128(mask, _mm_load_si128(sptr1++));
         xmm7 = _mm_and_si128(mask, _mm_load_si128(sptr2++));

         xmm0 = _mm_srli_epi32(xmm2, 8);
         xmm1 = _mm_slli_epi32(xmm3, 8);
         xmm4 = _mm_srli_epi32(xmm6, 8);
         xmm5 = _mm_slli_epi32(xmm7, 8);

         if (tmp) {
            _mm_storeu_si128(dptr++, _mm_or_si128(xmm1, xmm0));
            _mm_storeu_si128(dptr++, _mm_or_si128(xmm5, xmm4));
         } else {
            _mm_store_si128(dptr++, _mm_or_si128(xmm1, xmm0));
            _mm_store_si128(dptr++, _mm_or_si128(xmm5, xmm4));
         }
      }
      while (--i);
   }

   if (num)
   {
      i = num;
      do
      {
         *d++ = *s1++ >> 8;
         *d++ = *s2++ >> 8;
      }
      while (--i);
   }
}

void
_batch_ema_iir_float_sse_vex(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float a1)
{
   if (num)
   {
      float32_ptr s = (float32_ptr)sptr;
      size_t i = num;
      float smp;

      smp = *hist;
      do
      {
         smp += a1*(*s++ - smp);
         *d++ = smp;
      }
      while (--i);
      *hist = smp;
   }
}

void
_batch_freqfilter_sse_vex(int32_ptr dptr, const_int32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   const_int32_ptr s = sptr;

   if (num)
   {
      __m128 c, h, mk;
      float *cptr, *hist;
      int stages;

      cptr = filter->coeff;
      hist = filter->freqfilter->history[t];
      stages = filter->no_stages;
      if (!stages) stages++;

      if (filter->state == AAX_BESSEL) {
         mk = _mm_set_ss(filter->k * (filter->high_gain - filter->low_gain));
      } else {
         mk = _mm_set_ss(filter->k * filter->high_gain);
      }

      do
      {
         int32_ptr d = dptr;
         size_t i = num;

//       c = _mm_set_ps(cptr[3], cptr[1], cptr[2], cptr[0]);
         c = _mm_load_ps(cptr);
         c = _mm_shuffle_ps(c, c, _MM_SHUFFLE(3,1,2,0));

//       h = _mm_set_ps(hist[1], hist[1], hist[0], hist[0]);
         h = _mm_loadl_pi(_mm_setzero_ps(), (__m64*)hist);
         h = _mm_shuffle_ps(h, h, _MM_SHUFFLE(1,1,0,0));

         do
         {
            __m128 pz, smp, nsmp, tmp;

            smp = _mm_cvtepi32_ps((__m128i)_mm_load_ss((const float*)s));

            // pz = { c[3]*h1, -c[1]*h1, c[2]*h0, -c[0]*h0 };
            pz = _mm_mul_ps(c, h); // poles and zeros

            // smp = *s++ * k;
            smp = _mm_mul_ss(smp, mk);

            // tmp[0] = -c[0]*h0 + -c[1]*h1;
            tmp = _mm_add_ps(pz, _mm_shuffle_ps(pz, pz, _MM_SHUFFLE(1,3,0,2)));
            s++;

            // nsmp = smp - h0*c[0] - h1*c[1];
            nsmp = _mm_add_ss(smp, tmp);

            // h1 = h0, h0 = smp: h = { h0, h0, smp, smp };
            h = _mm_shuffle_ps(nsmp, h, _MM_SHUFFLE(0,0,0,0));

            // tmp[0] = -c[0]*h0 + -c[1]*h1 + c[2]*h0 + c[3]*h1;
            tmp = _mm_add_ps(tmp, _mm_shuffle_ps(tmp, tmp, _MM_SHUFFLE(0,1,2,3)));

            // smp = smp - h0*c[0] - h1*c[1] + h0*c[2] + h1*c[3];
            smp = _mm_add_ss(smp, tmp);
            _mm_store_ss((float*)d++, (__m128)_mm_cvtps_epi32(smp));
         }
         while (--i);

         _mm_storel_pi((__m64*)hist, h);

         hist += 2;
         cptr += 4;
         mk = _mm_set_ss(1.0f);
         s = dptr;
      }
      while (--stages);
   }
}

// https://en.wikipedia.org/wiki/Digital_biquad_filter#Direct_form_1
// z[n] = k*x[n] + c0*x[n-1] + c1*x[n-2] + -c2*z[n-1] + -c3*z[n-2];
// note: c2 and c3 are negated by the frequency filter code.
void
_batch_freqfilter_float_sse_vex(float32_ptr dptr, const_float32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   const_float32_ptr s = sptr;

   if (num)
   {
      float k, *cptr, *hist;
      int stage;

      if (filter->state == AAX_BESSEL) {
         k = filter->k * (filter->high_gain - filter->low_gain);
      } else {
         k = filter->k * filter->high_gain;
      }

      if (fabsf(k-1.0f) < LEVEL_96DB)
      {
         memcpy(dptr, sptr, num*sizeof(float));
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

      assert(((size_t)cptr & MEMMASK16) == 0);

      do
      {
         float32_ptr d = dptr;
         float h0, h1;
         size_t i = num;

         h0 = hist[0];
         h1 = hist[1];

         if (filter->state == AAX_BUTTERWORTH)
         {
            __m128 cp01 = _mm_load_ps(cptr);
            __m128 cp23 = _mm_movehl_ps(cp01, cp01);
            __m128 hist = _mm_set_ps(0.0f, 0.0f, h1, h0);

            do
            {
               __m128 v23, v01 = _mm_mul_ps(hist, cp01);
               __m128 d4 = _mm_set_ss(*s++ * k);
               __m128 shuf = _mm_movehdup_ps(v01);
               __m128 sums = _mm_add_ps(v01, d4);

               v23 = _mm_mul_ps(hist, cp23);
               d4 = _mm_add_ps(sums, shuf);

               shuf = _mm_movehdup_ps(v23);
               sums = _mm_add_ps(v23, d4);

               hist = _mm_moveldup_ps(hist);

               sums = _mm_add_ps(sums, shuf);
               *d++ = _mm_cvtss_f32(sums);

               hist = _mm_move_ss(hist, d4);
            }
            while (--i);

            h0 = hist[0];
            h1 = hist[1];
         }
         else
         {
            __m128 cp01 = _mm_load_ps(cptr);
            __m128 hist = _mm_set_ps(0.0f, 0.0f, h1, h0);

            do
            {
#if 0
               // d[0] = (*s++ * k) + d[-1]*cptr[0] + d[-2]*cptr[1]; d++;
               __m128 samp;

               *d = (*s++ * k);
               samp = _mm_mul_ps(hist, cp01);
               *d += hsum_half_ps_sse_vex(samp);
               hist = _mm_shuffle_ps(hist, hist, _MM_SHUFFLE(2, 1, 0, 3));
               hist = _mm_move_ss(hist, _mm_load_ss(d++));
#else
               __m128 v01 = _mm_mul_ps(hist, cp01);
               __m128 d4 = _mm_set_ss(*s++ * k);
               __m128 shuf = _mm_movehdup_ps(v01);
               __m128 sums = _mm_add_ps(v01, d4);

               hist = _mm_moveldup_ps(hist);

               d4 = _mm_add_ps(sums, shuf);
               hist = _mm_move_ss(hist, d4);
               *d++ = _mm_cvtss_f32(d4);
#endif
            }
            while (--i);

            d -= 2;
            h0 = *d++;
            h1 = *d;
         }

         *hist++ = h0;
         *hist++ = h1;
         cptr += 4;
         k = 1.0f;
         s = dptr;
      }
      while (--stage);
   }
}


#if !RB_FLOAT_DATA
static inline void
_aaxBufResampleDecimate_sse_vex(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   int32_t samp, dsamp;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor >= 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   dptr += dmin;

   samp = *sptr++;              // n+(step-1)
   dsamp = *sptr - samp;        // (n+1) - n


   i=dmax-dmin;
   if (i)
   {
      do
      {
         size_t step;

         *dptr++ = samp + (int32_t)(dsamp * smu);

         smu += freq_factor;
         step = (size_t)floorf(smu);

         smu -= step;
         sptr += step-1;
         samp = *sptr++;
         dsamp = *sptr - samp;
      }
      while (--i);
   }
}

static inline void
_aaxBufResampleLinear_sse_vex(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   int32_t samp, dsamp;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor < 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   dptr += dmin;

   samp = *sptr++;		// n
   dsamp = *sptr - samp;	// (n+1) - n

   i = dmax-dmin;
   if (i)
   {
      do
      {
         *dptr++ = samp + (int32_t)(dsamp * smu);

         smu += freq_factor;
         if (smu >= 1.0)
         {
            smu -= 1.0;
            samp = *sptr++;
            dsamp = *sptr - samp;
         }
      }
      while (--i);
   }
}

static inline void
_aaxBufResampleCubic_sse_vex(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float y0, y1, y2, y3, a0, a1, a2;
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

   dptr += dmin;

   y0 = (float)*sptr++;
   y1 = (float)*sptr++;
   y2 = (float)*sptr++;
   y3 = (float)*sptr++;

   a0 = y3 - y2 - y0 + y1;
   a1 = y0 - y1 - a0;
   a2 = y2 - y0;

   i = dmax-dmin;
   if (i)
   {
      do
      {
         float smu2, ftmp;

         smu2 = smu*smu;
         ftmp = (a0*smu*smu2 + a1*smu2 + a2*smu + y1);
         *dptr++ = (int32_t)ftmp;

         smu += freq_factor;
         if (smu >= 1.0)
         {
            smu--;
            a0 += y0;
            y0 = y1;
            y1 = y2;
            y2 = y3;
            y3 = (float)*sptr++;
            a0 = -a0 + y3;			/* a0 = y3 - y2 - y0 + y1; */
            a1 = y0 - y1 - a0;
            a2 = y2 - y0;
         }
      }
      while (--i);
   }
}

void
_batch_resample_sse_vex(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_sse_vex(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_sse_vex(d, s, dmin, dmax, smu, fact);
   }
   else if (fact >= 1.0f) {
      _aaxBufResampleDecimate_sse_vex(d, s, dmin, dmax, smu, fact);
   } else {
//    _aaxBufResampleNearest_sse_vex(d, s, dmin, dmax, smu, fact);
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}
#else

static inline void
_aaxBufResampleDecimate_float_sse_vex(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
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

   samp = *s++;			// n+(step-1)
   dsamp = *s - samp;		// (n+1) - n

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
_aaxBufResampleLinear_float_sse_vex(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
         dout = _mm_add_ss(dout, _mm_mul_ss(dsamp, tau));

         if (smu >= 1.0)
         {
            samp = nsamp;
            nsamp = _mm_load_ss(sptr++);

            smu -= 1.0;

            dsamp = _mm_sub_ss(nsamp, samp);
         }
         _mm_store_ss(dptr++, dout);
      }
      while (--i);
   }
}


static inline void
_aaxBufResampleCubic_float_sse_vex(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float32_ptr sptr = (float32_ptr)s;
   float32_ptr dptr = d;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

#if 1
   float y0, y1, y2, y3, a0, a1, a2;

   dptr += dmin;

   y0 = *sptr++;
   y1 = *sptr++;
   y2 = *sptr++;
   y3 = *sptr++;

   a0 = y3 - y2 - y0 + y1;
   a1 = y0 - y1 - a0;
   a2 = y2 - y0;

   i = dmax-dmin;
   if (i)
   {
      do
      {
         float smu2, ftmp;

         smu2 = smu*smu;
         ftmp = (a0*smu*smu2 + a1*smu2 + a2*smu + y1);
         *dptr++ = ftmp;

         smu += freq_factor;
         if (smu >= 1.0)
         {
            smu--;
            a0 += y0;
            y0 = y1;
            y1 = y2;
            y2 = y3;
            y3 = *sptr++;
            a0 = -a0 + y3;                      /* a0 = y3 - y2 - y0 + y1; */
            a1 = y0 - y1 - a0;
            a2 = y2 - y0;
         }
      }
      while (--i);
   }
#else
   dptr += dmin;
   i = dmax-dmin;
   if (i > 4)
   {
      /*
       * http://paulbourke.net/miscellaneous/interpolation/
       *
       * -- Catmull-Rom splines --
       * a0 = -0.5*y0 + 1.5*y1 - 1.5*y2 + 0.5*y3;
       * a1 =      y0 - 2.5*y1 +   2*y2 - 0.5*y3;
       * a2 = -0.5*y0          + 0.5*y2;
       * a3 =               y1;
       */
      const __m128 y0m = _mm_set_ps(-0.5f, 1.5f,-1.5f, 0.5f);
      const __m128 y1m = _mm_set_ps( 1.0f,-2.5f, 2.0f,-0.5f);
      const __m128 y2m = _mm_set_ps(-0.5f, 0.0f, 0.5f, 0.0f);
      const __m128 y3m = _mm_set_ps( 0.0f, 1.0f, 0.0f, 0.0f);
      __m128 a0, a1, a2, a3;
      __m128 y0123;

      if (((size_t)sptr & MEMMASK16) == 0) {
         y0123 = _mm_load_ps((float*)sptr);
      } else {
         y0123 = _mm_loadu_ps((float*)sptr);
      }
      sptr += 4;

      a0 = _mm_mul_ps(y0123, y0m);
      a1 = _mm_mul_ps(y0123, y1m);
      a2 = _mm_mul_ps(y0123, y2m);
      a3 = _mm_mul_ps(y0123, y3m);

      do
      {
//       *d++ = a0*smu*smu*smu + a1*smu*smu + a2*smu + y1
         __m128 xsmu = _mm_set1_ps(smu);
         __m128 xsmu2 = _mm_mul_ps(xsmu, xsmu);
         __m128 xsmu3, xtmp1, xtmp2, v;
         __m128 shuf, sums;

         xtmp2 = _mm_add_ps(_mm_mul_ps(a2, xsmu), a3);

         xsmu3 = _mm_mul_ps(xsmu2, xsmu);
         xtmp1 = _mm_add_ps(_mm_mul_ps(a0, xsmu3), _mm_mul_ps(a1, xsmu2));

         smu += freq_factor;

         // horizontal sum and store
         v = _mm_add_ps(xtmp1, xtmp2);
         shuf = _mm_movehdup_ps(v);
         sums = _mm_add_ps(v, shuf);
         shuf = _mm_movehl_ps(shuf, sums);
         _mm_store_ss(d++, _mm_add_ss(sums, shuf));

         if (smu >= 1.0)
         {
            y0123 =_mm_castsi128_ps(_mm_slli_si128(_mm_castps_si128(y0123),4));
            y0123 = _mm_move_ss(y0123, _mm_load_ss((float*)sptr++));

            smu--;

            a0 = _mm_mul_ps(y0123, y0m);
            a1 = _mm_mul_ps(y0123, y1m);
            a2 = _mm_mul_ps(y0123, y2m);
            a3 = _mm_mul_ps(y0123, y3m);
         }
      }
      while (--i);
   }
#endif
}

void
_batch_resample_float_sse_vex(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_float_sse_vex(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_float_sse_vex(d, s, dmin, dmax, smu, fact);
   }
   else if (fact >= 1.0f) {
      _aaxBufResampleDecimate_float_sse_vex(d, s, dmin, dmax, smu, fact);
   } else {
//    _aaxBufResampleNearest_float_sse_vex(d, s, dmin, dmax, smu, fact);
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}
#endif // RB_FLOAT_DATA

#else
typedef int make_iso_compilers_happy;
#endif /* AVX */

