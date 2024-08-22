/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */
#ifdef __SSE2__

#define A sse2
#include "arch2d_sse_template.c"

void
_aax_init_SSE()
{
   const char *env = getenv("AAX_ENABLE_FTZ");

   if (env && _aax_getbool(env))
   {
// https://www.intel.com/content/www/us/en/docs/cpp-compiler/developer-guide-reference/2021-8/set-the-ftz-and-daz-flags.html
      _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
      _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
   }
}

void
_batch_fmul_value_sse2(void_ptr dptr, const_void_ptr sptr, unsigned bps, size_t num, float f)
{
   if (!num) return;

   if (fabsf(f - 1.0f) < LEVEL_96DB)
   {
      if (sptr != dptr) memcpy(dptr, sptr,  num*bps);
      return;
   }

   if (fabsf(f) <= LEVEL_96DB)
   {
      memset(dptr, 0, num*bps);
      return;
   }

   if (bps == 4)
   {
      const_float32_ptr s = (float32_ptr)sptr;
      float32_ptr d = (float32_ptr)dptr;
      size_t i, step, dtmp, stmp;

      /* work towards a 16-byte aligned d (and hence 16-byte aligned s) */
      dtmp = (size_t)d & MEMMASK16;
      stmp = (size_t)s & MEMMASK16;
      if (dtmp && num)
      {
         i = (MEMALIGN16 - dtmp)/sizeof(float);
         if (i <= num)
         {
            num -= i;
            do {
               *d++ = *s++ * f;
            } while(--i);
         }
      }

      step = 3*sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m128* sptr = (__m128*)s;
         __m128* dptr = (__m128*)d;
         __m128 tv = _mm_set1_ps(f);
         __m128 xmm0, xmm1, xmm2;

         num -= i*step;
         s += i*step;
         d += i*step;
         if (stmp)
         {
            do
            {
               xmm0 = _mm_mul_ps(tv, _mm_loadu_ps((const float*)(sptr++)));
               xmm1 = _mm_mul_ps(tv, _mm_loadu_ps((const float*)(sptr++)));
               xmm2 = _mm_mul_ps(tv, _mm_loadu_ps((const float*)(sptr++)));

               _mm_store_ps((float*)dptr++, xmm0);
               _mm_store_ps((float*)dptr++, xmm1);
               _mm_store_ps((float*)dptr++, xmm2);
            }
            while(--i);
         }
         else
         {
            do
            {
               xmm0 = _mm_mul_ps(tv, _mm_load_ps((const float*)(sptr++)));
               xmm1 = _mm_mul_ps(tv, _mm_load_ps((const float*)(sptr++)));
               xmm2 = _mm_mul_ps(tv, _mm_load_ps((const float*)(sptr++)));

               _mm_store_ps((float*)dptr++, xmm0);
               _mm_store_ps((float*)dptr++, xmm1);
               _mm_store_ps((float*)dptr++, xmm2);
            }
            while(--i);
         }
      }

      if (num)
      {
         i = num;
         do {
            *d++ = *s++ * f;
         } while(--i);
      }
   }
   else if (bps == 8)
   {
      const_double64_ptr s = (double64_ptr)sptr;
      double64_ptr d = (double64_ptr)dptr;
      size_t i, step, dtmp, stmp;

      stmp = (size_t)s & MEMMASK16;
      dtmp = (size_t)d & MEMMASK16;
      if (dtmp && num)
      {
         i = (MEMALIGN16 - dtmp)/sizeof(double);
         if (i <= num)
         {
            num -= i;
            do {
               *d++ = *s++ * f;
            } while(--i);
         }
      }

      step = sizeof(__m128d)/sizeof(double);

      i = num/step;
      if (i)
      {
         __m128d* sptr = (__m128d*)s;
         __m128d* dptr = (__m128d*)d;
         __m128d tv = _mm_set1_pd(f);
         __m128d xmm0;

         num -= i*step;
         s += i*step;
         d += i*step;
         if (stmp)
         {
            do
            {
               xmm0 = _mm_mul_pd(tv, _mm_loadu_pd((const double*)(sptr++)));
               _mm_store_pd((double*)dptr++, xmm0);
            }
            while(--i);
         }
         else
         {
            do
            {
               xmm0 = _mm_mul_pd(tv, _mm_load_pd((const double*)(sptr++)));
               _mm_store_pd((double*)dptr++, xmm0);
            }
            while(--i);
         }
      }

      if (num)
      {
         i = num;
         do {
            *d++ = *s++ * f;
         } while(--i);
      }
   }
   else {
      _batch_fmul_value_cpu(dptr, sptr, bps, num, f);
   }
}

void
_batch_cvt16_24_sse2(void_ptr dst, const_void_ptr src, size_t num)
{
   size_t i, step;
   int32_t* s = (int32_t*)src;
   int16_t* d = (int16_t*)dst;
   size_t tmp;

   if (!num) return;

   _batch_dither_cpu(s, 2, num);

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
         *d++ = (int32_t)*s++;
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
            *d++ = (int32_t)*s++;
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
         *d++ = (float)*s++;
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
            *d++ = (float)*s++;
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
_batch_cvt16_intl_24_sse2(void_ptr dst, const_int32_ptrptr src,
                                size_t offset, unsigned int tracks,
                                size_t num)
{
   size_t i, step;
   int16_t *d = (int16_t*)dst;
   int32_t *s1, *s2;
   size_t t, tmp;

   if (!num) return;

   for (t=0; t<tracks; t++)
   {
      int32_t *s = (int32_t *)src[t] + offset;
      _batch_dither_cpu(s, 2, num);
   }

   if (tracks != 2)
   {
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

#else
typedef int make_iso_compilers_happy;
#endif // __SSE2__
