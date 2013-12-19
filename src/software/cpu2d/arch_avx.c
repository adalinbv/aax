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

#include "software/ringbuffer.h"
#include "arch_simd.h"

#ifdef __AVX__

# define CACHE_ADVANCE_FMADD	 (2*16)
# define CACHE_ADVANCE_FF	 (2*32)

FN_PREALIGN void
_batch_fma3_avx(int32_ptr d, const_int32_ptr src, unsigned int num, float v, float vstep)
{
   __m256i *sptr = (__m256i *)s;
   __m256i *dptr = (__m256i*)d;
   __m256 tv = _mm256_set1_ps(v);
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

   step = 2*sizeof(__m256i)/sizeof(int32_t);

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
      dptr = (__m256i *)d;
      sptr = (__m256i *)s;
   }

   vstep *= step;				/* 8 samples at a time */
   i = size = num/step;
   if (i)
   {
      __m256i ymm0i, ymm3i, ymm4i, ymm7i;
      __m256 ymm1, ymm2, ymm5, ymm6;

      do
      {
         _mm_prefetch(((char *)sptr)+CACHE_ADVANCE_FMADD, _MM_HINT_NTA);
         ymm0i = _mm256_load_si256(sptr++);
         ymm4i = _mm256_load_si256(sptr++);
         ymm1 = _mm256_cvtepi32_ps(ymm0i);
         ymm5 = _mm256_cvtepi32_ps(ymm4i);

         ymm0i = _mm256_load_si256(dptr);
         ymm4i = _mm256_load_si256(dptr+1);
         ymm2 = _mm256_cvtepi32_ps(ymm0i);
         ymm6 = _mm256_cvtepi32_ps(ymm4i);

         v += vstep;

         ymm2 = _mm256_fmadd_ps(ymm2, ymm1, tv);
         ymm6 = _mm256_fmadd_ps(ymm6, ymm5, tv);

         ymm0i = _mm256_cvtps_epi32(ymm2);
         ymm4i = _mm256_cvtps_epi32(ymm6)

         _mm256_store_si256(dptr, ymm0i);
         _mm256_store_si256(dptr+1, ymm4i);
         dptr += 2;

         tv = _mm256_set1_ps(v);
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
_batch_fma4_avx(int32_ptr d, const_int32_ptr src, unsigned int num, float v, float vstep)
{
   __m256i *sptr = (__m256i *)s;
   __m256i *dptr = (__m256i*)d;
   __m256 tv = _mm256_set1_ps(v);
   int32_ptr s = (int32_ptr)src;
   unsigned int i, size, step;
   long dtmp, stmp;


   dtmp = (long)dptr & 0xF;
   stmp = (long)sptr & 0xF;
   if ((dtmp || stmp) && dtmp != stmp)
   {
      i = num;                          /* improperly aligned,            */
      do                                /* let the compiler figure it out */
      {
         *d++ += (int32_t)((float)*s++ * v);
         v += vstep;
      }
      while (--i);
      return;
   }

   step = 2*sizeof(__m256i)/sizeof(int32_t);

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
      dptr = (__m256i *)d;
      sptr = (__m256i *)s;
   }

   vstep *= step;                               /* 8 samples at a time */
   i = size = num/step;
   if (i)
   {
      __m256i ymm0i, ymm3i, ymm4i, ymm7i;
      __m256 ymm1, ymm2, ymm5, ymm6;

      do
      {
         _mm_prefetch(((char *)sptr)+CACHE_ADVANCE_FMADD, _MM_HINT_NTA);
         ymm0i = _mm256_load_si256(sptr++);
         ymm4i = _mm256_load_si256(sptr++);
         ymm1 = _mm256_cvtepi32_ps(ymm0i);
         ymm5 = _mm256_cvtepi32_ps(ymm4i);

         ymm0i = _mm256_load_si256(dptr);
         ymm4i = _mm256_load_si256(dptr+1);
         ymm2 = _mm256_cvtepi32_ps(ymm0i);
         ymm6 = _mm256_cvtepi32_ps(ymm4i);

         v += vstep;

         ymm2 = _mm256_fmacc_ps(ymm2, ymm1, tv);
         ymm6 = _mm256_fmacc_ps(ymm6, ymm5, tv);

         ymm0i = _mm256_cvtps_epi32(ymm2);
         ymm4i = _mm256_cvtps_epi32(ymm6)

         _mm256_store_si256(dptr, ymm0i);
         _mm256_store_si256(dptr+1, ymm4i);
         dptr += 2;

         tv = _mm256_set1_ps(v);
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

