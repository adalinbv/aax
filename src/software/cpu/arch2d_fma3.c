/*
 * Copyright 2005-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
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

#include "software/rbuf_int.h"
#include "software/cpu/arch2d_simd.h"

#ifdef __FMA__

# define CACHE_ADVANCE_FMADD	 (2*16)
# define CACHE_ADVANCE_FF	 (2*32)

FN_PREALIGN void
_batch_fma3_avx(int32_ptr d, const_int32_ptr src, size_t num, float v, float vstep)
{
   __m256i *sptr = (__m256i *)src;
   __m256i *dptr = (__m256i*)d;
   __m256 tv = _mm256_set1_ps(v);
   int32_ptr s = (int32_ptr)src;
   size_t i, size, step;
   long dtmp, stmp;

   dtmp = (long)dptr & 0xF;
   stmp = (long)sptr & 0xF;
   if ((dtmp || stmp) && dtmp != stmp)
   {
      i = num;				/* improperly aligned,            */
      do				/* let the compiler figure it out */
      {
         *d++ += (int32_t)((float)*s++ * v);
         v += vstep;
      }
      while (--i);
      return;
   }

   step = 2*sizeof(__m256i)/sizeof(int32_t);

   /* work towards a 16-byte aligned dptr (and hence 16-byte aligned sptr) */
   i = num/step;
   if (dtmp && i)
   {
      i = (0x10 - dtmp)/sizeof(int32_t);
      num -= i;
      do {
         *d++ += (int32_t)((float)*s++ * v);
         v += vstep;
      } while(--i);
      dptr = (__m256i *)d;
      sptr = (__m256i *)s;
   }

   vstep *= step;				/* 8 samples at a time */
   i = size = num/step;
   if (i)
   {
      __m256i ymm0i, ymm4i;
      __m256 ymm1, ymm2, ymm5, ymm6;

      do
      {
         _mm_prefetch(((char *)sptr)+CACHE_ADVANCE_FMADD, _MM_HINT_NTA);
         ymm0i = _mm256_load_si256(sptr++);
         ymm4i = _mm256_load_si256(sptr++);
         ymm1 = _mm256_cvtepi32_ps(ymm0i);
         ymm5 = _mm256_cvtepi32_ps(ymm4i);

         ymm0i = _mm256_load_si256(dptr);
         ymm4i = _mm256_load_si256(dptr+1);
         ymm2 = _mm256_cvtepi32_ps(ymm0i);
         ymm6 = _mm256_cvtepi32_ps(ymm4i);

         v += vstep;

         ymm2 = _mm256_fmadd_ps(ymm2, ymm1, tv);
         ymm6 = _mm256_fmadd_ps(ymm6, ymm5, tv);

         ymm0i = _mm256_cvtps_epi32(ymm2);
         ymm4i = _mm256_cvtps_epi32(ymm6);

         _mm256_store_si256(dptr, ymm0i);
         _mm256_store_si256(dptr+1, ymm4i);
         dptr += 2;

         tv = _mm256_set1_ps(v);
      }
      while(--i);

   }

   i = num - size*step;
   if (i) {
      d = (int32_t *)dptr;
      s = (int32_t *)sptr;
      vstep /= step;
      do {
         *d++ += (int32_t)((float)*s++ * v);
         v += vstep;
      } while(--i);
   }
}

#endif /* __FMA__ */

