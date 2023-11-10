/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>	/* for floorf */


#include "software/rbuf_int.h"
#include "arch2d_simd.h"

#ifdef __SSE2__

void
_aax_init_SSE()
{
   const char *env = getenv("AAX_ENABLE_FTZ");

   if (env && _aax_getbool(env))
   {
// https://www.intel.com/content/www/us/en/docs/cpp-compiler/developer-guide-reference/2021-8/set-the-ftz-and-daz-flags.html
      _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
      _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
   }
}

inline float	// range -1.0f .. 1.0f
fast_sin_sse2(float x)
{
   return -4.0f*(x - x*fabsf(x));
}

static inline FN_PREALIGN float
hsum_ps_sse2(__m128 v) {
   __m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
   __m128 sums = _mm_add_ps(v, shuf);
   shuf = _mm_movehl_ps(shuf, sums);
   sums = _mm_add_ss(sums, shuf);
   return _mm_cvtss_f32(sums);
}

static inline __m128
_mm_abs_ps(__m128 x) {
   return _mm_andnot_ps(_mm_set1_ps(-0.0f), x);
}

static inline int
_mm_testz_ps_sse2(__m128 x)
{
   __m128i zero = _mm_setzero_si128();
   return (_mm_movemask_epi8(_mm_cmpeq_epi32(_mm_castps_si128(x), zero)) != 0xFFFF);
}

static inline __m128	// range -1.0f .. 1.0f
fast_sin4_sse2(__m128 x)
{
   __m128 four = _mm_set1_ps(-4.0f);
   return _mm_mul_ps(four, _mm_sub_ps(x, _mm_mul_ps(x, _mm_abs_ps(x))));
}

#define MUL     (65536.0f*256.0f)
#define IMUL    (1.0f/MUL)

// Use the faster, less accurate algorithm:
//    GMATH_PI_4*x + 0.273f*x * (1.0f-fabsf(x));
//    which equals to:  x*(GMATH_PI_4+0.273f - 0.273f*fabsf(x))
static inline __m128
fast_atan4_sse2(__m128 x)
{
  __m128 offs = _mm_set1_ps(GMATH_PI_4+0.273f);
  __m128 mul = _mm_set1_ps(-0.273f);
  return _mm_mul_ps(x, _mm_add_ps(offs, _mm_mul_ps(mul, _mm_abs_ps(x))));
}

