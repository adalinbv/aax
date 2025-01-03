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

#include <math.h>	/* for floorf */

#include "base/random.h"
#include "software/rbuf_int.h"
#include "arch2d_simd.h"

#define __FN(NAME,ARCH)	_##NAME##_##ARCH
#define FN(NAME,ARCH)	__FN(NAME,ARCH)

inline float	// range -1.0f .. 1.0f
FN(fast_sin,A)(float x)
{
   return -4.0f*(x - x*fabsf(x));
}

static inline FN_PREALIGN float
FN(hsum_ps,A)(__m128 v)
{
   __m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
   __m128 sums = _mm_add_ps(v, shuf);
   shuf = _mm_movehl_ps(shuf, sums);
   sums = _mm_add_ss(sums, shuf);
   return _mm_cvtss_f32(sums);
}

static inline __m128
FN(mm_abs_ps,A)(__m128 x)
{
   const __m128 sign_mask = _mm_set1_ps(-0.0f);
   return _mm_andnot_ps(sign_mask, x);
}

static inline int
FN(mm_testz_ps,A)(__m128 x)
{
   __m128i zero = _mm_setzero_si128();
   return (_mm_movemask_epi8(_mm_cmpeq_epi32(_mm_castps_si128(x), zero)) != 0xFFFF);
}

static inline __m128	// range -1.0f .. 1.0f
FN(fast_sin4,A)(__m128 x)
{
   const __m128 four = _mm_set1_ps(-4.0f);
   return _mm_mul_ps(four, _mm_sub_ps(x, _mm_mul_ps(x, FN(mm_abs_ps,A)(x))));
}

// Use the faster, less accurate algorithm:
//    GMATH_PI_4*x + 0.273f*x * (1.0f-fabsf(x));
//    which equals to:  x*(GMATH_PI_4+0.273f - 0.273f*fabsf(x))
static inline __m128
FN(fast_atan4,A)(__m128 x)
{
  const __m128 offs = _mm_set1_ps(GMATH_PI_4+0.273f);
  const __m128 mul = _mm_set1_ps(-0.273f);
  return _mm_mul_ps(x, _mm_add_ps(offs, _mm_mul_ps(mul, FN(mm_abs_ps,A)(x))));
}

static inline FN_PREALIGN __m128
_sse_fmadd_ps(__m128 a, __m128 b, __m128 c) {
   return _mm_add_ps(_mm_mul_ps(a, b), c);
}
static inline FN_PREALIGN __m128
FN(_mm_atan_ps,A)(__m128 a)
{
   const __m128 sign_mask = _mm_set1_ps(-0.0f);
   __m128 sign = _mm_and_ps(a, sign_mask); // Preserve sign
   a = _mm_andnot_ps(sign_mask, a); // Absolute value

   // w = a > tan(PI / 8)
   const __m128 tan_pi_8 = _mm_set1_ps(GMATH_TAN_PI_8);
   __m128 w = _mm_cmpgt_ps(a, tan_pi_8);

   // x = a > tan(3 * PI / 8)
   const __m128 tan_3pi_8 = _mm_set1_ps(GMATH_TAN_3PI_8);
   __m128 x = _mm_cmpgt_ps(a, tan_3pi_8);

   // z = ~w & x
   __m128 z = _mm_andnot_ps(w, x);

   // y = (~w & PI/2) | (z & PI/4)
   __m128 y = _mm_or_ps(_mm_andnot_ps(w, _mm_set1_ps(GMATH_PI_2)),
                        _mm_and_ps(z, _mm_set1_ps(GMATH_PI_4)));

   // w = (w & -1/a) | (z & (a - 1) * 1/(a + 1))
   const __m128 one = _mm_set1_ps(1.0f);
   __m128 inv_a = _mm_rcp_ps(a); // -1 / a
   __m128 w_part1 = _mm_and_ps(w, inv_a);
   __m128 w_part2 = _mm_and_ps(z, _mm_mul_ps(_mm_sub_ps(a, one), _mm_rcp_ps(_mm_add_ps(a, one))));
   __m128 w_final = _mm_or_ps(w_part1, w_part2);

   // a = (~x & a) | w
   __m128 adjusted_a = _mm_or_ps(_mm_andnot_ps(x, a), w_final);

   // Polynomial approximation for arctangent
   __m128 poly, a2 = _mm_mul_ps(adjusted_a, adjusted_a);
   poly = _sse_fmadd_ps(_mm_set1_ps(ATAN_COEF1), a2, _mm_set1_ps(ATAN_COEF2));
   poly = _sse_fmadd_ps(poly, a2, _mm_set1_ps(ATAN_COEF3));
   poly = _sse_fmadd_ps(poly, a2, _mm_set1_ps(ATAN_COEF4));
   __m128 result = _sse_fmadd_ps(poly, _mm_mul_ps(a2, adjusted_a), adjusted_a);

   return _mm_or_ps(result, sign);}

