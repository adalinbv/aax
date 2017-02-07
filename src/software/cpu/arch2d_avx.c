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

#include <math.h>	/* for floorf */


#include "software/rbuf_int.h"
#include "arch2d_simd.h"

#ifdef __AVX__

# define CACHE_ADVANCE_FMADD	 32
# define CACHE_ADVANCE_CPY	 32
# define CACHE_ADVANCE_CVT	 64
# define CACHE_ADVANCE_IMADD     32
# define CACHE_ADVANCE_INTL      32


void
_batch_cvt24_ps_avx(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *d = (int32_t*)dst;
   float *s = (float*)src;

   if (((size_t)d & MEMMASK) != 0 || ((size_t)s & MEMMASK) != 0)
   {
      if (((size_t)d & MEMMASK16) == 0 || ((size_t)s & MEMMASK16) == 0) {
         return _batch_cvt24_ps_sse2(dst, src, num);
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
      __m256i *dptr = (__m256i*)d;
      __m256* sptr = (__m256*)s;
      size_t i, step;

      step = 4*sizeof(__m256)/sizeof(float);

      i = num/step;
      num -= i*step;
      if (i)
      {
         __m256i ymm4i, ymm5i, ymm6i, ymm7i;
         __m256 ymm0, ymm1, ymm2, ymm3;
         __m256 mul = _mm256_set1_ps((float)(1<<23));
         do
         {
            _mm_prefetch(((char *)s)+CACHE_ADVANCE_CVT, _MM_HINT_NTA);

            ymm0 = _mm256_load_ps((const float*)sptr++);
            ymm1 = _mm256_load_ps((const float*)sptr++);
            ymm2 = _mm256_load_ps((const float*)sptr++);
            ymm3 = _mm256_load_ps((const float*)sptr++);

            ymm0 = _mm256_mul_ps(ymm0, mul);
            ymm1 = _mm256_mul_ps(ymm1, mul);
            ymm2 = _mm256_mul_ps(ymm2, mul);
            ymm3 = _mm256_mul_ps(ymm3, mul);

            ymm4i = _mm256_cvtps_epi32(ymm0);
            ymm5i = _mm256_cvtps_epi32(ymm1);
            ymm6i = _mm256_cvtps_epi32(ymm2);
            ymm7i = _mm256_cvtps_epi32(ymm3);

            d += step;
            s += step;

            _mm256_store_si256(dptr++, ymm4i);
            _mm256_store_si256(dptr++, ymm5i);
            _mm256_store_si256(dptr++, ymm6i);
            _mm256_store_si256(dptr++, ymm7i);
         }
         while(--i);
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

void
_batch_cvt24_ps24_avx(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *d = (int32_t*)dst;
   float *s = (float*)src;
   size_t i, step;
   size_t dtmp, stmp;

   if (!num) return;

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if ((dtmp || stmp) && dtmp != stmp)  /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      i = num;
      do {
         *d++ += (int32_t)*s++;
      }
      while (--i);
      return;
   }

   /* work towards a 16-byte aligned d (and hence 16-byte aligned sptr) */
   if (dtmp && num)
   {
      i = (0x20 - dtmp)/sizeof(int32_t);
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
      __m256i *dptr = (__m256i*)d;
      __m256* sptr = (__m256*)s;

      step = 4*sizeof(__m256)/sizeof(float);

      i = num/step;
      num -= i*step;
      if (i)
      {
         __m256i ymm4i, ymm5i, ymm6i, ymm7i;
         __m256 ymm0, ymm1, ymm2, ymm3;
         do
         {
            _mm_prefetch(((char *)s)+CACHE_ADVANCE_CVT, _MM_HINT_NTA);

            ymm0 = _mm256_load_ps((const float*)sptr++);
            ymm1 = _mm256_load_ps((const float*)sptr++);
            ymm2 = _mm256_load_ps((const float*)sptr++);
            ymm3 = _mm256_load_ps((const float*)sptr++);

            ymm4i = _mm256_cvtps_epi32(ymm0);
            ymm5i = _mm256_cvtps_epi32(ymm1);
            ymm6i = _mm256_cvtps_epi32(ymm2);
            ymm7i = _mm256_cvtps_epi32(ymm3);

            d += step;
            s += step;

            _mm256_store_si256(dptr++, ymm4i);
            _mm256_store_si256(dptr++, ymm5i);
            _mm256_store_si256(dptr++, ymm6i);
            _mm256_store_si256(dptr++, ymm7i);
         }
         while(--i);
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

void
_batch_cvtps_24_avx(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *s = (int32_t*)src;
   float *d = (float*)dst;

   assert(((size_t)d & MEMMASK) == 0);
   assert(((size_t)s & MEMMASK) == 0);

   if (num)
   {
      __m256i* sptr = (__m256i*)s;
      __m256 *dptr = (__m256*)d;
      size_t i, step;

      step = 4*sizeof(__m256i)/sizeof(int32_t);

      i = num/step;
      num -= i*step;
      if (i)
      {
         __m256i ymm0i, ymm1i, ymm2i, ymm3i;
         __m256 ymm4, ymm5, ymm6, ymm7;
         __m256 mul = _mm256_set1_ps(1.0f/(float)(1<<23));
         do
         {
            _mm_prefetch(((char *)s)+CACHE_ADVANCE_CVT, _MM_HINT_NTA);

            ymm0i = _mm256_load_si256(sptr++);
            ymm1i = _mm256_load_si256(sptr++);
            ymm2i = _mm256_load_si256(sptr++);
            ymm3i = _mm256_load_si256(sptr++);

            ymm4 = _mm256_cvtepi32_ps(ymm0i);
            ymm5 = _mm256_cvtepi32_ps(ymm1i);
            ymm6 = _mm256_cvtepi32_ps(ymm2i);
            ymm7 = _mm256_cvtepi32_ps(ymm3i);

            ymm4 = _mm256_mul_ps(ymm4, mul);
            ymm5 = _mm256_mul_ps(ymm5, mul);
            ymm6 = _mm256_mul_ps(ymm6, mul);
            ymm7 = _mm256_mul_ps(ymm7, mul);

            s += step;
            d += step;

            _mm256_store_ps((float*)dptr++, ymm4);
            _mm256_store_ps((float*)dptr++, ymm5);
            _mm256_store_ps((float*)dptr++, ymm6);
            _mm256_store_ps((float*)dptr++, ymm7);;
         }
         while(--i);
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

void
_batch_cvtps24_24_avx(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *s = (int32_t*)src;
   float *d = (float*)dst;
   size_t i, step;
   size_t dtmp, stmp;

   assert(s != 0);
   assert(d != 0);

   if (!num) return;

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if ((dtmp || stmp) && dtmp != stmp)  /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      i = num;
      do {
         *d++ += (float)*s++;
      }
      while (--i);
      return;
   }

   /* work towards a 16-byte aligned d (and hence 16-byte aligned sptr) */
   if (dtmp && num)
   {
      i = (0x20 - dtmp)/sizeof(int32_t);
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
      __m256i* sptr = (__m256i*)s;
      __m256 *dptr = (__m256*)d;

      step = 4*sizeof(__m256i)/sizeof(int32_t);

      i = num/step;
      num -= i*step;
      if (i)
      {
         __m256i ymm0i, ymm1i, ymm2i, ymm3i;
         __m256 ymm4, ymm5, ymm6, ymm7;
         do
         {
            _mm_prefetch(((char *)s)+CACHE_ADVANCE_CVT, _MM_HINT_NTA);

            ymm0i = _mm256_load_si256(sptr++);
            ymm1i = _mm256_load_si256(sptr++);
            ymm2i = _mm256_load_si256(sptr++);
            ymm3i = _mm256_load_si256(sptr++);

            ymm4 = _mm256_cvtepi32_ps(ymm0i);
            ymm5 = _mm256_cvtepi32_ps(ymm1i);
            ymm6 = _mm256_cvtepi32_ps(ymm2i);
            ymm7 = _mm256_cvtepi32_ps(ymm3i);

            s += step;
            d += step;

            _mm256_store_ps((float*)dptr++, ymm4);
            _mm256_store_ps((float*)dptr++, ymm5);
            _mm256_store_ps((float*)dptr++, ymm6);
            _mm256_store_ps((float*)dptr++, ymm7);
         }
         while(--i);

         if (num)
         {
            step = 2*sizeof(__m256i)/sizeof(int32_t);

            i = num/step;
            num -= i*step;
            if (i)
            {
               do
               {
                  ymm0i = _mm256_load_si256(sptr++);
                  ymm2i = _mm256_load_si256(sptr++);

                  ymm4 = _mm256_cvtepi32_ps(ymm0i);
                  ymm5 = _mm256_cvtepi32_ps(ymm1i);

                  s += step;
                  d += step;

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

static void
_batch_fadd_avx(float32_ptr dst, const_float32_ptr src, size_t num)
{
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if ((dtmp || stmp) && dtmp != stmp)
   {
      i = num;                          /* improperly aligned,            */
      do                                /* let the compiler figure it out */
      {
         *d++ += *s++;
      }
      while (--i);
      return;
   }

   /* work towards a 16-byte aligned d (and hence 16-byte aligned s) */
   if (dtmp && num)
   {
      i = (0x20 - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ += *s++;
         } while(--i);
      }
   }

   step = 4*sizeof(__m256)/sizeof(float);

   i = num/step;
   num -= i*step;
   if (i)
   {
      __m256* sptr = (__m256*)s;
      __m256 *dptr = (__m256*)d;
      __m256 ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

      do
      {
         _mm_prefetch(((char *)s)+CACHE_ADVANCE_FMADD, _MM_HINT_NTA);
         _mm_prefetch(((char *)d)+CACHE_ADVANCE_FMADD, _MM_HINT_NTA);

         ymm0 = _mm256_load_ps((const float*)sptr++);
         ymm1 = _mm256_load_ps((const float*)sptr++);
         ymm4 = _mm256_load_ps((const float*)sptr++);
         ymm5 = _mm256_load_ps((const float*)sptr++);

         s += step;
         d += step;

         ymm2 = _mm256_load_ps((const float*)dptr);
         ymm3 = _mm256_load_ps((const float*)(dptr+1));
         ymm6 = _mm256_load_ps((const float*)(dptr+2));
         ymm7 = _mm256_load_ps((const float*)(dptr+3));

         ymm2 = _mm256_add_ps(ymm2, ymm0);
         ymm3 = _mm256_add_ps(ymm3, ymm1);
         ymm6 = _mm256_add_ps(ymm6, ymm4);
         ymm7 = _mm256_add_ps(ymm7, ymm5);

         _mm256_store_ps((float*)dptr++, ymm2);
         _mm256_store_ps((float*)dptr++, ymm3);
         _mm256_store_ps((float*)dptr++, ymm6);
         _mm256_store_ps((float*)dptr++, ymm7);
      }
      while(--i);
      _mm256_zeroupper();
   }

   if (num)
   {
      i = num;
      do {
         *d++ += *s++;
      } while(--i);
   }
}


void
_batch_fmadd_avx(float32_ptr dst, const_float32_ptr src, size_t num, float v, float vstep)
{
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   if (!num || (v == 0.0f && vstep == 0.0f)) return;
   if (fabsf(v - 1.0f) < GMATH_128DB && vstep == 0.0f) {
      _batch_fadd_avx(dst, src, num);
      return;
   }

   dtmp = (size_t)d & MEMMASK;
   stmp = (size_t)s & MEMMASK;
   if ((dtmp || stmp) && dtmp != stmp)
   {
      i = num;				/* improperly aligned,            */
      do				/* let the compiler figure it out */
      {
         *d++ += *s++ * v;
         v += vstep;
      }
      while (--i);
      return;
   }

   /* work towards a 16-byte aligned d (and hence 16-byte aligned s) */
   if (dtmp && num)
   {
      i = (0x20 - dtmp)/sizeof(int32_t);
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

   vstep *= step;				/* 8 samples at a time */
   i = num/step;
   num -= i*step;
   if (i)
   {
      __m256* sptr = (__m256*)s;
      __m256 *dptr = (__m256*)d;
      __m256 ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
      __m256 tv = _mm256_set1_ps(v);

      do
      {
         _mm_prefetch(((char *)s)+CACHE_ADVANCE_FMADD, _MM_HINT_NTA);
         _mm_prefetch(((char *)d)+CACHE_ADVANCE_FMADD, _MM_HINT_NTA);

         ymm0 = _mm256_load_ps((const float*)sptr++);
         ymm1 = _mm256_load_ps((const float*)sptr++);
         ymm4 = _mm256_load_ps((const float*)sptr++);
         ymm5 = _mm256_load_ps((const float*)sptr++);

         ymm0 = _mm256_mul_ps(ymm0, tv);
         ymm1 = _mm256_mul_ps(ymm1, tv);
         ymm4 = _mm256_mul_ps(ymm4, tv);
         ymm5 = _mm256_mul_ps(ymm5, tv);

         s += step;
         d += step;

         ymm2 = _mm256_load_ps((const float*)dptr);
         ymm3 = _mm256_load_ps((const float*)(dptr+1));
         ymm6 = _mm256_load_ps((const float*)(dptr+2));
         ymm7 = _mm256_load_ps((const float*)(dptr+3));

         ymm2 = _mm256_add_ps(ymm2, ymm0);
         ymm3 = _mm256_add_ps(ymm3, ymm1);
         ymm6 = _mm256_add_ps(ymm6, ymm4);
         ymm7 = _mm256_add_ps(ymm7, ymm5);

         v += vstep;

         _mm256_store_ps((float*)dptr++, ymm2);
         _mm256_store_ps((float*)dptr++, ymm3);
         _mm256_store_ps((float*)dptr++, ymm6);
         _mm256_store_ps((float*)dptr++, ymm7);

         tv = _mm256_set1_ps(v);
      }
      while(--i);
      _mm256_zeroupper();
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

/*
 * optimized memcpy for 16-byte aligned destination buffer
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
    * work towards a 16-byte aligned dptr and possibly sptr
    */
   tmp = (size_t)d & MEMMASK;
   if (tmp)
   {
      i = (0x20 - tmp);
      num -= i;

      memcpy(d, s, i);
      d += i;
      s += i;
   }

   tmp = (size_t)s & MEMMASK;
   step = 8*sizeof(__m256i)/sizeof(int8_t);

   i = num/step;
   num -= i*step;
   if (i)
   {
      __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
      __m256i *sptr = (__m256i*)s;
      __m256i *dptr = (__m256i*)d;

      s += i*step;
      d += i*step;

      if (tmp)
      {
         do
         {
            _mm_prefetch(((char *)s)+CACHE_ADVANCE_CPY, _MM_HINT_NTA);

            ymm0 = _mm256_loadu_si256(sptr++);
            ymm1 = _mm256_loadu_si256(sptr++);
            ymm2 = _mm256_loadu_si256(sptr++);
            ymm3 = _mm256_loadu_si256(sptr++);
            ymm4 = _mm256_loadu_si256(sptr++);
            ymm5 = _mm256_loadu_si256(sptr++);
            ymm6 = _mm256_loadu_si256(sptr++);
            ymm7 = _mm256_loadu_si256(sptr++);

            _mm256_store_si256(dptr++, ymm0);
            _mm256_store_si256(dptr++, ymm1);
            _mm256_store_si256(dptr++, ymm2);
            _mm256_store_si256(dptr++, ymm3);
            _mm256_store_si256(dptr++, ymm4);
            _mm256_store_si256(dptr++, ymm5);
            _mm256_store_si256(dptr++, ymm6);
            _mm256_store_si256(dptr++, ymm7);
         }
         while(--i);
      }
      else	/* both buffers are 16-byte aligned */
      {
         do
         {
            _mm_prefetch(((char *)s)+CACHE_ADVANCE_CPY, _MM_HINT_NTA);

            ymm0 = _mm256_load_si256(sptr++);
            ymm1 = _mm256_load_si256(sptr++);
            ymm2 = _mm256_load_si256(sptr++);
            ymm3 = _mm256_load_si256(sptr++);
            ymm4 = _mm256_load_si256(sptr++);
            ymm5 = _mm256_load_si256(sptr++);
            ymm6 = _mm256_load_si256(sptr++);
            ymm7 = _mm256_load_si256(sptr++);

            _mm256_store_si256(dptr++, ymm0);
            _mm256_store_si256(dptr++, ymm1);
            _mm256_store_si256(dptr++, ymm2);
            _mm256_store_si256(dptr++, ymm3);
            _mm256_store_si256(dptr++, ymm4);
            _mm256_store_si256(dptr++, ymm5);
            _mm256_store_si256(dptr++, ymm6);
            _mm256_store_si256(dptr++, ymm7);
         }
         while(--i);
      }
   }

   if (num) {
      memcpy(d, s, num);
   }

   return dst;
}

#endif /* AVX */

