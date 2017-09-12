/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifdef __AVX2__

static void
_batch_iadd_avx2(int32_ptr dst, const_int32_ptr src, size_t num)
{
   int32_ptr d = (int32_ptr)dst;
   int32_ptr s = (int32_ptr)src;
   size_t i, step, dtmp, stmp;

   dtmp = (size_t)d & 0x1F;
   stmp = (size_t)s & 0x1F;
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
      i = (0x20 - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do
         {
            *d++ += *s++;
         } while(--i);
      }
   }

   step = 2*sizeof(__m256i)/sizeof(int32_t);

   i = num/step;
   num -= i*step;
   if (i)
   {
      __m256i *sptr = (__m256i *)s;
      __m256i *dptr = (__m256i *)d;
      __m256i ymm0i, ymm3i, ymm4i, ymm7i;

      do
      {
         ymm0i = _mm256_load_si256(sptr++);
         ymm4i = _mm256_load_si256(sptr++);

         s += step;

         ymm3i = _mm256_load_si256(dptr);
         ymm7i = _mm256_load_si256(dptr+1);

         d += step;

         ymm0i = _mm256_add_epi32(ymm0i, ymm3i);
         ymm4i = _mm256_add_epi32(ymm4i, ymm7i);

         _mm256_store_si256(dptr++, ymm0i);
         _mm256_store_si256(dptr++, ymm4i);
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
_batch_imadd_avx2(int32_ptr dst, const_int32_ptr src, size_t num, float v, float vstep)
{
   int32_ptr d = (int32_ptr)dst;
   int32_ptr s = (int32_ptr)src;
   size_t i, step, dtmp, stmp;

   if (!num || (v <= LEVEL_128DB && vstep <= LEVEL_128DB)) return;
   if (fabsf(v - 1.0f) < LEVEL_96DB && vstep <=  LEVEL_96DB) {
      _batch_iadd_avx2(dst, src, num);
      return;
   }

   dtmp = (size_t)d & 0x1F;
   stmp = (size_t)s & 0x1F;
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
      i = (0x20 - dtmp)/sizeof(int32_t);
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

   step = 2*sizeof(__m256i)/sizeof(int32_t);

   vstep *= step;				/* 8 samples at a time */
   i = num/step;
   num -= i*step;
   if (i)
   {
      __m256i *sptr = (__m256i *)s;
      __m256i *dptr = (__m256i *)d;
      __m256 tv = _mm256_set1_ps(v);
      __m256i ymm0i, ymm3i, ymm4i, ymm7i;
      __m256 ymm1, ymm5;

      do
      {
         ymm0i = _mm256_load_si256(sptr++);
         ymm4i = _mm256_load_si256(sptr++);

         ymm1 = _mm256_cvtepi32_ps(ymm0i);
         ymm5 = _mm256_cvtepi32_ps(ymm4i);

         s += step;

         ymm1 = _mm256_mul_ps(ymm1, tv);
         ymm5 = _mm256_mul_ps(ymm5, tv);

         ymm0i = _mm256_load_si256(dptr);
         ymm4i = _mm256_load_si256(dptr+1);

         ymm3i = _mm256_cvtps_epi32(ymm1);
         ymm7i = _mm256_cvtps_epi32(ymm5);

         d += step;

         ymm0i = _mm256_add_epi32(ymm0i, ymm3i);
         ymm4i = _mm256_add_epi32(ymm4i, ymm7i);

         v += vstep;

         _mm256_store_si256(dptr++, ymm0i);
         _mm256_store_si256(dptr++, ymm4i);

         tv = _mm256_set1_ps(v);
      }
      while(--i);
   }

   if (num) {
      _batch_imadd_sse2(d, s, num, v, vstep);
   }
}

void
_batch_cvt24_16_avx2(void_ptr dst, const_void_ptr src, size_t num)
{
   int16_t *s = (int16_t *)src;
   int32_t *d = (int32_t*)dst;
   size_t i, step;
   size_t tmp;

   if (!num) return;

   /*
    * work towards 16-byte aligned d
    */
   tmp = (size_t)d & 0x1F;
   if (tmp && num)
   {
      i = (0x20 - tmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ = *s++ << 8;
         } while(--i);
      }
   }

   step = 2*sizeof(__m256i)/sizeof(int16_t);

   tmp = (size_t)s & 0x1F;
   i = num/step;
   num -= i*step;
   if (i)
   {
      __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
      __m256i zero = _mm256_setzero_si256();
      __m256i *dptr = (__m256i *)d;
      __m256i *sptr = (__m256i *)s;

      do {
         if (tmp)
         {
            ymm0 = _mm256_loadu_si256(sptr++);
            ymm4 = _mm256_loadu_si256(sptr++);

            ymm1 = _mm256_unpacklo_epi16(zero, ymm0);
            ymm3 = _mm256_unpackhi_epi16(zero, ymm0);
            ymm5 = _mm256_unpacklo_epi16(zero, ymm4);
            ymm7 = _mm256_unpackhi_epi16(zero, ymm4);
         }
         else
         {
            ymm0 = _mm256_load_si256(sptr++);
            ymm4 = _mm256_load_si256(sptr++);

            ymm1 = _mm256_unpacklo_epi16(zero, ymm0);
            ymm3 = _mm256_unpackhi_epi16(zero, ymm0);
            ymm5 = _mm256_unpacklo_epi16(zero, ymm4);
            ymm7 = _mm256_unpackhi_epi16(zero, ymm4);
         }

         s += step;
         d += step;

         ymm0 = _mm256_srai_epi32(ymm1, 8);
         ymm2 = _mm256_srai_epi32(ymm3, 8);
         ymm4 = _mm256_srai_epi32(ymm5, 8);
         ymm6 = _mm256_srai_epi32(ymm7, 8);

         _mm256_store_si256(dptr++, ymm0);
         _mm256_store_si256(dptr++, ymm2);
         _mm256_store_si256(dptr++, ymm4);
         _mm256_store_si256(dptr++, ymm6);
      }
      while (--i);
   }

   if (num) {
      _batch_cvt24_16_sse2(d, s, num);
   }
}

void
_batch_cvt16_24_avx2(void_ptr dst, const_void_ptr src, size_t num)
{
   size_t i, step;
   int32_t* s = (int32_t*)src;
   int16_t* d = (int16_t*)dst;
   size_t tmp;

   /*
    * work towards 16-byte aligned sptr
    */
   tmp = (size_t)s & 0x1F;
   if (tmp && num)
   {
      i = (0x20 - tmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ = *s++ >> 8;
         } while(--i);
      }
   }

   assert(((size_t)s & 0x1F) == 0);
   tmp = (size_t)d & 0x1F;

   step = 4*sizeof(__m256i)/sizeof(int32_t);

   i = num/step;
   num -= i*step;
   if (i)
   {
      __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
      __m256i *dptr, *sptr;

      dptr = (__m256i *)d;
      sptr = (__m256i *)s;
      do
      {
         ymm0 = _mm256_load_si256(sptr++);
         ymm1 = _mm256_load_si256(sptr++);
         ymm4 = _mm256_load_si256(sptr++);
         ymm5 = _mm256_load_si256(sptr++);

         ymm2 = _mm256_srai_epi32(ymm0, 8);
         ymm3 = _mm256_srai_epi32(ymm1, 8);
         ymm6 = _mm256_srai_epi32(ymm4, 8);
         ymm7 = _mm256_srai_epi32(ymm5, 8);

         s += step;

         ymm4 = _mm256_packs_epi32(ymm2, ymm3);
         ymm6 = _mm256_packs_epi32(ymm6, ymm7);

         d += step;

         if (tmp) {
            _mm256_storeu_si256(dptr++, ymm4);
            _mm256_storeu_si256(dptr++, ymm6);
         } else {
            _mm256_store_si256(dptr++, ymm4);
            _mm256_store_si256(dptr++, ymm6);
         }
      }
      while (--i);
   }

   if (num) {
      _batch_cvt16_24_sse2(d, s, num);
   }
}

