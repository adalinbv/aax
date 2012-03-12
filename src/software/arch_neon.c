/*
 * Copyright 2005-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
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

#ifdef HAVE_ARM_NEON_H
#include <arm_neon.h>

#pragma GCC target ("fpu=neon","float-abi=softfp")

#include "arch_simd.h"

void
_vec4Copy_neon(vec4 d, const vec4 v)
{
   const float32_t *src = (const float32_t *)v;
   float32_t *dst = (float32_t *)d;
   float32x4_t nfr1;

   nfr1 = vld1q_f32(src);
   vst1q_f32(dst, nfr1);
}

void
_vec4Add_neon(vec4 d, const vec4 v)
{
   const float32_t *src = (const float32_t *)v;
   float32_t *dst = (float32_t *)d;
   float32x4_t nfr1, nfr2;

   nfr1 = vld1q_f32(dst);
   nfr2 = vld1q_f32(src);
   nfr1 = vaddq_f32(nfr1, nfr2);
   vst1q_f32(dst, nfr1);
}

void
_vec4Sub_neon(vec4 d, const vec4 v)
{
   const float32_t *src = (const float32_t *)v;
   float32_t *dst = (float32_t *)d;
   float32x4_t nfr1, nfr2;

   nfr1 = vld1q_f32(dst);
   nfr2 = vld1q_f32(src);
   nfr1 = vsubq_f32(nfr1, nfr2);
   vst1q_f32(dst, nfr1);
}

void
_vec4Devide_neon(vec4 v, float s)
{
   if (s)
   {
      float32_t *dst = (float32_t *)v;
      float32x4_t den = vdupq_n_f32(s);
      float32x4_t num = vld1q_f32(dst);
      float32x4_t q_inv0 = vrecpeq_f32(den);
      float32x4_t q_step0 = vrecpsq_f32(q_inv0, den);
      float32x4_t q_inv1 = vmulq_f32(q_step0, q_inv0);
      vst1q_f32(dst, vmulq_f32(num, q_inv1));
   }
}

void
_vec4Mulvec4_neon(vec4 r, const vec4 v1, const vec4 v2)
{
   const float32_t *src1 = (const float32_t *)v1;
   const float32_t *src2 = (const float32_t *)v2;
   float32_t *dst = (float32_t *)r;
   float32x4_t nfr1, nfr2;

   nfr1 = vld1q_f32(src1);
   nfr2 = vld1q_f32(src2);
   nfr1 = vmulq_f32(nfr1, nfr2);
   vst1q_f32(dst, nfr1);
}

void
_ivec4Copy_neon(ivec4 d, ivec4 v)
{
   int32x4_t nfr1 = vld1q_s32(v);
   vst1q_s32(d, nfr1);
}

void
_ivec4Add_neon(ivec4 d, ivec4 v)
{
   int32x4_t nfr1 = vld1q_s32(d);
   int32x4_t nfr2 = vld1q_s32(v);

   nfr1 = vaddq_s32(nfr1, nfr2);
   vst1q_s32(d, nfr1);
}

void
_ivec4Sub_neon(ivec4 d, ivec4 v)
{
   int32x4_t nfr1 = vld1q_s32(d);
   int32x4_t nfr2 = vld1q_s32(v);

   nfr1 = vsubq_s32(nfr1, nfr2);
   vst1q_s32(d, nfr1);
}

void
_ivec4Devide_neon(ivec4 d, float s)
{
   if (s)
   {
      int32x4_t vi = vld1q_s32(d);
      float32x4_t num = vcvtq_f32_s32(vi);
      float32x4_t den = vdupq_n_f32(s);
      float32x4_t q_inv0 = vrecpeq_f32(den);
      float32x4_t q_step0 = vrecpsq_f32(q_inv0, den);
      float32x4_t q_inv1 = vmulq_f32(q_step0, q_inv0);

      vi = vcvtq_s32_f32(vmulq_f32(num, q_inv1));
      vst1q_s32(d, vi);
   }
}

void
_ivec4Mulivec4_neon(ivec4 d, const ivec4 v1, const ivec4 v2)
{
   int32x4_t nfr1 = vld1q_s32(v1);
   int32x4_t nfr2 = vld1q_s32(v2);

   nfr1 = vmulq_s32(nfr1, nfr2);
   vst1q_s32(d, nfr1);
}

void
_vec4Matrix4_neon(vec4 d, const vec4 v, mtx4 m)
{
   float32x4_t a_line, b_line, r_line;
   const float32_t *a = (const float32_t *)m;
   const float32_t *b = (const float32_t *)v;
   float32_t *r = (float32_t *)d;
   int i;

  /*
   * unroll the first step of the loop to avoid having to initialize
   * r_line to zero
   */
   a_line = vld1q_f32(a);             /* a_line = vec4(column(m, 0)) */
   b_line = vdupq_n_f32(b[0]);          /* b_line = vec4(v[0])         */
   r_line = vmulq_f32(a_line, b_line); /* r_line = a_line * b_line    */
   for (i=1; i<4; i++)
   {
      a_line = vld1q_f32(&a[i*4]);    /* a_line = vec4(column(m, i)) */
      b_line = vdupq_n_f32(b[i]);       /* b_line = vec4(v[i])         */
                                        /* r_line += a_line * b_line   */
      r_line = vaddq_f32(vmulq_f32(a_line, b_line), r_line);
   }
   vst1q_f32(r, r_line);             /* r = r_line                  */
}

