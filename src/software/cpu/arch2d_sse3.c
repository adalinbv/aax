/*
 * Copyright 2005-2012 by Erik Hofman.
 * Copyright 2009-2012 by Adalin B.V.
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

#include "arch2d_simd.h"

#ifdef __SSE3__

void
_batch_imul_value_sse3(void* data, unsigned bps, unsigned int num, float f)
{
   unsigned int i = num;

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

void
_batch_fmul_value_sse3(void* data, unsigned bps, unsigned int num, float f)
{
   unsigned int i = num;

   if (num)
   {
      switch (bps)
      {
      case 4:
      {
         float *d = (float*)data;
         do {
            *d++ *= f;
         }
         while (--i);
         break;
      }
      case 8:
      {
         double *d = (double*)data;
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

void
_aaxBufResampleCubic_sse3(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   const __m128 y0m = _mm_set_ps( 0.0f, 1.0f, 0.0f, 0.0f);
   const __m128 y1m = _mm_set_ps(-1.0f, 0.0f, 1.0f, 0.0f);
   const __m128 y2m = _mm_set_ps( 2.0f,-2.0f, 1.0f,-1.0f);
   const __m128 y3m = _mm_set_ps(-1.0f, 1.0f,-1.0f, 1.0f);
   __m128 y0123, a0123, vm0, vm1, vm2, vm3;
   __m128i xmm0i;
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   unsigned int i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

   sptr += sdesamps;
   dptr += dmin;

   if (((size_t)sptr & 0xF) == 0) {
      xmm0i = _mm_load_si128((__m128i*)sptr);
   } else {
      xmm0i = _mm_loadu_si128((__m128i*)sptr);
   }
   y0123 = _mm_cvtepi32_ps(xmm0i);
   sptr += 4;

   vm0 = _mm_mul_ps(y0123, y0m);
   vm1 = _mm_mul_ps(y0123, y1m);
   vm2 = _mm_mul_ps(y0123, y2m);
   vm3 = _mm_mul_ps(y0123, y3m);
   a0123 = _mm_hadd_ps(_mm_hadd_ps(vm0, vm1), _mm_hadd_ps(vm2, vm3));

   i = dmax-dmin;
   if (i)
   {
      do
      {
         __m128 xsmu, xtmp;
         float smu2 = smu*smu;
#if 0
         ftmp = (a0*smu*smu2 + a1*smu2 + a2*smu + a3);
         *dptr++ = (int32_t)ftmp;
#else
         xsmu = _mm_set_ps(smu*smu2, smu2, smu, 1.0f);
         xtmp = _mm_mul_ps(a0123, xsmu);
         xtmp = _mm_hadd_ps(_mm_hadd_ps(xtmp, xtmp), _mm_hadd_ps(xtmp, xtmp));
         *dptr++ = _mm_cvttss_si32(xtmp);
#endif

         smu += freq_factor;
         if (smu >= 1.0)
         {
            smu--;

#if 0
            y0 = y1;
            y1 = y2;
            y2 = y3;
            y3 = (float)*sptr++;
#endif
            y0123 = _mm_castsi128_ps(_mm_slli_si128(_mm_castps_si128(y0123),
                                                    sizeof(float)));
            y0123 = _mm_cvtsi32_ss(y0123, *sptr++);

#if 0
            a0 = y3 - y2 - y0 + y1;
            a1 = y0 - y1 - a0;
            a2 = y2 - y0;
            a3 = y1;
#else
            vm0 = _mm_mul_ps(y0123, y0m);
            vm1 = _mm_mul_ps(y0123, y1m);
            vm2 = _mm_mul_ps(y0123, y2m);
            vm3 = _mm_mul_ps(y0123, y3m);
            a0123 = _mm_hadd_ps(_mm_hadd_ps(vm0, vm1), _mm_hadd_ps(vm2, vm3));
#endif
         }
      }
      while (--i);
   }
}

void
_batch_resample_sse3(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_sse3(d, s, dmin, dmax, 0, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_sse2(d, s, dmin, dmax, 0, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleSkip_sse2(d, s, dmin, dmax, 0, smu, fact);
   } else {
      _aaxBufResampleNearest_sse2(d, s, dmin, dmax, 0, smu, fact);
   }
}
#endif /* SSE3 */

