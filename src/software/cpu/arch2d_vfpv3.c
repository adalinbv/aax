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

#include <math.h>	/* rinft */

#include "software/rbuf_int.h"
#include "arch2d_simd.h"

#ifdef __ARM_VFPV3
void
_batch_fmadd_vfpv3(float32_ptr dptr, const_float32_ptr sptr, size_t num, float v, float vstep)
{
   float *s = (float*)sptr;
   float *d = dptr;
   size_t i = num;

   if (!num || (v <= LEVEL_128DB && vstep <= LEVEL_128DB)) return;

   if (fabsf(v - 1.0f) < LEVEL_96DB && vstep <=  LEVEL_96DB) 
   {
      do {
         *d++ += *s++;
         v += vstep;
      }
      while (--i);
   }
   else
   {
      do {
         *d++ += *s++ * v;
         v += vstep;
      }
      while (--i);
   }
}

void
_batch_imul_value_vfpv3(void* data, unsigned bps, size_t num, float f)
{
   size_t i = num;
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
_batch_fmul_value_vfpv3(void* data, unsigned bps, size_t num, float f)
{
   if (!num || fabsf(f - 1.0f) < LEVEL_96DB) return;

   if (f <= LEVEL_128DB) {
      memset(data, 0, num*bps);
   }
   else if (num)
   {
      size_t  i = num;

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

void
_batch_cvt24_ps24_vfpv3(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      int32_t* d = (int32_t*)dptr;
      float* s = (float*)sptr;
      size_t i = num;

      do {
         *d++ = (int32_t)*s++;
      } while (--i);
   }
}

void
_batch_cvtps24_24_vfpv3(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      float* d = (float*)dptr;
      int32_t* s = (int32_t*)sptr;
      size_t i = num;

      do {
         *d++ = (float)*s++;
      } while (--i);
   }
}

void
_batch_cvt24_ps_vfpv3(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      static const float mul = (float)(1<<23);
      int32_t* d = (int32_t*)dptr;
      float* s = (float*)sptr;
      size_t i = num;

      do {
         *d++ = (int32_t)(*s++ * mul);
      } while (--i);
   }
}

void
_batch_cvtps_24_vfpv3(void_ptr dst, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      static const float mul = 1.0f/(float)(1<<23);
      int32_t* s = (int32_t*)sptr;
      float* d = (float*)dst;
      size_t i = num;

      do {
         *d++ = (float)*s++ * mul;
      } while (--i);
   }
}

void
_batch_cvt24_pd_vfpv3(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      static const double mul = (double)(1<<23);
      int32_t* d = (int32_t*)dptr;
      double* s = (double*)sptr;
      size_t i = num;

      do {
         *d++ = (int32_t)(*s++ * mul);
      } while (--i);
   }
}

void
_batch_cvt24_ps_intl_vfpv3(int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      if (tracks == 1) {
         _batch_cvt24_ps(dptr[0]+offset, sptr, num);
      }
      else if (tracks)
      {
         static const float mul = (float)(1<<23);
         size_t t;

         for (t=0; t<tracks; t++)
         {
            float *s = (float*)sptr + t;
            int32_t *d = dptr[t] + offset;
            size_t i = num;

            do {
               *d++ = (int32_t)(*s * mul);
               s += tracks;
            }
            while (--i);
         }
      }
   }
}

void
_batch_cvt24_pd_intl_vfpv3(int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      if (tracks == 1) {
         _batch_cvt24_pd(dptr[0]+offset, sptr, num);
      }
      else if (tracks)
      {
         static const double mul = (double)(1<<23);
         size_t t;
         for (t=0; t<tracks; t++)
         {
            double *s = (double*)sptr + t;
            int32_t *d = dptr[t] + offset;
            size_t i = num;

            do {
               *d++ = (int32_t)(*s * mul);
               s += tracks;
            }
            while (--i);
         }
      }
   }
}

void
_batch_cvtpd_24_vfpv3(void_ptr dst, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      static const double mul = 1.0/(double)(1<<23);
      int32_t* s = (int32_t*)sptr;
      double* d = (double*)dst;
      size_t i = num;
      do {
         *d++ = (double)*s++ * mul;
      } while (--i);
   }
}

