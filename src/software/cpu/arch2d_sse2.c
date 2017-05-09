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

#ifdef __SSE2__


void
_batch_get_average_rms_sse2(const_float32_ptr data, size_t num, float *rms, float *peak, float *pct_silence)
{
   if (num)
   {
      float threshold = LEVEL_96DB;
      double rms_total = 0.0;
      float peak_cur = 0.0f;
      double silence = 0.0f;

      size_t j = num;
      do
      {
         float samp = *data++;            // rms
         float val = samp*samp;
         rms_total += val;
         if (fabsf(samp) < threshold) silence += (1.0/num);
         else if (val > peak_cur) peak_cur = val;
      }
      while (--j);

      *rms = sqrt(rms_total/num);
      *peak = sqrtf(peak_cur);
      *pct_silence = silence;
   }
   else { 
      *rms = *peak = *pct_silence = 0;
   }
}

void
_batch_saturate24_sse2(void *data, size_t num)
{
   if (num)
   {
      int32_t* p = (int32_t*)data;
      size_t i = num;
      do
      {
         int32_t samp = _MINMAX(*p, -8388607, 8388607);
         *p++ = samp;
      }
      while(--i);
   }
}

void
_batch_cvt24_ps_sse2(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *d = (int32_t*)dst;
   float *s = (float*)src;

   if (((size_t)d & MEMMASK16) != 0 || ((size_t)s & MEMMASK16) != 0)
   {
      float mul = (float)(1<<23);
      size_t i = num;
      do {
         *d++ = (int32_t)(*s++ * mul);
      } while (--i);
      return;
   }

   assert(((size_t)d & MEMMASK16) == 0);
   assert(((size_t)s & MEMMASK16) == 0);

   if (num)
   {
      __m128i *dptr = (__m128i*)d;
      __m128* sptr = (__m128*)s;
      size_t i, step;

      step = 4*sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m128i xmm4i, xmm5i, xmm6i, xmm7i;
         __m128 xmm0, xmm1, xmm2, xmm3;
         __m128 mul = _mm_set1_ps((float)(1<<23));

         num -= i*step;
         d += i*step;
         s += i*step;
         do
         {
            xmm0 = _mm_load_ps((const float*)sptr++);
            xmm1 = _mm_load_ps((const float*)sptr++);
            xmm2 = _mm_load_ps((const float*)sptr++);
            xmm3 = _mm_load_ps((const float*)sptr++);

            xmm0 = _mm_mul_ps(xmm0, mul);
            xmm1 = _mm_mul_ps(xmm1, mul);
            xmm2 = _mm_mul_ps(xmm2, mul);
            xmm3 = _mm_mul_ps(xmm3, mul);

            xmm4i = _mm_cvtps_epi32(xmm0);
            xmm5i = _mm_cvtps_epi32(xmm1);
            xmm6i = _mm_cvtps_epi32(xmm2);
            xmm7i = _mm_cvtps_epi32(xmm3);

            _mm_store_si128(dptr++, xmm4i);
            _mm_store_si128(dptr++, xmm5i);
            _mm_store_si128(dptr++, xmm6i);
            _mm_store_si128(dptr++, xmm7i);
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

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
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
      __m128i *dptr = (__m128i*)d;
      __m128* sptr = (__m128*)s;

      step = 4*sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m128i xmm4i, xmm5i, xmm6i, xmm7i;
         __m128 xmm0, xmm1, xmm2, xmm3;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xmm0 = _mm_load_ps((const float*)sptr++);
            xmm1 = _mm_load_ps((const float*)sptr++);
            xmm2 = _mm_load_ps((const float*)sptr++);
            xmm3 = _mm_load_ps((const float*)sptr++);

            xmm4i = _mm_cvtps_epi32(xmm0);
            xmm5i = _mm_cvtps_epi32(xmm1);
            xmm6i = _mm_cvtps_epi32(xmm2);
            xmm7i = _mm_cvtps_epi32(xmm3);

            _mm_store_si128(dptr++, xmm4i);
            _mm_store_si128(dptr++, xmm5i);
            _mm_store_si128(dptr++, xmm6i);
            _mm_store_si128(dptr++, xmm7i);
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

   assert(((size_t)d & MEMMASK16) == 0);
   assert(((size_t)s & MEMMASK16) == 0);

   if (num)
   {
      __m128i* sptr = (__m128i*)s;
      __m128 *dptr = (__m128*)d;
      size_t i, step;

      step = 4*sizeof(__m128i)/sizeof(int32_t);

      i = num/step;
      if (i)
      {
         __m128i xmm0i, xmm1i, xmm2i, xmm3i;
         __m128 xmm4, xmm5, xmm6, xmm7;
         __m128 mul = _mm_set1_ps(1.0f/(float)(1<<23));

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
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

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
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
      i = (MEMALIGN16 - dtmp)/sizeof(int32_t);
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

      step = 4*sizeof(__m128i)/sizeof(int32_t);

      i = num/step;
      if (i)
      {
         __m128i xmm0i, xmm1i, xmm2i, xmm3i;
         __m128 xmm4, xmm5, xmm6, xmm7;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xmm0i = _mm_load_si128(sptr++);
            xmm1i = _mm_load_si128(sptr++);
            xmm2i = _mm_load_si128(sptr++);
            xmm3i = _mm_load_si128(sptr++);

            xmm4 = _mm_cvtepi32_ps(xmm0i);
            xmm5 = _mm_cvtepi32_ps(xmm1i);
            xmm6 = _mm_cvtepi32_ps(xmm2i);
            xmm7 = _mm_cvtepi32_ps(xmm3i);

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
            *d++ = (float)*s++;
         } while (--i);
      }
   }
}

static void
_batch_iadd_sse2(int32_ptr dst, const_int32_ptr src, size_t num)
{
   int32_ptr d = (int32_ptr)dst;
   int32_ptr s = (int32_ptr)src;
   size_t i, step, dtmp, stmp;

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if ((dtmp || stmp) && dtmp != stmp)  /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      i = num;
      do {
         *d++ += *s++;
      } while (--i);
      return;
   }

   /* work towards a 16-byte aligned d (and hence 16-byte aligned sptr) */
   if (dtmp && num)
   {
      i = (MEMALIGN16 - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do
         {
            *d++ += *s++;
         } while(--i);
      }
   }

   step = 2*sizeof(__m128i)/sizeof(int32_t);

   i = num/step;
   if (i)
   {
      __m128i *sptr = (__m128i *)s;
      __m128i *dptr = (__m128i *)d;
      __m128i xmm0i, xmm3i, xmm4i, xmm7i;

      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         xmm0i = _mm_load_si128(sptr++);
         xmm4i = _mm_load_si128(sptr++);

         xmm3i = _mm_load_si128(dptr);
         xmm7i = _mm_load_si128(dptr+1);

         xmm0i = _mm_add_epi32(xmm0i, xmm3i);
         xmm4i = _mm_add_epi32(xmm4i, xmm7i);

         _mm_store_si128(dptr++, xmm0i);
         _mm_store_si128(dptr++, xmm4i);
      }
      while(--i);
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
_batch_imadd_sse2(int32_ptr dst, const_int32_ptr src, size_t num, float v, float vstep)
{
   int32_ptr d = (int32_ptr)dst;
   int32_ptr s = (int32_ptr)src;
   size_t i, step, dtmp, stmp;

   if (!num || (v <= LEVEL_90DB && vstep <= LEVEL_90DB)) return;
   if (fabsf(v - 1.0f) < LEVEL_90DB && vstep <=  LEVEL_90DB) {
      _batch_iadd_sse2(dst, src, num);
      return;
   }

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
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
      i = (MEMALIGN16 - dtmp)/sizeof(int32_t);
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

   i = num/step;
   if (i)
   {
      __m128i *sptr = (__m128i *)s;
      __m128i *dptr = (__m128i *)d;
      __m128i xmm0i, xmm3i, xmm4i, xmm7i;
      __m128 xmm1, xmm5;

      vstep *= step;
      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         __m128 tv = _mm_set1_ps(v);

         xmm0i = _mm_load_si128(sptr++);
         xmm4i = _mm_load_si128(sptr++);

         xmm1 = _mm_cvtepi32_ps(xmm0i);
         xmm5 = _mm_cvtepi32_ps(xmm4i);

         xmm1 = _mm_mul_ps(xmm1, tv);
         xmm5 = _mm_mul_ps(xmm5, tv);

         xmm0i = _mm_load_si128(dptr);
         xmm4i = _mm_load_si128(dptr+1);

         xmm3i = _mm_cvtps_epi32(xmm1);
         xmm7i = _mm_cvtps_epi32(xmm5);

         xmm0i = _mm_add_epi32(xmm0i, xmm3i);
         xmm4i = _mm_add_epi32(xmm4i, xmm7i);

         v += vstep;

         _mm_store_si128(dptr++, xmm0i);
         _mm_store_si128(dptr++, xmm4i);
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

static void
_batch_fadd_sse2(float32_ptr dst, const_float32_ptr src, size_t num)
{
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   /* work towards a 16-byte aligned d (and hence 16-byte aligned s) */
   dtmp = (size_t)d & MEMMASK16;
   if (dtmp && num)
   {
      i = (MEMALIGN16 - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ += *s++;
         } while(--i);
      }
   }
   stmp = (size_t)s & MEMMASK16;

   step = 8*sizeof(__m128)/sizeof(float);
   if (num >= step)
   {
      __m128* sptr = (__m128*)s;
      __m128 *dptr = (__m128*)d;
      __m128 xmm0, xmm1, xmm2, xmm3;
      __m128 xmm4, xmm5, xmm6, xmm7;

      i = num/step;
      num -= i*step;

      s += i*step;
      d += i*step;
      if (stmp)
      {
         do
         {
            xmm0 = _mm_loadu_ps((const float*)sptr++);
            xmm1 = _mm_loadu_ps((const float*)sptr++);
            xmm2 = _mm_loadu_ps((const float*)sptr++);
            xmm3 = _mm_loadu_ps((const float*)sptr++);
            xmm4 = _mm_loadu_ps((const float*)sptr++);
            xmm5 = _mm_loadu_ps((const float*)sptr++);
            xmm6 = _mm_loadu_ps((const float*)sptr++);
            xmm7 = _mm_loadu_ps((const float*)sptr++);

            xmm0 = _mm_add_ps(_mm_load_ps((const float*)(dptr+0)), xmm0);
            xmm1 = _mm_add_ps(_mm_load_ps((const float*)(dptr+1)), xmm1);
            xmm2 = _mm_add_ps(_mm_load_ps((const float*)(dptr+2)), xmm2);
            xmm3 = _mm_add_ps(_mm_load_ps((const float*)(dptr+3)), xmm3);
            xmm4 = _mm_add_ps(_mm_load_ps((const float*)(dptr+4)), xmm4);
            xmm5 = _mm_add_ps(_mm_load_ps((const float*)(dptr+5)), xmm5);
            xmm6 = _mm_add_ps(_mm_load_ps((const float*)(dptr+6)), xmm6);
            xmm7 = _mm_add_ps(_mm_load_ps((const float*)(dptr+7)), xmm7);

            _mm_store_ps((float*)dptr++, xmm0);
            _mm_store_ps((float*)dptr++, xmm1);
            _mm_store_ps((float*)dptr++, xmm2);
            _mm_store_ps((float*)dptr++, xmm3);
            _mm_store_ps((float*)dptr++, xmm4);
            _mm_store_ps((float*)dptr++, xmm5);
            _mm_store_ps((float*)dptr++, xmm6);
            _mm_store_ps((float*)dptr++, xmm7);
         }
         while(--i);
      }
      else
      {
         do
         {
            xmm0 = _mm_load_ps((const float*)sptr++);
            xmm1 = _mm_load_ps((const float*)sptr++);
            xmm2 = _mm_load_ps((const float*)sptr++);
            xmm3 = _mm_load_ps((const float*)sptr++);
            xmm4 = _mm_load_ps((const float*)sptr++);
            xmm5 = _mm_load_ps((const float*)sptr++);
            xmm6 = _mm_load_ps((const float*)sptr++);
            xmm7 = _mm_load_ps((const float*)sptr++);

            xmm0 = _mm_add_ps(_mm_load_ps((const float*)(dptr+0)), xmm0);
            xmm1 = _mm_add_ps(_mm_load_ps((const float*)(dptr+1)), xmm1);
            xmm2 = _mm_add_ps(_mm_load_ps((const float*)(dptr+2)), xmm2);
            xmm3 = _mm_add_ps(_mm_load_ps((const float*)(dptr+3)), xmm3);
            xmm4 = _mm_add_ps(_mm_load_ps((const float*)(dptr+4)), xmm4);
            xmm5 = _mm_add_ps(_mm_load_ps((const float*)(dptr+5)), xmm5);
            xmm6 = _mm_add_ps(_mm_load_ps((const float*)(dptr+6)), xmm6);
            xmm7 = _mm_add_ps(_mm_load_ps((const float*)(dptr+7)), xmm7);

            _mm_store_ps((float*)dptr++, xmm0);
            _mm_store_ps((float*)dptr++, xmm1);
            _mm_store_ps((float*)dptr++, xmm2);
            _mm_store_ps((float*)dptr++, xmm3);
            _mm_store_ps((float*)dptr++, xmm4);
            _mm_store_ps((float*)dptr++, xmm5);
            _mm_store_ps((float*)dptr++, xmm6);
            _mm_store_ps((float*)dptr++, xmm7);
         }
         while(--i);
      }
   }

   step = 2*sizeof(__m128)/sizeof(float);
   i = num/step;
   if (i)
   {
      __m128* sptr = (__m128*)s;
      __m128 *dptr = (__m128*)d;
      __m128 xmm0, xmm1;

      num -= i*step;
      s += i*step;
      d += i*step;
      if (stmp)
      {
         do
         {
            xmm0 = _mm_loadu_ps((const float*)sptr++);
            xmm1 = _mm_loadu_ps((const float*)sptr++);

            xmm0 =_mm_add_ps(_mm_load_ps((const float*)(dptr+0)), xmm0);
            xmm1 =_mm_add_ps(_mm_load_ps((const float*)(dptr+1)), xmm1);

            _mm_store_ps((float*)dptr++, xmm0);
            _mm_store_ps((float*)dptr++, xmm1);
         }
         while(--i);
      }
      else
      {
         do
         {
            xmm0 = _mm_load_ps((const float*)sptr++);
            xmm1 = _mm_load_ps((const float*)sptr++);

            xmm0 =_mm_add_ps(_mm_load_ps((const float*)(dptr+0)), xmm0);
            xmm1 =_mm_add_ps(_mm_load_ps((const float*)(dptr+1)), xmm1);

            _mm_store_ps((float*)dptr++, xmm0);
            _mm_store_ps((float*)dptr++, xmm1);
         }
         while(--i);
      }
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
_batch_fmul_value_sse2(void* data, unsigned bps, size_t num, float f)
{
   if (!num || fabsf(f - 1.0f) < LEVEL_90DB) return;

   if (f <= LEVEL_90DB) {
      memset(data, 0, num*bps);
   }
   else if (bps == 4)
   {
      float32_ptr d = (float32_ptr)data;
      size_t i, step, dtmp;

      /* work towards a 16-byte aligned d (and hence 16-byte aligned s) */
      dtmp = (size_t)d & MEMMASK16;
      if (dtmp && num)
      {
         i = (MEMALIGN16 - dtmp)/sizeof(float);
         if (i <= num)
         {
            num -= i;
            do {
               *d++ *=  f;
            } while(--i);
         }
      }

      step = 8*sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m128* dptr = (__m128*)d;
         __m128 xmm0, xmm1, xmm2, xmm3;
         __m128 xmm4, xmm5, xmm6, xmm7 = _mm_set1_ps(f);

         num -= i*step;
         d += i*step;
         do
         {
            xmm0 = _mm_mul_ps(_mm_load_ps((const float*)(dptr+0)), xmm7);
            xmm1 = _mm_mul_ps(_mm_load_ps((const float*)(dptr+1)), xmm7);
            xmm2 = _mm_mul_ps(_mm_load_ps((const float*)(dptr+2)), xmm7);
            xmm3 = _mm_mul_ps(_mm_load_ps((const float*)(dptr+3)), xmm7);
            xmm4 = _mm_mul_ps(_mm_load_ps((const float*)(dptr+4)), xmm7);
            xmm5 = _mm_mul_ps(_mm_load_ps((const float*)(dptr+5)), xmm7);
            xmm6 = _mm_mul_ps(_mm_load_ps((const float*)(dptr+6)), xmm7);
            xmm7 = _mm_mul_ps(_mm_load_ps((const float*)(dptr+7)), xmm7);

            _mm_store_ps((float*)dptr++, xmm0);
            _mm_store_ps((float*)dptr++, xmm1);
            _mm_store_ps((float*)dptr++, xmm2);
            _mm_store_ps((float*)dptr++, xmm3);
            _mm_store_ps((float*)dptr++, xmm4);
            _mm_store_ps((float*)dptr++, xmm5);
            _mm_store_ps((float*)dptr++, xmm6);
            _mm_store_ps((float*)dptr++, xmm7);

            xmm7 = _mm_set1_ps(f);
         }
         while(--i);
      }

      step = 2*sizeof(__m128)/sizeof(float);
      i = num/step;
      if (i)
      {
         __m128 xmm0, xmm1, xmm7 = _mm_set1_ps(f);
         __m128* dptr = (__m128*)d;

         num -= i*step;
         d += i*step;
         do
         {
            xmm0 =_mm_mul_ps(_mm_load_ps((const float*)(dptr+0)), xmm7);
            xmm1 =_mm_mul_ps(_mm_load_ps((const float*)(dptr+1)), xmm7);

            _mm_store_ps((float*)dptr++, xmm0);
            _mm_store_ps((float*)dptr++, xmm1);
         }
         while(--i);
      }

      if (num)
      {
         i = num;
         do {
            *d++ *= f;
         } while(--i);
      }
   }
   else
   {
      double64_ptr d = (double64_ptr)data;
      size_t i, step, dtmp;

      dtmp = (size_t)d & MEMMASK16;
      if (dtmp && num)
      {
         i = (MEMALIGN16 - dtmp)/sizeof(double);
         if (i <= num)
         {
            num -= i;
            do {
               *d++ *=  f;
            } while(--i);
         }
      }

      step = 8*sizeof(__m128d)/sizeof(double);

      i = num/step;
      if (i)
      {
         __m128d* dptr = (__m128d*)d;
         __m128d xmm0, xmm1, xmm2, xmm3;
         __m128d xmm4, xmm5, xmm6, xmm7 = _mm_set1_pd(f);

         num -= i*step;
         d += i*step;
         do
         {
            xmm0 = _mm_mul_pd(_mm_load_pd((const double*)(dptr+0)), xmm7);
            xmm1 = _mm_mul_pd(_mm_load_pd((const double*)(dptr+1)), xmm7);
            xmm2 = _mm_mul_pd(_mm_load_pd((const double*)(dptr+2)), xmm7);
            xmm3 = _mm_mul_pd(_mm_load_pd((const double*)(dptr+3)), xmm7);
            xmm4 = _mm_mul_pd(_mm_load_pd((const double*)(dptr+4)), xmm7);
            xmm5 = _mm_mul_pd(_mm_load_pd((const double*)(dptr+5)), xmm7);
            xmm6 = _mm_mul_pd(_mm_load_pd((const double*)(dptr+6)), xmm7);
            xmm7 = _mm_mul_pd(_mm_load_pd((const double*)(dptr+7)), xmm7);

            _mm_store_pd((double*)dptr++, xmm0);
            _mm_store_pd((double*)dptr++, xmm1);
            _mm_store_pd((double*)dptr++, xmm2);
            _mm_store_pd((double*)dptr++, xmm3);
            _mm_store_pd((double*)dptr++, xmm4);
            _mm_store_pd((double*)dptr++, xmm5);
            _mm_store_pd((double*)dptr++, xmm6);
            _mm_store_pd((double*)dptr++, xmm7);

            xmm7 = _mm_set1_pd(f);
         }
         while(--i);
      }

      step = 2*sizeof(__m128d)/sizeof(double);
      i = num/step;
      if (i)
      {
         __m128d xmm0, xmm1, xmm7 = _mm_set1_pd(f);
         __m128d* dptr = (__m128d*)d;

         num -= i*step;
         d += i*step;
         do
         {
            xmm0 =_mm_mul_pd(_mm_load_pd((const double*)(dptr+0)), xmm7);
            xmm1 =_mm_mul_pd(_mm_load_pd((const double*)(dptr+1)), xmm7);

            _mm_store_pd((double*)dptr++, xmm0);
            _mm_store_pd((double*)dptr++, xmm1);
         }
         while(--i);
      }

      if (num)
      {
         i = num;
         do {
            *d++ *= f;
         } while(--i);
      }
   }
}


void
_batch_fmadd_sse2(float32_ptr dst, const_float32_ptr src, size_t num, float v, float vstep)
{
   int need_step = (vstep <=  LEVEL_90DB) ? 0 : 1;
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   if (!num || (v <= LEVEL_90DB && vstep <= LEVEL_90DB)) return;
   if (fabsf(v - 1.0f) < LEVEL_90DB && !need_step) {
      _batch_fadd_sse2(dst, src, num);
      return;
   }

   /* work towards a 16-byte aligned d (and hence 16-byte aligned s) */
   dtmp = (size_t)d & MEMMASK16;
   if (dtmp)
   {
      i = (MEMALIGN16 - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ += *s++ * v;
            v += vstep;
         } while(--i);
      }
   }
   stmp = (size_t)s & MEMMASK16;

   step = 8*sizeof(__m128)/sizeof(float);

   i = num/step;
   if (i)
   {
      __m128 xmm0, xmm1, xmm2, xmm3;
      __m128 xmm4, xmm5, xmm6, xmm7 = _mm_set1_ps(v);
      __m128* sptr = (__m128*)s;
      __m128 *dptr = (__m128*)d;

      vstep *= step;
      num -= i*step;
      s += i*step;
      d += i*step;
      if (stmp)
      {
         do
         {
            xmm0 = _mm_mul_ps(_mm_loadu_ps((const float*)sptr++), xmm7);
            xmm1 = _mm_mul_ps(_mm_loadu_ps((const float*)sptr++), xmm7);
            xmm2 = _mm_mul_ps(_mm_loadu_ps((const float*)sptr++), xmm7);
            xmm3 = _mm_mul_ps(_mm_loadu_ps((const float*)sptr++), xmm7);
            xmm4 = _mm_mul_ps(_mm_loadu_ps((const float*)sptr++), xmm7);
            xmm5 = _mm_mul_ps(_mm_loadu_ps((const float*)sptr++), xmm7);
            xmm6 = _mm_mul_ps(_mm_loadu_ps((const float*)sptr++), xmm7);
            xmm7 = _mm_mul_ps(_mm_loadu_ps((const float*)sptr++), xmm7);

            xmm0 = _mm_add_ps(_mm_load_ps((const float*)(dptr+0)), xmm0);
            xmm1 = _mm_add_ps(_mm_load_ps((const float*)(dptr+1)), xmm1);
            xmm2 = _mm_add_ps(_mm_load_ps((const float*)(dptr+2)), xmm2);
            xmm3 = _mm_add_ps(_mm_load_ps((const float*)(dptr+3)), xmm3);
            xmm4 = _mm_add_ps(_mm_load_ps((const float*)(dptr+4)), xmm4);
            xmm5 = _mm_add_ps(_mm_load_ps((const float*)(dptr+5)), xmm5);
            xmm6 = _mm_add_ps(_mm_load_ps((const float*)(dptr+6)), xmm6);
            xmm7 = _mm_add_ps(_mm_load_ps((const float*)(dptr+7)), xmm7);

            _mm_store_ps((float*)dptr++, xmm0);
            _mm_store_ps((float*)dptr++, xmm1);
            _mm_store_ps((float*)dptr++, xmm2);
            _mm_store_ps((float*)dptr++, xmm3);
            _mm_store_ps((float*)dptr++, xmm4);
            _mm_store_ps((float*)dptr++, xmm5);
            _mm_store_ps((float*)dptr++, xmm6);
            _mm_store_ps((float*)dptr++, xmm7);

            v += vstep;
            xmm7 = _mm_set1_ps(v);
         }
         while(--i);
      }
      else
      {
         do
         {
            xmm0 = _mm_mul_ps(_mm_load_ps((const float*)sptr++), xmm7);
            xmm1 = _mm_mul_ps(_mm_load_ps((const float*)sptr++), xmm7);
            xmm2 = _mm_mul_ps(_mm_load_ps((const float*)sptr++), xmm7);
            xmm3 = _mm_mul_ps(_mm_load_ps((const float*)sptr++), xmm7);
            xmm4 = _mm_mul_ps(_mm_load_ps((const float*)sptr++), xmm7);
            xmm5 = _mm_mul_ps(_mm_load_ps((const float*)sptr++), xmm7);
            xmm6 = _mm_mul_ps(_mm_load_ps((const float*)sptr++), xmm7);
            xmm7 = _mm_mul_ps(_mm_load_ps((const float*)sptr++), xmm7);

            xmm0 = _mm_add_ps(_mm_load_ps((const float*)(dptr+0)), xmm0);
            xmm1 = _mm_add_ps(_mm_load_ps((const float*)(dptr+1)), xmm1);
            xmm2 = _mm_add_ps(_mm_load_ps((const float*)(dptr+2)), xmm2);
            xmm3 = _mm_add_ps(_mm_load_ps((const float*)(dptr+3)), xmm3);
            xmm4 = _mm_add_ps(_mm_load_ps((const float*)(dptr+4)), xmm4);
            xmm5 = _mm_add_ps(_mm_load_ps((const float*)(dptr+5)), xmm5);
            xmm6 = _mm_add_ps(_mm_load_ps((const float*)(dptr+6)), xmm6);
            xmm7 = _mm_add_ps(_mm_load_ps((const float*)(dptr+7)), xmm7);

            _mm_store_ps((float*)dptr++, xmm0);
            _mm_store_ps((float*)dptr++, xmm1);
            _mm_store_ps((float*)dptr++, xmm2);
            _mm_store_ps((float*)dptr++, xmm3);
            _mm_store_ps((float*)dptr++, xmm4);
            _mm_store_ps((float*)dptr++, xmm5);
            _mm_store_ps((float*)dptr++, xmm6);
            _mm_store_ps((float*)dptr++, xmm7);

            v += vstep;
            xmm7 = _mm_set1_ps(v);
         }
         while(--i);
      }
   }

   step = 2*sizeof(__m128)/sizeof(float);
   i = num/step;
   if (i)
   {
      __m128* sptr = (__m128*)s;
      __m128* dptr = (__m128*)d;
      __m128 xmm0, xmm1;

      vstep *= step;
      num -= i*step;
      s += i*step;
      d += i*step;
      if (stmp)
      {
         do
         {
            __m128 tv = _mm_set1_ps(v);

            xmm0 = _mm_mul_ps(_mm_loadu_ps((const float*)sptr++), tv);
            xmm1 = _mm_mul_ps(_mm_loadu_ps((const float*)sptr++), tv);

            xmm0 =_mm_add_ps(_mm_load_ps((const float*)(dptr+0)),xmm0);
            xmm1 =_mm_add_ps(_mm_load_ps((const float*)(dptr+1)),xmm1);

            v += vstep;

            _mm_store_ps((float*)dptr++, xmm0);
            _mm_store_ps((float*)dptr++, xmm1);
         }
         while(--i);
      }
      else
      {
         do
         {
            __m128 tv = _mm_set1_ps(v);

            xmm0 = _mm_mul_ps(_mm_load_ps((const float*)sptr++), tv);
            xmm1 = _mm_mul_ps(_mm_load_ps((const float*)sptr++), tv);

            xmm0 =_mm_add_ps(_mm_load_ps((const float*)(dptr+0)),xmm0);
            xmm1 =_mm_add_ps(_mm_load_ps((const float*)(dptr+1)),xmm1);

            v += vstep;

            _mm_store_ps((float*)dptr++, xmm0);
            _mm_store_ps((float*)dptr++, xmm1);
         }
         while(--i);
      }
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
   tmp = (size_t)d & MEMMASK16;
   if (tmp && num)
   {
      i = (MEMALIGN16 - tmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ = *s++ << 8;
         } while(--i);
      }
   }

   step = 2*sizeof(__m128i)/sizeof(int16_t);

   i = num/step;
   if (i)
   {
      __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i zero = _mm_setzero_si128();
      __m128i *dptr = (__m128i *)d;
      __m128i *sptr = (__m128i *)s;

      tmp = (size_t)s & MEMMASK16;
      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         if (tmp) {
            xmm0 = _mm_loadu_si128(sptr++);
            xmm4 = _mm_loadu_si128(sptr++);
         } else {
            xmm0 = _mm_load_si128(sptr++);
            xmm4 = _mm_load_si128(sptr++);
         }

         xmm1 = _mm_unpacklo_epi16(zero, xmm0);
         xmm3 = _mm_unpackhi_epi16(zero, xmm0);
         xmm5 = _mm_unpacklo_epi16(zero, xmm4);
         xmm7 = _mm_unpackhi_epi16(zero, xmm4);

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
   tmp = (size_t)s & MEMMASK16;
   if (tmp && num)
   {
      i = (MEMALIGN16 - tmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ = *s++ >> 8;
         } while(--i);
      }
   }

   assert(((size_t)s & MEMMASK16) == 0);
   tmp = (size_t)d & MEMMASK16;

   step = 4*sizeof(__m128i)/sizeof(int32_t);

   i = num/step;
   if (i)
   {
      __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i *sptr = (__m128i *)s;
      __m128i *dptr = (__m128i *)d;

      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         xmm0 = _mm_load_si128(sptr++);
         xmm1 = _mm_load_si128(sptr++);
         xmm4 = _mm_load_si128(sptr++);
         xmm5 = _mm_load_si128(sptr++);

         xmm2 = _mm_srai_epi32(xmm0, 8);
         xmm3 = _mm_srai_epi32(xmm1, 8);
         xmm6 = _mm_srai_epi32(xmm4, 8);
         xmm7 = _mm_srai_epi32(xmm5, 8);

         xmm4 = _mm_packs_epi32(xmm2, xmm3);
         xmm6 = _mm_packs_epi32(xmm6, xmm7);

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
   tmp = (size_t)s1 & MEMMASK16;
   assert(tmp == ((size_t)s2 & MEMMASK16));

   i = num/step;
   if (tmp && i)
   {
      i = (MEMALIGN16 - tmp)/sizeof(int32_t);
      num -= i;
      do
      {
         *d++ = *s1++ >> 8;
         *d++ = *s2++ >> 8;
      }
      while (--i);
   }

   i = num/step;
   if (i)
   {
      __m128i mask, xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i *sptr1 = (__m128i*)s1;
      __m128i *sptr2 = (__m128i*)s2;
      __m128i *dptr = (__m128i*)d;

      mask = _mm_set_epi32(0x00FFFF00, 0x00FFFF00, 0x00FFFF00, 0x00FFFF00);
      tmp = (size_t)d & MEMMASK16;

      num -= i*step;
      s1 += i*step;
      s2 += i*step;
      d += 2*i*step;
      do
      {
         xmm0 = _mm_load_si128(sptr1++);
         xmm4 = _mm_load_si128(sptr1++);
         xmm1 = _mm_load_si128(sptr2++);
         xmm5 = _mm_load_si128(sptr2++);

         xmm2 = _mm_and_si128(xmm0, mask);
         xmm3 = _mm_and_si128(xmm1, mask);
         xmm6 = _mm_and_si128(xmm4, mask);
         xmm7 = _mm_and_si128(xmm5, mask);

         xmm0 = _mm_srli_epi32(xmm2, 8);
         xmm1 = _mm_slli_epi32(xmm3, 8);
         xmm4 = _mm_srli_epi32(xmm6, 8);
         xmm5 = _mm_slli_epi32(xmm7, 8);

         xmm0 = _mm_or_si128(xmm1, xmm0);
         xmm4 = _mm_or_si128(xmm5, xmm4);

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
_batch_freqfilter_sse2(int32_ptr dptr, const_int32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   const_int32_ptr s = sptr;

   if (num)
   {
      __m128 c, h, mk;
      float *cptr, *hist;
      int stages;

      cptr = filter->coeff;
      hist = filter->freqfilter_history[t];
      stages = filter->no_stages;
      if (!stages) stages++;

      if (filter->state) {
         mk = _mm_set_ss(filter->k * (filter->high_gain - filter->low_gain));
      } else {
         mk = _mm_set_ss(filter->k * filter->high_gain);
      }

      do
      {
         int32_ptr d = dptr;
         size_t i = num;

//       c = _mm_set_ps(cptr[3], cptr[1], cptr[2], cptr[0]);
         c = _mm_load_ps(cptr);
         c = _mm_shuffle_ps(c, c, _MM_SHUFFLE(3,1,2,0));

//       h = _mm_set_ps(hist[1], hist[1], hist[0], hist[0]);
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

         hist += 2;
         cptr += 4;
         mk = _mm_set_ss(1.0f);
         s = dptr;
      }
      while (--stages);
   }
}

// https://software.intel.com/en-us/articles/practical-intel-avx-optimization-on-2nd-generation-intel-core-processors
void
_batch_freqfilter_float_sse2(float32_ptr dptr, const_float32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   const_float32_ptr s = sptr;

   if (num)
   {
      __m128 c, h, mk;
      float *cptr, *hist;
      int stages;

      cptr = filter->coeff;
      hist = filter->freqfilter_history[t];
      stages = filter->no_stages;
      if (!stages) stages++;

      if (filter->state) {
         mk = _mm_set_ss(filter->k * (filter->high_gain - filter->low_gain));
      } else {
         mk = _mm_set_ss(filter->k * filter->high_gain);
      }

      do
      {
         float32_ptr d = dptr;
         size_t i = num;

//       c = _mm_set_ps(cptr[3], cptr[1], cptr[2], cptr[0]);
         if (((size_t)cptr & MEMMASK16) == 0) {
            c = _mm_load_ps(cptr);
         } else {
            c = _mm_loadu_ps(cptr);
         }
         c = _mm_shuffle_ps(c, c, _MM_SHUFFLE(3,1,2,0));

//       h = _mm_set_ps(hist[1], hist[1], hist[0], hist[0]);
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

         h = _mm_shuffle_ps(h, h, _MM_SHUFFLE(3,1,2,0));
         _mm_storel_pi((__m64*)hist, h);

         hist += 2;
         cptr += 4;
         mk = _mm_set_ss(1.0f);
         s = dptr;
      }
      while (--stages);
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
   tmp = (size_t)d & MEMMASK16;
   if (tmp)
   {
      i = (MEMALIGN16 - tmp);
      num -= i;

      memcpy(d, s, i);
      d += i;
      s += i;
   }

   step = 8*sizeof(__m128i)/sizeof(int8_t);

   i = num/step;
   if (i)
   {
      __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i *sptr = (__m128i*)s;
      __m128i *dptr = (__m128i*)d;

      tmp = (size_t)s & MEMMASK16;
      num -= i*step;
      s += i*step;
      d += i*step;
      if (tmp)
      {
         do
         {
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

#if 0
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
#endif

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
//    _aaxBufResampleNearest_sse2(d, s, dmin, dmax, smu, fact);
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
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

#if 0
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
#endif

// https://github.com/depp/libfresample, BSD license
// http://www.earlevel.com/main/2007/07/03/sample-rate-conversion/
// https://en.wikipedia.org/wiki/Hermite_interpolation
static inline void
_aaxBufResampleLinear_float_sse2(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float32_ptr sptr = (float32_ptr)s;
   float32_ptr dptr = d;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor < 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   dptr += dmin;

   i = dmax-dmin;
   if (i)
   {
#if 1
      __m128 samp = _mm_load_ss(sptr++);       // n
      __m128 nsamp = _mm_load_ss(sptr++);      // (n+1)
      __m128 dsamp = _mm_sub_ss(nsamp, samp);  // (n+1) - n

      do
      {
         __m128 tau = _mm_set_ss(smu);
         __m128 dout = samp;

         smu += freq_factor;

         // fmadd
         dout = _mm_add_ss(dout, _mm_mul_ss(dsamp, tau));

         if (smu >= 1.0)
         {
            samp = nsamp;
            nsamp = _mm_load_ss(sptr++);

            smu -= 1.0;

            dsamp = _mm_sub_ss(nsamp, samp);
         }
         _mm_store_ss(dptr++, dout);
      }
      while (--i);
#else
      float samp, dsamp;
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
#endif
   }
}

// https://en.wikipedia.org/wiki/Cubic_Hermite_spline
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
//    _aaxBufResampleNearest_float_sse2(d, s, dmin, dmax, smu, fact);
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}
#endif // RB_FLOAT_DATA

#else
typedef int make_iso_compilers_happy;
#endif /* SSE2 */