float *
_aax_generate_waveform_sse2(float32_ptr rv, size_t no_samples, float freq, float phase, enum aaxSourceType wtype)
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
         __m128 phase4, freq4, h4;
         __m128 one, two, four;
         __m128 ngain, nfreq;
         __m128 hdt, s;
         int i, h;
         float *ptr;

         assert(MAX_HARMONICS % 4 == 0);

         one = _mm_set1_ps(1.0f);
         two = _mm_set1_ps(2.0f);
         four = _mm_set1_ps(4.0f);

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
            __m128 rv = fast_sin4_sse2(s);

            *ptr++ = hsum_ps_sse2(_mm_mul_ps(ngain, rv));

            s = _mm_add_ps(s, hdt);
            s = _mm_sub_ps(s, _mm_and_ps(two, _mm_cmpge_ps(s, one)));
         }
         while (--i);

         h4 = _mm_add_ps(h4, four);
         for(h=4; h<MAX_HARMONICS; h += 4)
         {
            nfreq = _mm_div_ps(freq4, h4);
            ngain = _mm_and_ps(_mm_cmplt_ps(two, nfreq), _mm_load_ps(harmonics+h));
            if (_mm_testz_ps_sse2(ngain))
            {
               hdt = _mm_div_ps(two, nfreq);

               ptr = rv;
               i = no_samples;
               s = _mm_add_ps(phase4, _mm_load_ps(phases+h));
               s = _mm_sub_ps(s, _mm_and_ps(one, _mm_cmpge_ps(s, one)));
               do
               {
                  __m128 rv = fast_sin4_sse2(s);

                  *ptr++ += hsum_ps_sse2(_mm_mul_ps(ngain, rv));

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

void
_batch_get_average_rms_sse2(const_float32_ptr s, size_t num, float *rms, float *peak)
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
_batch_saturate24_sse2(void *data, size_t num)
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
_batch_cvt24_ps_sse2(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *d = (int32_t*)dst;
   float *s = (float*)src;

   if (((size_t)d & MEMMASK16) != 0 || ((size_t)s & MEMMASK16) != 0)
   {
      float mul = (float)(1<<23);
      size_t i = num;
      do {
         *d++ = (int32_t)(*s++ * mul);
      } while (--i);
      return;
   }

   assert(((size_t)d & MEMMASK16) == 0);
   assert(((size_t)s & MEMMASK16) == 0);

   if (num)
   {
      __m128i *dptr = (__m128i*)d;
      __m128* sptr = (__m128*)s;
      size_t i, step;

      step = 4*sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m128i xmm4i, xmm5i, xmm6i, xmm7i;
         __m128 xmm0, xmm1, xmm2, xmm3;
         __m128 mul = _mm_set1_ps((float)(1<<23));

         num -= i*step;
         d += i*step;
         s += i*step;
         do
         {
            xmm0 = _mm_load_ps((const float*)sptr++);
            xmm1 = _mm_load_ps((const float*)sptr++);
            xmm2 = _mm_load_ps((const float*)sptr++);
            xmm3 = _mm_load_ps((const float*)sptr++);

            xmm0 = _mm_mul_ps(xmm0, mul);
            xmm1 = _mm_mul_ps(xmm1, mul);
            xmm2 = _mm_mul_ps(xmm2, mul);
            xmm3 = _mm_mul_ps(xmm3, mul);

            xmm4i = _mm_cvtps_epi32(xmm0);
            xmm5i = _mm_cvtps_epi32(xmm1);
            xmm6i = _mm_cvtps_epi32(xmm2);
            xmm7i = _mm_cvtps_epi32(xmm3);

            _mm_store_si128(dptr++, xmm4i);
            _mm_store_si128(dptr++, xmm5i);
            _mm_store_si128(dptr++, xmm6i);
            _mm_store_si128(dptr++, xmm7i);
         }
         while(--i);
      }

      if (num)
      {
         float mul = (float)(1<<23);
         i = num;
         do {
            *d++ = (int32_t)(*s++ * mul);
         } while (--i);
      }
   }
}

void
_batch_cvt24_ps24_sse2(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *d = (int32_t*)dst;
   float *s = (float*)src;
   size_t i, step;
   size_t dtmp, stmp;

   if (!num) return;

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if ((dtmp || stmp) && dtmp != stmp)  /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      i = num;
      do {
         *d++ = (int32_t)*s++;
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
         do {
            *d++ = (int32_t)*s++;
         } while(--i);
      }
   }

   if (num)
   {
      __m128i *dptr = (__m128i*)d;
      __m128* sptr = (__m128*)s;

      step = 4*sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m128i xmm4i, xmm5i, xmm6i, xmm7i;
         __m128 xmm0, xmm1, xmm2, xmm3;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xmm0 = _mm_load_ps((const float*)sptr++);
            xmm1 = _mm_load_ps((const float*)sptr++);
            xmm2 = _mm_load_ps((const float*)sptr++);
            xmm3 = _mm_load_ps((const float*)sptr++);

            xmm4i = _mm_cvtps_epi32(xmm0);
            xmm5i = _mm_cvtps_epi32(xmm1);
            xmm6i = _mm_cvtps_epi32(xmm2);
            xmm7i = _mm_cvtps_epi32(xmm3);

            _mm_store_si128(dptr++, xmm4i);
            _mm_store_si128(dptr++, xmm5i);
            _mm_store_si128(dptr++, xmm6i);
            _mm_store_si128(dptr++, xmm7i);
         }
         while(--i);
      }

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
_batch_cvtps_24_sse2(void_ptr dst, const_void_ptr src, size_t num)
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
         __m128 mul = _mm_set1_ps(1.0f/(float)(1<<23));

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
         float mul = 1.0f/(float)(1<<23);
         i = num;
         do {
            *d++ = (float)(*s++) * mul;
         } while (--i);
      }
   }
}

