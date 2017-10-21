/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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

#else
typedef int make_iso_compilers_happy;
#endif /* SSE4 */

