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

#include <math.h>	/* for floorf */


#include "software/rbuf_int.h"
#include "arch2d_simd.h"

#ifdef __AVX__

#if 0
# define PRINTFUNC	printf("%s\n", __func__)
#else
# define PRINTFUNC
#endif

FN_PREALIGN void
_batch_cvt24_ps_avx(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *d = (int32_t*)dst;
   float *s = (float*)src;

   PRINTFUNC;
   if (((size_t)d & MEMMASK) != 0 || ((size_t)s & MEMMASK) != 0)
   {
      if (((size_t)d & MEMMASK16) == 0 || ((size_t)s & MEMMASK16) == 0)
      {
         _batch_cvt24_ps_sse_vex(dst, src, num);
         return;
      }
      else
      {
         float mul = (float)(1<<23);
         size_t i = num;
         do {
            *d++ = (int32_t)(*s++ * mul);
         } while (--i);
         return;
      }
   }

   assert(((size_t)d & MEMMASK) == 0);
   assert(((size_t)s & MEMMASK) == 0);

   if (num)
   {
      size_t i, step;

      step = 6*sizeof(__m256)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m256 ymm0, ymm1, ymm2, ymm3, ymm4, ymm5;
         __m256 mul = _mm256_set1_ps((float)(1<<23));
         __m256i *dptr = (__m256i*)d;
         __m256* sptr = (__m256*)s;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            ymm0 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);
            ymm1 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);
            ymm2 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);
            ymm3 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);
            ymm4 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);
            ymm5 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);

            _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm0));
            _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm1));
            _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm2));
            _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm3));
            _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm4));
            _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm5));
         }
         while(--i);

         step = 2*sizeof(__m256)/sizeof(float);
         i = num/step;
         if (i)
         {
            num -= i*step;
            s += i*step;
            d += i*step;
            do
            {
               ymm0 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);
               ymm1 = _mm256_mul_ps(_mm256_load_ps((const float*)sptr++), mul);

               _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm0));
               _mm256_store_si256(dptr++, _mm256_cvtps_epi32(ymm1));
            }
            while(--i);
         }
         _mm256_zeroupper();
      }

      if (num)
      {
         float mul = (float)(1<<23);
         i = num;
         do {
            *d++ = (int32_t)(*s++ * mul);
         } while (--i);
      }
   }
}

FN_PREALIGN void
_batch_cvt24_ps24_avx(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *d = (int32_t*)dst;
   float *s = (float*)src;
   size_t i, step;
   size_t dtmp, stmp;

   PRINTFUNC;
   if (!num) return;

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if ((dtmp || stmp) && dtmp != stmp)
   {
      _batch_cvt24_ps24_sse_vex(dst, src, num);
      return;
   }

   /* work towards a 32-byte aligned d (and hence 32-byte aligned sptr) */
   if (dtmp && num)
   {
      i = (MEMALIGN - dtmp)/sizeof(int32_t);
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
      step = 6*sizeof(__m256)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m256i ymm2i, ymm3i, ymm4i, ymm5i, ymm6i, ymm7i;
         __m256i *dptr = (__m256i*)d;
         __m256* sptr = (__m256*)s;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            ymm2i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));
            ymm3i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));
            ymm4i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));
            ymm5i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));
            ymm6i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));
            ymm7i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));

            _mm256_store_si256(dptr++, ymm2i);
            _mm256_store_si256(dptr++, ymm3i);
            _mm256_store_si256(dptr++, ymm4i);
            _mm256_store_si256(dptr++, ymm5i);
            _mm256_store_si256(dptr++, ymm6i);
            _mm256_store_si256(dptr++, ymm7i);
         }
         while(--i);

         step = 2*sizeof(__m256)/sizeof(float);
         i = num/step;
         if (i)
         {
            num -= i*step;
            s += i*step;
            d += i*step;
            do
            {
               ymm2i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));
               ymm3i = _mm256_cvtps_epi32(_mm256_load_ps((const float*)sptr++));

               _mm256_store_si256(dptr++, ymm2i);
               _mm256_store_si256(dptr++, ymm3i);
            }
            while(--i);
         }
         _mm256_zeroupper();
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

