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

#include <software/rbuf_int.h>
#include "arch2d_simd.h"

#ifdef __SSE3__

void
_batch_imul_value_sse3(void* data, unsigned bps, size_t num, float f)
{
   size_t i = num;

   if (num)
   {
      switch (bps)
      {
      case 1:
      {
         int8_t* d = (int8_t*)data;
         do {
            *d++ *= f;
         }
         while (--i);
         break;
      }
      case 2:
      {
         int16_t* d = (int16_t*)data;
         do {
            *d++ *= f;
         }
         while (--i);
         break;
         }
      case 4:
      {
         int32_t* d = (int32_t*)data;
         do {
            *d++ *= f;
         }
         while (--i);
         break;
      }
      default:
         break;
      }
   }
}

#if !RB_FLOAT_DATA
static inline void
_aaxBufResampleDecimate_sse3(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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

#if 0
static inline void
_aaxBufResampleNearest_sse3(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   if (freq_factor == 1.0f) {
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(int32_t));
   }
   else
   {
      int32_ptr sptr = (int32_ptr)s;
      int32_ptr dptr = d;
      size_t i;

      assert(s != 0);
      assert(d != 0);
      assert(dmin < dmax);
      assert(0.95f <= freq_factor && freq_factor <= 1.05f);
      assert(0.0f <= smu && smu < 1.0f);

      dptr += dmin;

      i = dmax-dmin;
      if (i)
      {
         do
         {
            *dptr++ = *sptr;

            smu += freq_factor;
            if (smu > 0.5f)
            {
               sptr++;
               smu -= 1.0f;
            }
         }
         while (--i);
      }
   }
}
#endif