static inline __m128
FN(copysign,A)(__m128 x, __m128 y)
{
    const __m128 sign_mask = _mm_set1_ps(-0.0f); // This is 0x80000000 in binary
    __m128 y_sign = _mm_and_ps(y, sign_mask);
    __m128 abs_x = _mm_andnot_ps(sign_mask, x);
    return _mm_or_ps(abs_x, y_sign);
}

void
FN(batch_dc_shift,A)(float32_ptr d, const_float32_ptr s, size_t num, float offset)
{
   size_t i, step;
   size_t dtmp, stmp;

   if (!num || offset == 0.0f)
   {
      if (num && d != s) {
         memcpy(d, s, num*sizeof(float));
      }
      return;
   }

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if (dtmp || stmp)                    /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      _batch_dc_shift_cpu(d, s, num, offset);
      return;
   }

   if (num)
   {
      __m128 *dptr = (__m128*)d;
      __m128* sptr = (__m128*)s;

      step = sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         const __m128 one = _mm_set1_ps(1.0f);
         __m128 xoffs = _mm_set1_ps(offset);

         num -= i*step;
         d += i*step;
         s += i*step;
         do
         {
             __m128 xsamp = _mm_load_ps((const float*)sptr++);
             __m128 xdptr, xfact;

             xfact = FN(copysign,A)(xoffs, xsamp);
             xfact = _mm_sub_ps(one, xfact);

             xdptr = _mm_add_ps(xoffs, _mm_mul_ps(xsamp, xfact));

             _mm_store_ps((float*)dptr++, xdptr);
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
}

#if 0
void
FN(batch_wavefold,A)(float32_ptr d, const_float32_ptr s, size_t num, float threshold)
{
   size_t i, step;
   size_t dtmp, stmp;

   if (!num || threshold == 0.0f)
   {
      if (num && d != s) {
         memcpy(d, s, num*sizeof(float));
      }
      return;
   }

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if (dtmp || stmp)                    /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      _batch_wavefold_cpu(d, s, num, threshold);
      return;
   }

   if (num)
   {
      __m128 *dptr = (__m128*)d;
      __m128* sptr = (__m128*)s;

      step = sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         static const float max = (float)(1 << 23);
         __m128 xthresh, xthresh2;
         float threshold2;

         threshold = max*threshold;
         threshold2 = 2.0f*threshold;

         xthresh = _mm_set1_ps(threshold);
         xthresh2 = _mm_set1_ps(threshold2);

         num -= i*step;
         d += i*step;
         s += i*step;
         do
         {
             __m128 xsamp = _mm_load_ps((const float*)sptr++);
             __m128 xasamp = FN(mm_abs_ps,A)(xsamp);
             __m128 xmask = _mm_cmpgt_ps(xasamp, xthresh);

             xasamp = FN(copysign,A)(_mm_sub_ps(xthresh2, xasamp), xsamp);

             xsamp = _mm_andnot_ps(xmask, xsamp);
             xasamp = _mm_and_ps(xmask, xasamp);

             _mm_store_ps((float*)dptr++, _mm_or_ps(xsamp, xasamp));
         } while(--i);

         if (num)
         {
            i = num;
            do
            {
               float samp = *s++;
               float asamp = fabsf(samp);
               if (asamp > threshold) {
                  samp = copysignf(threshold2 - asamp, samp);
               }
               *d++ = samp;
            } while(--i);
         }
      }
   }
}
#endif

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
         const __m128 one = _mm_set1_ps(1.0f);
         const __m128 two = _mm_set1_ps(2.0f);
         const __m128 four = _mm_set1_ps(4.0f);
         __m128 phase4, freq4, h4;
         __m128 ngain, nfreq;
         __m128 hdt, s;
         int i, h;
         float *ptr;

         assert(MAX_HARMONICS % 4 == 0);

         phase4 = _mm_set1_ps(-1.0f + phase/GMATH_PI);
         freq4 = _mm_set1_ps(freq);
         h4 = _mm_set_ps(4.0f, 3.0f, 2.0f, 1.0f);

         nfreq = _mm_div_ps(freq4, h4);
         ngain = _mm_and_ps(_mm_cmplt_ps(two, nfreq), _mm_load_ps(harmonics));
         hdt = _mm_div_ps(two, nfreq);

         ptr = rv;
         i = no_samples;
         s = _mm_add_ps(phase4, _mm_load_ps(phases));
         s = _mm_sub_ps(s, _mm_and_ps(one, _mm_cmpge_ps(s, one)));
         do
         {
            __m128 rv = FN(fast_sin4,A)(s);

            *ptr++ = FN(hsum_ps,A)(_mm_mul_ps(ngain, rv));

            s = _mm_add_ps(s, hdt);
            s = _mm_sub_ps(s, _mm_and_ps(two, _mm_cmpge_ps(s, one)));
         }
         while (--i);

         h4 = _mm_add_ps(h4, four);
         for(h=4; h<MAX_HARMONICS; h += 4)
         {
            nfreq = _mm_div_ps(freq4, h4);
            ngain = _mm_and_ps(_mm_cmplt_ps(two, nfreq), _mm_load_ps(harmonics+h));
            if (FN(mm_testz_ps,A)(ngain))
            {
               hdt = _mm_div_ps(two, nfreq);

               ptr = rv;
               i = no_samples;
               s = _mm_add_ps(phase4, _mm_load_ps(phases+h));
               s = _mm_sub_ps(s, _mm_and_ps(one, _mm_cmpge_ps(s, one)));
               do
               {
                  __m128 rv = FN(fast_sin4,A)(s);

                  *ptr++ += FN(hsum_ps,A)(_mm_mul_ps(ngain, rv));

                  s = _mm_add_ps(s, hdt);
                  s = _mm_sub_ps(s, _mm_and_ps(two, _mm_cmpge_ps(s, one)));
               }
               while (--i);
            }
            h4 = _mm_add_ps(h4, four);
         }
      }
      break;
   default:
      break;
   }
   return rv;
}