FN_PREALIGN void
_batch_cvtps_24_avx(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *s = (int32_t*)src;
   float *d = (float*)dst;

   assert(((size_t)d & MEMMASK) == 0);
   assert(((size_t)s & MEMMASK) == 0);

   PRINTFUNC;
   if (num)
   {
      size_t i, step;

      step = 6*sizeof(__m256i)/sizeof(int32_t);

      i = num/step;
      if (i)
      {
         __m256 ymm0, ymm1, ymm2, ymm3, ymm4, ymm5;
         __m256 mul = _mm256_set1_ps(1.0f/(float)(1<<23));
         __m256i* sptr = (__m256i*)s;
         __m256 *dptr = (__m256*)d;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            ymm0 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm1 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm2 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm3 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm4 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm5 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));

            _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm0, mul));
            _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm1, mul));
            _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm2, mul));
            _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm3, mul));
            _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm4, mul));
            _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm5, mul));
         }
         while(--i);

         step = 2*sizeof(__m256i)/sizeof(int32_t);
         i = num/step;
         if (i)
         {
            num -= i*step;
            s += i*step;
            d += i*step;
            do
            {
               ymm0 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
               ymm1 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));

               _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm0, mul));
               _mm256_store_ps((float*)dptr++, _mm256_mul_ps(ymm1, mul));
            }
            while(--i);
         }
         _mm256_zeroupper();
      }

      if (num)
      {
         float mul = 1.0f/(float)(1<<23);
         i = num;
         do {
            *d++ = (float)(*s++) * mul;
         } while (--i);
      }
   }
}

FN_PREALIGN void
_batch_cvtps24_24_avx(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *s = (int32_t*)src;
   float *d = (float*)dst;
   size_t i, step;
   size_t dtmp, stmp;

   PRINTFUNC;
   assert(s != 0);
   assert(d != 0);

   if (!num) return;

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if ((dtmp || stmp) && dtmp != stmp)
   {
      _batch_cvtps24_24_sse_vex(dst, src, num);
      return;
   }

   /* work towards a 32-byte aligned d (and hence 32-byte aligned sptr) */
   if (dtmp && num)
   {
      i = (MEMALIGN - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ += (float)*s++;
         } while(--i);
      }
   }

   if (num)
   {
      step = 6*sizeof(__m256i)/sizeof(int32_t);

      i = num/step;
      if (i)
      {
         __m256 ymm0, ymm1, ymm2, ymm3, ymm4, ymm5;
         __m256i* sptr = (__m256i*)s;
         __m256 *dptr = (__m256*)d;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            ymm0 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm1 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm2 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm3 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm4 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
            ymm5 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));

            _mm256_store_ps((float*)dptr++, ymm0);
            _mm256_store_ps((float*)dptr++, ymm1);
            _mm256_store_ps((float*)dptr++, ymm2);
            _mm256_store_ps((float*)dptr++, ymm3);
            _mm256_store_ps((float*)dptr++, ymm4);
            _mm256_store_ps((float*)dptr++, ymm5);
         }
         while(--i);

         step = 2*sizeof(__m256i)/sizeof(int32_t);
         i = num/step;
         if (i)
         {
            i = num/step;
            num -= i*step;
            s += i*step;
            d += i*step;
            if (i)
            {
               do
               {
                  ymm4 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));
                  ymm5 = _mm256_cvtepi32_ps(_mm256_load_si256(sptr++));

                  _mm256_store_ps((float*)dptr++, ymm4);
                  _mm256_store_ps((float*)dptr++, ymm5);
               }
               while(--i);
            }
         }
         _mm256_zeroupper();
      }

      if (num)
      {
         i = num;
         do {
            *d++ = (float)*s++;
         } while (--i);
      }
   }
}

