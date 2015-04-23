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

#include "software/rbuf_int.h"
#include "arch2d_simd.h"

#ifdef __ARM_NEON__
#include <arm_neon.h>

/*
 * http://gcc.gnu.org/projects/prefetch.html
 *
 * ARMv7 has (from ARM Architecture Reference Manual):
 *  PLD data preload with intent to read
 *  PLDW data preload with intent to write
 *  PLI instruction preload
 *
 * ARMv8 AArch64 has a PRFM instruction with the following hints
 * (from LLVM and binutils code):
 *  PLD (data load), PLI (instruction), PST (data store)
 *  Level l1, l2, l3
 *  KEEP (retained), STRM (streaming)
 */

void
_batch_cvtps24_24_neon(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *s = (int32_t*)src;
   float *d = (float*)dst;
   size_t i, step;

   assert(s != 0);
   assert(d != 0);

   if (!num) return;

   step = sizeof(int32x4x4_t)/sizeof(int32_t);

   i = num/step;
   num -= i*step;
   if (i)
   {
      int32x4x4_t nir4;
      float32x4x4_t nfr4d;

      do
      {
         nir4 = vld4q_s32(s);
         s += 4*4;

         nfr4d.val[0] = vcvtq_f32_s32(nir4.val[0]);
         nfr4d.val[1] = vcvtq_f32_s32(nir4.val[1]);
         nfr4d.val[2] = vcvtq_f32_s32(nir4.val[2]);
         nfr4d.val[3] = vcvtq_f32_s32(nir4.val[3]);

         vst4q_f32(d, nfr4d);
         d += step;
      }
      while(--i);

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
_batch_cvt24_ps24_neon(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *d = (int32_t*)dst;
   float *s = (float*)src;
   size_t i, step;

   assert(s != 0);
   assert(d != 0);

   if (!num) return;

   step = sizeof(float32x4x4_t)/sizeof(float32_t);

   i = num/step;
   num -= i*step;
   if (i)
   {
      float32x4x4_t nir4;
      int32x4x4_t nfr4d;

      do
      {
         nir4 = vld4q_f32(s);
         s += 4*4;

         nfr4d.val[0] = vcvtq_s32_f32(nir4.val[0]);
         nfr4d.val[1] = vcvtq_s32_f32(nir4.val[1]);
         nfr4d.val[2] = vcvtq_s32_f32(nir4.val[2]);
         nfr4d.val[3] = vcvtq_s32_f32(nir4.val[3]);

         vst4q_s32(d, nfr4d);
         d += step;
      }
      while(--i);

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
_batch_imadd_neon(int32_ptr d, const_int32_ptr src, size_t num, float f, float fstep)
{
   int32_t *s = (int32_t *)src;
   size_t i, size, step;

   if (!num) return;

   step = 2*sizeof(int32x4_t)/sizeof(int32_t);

   fstep *= 4*4;                                  /* 8 samples at a time */
   i = size = num/step;
   if (i)
   {
      float32x4_t tv = vdupq_n_f32(f);
      int32x4x4_t nir4;

      do
      {
         float32x4_t nfr[4];
         int32x4x4_t nnir4;

         nir4 = vld4q_s32(s);
         nfr[0] = vcvtq_f32_s32(nir4.val[0]);
         nfr[1] = vcvtq_f32_s32(nir4.val[1]);
         nfr[2] = vcvtq_f32_s32(nir4.val[2]);
         nfr[3] = vcvtq_f32_s32(nir4.val[3]);

         nfr[0] = vmulq_f32(nfr[0], tv);
         nfr[1] = vmulq_f32(nfr[1], tv);
         nfr[2] = vmulq_f32(nfr[2], tv);
         nfr[3] = vmulq_f32(nfr[3], tv);

         f += fstep;

         nnir4.val[0] = vcvtq_s32_f32(nfr[0]);
         nnir4.val[1] = vcvtq_s32_f32(nfr[1]);
         nnir4.val[2] = vcvtq_s32_f32(nfr[2]);
         nnir4.val[3] = vcvtq_s32_f32(nfr[3]);

         nir4.val[0] = vaddq_s32(nir4.val[0], nnir4.val[0]);
         nir4.val[1] = vaddq_s32(nir4.val[1], nnir4.val[1]);
         nir4.val[2] = vaddq_s32(nir4.val[2], nnir4.val[2]);
         nir4.val[3] = vaddq_s32(nir4.val[3], nnir4.val[3]);
         vst4q_s32(d, nir4);

         s += 4*4;
         d += 4*4;
         tv = vdupq_n_f32(f);
      }
      while(--i);
   }

   i = num - size*step;
   if (i) {
      fstep /= 8;
      do {
         *d++ += *s++ * f;
         f += fstep;
      } while(--i);
   }
}

void
_batch_fmadd_neon(float32_ptr dst, const_float32_ptr src, size_t num, float v, float vstep)
{
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step;

   step = sizeof(float32x4x4_t)/sizeof(float);

   vstep *= step;
   i = num/step;
   num -= i*step;
   if (i)
   {
      float32x4x4_t sfr4, dfr4;
      float32x4_t tv;

      tv = vdupq_n_f32(v);
      do
      {
         sfr4 = vld4q_f32(s);   // load s
         dfr4 = vld4q_f32(d);   // load d
         s += step;

         dfr4.val[0] = vmlaq_f32(dfr4.val[0], sfr4.val[0], tv);
         dfr4.val[1] = vmlaq_f32(dfr4.val[1], sfr4.val[1], tv);
         dfr4.val[2] = vmlaq_f32(dfr4.val[2], sfr4.val[2], tv);
         dfr4.val[3] = vmlaq_f32(dfr4.val[3], sfr4.val[3], tv);

         v += vstep;

         vst4q_f32(d, dfr4);    // store d
         d += step;

         tv = vdupq_n_f32(v);
      }
      while(--i);
   }

   if (num) {
      vstep /= step;
      i = num;
      do {
         *d++ += *s++ * v;
         v += vstep;
      } while(--i);
   }

}

void
_batch_cvt24_16_neon(void_ptr dst, const_void_ptr src, size_t num)
{
// int32x4_t  vshlq_n_s32(int32x4_t a, __constrange(0,31) int b);
// int32x4_t *sptr = (int32x4_t*)src;
   int16_t* s = (int16_t*)src;
   int32_t *d = (int32_t*)dst;
   size_t i, step;

   if (!num) return;

   step = sizeof(int32x4x2_t)/sizeof(int32_t);
   
   i = num/step;
   num -= i*step;
   if (i)
   {
      int32x4_t nfr0, nfr1, nfr2, nfr3;
      int32x4x2_t nfr2x0, nfr2x1;
      int16x4x4_t nfr2x4;

      do
      {
         nfr2x4 = vld4_s16(s);

         /* widen 16-bit to 32-bit */
         nfr0 = vmovl_s16(nfr2x4.val[0]);
         nfr1 = vmovl_s16(nfr2x4.val[1]);
         nfr2 = vmovl_s16(nfr2x4.val[2]);
         nfr3 = vmovl_s16(nfr2x4.val[3]);

         s += step;

         /* shift from 16-bit to 24-bit */
         nfr2x0.val[0] = vshlq_n_s32(nfr0, 8);
         nfr2x0.val[1] = vshlq_n_s32(nfr1, 8);
         nfr2x1.val[0] = vshlq_n_s32(nfr2, 8);
         nfr2x1.val[1] = vshlq_n_s32(nfr3, 8);

         vst2q_s32(d, nfr2x0);
         vst2q_s32(d+step/2, nfr2x1);
         d += step;

      } while (--i);
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
_batch_cvt16_24_neon(void_ptr dst, const_void_ptr sptr, size_t num)
{
   // int32x4_t  vshrq_n_s32(int32x4_t a, __constrange(1,32) int b);
   int32_t *s = (int32_t*)sptr;
   int16_t* d = (int16_t*)dst;
   size_t i, step;

   if (!num) return;

   step = sizeof(int32x4x2_t)/sizeof(int32_t);

   i = num/step;
   num -= i*step;
   if (i)
   {
      int32x4_t nfr0, nfr1, nfr2, nfr3;
      int32x4x2_t nfr2x0, nfr2x1;
      int16x4x4_t nfr2x4;

      do
      {
         nfr2x0 = vld2q_s32(s);
         nfr2x1 = vld2q_s32(s+step/2);

         /* shift from 24-bit to 16-bit */
         nfr0 = vshrq_n_s32(nfr2x0.val[0], 8);
         nfr1 = vshrq_n_s32(nfr2x0.val[1], 8);
         nfr2 = vshrq_n_s32(nfr2x1.val[0], 8);
         nfr3 = vshrq_n_s32(nfr2x1.val[1], 8);

         s += step;

         /* extract lower part */
         nfr2x4.val[0] = vmovn_s32(nfr0);
         nfr2x4.val[1] = vmovn_s32(nfr1);
         nfr2x4.val[2] = vmovn_s32(nfr2);
         nfr2x4.val[3] = vmovn_s32(nfr3);

         vst4_s16(d, nfr2x4);
         d += step;

      } while (--i);
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
_batch_cvt16_intl_24_neon(void_ptr dst, const_int32_ptrptr src,
                                size_t offset, unsigned int tracks,
                                size_t num)
{
   int16_t* d = (int16_t*)dst;
   size_t i, step;
   int32_t *s1, *s2;

   if (!num) return;

   s1 = (int32_t *)src[0] + offset;
   s2 = (int32_t *)src[1] + offset;

   step = sizeof(int32x4x2_t)/sizeof(int32_t);

   i = num/step;
   num -= i*step;
   if (i)
   {
      int32x4x2_t nfr2x0, nfr2x1, nfr2x2, nfr2x3;
      int32x4_t nfr0, nfr1, nfr2, nfr3;
      int16x4x4_t nfr2x4;

      do
      {
         nfr2x0 = vld2q_s32(s1);
         nfr2x1 = vld2q_s32(s2);

         /* interleave s1 and s2 */
         nfr2x2 = vzipq_s32(nfr2x0.val[0], nfr2x1.val[0]);
         nfr2x3 = vzipq_s32(nfr2x0.val[1], nfr2x1.val[1]);

         s1 += step;

         /* shift from 24-bit to 16-bit */
         nfr0 = vshrq_n_s32(nfr2x2.val[0], 8);
         nfr1 = vshrq_n_s32(nfr2x2.val[1], 8);
         nfr2 = vshrq_n_s32(nfr2x3.val[0], 8);
         nfr3 = vshrq_n_s32(nfr2x3.val[1], 8);

         s2 += step;

         /* extract lower part */
         nfr2x4.val[0] = vmovn_s32(nfr0);
         nfr2x4.val[1] = vmovn_s32(nfr1);
         nfr2x4.val[2] = vmovn_s32(nfr2);
         nfr2x4.val[3] = vmovn_s32(nfr3);

         vst4_s16(d, nfr2x4);
         d += step;

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
_batch_freqfilter_neon(int32_ptr d, const_int32_ptr src, size_t num,
                  float *hist, float k, const float *cptr)
{
   int32_t *s = (int32_t *)src;

   if (num)
   {
      float32x4_t c, h;
      float32x2_t h2;
      size_t i = num;

      c = vld1q_f32(cptr);

      h2 = vld1_f32(hist);
      h = vcombine_f32(h2, h2);

      do
      {
         float32x4_t pz;
         float32x2_t half;
         float smp, tmp;

         smp = *s++ * k;

         pz = vmulq_f32(c, h); // poles and zeros
         half = vget_low_f32(pz);
         tmp = vget_lane_f32(half, 0) + vget_lane_f32(half, 1);

         hist[1] = hist[0];
         hist[0] = smp + tmp;
         h2 = vld1_f32(hist);
         h = vcombine_f32(h2, h2);

         half = vget_high_f32(pz);
         tmp = vget_lane_f32(half, 0) + vget_lane_f32(half, 1);
         *d++ = smp + tmp;
      }
      while (--i);
   }
}

void
_batch_freqfilter_float_neon(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float k, const float *cptr)
{
   float32_ptr s = (float32_ptr)sptr;

   if (num)
   {
      float32x4_t c, h;
      float32x2_t h2;
      size_t i = num;

      c = vld1q_f32(cptr);

      h2 = vld1_f32(hist);
      h = vcombine_f32(h2, h2);

      do
      {
         float32x4_t pz;
         float32x2_t half;
         float smp, tmp;

         smp = *s++ * k;

         pz = vmulq_f32(c, h); // poles and zeros
         half = vget_low_f32(pz);
         tmp = vget_lane_f32(half, 0) + vget_lane_f32(half, 1);

         hist[1] = hist[0];
         hist[0] = smp + tmp;
         h2 = vld1_f32(hist);
         h = vcombine_f32(h2, h2);

         half = vget_high_f32(pz);
         tmp = vget_lane_f32(half, 0) + vget_lane_f32(half, 1);
         *d++ = smp + tmp;
      }
      while (--i);

      vst1_f32(hist, h.val);
   }
}

void
_batch_fmul_value_neon(void* data, unsigned bps, size_t num, float f)
{
   size_t i = num;

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
_aaxBufResampleSkip_neon(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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

static inline void
_aaxBufResampleNearest_neon(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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

static inline void
_aaxBufResampleLinear_neon(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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

static inline void
_aaxBufResampleCubic_neon(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
            a0 = -a0 + y3;                      /* a0 = y3 - y2 - y0 + y1; */
            a1 = y0 - y1 - a0;
            a2 = y2 - y0;
         }
      }
      while (--i);
   }
}

void
_batch_resample_neon(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_neon(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_neon(d, s, dmin, dmax, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleSkip_neon(d, s, dmin, dmax, smu, fact);
   } else {
      _aaxBufResampleNearest_neon(d, s, dmin, dmax, smu, fact);
   }
}
#else

static inline void
_aaxBufResampleSkip_float_neon(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
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

static inline void
_aaxBufResampleNearest_float_neon(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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

static inline void
_aaxBufResampleLinear_float_neon(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_aaxBufResampleCubic_float_neon(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_batch_resample_float_neon(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_float_neon(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_float_neon(d, s, dmin, dmax, smu, fact);
   }
   else if (fact > 1.0f) {
      _aaxBufResampleSkip_float_neon(d, s, dmin, dmax, smu, fact);
   } else {
      _aaxBufResampleNearest_float_neon(d, s, dmin, dmax, smu, fact);
   }
}
#endif // RB_FLOAT_DATA

#endif /* NEON */