void
_mtx4Mul_neon(mtx4 d, mtx4 m1, mtx4 m2)
{
   float32x4_t a_line, b_line, r_line;
   const float32_t *a = (const float32_t *)m1;
   const float32_t *b = (const float32_t *)m2;
   float32_t *r = (float32_t *)d;
   int i;

   for (i=0; i<16; i += 4)
   {
     int j;
     /*
      * unroll the first step of the loop to avoid having to initialize
      * r_line to zero
      */
      a_line = vld1q_f32(a);             /* a_line = vec4(column(a, 0)) */
      b_line = vdupq_n_f32(b[i]);          /* b_line = vec4(b[i][0])      */
      r_line = vmulq_f32(a_line, b_line); /* r_line = a_line * b_line    */
      for (j=1; j<4; j++)
      {
         a_line = vld1q_f32(&a[j*4]);    /* a_line = vec4(column(a, j)) */
         b_line = vdupq_n_f32(b[i+j]);     /* b_line = vec4(b[i][j])      */
                                           /* r_line += a_line * b_line   */
         r_line = vaddq_f32(vmulq_f32(a_line, b_line), r_line);
      }
      vst1q_f32(&r[i], r_line);         /* r[i] = r_line               */
   }
}

void
_batch_fmadd_neon(int32_ptr d, const int32_ptr src, unsigned int num, float f, float fstep)
{
#if 0
   unsigned int i = (num/4)*4;
   do
   {
      *d++ += *s++ * f;
      f += fstep;
   }
   while (--i);
#else
   int32_t *s = (int32_t *)src;
   unsigned int i, size, step;
   long dtmp, stmp;

   dtmp = (long)d & 0xF;
   stmp = (long)s & 0xF;
#if 0
   if ((dtmp || stmp) && dtmp != stmp)
   {
      _batch_fmadd_cpu(d, s, num, f, fstep);
      return;
   }
#endif

   step = 2*sizeof(int32x4_t)/sizeof(int32_t);

#if 0
   /* work towards a 16-byte aligned dptr (and hence 16-byte aligned sptr) */
   i = num/step;
   if (dtmp && i)
   {
      i = (0x10 - dtmp)/4; // sizeof(int32_t);
      num -= i;
      do {
         *d++ += *s++ * f;
         f += fstep;
      } while(--i);
   }
#endif

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
#endif
}

