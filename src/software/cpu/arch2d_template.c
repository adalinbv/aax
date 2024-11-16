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

#include <math.h>	/* rinft */

#include <base/random.h>
#include <dsp/common.h>
#include <software/rbuf_int.h>
#include "waveforms.h"
#include "arch2d_simd.h"

#define __FN(NAME,ARCH)	_##NAME##_##ARCH
#define FN(NAME,ARCH)	__FN(NAME,ARCH)

/**
 * Generate a waveform based on the harminics list
 * output range is -1.0 .. 1.0
 */

float	// range -1.0f .. 1.0f
FN(fast_sin,A)(float x)
{
   return -4.0f*(x - x*fabsf(x));
}

float *
FN(aax_generate_waveform,A)(float32_ptr rv, size_t no_samples, float freq, float phase, enum aaxSourceType wtype)
{
   const_float32_ptr harmonics = _harmonics[wtype-AAX_1ST_WAVE];
   if (rv)
   {
      float ngain = harmonics[0];
      float hdt = 2.0f/freq;
      float s = -1.0f + phase/GMATH_PI;
      int h, i = no_samples;
      float *ptr = rv;

      // first harmonic
      do
      {
         *ptr++ = ngain * FN(fast_sin,A)(s);
         s = s+hdt;
         if (s >= 1.0f) s -= 2.0f;
      }
      while (--i);

      // remaining harmonics, if required
      if (wtype != AAX_SINE)
      {
          for(h=1; h<MAX_HARMONICS; ++h)
          {
             float nfreq = freq/(h+1);
             if (nfreq < 2.0f) break;    // higher than the nyquist-frequency

             ngain = harmonics[h];
             if (ngain)
             {
                int i = no_samples;
                float hdt = 2.0f/nfreq;
                float s = -1.0f + phase/GMATH_PI;

                ptr = rv;
                do
                {
                   *ptr++ += ngain * FN(fast_sin,A)(s);
                   s = s+hdt;
                   if (s >= 1.0f) s -= 2.0f;
                }
                while (--i);
            }
         }
      }
   }
   return rv;
}

#define FC	50.0f // 50Hz high-pass EMA filter cutoff frequency
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
FN(batch_imadd,A)(int32_ptr dptr, const_int32_ptr sptr, size_t num, float v, float vstep)
{
   int32_t* s = (int32_t* )sptr;
   int32_t* d = dptr;
   size_t i = num;

   if (!num || (v <= LEVEL_128DB && vstep <= LEVEL_128DB)) return;

   /* f == 1.0f && step = 0.0f */
   if (fabsf(v - 1.0f) < LEVEL_96DB && vstep <=  LEVEL_96DB)
   {
      do {
         *d++ += *s++;
      }
      while (--i);
   }
   else
   {
      int32_t f = (int32_t)(v*1024.0f);
      do {
         *d++ += ((*s++ >> 2) * f) >> 8;
      }
      while (--i);
   }
}

#if 0
void
FN(batch_hmadd,A)(float32_ptr dptr, const_float16_ptr sptr, size_t num, float v, float vstep)
{
   int16_t *s = (int16_t*)sptr;
   float *d = dptr;
   size_t i = num;

   if (!num || (v <= LEVEL_128DB && vstep <= LEVEL_128DB)) return;

   /* v == 1.0f && step = 0.0f */
   if (fabsf(v - 1.0f) < LEVEL_96DB && vstep <=  LEVEL_96DB)
   {
      do {
         *d++ += HALF2FLOAT(*s);
         s++;
      }
      while (--i);
   }
   else
   {
      do {
         *d++ += HALF2FLOAT(*s) * v;
         v += vstep;
         s++;
      }
      while (--i);
   }
}
#endif

void
FN(batch_fmadd,A)(float32_ptr dptr, const_float32_ptr sptr, size_t num, float v, float vstep)
{
   int need_step = (fabsf(vstep) <= LEVEL_90DB) ? 0 : 1;
   float32_ptr s = (float*)sptr;
   float32_ptr d = dptr;
   size_t i = num;

   if (!num || (fabsf(v) <= LEVEL_90DB && !need_step)) return;

   if (fabsf(v - 1.0f) < LEVEL_90DB && !need_step)
   {
      do {
         *d++ += *s++;
      }
      while (--i);
   }
   else if (need_step)
   {
      do {
         *d++ += (*s++ * v);
         v += vstep;
      }
      while (--i);
   }
   else
   {
      do {
         *d++ += (*s++ * v);
      }
      while (--i);
   }
}