void
_batch_cvtps24_24_sse2(void_ptr dst, const_void_ptr src, size_t num)
{
   int32_t *s = (int32_t*)src;
   float *d = (float*)dst;
   size_t i, step;
   size_t dtmp, stmp;

   assert(s != 0);
   assert(d != 0);

   if (!num) return;

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if ((dtmp || stmp) && dtmp != stmp)  /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      i = num;
      do {
         *d++ = (float)*s++;
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
         do {
            *d++ = (float)*s++;
         } while(--i);
      }
   }

   if (num)
   {
      __m128i* sptr = (__m128i*)s;
      __m128 *dptr = (__m128*)d;

      step = 4*sizeof(__m128i)/sizeof(int32_t);

      i = num/step;
      if (i)
      {
         __m128i xmm0i, xmm1i, xmm2i, xmm3i;
         __m128 xmm4, xmm5, xmm6, xmm7;

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

            _mm_store_ps((float*)dptr++, xmm4);
            _mm_store_ps((float*)dptr++, xmm5);
            _mm_store_ps((float*)dptr++, xmm6);
            _mm_store_ps((float*)dptr++, xmm7);
         }
         while(--i);
      }

      if (num)
      {
         i = num;
         do {
            *d++ = (float)*s++;
         } while (--i);
      }
   }
}

void _batch_atanps_sse2(void_ptr dptr, const_void_ptr sptr, size_t num)
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
         __m128 xmin = _mm_set1_ps(-1.94139795f);
         __m128 xmax = _mm_set1_ps(1.94139795f);
         __m128 mul = _mm_set1_ps(MUL*GMATH_1_PI_2);
         __m128 imul = _mm_set1_ps(IMUL);
         __m128 xmm0, xmm1;

         num -= i*step;
         s += i*step;
         d += i*step;
         do
         {
            xmm0 = _mm_load_ps((const float*)sptr++);

            xmm0 = _mm_mul_ps(xmm0, imul);
            xmm0 = _mm_min_ps(_mm_max_ps(xmm0, xmin), xmax);
            xmm1 = _mm_mul_ps(mul, fast_atan4_sse2(xmm0));

            _mm_store_ps((float*)dptr++, xmm1);
         }
         while(--i);
      }

      if (num) {
         _batch_atanps_cpu(d, s, num);
      }
   }
}

