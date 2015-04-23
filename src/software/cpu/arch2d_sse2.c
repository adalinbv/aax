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

#include <math.h>	/* for floorf */


#include "software/rbuf_int.h"
#include "arch2d_simd.h"

#ifdef __SSE2__

#if defined(__x86_64__) || defined(_M_X64)
# define SSE_16BUFFERS 0
#else
# define SSE_16BUFFERS 0
#endif

# define CACHE_ADVANCE_IMADD     16
# define CACHE_ADVANCE_FMADD	 16
# define CACHE_ADVANCE_MUL	 32
# define CACHE_ADVANCE_CPY	 16
# define CACHE_ADVANCE_CVT	 32
# define CACHE_ADVANCE_INTL	 16
# define CACHE_ADVANCE_FF	 32


void
_batch_cvt24_ps_sse2(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *d = (int32_t*)dst;
   float *s = (float*)src;

   assert(((size_t)d & 0xF) == 0);
   assert(((size_t)s & 0xF) == 0);

   if (num)
   {
      __m128i *dptr = (__m128i*)d;
      __m128* sptr = (__m128*)s;
      size_t i, step;

#if !SSE_16BUFFERS
      step = 4*sizeof(__m128)/sizeof(float);
#else
      step = 8*sizeof(__m128)/sizeof(float);
#endif

      i = num/step;
      num -= i*step;
      if (i)
      {
         __m128i xmm4i, xmm5i, xmm6i, xmm7i;
#if SSE_16BUFFERS
         __m128i xmm12i, xmm13i, xmm14i, xmm15i;
         __m128 xmm8, xmm9, xmm10, xmm11;
#endif
         __m128 xmm0, xmm1, xmm2, xmm3;
         __m128 mul = _mm_set1_ps((float)(1<<23));
         do
         {
            _mm_prefetch(((char *)s)+CACHE_ADVANCE_CVT, _MM_HINT_NTA);

            xmm0 = _mm_load_ps((const float*)sptr++);
            xmm1 = _mm_load_ps((const float*)sptr++);
            xmm2 = _mm_load_ps((const float*)sptr++);
            xmm3 = _mm_load_ps((const float*)sptr++);
#if SSE_16BUFFERS
            xmm8 = _mm_load_ps((const float*)sptr++);
            xmm9 = _mm_load_ps((const float*)sptr++);
            xmm10 = _mm_load_ps((const float*)sptr++);
            xmm11 = _mm_load_ps((const float*)sptr++);
#endif

            xmm0 = _mm_mul_ps(xmm0, mul);
            xmm1 = _mm_mul_ps(xmm1, mul);
            xmm2 = _mm_mul_ps(xmm2, mul);
            xmm3 = _mm_mul_ps(xmm3, mul);
#if SSE_16BUFFERS
            xmm8 = _mm_mul_ps(xmm8, mul);
            xmm9 = _mm_mul_ps(xmm9, mul);
            xmm10 = _mm_mul_ps(xmm10, mul);
            xmm11 = _mm_mul_ps(xmm11, mul);
#endif

            xmm4i = _mm_cvtps_epi32(xmm0);
            xmm5i = _mm_cvtps_epi32(xmm1);
            xmm6i = _mm_cvtps_epi32(xmm2);
            xmm7i = _mm_cvtps_epi32(xmm3);
#if SSE_16BUFFERS
            xmm12i = _mm_cvtps_epi32(xmm8);
            xmm13i = _mm_cvtps_epi32(xmm9);
            xmm14i = _mm_cvtps_epi32(xmm10);
            xmm15i = _mm_cvtps_epi32(xmm11);
#endif

            d += step;
            s += step;

            _mm_store_si128(dptr++, xmm4i);
            _mm_store_si128(dptr++, xmm5i);
            _mm_store_si128(dptr++, xmm6i);
            _mm_store_si128(dptr++, xmm7i);
#if SSE_16BUFFERS
            _mm_store_si128(dptr++, xmm12i);
            _mm_store_si128(dptr++, xmm13i);
            _mm_store_si128(dptr++, xmm14i);
            _mm_store_si128(dptr++, xmm15i);
#endif
         }
         while(--i);
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
_batch_cvt24_ps24_sse2(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *d = (int32_t*)dst;
   float *s = (float*)src;
   size_t i, step;
   size_t dtmp, stmp;

   if (!num) return;

   dtmp = (size_t)d & 0xF;
   stmp = (size_t)s & 0xF;
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
      i = (0x10 - dtmp)/sizeof(int32_t);
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
      __m128i *dptr = (__m128i*)d;
      __m128* sptr = (__m128*)s;

#if !SSE_16BUFFERS
      step = 4*sizeof(__m128)/sizeof(float);
#else
      step = 8*sizeof(__m128)/sizeof(float);
#endif

      i = num/step;
      num -= i*step;
      if (i)
      {
         __m128i xmm4i, xmm5i, xmm6i, xmm7i;
#if SSE_16BUFFERS
         __m128i xmm12i, xmm13i, xmm14i, xmm15i;
         __m128 xmm8, xmm9, xmm10, xmm11;
#endif
         __m128 xmm0, xmm1, xmm2, xmm3;
         do
         {
            _mm_prefetch(((char *)s)+CACHE_ADVANCE_CVT, _MM_HINT_NTA);

            xmm0 = _mm_load_ps((const float*)sptr++);
            xmm1 = _mm_load_ps((const float*)sptr++);
            xmm2 = _mm_load_ps((const float*)sptr++);
            xmm3 = _mm_load_ps((const float*)sptr++);
#if SSE_16BUFFERS
            xmm8 = _mm_load_ps((const float*)sptr++);
            xmm9 = _mm_load_ps((const float*)sptr++);
            xmm10 = _mm_load_ps((const float*)sptr++);
            xmm11 = _mm_load_ps((const float*)sptr++);
#endif

            xmm4i = _mm_cvtps_epi32(xmm0);
            xmm5i = _mm_cvtps_epi32(xmm1);
            xmm6i = _mm_cvtps_epi32(xmm2);
            xmm7i = _mm_cvtps_epi32(xmm3);
#if SSE_16BUFFERS
            xmm12i = _mm_cvtps_epi32(xmm8);
            xmm13i = _mm_cvtps_epi32(xmm9);
            xmm14i = _mm_cvtps_epi32(xmm10);
            xmm15i = _mm_cvtps_epi32(xmm11);
#endif

            d += step;
            s += step;

            _mm_store_si128(dptr++, xmm4i);
            _mm_store_si128(dptr++, xmm5i);
            _mm_store_si128(dptr++, xmm6i);
            _mm_store_si128(dptr++, xmm7i);
#if SSE_16BUFFERS
            _mm_store_si128(dptr++, xmm12i);
            _mm_store_si128(dptr++, xmm13i);
            _mm_store_si128(dptr++, xmm14i);
            _mm_store_si128(dptr++, xmm15i);
#endif
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
_batch_cvtps_24_sse2(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *s = (int32_t*)src;
   float *d = (float*)dst;

   assert(((size_t)d & 0xF) == 0);
   assert(((size_t)s & 0xF) == 0);

   if (num)
   {
      __m128i* sptr = (__m128i*)s;
      __m128 *dptr = (__m128*)d;
      size_t i, step;

      step = 4*sizeof(__m128i)/sizeof(int32_t);

      i = num/step;
      num -= i*step;
      if (i)
      {
         __m128i xmm0i, xmm1i, xmm2i, xmm3i;
         __m128 xmm4, xmm5, xmm6, xmm7;
         __m128 mul = _mm_set1_ps(1.0f/(float)(1<<23));
         do
         {
            _mm_prefetch(((char *)s)+CACHE_ADVANCE_CVT, _MM_HINT_NTA);

            xmm0i = _mm_load_si128(sptr++);
            xmm1i = _mm_load_si128(sptr++);
            xmm2i = _mm_load_si128(sptr++);
            xmm3i = _mm_load_si128(sptr++);

            xmm4 = _mm_cvtepi32_ps(xmm0i);
            xmm5 = _mm_cvtepi32_ps(xmm1i);
            xmm6 = _mm_cvtepi32_ps(xmm2i);
            xmm7 = _mm_cvtepi32_ps(xmm3i);

            xmm4 = _mm_mul_ps(xmm4, mul);
            xmm5 = _mm_mul_ps(xmm5, mul);
            xmm6 = _mm_mul_ps(xmm6, mul);
            xmm7 = _mm_mul_ps(xmm7, mul);

            s += step;
            d += step;

            _mm_store_ps((float*)dptr++, xmm4);
            _mm_store_ps((float*)dptr++, xmm5);
            _mm_store_ps((float*)dptr++, xmm6);
            _mm_store_ps((float*)dptr++, xmm7);;
         }
         while(--i);
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
_batch_cvtps24_24_sse2(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *s = (int32_t*)src;
   float *d = (float*)dst;
   size_t i, step;
   size_t dtmp, stmp;

   assert(s != 0);
   assert(d != 0);

   if (!num) return;

   dtmp = (size_t)d & 0xF;
   stmp = (size_t)s & 0xF;
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
      i = (0x10 - dtmp)/sizeof(int32_t);
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
      __m128i* sptr = (__m128i*)s;
      __m128 *dptr = (__m128*)d;

#if !SSE_16BUFFERS
      step = 4*sizeof(__m128i)/sizeof(int32_t);
#else
      step = 8*sizeof(__m128i)/sizeof(int32_t);
#endif

      i = num/step;
      num -= i*step;
      if (i)
      {
         __m128i xmm0i, xmm1i, xmm2i, xmm3i;
#if SSE_16BUFFERS
         __m128i xmm8i, xmm9i, xmm10i, xmm11i;
         __m128 xmm12, xmm13, xmm14, xmm15;
#endif
         __m128 xmm4, xmm5, xmm6, xmm7;
         do
         {
            _mm_prefetch(((char *)s)+CACHE_ADVANCE_CVT, _MM_HINT_NTA);

            xmm0i = _mm_load_si128(sptr++);
            xmm1i = _mm_load_si128(sptr++);
            xmm2i = _mm_load_si128(sptr++);
            xmm3i = _mm_load_si128(sptr++);
#if SSE_16BUFFERS
            xmm8i = _mm_load_si128(sptr++);
            xmm9i = _mm_load_si128(sptr++);
            xmm10i = _mm_load_si128(sptr++);
            xmm11i = _mm_load_si128(sptr++);
#endif

            xmm4 = _mm_cvtepi32_ps(xmm0i);
            xmm5 = _mm_cvtepi32_ps(xmm1i);
            xmm6 = _mm_cvtepi32_ps(xmm2i);
            xmm7 = _mm_cvtepi32_ps(xmm3i);
#if SSE_16BUFFERS
            xmm12 = _mm_cvtepi32_ps(xmm8i);
            xmm13 = _mm_cvtepi32_ps(xmm9i);
            xmm14 = _mm_cvtepi32_ps(xmm10i);
            xmm15 = _mm_cvtepi32_ps(xmm11i);
#endif

            s += step;
            d += step;

            _mm_store_ps((float*)dptr++, xmm4);
            _mm_store_ps((float*)dptr++, xmm5);
            _mm_store_ps((float*)dptr++, xmm6);
            _mm_store_ps((float*)dptr++, xmm7);
#if SSE_16BUFFERS
            _mm_store_ps((float*)dptr++, xmm12);
            _mm_store_ps((float*)dptr++, xmm13);
            _mm_store_ps((float*)dptr++, xmm14);
            _mm_store_ps((float*)dptr++, xmm15);
#endif
         }
         while(--i);
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

void
_batch_imadd_sse2(int32_ptr dst, const_int32_ptr src, size_t num, float v, float vstep)
{
   int32_ptr d = (int32_ptr)dst;
   int32_ptr s = (int32_ptr)src;
   size_t i, step;
   size_t dtmp, stmp;

   if (!num) return;

   dtmp = (size_t)d & 0xF;
   stmp = (size_t)s & 0xF;
   if ((dtmp || stmp) && dtmp != stmp)	/* improperly aligned,            */
   {					/* let the compiler figure it out */
      i = num;
      do
      {
         *d++ += (int32_t)((float)*s++ * v);
         v += vstep;
      }
      while (--i);
      return;
   }

   /* work towards a 16-byte aligned d (and hence 16-byte aligned sptr) */
   if (dtmp && num)
   {
      i = (0x10 - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do
         {
            *d++ += (int32_t)((float)*s++ * v);
            v += vstep;
         } while(--i);
      }
   }

   step = 2*sizeof(__m128i)/sizeof(int32_t);

   vstep *= step;				/* 8 samples at a time */
   i = num/step;
   num -= i*step;
   if (i)
   {
      __m128i *sptr = (__m128i *)s;
      __m128i *dptr = (__m128i *)d;
      __m128 tv = _mm_set1_ps(v);
      __m128i xmm0i, xmm3i, xmm4i, xmm7i;
      __m128 xmm1, xmm5;

      do
      {
         _mm_prefetch(((char *)sptr)+CACHE_ADVANCE_IMADD, _MM_HINT_NTA);
         xmm0i = _mm_load_si128(sptr++);
         xmm4i = _mm_load_si128(sptr++);

         xmm1 = _mm_cvtepi32_ps(xmm0i);
         xmm5 = _mm_cvtepi32_ps(xmm4i);

         s += step;

         xmm1 = _mm_mul_ps(xmm1, tv);
         xmm5 = _mm_mul_ps(xmm5, tv);

         xmm0i = _mm_load_si128(dptr);
         xmm4i = _mm_load_si128(dptr+1);

         xmm3i = _mm_cvtps_epi32(xmm1);
         xmm7i = _mm_cvtps_epi32(xmm5);

         d += step;

         xmm0i = _mm_add_epi32(xmm0i, xmm3i);
         xmm4i = _mm_add_epi32(xmm4i, xmm7i);

         v += vstep;

         _mm_store_si128(dptr++, xmm0i);
         _mm_store_si128(dptr++, xmm4i);

         tv = _mm_set1_ps(v);
      }
      while(--i);
   }

   if (num)
   {
      vstep /= step;
      i = num;
      do {
         *d++ += (int32_t)((float)*s++ * v);
         v += vstep;
      } while(--i);
   }
}

void
_batch_fmadd_sse2(float32_ptr dst, const_float32_ptr src, size_t num, float v, float vstep)
{
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step;
   size_t dtmp, stmp;

   if (!num) return;

   dtmp = (size_t)d & 0xF;
   stmp = (size_t)s & 0xF;
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
      i = (0x10 - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ += *s++ * v;
            v += vstep;
         } while(--i);
      }
   }

#if !SSE_16BUFFERS
   step = 4*sizeof(__m128)/sizeof(float);
#else
   step = 8*sizeof(__m128)/sizeof(float);
#endif

   vstep *= step;				/* 8 samples at a time */
   i = num/step;
   num -= i*step;
   if (i)
   {
      __m128* sptr = (__m128*)s;
      __m128 *dptr = (__m128*)d;
      __m128 xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
#if SSE_16BUFFERS
      __m128 xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15;
#endif
      __m128 tv = _mm_set1_ps(v);

      do
      {
         _mm_prefetch(((char *)s)+CACHE_ADVANCE_FMADD, _MM_HINT_NTA);
         _mm_prefetch(((char *)d)+CACHE_ADVANCE_FMADD, _MM_HINT_NTA);

         xmm0 = _mm_load_ps((const float*)sptr++);
         xmm1 = _mm_load_ps((const float*)sptr++);
         xmm4 = _mm_load_ps((const float*)sptr++);
         xmm5 = _mm_load_ps((const float*)sptr++);
#if SSE_16BUFFERS
         xmm10 = _mm_load_ps((const float*)sptr++);
         xmm11 = _mm_load_ps((const float*)sptr++);
         xmm14 = _mm_load_ps((const float*)sptr++);
         xmm15 = _mm_load_ps((const float*)sptr++);
#endif

         xmm0 = _mm_mul_ps(xmm0, tv);
         xmm1 = _mm_mul_ps(xmm1, tv);
         xmm4 = _mm_mul_ps(xmm4, tv);
         xmm5 = _mm_mul_ps(xmm5, tv);
#if SSE_16BUFFERS
         xmm10 = _mm_mul_ps(xmm10, tv);
         xmm11 = _mm_mul_ps(xmm11, tv);
         xmm14 = _mm_mul_ps(xmm14, tv);
         xmm15 = _mm_mul_ps(xmm15, tv);
#endif

         s += step;
         d += step;

         xmm2 = _mm_load_ps((const float*)dptr);
         xmm3 = _mm_load_ps((const float*)(dptr+1));
         xmm6 = _mm_load_ps((const float*)(dptr+2));
         xmm7 = _mm_load_ps((const float*)(dptr+3));
#if SSE_16BUFFERS
         xmm8 = _mm_load_ps((const float*)dptr+4);
         xmm9 = _mm_load_ps((const float*)(dptr+5));
         xmm12 = _mm_load_ps((const float*)(dptr+6));
         xmm13 = _mm_load_ps((const float*)(dptr+7));
#endif

         xmm2 = _mm_add_ps(xmm2, xmm0);
         xmm3 = _mm_add_ps(xmm3, xmm1);
         xmm6 = _mm_add_ps(xmm6, xmm4);
         xmm7 = _mm_add_ps(xmm7, xmm5);
#if SSE_16BUFFERS
         xmm8 = _mm_add_ps(xmm8, xmm10);
         xmm9 = _mm_add_ps(xmm9, xmm11);
         xmm12 = _mm_add_ps(xmm12, xmm14);
         xmm13 = _mm_add_ps(xmm13, xmm15);
#endif

         v += vstep;

         _mm_store_ps((float*)dptr++, xmm2);
         _mm_store_ps((float*)dptr++, xmm3);
         _mm_store_ps((float*)dptr++, xmm6);
         _mm_store_ps((float*)dptr++, xmm7);
#if SSE_16BUFFERS
         _mm_store_ps((float*)dptr++, xmm8);
         _mm_store_ps((float*)dptr++, xmm9);
         _mm_store_ps((float*)dptr++, xmm12);
         _mm_store_ps((float*)dptr++, xmm13);
#endif

         tv = _mm_set1_ps(v);
      }
      while(--i);
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


void
_batch_cvt24_16_sse2(void_ptr dst, const_void_ptr src, size_t num)
{
   int16_t *s = (int16_t *)src;
   int32_t *d = (int32_t*)dst;
   size_t i, step;
   size_t tmp;

   if (!num) return;

   /*
    * work towards 16-byte aligned d
    */
   tmp = (size_t)d & 0xF;
   if (tmp && num)
   {
      i = (0x10 - tmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ = *s++ << 8;
         } while(--i);
      }
   }

   step = 2*sizeof(__m128i)/sizeof(int16_t);

   tmp = (size_t)s & 0xF;
   i = num/step;
   num -= i*step;
   if (i)
   {
      __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i zero = _mm_setzero_si128();
      __m128i *dptr = (__m128i *)d;
      __m128i *sptr = (__m128i *)s;

      do {
         _mm_prefetch(((char *)s)+CACHE_ADVANCE_CVT, _MM_HINT_NTA);

         if (tmp)
         {
            xmm0 = _mm_loadu_si128(sptr++);
            xmm4 = _mm_loadu_si128(sptr++);

            xmm1 = _mm_unpacklo_epi16(zero, xmm0);
            xmm3 = _mm_unpackhi_epi16(zero, xmm0);
            xmm5 = _mm_unpacklo_epi16(zero, xmm4);
            xmm7 = _mm_unpackhi_epi16(zero, xmm4);
         }
         else
         {
            xmm0 = _mm_load_si128(sptr++);
            xmm4 = _mm_load_si128(sptr++);

            xmm1 = _mm_unpacklo_epi16(zero, xmm0);
            xmm3 = _mm_unpackhi_epi16(zero, xmm0);
            xmm5 = _mm_unpacklo_epi16(zero, xmm4);
            xmm7 = _mm_unpackhi_epi16(zero, xmm4);
         }

         s += step;
         d += step;

         xmm0 = _mm_srai_epi32(xmm1, 8);
         xmm2 = _mm_srai_epi32(xmm3, 8);
         xmm4 = _mm_srai_epi32(xmm5, 8);
         xmm6 = _mm_srai_epi32(xmm7, 8);

         _mm_store_si128(dptr++, xmm0);
         _mm_store_si128(dptr++, xmm2);
         _mm_store_si128(dptr++, xmm4);
         _mm_store_si128(dptr++, xmm6);
      }
      while (--i);
   }

   if (num)
   {
      i = num;
      do {
         *d++ = *s++ << 8;
      } while (--i);
   }
}

void
_batch_cvt16_24_sse2(void_ptr dst, const_void_ptr src, size_t num)
{
   size_t i, step;
   int32_t* s = (int32_t*)src;
   int16_t* d = (int16_t*)dst;
   size_t tmp;

   /*
    * work towards 16-byte aligned sptr
    */
   tmp = (size_t)s & 0xF;
   if (tmp && num)
   {
      i = (0x10 - tmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ = *s++ >> 8;
         } while(--i);
      }
   }

   assert(((size_t)s & 0xF) == 0);
   tmp = (size_t)d & 0xF;

   step = 4*sizeof(__m128i)/sizeof(int32_t);

   i = num/step;
   num -= i*step;
   if (i)
   {
      __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i *dptr, *sptr;

      dptr = (__m128i *)d;
      sptr = (__m128i *)s;
      do
      {
         _mm_prefetch(((char *)s)+CACHE_ADVANCE_CVT, _MM_HINT_NTA);

         xmm0 = _mm_load_si128(sptr++);
         xmm1 = _mm_load_si128(sptr++);
         xmm4 = _mm_load_si128(sptr++);
         xmm5 = _mm_load_si128(sptr++);

         xmm2 = _mm_srai_epi32(xmm0, 8);
         xmm3 = _mm_srai_epi32(xmm1, 8);
         xmm6 = _mm_srai_epi32(xmm4, 8);
         xmm7 = _mm_srai_epi32(xmm5, 8);

         s += step;

         xmm4 = _mm_packs_epi32(xmm2, xmm3);
         xmm6 = _mm_packs_epi32(xmm6, xmm7);

         d += step;

         if (tmp) {
            _mm_storeu_si128(dptr++, xmm4);
            _mm_storeu_si128(dptr++, xmm6);
         } else {
            _mm_store_si128(dptr++, xmm4);
            _mm_store_si128(dptr++, xmm6);
         }
      }
      while (--i);
   }

   if (num)
   {
      i = num;
      do {
         *d++ = *s++ >> 8;
      } while (--i);
   }
}

void
_batch_cvt16_intl_24_sse2(void_ptr dst, const_int32_ptrptr src,
                                size_t offset, unsigned int tracks,
                                size_t num)
{
   size_t i, step;
   int16_t *d = (int16_t*)dst;
   int32_t *s1, *s2;
   size_t tmp;

   if (!num) return;

   if (tracks != 2)
   {
      size_t t;
      for (t=0; t<tracks; t++)
      {
         int32_t *s = (int32_t *)src[t] + offset;
         int16_t *d = (int16_t *)dst + t;
         size_t i = num;

         do
         {
            *d = *s++ >> 8;
            d += tracks;
         }
         while (--i);
      }
      return;
   }

   s1 = (int32_t *)src[0] + offset;
   s2 = (int32_t *)src[1] + offset;

   step = 2*sizeof(__m128i)/sizeof(int32_t);

   /*
    * work towards 16-byte aligned sptr
    */
   tmp = (size_t)s1 & 0xF;
   assert(tmp == ((size_t)s2 & 0xF));

   i = num/step;
   if (tmp && i)
   {
      i = (0x10 - tmp)/sizeof(int32_t);
      num -= i;
      do
      {
         *d++ = *s1++ >> 8;
         *d++ = *s2++ >> 8;
      }
      while (--i);
   }

   tmp = (size_t)d & 0xF;
   i = num/step;
   num -= i*step;
   if (i)
   {
      __m128i mask, xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i *dptr, *sptr1, *sptr2;

      sptr1 = (__m128i*)s1;
      sptr2 = (__m128i*)s2;

      mask = _mm_set_epi32(0x00FFFF00, 0x00FFFF00, 0x00FFFF00, 0x00FFFF00);
      dptr = (__m128i *)d;
      do
      {
         _mm_prefetch(((char *)s1)+CACHE_ADVANCE_INTL, _MM_HINT_NTA);
         _mm_prefetch(((char *)s2)+CACHE_ADVANCE_INTL, _MM_HINT_NTA);

         xmm0 = _mm_load_si128(sptr1++);
         xmm4 = _mm_load_si128(sptr1++);
         xmm1 = _mm_load_si128(sptr2++);
         xmm5 = _mm_load_si128(sptr2++);

         xmm2 = _mm_and_si128(xmm0, mask);
         xmm3 = _mm_and_si128(xmm1, mask);
         xmm6 = _mm_and_si128(xmm4, mask);
         xmm7 = _mm_and_si128(xmm5, mask);

         s1 += step;
         s2 += step;

         xmm0 = _mm_srli_epi32(xmm2, 8);
         xmm1 = _mm_slli_epi32(xmm3, 8);
         xmm4 = _mm_srli_epi32(xmm6, 8);
         xmm5 = _mm_slli_epi32(xmm7, 8);

         xmm0 = _mm_or_si128(xmm1, xmm0);
         xmm4 = _mm_or_si128(xmm5, xmm4);

         d += 2*step;

         if (tmp) {
            _mm_storeu_si128(dptr++, xmm0);
            _mm_storeu_si128(dptr++, xmm4);
         } else {
            _mm_store_si128(dptr++, xmm0);
            _mm_store_si128(dptr++, xmm4);
         }
      } while (--i);
   }

   if (num)
   {
      i = num;
      do
      {
         *d++ = *s1++ >> 8;
         *d++ = *s2++ >> 8;
      }
      while (--i);
   }
}


void
_batch_freqfilter_sse2(int32_ptr d, const_int32_ptr sptr, size_t num,
                  float *hist, float k, const float *cptr)
{
   int32_ptr s = (int32_ptr)sptr;

   if (num)
   {
      __m128 c, h, mk = _mm_set_ss(k);
      size_t i = num;

//    c = _mm_set_ps(cptr[3], cptr[1], cptr[2], cptr[0]);
      c = _mm_load_ps(cptr);
      c = _mm_shuffle_ps(c, c, _MM_SHUFFLE(3,1,2,0));

//    h = _mm_set_ps(hist[1], hist[1], hist[0], hist[0]);
      h = _mm_loadl_pi(_mm_setzero_ps(), (__m64*)hist);
      h = _mm_shuffle_ps(h, h, _MM_SHUFFLE(1,1,0,0));

      do
      {
         __m128 pz, smp, nsmp, tmp;

         smp = _mm_cvtepi32_ps((__m128i)_mm_load_ss((const float*)s));

         // pz = { c[3]*h1, -c[1]*h1, c[2]*h0, -c[0]*h0 };
         pz = _mm_mul_ps(c, h); // poles and zeros

         // smp = *s++ * k;
         smp = _mm_mul_ss(smp, mk);

         // tmp[0] = -c[0]*h0 + -c[1]*h1;
         tmp = _mm_add_ps(pz, _mm_shuffle_ps(pz, pz, _MM_SHUFFLE(1,3,0,2)));
         s++;

         // nsmp = smp - h0*c[0] - h1*c[1];
         nsmp = _mm_add_ss(smp, tmp);

         // h1 = h0, h0 = smp: h = { h0, h0, smp, smp };
         h = _mm_shuffle_ps(nsmp, h, _MM_SHUFFLE(0,0,0,0));

         // tmp[0] = -c[0]*h0 + -c[1]*h1 + c[2]*h0 + c[3]*h1;
         tmp = _mm_add_ps(tmp, _mm_shuffle_ps(tmp, tmp, _MM_SHUFFLE(0,1,2,3)));

         // smp = smp - h0*c[0] - h1*c[1] + h0*c[2] + h1*c[3];
         smp = _mm_add_ss(smp, tmp);
         _mm_store_ss((float*)d++, (__m128)_mm_cvtps_epi32(smp));
      }
      while (--i);

      _mm_storel_pi((__m64*)hist, h);
   }
}


void
_batch_freqfilter_float_sse2(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float k, const float *cptr)
{
   float32_ptr s = (float32_ptr)sptr;

   if (num)
   {
      __m128 c, h, mk = _mm_set_ss(k);
      size_t i = num;

//    c = _mm_set_ps(cptr[3], cptr[1], cptr[2], cptr[0]);
      c = _mm_load_ps(cptr);
      c = _mm_shuffle_ps(c, c, _MM_SHUFFLE(3,1,2,0));

//    h = _mm_set_ps(hist[1], hist[1], hist[0], hist[0]);
      h = _mm_loadl_pi(_mm_setzero_ps(), (__m64*)hist);
      h = _mm_shuffle_ps(h, h, _MM_SHUFFLE(1,1,0,0));

      do
      {     
         __m128 pz, smp, nsmp, tmp;

         smp = _mm_load_ss(s);

         // pz = { c[3]*h1, -c[1]*h1, c[2]*h0, -c[0]*h0 };
         pz = _mm_mul_ps(c, h); // poles and zeros

         // smp = *s++ * k;
         smp = _mm_mul_ss(smp, mk);

         // tmp[0] = -c[0]*h0 + -c[1]*h1;
         tmp = _mm_add_ps(pz, _mm_shuffle_ps(pz, pz, _MM_SHUFFLE(1,3,0,2)));
         s++;

         // nsmp = smp - h0*c[0] - h1*c[1];
         nsmp = _mm_add_ss(smp, tmp);

         // h1 = h0, h0 = smp: h = { h0, h0, smp, smp };
         h = _mm_shuffle_ps(nsmp, h, _MM_SHUFFLE(0,0,0,0));

         // tmp[0] = -c[0]*h0 + -c[1]*h1 + c[2]*h0 + c[3]*h1;
         tmp = _mm_add_ps(tmp, _mm_shuffle_ps(tmp, tmp, _MM_SHUFFLE(0,1,2,3)));

         // smp = smp - h0*c[0] - h1*c[1] + h0*c[2] + h1*c[3];
         smp = _mm_add_ss(smp, tmp);
         _mm_store_ss(d++, smp);
      }
      while (--i);

      _mm_storel_pi((__m64*)hist, h);
   }
}


/*
 * optimized memcpy for 16-byte aligned destination buffer
 * fall back tobuildt-in  memcpy otherwise.
 */
void *
_aax_memcpy_sse2(void_ptr dst, const_void_ptr src, size_t num)
{
   size_t i, step;
   char *d = (char*)dst;
   char *s = (char*)src;
   size_t tmp;

   if (!num) return dst;

   /*
    * work towards a 16-byte aligned dptr and possibly sptr
    */
   tmp = (size_t)d & 0xF;
   if (tmp)
   {
      i = (0x10 - tmp);
      num -= i;

      memcpy(d, s, i);
      d += i;
      s += i;
   }

   tmp = (size_t)s & 0xF;
   step = 8*sizeof(__m128i)/sizeof(int8_t);

   i = num/step;
   num -= i*step;
   if (i)
   {
      __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i *sptr = (__m128i*)s;
      __m128i *dptr = (__m128i*)d;

      s += i*step;
      d += i*step;

      if (tmp)
      {
         do
         {
            _mm_prefetch(((char *)s)+CACHE_ADVANCE_CPY, _MM_HINT_NTA);

            xmm0 = _mm_loadu_si128(sptr++);
            xmm1 = _mm_loadu_si128(sptr++);
            xmm2 = _mm_loadu_si128(sptr++);
            xmm3 = _mm_loadu_si128(sptr++);
            xmm4 = _mm_loadu_si128(sptr++);
            xmm5 = _mm_loadu_si128(sptr++);
            xmm6 = _mm_loadu_si128(sptr++);
            xmm7 = _mm_loadu_si128(sptr++);

            _mm_store_si128(dptr++, xmm0);
            _mm_store_si128(dptr++, xmm1);
            _mm_store_si128(dptr++, xmm2);
            _mm_store_si128(dptr++, xmm3);
            _mm_store_si128(dptr++, xmm4);
            _mm_store_si128(dptr++, xmm5);
            _mm_store_si128(dptr++, xmm6);
            _mm_store_si128(dptr++, xmm7);
         }
         while(--i);
      }
      else	/* both buffers are 16-byte aligned */
      {
         do
         {
            _mm_prefetch(((char *)s)+CACHE_ADVANCE_CPY, _MM_HINT_NTA);

            xmm0 = _mm_load_si128(sptr++);
            xmm1 = _mm_load_si128(sptr++);
            xmm2 = _mm_load_si128(sptr++);
            xmm3 = _mm_load_si128(sptr++);
            xmm4 = _mm_load_si128(sptr++);
            xmm5 = _mm_load_si128(sptr++);
            xmm6 = _mm_load_si128(sptr++);
            xmm7 = _mm_load_si128(sptr++);

            _mm_store_si128(dptr++, xmm0);
            _mm_store_si128(dptr++, xmm1);
            _mm_store_si128(dptr++, xmm2);
            _mm_store_si128(dptr++, xmm3);
            _mm_store_si128(dptr++, xmm4);
            _mm_store_si128(dptr++, xmm5);
            _mm_store_si128(dptr++, xmm6);
            _mm_store_si128(dptr++, xmm7);
         }
         while(--i);
      }
   }

   if (num) {
      memcpy(d, s, num);
   }

   return dst;
}

#if !RB_FLOAT_DATA
static inline void
_aaxBufResampleSkip_sse2(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   int32_t samp, dsamp;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor >= 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   dptr += dmin;

   samp = *sptr++;              // n+(step-1)
   dsamp = *sptr - samp;        // (n+1) - n


   i=dmax-dmin;
   if (i)
   {
      do
      {
         size_t step;

         *dptr++ = samp + (int32_t)(dsamp * smu);

         smu += freq_factor;
         step = (size_t)floorf(smu);

         smu -= step;
         sptr += step-1;
         samp = *sptr++;
         dsamp = *sptr - samp;
      }
      while (--i);
   }
}

static inline void
_aaxBufResampleNearest_sse2(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   if (freq_factor == 1.0f) {
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(int32_t));
   }
   else
   {
      int32_ptr sptr = (int32_ptr)s;
      int32_ptr dptr = d;
      size_t i;

      assert(s != 0);
      assert(d != 0);
      assert(dmin < dmax);
      assert(0.95f <= freq_factor && freq_factor <= 1.05f);
      assert(0.0f <= smu && smu < 1.0f);

      dptr += dmin;

      i = dmax-dmin;
      if (i)
      {
         do
         {
            *dptr++ = *sptr;

            smu += freq_factor;
            if (smu > 0.5f)
            {
               sptr++;
               smu -= 1.0f;
            }
         }
         while (--i);
      }
   }
}

static inline void
_aaxBufResampleLinear_sse2(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   int32_t samp, dsamp;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor < 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   dptr += dmin;

   samp = *sptr++;		// n
   dsamp = *sptr - samp;	// (n+1) - n

   i = dmax-dmin;
   if (i)
   {
      do
      {
         *dptr++ = samp + (int32_t)(dsamp * smu);

         smu += freq_factor;
         if (smu >= 1.0)
         {
            smu -= 1.0;
            samp = *sptr++;
            dsamp = *sptr - samp;
         }
      }
      while (--i);
   }
}

static inline void
_aaxBufResampleCubic_sse2(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float y0, y1, y2, y3, a0, a1, a2;
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

   dptr += dmin;

   y0 = (float)*sptr++;
   y1 = (float)*sptr++;
   y2 = (float)*sptr++;
   y3 = (float)*sptr++;

   a0 = y3 - y2 - y0 + y1;
   a1 = y0 - y1 - a0;
   a2 = y2 - y0;

   i = dmax-dmin;
   if (i)
   {
      do
      {
         float smu2, ftmp;

         smu2 = smu*smu;
         ftmp = (a0*smu*smu2 + a1*smu2 + a2*smu + y1);
         *dptr++ = (int32_t)ftmp;

         smu += freq_factor;
         if (smu >= 1.0)
         {
            smu--;
            a0 += y0;
            y0 = y1;
            y1 = y2;
            y2 = y3;
            y3 = (float)*sptr++;
            a0 = -a0 + y3;			/* a0 = y3 - y2 - y0 + y1; */
            a1 = y0 - y1 - a0;
            a2 = y2 - y0;
         }
      }
      while (--i);
   }
}

void
_batch_resample_sse2(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_sse2(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_sse2(d, s, dmin, dmax, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleSkip_sse2(d, s, dmin, dmax, smu, fact);
   } else {
      _aaxBufResampleNearest_sse2(d, s, dmin, dmax, smu, fact);
   }
}
#else

static inline void
_aaxBufResampleSkip_float_sse2(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float32_ptr s = (float32_ptr)sptr;
   float32_ptr d = dptr;
   float samp, dsamp;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor >= 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   d += dmin;

   samp = *s++;			// n+(step-1)
   dsamp = *s - samp;		// (n+1) - n

   i = dmax-dmin;
   if (i)
   {
      do
      {
         size_t step;

         *d++ = samp + (dsamp * smu);

         smu += freq_factor;
         step = (size_t)floorf(smu);

         smu -= step;
         s += step-1;
         samp = *s++;
         dsamp = *s - samp;
      }
      while (--i);
   }
}

static inline void
_aaxBufResampleNearest_float_sse2(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   if (freq_factor == 1.0f) {
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(float));
   }
   else
   {
      float32_ptr sptr = (float32_ptr)s;
      float32_ptr dptr = d;
      size_t i;

      assert(s != 0);
      assert(d != 0);
      assert(dmin < dmax);
      assert(0.95f <= freq_factor && freq_factor <= 1.05f);
      assert(0.0f <= smu && smu < 1.0f);

      dptr += dmin;

      i = dmax-dmin;
      if (i)
      {
         do
         {
            *dptr++ = *sptr;

            smu += freq_factor;
            if (smu > 0.5f)
            {
               sptr++;
               smu -= 1.0f;
            }
         }
         while (--i);
      }
   }
}

static inline void
_aaxBufResampleLinear_float_sse2(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float32_ptr sptr = (float32_ptr)s;
   float32_ptr dptr = d;
   float samp, dsamp;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor < 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   dptr += dmin;

   samp = *sptr++;              // n
   dsamp = *sptr - samp;        // (n+1) - n

   i = dmax-dmin;
   if (i)
   {
      do
      {
         *dptr++ = samp + (dsamp * smu);

         smu += freq_factor;
         if (smu >= 1.0)
         {
            smu -= 1.0;
            samp = *sptr++;
            dsamp = *sptr - samp;
         }
      }
      while (--i);
   }
}

static inline void
_aaxBufResampleCubic_float_sse2(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float y0, y1, y2, y3, a0, a1, a2;
   float32_ptr sptr = (float32_ptr)s;
   float32_ptr dptr = d;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

   dptr += dmin;

   y0 = *sptr++;
   y1 = *sptr++;
   y2 = *sptr++;
   y3 = *sptr++;

   a0 = y3 - y2 - y0 + y1;
   a1 = y0 - y1 - a0;
   a2 = y2 - y0;

   i = dmax-dmin;
   if (i)
   {
      do
      {
         float smu2, ftmp;

         smu2 = smu*smu;
         ftmp = (a0*smu*smu2 + a1*smu2 + a2*smu + y1);
         *dptr++ = ftmp;

         smu += freq_factor;
         if (smu >= 1.0)
         {
            smu--;
            a0 += y0;
            y0 = y1;
            y1 = y2;
            y2 = y3;
            y3 = *sptr++;
            a0 = -a0 + y3;                      /* a0 = y3 - y2 - y0 + y1; */
            a1 = y0 - y1 - a0;
            a2 = y2 - y0;
         }
      }
      while (--i);
   }
}

void
_batch_resample_float_sse2(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_float_sse2(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_float_sse2(d, s, dmin, dmax, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleSkip_float_sse2(d, s, dmin, dmax, smu, fact);
   } else {
      _aaxBufResampleNearest_float_sse2(d, s, dmin, dmax, smu, fact);
   }
}
#endif // RB_FLOAT_DATA

#endif /* SSE2 */