void
_batch_cvt24_16_neon(int32_t*__restrict d, const void*__restrict src, unsigned int num)
{
   // int32x4_t  vshlq_n_s32(int32x4_t a, __constrange(0,31) int b);
   int32x4_t *sptr = (int32x4_t*)src;
   int16_t* s = (int16_t*)src;
   unsigned int i, size, step;
   long tmp;

   step = 2*4*sizeof(int32x4_t)/sizeof(int32_t);
   
#if 0
   /*
    * work towards 16-byte aligned sptr
    */
   i = num/step;
   tmp = (long)s & 0xF;
   if (tmp && i)
   {
      i = (0x10 - tmp)/sizeof(int32_t);
      num -= i;
      do {
         *d++ = *s++ >> 8;
      } while(--i);
      sptr = (int32x4_t*)s;
   }
#endif

   i = size = num/step;
   if (i)
   {
      int32x4_t nfr0, nfr1, nfr2, nfr3;
      int32x4x2_t nfr2x0, nfr2x1;
      int16x4x4_t nfr2x4;

      do
      {
//       _mm_prefetch(((char *)s1)+CACHE_ADVANCE_INTL, _MM_HINT_NTA);
//       _mm_prefetch(((char *)s2)+CACHE_ADVANCE_INTL, _MM_HINT_NTA);

         nfr2x4 = vld4_s16(s);

         /* widen 16-bit to 32-bit */
         nfr0 = vmovl_s16(nfr2x4.val[0]);
         nfr1 = vmovl_s16(nfr2x4.val[1]);
         nfr2 = vmovl_s16(nfr2x4.val[2]);
         nfr3 = vmovl_s16(nfr2x4.val[3]);

         /* shift from 16-bit to 24-bit */
         nfr2x0.val[0] = vshlq_n_s32(nfr0, 8);
         nfr2x0.val[1] = vshlq_n_s32(nfr1, 8);
         nfr2x1.val[0] = vshlq_n_s32(nfr2, 8);
         nfr2x1.val[1] = vshlq_n_s32(nfr3, 8);

         s += step;

         vst2q_s32(d, nfr2x0);
         vst2q_s32(d+step/2, nfr2x1);
         d += step;

      } while (--i);
   }

   i = num - size*16;
   if (i)
   {
      do {
         *d++ = *s++ >> 8;
      } while (--i);
   }
}

