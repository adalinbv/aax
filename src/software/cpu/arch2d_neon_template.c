/*
 * SPDX-FileCopyrightText: Copyright © 2005-2024 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2024 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "software/rbuf_int.h"
#include "arch2d_simd.h"

#define __FN(NAME,ARCH) _##NAME##_##ARCH
#define FN(NAME,ARCH)   __FN(NAME,ARCH)

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
FN(fast_sin,A)(float x)
{
   return -4.0f*(x - x*fabsf(x));
}

static inline float32x4_t
FN(vandq_f32,A)(float32x4_t a, float32x4_t b)
{
   return vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
}

static inline float32x4x2_t
FN(vand2q_f32,A)(float32x4x2_t a, float32x4x2_t b)
{
   a.val[0] = FN(vandq_f32,A)(a.val[0], b.val[0]);
   a.val[1] = FN(vandq_f32,A)(a.val[1], b.val[1]);
   return a;
}

static inline float
FN(hsum_float32x4,A)(float32x4_t v)
{
   float32x2_t r = vadd_f32(vget_high_f32(v), vget_low_f32(v));
   return vget_lane_f32(vpadd_f32(r, r), 0);
}

static inline float
FN(hsum_float32x4x2,A)(float32x4x2_t v)
{
   return FN(hsum_float32x4,A)(vaddq_f32(v.val[0], v.val[1]));
}

static inline float32x4_t
FN(vdivq_f32,A)(float32x4_t a, float32x4_t b)
{
   float32x4_t reciprocal = vrecpeq_f32(b);

   // use Newton-Raphson steps to refine the estimate.  Depending on your
   // application's accuracy requirements, you may be able to get away with only
   // one refinement (instead of the two used here).  Be sure to test!
   reciprocal = vmulq_f32(vrecpsq_f32(b, reciprocal), reciprocal);
   reciprocal = vmulq_f32(vrecpsq_f32(b, reciprocal), reciprocal);

   // and finally, compute a/b = a*(1/b)
   return vmulq_f32(a, reciprocal);
}

static inline int
FN(vtestzq_f32,A)(float32x4_t x)
{
   uint32x4_t v = (uint32x4_t)x;
   uint32x2_t tmp = vorr_u32(vget_low_u32(v), vget_high_u32(v));
   return vget_lane_u32(vpmax_u32(tmp, tmp), 0);
}

static inline int
FN(vtestz2q_f32,A)(float32x4x2_t x)
{
   return (FN(vtestzq_f32,A)(x.val[0]) && FN(vtestzq_f32,A)(x.val[1]));
}

static inline float32x4x2_t
FN(vdup2q_n_f32,A)(float a)
{
   float32x4x2_t rv;
   rv.val[0] = rv.val[1] = vdupq_n_f32(a);
   return rv;
}

static inline float32x4x2_t
FN(vadd2q_f32,A)(float32x4x2_t a, float32x4x2_t b)
{
   a.val[0] = vaddq_f32(a.val[0], b.val[0]);
   a.val[1] = vaddq_f32(a.val[1], b.val[1]);
   return a;
}

static inline float32x4x2_t
FN(vsub2q_f32,A)(float32x4x2_t a, float32x4x2_t b)
{
   a.val[0] = vsubq_f32(a.val[0], b.val[0]);
   a.val[1] = vsubq_f32(a.val[1], b.val[1]);
   return a;
}

static inline float32x4x2_t
FN(vmul2q_f32,A)(float32x4x2_t a, float32x4x2_t b)
{
   a.val[0] = vmulq_f32(a.val[0], b.val[0]);
   a.val[1] = vmulq_f32(a.val[1], b.val[1]);
   return a;
}

static inline float32x4x2_t
FN(vdiv2q_f32,A)(float32x4x2_t a, float32x4x2_t b)
{
   a.val[0] = FN(vdivq_f32,A)(a.val[0], b.val[0]);
   a.val[1] = FN(vdivq_f32,A)(a.val[1], b.val[1]);
   return a;
}

static inline float32x4x2_t
FN(vclt2q_f32,A)(float32x4x2_t a, float32x4x2_t b)
{
   a.val[0] = vreinterpretq_f32_u32(vcltq_f32(a.val[0], b.val[0]));
   a.val[1] = vreinterpretq_f32_u32(vcltq_f32(a.val[1], b.val[1]));
   return a;
}

static inline float32x4x2_t
FN(vcge2q_f32,A)(float32x4x2_t a, float32x4x2_t b)
{
   a.val[0] = vreinterpretq_f32_u32(vcgeq_f32(a.val[0], b.val[0]));
   a.val[1] = vreinterpretq_f32_u32(vcgeq_f32(a.val[1], b.val[1]));
   return a;
}

static inline float32x4x2_t
FN(vabs2q_f32,A)(float32x4x2_t a)
{
   a.val[0] = vabsq_f32(a.val[0]);
   a.val[1] = vabsq_f32(a.val[1]);
   return a;
}

static inline float32x4x2_t
FN(vmla2q_f32,A)(float32x4x2_t a, float32x4x2_t b, float32x4x2_t c)
{
   a.val[0] = vmlaq_f32(a.val[0], b.val[0], c.val[0]);
   a.val[1] = vmlaq_f32(a.val[1], b.val[1], c.val[1]);
   return a;
}

static inline float32x4x2_t
FN(vneg2q_f32,A)(float32x4x2_t a)
{
   a.val[0] = vnegq_f32(a.val[0]);
   a.val[1] = vnegq_f32(a.val[1]);
   return a;
}

static inline float32x4x2_t    // range -1.0f .. 1.0f
FN(fast_sin8,A)(float32x4x2_t x)
{
   float32x4x2_t four = FN(vdup2q_n_f32,A)(-4.0f);
   return FN(vmul2q_f32,A)(four, FN(vmla2q_f32,A)(x, FN(vneg2q_f32,A)(x), FN(vabs2q_f32,A)(x)));
// return FN(vmul2q_f32,A)(four, FN(vsub2q_f32,A)(x, FN(vmul2q_f32,A)(x, FN(vabs2q_f32,A)(x))));
}

#define MUL	(65536.0f*256.0f)
#define IMUL	(1.0f/MUL)

#if 1
// Use the faster, less accurate algorithm
//    GMATH_PI_4*x + 0.273f*x * (1.0f-fabsf(x));
//    which equals to:  x*(GMATH_PI_4+0.273f - 0.273f*fabsf(x))
static inline float32x4_t
FN(fast_atan4,A)(float32x4_t x)
{
   const float32x4_t offs = vmovq_n_f32(GMATH_PI_4+0.273f);
   const float32x4_t mul = vmovq_n_f32(-0.273);

   return vmulq_f32(x, vaddq_f32(offs, vmulq_f32(mul, vabsq_f32(x))));
}
#else
// Use the slower, more accurate algorithm:
//    M_PI_4*x - x*(fabs(x) - 1)*(0.2447 + 0.0663*fabs(x)); // -1 < x < 1
//    which equals to: x*(1.03 - 0.1784*abs(x) - 0.0663*x*x)
static inline float32x4_t
FN(fast_atan4,A)(float32x4_t x)
{
   const float32x4_t pi_4_mul = vmovq_n_f32(1.03f);
   const float32x4_t add = vmovq_n_f32(-0.1784);
   const float32x4_t mull = vmovq_n_f32(-0.0663);

   return vmulq_f32(x, vaddq_f32(pi_4_mul,
                                 vaddq_f32(vmulq_f32(add, vabsq_f32(x)),
                                           vmulq_f32(mull, vmulq_f32(x, x)))));
}
#endif

static inline float32x4_t
FN(copysign,A)(float32x4_t x, float32x4_t y)
{
    uint32x4_t sign_mask = vdupq_n_u32(0x80000000); // This is 0x80000000 in binary
    uint32x4_t y_sign = vandq_u32(vreinterpretq_u32_f32(y), sign_mask);
    uint32x4_t abs_x = vandq_u32(vreinterpretq_u32_f32(x), vmvnq_u32(sign_mask));
    return vreinterpretq_f32_u32(vorrq_u32(abs_x, y_sign));
}

void
FN(batch_dc_shift,A)(float32_ptr d, const_float32_ptr s, size_t num, float offset)
{
   size_t i, step;

   if (!num || offset == 0.0f)
   {
      if (num && d != s) {
         memcpy(d, s, num*sizeof(float));
      }
      return;
   }

   step = sizeof(float32x4_t)/sizeof(float);

   i = num/step;
   if (i)
   {
      float32x4_t *xsptr = (float32x4_t*)s;
      float32x4_t *xdptr = (float32x4_t*)d;
      float32x4_t xoffs = vdupq_n_f32(offset);
      float32x4_t one = vdupq_n_f32(1.0f);

      num -= i*step;
      d += i*step;
      s += i*step;
      do
      {
          float32x4_t xsamp = vld1q_f32((const float*)xsptr++);
          float32x4_t xfact;

          xfact = FN(copysign,A)(xoffs, xsamp);
          xfact = vsubq_f32(one, xfact);

          xsamp = vaddq_f32(xoffs, vmulq_f32(xsamp, xfact));

          vst1q_f32((float*)xdptr++, xsamp);
      } while(--i);

      if (num)
      {
         i = num;
         do
         {
             float samp = *s++;
             float fact = 1.0f-copysignf(offset, samp);
             *d++ = offset + samp*fact;

         } while(--i);
      }
   }
}

void
FN(batch_wavefold,A)(float32_ptr d, const_float32_ptr s, size_t num, float threshold)
{

   size_t i, step;

   if (!num || threshold == 0.0f)
   {
      if (num && d != s) {
         memcpy(d, s, num*sizeof(float));
      }
      return;
   }

   if (num)
   {
      float32x4_t *xdptr = (float32x4_t*)d;
      float32x4_t *xsptr = (float32x4_t*)s;

      step = sizeof(float32x4_t)/sizeof(float);

      i = num/step;
      if (i)
      {
         static const float max = (float)(1 << 23);
         float32x4_t xthresh, xthresh2;
         float threshold2;

         threshold = max*threshold;
         threshold2 = 2.0f*threshold;

         xthresh = vdupq_n_f32(threshold);
         xthresh2 = vdupq_n_f32(2.0f*threshold);

         num -= i*step;
         d += i*step;
         s += i*step;
         do
         {
             float32x4_t xsamp = vld1q_f32((const float*)xsptr++);
             float32x4_t xasamp = vabsq_f32(xsamp);
             uint32x4_t xmask = vcgtq_f32(xasamp, xthresh);

             xasamp = FN(copysign,A)(vsubq_f32(xthresh2, xasamp), xsamp);

             vst1q_f32((float*)xdptr++, vbslq_f32(xmask, xasamp, xsamp));
         } while(--i);

         if (num)
         {
            i = num;
            do
            {
               float samp = *s++;
               float asamp = fabsf(samp);
               if (asamp > threshold)
               {
                  float thresh2 = copysignf(threshold2, samp);
                  samp = thresh2 - asamp;
               }
               *d++ = samp;
            } while(--i);
         }
      }
   }
   else if (num && d != s) {
      memcpy(d, s, num*sizeof(float));
   }
}

float *
FN(aax_generate_waveform,A)(float32_ptr rv, size_t no_samples, float freq, float phase, enum aaxSourceType wtype)
{
   const_float32_ptr phases = _harmonic_phases[wtype-AAX_1ST_WAVE];
   const_float32_ptr harmonics = _harmonics[wtype-AAX_1ST_WAVE];

   switch(wtype)
   {
   case AAX_SINE:
      rv = _aax_generate_waveform_cpu(rv, no_samples, freq, phase, wtype);
      break;
   case AAX_SAWTOOTH:
   case AAX_SQUARE:
   case AAX_TRIANGLE:
   case AAX_CYCLOID:
   case AAX_IMPULSE:
      if (rv)
      {
         static const float fact[8] = { 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f };
         float32x4x2_t phase8, freq8, h8;
         float32x4x2_t one, two, eight;
         float32x4x2_t ngain, nfreq;
         float32x4x2_t hdt, s;
         int i, h;
         float *ptr;

         assert(MAX_HARMONICS % 8 == 0);

         one = FN(vdup2q_n_f32,A)(1.0f);
         two = FN(vdup2q_n_f32,A)(2.0f);
         eight = FN(vdup2q_n_f32,A)(8.0f);

         phase8 = FN(vdup2q_n_f32,A)(-1.0f + phase/GMATH_PI);
         freq8 = FN(vdup2q_n_f32,A)(freq);
         h8 = vld2q_f32(fact);

         nfreq = FN(vdiv2q_f32,A)(freq8, h8);
         ngain = FN(vand2q_f32,A)(FN(vclt2q_f32,A)(two, nfreq), vld2q_f32(harmonics));
         hdt = FN(vdiv2q_f32,A)(two, nfreq);

         ptr = rv;
         i = no_samples;
         s = FN(vadd2q_f32,A)(phase8, vld2q_f32(phases));
         s = FN(vsub2q_f32,A)(s, FN(vand2q_f32,A)(one, FN(vcge2q_f32,A)(s, one)));
         do
         {
            float32x4x2_t rv = FN(fast_sin8,A)(s);

            *ptr++ = FN(hsum_float32x4x2,A)(FN(vmul2q_f32,A)(ngain, rv));

            s = FN(vadd2q_f32,A)(s, hdt);
            s = FN(vsub2q_f32,A)(s, FN(vand2q_f32,A)(two, FN(vcge2q_f32,A)(s, one)));
         }
         while (--i);

         h8 = FN(vadd2q_f32,A)(h8, eight);
         for(h=8; h<MAX_HARMONICS; h += 8)
         {
            nfreq = FN(vdiv2q_f32,A)(freq8, h8);
            ngain = FN(vand2q_f32,A)(FN(vclt2q_f32,A)(two, nfreq), vld2q_f32(harmonics+h));
            if (FN(vtestz2q_f32,A)(ngain))
            {
               hdt = FN(vdiv2q_f32,A)(two, nfreq);

               ptr = rv;
               i = no_samples;
               s = FN(vadd2q_f32,A)(phase8, vld2q_f32(phases+h));
               s = FN(vsub2q_f32,A)(s, FN(vand2q_f32,A)(one, FN(vcge2q_f32,A)(s, one)));
               do
               {
                  float32x4x2_t rv = FN(fast_sin8,A)(s);

                  *ptr++ += FN(hsum_float32x4x2,A)(FN(vmul2q_f32,A)(ngain, rv));

                  s = FN(vadd2q_f32,A)(s, hdt);
                  s = FN(vsub2q_f32,A)(s, FN(vand2q_f32,A)(two, FN(vcge2q_f32,A)(s, one)));
               }
               while (--i);
            }
            h8 = FN(vadd2q_f32,A)(h8, eight);
         }
      }
      break;
   default:
      break;
   }
   return rv;
}

void
FN(batch_get_average_rms,A)(const_float32_ptr s, size_t num, float *rms, float *peak)
{
   size_t step, total;
   double rms_total = 0.0;
   float peak_cur = 0.0f;
   int i;

   *rms = *peak = 0;

   if (!num) return;

   total = num;
   step = 3*sizeof(float32x4_t)/sizeof(float);

   i = num/step;
   if (i)
   {
      union {
         float32x4_t ps;
         float f[4];
      } rms1, rms2, rms3, peak1, peak2, peak3;

      peak1.ps = peak2.ps = peak3.ps = vmovq_n_f32(0.0f);
      rms1.ps = rms2.ps = rms3.ps = vmovq_n_f32(0.0f);

      num -= i*step;
      do
      {
         float32x4_t smp1 = vld1q_f32(s);
         float32x4_t smp2 = vld1q_f32(s+4);
         float32x4_t smp3 = vld1q_f32(s+8);
         float32x4_t val1, val2, val3;

         s += step;

         val1 = vmulq_f32(smp1, smp1);
         val2 = vmulq_f32(smp2, smp2);
         val3 = vmulq_f32(smp3, smp3);

         rms1.ps = vaddq_f32(rms1.ps, val1);
         rms2.ps = vaddq_f32(rms2.ps, val2);
         rms3.ps = vaddq_f32(rms3.ps, val3);

         peak1.ps = vmaxq_f32(peak1.ps, val1);
         peak2.ps = vmaxq_f32(peak2.ps, val2);
         peak3.ps = vmaxq_f32(peak3.ps, val3);
      }
      while(--i);

      rms_total += rms1.f[0] + rms1.f[1] + rms1.f[2] + rms1.f[3];
      rms_total += rms2.f[0] + rms2.f[1] + rms2.f[2] + rms2.f[3];
      rms_total += rms3.f[0] + rms3.f[1] + rms3.f[2] + rms3.f[3];

      peak1.ps = vmaxq_f32(peak1.ps, peak2.ps);
      peak1.ps = vmaxq_f32(peak1.ps, peak3.ps);
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
FN(batch_cvtps24_24,A)(void_ptr dst, const_void_ptr src, size_t num)
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
FN(batch_atanps,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   float32_ptr d = (float32_ptr)dptr;
   float32_ptr s = (float32_ptr)sptr;
   size_t i, step;

   if (num)
   {
      float32x4_t *xdptr = (float32x4_t*)d;
      float32x4_t *xsptr = (float32x4_t*)s;

      step = sizeof(float32x4_t)/sizeof(float);

      i = num/step;
      if (i)
      {
        float32x4_t xmin = vmovq_n_f32(-1.55f);
        float32x4_t xmax = vmovq_n_f32(1.55f);
        float32x4_t mul = vmovq_n_f32(MUL*GMATH_1_PI_2);
        float32x4_t imul = vmovq_n_f32(IMUL);
        float32x4_t res0, res1;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            res0 = vld1q_f32((const float*)xsptr++);

            res0 = vmulq_f32(res0, imul);
            res0 = vminq_f32(vmaxq_f32(res0, xmin), xmax);
            res1 = vmulq_f32(mul, FN(fast_atan4,A)(res0));

            vst1q_f32((float*)xdptr++, res1);
         }
         while(--i);
      }

      if (num) {
         _batch_atanps_cpu(d, s, num);
      }
   }
}

void
FN(batch_roundps,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   float *d = (float*)dptr;
   float *s = (float*)sptr;
   size_t i, step;

   assert(s != 0);
   assert(d != 0);

   if (!num) return;

   step = sizeof(float32x4x4_t)/sizeof(float32_t);

   i = num/step;
   if (i)
   {
      float32x4x4_t *xsptr = (float32x4x4_t*)s;
      float32x4x4_t *xdptr = (float32x4x4_t*)d;
      float32x4x4_t nir4;
      int32x4x4_t nfr4d;

      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         nir4 = vld4q_f32((const float*)xsptr++);

         nfr4d.val[0] = vcvtq_s32_f32(nir4.val[0]);
         nfr4d.val[1] = vcvtq_s32_f32(nir4.val[1]);
         nfr4d.val[2] = vcvtq_s32_f32(nir4.val[2]);
         nfr4d.val[3] = vcvtq_s32_f32(nir4.val[3]);

         nir4.val[0] = vcvtq_f32_s32(nfr4d.val[0]);
         nir4.val[1] = vcvtq_f32_s32(nfr4d.val[1]);
         nir4.val[2] = vcvtq_f32_s32(nfr4d.val[2]);
         nir4.val[3] = vcvtq_f32_s32(nfr4d.val[3]);

         vst4q_f32((float*)xdptr++, nir4);
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
FN(batch_cvt24_ps24,A)(void_ptr dst, const_void_ptr src, size_t num)
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
         s += step;

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
FN(batch_iadd,A)(int32_ptr d, const_int32_ptr src, size_t num)
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
FN(batch_imadd,A)(int32_ptr dst, const_int32_ptr src, size_t num, float v, float vstep)
{
   int32_ptr d = (int32_ptr)dst;
   int32_ptr s = (int32_t *)src;
   size_t i, size, step;

   if (!num || (v <= LEVEL_128DB && vstep <= LEVEL_128DB)) return;
   if (fabsf(v - 1.0f) < LEVEL_96DB && vstep <=  LEVEL_96DB) {
      FN(batch_iadd,A)(dst, src, num);
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
FN(batch_fadd,A)(float32_ptr dst, const_float32_ptr src, size_t num)
{
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step;

   // CPU is faster
   step = 2*sizeof(float32x4_t)/sizeof(float);

   i = num/step;
   if (i)
   {
      float32x4_t *xdptr = (float32x4_t*)d;
      float32x4_t *xsptr = (float32x4_t*)s;

      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         xdptr[0] = vaddq_f32(vld1q_f32((const float*)&xdptr[0]),
                              vld1q_f32((const float*)&xsptr[0]));
         xdptr[1] = vaddq_f32(vld1q_f32((const float*)&xdptr[1]),
                              vld1q_f32((const float*)&xsptr[1]));

         xsptr += 2;
         xdptr += 2;
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
FN(batch_fmadd,A)(float32_ptr dst, const_float32_ptr src, size_t num, float v, float vstep)
{
   int need_step = (fabsf(vstep) <= LEVEL_90DB) ? 0 : 1;
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i;

   // nothing to do
   if (!num || (fabsf(v) <= LEVEL_96DB && !need_step)) return;

   // volume ~= 1.0f and no change requested: just add both buffers
   if (fabsf(v - 1.0f) < LEVEL_90DB && !need_step) {
      FN(batch_fadd,A)(dst, src, num);
      return;
   }

   // volume change requested
   if (need_step)
   {
      // CPU is faster
      size_t step = 2*sizeof(float32x4_t)/sizeof(float);

      i = num/step;
      if (i)
      {
         static const float fact[4] = { 0.0f, 1.0f, 2.0f, 3.0f };
         float32x4_t *xdptr = (float32x4_t*)d;
         float32x4_t *xsptr = (float32x4_t*)s;
         float32x4_t dv, tv, dvstep;

         dvstep = vld1q_f32(fact);
         dvstep = vmulq_f32(dvstep, vdupq_n_f32(vstep));

         dv = vdupq_n_f32(vstep*step);
         tv = vmlaq_f32(vdupq_n_f32(v), dvstep, vdupq_n_f32(vstep));
         v += i*step*vstep;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xdptr[0] = vmlaq_f32(vld1q_f32((const float*)&xdptr[0]),
                                 vld1q_f32((const float*)&xsptr[0]), tv);
            xdptr[1] = vmlaq_f32(vld1q_f32((const float*)&xdptr[1]),
                                 vld1q_f32((const float*)&xsptr[1]), tv);

            tv = vaddq_f32(tv, dv);

            xsptr += 2;
            xdptr += 2;
         }
         while(--i);
      }

      if (num)
      {
         i = num;
         do {
            *d++ += (*s++ * v);
            v += vstep;
         } while(--i);
      }
   }
   else
   {
      size_t step = 2*sizeof(float32x4_t)/sizeof(float);

      i = num/step;
      if (i)
      {
         float32x4_t *xdptr = (float32x4_t*)d;
         float32x4_t *xsptr = (float32x4_t*)s;
         float32x4_t tv;

         tv = vdupq_n_f32(vstep);

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xdptr[0] = vmlaq_f32(vld1q_f32((const float*)&xdptr[0]),
                                 vld1q_f32((const float*)&xsptr[0]), tv);
            xdptr[1] = vmlaq_f32(vld1q_f32((const float*)&xdptr[1]),
                                 vld1q_f32((const float*)&xsptr[1]), tv);

            xsptr += 2;
            xdptr += 2;
         }
         while(--i);
      }

      if (num)
      {
         i = num;
         do {
            *d++ += (*s++ * v);
         } while(--i);
      }
   }
}

void
FN(batch_cvt24_16,A)(void_ptr dst, const_void_ptr src, size_t num)
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
FN(batch_cvt16_24,A)(void_ptr dst, const_void_ptr sptr, size_t num)
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
FN(batch_cvt16_intl_24,A)(void_ptr dst, const_int32_ptrptr src,
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
FN(batch_fmul_value,A)(void_ptr dptr, const_void_ptr sptr, unsigned bps, size_t num, float f)
{
   if (!num) return;

   if (fabsf(f - 1.0f) < LEVEL_128DB) {
      if (sptr != dptr) memcpy(dptr, sptr,  num*bps);
   } else if  (fabsf(f) <= LEVEL_128DB) {
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

         step = 2*sizeof(float32x4_t)/sizeof(float);

         i = num/step;
         if (i)
         {
            float32x4_t *xdptr = (float32x4_t*)d;
            float32x4_t *xsptr = (float32x4_t*)s;
            float32x4_t sfact = vdupq_n_f32(f);

            num -= i*step;
            s += i*step;
            d += i*step;
            do
            {
               xdptr[0] = vmulq_f32(sfact, vld1q_f32((const float*)&xsptr[0]));
               xdptr[1] = vmulq_f32(sfact, vld1q_f32((const float*)&xsptr[1]));

               xsptr += 2;
               xdptr += 2;
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

void
FN(batch_fmul,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   float32_ptr s = (float32_ptr)sptr;
   float32_ptr d = (float32_ptr)dptr;
   size_t i, step;

   if (!num) return;

   step = 2*sizeof(float32x4_t)/sizeof(float);

   i = num/step;
   if (i)
   {
      float32x4_t *xdptr = (float32x4_t*)d;
      float32x4_t *xsptr = (float32x4_t*)s;

      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         xdptr[0] = vmulq_f32(vld1q_f32((const float*)&xdptr[0]),
                              vld1q_f32((const float*)&xsptr[0]));
         xdptr[1] = vmulq_f32(vld1q_f32((const float*)&xdptr[1]),
                              vld1q_f32((const float*)&xsptr[1]));

         xsptr += 2;
         xdptr += 2;
      }
      while(--i);

      if (num) {
         i = num;
         do {
            *d++ *= *s++;
         } while(--i);
      }
   }
}

void
FN(batch_freqfilter_float,A)(float32_ptr dptr, const_float32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;

#ifdef __arm__
   if (filter->state == AAX_BESSEL) {
      return _batch_freqfilter_float_cpu(dptr, sptr, t, num, flt);
   }
#endif

   if (num)
   {
      const_float32_ptr s = sptr;
      float k, *cptr, *hist;
      float32_ptr d = dptr;
      float h0, h1;
      int i, rest;
      int stage;

      cptr = filter->coeff;
      hist = filter->freqfilter->history[t];
      stage = filter->no_stages;
      if (!stage) stage++;

      h0 = hist[0];
      h1 = hist[1];

      i = num/3;
      rest = num-i*3;
      k = filter->k;
      if (i)
      {
         do
         {
            float nsmp = (*s++ * k) + h0 * cptr[0] + h1 * cptr[1];
            *d++ = nsmp             + h0 * cptr[2] + h1 * cptr[3];

            h1 = h0;
            h0 = nsmp;

            nsmp = (*s++ * k) + h0 * cptr[0] + h1 * cptr[1];
            *d++ = nsmp             + h0 * cptr[2] + h1 * cptr[3];

            h1 = h0;
            h0 = nsmp;

            nsmp = (*s++ * k) + h0 * cptr[0] + h1 * cptr[1];
            *d++ = nsmp             + h0 * cptr[2] + h1 * cptr[3];

            h1 = h0;
            h0 = nsmp;
         }
         while (--i);
      }

      if (rest)
      {
         i = rest;
         do
         {
            float nsmp = (*s++ * k) + h0 * cptr[0] + h1 * cptr[1];
            *d++ = nsmp             + h0 * cptr[2] + h1 * cptr[3];

            h1 = h0;
            h0 = nsmp;
         }
         while (--i);
      }

      *hist++ = h0;
      *hist++ = h1;

      while(--stage)
      {
         d = dptr;

         cptr += 4;

         h0 = hist[0];
         h1 = hist[1];

         i = num/3;
         rest = num-i*3;
         if (i)
         {
            do
            {
               float nsmp = *d + h0 * cptr[0] + h1 * cptr[1];
               *d++ = nsmp     + h0 * cptr[2] + h1 * cptr[3];

               h1 = h0;
               h0 = nsmp;

               nsmp = *d + h0 * cptr[0] + h1 * cptr[1];
               *d++ = nsmp     + h0 * cptr[2] + h1 * cptr[3];

               h1 = h0;
               h0 = nsmp;

               nsmp = *d + h0 * cptr[0] + h1 * cptr[1];
               *d++ = nsmp     + h0 * cptr[2] + h1 * cptr[3];

               h1 = h0;
               h0 = nsmp;
            }
            while (--i);
         }

         if (rest)
         {
            i = rest;
            do
            {
               float nsmp = *d + h0 * cptr[0] + h1 * cptr[1];
               *d++ = nsmp     + h0 * cptr[2] + h1 * cptr[3];

               h1 = h0;
               h0 = nsmp;
            }
            while (--i);
         }

         *hist++ = h0;
         *hist++ = h1;
      }
      _batch_fmul_value(dptr, dptr, sizeof(MIX_T), num, filter->gain);
   }
}

static inline void
FN(aaxBufResampleDecimate_float,A)(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   const_float32_ptr s = sptr;
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

   i=dmax-dmin;
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
FN(aaxBufResampleLinear_float,A)(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   const_float32_ptr s = sptr;
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
}

static inline void
FN(aaxBufResampleCubic_float,A)(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
FN(batch_resample_float,A)(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);

   if (fact < CUBIC_TRESHOLD) {
      FN(aaxBufResampleCubic_float,A)(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      FN(aaxBufResampleLinear_float,A)(d, s, dmin, dmax, smu, fact);
   }
   else if (fact >= 1.0f) {
      FN(aaxBufResampleDecimate_float,A)(d, s, dmin, dmax, smu, fact);
   } else {
//    _aaxBufResampleNearest_float,A)(d, s, dmin, dmax, smu, fact);
      memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}
