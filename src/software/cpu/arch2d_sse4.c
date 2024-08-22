/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <software/rbuf_int.h>
#include "arch2d_simd.h"

#ifdef __SSE4_1__

#define ROUNDING	(_MM_FROUND_TO_NEAREST_INT|_MM_FROUND_NO_EXC)

static inline __m128
_mm_abs_ps(__m128 x) {
   return _mm_andnot_ps(_mm_set1_ps(-0.0f), x);
}

static inline __m128
copysign_sse2(__m128 x, __m128 y)
{
    __m128 sign_mask = _mm_set1_ps(-0.0f); // This is 0x80000000 in binary
    __m128 y_sign = _mm_and_ps(y, sign_mask);
    __m128 abs_x = _mm_andnot_ps(sign_mask, x);
    return _mm_or_ps(abs_x, y_sign);
}

void
_batch_wavefold_sse4(float32_ptr d, const_float32_ptr s, size_t num, float threshold)
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
      __m128 *dptr = (__m128*)d;
      __m128* sptr = (__m128*)s;

      step = sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         static const float max = (float)(1 << 23);
         __m128 xthresh, x2thresh;

         threshold = max*threshold;

         xthresh = _mm_set1_ps(threshold);
         x2thresh = _mm_set1_ps(2.0f*threshold);

         num -= i*step;
         d += i*step;
         s += i*step;
         do
         {
             __m128 xsamp = _mm_load_ps((const float*)sptr++);
             __m128 xasamp = _mm_abs_ps(xsamp);
             __m128 xthres2 = copysign_sse2(x2thresh, xsamp);
             __m128 xmask = _mm_cmpgt_ps(xasamp, xthresh);

             xasamp = _mm_sub_ps(xthres2, xasamp);
             xsamp = _mm_blendv_ps(xsamp, xasamp, xmask);

             _mm_store_ps((float*)dptr++, xsamp);
         } while(--i);

         if (num)
         {
            i = num;
            do
            {
               float samp = *s++;
               float asamp = fabsf(samp);
               if (asamp > threshold)
               {
                  float thresh2 = copysignf(2.0f*threshold, samp);
                  samp = thresh2 - asamp;
               }
               *d++ = samp;
            } while(--i);
         }
      }
   }
}

void
_batch_saturate24_sse4(void *data, size_t num)
{
   int32_t *d = (int32_t*)data;
   size_t i, step;
   size_t dtmp;

   dtmp = (size_t)data & MEMMASK16;
   if (dtmp && num)
   {
      i = (MEMALIGN16 - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            int32_t samp = _MINMAX(*d, -AAX_PEAK_MAX, AAX_PEAK_MAX);
            *d++ = samp;
         } while(--i);
      }
   }

   step = 4*sizeof(__m128)/sizeof(int32_t);
   i = num/step;
   if (i)
   {
      __m128i *dptr = (__m128i*)d;
      __m128i xmm0i, xmm1i, xmm2i, xmm3i;
      __m128i xmin, xmax;

      xmin = _mm_set1_epi32(-AAX_PEAK_MAX);
      xmax = _mm_set1_epi32(AAX_PEAK_MAX);

      num -= i*step;
      d += i*step;

      do
      {
         xmm0i = _mm_load_si128(dptr+0);
         xmm1i = _mm_load_si128(dptr+1);
         xmm2i = _mm_load_si128(dptr+2);
         xmm3i = _mm_load_si128(dptr+3);

         xmm0i =  _mm_min_epi32(_mm_max_epi32(xmm0i, xmin), xmax);
         xmm1i =  _mm_min_epi32(_mm_max_epi32(xmm1i, xmin), xmax);
         xmm2i =  _mm_min_epi32(_mm_max_epi32(xmm2i, xmin), xmax);
         xmm3i =  _mm_min_epi32(_mm_max_epi32(xmm3i, xmin), xmax);

         _mm_store_si128(dptr++, xmm0i);
         _mm_store_si128(dptr++, xmm1i);
         _mm_store_si128(dptr++, xmm2i);
         _mm_store_si128(dptr++, xmm3i);
      }
      while(--i);
   }

   if (num)
   {
      i = num;
      do
      {
         int32_t samp = _MINMAX(*d, -AAX_PEAK_MAX, AAX_PEAK_MAX);
         *d++ = samp;
      }
      while (--i);
   }
}

#else
typedef int make_iso_compilers_happy;
#endif /* SSE4 */

