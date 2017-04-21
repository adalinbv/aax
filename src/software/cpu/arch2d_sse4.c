/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#include <software/rbuf_int.h>
#include "arch2d_simd.h"

#ifdef __SSE4__

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
            int32_t samp = _MINMAX(*d, -8388607, 8388607);
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

      xmin = _mm_set1_epi32(-8388607);
      xmax = _mm_set1_epi32(8388607);

      num -= i*step;
      d += i*step;

      do
      {
         xmm0i = _mm_load_si128(sptr+0);
         xmm1i = _mm_load_si128(sptr+1);
         xmm2i = _mm_load_si128(sptr+2);
         xmm3i = _mm_load_si128(sptr+3);

         xmm0i =  _mm_min_epi32(_mm_max_epi32(xmm0i, xmin), xmax);
         xmm1i =  _mm_min_epi32(_mm_max_epi32(xmm1i, xmin), xmax);
         xmm2i =  _mm_min_epi32(_mm_max_epi32(xmm2i, xmin), xmax);
         xmm3i =  _mm_min_epi32(_mm_max_epi32(xmm3i, xmin), xmax);

         _mm_store_si128(dptr++, xmm0i);
         _mm_store_si128(dptr++, xmm4i);
         _mm_store_si128(dptr++, xmm0i);
         _mm_store_si128(dptr++, xmm4i);
      }
      while(--i);
   }

   if (num)
   {
      i = num;
      do
      {
         int32_t samp = _MINMAX(*d, -8388607, 8388607);
         *d++ = samp;
      }
      while (--i);
   }
}

#endif /* SSE4 */

