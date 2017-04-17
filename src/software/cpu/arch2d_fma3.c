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

#include "software/rbuf_int.h"
#include "software/cpu/arch2d_simd.h"

#ifdef __FMA__

# define CACHE_ADVANCE_FMADD	 (2*16)
# define CACHE_ADVANCE_FF	 (2*32)

FN_PREALIGN void
_batch_fma3_float_avx(float32_ptr dst, const_float32_ptr src, size_t num, float v, float vstep)
{
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   if (!num || (v <= LEVEL_128DB && vstep <= LEVEL_128DB)) return;
   if (fabsf(v - 1.0f) < LEVEL_96DB && vstep <=  LEVEL_96DB) {
      _batch_fadd_avx(dst, src, num);
      return;
   }

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if ((dtmp || stmp) && dtmp != stmp)
   {
      return _batch_fmadd_sse_vex(dst, src, num, v, vstep);
   }

   /* work towards a 16-byte aligned dptr (and hence 16-byte aligned sptr) */
   if (dtmp && num)
   {
      i = (MEMALIGN - dtmp)/sizeof(float);
      if (i <= num)
      {
         do {
            *d++ += *s++ * v;
            v += vstep;
         } while(--i);
      }
   }

   step = 8*sizeof(__m256)/sizeof(float);

   i = num/step;
   if (i)
   {
      __m256 ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
      __m256 *sptr = (__m256 *)s;
      __m256 *dptr = (__m256 *)d;

      vstep *= step;
      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         __m256 tv = _mm256_set1_ps(v);

         ymm0 = _mm256_load_ps((const float*)sptr++);
         ymm1 = _mm256_load_ps((const float*)sptr++);
         ymm2 = _mm256_load_ps((const float*)sptr++);
         ymm3 = _mm256_load_ps((const float*)sptr++);
         ymm4 = _mm256_load_ps((const float*)sptr++);
         ymm5 = _mm256_load_ps((const float*)sptr++);
         ymm5 = _mm256_load_ps((const float*)sptr++);
         ymm7 = _mm256_load_ps((const float*)sptr++);

         v += vstep;

         ymm0 =_mm256_fmadd_ps(ymm0, tv,_mm256_load_ps((const float*)(dptr+0)));
         ymm1 =_mm256_fmadd_ps(ymm1, tv,_mm256_load_ps((const float*)(dptr+1)));
         ymm2 =_mm256_fmadd_ps(ymm2, tv,_mm256_load_ps((const float*)(dptr+2)));
         ymm3 =_mm256_fmadd_ps(ymm3, tv,_mm256_load_ps((const float*)(dptr+3)));
         ymm4 =_mm256_fmadd_ps(ymm4, tv,_mm256_load_ps((const float*)(dptr+4)));
         ymm5 =_mm256_fmadd_ps(ymm5, tv,_mm256_load_ps((const float*)(dptr+5)));
         ymm6 =_mm256_fmadd_ps(ymm6, tv,_mm256_load_ps((const float*)(dptr+6)));
         ymm7 =_mm256_fmadd_ps(ymm7, tv,_mm256_load_ps((const float*)(dptr+7)));

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

            ymm0 = _mm256_load_ps((const float*)sptr++);
            ymm1 = _mm256_load_ps((const float*)sptr++);

            ymm0 =_mm256_fmadd_ps(ymm0, tv,_mm256_load_ps((const float*)(dptr+0)));
            ymm1 =_mm256_fmadd_ps(ymm1, tv,_mm256_load_ps((const float*)(dptr+1)));

            v += vstep;

            _mm256_store_ps((float*)dptr++, ymm0);
            _mm256_store_ps((float*)dptr++, ymm1);
         }
         while(--i);
         vstep /= step;
      }
      _mm256_zeroall();
   }

   if (num)
   {
      vstep /= step;
      i = num;
      do {
         *d++ += *s++ * v;
         v += vstep;
      } while(--i);
   }
}

#endif /* __FMA__ */