#define FC      50.0f // 50Hz high-pass EMA filter cutoff frequency
float *
FN(aax_generate_noise,A)(float32_ptr rv, size_t no_samples, uint64_t seed, unsigned char skip, float fs)
{
   if (rv)
   {
      float (*rnd_fn)() = _aax_random;
      float rnd_skip = skip ? skip : 1.0f;
      float ds, prev, alpha;
      float *end = rv + no_samples;
      float *ptr = rv;

      if (seed)
      {
          _aax_srand(seed);
          rnd_fn = _aax_seeded_random;
      }

      prev = 0.0f;
      alpha = 1.0f;
      // exponential moving average (ema) filter
      // to filter frequencies below FC (50Hz)
      _aax_ema_compute(FC, fs, &alpha);

      ds = FC/fs;

      memset(rv, 0, no_samples*sizeof(float));
      do
      {
         float rnd = 0.5f*rnd_fn();
         rnd = rnd - _MINMAX(rnd, -ds, ds);

         // exponential moving average filter
         prev = (1.0f-alpha)*prev + alpha*rnd;
         *ptr += rnd - prev; // high-pass

         ptr += (int)rnd_skip;
         if (skip > 1) {
            rnd_skip = 1.0f + fabsf((2*skip-rnd_skip)*rnd_fn());
         }
      }
      while (ptr < end);
   }
   return rv;
}

void
FN(batch_get_average_rms,A)(const_float32_ptr s, size_t num, float *rms, float *peak)
{
   size_t stmp, step, total;
   double rms_total = 0.0;
   float peak_cur = 0.0f;
   int i;

   *rms = *peak = 0;

   if (!num) return;

   total = num;
   stmp = (size_t)s & MEMMASK16;
   if (stmp)
   {
      i = (MEMALIGN16 - stmp)/sizeof(float);
      if (i <= num)
      {
         do
         {
            float samp = *s++;            // rms
            float val = samp*samp;
            rms_total += val;
            if (val > peak_cur) peak_cur = val;
         }
         while (--i);
      }
   }

   if (num)
   {
      __m128* sptr = (__m128*)s;

      step = 2*sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         union {
             __m128 ps;
             float f[4];
         } rms1, rms2, peak1, peak2;

         peak1.ps = peak2.ps = _mm_setzero_ps();
         rms1.ps = rms2.ps = _mm_setzero_ps();

         s += i*step;
         num -= i*step;
         do
         {
            __m128 smp1 = _mm_load_ps((const float*)sptr++);
            __m128 smp2 = _mm_load_ps((const float*)sptr++);
            __m128 val1, val2;

            val1 = _mm_mul_ps(smp1, smp1);
            val2 = _mm_mul_ps(smp2, smp2);

            rms1.ps = _mm_add_ps(rms1.ps, val1);
            rms2.ps = _mm_add_ps(rms2.ps, val2);

            peak1.ps = _mm_max_ps(peak1.ps, val1);
            peak2.ps = _mm_max_ps(peak2.ps, val2);
         }
         while(--i);

         rms_total += rms1.f[0] + rms1.f[1] + rms1.f[2] + rms1.f[3];
         rms_total += rms2.f[0] + rms2.f[1] + rms2.f[2] + rms2.f[3];

         peak1.ps = _mm_max_ps(peak1.ps, peak2.ps);
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
   }

   *rms = (float)sqrt(rms_total/total);
   *peak = sqrtf(peak_cur);
}

