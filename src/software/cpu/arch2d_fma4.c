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

#include "software/rbuf_int.h"
#include "software/cpu/arch2d_simd.h"

#ifdef __FMA4__

# define CACHE_ADVANCE_FMADD	 (2*16)
# define CACHE_ADVANCE_FF	 (2*32)

FN_PREALIGN void
_batch_fma4_float_avx(float32_ptr dst, const_float32_ptr src, size_t num, float v, float vstep)
{
   int need_step = (fabsf(vstep) <=  LEVEL_96DB) ? 0 : 1;
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   if (!num || (fabsf(v) <= LEVEL_128DB && !need_step)) return;

   if (fabsf(v - 1.0f) < LEVEL_96DB && !need_step) {
      _batch_fadd_avx(dst, src, num);
      return;
   }

   if (need_step)
   {
      /* work towards a 32-byte aligned d (and hence 32-byte aligned s) */
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

      step = sizeof(__m256)/sizeof(float);

      i = num/step;

      if (i)
      {
         __m256 ymm0, dv, tv, dvstep;
         __m256 *sptr = (__m256 *)s;
         __m256 *dptr = (__m256 *)d;


         assert(step == 8);
         dvstep = _mm256_set_ps(7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f);
         dvstep = _mm256_mul_ps(dvstep, _mm256_set1_ps(vstep));

         dv = _mm256_set1_ps(vstep*step);
         tv = _mm256_add_ps(_mm256_set1_ps(v), dvstep);
         v += i*step*vstep;

         num -= i*step;
         s += i*step;
         d += i*step;
         if (stmp)
         {
            do
            {
               ymm0 = _mm256_loadu_ps((const float*)sptr++);

               ymm0 =_mm256_macc_ps(_mm256_load_ps((const float*)dptr), ymm0, tv);
               tv = _mm256_add_ps(tv, dv);

               _mm256_store_ps((float*)dptr++, ymm0);
            }
            while(--i);
         }
         else
         {
            do
            {
               ymm0 = _mm256_load_ps((const float*)sptr++);

               ymm0 =_mm256_macc_ps(_mm256_load_ps((const float*)dptr), ymm0, tv);
               tv = _mm256_add_ps(tv, dv);

               _mm256_store_ps((float*)dptr++, ymm0);
            }
            while(--i);
         }
         _mm256_zeroupper();
      }

      if (num)
      {
         i = num;
         do {
            *d++ += *s++ * v;
            v += vstep;
         } while(--i);
      }
      _mm256_zeroall();
   }
   else
   {
      /* work towards a 32-byte aligned d (and hence 32-byte aligned s) */
      dtmp = (size_t)d & MEMMASK;
      if (dtmp && num)
      {
         i = (MEMALIGN - dtmp)/sizeof(float);
         if (i <= num)
         {
            num -= i;
            do {
               *d++ += *s++ * v;
            } while(--i);
         }
      }
      stmp = (size_t)s & MEMMASK;

      step = 2*sizeof(__m256)/sizeof(float);
      i = num/step;
      if (i)
      {
         __m256* sptr = (__m256*)s;
         __m256* dptr = (__m256*)d;
         __m256 ymm0, ymm1;

         num -= i*step;
         s += i*step;
         d += i*step;
         if (stmp)
         {
            __m256 tv = _mm256_set1_ps(v);
            do
            {
               ymm0 = _mm256_loadu_ps((const float*)sptr++);
               ymm1 = _mm256_loadu_ps((const float*)sptr++);

               ymm0 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+0)), ymm0, tv);
               ymm1 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+1)), ymm1, tv);

               _mm256_store_ps((float*)dptr++, ymm0);
               _mm256_store_ps((float*)dptr++, ymm1);
            }
            while(--i);
         }
         else
         {
            __m256 tv = _mm256_set1_ps(v);
            do
            {
               ymm0 = _mm256_load_ps((const float*)sptr++);
               ymm1 = _mm256_load_ps((const float*)sptr++);

               ymm0 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+0)), ymm0, tv);
               ymm1 =_mm256_macc_ps(_mm256_load_ps((const float*)(dptr+1)), ymm1, tv);

               _mm256_store_ps((float*)dptr++, ymm0);
               _mm256_store_ps((float*)dptr++, ymm1);
            }
            while(--i);
         }
         _mm256_zeroupper();
      }

      if (num)
      {
         i = num;
         do {
            *d++ += *s++ * v;
         } while(--i);
      }
      _mm256_zeroall();
   }
}

#else
typedef int make_iso_compilers_happy;
#endif /* __FMA4__ */