void
FN(batch_imul_value,A)(void* dptr, const void* sptr, unsigned bps, size_t num, float f)
{
   size_t i = num;
   if (num)
   {
      switch (bps)
      {
      case 1:
      {
         int8_t* s = (int8_t*)sptr;
         int8_t* d = (int8_t*)dptr;
         do {
            *d++ = *s++ * f;
         }
         while (--i);
         break;
      }
      case 2:
      {
         int16_t* s = (int16_t*)sptr;
         int16_t* d = (int16_t*)dptr;
         do {
            *d++ = *s++ * f;
         }
         while (--i);
         break;
      }
      case 4:
      {
         int32_t* s = (int32_t*)sptr;
         int32_t* d = (int32_t*)dptr;
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
FN(batch_fmul_value,A)(float32_ptr dptr, const_float32_ptr sptr, size_t num, float numerator, float denomerator)
{
   float *s = (float*)sptr;
   float *d = (float*)dptr;
   float f = numerator/denomerator;
   size_t i = num;

   if (!num || (fabsf(f) <= LEVEL_90DB)) return;

   if (fabsf(f - 1.0f) < LEVEL_96DB)
   {
      if (sptr != dptr) memcpy(dptr, sptr,  num*sizeof(float));
      return;
   }

   if (f <= LEVEL_96DB)
   {
      memset(dptr, 0, num*sizeof(float));
      return;
   }

   do {
      *d++ = (*s++ * f);
   }
   while (--i);
}

void
FN(batch_fmul,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      size_t i = num;
      float *s = (float*)sptr;
      float *d = (float*)dptr;
      do {
         *d++ *= *s++;
      }
      while (--i);
   }
}

void
FN(batch_cvt24_24,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num && dptr != sptr) {
      memcpy(dptr, sptr, num*sizeof(int32_t));
   }
}

void
FN(batch_cvt24_32,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      int32_t *s = (int32_t *)sptr;
      int32_t *d = dptr;
      size_t i = num;

      do {
         *d++ = *s++ >> 8;
      }
      while (--i);
   }
}

void
FN(batch_cvt32_24,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      int32_t *d = (int32_t *)dptr;
      int32_t *s = (int32_t *)sptr;
      size_t i = num;

      do {
         *d++ = *s++ << 8;
      }
      while (--i);
   }
}

void
FN(batch_cvt24_ps24,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
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
FN(batch_cvtps24_24,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
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

#define MUL	(65536.0f*256.0f)
#define IMUL	(1.0f/MUL)

// range: 0.0 .. 1.0
// slower, more accurate:
//     GMATH_PI_4*x - x*(fabsf(x) - 1)*(0.2447 + 0.0663*fabsf(x));
// twice as fast, twice less-accurate:
//     GMATH_PI_4*x + 0.273f*x * (1.0f-fabsf(x));
//
// Use the faster, less accurate algorithm for CPU's:
static inline float fast_atanf(float x) {
   return x*((GMATH_PI_4+0.273f) - 0.273f*fabsf(x));
}

void
FN(batch_atanps,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      float* d = (float*)dptr;
      float* s = (float*)sptr;
      size_t i = num;

      do {
         float samp = *s++ * IMUL;
         samp = _MINMAX(samp, -1.94139795f, 1.94139795f);
         *d++ = fast_atanf(samp)*(MUL*GMATH_1_PI_2);
      } while (--i);
   }
}

void
FN(batch_atan,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      float* d = (float*)dptr;
      float* s = (float*)sptr;
      size_t i = num;

      do {
         *d++ = atanf(*s++ * IMUL)*(MUL*GMATH_1_PI_2);
      } while (--i);
   }
}

void
FN(batch_roundps,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   _batch_cvt24_ps24(dptr, sptr, num);
   _batch_cvtps24_24(dptr, dptr, num);
}