void
_batch_roundps_sse2(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   float *d = (float*)dptr;
   float *s = (float*)sptr;
   size_t i, step;
   size_t dtmp, stmp;

   if (!num) return;

   dtmp = (size_t)d & MEMMASK16;
   stmp = (size_t)s & MEMMASK16;
   if ((dtmp || stmp) && dtmp != stmp)  /* improperly aligned,            */
   {                                    /* let the compiler figure it out */
      i = num;
      do {
         *d++ = (float)(int32_t)*s++;
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
         do {
            *d++ = (int32_t)*s++;
         } while(--i);
      }
   }

   if (num)
   {
      __m128 *dptr = (__m128*)d;
      __m128* sptr = (__m128*)s;

      step = 4*sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m128i xmm4i, xmm5i, xmm6i, xmm7i;
         __m128 xmm0, xmm1, xmm2, xmm3;

         num -= i*step;;
         s += i*step;
         d += i*step;
         do
         {
            xmm0 = _mm_load_ps((const float*)sptr++);
            xmm1 = _mm_load_ps((const float*)sptr++);
            xmm2 = _mm_load_ps((const float*)sptr++);
            xmm3 = _mm_load_ps((const float*)sptr++);

            xmm4i = _mm_cvtps_epi32(xmm0);
            xmm5i = _mm_cvtps_epi32(xmm1);
            xmm6i = _mm_cvtps_epi32(xmm2);
            xmm7i = _mm_cvtps_epi32(xmm3);

            xmm0 = _mm_cvtepi32_ps(xmm4i);
            xmm1 = _mm_cvtepi32_ps(xmm5i);
            xmm2 = _mm_cvtepi32_ps(xmm6i);
            xmm3 = _mm_cvtepi32_ps(xmm7i);

            _mm_store_ps((float*)dptr++, xmm0);
            _mm_store_ps((float*)dptr++, xmm1);
            _mm_store_ps((float*)dptr++, xmm2);
            _mm_store_ps((float*)dptr++, xmm3);
         }
         while(--i);
      }

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
_batch_iadd_sse2(int32_ptr dst, const_int32_ptr src, size_t num)
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
_batch_imadd_sse2(int32_ptr dst, const_int32_ptr src, size_t num, float v, float vstep)
{
   int32_ptr d = (int32_ptr)dst;
   int32_ptr s = (int32_ptr)src;
   size_t i, step, dtmp, stmp;

   if (!num || (v <= LEVEL_90DB && vstep <= LEVEL_90DB)) return;
   if (fabsf(v - 1.0f) < LEVEL_90DB && vstep <=  LEVEL_90DB) {
      _batch_iadd_sse2(dst, src, num);
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
_batch_fadd_sse2(float32_ptr dst, const_float32_ptr src, size_t num)
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
_batch_fmul_sse2(void_ptr dptr, const_void_ptr sptr, size_t num)
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
_batch_fmul_value_sse2(void_ptr dptr, const_void_ptr sptr, unsigned bps, size_t num, float f)
{
   if (!num) return;

   if (fabsf(f - 1.0f) < LEVEL_96DB)
   {
      if (sptr != dptr) memcpy(dptr, sptr,  num*bps);
      return;
   }

   if (fabsf(f) <= LEVEL_96DB)
   {
      memset(dptr, 0, num*bps);
      return;
   }

   if (bps == 4)
   {
      const_float32_ptr s = (float32_ptr)sptr;
      float32_ptr d = (float32_ptr)dptr;
      size_t i, step, dtmp, stmp;

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
               *d++ = *s++ * f;
            } while(--i);
         }
      }

      step = 3*sizeof(__m128)/sizeof(float);

      i = num/step;
      if (i)
      {
         __m128* sptr = (__m128*)s;
         __m128* dptr = (__m128*)d;
         __m128 tv = _mm_set1_ps(f);
         __m128 xmm0, xmm1, xmm2;

         num -= i*step;
         s += i*step;
         d += i*step;
         if (stmp)
         {
            do
            {
               xmm0 = _mm_mul_ps(tv, _mm_loadu_ps((const float*)(sptr++)));
               xmm1 = _mm_mul_ps(tv, _mm_loadu_ps((const float*)(sptr++)));
               xmm2 = _mm_mul_ps(tv, _mm_loadu_ps((const float*)(sptr++)));

               _mm_store_ps((float*)dptr++, xmm0);
               _mm_store_ps((float*)dptr++, xmm1);
               _mm_store_ps((float*)dptr++, xmm2);
            }
            while(--i);
         }
         else
         {
            do
            {
               xmm0 = _mm_mul_ps(tv, _mm_load_ps((const float*)(sptr++)));
               xmm1 = _mm_mul_ps(tv, _mm_load_ps((const float*)(sptr++)));
               xmm2 = _mm_mul_ps(tv, _mm_load_ps((const float*)(sptr++)));

               _mm_store_ps((float*)dptr++, xmm0);
               _mm_store_ps((float*)dptr++, xmm1);
               _mm_store_ps((float*)dptr++, xmm2);
            }
            while(--i);
         }
      }

      if (num)
      {
         i = num;
         do {
            *d++ = *s++ * f;
         } while(--i);
      }
   }
   else if (bps == 8)
   {
      const_double64_ptr s = (double64_ptr)sptr;
      double64_ptr d = (double64_ptr)dptr;
      size_t i, step, dtmp, stmp;

      stmp = (size_t)s & MEMMASK16;
      dtmp = (size_t)d & MEMMASK16;
      if (dtmp && num)
      {
         i = (MEMALIGN16 - dtmp)/sizeof(double);
         if (i <= num)
         {
            num -= i;
            do {
               *d++ = *s++ * f;
            } while(--i);
         }
      }

      step = sizeof(__m128d)/sizeof(double);

      i = num/step;
      if (i)
      {
         __m128d* sptr = (__m128d*)s;
         __m128d* dptr = (__m128d*)d;
         __m128d tv = _mm_set1_pd(f);
         __m128d xmm0;

         num -= i*step;
         s += i*step;
         d += i*step;
         if (stmp)
         {
            do
            {
               xmm0 = _mm_mul_pd(tv, _mm_loadu_pd((const double*)(sptr++)));
               _mm_store_pd((double*)dptr++, xmm0);
            }
            while(--i);
         }
         else
         {
            do
            {
               xmm0 = _mm_mul_pd(tv, _mm_load_pd((const double*)(sptr++)));
               _mm_store_pd((double*)dptr++, xmm0);
            }
            while(--i);
         }
      }

      if (num)
      {
         i = num;
         do {
            *d++ = *s++ * f;
         } while(--i);
      }
   }
   else {
      _batch_fmul_value_cpu(dptr, sptr, bps, num, f);
   }
}


