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

#ifndef _MSC_VER
# pragma GCC target ("avx","fpmath=avx")
#else
# define __AVX__
#endif

#include "arch_simd.h"

#ifdef __AVX__

# define CACHE_ADVANCE_FMADD	 16
# define CACHE_ADVANCE_FF	 32

void
_batch_fma3_avx(int32_ptr d, const_int32_ptr src, unsigned int num, float v, float vstep)
{
   int32_ptr s = (int32_ptr)src;
   __m128i *sptr = (__m128i *)s;
   __m128i *dptr = (__m128i*)d;
   __m128 tv = _mm_set1_ps(v);
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

#define CALCULATE_NEW_SAMPLE(i, smp) \
        dhist = _mm_set_ps(h1, h0, h1, h0);     \
        h1 = h0;                                \
        xmm##i = _mm_mul_ps(dhist, coeff);      \
        _mm_store_ps(mpf, xmm##i);              \
        smp -=  mpf[0]; h0 = smp - mpf[1];      \
        smp = h0 + mpf[2]; smp += mpf[3]

void
_batch_freqfilter_avx(int32_ptr d, const_int32_ptr sptr, unsigned int num,
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

void
_aaxBufResampleSkip_avx(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
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
_aaxBufResampleNearest_avx(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
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
_aaxBufResampleLinear_avx(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
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
_aaxBufResampleCubic_avx(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
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
#endif /* AVX */