#if 0
void
FN(batch_cvt24_ph,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      static const float mul = (float)(1<<23);
      int32_t* d = (int32_t*)dptr;
      int16_t* s = (int16_t*)sptr;
      size_t i = num;

      do {
         *d++ = (int32_t)(HALF2FLOAT(*s) * mul);
         s++;
      } while (--i);
   }
}
#endif

void
FN(batch_cvt24_ps,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
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

#if 0
void
FN(batch_cvtph_24,A)(void_ptr dst, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      static const float mul = 1.0f/(float)(1<<23);
      int32_t* s = (int32_t*)sptr;
      int16_t* d = (int16_t*)dst;
      size_t i = num;

      do {
         *d++ = FLOAT2HALF((float)*s++ * mul);
      } while (--i);
   }
}
#endif

void
FN(batch_cvtps_24,A)(void_ptr dst, const_void_ptr sptr, size_t num)
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
FN(batch_cvt24_pd,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
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
FN(batch_cvt24_8_intl,A)(int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      if (tracks == 1) {
         _batch_cvt24_8(dptr[0]+offset, sptr, num);
      }
      else if (tracks)
      {
         size_t t;
         for (t=0; t<tracks; t++)
         {
            int8_t *s = (int8_t *)sptr + t;
            int32_t *d = dptr[t] + offset;
            size_t i = num;

            do
            {
               *d++ = ((int32_t)*s + 128) << 16;
               s += tracks;
            }
            while (--i);
         }
      }
   }
}

void
FN(batch_cvt24_16_intl,A)(int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      if (tracks == 1) {
         _batch_cvt24_16(dptr[0]+offset, sptr, num);
      }
      else if (tracks)
      {
         size_t t;
         for (t=0; t<tracks; t++)
         {
            int16_t *s = (int16_t *)sptr + t;
            int32_t *d = dptr[t] + offset;
            size_t i = num;

            do
            {
               *d++ = (int32_t)*s << 8;
               s += tracks;
            }
            while (--i);
         }
      }
   }
}

void
FN(batch_cvt24_24_intl,A)(int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      if (tracks == 1) {
         _batch_cvt24_24(dptr[0]+offset, sptr, num);
      }
      else if (tracks)
      {
         size_t t;
         for (t=0; t<tracks; t++)
         {
            int32_t *s = (int32_t *)sptr + t;
            int32_t *d = dptr[t] + offset;
            size_t i = num;

            do {
               *d++ = *s;
               s += tracks;
            }
            while (--i);
         }
      }
   }
}

void
FN(batch_cvt24_24_3intl,A)(int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      if (tracks == 1) {
         _batch_cvt24_24_3(dptr[0]+offset, sptr, num);
      }
      else if (tracks)
      {
         size_t t;
         for (t=0; t<tracks; t++)
         {
            uint8_t *s = (uint8_t*)sptr + 3*t;
            int32_t smp, *d = dptr[t] + offset;
            size_t i = num;

            do {
               smp = *s++;
               smp |= (*s++ << 8);
               smp |= (*s++ << 16);
               if ((smp & 0x00800000) > 0) smp |= 0xFF000000;

               *d++ = smp;
               s += 3*(tracks-1);
            }
            while (--i);
         }
      }
   }
}

void
FN(batch_cvt24_32_intl,A)(int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      if (tracks == 1) {
         _batch_cvt24_32(dptr[0]+offset, sptr, num);
      }
      else if (tracks)
      {
         size_t t;
         for (t=0; t<tracks; t++)
         {
            int32_t *s = (int32_t *)sptr + t;
            int32_t *d = dptr[t] + offset;
            size_t i = num;

            do {
               *d++ = *s >> 8;
               s += tracks;
            }
            while (--i);
         }
      }
   }
}

#if 0
void
FN(batch_cvt24_ph_intl,A)(int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      if ((tracks == 1) &&
          ((size_t)(dptr[0]+offset) & MEMMASK) == 0 &&
          ((size_t)sptr & MEMMASK) == 0)
      {
         _batch_cvt24_ph(dptr[0]+offset, sptr, num);
      }
      else if (tracks)
      {
         static const float mul = (float)(1<<23);
         size_t t;

         for (t=0; t<tracks; t++)
         {
            int16_t *s = (int16_t*)sptr + t;
            int32_t *d = dptr[t] + offset;
            size_t i = num;

            do {
               *d++ = (int32_t)(HALF2FLOAT(*s) * mul);
               s += tracks;
            }
            while (--i);
         }
      }
   }
}
#endif

