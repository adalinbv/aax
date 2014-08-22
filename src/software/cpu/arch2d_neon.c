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

#ifdef __ARM_NEON__
#include <arm_neon.h>
#include "arch2d_simd.h"

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
      float32x4_t tv = vdupq_n_f32(v);	// set (v,v,v,v)
      float32x4x4_t sfr4, dfr4;

      do
      {
         sfr4 = vld4q_f32(s);	// load s
         dfr4 = vld4q_f32(d);	// load d

         s += step;

         sfr4.val[0] = vmulq_f32(sfr4.val[0], tv);	// multiply
         sfr4.val[1] = vmulq_f32(sfr4.val[1], tv);
         sfr4.val[2] = vmulq_f32(sfr4.val[2], tv);
         sfr4.val[3] = vmulq_f32(sfr4.val[3], tv);

         d += step;

         dfr4.val[0] = vaddq_f32(dfr4.val[0], sfr4.val[0]);	// add
         dfr4.val[1] = vaddq_f32(dfr4.val[1], sfr4.val[1]);
         dfr4.val[2] = vaddq_f32(dfr4.val[2], sfr4.val[2]);
         dfr4.val[3] = vaddq_f32(dfr4.val[3], sfr4.val[3]);

         v += vstep;

         vst4q_f32(d, dfr4);	// store d
         tv = vdupq_n_f32(v);	// set (v,v,v,v)
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

#define CALCULATE_NEW_SAMPLE(i, smp) \
        dhist = vsetq_lane_f32(h0, dhist, 0); \
        dhist = vsetq_lane_f32(h1, dhist, 1); \
        dhist = vsetq_lane_f32(h0, dhist, 2); \
        dhist = vsetq_lane_f32(h1, dhist, 3); \
        h1 = h0;                              \
        nfr##i = vmulq_f32(dhist, coeff);     \
        vst1q_f32(mpf, nfr##i);               \
        smp -=  mpf[0]; h0 = smp - mpf[1];    \
        smp = h0 + mpf[2]; smp += mpf[3]
void
_batch_freqfilter_neon(int32_ptr d, const_int32_ptr src, size_t num,
                  float *hist, float lfgain, float hfgain, float k,
                  const float *cptr)
{
   int32_t *s = (int32_t *)src;
   size_t i, step;
   float h0, h1;

   if (!num) return;

   h0 = hist[0];
   h1 = hist[1];

   step = sizeof(int32x4x2_t)/sizeof(int32_t);
   i = num/step;
   num -= i*step;
   if (i)
   {
      float32x4_t nfr0, nfr1, nfr2, nfr3, nfr4, nfr5, nfr6, nfr7;
      float32x4_t osmp0, osmp1, fact, dhist, coeff, lf, hf, tmp2;
      float32_t *cfp = (float32_t *)cptr;
      float32_t *smp0, *mpf;
      float32x4x2_t tmp0;

      /* 16-byte aligned */
      smp0 = (float32_t *)&tmp0;
      mpf = (float32_t *)&tmp2;

      fact = vdupq_n_f32(k);
      coeff = vrev64q_f32( vld1q_dup_f32(cfp) );
      lf = vdupq_n_f32(lfgain);
      hf = vdupq_n_f32(hfgain);

      do
      {
         int32x4x2_t nir2_1;
         float32x4x2_t nfr2d;

         nir2_1 = vld2q_s32(s);

         osmp0 = vcvtq_f32_s32(nir2_1.val[0]);
         osmp1 = vcvtq_f32_s32(nir2_1.val[1]);
         nfr2d.val[0] = vmulq_f32(osmp0, fact);	/* *s * k */
         nfr2d.val[1] = vmulq_f32(osmp1, fact);
         vst2q_f32(smp0, nfr2d);

         s += step;

         CALCULATE_NEW_SAMPLE(0, smp0[0]);
         CALCULATE_NEW_SAMPLE(1, smp0[1]);
         CALCULATE_NEW_SAMPLE(2, smp0[2]);
         CALCULATE_NEW_SAMPLE(3, smp0[3]);
         CALCULATE_NEW_SAMPLE(4, smp0[4]);
         CALCULATE_NEW_SAMPLE(5, smp0[5]);
         CALCULATE_NEW_SAMPLE(6, smp0[6]);
         CALCULATE_NEW_SAMPLE(7, smp0[7]);

         nfr2d = vld2q_f32(smp0);		/* smp */
         nfr1 = vmulq_f32(nfr2d.val[0], lf);	/* smp * lfgain */
         nfr5 = vmulq_f32(nfr2d.val[1], lf);
         nfr3 = vsubq_f32(osmp0, nfr2d.val[0]);	/* *s - smp */
         nfr7 = vsubq_f32(osmp1, nfr2d.val[1]);
         nfr2 = vmlaq_f32(nfr1, nfr3, hf);    /* smp*lfgain + (*s-smp)*hfgain */
         nfr6 = vmlaq_f32(nfr5, nfr7, hf);
         nir2_1.val[0] = vcvtq_s32_f32(nfr2);
         nir2_1.val[1] = vcvtq_s32_f32(nfr6);

         vst2q_s32(d, nir2_1);
         d += 8;
      }
      while (--i);
   }

   if (num)
   {
      float smp, nsmp;
      i = num;
      do
      {
         smp = *s * k;
         smp = smp - h0 * cptr[0];
         nsmp = smp - h1 * cptr[1];
         smp = nsmp + h0 * cptr[2];
         smp = smp + h1 * cptr[3];

         h1 = h0;
         h0 = nsmp;

         *d++ = smp*lfgain + (*s-smp)*hfgain;
         s++;
      }
      while (--i);
   }

   hist[0] = h0;
   hist[1] = h1;
}

void
_batch_freqfilter_float_neon(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float lfgain, float hfgain, float k, const float *cptr)
{
   float32_ptr s = (float32_ptr)sptr;
   size_t i, size, step;
   float h0, h1;

   if (!num) return;

   h0 = hist[0];
   h1 = hist[1];

   step = sizeof(float32x4x2_t)/sizeof(float32_t);
   i = num/step;
   num -= i*step;
   if (i)
   {
      float32x4_t nfr0, nfr1, nfr2, nfr3, nfr4, nfr5, nfr6, nfr7;
      float32x4_t fact, dhist, coeff, lf, hf;
      float32_t *cfp = (float32_t *)cptr;
      float32x4_t tmp0, tmp1, tmp2;
      float32_t *smp0, *smp1, *mpf;

      smp0 = (float *)&tmp0;
      smp1 = (float *)&tmp1;
      mpf = (float *)&tmp2;

      fact = vdupq_n_f32(k);
      coeff = vrev64q_f32( vld1q_dup_f32(cfp) );
      lf = vdupq_n_f32(lfgain);
      hf = vdupq_n_f32(hfgain);

      do
      {
         float32x4x2_t nfr2_1;
         float32x4x2_t nfr2d;

         nfr2_1 = vld2q_f32(s);

         nfr2d.val[0] = vmulq_f32(nfr2_1.val[0], fact); /* *s * k */
         nfr2d.val[1] = vmulq_f32(nfr2_1.val[1], fact);
         vst2q_f32(smp0, nfr2d);
         s += step;

         CALCULATE_NEW_SAMPLE(0, smp0[0]);
         CALCULATE_NEW_SAMPLE(1, smp0[1]);
         CALCULATE_NEW_SAMPLE(2, smp0[2]);
         CALCULATE_NEW_SAMPLE(3, smp0[3]);
         CALCULATE_NEW_SAMPLE(4, smp0[4]);
         CALCULATE_NEW_SAMPLE(5, smp0[5]);
         CALCULATE_NEW_SAMPLE(6, smp0[6]);
         CALCULATE_NEW_SAMPLE(7, smp0[7]);

         nfr2d = vld2q_f32(smp0);               /* smp */
         nfr1 = vmulq_f32(nfr2d.val[0], lf);    /* smp * lfgain */
         nfr5 = vmulq_f32(nfr2d.val[1], lf);
         nfr3 = vsubq_f32(nfr2_1.val[0], nfr2d.val[0]); /* *s - smp */
         nfr7 = vsubq_f32(nfr2_1.val[1], nfr2d.val[1]);
         nfr2 = vmlaq_f32(nfr1, nfr3, hf);    /* smp*lfgain + (*s-smp)*hfgain */
         nfr6 = vmlaq_f32(nfr5, nfr7, hf);
         nfr2_1.val[0] = nfr2;
         nfr2_1.val[1] = nfr6;

         vst2q_f32(d, nfr2_1);
         d += step;
      }
      while (--i);
   }

   if (num)
   {
      float smp, nsmp;
      i = num;
      do
      {
         smp = *s * k;
         smp = smp - h0 * cptr[0];
         nsmp = smp - h1 * cptr[1];
         smp = nsmp + h0 * cptr[2];
         smp = smp + h1 * cptr[3];

         h1 = h0;
         h0 = nsmp;

         *d++ = smp*lfgain + (*s-smp)*hfgain;
         s++;
      }
      while (--i);
   }

   hist[0] = h0;
   hist[1] = h1;
}

static inline void
_aaxBufResampleSkip_neon(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   const int32_t *sptr = s;
   int32_t *dptr = d;
   int32_t samp, nsamp;
   size_t i;
   float pos;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor >= 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   pos = smu + 1;
   sptr = s + (size_t)pos;
   dptr += dmin;
   samp = *sptr;

   pos += freq_factor;
   sptr = s + (size_t)pos;
   nsamp = *sptr;

   i=dmax-dmin;
   if (i)
   {
      do
      {
         *dptr++ = (samp + nsamp) / 2;

         samp = nsamp;
         pos += freq_factor;
         sptr = s + (size_t)pos;
         nsamp = *sptr;
      }
      while (--i);
   }
}

static void
_aaxBufResampleNearest_neon(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   const int32_t *sptr = s;
   int32_t *dptr = d;
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
         if (smu > 0.5)
         {
            sptr++;
            smu -= 1.0;
         }
      }
      while (--i);
   }
}

static void
_aaxBufResampleLinear_neon(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   const int32_t *sptr = s;
   int32_t *dptr = d;
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

static void
_aaxBufResampleCubic_neon(int32_ptr d, const_int32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   float y0, y1, y2, y3, a0, a1, a2;
   const int32_t *sptr = s;
   int32_t *dptr = d;
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
         *dptr++ = (int32_t)ftmp;

         smu += freq_factor;
         if (smu >= 1.0)      {
            smu--;
            a0 += y0;
            y0 = y1;
            y1 = y2;
            y2 = y3;
            y3 = *sptr++;
            a0 = -a0 + y3;                 /* a0 = y3 - y2 - y0 + y1; */
            a1 = y0 - y1 - a0;
            a2 = y2 - y0;
         }
      }
      while (--i);
   }
}
#endif /* NEON */

