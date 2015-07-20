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
#include "software/cpu/arch2d_simd.h"

#if !RB_FLOAT_DATA
static void
_aaxBufResampleSkip_avx(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, size_t sdesamps, float smu, float freq_factor)
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

   sptr += sdesamps;
   dptr += dmin;

   samp = *sptr++;              // n+(step-1)
   dsamp = *sptr - samp;        // (n+1) - n


   i=dmax-dmin;
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

#if 0
static void
_aaxBufResampleNearest_avx(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, size_t sdesamps, float smu, float freq_factor)
{
   if (freq_factor == 1.0f) {
      _aax_memcpy(d+dmin, s+sdesamps, (dmax-dmin)*sizeof(int32_t));
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
#endif

static void
_aaxBufResampleLinear_avx(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, size_t sdesamps, float smu, float freq_factor)
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


static void
_aaxBufResampleCubic_avx(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, size_t sdesamps, float smu, float freq_factor)
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

void
_batch_resample_avx(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_avx(d, s, dmin, dmax, 0, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_avx(d, s, dmin, dmax, 0, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleSkip_avx(d, s, dmin, dmax, 0, smu, fact);
   } else {
//    _aaxBufResampleNearest_avx(d, s, dmin, dmax, 0, smu, fact);
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}

#else



static inline void
_aaxBufResampleSkip_float_avx(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_aaxBufResampleNearest_float_avx(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_aaxBufResampleLinear_float_avx(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_aaxBufResampleCubic_float_avx(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
            smu--;            a0 += y0;
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
_batch_resample_float_avx(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_float_avx(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_float_avx(d, s, dmin, dmax, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleSkip_float_avx(d, s, dmin, dmax, smu, fact);
   } else {
//    _aaxBufResampleNearest_float_avx(d, s, dmin, dmax, smu, fact);
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}
#endif // RB_FLOAT_DATA