void
FN(batch_cvt24_ps_intl,A)(int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      if ((tracks == 1) &&
          ((size_t)(dptr[0]+offset) & MEMMASK) == 0 &&
          ((size_t)sptr & MEMMASK) == 0)
      {
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
FN(batch_cvt24_pd_intl,A)(int32_ptrptr dptr, const_void_ptr sptr, size_t offset, unsigned int tracks, size_t num)
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
FN(batch_cvtpd_24,A)(void_ptr dst, const_void_ptr sptr, size_t num)
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
FN(batch_cvt24_8,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
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
FN(batch_cvt24_16,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      int16_t* s = (int16_t*)sptr;
      int32_t* d = dptr;
      size_t i = num;

      do {
         *d++ = (int32_t)*s++ << 8;
      }
      while (--i);
   }
}

void
FN(batch_cvt24_24_3,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      uint8_t *s = (uint8_t *)sptr;
      int32_t smp, *d = dptr;
      size_t i = num;

      do
      {
         smp = ((int32_t)*s++);
         smp |= ((int32_t)*s++) << 8;
         smp |= ((int32_t)*s++) << 16;
         if ((smp & 0x00800000) > 0) smp |= 0xFF000000;

         *d++ = smp;
      }
      while (--i);
   }
}


void
FN(batch_cvt8_24,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      int32_t* s = (int32_t*)sptr;
      int8_t* d = (int8_t*)dptr;
      size_t i = num;

      FN(batch_dither,A)(s, 1, num);
      do {
         *d++ = *s++ >> 16;
      }
      while (--i);
   }
}

void
FN(batch_cvt16_24,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      int32_t* s = (int32_t*)sptr;
      int16_t* d = (int16_t*)dptr;
      size_t i = num;

      FN(batch_dither,A)(s, 2, num);
      do {
         *d++ = *s++ >> 8;
      }
      while (--i);
   }
}

void
FN(batch_cvt24_3_24,A)(void_ptr dptr, const_void_ptr sptr, size_t num)
{
   if (num)
   {
      uint8_t *d = (uint8_t *)dptr;
      int32_t *s = (int32_t*)sptr;
      size_t i = num;

      do
      {
         *d++ = (uint8_t)(*s & 0xFF);
         *d++ = (uint8_t)((*s >> 8) & 0xFF);
         *d++ = (uint8_t)((*s++ >> 16) & 0xFF);
      }
      while (--i);
   }
}

void
FN(batch_cvt8_intl_24,A)(void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      size_t t;

      for (t=0; t<tracks; t++)
      {
         int32_t *s = (int32_t *)sptr[t] + offset;
         int8_t *d = (int8_t *)dptr + t;
         size_t i = num;

         FN(batch_dither,A)(s, 1, num);
         do
         {
            *d = (*s++ >> 16) - 128;
            d += tracks;
         }
         while (--i);
      }
   }
}

void
FN(batch_cvt16_intl_24,A)(void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      size_t t;
      for (t=0; t<tracks; t++)
      {
         int32_t *s = (int32_t *)sptr[t] + offset;
         int16_t *d = (int16_t *)dptr + t;
         size_t i = num;

         FN(batch_dither,A)(s, 2, num);
         do
         {
            *d = *s++ >> 8;
            d += tracks;
         }
         while (--i);
      }
   }
}

void
FN(batch_cvt24_3intl_24,A)(void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      size_t t;

      for (t=0; t<tracks; t++)
      {
         int32_t *s = (int32_t *)sptr[t] + offset;
         int8_t *d = (int8_t *)dptr + 3*t;
         size_t i = num;

         do
         {
            *d++ = *s & 0xFF;
            *d++ = (*s >> 8) & 0xFF;
            *d++ = (*s++ >> 16) & 0xFF;
         }
         while (--i);
      }
   }
}

void
FN(batch_cvt24_intl_24,A)(void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      size_t t;

      for (t=0; t<tracks; t++)
      {
         int32_t *s = (int32_t *)sptr[t] + offset;
         int32_t *d = (int32_t *)dptr + t;
         size_t i = num;

         do
         {
            *d = *s++;
            d += tracks;
         }
         while (--i);
      }
   }
}