void
_batch_fmadd_sse2(float32_ptr dst, const_float32_ptr src, size_t num, float v, float vstep)
{
   int need_step = (fabsf(vstep) <= LEVEL_90DB) ? 0 : 1;
   float32_ptr s = (float32_ptr)src;
   float32_ptr d = (float32_ptr)dst;
   size_t i;

   // nothing to do
   if (!num || (fabsf(v) <= LEVEL_96DB && !need_step)) return;

   // volume ~= 1.0f and no change requested: just add both buffers
   if (fabsf(v - 1.0f) < LEVEL_90DB && !need_step) {
      _batch_fadd_sse2(dst, src, num);
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
_batch_cvt24_16_sse2(void_ptr dst, const_void_ptr src, size_t num)
{
   int16_t *s = (int16_t *)src;
   int32_t *d = (int32_t*)dst;
   size_t i, step;
   size_t tmp;

   if (!num) return;

   /*
    * work towards 16-byte aligned d
    */
   tmp = (size_t)d & MEMMASK16;
   if (tmp && num)
   {
      i = (MEMALIGN16 - tmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ = *s++ << 8;
         } while(--i);
      }
   }

   step = 2*sizeof(__m128i)/sizeof(int16_t);

   i = num/step;
   if (i)
   {
      __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i zero = _mm_setzero_si128();
      __m128i *dptr = (__m128i *)d;
      __m128i *sptr = (__m128i *)s;

      tmp = (size_t)s & MEMMASK16;
      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         if (tmp) {
            xmm0 = _mm_loadu_si128(sptr++);
            xmm4 = _mm_loadu_si128(sptr++);
         } else {
            xmm0 = _mm_load_si128(sptr++);
            xmm4 = _mm_load_si128(sptr++);
         }

         xmm1 = _mm_unpacklo_epi16(zero, xmm0);
         xmm3 = _mm_unpackhi_epi16(zero, xmm0);
         xmm5 = _mm_unpacklo_epi16(zero, xmm4);
         xmm7 = _mm_unpackhi_epi16(zero, xmm4);

         xmm0 = _mm_srai_epi32(xmm1, 8);
         xmm2 = _mm_srai_epi32(xmm3, 8);
         xmm4 = _mm_srai_epi32(xmm5, 8);
         xmm6 = _mm_srai_epi32(xmm7, 8);

         _mm_store_si128(dptr++, xmm0);
         _mm_store_si128(dptr++, xmm2);
         _mm_store_si128(dptr++, xmm4);
         _mm_store_si128(dptr++, xmm6);
      }
      while (--i);
   }

   if (num)
   {
      i = num;
      do {
         *d++ = *s++ << 8;
      } while (--i);
   }
}