FN_PREALIGN void
_batch_fmul_value_avx(void* dptr, const void *sptr, unsigned bps, size_t num, float f)
{
   if (!num || fabsf(f - 1.0f) < LEVEL_96DB) return;

   if (f <= LEVEL_128DB) {
      memset(dptr, 0, num*bps);
   }
   else if (bps == 4)
   {
      const_float32_ptr s = (float32_ptr)sptr;
      float32_ptr d = (float32_ptr)dptr;
      size_t i, step, dtmp;

      dtmp = (size_t)d & MEMMASK;
      /* work towards a 32-byte aligned d (and hence 32-byte aligned s) */
      if (dtmp && num)
      {
         i = (MEMALIGN - dtmp)/sizeof(float);
         if (i <= num)
         {
            num -= i;
            do {
               *d++ = *s++ * f;
            } while(--i);
         }
      }

      step = 2*sizeof(__m256)/sizeof(float);
      i = num/step;
      if (i)
      {
         __m256 tv = _mm256_set1_ps(f);
         __m256* sptr = (__m256*)s;
         __m256* dptr = (__m256*)d;
         __m256 ymm0, ymm1;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            ymm0 = _mm256_mul_ps(tv, _mm256_load_ps((const float*)(sptr++)));
            ymm1 = _mm256_mul_ps(tv, _mm256_load_ps((const float*)(sptr++)));

            _mm256_store_ps((float*)dptr++, ymm0);
            _mm256_store_ps((float*)dptr++, ymm1);
         }
         while(--i);
      }
      _mm256_zeroupper();

      if (num)
      {
         i = num;
         do {
            *d++ = *s++ * f;
         } while(--i);
      }
   }
   else
   {
      const_double64_ptr s = (double64_ptr)sptr;
      double64_ptr d = (double64_ptr)dptr;
      size_t i, step, dtmp;

      dtmp = (size_t)d & MEMMASK;
      if (dtmp && num)
      {
         i = (MEMALIGN - dtmp)/sizeof(double);
         if (i <= num)
         {
            num -= i;
            do {
               *d++ = *s++ * f;
            } while(--i);
         }
      }

      step = 2*sizeof(__m256d)/sizeof(double);

      i = num/step;
      if (i)
      {
         __m256d ymm0, ymm1;
         __m256d tv = _mm256_set1_pd(f);
         __m256d* sptr = (__m256d*)s;
         __m256d* dptr = (__m256d*)d;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            ymm0 = _mm256_mul_pd(_mm256_load_pd((const double*)(sptr++)), tv);
            ymm1 = _mm256_mul_pd(_mm256_load_pd((const double*)(sptr++)), tv);

            _mm256_store_pd((double*)dptr++, ymm0);
            _mm256_store_pd((double*)dptr++, ymm1);
         }
         while(--i);
      }
      _mm256_zeroupper();

      if (num)
      {
         i = num;
         do {
            *d++ = *s++ * f;
         } while(--i);
      }
   }
}

#if 0
FN_PREALIGN void
_batch_hadd_avx(float32_ptr dst, const_float16_ptr src, size_t num)
{
   float16_ptr s = (float16_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   PRINTFUNC;
   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if ((dtmp || stmp) && dtmp != stmp)
   {
      i = num;                          /* improperly aligned,            */
      do                                /* let the compiler figure it out */
      {
         *d++ += HALF2FLOAT(*s);
          s++;
      }
      while (--i);
      return;
   }

   /* work towards a 32-byte aligned d (and hence 32-byte aligned s) */
   if (dtmp && num)
   {
      i = (MEMALIGN - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ += HALF2FLOAT(*s);
            s++;
         } while(--i);
      }
   }

   step = 8*sizeof(__m256)/sizeof(float);

   i = num/step;
   if (i)
   {
      __m256 ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
      __m128i* sptr = (__m128i*)s;
      __m256* dptr = (__m256*)d;

      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         ymm0 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
         ymm1 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
         ymm2 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
         ymm3 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
         ymm4 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
         ymm5 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
         ymm6 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
         ymm7 = _mm256_cvtph_ps(_mm_load_si128(sptr++));

         ymm0 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+0)), ymm0);
         ymm1 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+1)), ymm1);
         ymm2 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+2)), ymm2);
         ymm3 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+3)), ymm3);
         ymm4 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+4)), ymm4);
         ymm5 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+5)), ymm5);
         ymm6 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+6)), ymm6);
         ymm7 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+7)), ymm7);

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

      step = 2*sizeof(__m256)/sizeof(float);
      i = num/step;
      if (i)
      {
         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            ymm0 = _mm256_cvtph_ps(_mm_load_si128(sptr++));
            ymm1 = _mm256_cvtph_ps(_mm_load_si128(sptr++));

            ymm0 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+0)), ymm0);
            ymm1 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+1)), ymm1);

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
         *d++ += HALF2FLOAT(*s);
         s++;
      } while(--i);
   }
}
#endif

