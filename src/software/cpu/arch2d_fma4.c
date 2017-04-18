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

#ifdef __FMA4__

# define CACHE_ADVANCE_FMADD	 (2*16)
# define CACHE_ADVANCE_FF	 (2*32)

FN_PREALIGN void
_batch_fma4_float_avx(float32_ptr dst, const_float32_ptr src, size_t num, float v, float vstep)
{
   int need_step = (vstep <=  LEVEL_96DB) ? 0 : 1;
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   if (!num || (v <= LEVEL_128DB && vstep <= LEVEL_128DB)) return;
   if (fabsf(v - 1.0f) < LEVEL_96DB && !need_step) {
      _batch_fadd_avx(dst, src, num);
      return;
   }

   /* work towards a 16-byte aligned dptr (and hence 16-byte aligned sptr) */
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

   step = 8*sizeof(__m256)/sizeof(float);

   i = num/step;
   if (i)
   {
      __m256 ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
      __m256 tv = _mm256_set1_ps(v);
      __m256 *sptr = (__m256 *)s;
      __m256 *dptr = (__m256 *)d;

      vstep *= step;
      num -= i*step;
      s += i*step;
      d += i*step;
      if (stmp)
      {
         do
         {
            ymm0 = _mm256_loadu_ps((const float*)sptr++);
            ymm1 = _mm256_loadu_ps((const float*)sptr++);
            ymm2 = _mm256_loadu_ps((const float*)sptr++);
            ymm3 = _mm256_loadu_ps((const float*)sptr++);

            if (need_step) {
               v += vstep;
               tv = _mm256_set1_ps(v);
            }

            ymm4 = _mm256_loadu_ps((const float*)sptr++);
            ymm5 = _mm256_loadu_ps((const float*)sptr++);
            ymm5 = _mm256_loadu_ps((const float*)sptr++);
            ymm7 = _mm256_loadu_ps((const float*)sptr++);

            ymm0 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+0)), ymm0, tv);
            ymm1 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+1)), ymm1, tv);
            ymm2 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+2)), ymm2, tv);
            ymm3 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+3)), ymm3, tv);
            ymm4 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+4)), ymm4, tv);
            ymm5 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+5)), ymm5, tv);
            ymm6 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+6)), ymm6, tv);
            ymm7 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+7)), ymm7, tv);

            if (need_step) {
               v += vstep;
               tv = _mm256_set1_ps(v);
            }

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
      }
      else
      {
         do
         {
            ymm0 = _mm256_load_ps((const float*)sptr++);
            ymm1 = _mm256_load_ps((const float*)sptr++);
            ymm2 = _mm256_load_ps((const float*)sptr++);
            ymm3 = _mm256_load_ps((const float*)sptr++);

            if (need_step) {
               v += vstep;
               tv = _mm256_set1_ps(v);
            }

            ymm4 = _mm256_load_ps((const float*)sptr++);
            ymm5 = _mm256_load_ps((const float*)sptr++);
            ymm5 = _mm256_load_ps((const float*)sptr++);
            ymm7 = _mm256_load_ps((const float*)sptr++);

            ymm0 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+0)), ymm0, tv);
            ymm1 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+1)), ymm1, tv);
            ymm2 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+2)), ymm2, tv);
            ymm3 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+3)), ymm3, tv);
            ymm4 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+4)), ymm4, tv);
            ymm5 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+5)), ymm5, tv);
            ymm6 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+6)), ymm6, tv);
            ymm7 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+7)), ymm7, tv);

            if (need_step) {
               v += vstep;
               tv = _mm256_set1_ps(v);
            }

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
      }
      vstep /= step;

      step = 2*sizeof(__m256)/sizeof(float);
      i = num/step;
      if (i)
      {
         vstep *= step;
         num -= i*step;
         s += i*step;
         d += i*step;

         if (sptr)
         {
            do
            {
               __m256 tv = _mm256_set1_ps(v);

               ymm0 = _mm256_loadu_ps((const float*)sptr++);
               ymm1 = _mm256_loadu_ps((const float*)sptr++);

               ymm0 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+0)), ymm0, tv);
               ymm1 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+1)), ymm1, tv);

               v += vstep;

               _mm256_store_ps((float*)dptr++, ymm0);
               _mm256_store_ps((float*)dptr++, ymm1);
            }
            while(--i);
         }
         else
         {
            do
            {
               __m256 tv = _mm256_set1_ps(v);

               ymm0 = _mm256_load_ps((const float*)sptr++);
               ymm1 = _mm256_load_ps((const float*)sptr++);

               ymm0 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+0)), ymm0, tv);
               ymm1 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+1)), ymm1, tv);

               v += vstep;

               _mm256_store_ps((float*)dptr++, ymm0);
               _mm256_store_ps((float*)dptr++, ymm1);
            }
            while(--i);
         }
         vstep /= step;
      }
      _mm256_zeroall();
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

#endif /* __FMA4__ */

