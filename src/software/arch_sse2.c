/*
 * Copyright 2005-2012 by Erik Hofman.
 * Copyright 2009-2012 by Adalin B.V.
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

#include <ringbuffer.h>

#include "arch_simd.h"

#ifdef __SSE2__

# define CACHE_ADVANCE_RSHFT	128
# define CACHE_ADVANCE_FMADD	 16
# define CACHE_ADVANCE_MUL	 32
# define CACHE_ADVANCE_CPY	256
# define CACHE_ADVANCE_CVT	 32
# define CACHE_ADVANCE_INTL	 16
# define CACHE_ADVANCE_FF	 32


FN_PREALIGN void
_ivec4Copy_sse2(ivec4 d, ivec4 v)
{
   const __m128i *sptr = (__m128i *)v;
   __m128i *dptr = (__m128i*)d;

   _mm_store_si128(dptr, _mm_load_si128(sptr));
}

FN_PREALIGN void
_ivec4Add_sse2(ivec4 d, ivec4 v)
{
   const __m128i *sptr = (__m128i *)v;
   __m128i *dptr = (__m128i*)d;
   __m128i xmm1, xmm2;

   
   xmm1 = _mm_load_si128(dptr);
   xmm2 = _mm_load_si128(sptr);
   _mm_store_si128(dptr, _mm_add_epi32(xmm1, xmm2));
}

FN_PREALIGN void
_ivec4Sub_sse2(ivec4 d, ivec4 v)
{
   const __m128i *sptr = (__m128i *)v;
   __m128i *dptr = (__m128i*)d;
   __m128i xmm1, xmm2;

   xmm1 = _mm_load_si128(dptr);
   xmm2 = _mm_load_si128(sptr);
   _mm_store_si128(dptr, _mm_sub_epi32(xmm1, xmm2));
}

FN_PREALIGN void
_ivec4Devide_sse2(ivec4 d, float s)
{
   if (s)
   {
      __m128i *dptr = (__m128i *)d;
      __m128 xmm1, xmm2;

      xmm1 = _mm_cvtepi32_ps(_mm_load_si128(dptr));
      xmm2 = _mm_set1_ps(s);
      _mm_store_si128(dptr, _mm_cvtps_epi32(_mm_div_ps(xmm1, xmm2)));
   }
}

FN_PREALIGN void
_ivec4Mulivec4_sse2(ivec4 d, const ivec4 v1, const ivec4 v2)
{
   const __m128i *sptr1 = (__m128i *)v1;
   const __m128i *sptr2 = (__m128i *)v2;
   __m128i *dptr = (__m128i *)d;
   __m128 xmm1, xmm2;

   xmm1 = _mm_cvtepi32_ps(_mm_load_si128(sptr1));
   xmm2 = _mm_cvtepi32_ps(_mm_load_si128(sptr2));
   _mm_store_si128(dptr, _mm_cvtps_epi32(_mm_mul_ps(xmm1, xmm2)));
}

void
_batch_cvt24_ps_sse2(void_ptr dptr, const_void_ptr sptr, unsigned int num)
{
   static const float mul = (float)(1<<24);
   int32_t *d = (int32_t*)dptr;
   float* s = (float*)sptr;
   unsigned int i = num;
   do {
      *d++ = (int32_t)(*s++ * mul);
   } while (--i);
}

void
_batch_cvt24_pd_sse2(void_ptr dptr, const_void_ptr sptr, unsigned int num)
{
   static const double mul = (double)(1<<24);
   int32_t *d = (int32_t*)dptr;
   double* s = (double*)sptr;
   unsigned int i = num;
   do {
      *d++ = (int32_t)(*s++ * mul);
   } while (--i);
}

FN_PREALIGN void
_batch_fmadd_sse2(int32_ptr d, const_int32_ptr src, unsigned int num, float v, float vstep)
{
   __m128i *sptr = (__m128i *)src;
   __m128i *dptr = (__m128i*)d;
   __m128 tv = _mm_set1_ps(v);
   int32_ptr s = (int32_ptr)src;
   unsigned int i, size, step;
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

   step = 2*sizeof(__m128i)/sizeof(int32_t);

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
      dptr = (__m128i *)d;
      sptr = (__m128i *)s;
   }

   vstep *= step;				/* 8 samples at a time */
   i = size = num/step;
   if (i)
   {
      __m128i xmm0i, xmm3i, xmm4i, xmm7i;
      __m128 xmm1, xmm2, xmm5, xmm6;

      do
      {
         _mm_prefetch(((char *)sptr)+CACHE_ADVANCE_FMADD, _MM_HINT_NTA);
         xmm0i = _mm_load_si128(sptr++);
         xmm4i = _mm_load_si128(sptr++);
         xmm1 = _mm_cvtepi32_ps(xmm0i);
         xmm5 = _mm_cvtepi32_ps(xmm4i);

         xmm0i = _mm_load_si128(dptr);
         xmm4i = _mm_load_si128(dptr+1);

         v += vstep;

         xmm2 = _mm_mul_ps(tv, xmm1);
         xmm6 = _mm_mul_ps(tv, xmm5);
         xmm3i = _mm_cvtps_epi32(xmm2);
         xmm7i = _mm_cvtps_epi32(xmm6);

         xmm0i = _mm_add_epi32(xmm0i, xmm3i);
         xmm4i = _mm_add_epi32(xmm4i, xmm7i);

         _mm_store_si128(dptr, xmm0i);
         _mm_store_si128(dptr+1, xmm4i);
         dptr += 2;

         tv = _mm_set1_ps(v);
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

FN_PREALIGN void
_batch_cvt24_16_sse2(void_ptr dbuf, const_void_ptr sbuf, unsigned int num)
{
   __m128i *dptr = (__m128i*)dbuf;
   int16_t *s = (int16_t *)sbuf;
   int32_t *d = (int32_t*)dbuf;
   unsigned int i, size, step, n;
   long tmp;

   n = num;
   step = 2*sizeof(__m128i)/sizeof(int16_t);

   /*
    * work towards 16-byte aligned dptr
    */
   i = n/step;
   tmp = (long)d & 0xF;
   if (tmp && i)
   {
      i = (0x10 - tmp)/sizeof(int32_t);
      n -= i;
      do {
         *d++ = *s++ << 8;
      } while(--i);
      dptr = (__m128i *)d;
   }

   tmp = (long)s & 0xF;
   i = size = n/step;
   if (i)
   {
      __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i zero = _mm_setzero_si128();
      __m128i *sptr = (__m128i *)s;

      do {
         _mm_prefetch(((char *)s)+CACHE_ADVANCE_CVT, _MM_HINT_NTA);
         if (tmp) {
            xmm0 = _mm_loadu_si128(sptr);
            xmm4 = _mm_loadu_si128(sptr+1);
         } else {
            xmm0 = _mm_load_si128(sptr);
            xmm4 = _mm_load_si128(sptr+1);
         }
         xmm1 = _mm_unpacklo_epi16(zero, xmm0);
         xmm3 = _mm_unpackhi_epi16(zero, xmm0);
         xmm5 = _mm_unpacklo_epi16(zero, xmm4);
         xmm7 = _mm_unpackhi_epi16(zero, xmm4);
         s += step;
         xmm0 = _mm_srai_epi32(xmm1, 8);
         xmm2 = _mm_srai_epi32(xmm3, 8);
         xmm4 = _mm_srai_epi32(xmm5, 8);
         xmm6 = _mm_srai_epi32(xmm7, 8);
         sptr = (__m128i *)s;
         _mm_store_si128(dptr, xmm0);
         _mm_store_si128(dptr+1, xmm2);
         _mm_store_si128(dptr+2, xmm4);
         _mm_store_si128(dptr+3, xmm6);
         dptr += 4;
      } while (--i);
   }

   i = n - size*step;
   if (i)
   {
      d = (int32_t *)dptr;
      do {
         *d++ = *s++ << 8;
      } while (--i);
   }
}

void
_batch_cvt16_24_sse2(void_ptr dst, const_void_ptr src, unsigned int num)
{
   int32_t* s = (int32_t*)src;
   int16_t* d = (int16_t*)dst;
#if 1
   unsigned int i = (num/4)*4;
   unsigned int j = num-i;
   if (i) {
      do {
         *d = *s >> 8;
         *(d+1) = *(s+1) >> 8;
         *(d+2) = *(s+2) >> 8;
         *(d+3) = *(s+3) >> 8;
         i -= 4;
         d += 4;
         s += 4;
      }
      while (i);
   }
   if (j)
   {
      do {
         *d++ = *s++ >> 8;
      }
      while (--j);
   }

#else
   /* somehow this doesn't work, no idea why? */
   unsigned int i, size, step;
   long tmp;

   step = 4*sizeof(__m128i)/sizeof(int32_t);

   /*
    * work towards 16-byte aligned sptr
    */
   i = num/step;
   tmp = (long)s & 0xF;
   if (tmp && i)
   {
      i = (0x10 - tmp)/sizeof(int32_t);
      num -= i;
      do {
         *d++ = *s++ >> 8;
      } while(--i);
   }

   tmp = (long)d & 0xF;
   i = size = num/step;
   if (i)
   {
      __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i *dptr, *sptr;

      dptr = (__m128i *)d;
      do
      {
         sptr = (__m128i *)s;
         _mm_prefetch(((char *)s)+CACHE_ADVANCE_CVT, _MM_HINT_NTA);
         xmm0 = _mm_load_si128(sptr);
         xmm1 = _mm_load_si128(sptr+1);
         xmm4 = _mm_load_si128(sptr+2);
         xmm5 = _mm_load_si128(sptr+3);

         xmm2 = _mm_srli_epi32(xmm0, 8);
         xmm3 = _mm_slli_epi32(xmm1, 8);
         xmm6 = _mm_srli_epi32(xmm4, 8);
         xmm7 = _mm_slli_epi32(xmm5, 8);
         s += step;

         xmm4 = _mm_packs_epi32(xmm2, xmm3);
         xmm6 = _mm_packs_epi32(xmm6, xmm7);

         if (tmp) {
            _mm_storeu_si128(dptr, xmm4);
            _mm_storeu_si128(dptr+1, xmm6);
         } else {
            _mm_store_si128(dptr, xmm4);
            _mm_store_si128(dptr+1, xmm6);
         }
         dptr += 2;
      } while (--i);
      d = (int16_t *)dptr;
   }

   i = num - size*step;
   if (i)
   {
      do {
         *d++ = *s++ >> 8;
      } while (--i);
   }
#endif
}

FN_PREALIGN void
_batch_cvt16_intl_24_sse2(void_ptr dst, const_int32_ptrptr src,
                                unsigned int offset, unsigned int tracks,
                                unsigned int num)
{
   unsigned int i, size, step;
   int16_t* d = (int16_t*)dst;
   int32_t *s1, *s2;
   long tmp;

   if (tracks != 2)
   {
      // _batch_cvt24_intl_16_cpu(d, src, offset, tracks, num);
      unsigned int t;
      for (t=0; t<tracks; t++)
      {
         int32_t *sptr = (int32_t *)src[t] + offset;
         int16_t *dptr = d + t;
         unsigned int i = (num/4)*4;
         assert(i == num);
         do
         {
            *dptr = *(sptr++) >> 8;
            dptr += tracks;
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
   i = num/step;
   tmp = (long)s1 & 0xF;
   assert(tmp == ((long)s2 & 0xF));
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

   tmp = (long)d & 0xF;
   i = size = num/step;
   if (i)
   {
      __m128i mask, xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i *dptr, *sptr1, *sptr2;

      mask = _mm_set_epi32(0x00FFFF00, 0x00FFFF00, 0x00FFFF00, 0x00FFFF00);
      dptr = (__m128i *)d;
      do
      {
         sptr1 = (__m128i*)s1;
         sptr2 = (__m128i*)s2;
         _mm_prefetch(((char *)s1)+CACHE_ADVANCE_INTL, _MM_HINT_NTA);
         _mm_prefetch(((char *)s2)+CACHE_ADVANCE_INTL, _MM_HINT_NTA);

         xmm0 = _mm_load_si128(sptr1);
         xmm1 = _mm_load_si128(sptr2);
         xmm4 = _mm_load_si128(sptr1+1);
         xmm5 = _mm_load_si128(sptr2+1);

         xmm2 = _mm_and_si128(xmm0, mask);
         xmm3 = _mm_and_si128(xmm1, mask);
         xmm6 = _mm_and_si128(xmm4, mask);
         xmm7 = _mm_and_si128(xmm5, mask);
         xmm0 = _mm_srli_epi32(xmm2, 8);
         xmm1 = _mm_slli_epi32(xmm3, 8);
         xmm4 = _mm_srli_epi32(xmm6, 8);
         xmm5 = _mm_slli_epi32(xmm7, 8);
         s1 += step;
         s2 += step;

         xmm0 = _mm_or_si128(xmm1, xmm0);
         xmm4 = _mm_or_si128(xmm5, xmm4);
         if (tmp) {
            _mm_storeu_si128(dptr, xmm0);
            _mm_storeu_si128(dptr+1, xmm4);
         } else {
            _mm_store_si128(dptr, xmm0);
            _mm_store_si128(dptr+1, xmm4);
         }
         dptr += 2;
      } while (--i);
      d = (int16_t *)dptr;
   }

   i = num - size*step;
   if (i)
   {
      do
      {
         *d++ = *s1++ >> 8;
         *d++ = *s2++ >> 8;
      }
      while (--i);
   }
}

#define CALCULATE_NEW_SAMPLE(i, smp) \
        dhist = _mm_set_ps(h1, h0, h1, h0);     \
        h1 = h0;                                \
        xmm##i = _mm_mul_ps(dhist, coeff);      \
        _mm_store_ps(mpf, xmm##i);              \
        smp -=  mpf[0]; h0 = smp - mpf[1];      \
        smp = h0 + mpf[2]; smp += mpf[3]

FN_PREALIGN void
_batch_freqfilter_sse2(int32_ptr d, const_int32_ptr sptr, unsigned int num,
                  float *hist, float lfgain, float hfgain, float k,
                  const float *cptr)
{
   int32_ptr s = (int32_ptr)sptr;
   unsigned int i, size, step;
   float h0, h1;
   long tmp;

   h0 = hist[0];
   h1 = hist[1];

   step = 2*sizeof(__m128i)/sizeof(int32_t);

   /* work towards 16-byte aligned dptr */
   i = num/step;
   tmp = (long)d & 0xF;
   if (tmp && i)
   {
      float smp, nsmp;

      i = (0x10 - tmp)/sizeof(int32_t);
      num -= i;
      do
      {
         smp = *s * k;
         smp = smp - h0 * cptr[0];
         nsmp = smp - h1 * cptr[1];
         smp = nsmp + h0 * cptr[2];
         smp = smp + h1 * cptr[3];

         h1 = h0;
         h0 = nsmp;

         *d++ = (int32_t)(smp*lfgain + (*s-smp)*hfgain);
         s++;
      }
      while (--i);
   }

   i = size = num/step;
   if (i)
   {
      __m128 xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128 osmp0, osmp1, fact, dhist, coeff, lf, hf;
      __m128i xmm0i, xmm1i;
      __m128 tmp0, tmp1, tmp2;
      __m128i *sptr, *dptr;
      float *smp0, *smp1, *mpf;

      /* 16-byte aligned */
      smp0 = (float *)&tmp0;
      smp1 = (float *)&tmp1;
      mpf = (float *)&tmp2;

      num -= size*step;

      fact = _mm_set1_ps(k);
      coeff = _mm_set_ps(cptr[3], cptr[2], cptr[1], cptr[0]);
      lf = _mm_set1_ps(lfgain);
      hf = _mm_set1_ps(hfgain);

      tmp = (long)s & 0xF;
      dptr = (__m128i *)d;
      do
      {
         sptr = (__m128i *)s;
         _mm_prefetch(((char *)s)+CACHE_ADVANCE_FF, _MM_HINT_NTA);

         if (tmp) {
            xmm0i = _mm_loadu_si128(sptr);
            xmm1i = _mm_loadu_si128(sptr+1);
         } else {
            xmm0i = _mm_load_si128(sptr);
            xmm1i = _mm_load_si128(sptr+1);
         }

         osmp0 = _mm_cvtepi32_ps(xmm0i);
         osmp1 = _mm_cvtepi32_ps(xmm1i);
         xmm3 = _mm_mul_ps(osmp0, fact);        /* *s * k */
         xmm7 = _mm_mul_ps(osmp1, fact);
         _mm_store_ps(smp0, xmm3);
         _mm_store_ps(smp1, xmm7);
         s += step;

         CALCULATE_NEW_SAMPLE(0, smp0[0]);
         CALCULATE_NEW_SAMPLE(1, smp0[1]);
         CALCULATE_NEW_SAMPLE(2, smp0[2]);
         CALCULATE_NEW_SAMPLE(3, smp0[3]);
         CALCULATE_NEW_SAMPLE(4, smp1[0]);
         CALCULATE_NEW_SAMPLE(5, smp1[1]);
         CALCULATE_NEW_SAMPLE(6, smp1[2]);
         CALCULATE_NEW_SAMPLE(7, smp1[3]);

         xmm0 = _mm_load_ps(smp0);		/* smp */
         xmm4 = _mm_load_ps(smp1);
         xmm1 = _mm_mul_ps(xmm0, lf);		/* smp * lfgain */
         xmm5 = _mm_mul_ps(xmm4, lf);
         xmm2 = _mm_sub_ps(osmp0, xmm0);	/* *s - smp */
         xmm6 = _mm_sub_ps(osmp1, xmm4);
         xmm3 = _mm_mul_ps(xmm2, hf);		/* (*s-smp) * hfgain */
         xmm7 = _mm_mul_ps(xmm6, hf);
         xmm2 = _mm_add_ps(xmm1, xmm3);       /* smp*lfgain + (*s-smp)*hfgain */
         xmm6 = _mm_add_ps(xmm5, xmm7);
         xmm0i = _mm_cvtps_epi32(xmm2);
         xmm1i = _mm_cvtps_epi32(xmm6);

         _mm_store_si128(dptr, xmm0i);
         _mm_store_si128(dptr+1, xmm1i);
         dptr += 2;
      }
      while (--i);

      d = (int32_t *)dptr;
   }

   i = num;
   if (i)
   {
      float smp, nsmp;
      do
      {
         smp = *s * k;
         smp = smp - h0 * cptr[0];
         nsmp = smp - h1 * cptr[1];
         smp = nsmp + h0 * cptr[2];
         smp = smp + h1 * cptr[3];

         h1 = h0;
         h0 = nsmp;

         *d++ = (int32_t)(smp*lfgain + (*s-smp)*hfgain);
         s++;
      }
      while (--i);
   }

   hist[0] = h0;
   hist[1] = h1;
}

/*
 * optimized memcpy for 16-byte aligned destination buffer
 * fall back tobuildt-in  memcpy otherwise.
 */
FN_PREALIGN void *
_aax_memcpy_sse2(void_ptr dst, const_void_ptr src, size_t  bufsz)
{
   unsigned int step = 8*sizeof(__m128i)/sizeof(int8_t);
   long tmp;

   tmp = (long)dst & 0xF;
   if ((bufsz < step) || (tmp != ((long)src & 0xF))) {
      memcpy(dst, src, bufsz);
   }
   else
   {
      __m128i *dptr = (__m128i *)dst;
      __m128i *sptr = (__m128i *)src;
      unsigned int i, num;

      /*
       * work towards a 16-byte aligned dptr and possibly sptr
       */
      if (tmp)
      {
         i = (0x10 - tmp);

         bufsz -= i;
         memcpy(dptr, sptr, i);

         sptr = (__m128i*)((char*)src+i);
         dptr = (__m128i*)((char*)dst+i);
      }

      num = bufsz/step;
      if (num)
      {
         __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

         bufsz -= num*step;
         i = num;
         if (tmp)
         {
            do
            {
               _mm_prefetch(((char *)sptr)+CACHE_ADVANCE_CPY, _MM_HINT_NTA);
               xmm0 = _mm_loadu_si128(sptr);
               xmm1 = _mm_loadu_si128(sptr+1);
               xmm2 = _mm_loadu_si128(sptr+2);
               xmm3 = _mm_loadu_si128(sptr+3);
               xmm4 = _mm_loadu_si128(sptr+4);
               xmm5 = _mm_loadu_si128(sptr+5);
               xmm6 = _mm_loadu_si128(sptr+6);
               xmm7 = _mm_loadu_si128(sptr+7);
               sptr += 8;

               _mm_store_si128(dptr, xmm0);
               _mm_store_si128(dptr+1, xmm1);
               _mm_store_si128(dptr+2, xmm2);
               _mm_store_si128(dptr+3, xmm3);
               _mm_store_si128(dptr+4, xmm4);
               _mm_store_si128(dptr+5, xmm5);
               _mm_store_si128(dptr+6, xmm6);
               _mm_store_si128(dptr+7, xmm7);
               dptr += 8;
            }
            while(--i);
         }
         else	/* both buffers are 16-byte aligned */
         {
            do
            {
               _mm_prefetch(((char *)sptr)+CACHE_ADVANCE_CPY, _MM_HINT_NTA);
               xmm0 = _mm_load_si128(sptr);
               xmm1 = _mm_load_si128(sptr+1);
               xmm2 = _mm_load_si128(sptr+2);
               xmm3 = _mm_load_si128(sptr+3);
               xmm4 = _mm_load_si128(sptr+4);
               xmm5 = _mm_load_si128(sptr+5);
               xmm6 = _mm_load_si128(sptr+6);
               xmm7 = _mm_load_si128(sptr+7);
               sptr += 8;

               _mm_store_si128(dptr, xmm0);
               _mm_store_si128(dptr+1, xmm1);
               _mm_store_si128(dptr+2, xmm2);
               _mm_store_si128(dptr+3, xmm3);
               _mm_store_si128(dptr+4, xmm4);
               _mm_store_si128(dptr+5, xmm5);
               _mm_store_si128(dptr+6, xmm6);
               _mm_store_si128(dptr+7, xmm7);
               dptr += 8;
            }
            while(--i);
         }
      }

#if 0
      step = 2*sizeof(__m128i)/sizeof(int8_t);
      num = bufsz/step;
      if (num)
      {
         __m128i xmm0, xmm1;

         bufsz -= num*step;
         i = num;
         if (tmp)
         {
            do
            {
               _mm_prefetch(((char*)sptr)+CACHE_ADVANCE_CPY, _MM_HINT_NTA);
               xmm0 = _mm_loadu_si128((__m128i *)sptr);
               xmm1 = _mm_loadu_si128((__m128i *)sptr+1);
               sptr += 2;

               _mm_store_si128(dptr, xmm0);
               _mm_store_si128(dptr+1, xmm1);
               dptr += 2;
            }
            while(--i);
         }
         else
         {
            do
            {
               _mm_prefetch(((char*)sptr)+CACHE_ADVANCE_CPY, _MM_HINT_NTA);
               xmm0 = _mm_load_si128((__m128i *)sptr);
               xmm1 = _mm_load_si128((__m128i *)sptr+1);
               sptr += 2;

               _mm_store_si128(dptr, xmm0);
               _mm_store_si128(dptr+1, xmm1);
               dptr += 2;
            }
            while(--i);
         }
      }
#endif

      if (bufsz) {
         memcpy(dptr, sptr, bufsz);
      }
   }

   return dst;
}

void
_aaxBufResampleSkip_sse2(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   int32_t samp, dsamp;
   unsigned int i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor >= 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   sptr += sdesamps;
   dptr += dmin;

   samp = *sptr++;              // n+(step-1)
   dsamp = *sptr - samp;        // (n+1) - n


   i=dmax-dmin;
   do
   {
      int step;

      *dptr++ = samp + (int32_t)(dsamp * smu);

      smu += freq_factor;
      step = (int)floorf(smu);

      smu -= step;
      sptr += step-1;
      samp = *sptr++;
      dsamp = *sptr - samp;
   }
   while (--i);
}

void
_aaxBufResampleNearest_sse2(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   if (freq_factor == 1.0f) {
      _aax_memcpy(d+dmin, s+sdesamps, (dmax-dmin)*sizeof(int32_t));
   }
   else
   {
      int32_ptr sptr = (int32_ptr)s;
      int32_ptr dptr = d;
      unsigned int i;

      assert(s != 0);
      assert(d != 0);
      assert(dmin < dmax);
      assert(0.95f <= freq_factor && freq_factor <= 1.05f);
      assert(0.0f <= smu && smu < 1.0f);

      sptr += sdesamps;
      dptr += dmin;

      i = dmax-dmin;
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

void
_aaxBufResampleLinear_sse2(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   int32_t samp, dsamp;
   unsigned int i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor < 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   sptr += sdesamps;
   dptr += dmin;

   samp = *sptr++;		// n
   dsamp = *sptr - samp;	// (n+1) - n

   i = dmax-dmin;
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


void
_aaxBufResampleCubic_sse2(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   float y0, y1, y2, y3, a0, a1, a2;
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   unsigned int i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

   sptr += sdesamps;
   dptr += dmin;

   y0 = (float)*sptr++;
   y1 = (float)*sptr++;
   y2 = (float)*sptr++;
   y3 = (float)*sptr++;

   a0 = y3 - y2 - y0 + y1;
   a1 = y0 - y1 - a0;
   a2 = y2 - y0;

   i = dmax-dmin;
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
#endif /* SSE2 */