FN_PREALIGN void
_batch_fadd_avx(float32_ptr dst, const_float32_ptr src, size_t num)
{
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   PRINTFUNC;

   /* work towards a 16-byte aligned d (and hence 16-byte aligned s) */
   dtmp = (size_t)d & MEMMASK;
   if (dtmp && num)
   {
      i = (MEMALIGN - dtmp)/sizeof(float);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ += *s++;
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
         do
         {
            ymm0 = _mm256_loadu_ps((const float*)sptr++);
            ymm1 = _mm256_loadu_ps((const float*)sptr++);

            ymm0 =_mm256_add_ps(ymm0, _mm256_load_ps((const float*)(dptr+0)));
            ymm1 =_mm256_add_ps(ymm1, _mm256_load_ps((const float*)(dptr+1)));

            _mm256_store_ps((float*)dptr++, ymm0);
            _mm256_store_ps((float*)dptr++, ymm1);
         }
         while(--i);
      }
      else
      {
         do
         {
            ymm0 = _mm256_load_ps((const float*)sptr++);
            ymm1 = _mm256_load_ps((const float*)sptr++);

            ymm0 =_mm256_add_ps(ymm0, _mm256_load_ps((const float*)(dptr+0)));
            ymm1 =_mm256_add_ps(ymm1, _mm256_load_ps((const float*)(dptr+1)));

            _mm256_store_ps((float*)dptr++, ymm0);
            _mm256_store_ps((float*)dptr++, ymm1);
         }
         while(--i);
      }
   }
   _mm256_zeroupper();

   if (num)
   {
      i = num;
      do {
         *d++ += *s++;
      } while(--i);
   }
   _mm256_zeroall();
}

#if 0
FN_PREALIGN void
_batch_hmadd_avx(float32_ptr dst, const_float16_ptr src, size_t num, float v, float vstep)
{
   float16_ptr s = (float16_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   PRINTFUNC;
   if (!num || (v <= LEVEL_128DB && vstep <= LEVEL_128DB)) return;
   if (fabsf(v - 1.0f) < LEVEL_96DB && vstep <=  LEVEL_96DB) {
      _batch_hadd_avx(dst, src, num);
      return;
   }

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if ((dtmp || stmp) && dtmp != stmp) {
      return _batch_hmadd_sse_vex(dst, src, num, v, vstep);
   }

   /* work towards a 32-byte aligned d (and hence 32-byte aligned s) */
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

   step = 4*sizeof(__m256)/sizeof(float);

   i = num/step;
   if (i)
   {
      __m256 ymm0, ymm1, ymm2, ymm3;
      __m128i* sptr = (__m128i*)s;
      __m256 *dptr = (__m256*)d;

       vstep *= step;
      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         __m256 tv = _mm256_set1_ps(v);

         ymm0 = _mm256_mul_ps(_mm256_cvtph_ps(_mm_load_si128(sptr++)), tv);
         ymm1 = _mm256_mul_ps(_mm256_cvtph_ps(_mm_load_si128(sptr++)), tv);
         ymm2 = _mm256_mul_ps(_mm256_cvtph_ps(_mm_load_si128(sptr++)), tv);
         ymm3 = _mm256_mul_ps(_mm256_cvtph_ps(_mm_load_si128(sptr++)), tv);

         ymm0 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+0)), ymm0);
         ymm1 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+1)), ymm1);
         ymm2 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+2)), ymm2);
         ymm3 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+3)), ymm3);

         v += vstep;

         _mm256_store_ps((float*)dptr++, ymm0);
         _mm256_store_ps((float*)dptr++, ymm1);
         _mm256_store_ps((float*)dptr++, ymm2);
         _mm256_store_ps((float*)dptr++, ymm3);
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

            ymm0 = _mm256_mul_ps(_mm256_cvtph_ps(_mm_load_si128(sptr++)), tv);
            ymm1 = _mm256_mul_ps(_mm256_cvtph_ps(_mm_load_si128(sptr++)), tv);

            ymm0 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+0)), ymm0);
            ymm1 = _mm256_add_ps(_mm256_load_ps((const float*)(dptr+1)), ymm1);

            v += vstep;

            _mm256_store_ps((float*)dptr++, ymm0);
            _mm256_store_ps((float*)dptr++, ymm1);
         }
         while(--i);
         vstep /= step;
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
}
#endif