void
_batch_cvt16_24_sse2(void_ptr dst, const_void_ptr src, size_t num)
{
   size_t i, step;
   int32_t* s = (int32_t*)src;
   int16_t* d = (int16_t*)dst;
   size_t tmp;

   if (!num) return;

   _batch_dither_cpu(s, 2, num);

   /*
    * work towards 16-byte aligned sptr
    */
   tmp = (size_t)s & MEMMASK16;
   if (tmp && num)
   {
      i = (MEMALIGN16 - tmp)/sizeof(int32_t);
      if (i <= num)
      {
         num -= i;
         do {
            *d++ = *s++ >> 8;
         } while(--i);
      }
   }

   assert(((size_t)s & MEMMASK16) == 0);
   tmp = (size_t)d & MEMMASK16;

   step = 4*sizeof(__m128i)/sizeof(int32_t);

   i = num/step;
   if (i)
   {
      __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i *sptr = (__m128i *)s;
      __m128i *dptr = (__m128i *)d;

      num -= i*step;
      s += i*step;
      d += i*step;
      do
      {
         xmm0 = _mm_load_si128(sptr++);
         xmm1 = _mm_load_si128(sptr++);
         xmm4 = _mm_load_si128(sptr++);
         xmm5 = _mm_load_si128(sptr++);

         xmm2 = _mm_srai_epi32(xmm0, 8);
         xmm3 = _mm_srai_epi32(xmm1, 8);
         xmm6 = _mm_srai_epi32(xmm4, 8);
         xmm7 = _mm_srai_epi32(xmm5, 8);

         xmm4 = _mm_packs_epi32(xmm2, xmm3);
         xmm6 = _mm_packs_epi32(xmm6, xmm7);

         if (tmp) {
            _mm_storeu_si128(dptr++, xmm4);
            _mm_storeu_si128(dptr++, xmm6);
         } else {
            _mm_store_si128(dptr++, xmm4);
            _mm_store_si128(dptr++, xmm6);
         }
      }
      while (--i);
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
_batch_cvt16_intl_24_sse2(void_ptr dst, const_int32_ptrptr src,
                                size_t offset, unsigned int tracks,
                                size_t num)
{
   size_t i, step;
   int16_t *d = (int16_t*)dst;
   int32_t *s1, *s2;
   size_t t, tmp;

   if (!num) return;

   for (t=0; t<tracks; t++)
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

   step = 2*sizeof(__m128i)/sizeof(int32_t);

   /*
    * work towards 16-byte aligned sptr
    */
   tmp = (size_t)s1 & MEMMASK16;
   assert(tmp == ((size_t)s2 & MEMMASK16));

   i = num/step;
   if (tmp && i)
   {
      i = (MEMALIGN16 - tmp)/sizeof(int32_t);
      num -= i;
      do
      {
         *d++ = *s1++ >> 8;
         *d++ = *s2++ >> 8;
      }
      while (--i);
   }

   i = num/step;
   if (i)
   {
      __m128i mask, xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
      __m128i *sptr1 = (__m128i*)s1;
      __m128i *sptr2 = (__m128i*)s2;
      __m128i *dptr = (__m128i*)d;

      mask = _mm_set_epi32(0x00FFFF00, 0x00FFFF00, 0x00FFFF00, 0x00FFFF00);
      tmp = (size_t)d & MEMMASK16;

      num -= i*step;
      s1 += i*step;
      s2 += i*step;
      d += 2*i*step;
      do
      {
         xmm0 = _mm_load_si128(sptr1++);
         xmm4 = _mm_load_si128(sptr1++);
         xmm1 = _mm_load_si128(sptr2++);
         xmm5 = _mm_load_si128(sptr2++);

         xmm2 = _mm_and_si128(xmm0, mask);
         xmm3 = _mm_and_si128(xmm1, mask);
         xmm6 = _mm_and_si128(xmm4, mask);
         xmm7 = _mm_and_si128(xmm5, mask);

         xmm0 = _mm_srli_epi32(xmm2, 8);
         xmm1 = _mm_slli_epi32(xmm3, 8);
         xmm4 = _mm_srli_epi32(xmm6, 8);
         xmm5 = _mm_slli_epi32(xmm7, 8);

         xmm0 = _mm_or_si128(xmm1, xmm0);
         xmm4 = _mm_or_si128(xmm5, xmm4);

         if (tmp) {
            _mm_storeu_si128(dptr++, xmm0);
            _mm_storeu_si128(dptr++, xmm4);
         } else {
            _mm_store_si128(dptr++, xmm0);
            _mm_store_si128(dptr++, xmm4);
         }
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
_batch_ema_iir_float_sse2(float32_ptr d, const_float32_ptr sptr, size_t num, float *hist, float a1)
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
_batch_freqfilter_sse2(int32_ptr dptr, const_int32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   const_int32_ptr s = sptr;

   if (num)
   {
      __m128 c, h, mk;
      float *cptr, *hist;
      int stages;

      cptr = filter->coeff;
      hist = filter->freqfilter->history[t];
      stages = filter->no_stages;
      if (!stages) stages++;

      if (filter->state == AAX_BESSEL) {
         mk = _mm_set_ss(filter->k * (filter->high_gain - filter->low_gain));
      } else {
         mk = _mm_set_ss(filter->k * filter->high_gain);
      }

      do
      {
         int32_ptr d = dptr;
         size_t i = num;

//       c = _mm_set_ps(cptr[3], cptr[1], cptr[2], cptr[0]);
         c = _mm_load_ps(cptr);
         c = _mm_shuffle_ps(c, c, _MM_SHUFFLE(3,1,2,0));

//       h = _mm_set_ps(hist[1], hist[1], hist[0], hist[0]);
         h = _mm_loadl_pi(_mm_setzero_ps(), (__m64*)hist);
         h = _mm_shuffle_ps(h, h, _MM_SHUFFLE(1,1,0,0));

         do
         {
            __m128 pz, smp, nsmp, tmp;

            smp = _mm_cvtepi32_ps((__m128i)_mm_load_ss((const float*)s));

            // pz = { c[3]*h1, -c[1]*h1, c[2]*h0, -c[0]*h0 };
            pz = _mm_mul_ps(c, h); // poles and zeros

            // smp = *s++ * k;
            smp = _mm_mul_ss(smp, mk);

            // tmp[0] = -c[0]*h0 + -c[1]*h1;
            tmp = _mm_add_ps(pz, _mm_shuffle_ps(pz, pz, _MM_SHUFFLE(1,3,0,2)));
            s++;

            // nsmp = smp - h0*c[0] - h1*c[1];
            nsmp = _mm_add_ss(smp, tmp);

            // h1 = h0, h0 = smp: h = { h0, h0, smp, smp };
            h = _mm_shuffle_ps(nsmp, h, _MM_SHUFFLE(0,0,0,0));

            // tmp[0] = -c[0]*h0 + -c[1]*h1 + c[2]*h0 + c[3]*h1;
            tmp = _mm_add_ps(tmp, _mm_shuffle_ps(tmp, tmp, _MM_SHUFFLE(0,1,2,3)));

            // smp = smp - h0*c[0] - h1*c[1] + h0*c[2] + h1*c[3];
            smp = _mm_add_ss(smp, tmp);
            _mm_store_ss((float*)d++, (__m128)_mm_cvtps_epi32(smp));
         }
         while (--i);

         _mm_storel_pi((__m64*)hist, h);

         hist += 2;
         cptr += 4;
         mk = _mm_set_ss(1.0f);
         s = dptr;
      }
      while (--stages);
   }
}

void
_batch_freqfilter_float_sse2(float32_ptr dptr, const_float32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   const_float32_ptr s = sptr;

   if (num)
   {
      float k, *cptr, *hist;
      float c0, c1;
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

      c0 = cptr[0];
      c1 = cptr[1];

      h0 = hist[0];
      h1 = hist[1];

      if (filter->state == AAX_BUTTERWORTH)
      {
         float32_ptr d = dptr;
         int i = num/3;
         int rest = num-i*3;

         if (i)
         {
            do
            {
               float nsmp = (*s++ * k) + h0 * cptr[0] + h1 * cptr[1];
               *d++ = nsmp             + h0 * cptr[2] + h1 * cptr[3];

               h1 = h0;
               h0 = nsmp;

               nsmp = (*s++ * k) + h0 * cptr[0] + h1 * cptr[1];
              *d++ = nsmp        + h0 * cptr[2] + h1 * cptr[3];

               h1 = h0;
               h0 = nsmp;

               nsmp = (*s++ * k) + h0 * cptr[0] + h1 * cptr[1];
               *d++ = nsmp       + h0 * cptr[2] + h1 * cptr[3];

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
      }
      else
      {
         float32_ptr d = dptr;
         int i = num/4;
         int rest = num-i*4;

         do
         {
            float smp = (*s++ * k) + ((h0 * c0) + (h1 * c1));
            *d++ = smp;

            h1 = h0;
            h0 = smp;

            smp = (*s++ * k) + ((h0 * c0) + (h1 * c1));
            *d++ = smp;

            h1 = h0;
            h0 = smp;

            smp = (*s++ * k) + ((h0 * c0) + (h1 * c1));
            *d++ = smp;

            h1 = h0;
            h0 = smp;

            smp = (*s++ * k) + ((h0 * c0) + (h1 * c1));
            *d++ = smp;

            h1 = h0;
            h0 = smp;
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
            int i = num/3;
            int rest = num-i*3;

            if (i)
            {
               do
               {
                  float nsmp = *d + h0 * cptr[0] + h1 * cptr[1];
                  *d++ = nsmp     + h0 * cptr[2] + h1 * cptr[3];

                  h1 = h0;
                  h0 = nsmp;

                  nsmp = *d    + h0 * cptr[0] + h1 * cptr[1];
                  *d++ = nsmp  + h0 * cptr[2] + h1 * cptr[3];

                  h1 = h0;
                  h0 = nsmp;

                  nsmp = *d    + h0 * cptr[0] + h1 * cptr[1];
                  *d++ = nsmp  + h0 * cptr[2] + h1 * cptr[3];

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
         }
         else
         {
            float32_ptr d = dptr;
            int i = num/4;
            int rest = num-i*4;

            do
            {
               float smp = *d  + ((h0 * c0) + (h1 * c1));
               *d++ = smp;

               h1 = h0;
               h0 = smp;

               smp = *d  + ((h0 * c0) + (h1 * c1));
               *d++ = smp;

               h1 = h0;
               h0 = smp;

               smp = *d  + ((h0 * c0) + (h1 * c1));
               *d++ = smp;

               h1 = h0;
               h0 = smp;

               smp = *d  + ((h0 * c0) + (h1 * c1));
               *d++ = smp;

               h1 = h0;
               h0 = smp;
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
   }
}


static inline void
_aaxBufResampleDecimate_float_sse2(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_aaxBufResampleNearest_float_sse2(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_aaxBufResampleLinear_float_sse2(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_aaxBufResampleCubic_float_sse2(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float freq_factor)
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
_batch_resample_float_sse2(float32_ptr d, const_float32_ptr s, size_t dmin, size_t dmax, float smu, float fact)
{
   assert(fact > 0.0f);
   assert(d != s);

   if (fact < CUBIC_TRESHOLD) {
      _aaxBufResampleCubic_float_sse2(d, s, dmin, dmax, smu, fact);
   }
   else if (fact < 1.0f) {
      _aaxBufResampleLinear_float_sse2(d, s, dmin, dmax, smu, fact);
   }
   else if (fact >= 1.0f) {
      _aaxBufResampleDecimate_float_sse2(d, s, dmin, dmax, smu, fact);
   } else {
//    _aaxBufResampleNearest_float_sse2(d, s, dmin, dmax, smu, fact);
      memcpy(d+dmin, s, (dmax-dmin)*sizeof(MIX_T));
   }
}

#else
typedef int make_iso_compilers_happy;
#endif /* SSE2 */