void
_batch_cvt24_8_vfpv3(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      int8_t* s = (int8_t*)sptr;
      int32_t* d = dptr;
      size_t i = num;

      do {
         *d++ = (int32_t)*s++ << 16;
      }
      while (--i);
   }
}

void
_batch_cvtps_intl_24_vfpv3(void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      static const float mul = 1.0/(float)(1<<23);
      size_t t;

      for (t=0; t<tracks; t++)
      {
         int32_t *s = (int32_t*)sptr[t] + offset;
         float *d = (float*)dptr + t;
         size_t i = num;

         do
         {
            *d = (float)*s++ * mul;
            d += tracks;
         }
         while (--i);
      }
   }
}

void
_batch_cvtpd_intl_24_vfpv3(void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      static const double mul = 1.0/(double)(1<<23);
      size_t t;

      for (t=0; t<tracks; t++)
      {
         int32_t *s = (int32_t *)sptr[t] + offset;
         double *d = (double*)dptr + t;
         size_t i = num;

         do 
         {
            *d = (double)*s++ * mul;
            d += tracks;
         }
         while (--i);
      }
   }
}

void
_batch_ema_iir_float_vfpv3(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float a1)
{
   if (num)
   {
      float32_ptr s = (float32_ptr)sptr;
      size_t i = num;
      float smp;

      smp = *hist;
      do
      {
         smp += a1*(*s++ - smp);
         *d++ = smp;
      }
      while (--i);
      *hist = smp;
   }
}

void
_batch_freqfilter_vfpv3(int32_ptr dptr, const_int32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   const_int32_ptr s = sptr;

   if (num)
   {
      float k, *cptr, *hist;
      float smp, h0, h1;
      int stages;

      if (filter->state == AAX_BESSEL) {
         k = filter->k * (filter->high_gain - filter->low_gain);
      } else {
         k = filter->k * filter->high_gain;
      }

      if (fabsf(k-1.0f) < LEVEL_96DB) 
      {
         memcpy(dptr, sptr, num*sizeof(float));
         return;
      }
      if (fabsf(k) < LEVEL_96DB)
      {
         memset(dptr, 0, num*sizeof(float));
         return;
      }

      cptr = filter->coeff;
      hist = filter->freqfilter->history[t];
      stages = filter->no_stages;
      if (!stages) stages++;

      do
      {
         int32_ptr d = dptr;
         size_t i = num;

         h0 = hist[0];
         h1 = hist[1];

         if (filter->state == AAX_BUTTERWORTH)
         {
            do
            {
               smp = (*s++ * k) + hist[0] * cptr[0] + hist[1] * cptr[1];
               *d++ = smp       + hist[0] * cptr[2] + hist[1] * cptr[3];

               h1 = h0;
               h0 = smp;
            }
            while (--i);
         }
         else
         {
            do
            {
               smp = (*s++ * k) + ((h0 * cptr[0]) + (h1 * cptr[1]));
               *d++ = smp;

               h1 = h0;
               h0 = smp;
            }
            while (--i);
         }

         *hist++ = h0;
         *hist++ = h1;
         k = 1.0f;
         s = dptr;
      }
      while (--stages);
   }
}

void
_batch_freqfilter_float_vfpv3(float32_ptr dptr, const_float32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   const_float32_ptr s = sptr;

   if (num)
   {
      float k, *cptr, *hist;
      float c0, c1, c2, c3;
      float smp, h0, h1;
      int stages;

      if (filter->state == AAX_BESSEL) {
         k = filter->k * (filter->high_gain - filter->low_gain);
      } else {
         k = filter->k * filter->high_gain;
      }

      if (fabsf(k-1.0f) < LEVEL_96DB)
      {
         memcpy(dptr, sptr, num*sizeof(float));
         return;
      }
      if (fabsf(k) < LEVEL_96DB)
      {
         memset(dptr, 0, num*sizeof(float));
         return;
      }

      cptr = filter->coeff;
      hist = filter->freqfilter->history[t];
      stage = filter->no_stages;
      if (!stage) stage++;

      do
      {
         float32_ptr d = dptr;
         size_t i = num;

         h0 = hist[0];
         h1 = hist[1];

         if (filter->state == AAX_BUTTERWORTH)
         {
            do
            {
               smp = (*s++ * k) + h0 * cptr[0] + h1 * cptr[1];
               *d++ = smp       + h0 * cptr[2] + h1 * cptr[3];

               h1 = h0;
               h0 = smp;
            }
            while (--i);
         }
         else
         {
            do
            {
               smp = (*s++ * k) + ((h0 * cptr[0]) + (h1 * cptr[1]));
               *d++ = smp;

               h1 = h0;
               h0 = smp;
            }
            while (--i);
         }

         *hist++ = h0;
         *hist++ = h1;
         k = 1.0f;
         s = dptr;
      }
      while (--stages);
   }
}