void
FN(batch_saturate24,A)(void *data, size_t num)
{
   if (num)
   {
      int32_t* p = (int32_t*)data;
      size_t i = num;
      do
      {
         int32_t samp = _MINMAX(*p, -AAX_PEAK_MAX, AAX_PEAK_MAX);
         *p++ = samp;
      }
      while(--i);
   }
}

void
FN(batch_cvtps_24,A)(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *s = (int32_t*)src;
   float *d = (float*)dst;

   assert(((size_t)d & MEMMASK16) == 0);
   assert(((size_t)s & MEMMASK16) == 0);

   if (num)
   {
      __m128i* sptr = (__m128i*)s;
      __m128 *dptr = (__m128*)d;
      size_t i, step;

      step = 4*sizeof(__m128i)/sizeof(int32_t);

      i = num/step;
      if (i)
      {
         __m128i xmm0i, xmm1i, xmm2i, xmm3i;
         __m128 xmm4, xmm5, xmm6, xmm7;
         __m128 mul = _mm_rcp_ps(_mm_set1_ps(MUL));

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xmm0i = _mm_load_si128(sptr++);
            xmm1i = _mm_load_si128(sptr++);
            xmm2i = _mm_load_si128(sptr++);
            xmm3i = _mm_load_si128(sptr++);

            xmm4 = _mm_cvtepi32_ps(xmm0i);
            xmm5 = _mm_cvtepi32_ps(xmm1i);
            xmm6 = _mm_cvtepi32_ps(xmm2i);
            xmm7 = _mm_cvtepi32_ps(xmm3i);

            xmm4 = _mm_mul_ps(xmm4, mul);
            xmm5 = _mm_mul_ps(xmm5, mul);
            xmm6 = _mm_mul_ps(xmm6, mul);
            xmm7 = _mm_mul_ps(xmm7, mul);

            _mm_store_ps((float*)dptr++, xmm4);
            _mm_store_ps((float*)dptr++, xmm5);
            _mm_store_ps((float*)dptr++, xmm6);
            _mm_store_ps((float*)dptr++, xmm7);;
         }
         while(--i);
      }

      if (num)
      {
         float mul = IMUL;
         i = num;
         do {
            *d++ = (float)(*s++) * mul;
         } while (--i);
      }
   }
}

