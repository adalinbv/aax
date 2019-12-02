/*
 * Copyright 2005-2019 by Erik Hofman.
 * Copyright 2009-2019 by Adalin B.V.
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
#include "arch2d_simd.h"

#ifdef __ARM_NEON__

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
 *
 * Note: In NEON, the SIMD supports up to 16 operations at the same time.
 *
 * SSE intrinsics to their ARM NEON equivalent:
 * http://codesuppository.blogspot.com/2015/02/
 */

inline float    // range -1.0f .. 1.0f
fast_sin_neon(float x)
{
   return -4.0f*(x - x*fabsf(x));
}

static inline float32x4_t
vandq_f32(float32x4_t a, float32x4_t b)
{
   return (float32x4_t)vandq_s32((int32x4_t)a, (int32x4_t)b);
}

static inline float
hsum_float32x4_neon(float32x4_t v)
{
   float32x2_t r = vadd_f32(vget_high_f32(v), vget_low_f32(v));
   return vget_lane_f32(vpadd_f32(r, r), 0);
}

static inline float32x4_t
vdivq_f32(float32x4_t a, float32x4_t b)
{
   float32x4_t reciprocal = vrecpeq_f32(b);

   // use Newton-Raphson steps to refine the estimate.  Depending on your
   // application's accuracy requirements, you may be able to get away with only
   // one refinement (instead of the two used here).  Be sure to test!
   reciprocal = vmulq_f32(vrecpsq_f32(b, reciprocal), reciprocal);
   reciprocal = vmulq_f32(vrecpsq_f32(b, reciprocal), reciprocal);

   // and finally, compute a/b = a*(1/b)
   return vmulq_f32(a,reciprocal);
}

static inline int
vtestzq_f32(float32x4_t x)
{
   uint32x4_t v = (uint32x4_t)x;
   uint32x2_t tmp = vorr_u32(vget_low_u32(v), vget_high_u32(v));
   return vget_lane_u32(vpmax_u32(tmp, tmp), 0);
}

static inline float32x4_t    // range -1.0f .. 1.0f
fast_sin4_neon(float32x4_t x)
{
   float32x4_t four = vdupq_n_f32(-4.0f);
   return vmulq_f32(four, vsubq_f32(x, vmulq_f32(x, vabsq_f32(x))));
}

