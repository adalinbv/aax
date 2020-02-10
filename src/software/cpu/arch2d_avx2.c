/*
 * Copyright 2005-2020 by Erik Hofman.
 * Copyright 2009-2020 by Adalin B.V.
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

#ifdef __AVX2__

inline float    // range -1.0f .. 1.0f
fast_sin_avx2(float x)
{
   return -4.0f*(x - x*fabsf(x));
}

static inline FN_PREALIGN float
hsum_ps_avx2(__m128 v) {
   __m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
   __m128 sums = _mm_add_ps(v, shuf);
   shuf = _mm_movehl_ps(shuf, sums);
   sums = _mm_add_ss(sums, shuf);
   return _mm_cvtss_f32(sums);
}

static inline __m128
_mm_abs_ps(__m128 x) {
   return _mm_andnot_ps(_mm_set1_ps(-0.0f), x);
}

static inline int
_mm_testz_ps_avx2(__m128 x)
{
   __m128i zero = _mm_setzero_si128();
   return (_mm_movemask_epi8(_mm_cmpeq_epi32(_mm_castps_si128(x), zero)) != 0xFFFF);
}

static inline __m128    // range -1.0f .. 1.0f
fast_sin4_avx2(__m128 x)
{
   __m128 four = _mm_set1_ps(-4.0f);
   return _mm_mul_ps(four, _mm_sub_ps(x, _mm_mul_ps(x, _mm_abs_ps(x))));
}

float *
_aax_generate_waveform_avx2(float32_ptr rv, size_t no_samples, float freq, float phase, enum wave_types wtype)
{
   const_float32_ptr harmonics = _harmonics[wtype];
   if (wtype == _SINE_WAVE || wtype == _CONSTANT_VALUE) {
      rv = _aax_generate_waveform_cpu(rv, no_samples, freq, phase, wtype);
   }
   else if (rv)
   {
      __m128 phase4, freq4, h4;
      __m128 one, two, four;
      __m128 ngain, nfreq;
      __m128 hdt, s;
      unsigned int i, h;
      float *ptr;

      assert(MAX_HARMONICS % 4 == 0);

      one = _mm_set1_ps(1.0f);
      two = _mm_set1_ps(2.0f);
      four = _mm_set1_ps(4.0f);

      phase4 = _mm_set1_ps(-1.0f + phase/GMATH_PI);
      freq4 = _mm_set1_ps(freq);
      h4 = _mm_set_ps(4.0f, 3.0f, 2.0f, 1.0f);

      nfreq = _mm_div_ps(freq4, h4);
      ngain = _mm_and_ps(_mm_cmplt_ps(two, nfreq), _mm_load_ps(harmonics));
      hdt = _mm_div_ps(two, nfreq);

      ptr = rv;
      i = no_samples;
      s = phase4;
      do
      {
         __m128 rv = fast_sin4_avx2(s);

         *ptr++ = hsum_ps_avx2(_mm_mul_ps(ngain, rv));

         s = _mm_add_ps(s, hdt);
         s = _mm_sub_ps(s, _mm_and_ps(two, _mm_cmpge_ps(s, one)));
      }
      while (--i);

      h4 = _mm_add_ps(h4, four);;
      for(h=4; h<MAX_HARMONICS; h += 4)
      {
         nfreq = _mm_div_ps(freq4, h4);
         ngain = _mm_and_ps(_mm_cmplt_ps(two, nfreq), _mm_load_ps(harmonics+h));
         if (_mm_testz_ps_avx2(ngain))
         {
            hdt = _mm_div_ps(two, nfreq);

            ptr = rv;
            i = no_samples;
            s = phase4;
            do
            {
               __m128 rv = fast_sin4_avx2(s);

               *ptr++ += hsum_ps_avx2(_mm_mul_ps(ngain, rv));

               s = _mm_add_ps(s, hdt);
               s = _mm_sub_ps(s, _mm_and_ps(two, _mm_cmpge_ps(s, one)));
            }
            while (--i);
         }
         h4 = _mm_add_ps(h4, four);
      }
   }
   return rv;
}

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

   if (!num) return;

   _batch_dither_cpu(s, 2, num);

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

   if (num)
   {
      i = num;
      do {
         *d++ = *s++ >> 8;
      } while (--i);
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
   size_t t, tmp;

   if (!num) return;

   for (t=0; t<tracks; t++)
   {
      int32_t *s = (int32_t *)src[t] + offset;
      _batch_dither_cpu(s, 2, num);
   }

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


#else
typedef int make_iso_compilers_happy;
#endif /* AVX2 */