void
FN(batch_limit,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   float *d = (float*)dptr;
   float *s = (float*)sptr;
   size_t i, step;
   size_t dtmp, stmp;

   if (!num) return;

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if (dtmp || stmp)  			/* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      _batch_limit_cpu(d, s, num);
      return;
   }

   if (num)
   {
      __m128 *dptr = (__m128*)d;
      __m128* sptr = (__m128*)s;

      step = sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         const __m128 xmin = _mm_set1_ps(-1.94139795f);
         const __m128 xmax = _mm_set1_ps(1.94139795f);
         const __m128 mul = _mm_set1_ps(MUL*GMATH_1_PI_2);
         const __m128 imul = _mm_set1_ps(IMUL);
         __m128 xmm0, xmm1;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xmm0 = _mm_load_ps((const float*)sptr++);

            xmm0 = _mm_mul_ps(xmm0, imul);
            xmm0 = _mm_min_ps(_mm_max_ps(xmm0, xmin), xmax);
            xmm1 = _mm_mul_ps(mul, FN(fast_atan4,A)(xmm0));

            _mm_store_ps((float*)dptr++, xmm1);
         }
         while(--i);
      }

      if (num) {
         _batch_limit_cpu(d, s, num);
      }
   }
}

void
FN(batch_atanps,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   float *d = (float*)dptr;
   float *s = (float*)sptr;
   size_t i, step;
   size_t dtmp, stmp;

   if (!num) return;

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if (dtmp || stmp)                    /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      _batch_atanps_cpu(d, s, num);
      return;
   }

   if (num)
   {
      __m128 *dptr = (__m128*)d;
      __m128* sptr = (__m128*)s;

      step = sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         const __m128 mul = _mm_set1_ps(MUL*GMATH_1_PI_2);
         const __m128 imul = _mm_set1_ps(IMUL);
         __m128 xmm0, xmm1;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xmm0 = _mm_mul_ps(imul, _mm_load_ps((const float*)sptr++));
            xmm1 = FN(_mm_atan_ps,A)(xmm0);
            _mm_store_ps((float*)dptr++, _mm_mul_ps(xmm1, mul));
         }
         while(--i);
      }

      if (num) {
         _batch_atanps_cpu(d, s, num);
      }
   }
}

static void
FN(batch_iadd,A)(int32_ptr dst, const_int32_ptr src, size_t num)
{
   int32_ptr d = (int32_ptr)dst;
   int32_ptr s = (int32_ptr)src;
   size_t i, step, dtmp, stmp;

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
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
      i = (MEMALIGN16 - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do
         {
            *d++ += *s++;
         } while(--i);
      }
   }

   step = 2*sizeof(__m128i)/sizeof(int32_t);

   i = num/step;
   if (i)
   {
      __m128i *sptr = (__m128i *)s;
      __m128i *dptr = (__m128i *)d;
      __m128i xmm0i, xmm3i, xmm4i, xmm7i;

      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         xmm0i = _mm_load_si128(sptr++);
         xmm4i = _mm_load_si128(sptr++);

         xmm3i = _mm_load_si128(dptr);
         xmm7i = _mm_load_si128(dptr+1);

         xmm0i = _mm_add_epi32(xmm0i, xmm3i);
         xmm4i = _mm_add_epi32(xmm4i, xmm7i);

         _mm_store_si128(dptr++, xmm0i);
         _mm_store_si128(dptr++, xmm4i);
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
FN(batch_imadd,A)(int32_ptr dst, const_int32_ptr src, size_t num, float v, float vstep)
{
   int32_ptr d = (int32_ptr)dst;
   int32_ptr s = (int32_ptr)src;
   size_t i, step, dtmp, stmp;

   if (!num || (v <= LEVEL_90DB && vstep <= LEVEL_90DB)) return;
   if (fabsf(v - 1.0f) < LEVEL_90DB && vstep <=  LEVEL_90DB)
   {
      FN(batch_iadd,A)(dst, src, num);
      return;
   }

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
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
      i = (MEMALIGN16 - dtmp)/sizeof(int32_t);
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

   step = 2*sizeof(__m128i)/sizeof(int32_t);

   i = num/step;
   if (i)
   {
      __m128i *sptr = (__m128i *)s;
      __m128i *dptr = (__m128i *)d;
      __m128i xmm0i, xmm3i, xmm4i, xmm7i;
      __m128 xmm1, xmm5;

      vstep *= step;
      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         __m128 tv = _mm_set1_ps(v);

         xmm0i = _mm_load_si128(sptr++);
         xmm4i = _mm_load_si128(sptr++);

         xmm1 = _mm_cvtepi32_ps(xmm0i);
         xmm5 = _mm_cvtepi32_ps(xmm4i);

         xmm1 = _mm_mul_ps(xmm1, tv);
         xmm5 = _mm_mul_ps(xmm5, tv);

         xmm0i = _mm_load_si128(dptr);
         xmm4i = _mm_load_si128(dptr+1);

         xmm3i = _mm_cvtps_epi32(xmm1);
         xmm7i = _mm_cvtps_epi32(xmm5);

         xmm0i = _mm_add_epi32(xmm0i, xmm3i);
         xmm4i = _mm_add_epi32(xmm4i, xmm7i);

         v += vstep;

         _mm_store_si128(dptr++, xmm0i);
         _mm_store_si128(dptr++, xmm4i);
      }
      while(--i);
   }

   if (num)
   {
      vstep /= step;
      i = num;
      do {
         *d++ += (int32_t)((float)*s++ * v);
         v += vstep;
      } while(--i);
   }
}

FN_PREALIGN static void
FN(batch_fadd,A)(float32_ptr dst, const_float32_ptr src, size_t num)
{
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i, step, dtmp, stmp;

   /* work towards a 16-byte aligned d (and hence 16-byte aligned s) */
   dtmp = (size_t)d & MEMMASK16;
   if (dtmp && num)
   {
      i = (MEMALIGN16 - dtmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ += *s++;
         } while(--i);
      }
   }
   stmp = (size_t)s & MEMMASK16;

   step = 2*sizeof(__m128)/sizeof(float);
   i = num/step;
   if (i)
   {
      __m128* sptr = (__m128*)s;
      __m128 *dptr = (__m128*)d;
      __m128 xmm0, xmm1, xmm2, xmm3;

      num -= i*step;
      s += i*step;
      d += i*step;
      if (stmp)
      {
         do
         {
            xmm0 = _mm_loadu_ps((const float*)sptr++);
            xmm1 = _mm_loadu_ps((const float*)sptr++);

            xmm2 = _mm_load_ps((const float*)(dptr+0));
            xmm3 = _mm_load_ps((const float*)(dptr+1));

            _mm_store_ps((float*)dptr++, _mm_add_ps(xmm0, xmm2));
            _mm_store_ps((float*)dptr++, _mm_add_ps(xmm0, xmm3));
         }
         while(--i);
      }
      else
      {
         do
         {
            xmm0 = _mm_load_ps((const float*)sptr++);
            xmm1 = _mm_load_ps((const float*)sptr++);

            xmm2 = _mm_load_ps((const float*)(dptr+0));
            xmm3 = _mm_load_ps((const float*)(dptr+1));

            _mm_store_ps((float*)dptr++, _mm_add_ps(xmm0, xmm2));
            _mm_store_ps((float*)dptr++, _mm_add_ps(xmm1, xmm3));
         }
         while(--i);
      }
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
FN(batch_fmul,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   const_float32_ptr s = (float32_ptr)sptr;
   float32_ptr d = (float32_ptr)dptr;
   size_t i, step, dtmp, stmp;

   if (!num) return;

   /* work towards a 16-byte aligned d (and hence 16-byte aligned s) */
   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if (dtmp && num)
   {
      i = (MEMALIGN16 - dtmp)/sizeof(float);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ *= *s++;
         } while(--i);
      }
   }

   step = 4*sizeof(__m128)/sizeof(float);

   i = num/step;
   if (i)
   {
      __m128* sptr = (__m128*)s;
      __m128* dptr = (__m128*)d;
      __m128 xmm0, xmm1, xmm2, xmm3;
      __m128 xmm4, xmm5, xmm6, xmm7;

      num -= i*step;
      s += i*step;
      d += i*step;
      if (stmp)
      {
         do
         {
            xmm0 = _mm_loadu_ps((const float*)(sptr++));
            xmm1 = _mm_loadu_ps((const float*)(sptr++));
            xmm2 = _mm_loadu_ps((const float*)(sptr++));
            xmm3 = _mm_loadu_ps((const float*)(sptr++));

            xmm4 = _mm_load_ps((const float*)(dptr+0));
            xmm5 = _mm_load_ps((const float*)(dptr+1));
            xmm6 = _mm_load_ps((const float*)(dptr+2));
            xmm7 = _mm_load_ps((const float*)(dptr+3));

            xmm0 = _mm_mul_ps(xmm0, xmm4);
            xmm1 = _mm_mul_ps(xmm1, xmm5);
            xmm2 = _mm_mul_ps(xmm2, xmm6);
            xmm3 = _mm_mul_ps(xmm3, xmm7);

           _mm_store_ps((float*)dptr++, xmm0);
           _mm_store_ps((float*)dptr++, xmm1);
           _mm_store_ps((float*)dptr++, xmm2);
           _mm_store_ps((float*)dptr++, xmm3);
        }
        while(--i);
     }
     else
     {
        do
        {
           xmm0 = _mm_load_ps((const float*)(sptr++));
           xmm1 = _mm_load_ps((const float*)(sptr++));
           xmm2 = _mm_load_ps((const float*)(sptr++));
           xmm3 = _mm_load_ps((const float*)(sptr++));

           xmm4 = _mm_load_ps((const float*)(dptr+0));
           xmm5 = _mm_load_ps((const float*)(dptr+1));
           xmm6 = _mm_load_ps((const float*)(dptr+2));
           xmm7 = _mm_load_ps((const float*)(dptr+3));

           xmm0 = _mm_mul_ps(xmm0, xmm4);
           xmm1 = _mm_mul_ps(xmm1, xmm5);
           xmm2 = _mm_mul_ps(xmm2, xmm6);
           xmm3 = _mm_mul_ps(xmm3, xmm7);

           _mm_store_ps((float*)dptr++, xmm0);
           _mm_store_ps((float*)dptr++, xmm1);
           _mm_store_ps((float*)dptr++, xmm2);
           _mm_store_ps((float*)dptr++, xmm3);
        }
        while(--i);
     }
  }

  if (num)
  {
     i = num;
     do {
        *d++ *= *s++;
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
   if (fabsf(v - 1.0f) < LEVEL_90DB && !need_step)
   {
      FN(batch_fadd,A)(dst, src, num);
      return;
   }

   // volume change requested
   if (need_step)
   {
      size_t step, dtmp, stmp;

      /* work towards a 16-byte aligned d (and hence 16-byte aligned s) */
      dtmp = (size_t)d & MEMMASK16;
      if (dtmp && num)
      {
         i = (MEMALIGN16 - dtmp)/sizeof(float);
         if (i <= num)
         {
            num -= i;
            do {
               *d++ += *s++ * v;
               v += vstep;
            } while(--i);
         }
      }
      stmp = (size_t)s & MEMMASK16;

      step = 2*sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m128 dvstep1 = _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f);
         __m128 dvstep2 = _mm_set_ps(7.0f, 6.0f, 5.0f, 4.0f);
         __m128 xmm1, xmm2, xmm3;
         __m128 xmm5, xmm6, xmm7;
         __m128 dv, tv1, tv2;
         __m128* sptr = (__m128*)s;
         __m128* dptr = (__m128*)d;

         assert(step == 2*4);
         dvstep1 = _mm_mul_ps(dvstep1, _mm_set1_ps(vstep));
         dvstep2 = _mm_mul_ps(dvstep2, _mm_set1_ps(vstep));

         dv = _mm_set1_ps(vstep*step);
         tv1 = _mm_add_ps(_mm_set1_ps(v), dvstep1);
         tv2 = _mm_add_ps(_mm_set1_ps(v), dvstep2);
         v += step*vstep;

         num -= i*step;
         s += i*step;
         d += i*step;
         if (stmp)
         {
            do
            {
               xmm1 = _mm_loadu_ps((const float*)sptr++);
               xmm5 = _mm_loadu_ps((const float*)sptr++);

               xmm2 = _mm_load_ps((const float*)dptr);
               xmm6 = _mm_load_ps((const float*)(dptr+1));

               xmm3 = _mm_add_ps(_mm_mul_ps(tv1, xmm1), xmm2);
               xmm7 = _mm_add_ps(_mm_mul_ps(tv2, xmm5), xmm6);

               tv1 = _mm_add_ps(tv1, dv);
               tv2 = _mm_add_ps(tv2, dv);

               _mm_store_ps((float*)dptr++, xmm3);
               _mm_store_ps((float*)dptr++, xmm7);
            }
            while(--i);
         }
         else
         {
            do
            {
               xmm1 = _mm_load_ps((const float*)sptr++);
               xmm5 = _mm_load_ps((const float*)sptr++);

               xmm2 = _mm_load_ps((const float*)dptr);
               xmm6 = _mm_load_ps((const float*)(dptr+1));

               xmm3 = _mm_add_ps(_mm_mul_ps(tv1, xmm1), xmm2);
               xmm7 = _mm_add_ps(_mm_mul_ps(tv2, xmm5), xmm6);

               tv1 = _mm_add_ps(tv1, dv);
               tv2 = _mm_add_ps(tv2, dv);

               _mm_store_ps((float*)dptr++, xmm3);
               _mm_store_ps((float*)dptr++, xmm7);
            }
            while(--i);
         }
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
      size_t step, dtmp, stmp;

      /* work towards a 16-byte aligned d (and hence 16-byte aligned s) */
      dtmp = (size_t)d & MEMMASK16;
      if (dtmp)
      {
         i = (MEMALIGN16 - dtmp)/sizeof(int32_t);
         if (i <= num)
         {
            num -= i;
            do {
               *d++ += *s++ * v;
            } while(--i);
         }
      }
      stmp = (size_t)s & MEMMASK16;

      step = 2*sizeof(__m128)/sizeof(float);
      i = num/step;
      if (i)
      {
         __m128* sptr = (__m128*)s;
         __m128* dptr = (__m128*)d;
         __m128 tv = _mm_set1_ps(v);
         __m128 xmm1, xmm2, xmm3;
         __m128 xmm5, xmm6, xmm7;

         num -= i*step;
         s += i*step;
         d += i*step;
         if (stmp)
         {
            do
            {
               xmm1 = _mm_loadu_ps((const float*)sptr++);
               xmm5 = _mm_loadu_ps((const float*)sptr++);

               xmm2 = _mm_load_ps((const float*)dptr);
               xmm6 = _mm_load_ps((const float*)(dptr+1));

               xmm3 = _mm_add_ps(_mm_mul_ps(tv, xmm1), xmm2);
               xmm7 = _mm_add_ps(_mm_mul_ps(tv, xmm5), xmm6);

               _mm_store_ps((float*)dptr++, xmm3);
               _mm_store_ps((float*)dptr++, xmm7);
            }
            while(--i);
         }
         else
         {
            do
            {
               xmm1 = _mm_load_ps((const float*)sptr++);
               xmm5 = _mm_load_ps((const float*)sptr++);

               xmm2 = _mm_load_ps((const float*)dptr);
               xmm6 = _mm_load_ps((const float*)(dptr+1));

               xmm3 = _mm_add_ps(_mm_mul_ps(tv, xmm1), xmm2);
               xmm7 = _mm_add_ps(_mm_mul_ps(tv, xmm5), xmm6);

               _mm_store_ps((float*)dptr++, xmm3);
               _mm_store_ps((float*)dptr++, xmm7);
            }
            while(--i);
         }
      }

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
FN(batch_ema_iir_float,A)(float32_ptr d, const_float32_ptr s, size_t num, float *hist, float a1)
{
   if (num)
   {
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

#define NUM_BUTTERWORTH	4
#define NUM_BESSEL	8
void
FN(batch_freqfilter_float,A)(float32_ptr dptr, const_float32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   const_float32_ptr s = sptr;

   if (num)
   {
      float k, *cptr, *hist;
      float c0, c1;
      float h0, h1;
      int stage;

      cptr = filter->coeff;
      hist = filter->freqfilter->history[t];
      stage = filter->no_stages;
      if (!stage) stage++;

      c0 = cptr[0];
      c1 = cptr[1];

      h0 = hist[0];
      h1 = hist[1];

      k = filter->k;
      if (filter->state == AAX_BUTTERWORTH)
      {
         float32_ptr d = dptr;
         int j, i = num/NUM_BUTTERWORTH;
         int rest = num-i*NUM_BUTTERWORTH;

         if (i)
         {
            do
            {
               for (j=0; j<NUM_BUTTERWORTH; ++j)
               {
                  float nsmp = (*s++ * k) + h0 * cptr[0] + h1 * cptr[1];
                  *d++ = nsmp             + h0 * cptr[2] + h1 * cptr[3];

                  h1 = h0;
                  h0 = nsmp;
               }
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
      }
      else
      {
         float32_ptr d = dptr;
         int j, i = num/NUM_BESSEL;
         int rest = num-i*NUM_BESSEL;

         do
         {
            for (j=0; j<NUM_BESSEL; ++j)
            {
               float smp = (*s++ * k) + ((h0 * c0) + (h1 * c1));
               *d++ = smp;

               h1 = h0;
               h0 = smp;
            }
         }
         while (--i);

         if (rest)
         {
            i = rest;
            do
            {
               float smp = (*s++ * k) + ((h0 * c0) + (h1 * c1));
               *d++ = smp;

               h1 = h0;
               h0 = smp;
            }
            while (--i);
         }
      }

      *hist++ = h0;
      *hist++ = h1;

      while(--stage)
      {
         cptr += 4;

         c0 = cptr[0];
         c1 = cptr[1];

         h0 = hist[0];
         h1 = hist[1];

         if (filter->state == AAX_BUTTERWORTH)
         {
            float32_ptr d = dptr;
            int j, i = num/NUM_BUTTERWORTH;
            int rest = num-i*NUM_BUTTERWORTH;

            if (i)
            {
               do
               {
                  for (j=0; j<NUM_BUTTERWORTH; ++j)
                  {
                     float nsmp = *d + h0 * cptr[0] + h1 * cptr[1];
                     *d++ = nsmp     + h0 * cptr[2] + h1 * cptr[3];

                     h1 = h0;
                     h0 = nsmp;
                  }
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
         }
         else
         {
            float32_ptr d = dptr;
            int j, i = num/NUM_BESSEL;
            int rest = num-i*NUM_BESSEL;

            do
            {
               for (j=0; j<NUM_BESSEL; ++j)
               {
                  float smp = *d  + ((h0 * c0) + (h1 * c1));
                  *d++ = smp;

                  h1 = h0;
                  h0 = smp;
               }
            }
            while (--i);

            if (rest)
            {
               i = rest;
               do
               {
                  float smp = *d  + ((h0 * c0) + (h1 * c1));
                  *d++ = smp;

                  h1 = h0;
                  h0 = smp;
               }
               while (--i);
            }
         }

         *hist++ = h0;
         *hist++ = h1;
      }
      _batch_fmul_value(dptr, dptr, num, filter->gain, 1.0f);
   }
}


static inline void
FN(aaxBufResampleDecimate_float,A)(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
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
FN(aaxBufResampleNearest_float,A)(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   if (freq_factor == 1.0f) {
      memcpy(d+dmin, s, (dmax-dmin)*sizeof(float));
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

// https://github.com/depp/libfresample, BSD license
// http://www.earlevel.com/main/2007/07/03/sample-rate-conversion/
// https://en.wikipedia.org/wiki/Hermite_interpolation
static inline void
FN(aaxBufResampleLinear_float,A)(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
#if 1
      __m128 samp = _mm_load_ss(sptr++);       // n
      __m128 nsamp = _mm_load_ss(sptr++);      // (n+1)
      __m128 dsamp = _mm_sub_ss(nsamp, samp);  // (n+1) - n

      do
      {
         __m128 tau = _mm_set_ss(smu);
         __m128 dout = samp;

         smu += freq_factor;

         // fmadd
         dout = _mm_add_ss(dout, _mm_mul_ss(dsamp, tau));

         if (smu >= 1.0)
         {
            samp = nsamp;
            nsamp = _mm_load_ss(sptr++);

            smu -= 1.0;

            dsamp = _mm_sub_ss(nsamp, samp);
         }
         _mm_store_ss(dptr++, dout);
      }
      while (--i);
#else
      float samp, dsamp;
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
#endif
   }
}

// https://en.wikipedia.org/wiki/Cubic_Hermite_spline
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
   assert(d != s);

   if (fact < CUBIC_TRESHOLD) {
      FN(aaxBufResampleCubic_float,A)(d, s, dmin, dmax, smu, fact);
   } else if (fact < 1.0f) {
      FN(aaxBufResampleLinear_float,A)(d, s, dmin, dmax, smu, fact);
   } else if (fact >= 1.0f) {
      FN(aaxBufResampleDecimate_float,A)(d, s, dmin, dmax, smu, fact);
   } else {
//    _aaxBufResampleNearest_float,A)(d, s, dmin, dmax, smu, fact);
      memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}
