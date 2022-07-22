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

#include "software/rbuf_int.h"
#include "software/cpu/arch2d_simd.h"

#ifdef __FMA__

FN_PREALIGN void
_batch_fmadd_fma3(float32_ptr dst, const_float32_ptr src, size_t num, float v, float vstep)
{
   int need_step = (fabsf(vstep) <= LEVEL_90DB) ? 0 : 1;
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

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

      step = 2*sizeof(__m256)/sizeof(float);
      i = num/step;
      if (i)
      {
         __m256 ymm0, ymm1, dv, tv0, tv1, dvstep0, dvstep1;
         __m256 *sptr = (__m256 *)s;
         __m256 *dptr = (__m256 *)d;

         assert(step == 8);
         dvstep0 = _mm256_set_ps(7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f);
         dvstep1 = _mm256_set_ps(15.0f, 14.0f, 13.0f, 12.0f, 11.0f, 10.0f, 9.0f, 8.0f);
         dvstep0 = _mm256_mul_ps(dvstep0, _mm256_set1_ps(vstep));
         dvstep1 = _mm256_mul_ps(dvstep1, _mm256_set1_ps(vstep));

         dv = _mm256_set1_ps(vstep*step);
         tv0 = _mm256_add_ps(_mm256_set1_ps(v), dvstep0);
         tv1 = _mm256_add_ps(_mm256_set1_ps(v), dvstep1);
         v += i*step*vstep;

         num -= i*step;
         s += i*step;
         d += i*step;
         if (stmp)
         {
            do
            {
               ymm0 = _mm256_loadu_ps((const float*)sptr++);
               ymm1 = _mm256_loadu_ps((const float*)sptr++);
               ymm0 =_mm256_fmadd_ps(ymm0, tv0, _mm256_load_ps((const float*)dptr));
               ymm1 =_mm256_fmadd_ps(ymm1, tv1, _mm256_load_ps((const float*)(dptr+1)));
               tv0 = _mm256_add_ps(tv0, dv);
               tv1 = _mm256_add_ps(tv1, dv);

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
               ymm0 =_mm256_fmadd_ps(ymm0, tv0, _mm256_load_ps((const float*)dptr));
               ymm1 =_mm256_fmadd_ps(ymm1, tv1, _mm256_load_ps((const float*)(dptr+1)));
               tv0 = _mm256_add_ps(tv0, dv);
               tv1 = _mm256_add_ps(tv1, dv);

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
            *d++ += *s++ * v;
            v += vstep;
         } while(--i);
      }
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
         if ( stmp)
         {
            __m256 tv = _mm256_set1_ps(v);
            do
            {
               ymm0 = _mm256_loadu_ps((const float*)sptr++);
               ymm1 = _mm256_loadu_ps((const float*)sptr++);

               ymm0 =_mm256_fmadd_ps(ymm0, tv, _mm256_load_ps((const float*)(dptr+0)));
               ymm1 =_mm256_fmadd_ps(ymm1, tv, _mm256_load_ps((const float*)(dptr+1)));
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
               ymm0 = _mm256_load_ps((const float*)sptr++);
               ymm1 = _mm256_load_ps((const float*)sptr++);

               ymm0 =_mm256_fmadd_ps(ymm0, tv, _mm256_load_ps((const float*)(dptr+0)));
               ymm1 =_mm256_fmadd_ps(ymm1, tv, _mm256_load_ps((const float*)(dptr+1)));
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
   }
}

void
_batch_freqfilter_float_fma3(float32_ptr dptr, const_float32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   const_float32_ptr s = sptr;

   if (num)
   {
      float k, *cptr, *hist;
      float h0, h1;
      int stage;

      if (filter->state == AAX_BESSEL) {
         k = filter->k * (filter->high_gain - filter->low_gain);
      } else {
         k = filter->k * filter->high_gain;
      }

      if (fabsf(k-1.0f) < LEVEL_96DB)
      {
         if (dptr != sptr) memcpy(dptr, sptr, num*sizeof(float));
         return;
      }
      if (fabsf(k) < LEVEL_96DB && filter->no_stages < 2)
      {
         memset(dptr, 0, num*sizeof(float));
         return;
      }

      cptr = filter->coeff;
      hist = filter->freqfilter->history[t];
      stage = filter->no_stages;
      if (!stage) stage++;

      assert(((size_t)cptr & MEMMASK16) == 0);

      h0 = hist[0];
      h1 = hist[1];

      if (filter->state == AAX_BUTTERWORTH)
      {
         float32_ptr d = dptr;
         size_t i = num;

         do
         {
            float nsmp = (*s++ * k) + h0 * cptr[0] + h1 * cptr[1];
            *d++ = nsmp             + h0 * cptr[2] + h1 * cptr[3];

            h1 = h0;
            h0 = nsmp;
         }
         while (--i);
      }
      else
      {
         float32_ptr d = dptr;
         size_t i = num;

         do
         {
            float smp = (*s++ * k) + ((h0 * cptr[0]) + (h1 * cptr[1]));
            *d++ = smp;

            h1 = h0;
            h0 = smp;
         }
         while (--i);
      }

      *hist++ = h0;
      *hist++ = h1;

      while(--stage)
      {
         cptr += 4;

         h0 = hist[0];
         h1 = hist[1];

         if (filter->state == AAX_BUTTERWORTH)
         {
            float32_ptr d = dptr;
            size_t i = num;

            do
            {
               float nsmp = *d + h0 * cptr[0] + h1 * cptr[1];
               *d++ = nsmp     + h0 * cptr[2] + h1 * cptr[3];

               h1 = h0;
               h0 = nsmp;
            }
            while (--i);
         }
         else
         {
            float32_ptr d = dptr;
            size_t i = num;

            do
            {
               float smp = *d + h0 * cptr[0] + h1 * cptr[1];
               *d++ = smp;

               h1 = h0;
               h0 = smp;
            }
            while (--i);
         }

         *hist++ = h0;
         *hist++ = h1;
      }
   }
}

static inline void
_aaxBufResampleDecimate_float_fma3(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
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

   samp = *s++;                 // n+(step-1)
   dsamp = *s - samp;           // (n+1) - n

   i = dmax-dmin;
   if (i)
   {
      if (freq_factor == 2.0f)
      {
         do {
            *d++ = (*s + *(s+1))*0.5f;
            s += 2;
         }
         while (--i);
      }
      else
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
}

static inline void
_aaxBufResampleLinear_float_fma3(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
      __m128 samp = _mm_load_ss(sptr++);       // n
      __m128 nsamp = _mm_load_ss(sptr++);      // (n+1)
      __m128 dsamp = _mm_sub_ss(nsamp, samp);  // (n+1) - n

      do
      {
         __m128 tau = _mm_set_ss(smu);
         __m128 dout = samp;

         smu += freq_factor;

         // fmadd
         dout = _mm_fmadd_ss(tau, dsamp, dout);

         if (smu >= 1.0)
         {
            samp = nsamp;
            nsamp = _mm_load_ss(sptr++);

            smu -= 1.0;;

            dsamp = _mm_sub_ss(nsamp, samp);
         }
         _mm_store_ss(dptr++, dout);
      }
      while (--i);
   }
}


static inline void
_aaxBufResampleCubic_float_fma3(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float32_ptr sptr = (float32_ptr)s;
   float32_ptr dptr = d;
   float y0, y1, y2, y3, a0, a1, a2;
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
_batch_resample_float_fma3(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);
   assert(d != s);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_float_fma3(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_float_fma3(d, s, dmin, dmax, smu, fact);
   }
   else if (fact >= 1.0f) {
      _aaxBufResampleDecimate_float_fma3(d, s, dmin, dmax, smu, fact);
   } else {
//    _aaxBufResampleNearest_float_fma3(d, s, dmin, dmax, smu, fact);
      memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}


#else
typedef int make_iso_compilers_happy;
#endif /* __FMA__ */