void
_batch_cvt16_intl_24_avx2(void_ptr dst, const_int32_ptrptr src,
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

   step = 2*sizeof(__m256i)/sizeof(int32_t);

   /*
    * work towards 16-byte aligned sptr
    */
   tmp = (size_t)s1 & 0x1F;
   assert(tmp == ((size_t)s2 & 0x1F));

   i = num/step;
   if (tmp && i)
   {
      i = (0x20 - tmp)/sizeof(int32_t);
      num -= i;
      do
      {
         *d++ = *s1++ >> 8;
         *d++ = *s2++ >> 8;
      }
      while (--i);
   }

   tmp = (size_t)d & 0x1F;
   i = num/step;
   num -= i*step;
   if (i)
   {
      __m256i mask, ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
      __m256i *dptr, *sptr1, *sptr2;

      sptr1 = (__m256i*)s1;
      sptr2 = (__m256i*)s2;

      mask = _mm256_set_epi32(0x00FFFF00, 0x00FFFF00, 0x00FFFF00, 0x00FFFF00,
                              0x00FFFF00, 0x00FFFF00, 0x00FFFF00, 0x00FFFF00);
      dptr = (__m256i *)d;
      do
      {
         ymm0 = _mm256_load_si256(sptr1++);
         ymm4 = _mm256_load_si256(sptr1++);
         ymm1 = _mm256_load_si256(sptr2++);
         ymm5 = _mm256_load_si256(sptr2++);

         ymm2 = _mm256_and_si256(ymm0, mask);
         ymm3 = _mm256_and_si256(ymm1, mask);
         ymm6 = _mm256_and_si256(ymm4, mask);
         ymm7 = _mm256_and_si256(ymm5, mask);

         s1 += step;
         s2 += step;

         ymm0 = _mm256_srli_epi32(ymm2, 8);
         ymm1 = _mm256_slli_epi32(ymm3, 8);
         ymm4 = _mm256_srli_epi32(ymm6, 8);
         ymm5 = _mm256_slli_epi32(ymm7, 8);

         ymm0 = _mm256_or_si256(ymm1, ymm0);
         ymm4 = _mm256_or_si256(ymm5, ymm4);

         d += 2*step;

         if (tmp) {
            _mm256_storeu_si256(dptr++, ymm0);
            _mm256_storeu_si256(dptr++, ymm4);
         } else {
            _mm256_store_si256(dptr++, ymm0);
            _mm256_store_si256(dptr++, ymm4);
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
_batch_freqfilter_float_avx2(float32_ptr dptr, const_float32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   const_float32_ptr s = sptr;

   if (num)
   {
      __m256 c, h, mk;
      float *cptr, *hist;
      int stages;

      cptr = filter->coeff;
      hist = filter->freqfilter_history[t];
      stages = filter->no_stages;
      if (!stages) stages++;

      if (filter->state) {
         mk = _mm256_set_ss(filter->k * (filter->high_gain - filter->low_gain));
      } else {
         mk = _mm256_set_ss(filter->k * filter->high_gain);
      }

      do
      {
         float32_ptr d = dptr;
         size_t i = num;

//       c = _mm256_set_ps(cptr[3], cptr[1], cptr[2], cptr[0]);
         if (((size_t)cptr & 0x1F) == 0) {
            c = _mm256_load_ps(cptr);
         } else {
            c = _mm256_loadu_ps(cptr);
         }
         c = _mm256_shuffle_ps(c, c, _MM_SHUFFLE(3,1,2,0));

//       h = _mm256_set_ps(hist[1], hist[1], hist[0], hist[0]);
         h = _mm256_loadl_pi(_mm256_setzero_ps(), (__m64*)hist);
         h = _mm256_shuffle_ps(h, h, _MM_SHUFFLE(1,1,0,0));

         do
         {     
            __m256 pz, smp, nsmp, tmp;

            smp = _mm256_load_ss(s);

            // pz = { c[3]*h1, -c[1]*h1, c[2]*h0, -c[0]*h0 };
            pz = _mm256_mul_ps(c, h); // poles and zeros

            // smp = *s++ * k;
            smp = _mm256_mul_ss(smp, mk);

            // tmp[0] = -c[0]*h0 + -c[1]*h1;
            tmp = _mm256_add_ps(pz, _mm256_shuffle_ps(pz, pz, _MM_SHUFFLE(1,3,0,2)));
            s++;

            // nsmp = smp - h0*c[0] - h1*c[1];
            nsmp = _mm256_add_ss(smp, tmp);

            // h1 = h0, h0 = smp: h = { h0, h0, smp, smp };
            h = _mm256_shuffle_ps(nsmp, h, _MM_SHUFFLE(0,0,0,0));

            // tmp[0] = -c[0]*h0 + -c[1]*h1 + c[2]*h0 + c[3]*h1;
            tmp = _mm256_add_ps(tmp, _mm256_shuffle_ps(tmp, tmp, _MM_SHUFFLE(0,1,2,3)));

            // smp = smp - h0*c[0] - h1*c[1] + h0*c[2] + h1*c[3];
            smp = _mm256_add_ss(smp, tmp);
            _mm256_store_ss(d++, smp);
         }
         while (--i);

         h = _mm256_shuffle_ps(h, h, _MM_SHUFFLE(3,1,2,0));
         _mm256_storel_pi((__m64*)hist, h);

         hist += 2;
         cptr += 4;
         mk = _mm256_set_ss(1.0f);
         s = dptr;
      }
      while (--stages);
   }
}
#endif


#if !RB_FLOAT_DATA
static inline void
_aaxBufResampleSkip_avx2(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_aaxBufResampleNearest_avx2(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_aaxBufResampleLinear_avx2(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_aaxBufResampleCubic_avx2(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_batch_resample_avx2(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_avx2(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_avx2(d, s, dmin, dmax, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleSkip_avx2(d, s, dmin, dmax, smu, fact);
   } else {
//    _aaxBufResampleNearest_avx2(d, s, dmin, dmax, smu, fact);
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}
#else

static inline void
_aaxBufResampleSkip_float_avx2(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_aaxBufResampleNearest_float_avx2(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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

static inline void
_aaxBufResampleLinear_float_avx2(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_aaxBufResampleCubic_float_avx2(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_batch_resample_float_avx2(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_float_avx2(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_float_avx2(d, s, dmin, dmax, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleSkip_float_avx2(d, s, dmin, dmax, smu, fact);
   } else {
//    _aaxBufResampleNearest_float_avx2(d, s, dmin, dmax, smu, fact);
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}
#endif // RB_FLOAT_DATA

#else
typedef int make_iso_compilers_happy;
#endif /* AVX2 */