/**
 * A mixer callback function mixes the audio from one mono source track and
 * the already existing audio in the mono destination track. The result is
 * presented in the destination buffer.
 *
 * @d 32-bit destination buffer
 * @s 32-bit source buffer
 * @dmin starting sample position in the dest buffer
 * @dmax last sample position in the dest buffer
 * @smin sample position where to return to in case of sample looping
 * @smu normalised starting position
 * @freq_factor stepsize in the src buffer for one step in the dest buffer.
 * @smax last sample position in the src buffer
 *
 * Note: smax is only used in the *Loop mixing functions
 */
# if !RB_FLOAT_DATA
static inline void
_aaxBufResampleDecimate_vfpv3(int32_ptr dptr, const_int32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   int32_ptr s = (int32_ptr)sptr;
   int32_ptr d = dptr;
   int32_t samp, dsamp;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor >= 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   d += dmin;

   samp = *s++;			// n+(step-1)
   dsamp = *s - samp;		// (n+1) - n

   i=dmax-dmin;
   if (i)
   {
      do
      {
         size_t step;

         *d++ = samp + (int32_t)(dsamp * smu);

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
_aaxBufResampleNearest_vfpv3(int32_ptr dptr, const_int32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   if (freq_factor == 1.0f) {
      _aax_memcpy(dptr+dmin, sptr, (dmax-dmin)*sizeof(int32_t));
   }
   else
   {
      int32_ptr s = (int32_ptr)sptr;
      int32_ptr d = dptr;
      size_t i;

      assert(s != 0);
      assert(d != 0);
      assert(dmin < dmax);
      assert(0.95f <= freq_factor && freq_factor <= 1.05f);
      assert(0.0f <= smu && smu < 1.0f);

      d += dmin;

      i = dmax-dmin;
      if (i)
      {
         do
         {
            *d++ = *s;

            smu += freq_factor;
            if (smu > 0.5f)
            {
               s++;
               smu -= 1.0f;
            }
         }
         while (--i);
      }
   }
}
#endif

static inline void
_aaxBufResampleLinear_vfpv3(int32_ptr dptr, const_int32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   int32_ptr s = (int32_ptr)sptr;
   int32_ptr d = dptr;
   int32_t samp, dsamp;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor < 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   d += dmin;

   samp = *s++;		// n
   dsamp = *s - samp;	// (n+1) - n

   i = dmax-dmin;
   if (i)
   {
      do
      {
         *d++ = samp + (int32_t)(dsamp * smu);

         smu += freq_factor;
         if (smu >= 1.0f)
         {
            smu -= 1.0f;
            samp = *s++;
            dsamp = *s - samp;
         }
      }
      while (--i);
   }

#  if 0
 printf("dptr: %x, d+dmax: %x, dptr-d: %i (%x)\n", d, dptr+dmax, d-dptr, samp);
 for (i=0; i<dmax; i++)
    if (d[i] != 0x333300) printf("->d[%i] = %x\n", i, d[i]);
#  endif
}

static inline void
_aaxBufResampleCubic_vfpv3(int32_ptr dptr, const_int32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float y0, y1, y2, y3, a0, a1, a2;
   int32_ptr s = (int32_ptr)sptr;
   int32_ptr d = dptr;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

   d += dmin;

   y0 = (float)*s++;
   y1 = (float)*s++;
   y2 = (float)*s++;
   y3 = (float)*s++;

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
         *d++ = (int32_t)ftmp;

         smu += freq_factor;
         if (smu >= 1.0f)
         {
            smu--;
#  if 0
            /* original code */
            s -= 3;
            y0 = *s++;
            y1 = *s++;
            y2 = *s++;
            y3 = *s++;

            a0 = y3 - y2 - y0 + y1;
            a1 = y0 - y1 - a0;
            a2 = y2 - y0;
#  else
            /* optimized code */
            a0 += y0;
            y0 = y1;
            y1 = y2;
            y2 = y3;
            y3 = (float)*s++;
            a0 = -a0 + y3;			/* a0 = y3 - y2 - y0 + y1; */
            a1 = y0 - y1 - a0;
            a2 = y2 - y0;
#  endif
         }
      }
      while (--i);
   }
}

void
_batch_resample_vfpv3(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_vfpv3(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_vfpv3(d, s, dmin, dmax, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleDecimate_vfpv3(d, s, dmin, dmax, smu, fact);
   } else {
//    _aaxBufResampleNearest_vfpv3(d, s, dmin, dmax, smu, fact);
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}

# else
static inline void
_aaxBufResampleDecimate_float_vfpv3(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
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

#if 0
static inline void
_aaxBufResampleNearest_float_vfpv3(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   if (freq_factor == 1.0f) {
      _aax_memcpy(dptr+dmin, sptr, (dmax-dmin)*sizeof(float));
   }
   else
   {
      float32_ptr s = (float32_ptr)sptr;
      float32_ptr d = dptr;
      size_t i;

      assert(s != 0);
      assert(d != 0);
      assert(dmin < dmax);
      assert(0.95f <= freq_factor && freq_factor <= 1.05f);
      assert(0.0f <= smu && smu < 1.0f);

      d += dmin;

      i = dmax-dmin;
      if (i)
      {
         do
         {
            *d++ = *s;

            smu += freq_factor;
            if (smu > 0.5f)
            {
               s++;
               smu -= 1.0f;
            }
         }
         while (--i);
      }
   }
}
#endif

static inline void
_aaxBufResampleLinear_float_vfpv3(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float32_ptr s = (float32_ptr)sptr;
   float32_ptr d = dptr;
   float samp, dsamp;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor < 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   d += dmin;

   samp = *s++;         // n
   dsamp = *s - samp;   // (n+1) - n

   i = dmax-dmin;
   if (i)
   {
      do
      {
         *d++ = samp + (dsamp * smu);

         smu += freq_factor;
         if (smu >= 1.0f)
         {
            smu -= 1.0f;
            samp = *s++;
            dsamp = *s - samp;
         }
      }
      while (--i);
   }

#  if 0
 printf("dptr: %x, d+dmax: %x, dptr-d: %i (%f)\n", d, dptr+dmax, d-dptr, samp);
#  endif
}

static inline void
_aaxBufResampleCubic_float_vfpv3(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float y0, y1, y2, y3, a0, a1, a2;
   float32_ptr s = (float32_ptr)sptr;
   float32_ptr d = dptr;
   size_t i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

   d += dmin;

   y0 = *s++;
   y1 = *s++;
   y2 = *s++;
   y3 = *s++;

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
         *d++ = ftmp;

         smu += freq_factor;
         if (smu >= 1.0f)
         {
            smu--;
#  if 0
            /* original code */
            /* original code */
            s -= 3;
            y0 = *s++;
            y1 = *s++;
            y2 = *s++;
            y3 = *s++;

            a0 = y3 - y2 - y0 + y1;
            a1 = y0 - y1 - a0;
            a2 = y2 - y0;
#  else
            /* optimized code */
            a0 += y0;
            y0 = y1;
            y1 = y2;
            y2 = y3;
            y3 = *s++;
            a0 = -a0 + y3;                      /* a0 = y3 - y2 - y0 + y1; */
            a1 = y0 - y1 - a0;
            a2 = y2 - y0;
#  endif
         }
      }
      while (--i);
   }
}

void
_batch_resample_float_vfpv3(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_float_vfpv3(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_float_vfpv3(d, s, dmin, dmax, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleDecimate_float_vfpv3(d, s, dmin, dmax, smu, fact);
   } else {
//    _aaxBufResampleNearest_float_vfpv3(d, s, dmin, dmax, smu, fact);
      _aax_memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}
# endif /* RB_FLOAT_DATA */

#else
typedef int make_iso_compilers_happy;
#endif /* __ARM_VFPV3 */