void
FN(batch_cvt24_intl_ps,A)(void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      static const float mul = (float)(1<<23);
      size_t t;

      for (t=0; t<tracks; t++)
      {
         float *s = (float *)sptr[t] + offset;
         int32_t *d = (int32_t *)dptr + t;
         size_t i = num;

         do
         {
            *d = (int32_t)(*s++ * mul);
            d += tracks;
         }
         while (--i);
      }
   }
}


void
FN(batch_cvt32_intl_24,A)(void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num)
{
   if (num)
   {
      size_t t;

      for (t=0; t<tracks; t++)
      {
         int32_t *s = (int32_t *)sptr[t] + offset;
         int32_t *d = (int32_t *)dptr + t;
         size_t i = num;

         do
         {
            *d = *s++ << 8;
            d += tracks;
         }
         while (--i);
      }
   }
}

void
FN(batch_cvtps_intl_24,A)(void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num)
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
FN(batch_cvtpd_intl_24,A)(void_ptr dptr, const_int32_ptrptr sptr, size_t offset, unsigned int tracks, size_t num)
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
FN(batch_get_average_rms,A)(const_float32_ptr data, size_t num, float *rms, float *peak)
{
   if (num)
   {
      double rms_total = 0.0;
      float peak_cur = 0.0f;

      size_t j = num;
      do
      {
         float samp = *data++;            // rms
         float val = samp*samp;
         rms_total += val;
         if (val > peak_cur) peak_cur = val;
      }
      while (--j);

      *rms = sqrt(rms_total/num);
      *peak = sqrtf(peak_cur);
   }
   else {
      *rms = *peak = 0;
   }
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
FN(batch_cvt8u_8s,A)(void *data, size_t num)
{
   if (num)
   {
      int8_t* p = (int8_t*)data;
      size_t i = num;

      do {
         *p++ -= 128;
      } while (--i);
   }
}

void
FN(batch_cvt8s_8u,A)(void *data, size_t num)
{
   if (num)
   {
      uint8_t* p = (uint8_t*)data;
      size_t i = num;

      do {
         *p++ += 128;
      } while (--i);
   }
}

void
FN(batch_cvt16u_16s,A)(void *data, size_t num)
{
   if (num)
   {
      int16_t* p = (int16_t*)data;
      size_t i = num;

      do {
         *p++ -= (int16_t)32768;
      } while (--i);
   }
}

void
FN(batch_cvt16s_16u,A)(void *data, size_t num)
{
   if (num)
   {
      uint16_t* p = (uint16_t*)data;
      size_t i = num;

      do {
         *p++ += (uint16_t)32768;
      } while (--i);
   }
}

void
FN(batch_cvt24u_24s,A)(void *data, size_t num)
{
   if (num)
   {
      int32_t* p = (int32_t*)data;
      size_t i = num;

      do {
         *p++ -= (int32_t)AAX_PEAK_MAX;
      } while (--i);
   }
}

void
FN(batch_cvt24s_24u,A)(void *data, size_t num)
{
   if (num)
   {
      uint32_t* p = (uint32_t*)data;
      size_t i = num;

      do {
         *p++ += (uint32_t)AAX_PEAK_MAX;
      } while (--i);
   }
}

void
FN(batch_cvt32u_32s,A)(void *data, size_t num)
{
   if (num)
   {
      int32_t* p = (int32_t*)data;
      size_t i = num;

      do {
         *p++ -= (int32_t)2147483647;
      } while (--i);
   }
}

void
FN(batch_cvt32s_32u,A)(void *data, size_t num)
{
   if (num)
   {
      uint32_t* p = (uint32_t*)data;
      size_t i = num;

      do {
         *p++ += (uint32_t)2147483647;
      } while (--i);
   }
}

void
FN(batch_dc_shift,A)(float32_ptr d, const_float32_ptr s, size_t num, float offset)
{
   if (offset != 0.0f)
   {
      int i = num;
      do
      {
          float samp = *s++;
          float fact = 1.0f-copysignf(offset, samp);
          *d++ = offset + samp*fact;

      } while(--i);
   }
   else if (num && d != s) {
      memcpy(d, s, num*sizeof(float));
   }
}

void
FN(batch_wavefold,A)(float32_ptr d, const_float32_ptr s, size_t num, float threshold)
{
   if (threshold != 0.0f)
   {
      static const float max = (float)(1 << 23);
      float threshold2;
      int i = num;

      threshold = max*threshold;
      threshold2 = 2.0f*threshold;
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
   else if (num && d != s) {
      memcpy(d, s, num*sizeof(float));
   }
}

// You could create a triangular probability density function by getting a
// random number between -1 and 0 and another between 0 and 1 and summing them.
// When noise shaping is added to dithering, there is less noise at low
// frequency and more noise at high frequency.
#define AAX_INT24_MIN	(-AAX_INT24_MAX - 1)
#define AAX_INT24_MAX	8388607
static inline int sign15(float x) {
   return (x < -0.5) ? -32768 : ((x > 0.5) ? 32768 : 0);
 }
static inline int sign7(float x) {
   return (x < -0.5) ? -128 : ((x > 0.5) ? 128 : 0);
}
void
FN(batch_dither,A)(int32_t *data, unsigned new_bps, size_t num)
{
   size_t i = num;
   if (num)
   {
      switch (new_bps)
      {
      case 1:
      {
         float s1 = _aax_rand_sample();
         int32_t* d = (int32_t*)data;
         do
         {
            float s2 = _aax_rand_sample();
            int32_t tpdf = (s1 - s2);
            *d = _MINMAX(*d + sign15(tpdf), AAX_INT24_MIN, AAX_INT24_MAX);
            s1 = s2;
            d++;
         }
         while (--i);
         break;
      }
      case 2:
      {
         float s1 = _aax_rand_sample();
         int32_t* d = (int32_t*)data;
         do
         {
            float s2 = _aax_rand_sample();
            float tpdf = (s1 - s2);
            *d = _MINMAX(*d + sign7(tpdf), AAX_INT24_MIN, AAX_INT24_MAX);
            s1 = s2;
            d++;
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
FN(batch_endianswap16,A)(void* data, size_t num)
{
   if (num)
   {
      int16_t* p = (int16_t*)data;
      size_t i = num;

      do
      {
         *p = _aax_bswap16(*p);
         p++;
      }
      while (--i);
   }
}

void
FN(batch_endianswap24,A)(void* data, size_t num)
{
   if (num)
   {
      uint8_t* p = (uint8_t*)data;
      size_t i = num;

      do
      {
         uint8_t s = p[0];
         p[0] = p[2];
         p[2] = s;
         p += 3;
      }
      while (--i);
   }
}

void
FN(batch_endianswap32,A)(void* data, size_t num)
{
   if (num)
   {
      int32_t* p = (int32_t*)data;
      size_t i = num;

      do
      {
         *p = _aax_bswap32(*p);
         p++;
      }
      while (--i);
   }
}

void
FN(batch_endianswap64,A)(void* data, size_t num)
{
   if (num)
   {
      int64_t* p = (int64_t*)data;
      size_t i = num;

      do
      {
         *p = _aax_bswap64(*p);
         p++;
      }
      while (--i);
   }
}

/**
 * 1st order all-pass filter
 *
 * d[k] = a1*s[k] + (s[k-1] - a1*d[k-1])
 * https://thewolfsound.com/allpass-filter/
 *
 * Used for:
 *  - phaser
 */
void
FN(batch_iir_allpass_float,A)(float32_ptr d, const_float32_ptr s, size_t i, float *hist, float a1)
{
   if (i)
   {
      float smp = hist[0];
      {
         *d = a1*(*s) + smp;
         smp = *s++ - a1*(*d++); // s[k-1] - a1*d[k-1]
      }
      while (--i);
      hist[0] = smp;
   }
}

/**
 * 1st order, 6dB/octave Exponential (weighted) Moving Average filter
 *
 * d[k] = (1-a1)*s[k] + a1*d[k-1]
 * https://web.archive.org/web/20150430031015/http://lorien.ncl.ac.uk/ming/filter/fillpass.htm
 *
 * Used for:
 *  - frequency filtering (frames and emitters)
 *  - per emitter HRTF head shadow filtering
 *  - surround crossover
 */

void
FN(batch_ema_iir_float,A)(float32_ptr d, const_float32_ptr s, size_t i, float *hist, float a1)
{
   if (i)
   {
      float smp = hist[0];
      do
      {
//      smp = (1.0f-a1)*smp + a1*(*s++);
//      smp = smp + a1*(-smp + *s++);
         smp += a1*(*s++ - smp);
         *d++ = smp;
      }
      while (--i);
      hist[0] = smp;
   }
}

void
FN(batch_freqfilter_float,A)(float32_ptr dptr, const_float32_ptr sptr, int t, size_t num, void *flt)
{
   _aaxRingBufferFreqFilterData *filter = (_aaxRingBufferFreqFilterData*)flt;
   if (num)
   {
      const_float32_ptr s = sptr;
      float k, *cptr, *hist;
      float c0, c1, c2, c3;
      float smp, h0, h1;
      int stage;

      cptr = filter->coeff;
      hist = filter->freqfilter->history[t];
      stage = filter->no_stages;
      if (!stage) stage++;

      k = filter->k;
      do
      {
         float32_ptr d = dptr;
         size_t i = num;

         c0 = *cptr++;
         c1 = *cptr++;
         c2 = *cptr++;
         c3 = *cptr++;

         h0 = hist[0];
         h1 = hist[1];

         // z[n] = k*x[n] + c0*x[n-1]  + c1*x[n-2] + c2*z[n-1] + c3*z[n-2];
         if (filter->state == AAX_BUTTERWORTH)
         {
            do
            {
               smp = (*s++ * k) + ((h0 * c0) + (h1 * c1));
               *d++ = smp       + ((h0 * c2) + (h1 * c3));

               h1 = h0;
               h0 = smp;
            }
            while (--i);
         }
         else
         {
            do
            {
               smp = (*s++ * k) + ((h0 * c0) + (h1 * c1));
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
      while (--stage);
      _batch_fmul_value(dptr, dptr, sizeof(MIX_T), num, filter->gain);
   }
}

void
FN(batch_convolution,A)(float32_ptr hcptr, const_float32_ptr cptr, const_float32_ptr sptr, unsigned int cnum, unsigned int dnum, int step, float v, float threshold)
{
   unsigned int q = cnum/step;
   do
   {
      float volume = *cptr * v;
      if (fabsf(volume) > threshold) {
         _batch_fmadd(hcptr, sptr, dnum, volume, 0.0f);
      }
      cptr += step;
      hcptr += step;
   }
   while (--q);
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
   assert(freq_factor < 10.0f);
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
FN(aaxBufResampleNearest_float,A)(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
{
   if (freq_factor == 1.0f) {
      memcpy(dptr+dmin, sptr, (dmax-dmin)*sizeof(float));
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
FN(aaxBufResampleLinear_float,A)(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
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

#if 0
 printf("dptr: %x, d+dmax: %x, dptr-d: %i (%f)\n", d, dptr+dmax, d-dptr, samp);
#endif
}

static inline void
FN(aaxBufResampleCubic_float,A)(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, float smu, float freq_factor)
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
#if 0
            // http://paulbourke.net/miscellaneous/interpolation/
            s -= 3;
            y0 = *s++;
            y1 = *s++;
            y2 = *s++;
            y3 = *s++;

            a0 = y3 - y2 - y0 + y1;
            a1 = y0 - y1 - a0;
            a2 = y2 - y0;
#else
            /* optimized code */
            a0 += y0;
            y0 = y1;
            y1 = y2;
            y2 = y3;
            y3 = *s++;
            a0 = -a0 + y3;                      /* a0 = y3 - y2 - y0 + y1; */
            a1 = y0 - y1 - a0;
            a2 = y2 - y0;
#endif
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
   }
   else if (fact < 1.0f) {
      FN(aaxBufResampleLinear_float,A)(d, s, dmin, dmax, smu, fact);
   }
   else if (fact >= 1.0f) {
      FN(aaxBufResampleDecimate_float,A)(d, s, dmin, dmax, smu, fact);
   } else {
//    FN(aaxBufResampleNearest_float,A)(d, s, dmin, dmax, smu, fact);
      memcpy(d+dmin, s, (dmax-dmin)*sizeof(float));
   }
}