float *
_aax_generate_waveform_neon(float32_ptr rv, size_t no_samples, float freq, float phase, enum wave_types wtype)
{
   const_float32_ptr harmonics = _harmonics[wtype];
   if (wtype == _SINE_WAVE) {
      rv = _aax_generate_waveform_cpu(rv, no_samples, freq, phase, wtype);
   }
   else if (rv)
   {
      static const float fact[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
      float32x4_t phase4, freq4, h4;
      float32x4_t one, two, four;
      float32x4_t ngain, nfreq;
      float32x4_t hdt, s;
      unsigned int i, h;
      float *ptr;

      assert(MAX_HARMONICS % 4 == 0);

      one = vdupq_n_f32(1.0f);
      two = vdupq_n_f32(2.0f);
      four = vdupq_n_f32(4.0f);

      phase4 = vdupq_n_f32(-1.0f + phase/GMATH_PI);
      freq4 = vdupq_n_f32(freq);
      h4 = vld1q_f32(fact);

      nfreq = vdivq_f32(freq4, h4);
      ngain = vandq_f32((float32x4_t)vcltq_f32(two, nfreq), vld1q_f32(harmonics));
      hdt = vdivq_f32(two, nfreq);

      ptr = rv;
      i = no_samples;
      s = phase4;
      do
      {
         float32x4_t rv = fast_sin4_neon(s);

         *ptr++ = hsum_float32x4_neon(vmulq_f32(ngain, rv));

         s = vaddq_f32(s, hdt);
         s = vsubq_f32(s, vandq_f32(two, (float32x4_t)vcgeq_f32(s, one)));
      }
      while (--i);

      h4 = vaddq_f32(h4, four);
      for(h=4; h<MAX_HARMONICS; h += 4)
      {
         nfreq = vdivq_f32(freq4, h4);
         ngain = vandq_f32((float32x4_t)vcltq_f32(two, nfreq), vld1q_f32(harmonics+h));
         if (vtestzq_f32(ngain))
         {
            hdt = vdivq_f32(two, nfreq);

            ptr = rv;
            i = no_samples;
            s = phase4;
            do
            {
               float32x4_t rv = fast_sin4_neon(s);

               *ptr++ += hsum_float32x4_neon(vmulq_f32(ngain, rv));

               s = vaddq_f32(s, hdt);
               s = vsubq_f32(s, vandq_f32(two, (float32x4_t)vcgeq_f32(s, one)));
            }
            while (--i);
         }
         h4 = vaddq_f32(h4, four);
      }
   }
   return rv;
}

void
_batch_get_average_rms_neon(const_float32_ptr s, size_t num, float *rms, float *peak)
{
   double rms_total = 0.0;
   float peak_cur = 0.0f;
   size_t i, step, total;

   *rms = *peak = 0;

   if (!num) return;

   total = num;
   step = 2*sizeof(float32x4_t)/sizeof(float);

   i = num/step;
   if (i)
   {
      union {
         float32x4_t ps;
         float f[4];
      } rms1, rms2, peak1, peak2;

      num -= i*step;
      do
      {
         float32x4_t smp1 = vld1q_f32(s);
         float32x4_t smp2 = vld1q_f32(s+4);
         float32x4_t val1, val2;

         val1 = vmulq_f32(smp1, smp1);
         val2 = vmulq_f32(smp2, smp2);

         rms1.ps = vaddq_f32(rms1.ps, val1);
         rms2.ps = vaddq_f32(rms2.ps, val2);

         peak1.ps = vmaxq_f32(peak1.ps, val1);
         peak2.ps = vmaxq_f32(peak2.ps, val2);
      }
      while(--i);

      rms_total += rms1.f[0] + rms1.f[1] + rms1.f[2] + rms1.f[3];
      rms_total += rms2.f[0] + rms2.f[1] + rms2.f[2] + rms2.f[3];

      peak1.ps = vmaxq_f32(peak1.ps, peak2.ps);
      if (peak1.f[0] > peak_cur) peak_cur = peak1.f[0];
      if (peak1.f[1] > peak_cur) peak_cur = peak1.f[1];
      if (peak1.f[2] > peak_cur) peak_cur = peak1.f[2];
      if (peak1.f[3] > peak_cur) peak_cur = peak1.f[3];
   }

   if (num)
   {
      i = num;
      do
      {
         float samp = *s++;            // rms
         float val = samp*samp;
         rms_total += val;
         if (val > peak_cur) peak_cur = val;
      }
      while (--i);
   }

   *rms = (float)sqrt(rms_total/total);
   *peak = sqrtf(peak_cur);
}

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
_batch_roundps_neon(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   float *d = (float*)dptr;
   float *s = (float*)sptr;
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

      do
      {
         nir4 = vld4q_f32(s);
         s += 4*4;

         nir4.val[0] = vrndqn_f32(nir4.val[0]);
         nir4.val[1] = vrndqn_f32(nir4.val[1]);
         nir4.val[2] = vrndqn_f32(nir4.val[2]);
         nir4.val[3] = vrndqn_f32(nir4.val[3]);

         vst4q_f32(d, nir4);
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

static void
_batch_iadd_neon(int32_ptr d, const_int32_ptr src, size_t num)
{
   int32_t *s = (int32_t *)src;
   size_t i, size, step;

   step = sizeof(int32x4x4_t)/sizeof(int32_t);

   i = size = num/step;
   if (i)
   {
      do
      {
         int32x4x4_t nir4d, nir4s;

         nir4s = vld4q_s32(s);
         nir4d = vld4q_s32(d);
         s += 4*4;

         nir4d.val[0] = vaddq_s32(nir4d.val[0], nir4s.val[0]);
         nir4d.val[1] = vaddq_s32(nir4d.val[1], nir4s.val[1]);
         nir4d.val[2] = vaddq_s32(nir4d.val[2], nir4s.val[2]);
         nir4d.val[3] = vaddq_s32(nir4d.val[3], nir4s.val[3]);

         vst4q_s32(d, nir4d);
         d += 4*4;
      }
      while(--i);
   }

   i = num - size*step;
   if (i) {
      do {
         *d++ += *s++;
      } while(--i);
   }
}

void
_batch_imadd_neon(int32_ptr dst, const_int32_ptr src, size_t num, float v, float vstep)
{
   int32_ptr d = (int32_ptr)dst;
   int32_ptr s = (int32_t *)src;
   size_t i, size, step;

   if (!num || (v <= LEVEL_128DB && vstep <= LEVEL_128DB)) return;
   if (fabsf(v - 1.0f) < LEVEL_96DB && vstep <=  LEVEL_96DB) {
      _batch_iadd_neon(dst, src, num);
      return;
   }

   step = sizeof(int32x4x4_t)/sizeof(int32_t);

   vstep *= step;                                 /* 16 samples at a time */
   i = size = num/step;
   if (i)
   {
      float32x4_t tv = vdupq_n_f32(v);
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

         s += step;

         nfr[0] = vmulq_f32(nfr[0], tv);
         nfr[1] = vmulq_f32(nfr[1], tv);
         nfr[2] = vmulq_f32(nfr[2], tv);
         nfr[3] = vmulq_f32(nfr[3], tv);

         v += vstep;

         nir4 = vld4q_s32(d);
         nnir4.val[0] = vcvtq_s32_f32(nfr[0]);
         nnir4.val[1] = vcvtq_s32_f32(nfr[1]);
         nnir4.val[2] = vcvtq_s32_f32(nfr[2]);
         nnir4.val[3] = vcvtq_s32_f32(nfr[3]);

         d += step;

         nir4.val[0] = vaddq_s32(nir4.val[0], nnir4.val[0]);
         nir4.val[1] = vaddq_s32(nir4.val[1], nnir4.val[1]);
         nir4.val[2] = vaddq_s32(nir4.val[2], nnir4.val[2]);
         nir4.val[3] = vaddq_s32(nir4.val[3], nnir4.val[3]);
         vst4q_s32(d, nir4);

         tv = vdupq_n_f32(v);
      }
      while(--i);
   }

   i = num - size*step;
   if (i) {
      vstep /= 16;
      do {
         *d++ += *s++ * v;
         v += vstep;
      } while(--i);
   }
}

static void
_batch_fadd_neon(float32_ptr dst, const_float32_ptr src, size_t num)
{
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step;

   step = sizeof(float32x4_t)/sizeof(float);

   i = num/step;
   num -= i*step;
   if (i)
   {
      float32x4_t sfr4, dfr4;

      do
      {
         sfr4 = vld1q_f32(s);   // load s
         dfr4 = vld1q_f32(d);   // load d
         s += step;

         dfr4 = vaddq_f32(dfr4, sfr4);

         vst1q_f32(d, dfr4);    // store d
         d += step;
      }
      while(--i);
   }

   if (num) {
      i = num;
      do {
         *d++ += *s++;
      } while(--i);
   }
}

void
_batch_fmadd_neon(float32_ptr dst, const_float32_ptr src, size_t num, float v, float vstep)
{
   int need_step = (fabsf(vstep) <=  LEVEL_96DB) ? 0 : 1;
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step;

   if (!num || (fabsf(v) <= LEVEL_128DB && !need_step)) return;

   if (fabsf(v - 1.0f) < LEVEL_96DB && !need_step) {
      _batch_fadd_neon(dst, src, num);
      return;
   }

   step = sizeof(float32x4_t)/sizeof(float);

   i = num/step;
   num -= i*step;

   if (i)
   {
      if (need_step)
      {
         static const float fact[4] = { 0.0f, 1.0f, 2.0f, 3.0f };
         float32x4_t sfr4, dfr4;
         float32x4_t dv, tv, dvstep;

         dvstep = vld1q_f32(fact);
         dvstep = vmulq_f32(dvstep, vdupq_n_f32(vstep));

         dv = vdupq_n_f32(vstep*step);
         tv = vaddq_f32(vdupq_n_f32(v), dvstep);
         v += i*step*vstep;

         do
         {
            sfr4 = vld1q_f32(s);   // load s
            dfr4 = vld1q_f32(d);   // load d

            s += step;
            dfr4 = vmlaq_f32(dfr4, sfr4, tv);

            tv = vaddq_f32(tv, dv);

            vst1q_f32(d, dfr4);    // store d
            d += step;
         }
         while(--i);

         if (num) {
            i = num;
            do {
               *d++ += *s++ * v;
               v += vstep;
            } while(--i);
         }
      }
      else
      {
         float32x4_t tv = vdupq_n_f32(v);
         float32x4_t sfr4, dfr4;

         do
         {
            sfr4 = vld1q_f32(s);   // load s
            dfr4 = vld1q_f32(d);   // load d

            s += step;
            dfr4 = vmlaq_f32(dfr4, sfr4, tv);

            vst1q_f32(d, dfr4);    // store d
            d += step;
         }
         while(--i);

         if (num) {
            i = num;
            do {
               *d++ += *s++ * v;
            } while(--i);
         }
      }
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

   _batch_dither_cpu(s, 2, num);

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
   size_t i, t, step;
   int32_t *s1, *s2;

   if (!num) return;

   for (t=0; t<tracks; ++t)
   {
      int32_t *s = (int32_t *)src[t] + offset;
      _batch_dither_cpu(s, 2, num);
   }

   if (tracks != 2)
   {
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
_batch_fmul_value_neon(void* dptr, const void* sptr, unsigned bps, size_t num, float f)
{
   if (!num || fabsf(f - 1.0f) < LEVEL_96DB) return;

   if (f <= LEVEL_128DB) {
      memset(dptr, 0, num*bps);
   }
   else if (num)
   {
      size_t i = num;

      switch (bps)
      {
      case 4:
      {
         float32_ptr s = (float32_ptr)sptr;
         float32_ptr d = (float32_ptr)dptr;
         size_t i, step;

         step = sizeof(float32x4_t)/sizeof(float);

         i = num/step;
         num -= i*step;
         if (i)
         {
            float32x4_t sfact = vdupq_n_f32(f);
            float32x4_t sfr4;

            do
            {
               sfr4 = vld1q_f32(s);   // load s
               s += step;

               sfr4 = vmulq_f32(sfr4, sfact);

               vst1q_f32(d, sfr4);    // store d
               d += step;
            }
            while(--i);
         }

         if (num) {
            i = num;
            do {
               *d++ *= *s++;
            } while(--i);
         }
         break;
      }
      case 8:
      {
         double *s = (double*)sptr;
         double *d = (double*)dptr;
         do {
            *d++ = *s++ * f;
         }
         while (--i);
         break;
      }
      default:
         break;
      }
   }
}

#endif /* NEON */