FN_PREALIGN void
_batch_fmadd_avx(float32_ptr dst, const_float32_ptr src, size_t num, float v, float vstep)
{
   int need_step = (fabsf(vstep) <= LEVEL_96DB) ? 0 : 1;
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   PRINTFUNC;
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
         __m256* sptr = (__m256*)s;
         __m256* dptr = (__m256*)d;

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
               ymm0 = _mm256_mul_ps(tv, _mm256_loadu_ps((const float*)sptr++));
               ymm0 = _mm256_add_ps(ymm0, _mm256_load_ps((const float*)dptr));

               tv = _mm256_add_ps(tv, dv);

               _mm256_store_ps((float*)dptr++, ymm0);
            }
            while(--i);
         }
         else
         {
            do
            {
               ymm0 = _mm256_mul_ps(tv, _mm256_load_ps((const float*)sptr++));
               ymm0 = _mm256_add_ps(ymm0, _mm256_load_ps((const float*)dptr));

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
               ymm0 = _mm256_mul_ps(tv, _mm256_loadu_ps((const float*)sptr++));
               ymm1 = _mm256_mul_ps(tv, _mm256_loadu_ps((const float*)sptr++));

               ymm0 =_mm256_add_ps(ymm0,_mm256_load_ps((const float*)(dptr+0)));
               ymm1 =_mm256_add_ps(ymm1,_mm256_load_ps((const float*)(dptr+1)));

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
               ymm0 = _mm256_mul_ps(tv, _mm256_load_ps((const float*)sptr++));
               ymm1 = _mm256_mul_ps(tv, _mm256_load_ps((const float*)sptr++));

               ymm0 =_mm256_add_ps(ymm0,_mm256_load_ps((const float*)(dptr+0)));
               ymm1 =_mm256_add_ps(ymm1,_mm256_load_ps((const float*)(dptr+1)));

               _mm256_store_ps((float*)dptr++, ymm0);
               _mm256_store_ps((float*)dptr++, ymm1);
            }
            while(--i);
         }
      }
      _mm256_zeroupper();

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

/*
 * optimized memcpy for 32-byte aligned destination buffer
 * fall back tobuildt-in  memcpy otherwise.
 */
void *
_aax_memcpy_avx(void_ptr dst, const_void_ptr src, size_t num)
{
   size_t i, step;
   char *d = (char*)dst;
   char *s = (char*)src;
   size_t tmp;

   if (!num) return dst;

   /*
    * work towards a 32-byte aligned dptr and possibly sptr
    */
   tmp = (size_t)d & MEMMASK;
   if (tmp)
   {
      i = (MEMALIGN - tmp);
      num -= i;

      memcpy(d, s, i);
      d += i;
      s += i;
   }

   tmp = (size_t)s & MEMMASK;
   step = 8*sizeof(__m256i)/sizeof(int8_t);

   i = num/step;
   if (i)
   {
      __m256i *sptr = (__m256i*)s;
      __m256i *dptr = (__m256i*)d;

      num -= i*step;
      s += i*step;
      d += i*step;

      if (tmp)
      {
         do
         {
            _mm256_store_si256(dptr++, _mm256_loadu_si256(sptr++));
            _mm256_store_si256(dptr++, _mm256_loadu_si256(sptr++));
            _mm256_store_si256(dptr++, _mm256_loadu_si256(sptr++));
            _mm256_store_si256(dptr++, _mm256_loadu_si256(sptr++));
            _mm256_store_si256(dptr++, _mm256_loadu_si256(sptr++));
            _mm256_store_si256(dptr++, _mm256_loadu_si256(sptr++));
            _mm256_store_si256(dptr++, _mm256_loadu_si256(sptr++));
            _mm256_store_si256(dptr++, _mm256_loadu_si256(sptr++));
         }
         while(--i);
      }
      else	/* both buffers are 32-byte aligned */
      {
         do
         {
            _mm256_store_si256(dptr++, _mm256_load_si256(sptr++));
            _mm256_store_si256(dptr++, _mm256_load_si256(sptr++));
            _mm256_store_si256(dptr++, _mm256_load_si256(sptr++));
            _mm256_store_si256(dptr++, _mm256_load_si256(sptr++));
            _mm256_store_si256(dptr++, _mm256_load_si256(sptr++));
            _mm256_store_si256(dptr++, _mm256_load_si256(sptr++));
            _mm256_store_si256(dptr++, _mm256_load_si256(sptr++));
            _mm256_store_si256(dptr++, _mm256_load_si256(sptr++));
         }
         while(--i);
      }
   }

   if (num) {
      memcpy(d, s, num);
   }

   return dst;
}

#else
typedef int make_iso_compilers_happy;
#endif /* AVX */