void
_batch_cvt16_24_neon(void*__restrict dst, const int32_t*__restrict s, unsigned int num)
{
   // int32x4_t  vshrq_n_s32(int32x4_t a, __constrange(1,32) int b);
   int32x4_t *sptr = (int32x4_t*)s;
   int16_t* d = (int16_t*)dst;
   unsigned int i, size, step;
   long tmp;

   step = 2*4*sizeof(int32x4_t)/sizeof(int32_t);

#if 0
   /*
    * work towards 16-byte aligned sptr
    */
   i = num/step;
   tmp = (long)s & 0xF;
   if (tmp && i)
   {
      i = (0x10 - tmp)/sizeof(int32_t);
      num -= i;
      do {
         *d++ = *s++ >> 8;
      } while(--i);
      sptr = (int32x4_t*)s;
   }
#endif

   i = size = num/step;
   if (i)
   {
      int32x4_t nfr0, nfr1, nfr2, nfr3;
      int32x4x2_t nfr2x0, nfr2x1;
      int16x4x4_t nfr2x4;

      do
      {
//       _mm_prefetch(((char *)s1)+CACHE_ADVANCE_INTL, _MM_HINT_NTA);
//       _mm_prefetch(((char *)s2)+CACHE_ADVANCE_INTL, _MM_HINT_NTA);

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

   i = num - size*16;
   if (i)
   {
      s = (int32_t *)sptr;
      do {
         *d++ = *s++ >> 8;
      } while (--i);
   }
}

void
_batch_cvt16_intl_24_neon(void*__restrict dst, const int32_t**__restrict src,
                                unsigned int offset, unsigned int tracks,
                                unsigned int num)
{
   unsigned int i, size, step;
   int16_t* d = (int16_t*)dst;
   int32_t *s1, *s2;
   long tmp;


#if 0
   if (tracks != 2)
   {
      _batch_cvt24_intl_16_cpu(d, src, offset, tracks, num);
      return;
   }
#endif

   s1 = (int32_t *)src[0] + offset;
   s2 = (int32_t *)src[1] + offset;

   step = 2*sizeof(int32x4_t)/sizeof(int32_t);

#if 0
   /*
    * work towards 16-byte aligned sptr
    */
   i = num/step;
   tmp = (long)s1 & 0xF;
   assert(tmp == ((long)s2 & 0xF));
   if (tmp && i)
   {
      i = (0x10 - tmp)/sizeof(int32_t);
      num -= i;
      do
      {
         *d++ = *s1++ >> 8;
         *d++ = *s2++ >> 8;
      }
      while (--i);
   }
#endif

   tmp = (long)d & 0xF;
   i = size = num/step;
   if (i)
   {
      int32x4x2_t nfr2x0, nfr2x1, nfr2x2, nfr2x3;
      int32x4_t nfr0, nfr1, nfr2, nfr3;
      int16x4x4_t nfr2x4;

      do
      {
//       _mm_prefetch(((char *)s1)+CACHE_ADVANCE_INTL, _MM_HINT_NTA);
//       _mm_prefetch(((char *)s2)+CACHE_ADVANCE_INTL, _MM_HINT_NTA);

         nfr2x0 = vld2q_s32(s1);
         nfr2x1 = vld2q_s32(s2);

         /* interleave s1 and s2 */
         nfr2x2 = vzipq_s32(nfr2x0.val[0], nfr2x1.val[0]);
         nfr2x3 = vzipq_s32(nfr2x0.val[1], nfr2x1.val[1]);

         s1 += step;
         s2 += step;

         /* shift from 24-bit to 16-bit */
         nfr0 = vshrq_n_s32(nfr2x2.val[0], 8);
         nfr1 = vshrq_n_s32(nfr2x2.val[1], 8);
         nfr2 = vshrq_n_s32(nfr2x3.val[0], 8);
         nfr3 = vshrq_n_s32(nfr2x3.val[1], 8);

         /* extract lower part */
         nfr2x4.val[0] = vmovn_s32(nfr0);
         nfr2x4.val[1] = vmovn_s32(nfr1);
         nfr2x4.val[2] = vmovn_s32(nfr2);
         nfr2x4.val[3] = vmovn_s32(nfr3);

         vst4_s16(d, nfr2x4);
         d += step;

      } while (--i);
   }

   i = num - size*step;
   if (i)
   {
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
_batch_freqfilter_neon(int32_ptr d, const int32_ptr src, unsigned int num,
                  float *hist, float lfgain, float hfgain, float k,
                  const float *cptr)
{
   int32_t *s = (int32_t *)src;
   unsigned int i, size, step;
   float h0, h1;
   long tmp;

   h0 = hist[0];
   h1 = hist[1];

#if 0
   /* work towards 16-byte aligned dptr */
   tmp = (long)d & 0xF;
   if (tmp)
   {
      float smp, nsmp;

      i = (0x10 - tmp)/sizeof(int32_t);
      num -= i;
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
#endif

   step = 2*sizeof(int32x4_t)/sizeof(int32_t);
   i = size = num/step;
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

//       _mm_prefetch(((char *)s)+CACHE_ADVANCE_FF, _MM_HINT_NTA);

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

   i = num - size*step;
   if (i)
   {
      float smp, nsmp;
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
_aaxBufResampleSkip_neon(int32_ptr d, const int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   const int32_t *sptr = s;
   int32_t *dptr = d;
   int32_t samp, nsamp;
   unsigned int i;
   float pos;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(freq_factor >= 1.0f);
   assert(0.0f <= smu && smu < 1.0f);

   pos = sdesamps + smu + 1;
   sptr = s + (unsigned int)pos;
   dptr += dmin;
   samp = *sptr;

   pos += freq_factor;
   sptr = s + (unsigned int)pos;
   nsamp = *sptr;

   i=dmax-dmin;
   do
   {
      *dptr++ = (samp + nsamp) / 2;

      samp = nsamp;
      pos += freq_factor;
      sptr = s + (unsigned int)pos;
      nsamp = *sptr;
   }
   while (--i);
}

void
_aaxBufResampleNearest_neon(int32_ptr d, const int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   const int32_t *sptr = s;
   int32_t *dptr = d;
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
      if (smu > 0.5)
      {
         sptr++;
         smu -= 1.0;
      }
   }
   while (--i);
}

void
_aaxBufResampleLinear_neon(int32_ptr d, const int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   const int32_t *sptr = s;
   int32_t *dptr = d;
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

void
_aaxBufResampleCubic_neon(int32_ptr d, const int32_ptr s, unsigned int dmin, unsigned int dmax, unsigned int sdesamps, float smu, float freq_factor)
{
   float y0, y1, y2, y3, a0, a1, a2;
   const int32_t *sptr = s;
   int32_t *dptr = d;
   unsigned int i;

   assert(s != 0);
   assert(d != 0);
   assert(dmin < dmax);
   assert(0.0f <= smu && smu < 1.0f);
   assert(0.0f < freq_factor && freq_factor <= 1.0f);

   sptr += sdesamps;
   dptr += dmin;

   y0 = *sptr++;
   y1 = *sptr++;
   y2 = *sptr++;
   y3 = *sptr++;

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
#endif /* NEON */

