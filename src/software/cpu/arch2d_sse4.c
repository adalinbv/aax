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

void
_batch_roundps_sse4(void_ptr dst, const_void_ptr src, size_t num)
{
   float *d = (float*)dst;
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
         *d++ += (float)(int32_t)*s++;
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
      __m128 *dptr = (__m128*)d;
      __m128* sptr = (__m128*)s;

      step = 4*sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m128 xmm0, xmm1, xmm2, xmm3;
         __m128 xmm4, xmm5, xmm6, xmm7;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xmm0 = _mm_load_ps((const float*)sptr++);
            xmm1 = _mm_load_ps((const float*)sptr++);
            xmm2 = _mm_load_ps((const float*)sptr++);
            xmm3 = _mm_load_ps((const float*)sptr++);

            xmm4 = _mm_round_ps(xmm0, ROUNDING);
            xmm5 = _mm_round_ps(xmm1, ROUNDING);
            xmm6 = _mm_round_ps(xmm2, ROUNDING);
            xmm7 = _mm_round_ps(xmm3, ROUNDING);

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
            *d++ = (int32_t)*s++;
         } while (--i);
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

