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

#include <software/rbuf_int.h>
#include "arch2d_simd.h"

#ifdef __SSE3__

void
_batch_imul_value_sse3(void* data, unsigned bps, unsigned int num, float f)
{
   unsigned int i = num;

   if (num)
   {
      switch (bps)
      {
      case 1:
      {
         int8_t* d = (int8_t*)data;
         do {
            *d++ *= f;
         }
         while (--i);
         break;
      }
      case 2:
      {
         int16_t* d = (int16_t*)data;
         do {
            *d++ *= f;
         }
         while (--i);
         break;
         }
      case 4:
      {
         int32_t* d = (int32_t*)data;
         do {
            *d++ *= f;
         }
         while (--i);
         break;
      }
      default:
         break;
      }
   }
}

void
_batch_fmul_value_sse3(void* data, unsigned bps, unsigned int num, float f)
{
   unsigned int i = num;

   if (num)
   {
      switch (bps)
      {
      case 4:
      {
         float *d = (float*)data;
         do {
            *d++ *= f;
         }
         while (--i);
         break;
      }
      case 8:
      {
         double *d = (double*)data;
         do {
            *d++ *= f;
         }
         while (--i);
         break;
      }
      default:
         break;
      }
   }
}

#if !RB_FLOAT_DATA
static inline void
_aaxBufResampleSkip_sse3(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
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
   if (i)
   {
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
}

static inline void
_aaxBufResampleNearest_sse3(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
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
_aaxBufResampleLinear_sse3(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
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

   samp = *sptr++;              // n
   dsamp = *sptr - samp;        // (n+1) - n

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


/** NOTE: instruction sequence is important for execution speed! */
static inline void
_aaxBufResampleCubic_sse3(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   const __m128 y0m = _mm_set_ps( 0.0f, 1.0f, 0.0f, 0.0f);
   const __m128 y1m = _mm_set_ps(-1.0f, 0.0f, 1.0f, 0.0f);
   const __m128 y2m = _mm_set_ps( 2.0f,-2.0f, 1.0f,-1.0f);
   const __m128 y3m = _mm_set_ps(-1.0f, 1.0f,-1.0f, 1.0f);
   __m128 y0123, a0123, xsmu;
   __m128 vm0, vm1, vm2, vm3;
   __m128 xtmp, xtmp1, xtmp2;
   __m128i xmm0i;
   int32_ptr sptr = (int32_ptr)s;
   int32_ptr dptr = d;
   unsigned int i;
   float smu2;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

   sptr += sdesamps;
   dptr += dmin;

   if (((size_t)sptr & 0xF) == 0) {
      xmm0i = _mm_load_si128((__m128i*)sptr);
   } else {
      xmm0i = _mm_loadu_si128((__m128i*)sptr);
   }
   y0123 = _mm_cvtepi32_ps(xmm0i);

   /* 
    * a0 = y3 - y2 - y0 + y1;
    * a1 = y0 - y1 - a0;
    * a2 = y2 - y0;
    * a3 = y1;
    */
   vm0 = _mm_mul_ps(y0123, y0m);
   vm1 = _mm_mul_ps(y0123, y1m);
   xtmp = _mm_hadd_ps(vm0, vm1);

   sptr += 4;

   vm2 = _mm_mul_ps(y0123, y2m);
   vm3 = _mm_mul_ps(y0123, y3m);
   xtmp1 = _mm_hadd_ps(vm2, vm3);

   /* work in advance */
   smu2 = smu*smu;
   xsmu = _mm_set_ps(smu*smu*smu, smu*smu, smu, 1.0f);

   a0123 = _mm_hadd_ps(xtmp, xtmp1);
   y0123 = _mm_shuffle_ps(y0123, y0123, _MM_SHUFFLE(2, 1, 0, 0));
   i = dmax-dmin;
   if (i)
   {
      do
      {
         xtmp = _mm_mul_ps(a0123, xsmu);
         xtmp = _mm_hadd_ps(xtmp, xtmp);

         smu += freq_factor;

         xtmp = _mm_hadd_ps(xtmp, xtmp);

         if (smu >= 1.0)
         {
            y0123 = _mm_cvtsi32_ss(y0123, *sptr++);

            /* 
             * a0 = y3 - y2 - y0 + y1;
             * a1 = y0 - y1 - a0;
             * a2 = y2 - y0;
             * a3 = y1;
             */
            vm0 = _mm_mul_ps(y0123, y0m);
            vm1 = _mm_mul_ps(y0123, y1m);
            xtmp1 = _mm_hadd_ps(vm0, vm1);

            smu--;

            vm2 = _mm_mul_ps(y0123, y2m);
            vm3 = _mm_mul_ps(y0123, y3m);
            xtmp2 = _mm_hadd_ps(vm2, vm3);

            /* work in advance */
            smu2 = smu*smu;
            xsmu = _mm_set_ps(smu*smu2, smu2, smu, 1.0f);

            a0123 = _mm_hadd_ps(xtmp1, xtmp2);
            y0123 = _mm_shuffle_ps(y0123, y0123, _MM_SHUFFLE(2, 1, 0, 3));
         }

         xsmu = _mm_set_ps(smu*smu2, smu2, smu, 1.0f);

         *dptr++ = _mm_cvttss_si32(xtmp);
      }
      while (--i);
   }
}

void
_batch_resample_sse3(int32_ptr d, const_int32_ptr s, unsigned int dmin, unsigned int dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_sse3(d, s, dmin, dmax, 0, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_sse3(d, s, dmin, dmax, 0, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleSkip_sse3(d, s, dmin, dmax, 0, smu, fact);
   } else {
      _aaxBufResampleNearest_sse3(d, s, dmin, dmax, 0, smu, fact);
   }
} 
#else

static inline void
_aaxBufResampleSkip_float_sse3(float32_ptr d, const_float32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   float32_ptr sptr = (float32_ptr)s;
   float32_ptr dptr = d;
   float samp, dsamp;
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
   if (i)
   {
      do
      {
         int step;

         *dptr++ = samp + (dsamp * smu);

         smu += freq_factor;
         step = (int)floorf(smu);

         smu -= step;
         sptr += step-1;
         samp = *sptr++;
         dsamp = *sptr - samp;
      }
      while (--i);
   }
}

static inline void
_aaxBufResampleNearest_float_sse3(float32_ptr d, const_float32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   if (freq_factor == 1.0f) {
      _aax_memcpy(d+dmin, s+sdesamps, (dmax-dmin)*sizeof(float));
   }
   else
   {
      float32_ptr sptr = (float32_ptr)s;
      float32_ptr dptr = d;
      unsigned int i;

      assert(s != 0);
      assert(d != 0);
      assert(dmin < dmax);
      assert(0.95f <= freq_factor && freq_factor <= 1.05f);
      assert(0.0f <= smu && smu < 1.0f);

      sptr += sdesamps;
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
_aaxBufResampleLinear_float_sse3(float32_ptr d, const_float32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   float32_ptr sptr = (float32_ptr)s;
   float32_ptr dptr = d;
   float samp, dsamp;
   unsigned int i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor < 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   sptr += sdesamps;
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
_aaxBufResampleCubic_float_sse3(float32_ptr d, const_float32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   const __m128 y0m = _mm_set_ps( 0.0f, 1.0f, 0.0f, 0.0f);
   const __m128 y1m = _mm_set_ps(-1.0f, 0.0f, 1.0f, 0.0f);
   const __m128 y2m = _mm_set_ps( 2.0f,-2.0f, 1.0f,-1.0f);
   const __m128 y3m = _mm_set_ps(-1.0f, 1.0f,-1.0f, 1.0f);
   __m128 y0123, a0123, xsmu;
   __m128 vm0, vm1, vm2, vm3;
   __m128 xtmp, xtmp1, xtmp2;
   float32_ptr sptr = (float32_ptr)s;
   float32_ptr dptr = d;
   unsigned int i;
   float smu2;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

   sptr += sdesamps;
   dptr += dmin;

   if (((size_t)sptr & 0xF) == 0) {
      y0123 = _mm_load_ps((float*)sptr);
   } else {
      y0123 = _mm_loadu_ps((float*)sptr);
   }

   /* 
    * a0 = y3 - y2 - y0 + y1;
    * a1 = y0 - y1 - a0;
    * a2 = y2 - y0;
    * a3 = y1;
    */
   vm0 = _mm_mul_ps(y0123, y0m);
   vm1 = _mm_mul_ps(y0123, y1m);
   vm2 = _mm_mul_ps(y0123, y2m);
   vm3 = _mm_mul_ps(y0123, y3m);

   sptr += 4;
   smu2 = smu*smu;

   xtmp = _mm_hadd_ps(vm0, vm1);
   xtmp1 = _mm_hadd_ps(vm2, vm3);

   sptr += 4;

   /* work in advance */
   xsmu = _mm_set_ps(smu*smu*smu, smu*smu, smu, 1.0f);

   a0123 = _mm_hadd_ps(xtmp, xtmp1);
   y0123 = _mm_shuffle_ps(y0123, y0123, _MM_SHUFFLE(2, 1, 0, 0));
   i = dmax-dmin;
   if (i)
   {
      do
      {
         xtmp = _mm_mul_ps(a0123, xsmu);
         xtmp = _mm_hadd_ps(xtmp, xtmp);

         smu += freq_factor;

         xtmp = _mm_hadd_ps(xtmp, xtmp);

         if (smu >= 1.0)
         {
            y0123 = _mm_move_ss(y0123, _mm_load_ss((float*)sptr++));

            /* 
             * a0 = y3 - y2 - y0 + y1;
             * a1 = y0 - y1 - a0;
             * a2 = y2 - y0;
             * a3 = y1;
             */
            vm0 = _mm_mul_ps(y0123, y0m);
            vm1 = _mm_mul_ps(y0123, y1m);
            vm2 = _mm_mul_ps(y0123, y2m);
            vm3 = _mm_mul_ps(y0123, y3m);

            smu--;

            xtmp1 = _mm_hadd_ps(vm0, vm1);
            xtmp2 = _mm_hadd_ps(vm2, vm3);

            /* work in advance */
            smu2 = smu*smu;
            xsmu = _mm_set_ps(smu*smu2, smu2, smu, 1.0f);

            a0123 = _mm_hadd_ps(xtmp1, xtmp2);
            y0123 = _mm_shuffle_ps(y0123, y0123, _MM_SHUFFLE(2, 1, 0, 3));
         }

         xsmu = _mm_set_ps(smu*smu2, smu2, smu, 1.0f);

         _mm_store_ss((float*)dptr++, xtmp);
      }
      while (--i);
   }
}

void
_batch_resample_float_sse3(float32_ptr d, const_float32_ptr s, unsigned int dmin, unsigned int dmax, float smu, float fact)
{
   assert(fact > 0.0f);
   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_float_sse3(d, s, dmin, dmax, 0, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_float_sse3(d, s, dmin, dmax, 0, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleSkip_float_sse3(d, s, dmin, dmax, 0, smu, fact);
   } else {
      _aaxBufResampleNearest_float_sse3(d, s, dmin, dmax, 0, smu, fact);
   }
}
#endif // RB_FLOAT_DATA

#endif /* SSE3 */