static inline void
_aaxBufResampleLinear_sse3(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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

   samp = *sptr++;              // n
   dsamp = *sptr - samp;        // (n+1) - n

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


/** NOTE: instruction sequence is important for execution speed! */
static inline void
_aaxBufResampleCubic_sse3(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   const __m128 y0m = _mm_set_ps( 0.0f, 1.0f, 0.0f, 0.0f);
   const __m128 y1m = _mm_set_ps(-1.0f, 0.0f, 1.0f, 0.0f);
   const __m128 y2m = _mm_set_ps( 2.0f,-2.0f, 1.0f,-1.0f);
   const __m128 y3m = _mm_set_ps(-1.0f, 1.0f,-1.0f, 1.0f);
   __m128 y0123, a0123, xsmu;
   __m128 vm0, vm1, vm2, vm3;
   __m128 xtmp, xtmp1, xtmp2;
   __m128i xmm0i;
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   size_t i;
   float smu2;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

   dptr += dmin;

   if (((size_t)sptr & MEMMASK) == 0) {
      xmm0i = _mm_load_si128((__m128i*)sptr);
   } else {
      xmm0i = _mm_loadu_si128((__m128i*)sptr);
   }
   y0123 = _mm_cvtepi32_ps(xmm0i);

   /* 
    * a0 = y3 - y2 - y0 + y1;
    * a1 = y0 - y1 - a0;
    * a2 = y2 - y0;
    * a3 = y1;
    */
   vm0 = _mm_mul_ps(y0123, y0m);
   vm1 = _mm_mul_ps(y0123, y1m);
   xtmp = _mm_hadd_ps(vm0, vm1);

   sptr += 4;

   vm2 = _mm_mul_ps(y0123, y2m);
   vm3 = _mm_mul_ps(y0123, y3m);
   xtmp1 = _mm_hadd_ps(vm2, vm3);

   /* work in advance */
   smu2 = smu*smu;
   xsmu = _mm_set_ps(smu*smu*smu, smu*smu, smu, 1.0f);

   a0123 = _mm_hadd_ps(xtmp, xtmp1);
   y0123 = _mm_shuffle_ps(y0123, y0123, _MM_SHUFFLE(2, 1, 0, 0));
   i = dmax-dmin;
   if (i)
   {
      do
      {
         xtmp = _mm_mul_ps(a0123, xsmu);
         xtmp = _mm_hadd_ps(xtmp, xtmp);

         smu += freq_factor;

         xtmp = _mm_hadd_ps(xtmp, xtmp);

         if (smu >= 1.0)
         {
            y0123 = _mm_cvtsi32_ss(y0123, *sptr++);

            /* 
             * a0 = y3 - y2 - y0 + y1;
             * a1 = y0 - y1 - a0;
             * a2 = y2 - y0;
             * a3 = y1;
             */
            vm0 = _mm_mul_ps(y0123, y0m);
            vm1 = _mm_mul_ps(y0123, y1m);
            xtmp1 = _mm_hadd_ps(vm0, vm1);

            smu--;

            vm2 = _mm_mul_ps(y0123, y2m);
            vm3 = _mm_mul_ps(y0123, y3m);
            xtmp2 = _mm_hadd_ps(vm2, vm3);

            /* work in advance */
            smu2 = smu*smu;
            xsmu = _mm_set_ps(smu*smu2, smu2, smu, 1.0f);

            a0123 = _mm_hadd_ps(xtmp1, xtmp2);
            y0123 = _mm_shuffle_ps(y0123, y0123, _MM_SHUFFLE(2, 1, 0, 3));
         }

         xsmu = _mm_set_ps(smu*smu2, smu2, smu, 1.0f);

         *dptr++ = _mm_cvttss_si32(xtmp);
      }
      while (--i);
   }
}

void
_batch_resample_sse3(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_sse3(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_sse3(d, s, dmin, dmax, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleDecimate_sse3(d, s, dmin, dmax, smu, fact);
   } else {
//    _aaxBufResampleNearest_sse3(d, s, dmin, dmax, smu, fact);
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
} 
#else

static inline void
_aaxBufResampleDecimate_float_sse3(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float32_ptr sptr = (float32_ptr)s;
   float32_ptr dptr = d;
   float samp, dsamp;
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

         *dptr++ = samp + (dsamp * smu);

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

#if 0
static inline void
_aaxBufResampleNearest_float_sse3(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   if (freq_factor == 1.0f) {
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(float));
   }
   else
   {
      float32_ptr sptr = (float32_ptr)s;
      float32_ptr dptr = d;
      size_t i;

      assert(s != 0);
      assert(d != 0);
      assert(dmin < dmax);
      assert(0.95f <= freq_factor && freq_factor <= 1.05f);
      assert(0.0f <= smu && smu < 1.0f);

      dptr += dmin;

      i = dmax-dmin;
      if (i)
      {
         do
         {
            *dptr++ = *sptr;

            smu += freq_factor;
            if (smu > 0.5f)
            {
               sptr++;
               smu -= 1.0f;
            }
         }
         while (--i);
      }
   }
}
#endif

static inline void
_aaxBufResampleLinear_float_sse3(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float32_ptr sptr = (float32_ptr)s;
   float32_ptr dptr = d;
   float samp, dsamp;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor < 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   dptr += dmin;

   samp = *sptr++;              // n
   dsamp = *sptr - samp;        // (n+1) - n

   i = dmax-dmin;
   if (i)
   {
      do
      {
         *dptr++ = samp + (dsamp * smu);

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

static inline FN_PREALIGN float
hsum_ps_sse3(__m128 v) {
   __m128 shuf = _mm_movehdup_ps(v);
   __m128 sums = _mm_add_ps(v, shuf);
   shuf = _mm_movehl_ps(shuf, sums);
   sums = _mm_add_ss(sums, shuf);
   return _mm_cvtss_f32(sums);
}

static inline void
_aaxBufResampleCubic_float_sse3(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
    /* 
    * a0 = y3 - y2 - y0 + y1;
    * a1 = 2y0 - 2y1 - y3 + y2;
    * a2 = y2 - y0;
    * a3 = y1;
    */
   const __m128 y0m = _mm_set_ps(-1.0f, 1.0f,-1.0f, 1.0f);
   const __m128 y1m = _mm_set_ps( 2.0f,-2.0f, 1.0f,-1.0f);
   const __m128 y2m = _mm_set_ps(-1.0f, 0.0f, 1.0f, 0.0f);
   const __m128 y3m = _mm_set_ps( 0.0f, 1.0f, 0.0f, 0.0f);
   float32_ptr sptr = (float32_ptr)s;
   float32_ptr dptr = d;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

   dptr += dmin;
   i = dmax-dmin;
   if (i > 4)
   {
      __m128 a0, a1, a2, a3;
      __m128 xtmp1, xtmp2;
      __m128 y0123, a0123;
      float smu2 = smu*smu;

      if (((size_t)sptr & MEMMASK) == 0) {
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
         __m128 xsmu3 = _mm_set1_ps(smu*smu2);
         __m128 xsmu2 = _mm_set1_ps(smu2);
         __m128 xsmu = _mm_set1_ps(smu);

         xtmp1 = _mm_add_ps(_mm_mul_ps(a0, xsmu3), _mm_mul_ps(a1, xsmu2));
         xtmp2 = _mm_add_ps(_mm_mul_ps(a2, xsmu), a3);
         a0123 = _mm_add_ps(xtmp1, xtmp2);

         smu += freq_factor;
         *dptr++ = hsum_ps_sse3(a0123);

         if (smu >= 1.0)
         {
            y0123 =_mm_castsi128_ps(_mm_slli_si128(_mm_castps_si128(y0123),4));
            y0123 = _mm_move_ss(y0123, _mm_load_ss((float*)sptr++));

            a0 = _mm_mul_ps(y0123, y0m);
            a1 = _mm_mul_ps(y0123, y1m);
            a2 = _mm_mul_ps(y0123, y2m);
            a3 = _mm_mul_ps(y0123, y3m);

            smu--;
         }
         smu2 = smu*smu;
      }
      while (--i);
   }
}

void
_batch_resample_float_sse3(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);
   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_float_sse3(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_float_sse3(d, s, dmin, dmax, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleDecimate_float_sse3(d, s, dmin, dmax, smu, fact);
   } else {
//    _aaxBufResampleNearest_float_sse3(d, s, dmin, dmax, smu, fact);
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}
#endif // RB_FLOAT_DATA

#else
typedef int make_iso_compilers_happy;
#endif /* SSE3 */

