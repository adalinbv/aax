/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
#endif
#include <math.h>	/* for floorf() */
#include <time.h>	/* for time() */
#include <assert.h>

#include <base/gmath.h>
#include <base/types.h>

#include <api.h>
#include <arch.h>

/** MT199367                                                                  *
 * "Cleaned up" and simplified Mersenne Twister implementation.               *
 * Vastly smaller and more easily understood and embedded.  Stores the        *
 * state in a user-maintained structure instead of static memory, so          *
 * you can have more than one, or save snapshots of the RNG state.            *
 * Lacks the "init_by_array()" feature of the original code in favor          *
 * of the simpler 32 bit seed initialization.                                 *
 * Verified to be identical to the original MT199367ar code through           *
 * the first 10M generated numbers.                                           *
 *                                                                            *
 * Note: Code Taken from SimGear.                                             *
 *       The original Mersenne Twister implementation is in public domain.    *
 */
#define MT_N 624
#define MT_M 397
#define MT(i) mt->array[i]

typedef struct {unsigned int array[MT_N]; int index; } mt;
static mt random_seed;

static void
mt_init(mt *mt, unsigned int seed)
{
   int i;
   MT(0)= seed;
   for(i=1; i<MT_N; i++) {
      MT(i) = (1812433253 * (MT(i-1) ^ (MT(i-1) >> 30)) + i);
   }
   mt->index = MT_N+1;
}

static unsigned int
mt_rand32(mt *mt)
{
   unsigned int i, y;
   if(mt->index >= MT_N)
   {
      for(i=0; i<MT_N; i++)
      {
         y = (MT(i) & 0x80000000) | (MT((i+1)%MT_N) & 0x7fffffff);
         MT(i) = MT((i+MT_M)%MT_N) ^ (y>>1) ^ (y&1 ? 0x9908b0df : 0);
      }
      mt->index = 0;
   }
   y = MT(mt->index++);
   y ^= (y >> 11);
   y ^= (y << 7) & 0x9d2c5680;
   y ^= (y << 15) & 0xefc60000;
   y ^= (y >> 18);
   return y;
}

/** WELL 512, Note: also in Public Domain */
static unsigned long state[16];
static unsigned int idx = 0;

unsigned int WELLRNG512(void)
{
   unsigned int a, b, c, d;
   a = state[idx];
   c = state[(idx+13) & 15];
   b = a^c^(a<<16)^(c<<15);
   c = state[(idx+9) & 15];
   c ^= (c>>11);
   a = state[idx] = b^c;
   d = a^((a<<5) & 0xDA442D24UL);
   idx = (idx + 15) & 15;
   a = state[idx];
   state[idx] = a^b^d^(a<<2)^(b<<18)^(c<<28);

   return state[idx];
}

#define MAX_RANDOM	4294967295.0f
#define MAX_RANDOM_2	2147483647.5f
static void
_aax_srandom()
{
   static int init = -1;
   if (init < 0)
   {
      unsigned int i, num;

      mt_init(&random_seed, time(NULL));
      num = 100 + (mt_rand32(&random_seed) & 255);
      for (i=0; i<num; i++) {
          mt_rand32(&random_seed);
      }
 
      for (i=0; i<15; i++) {
         state[i] = mt_rand32(&random_seed);
      }

      num = 1024+(WELLRNG512()>>22);
      for (i=0; i<num; i++) {
         WELLRNG512();
      }
   }
}

#define _aax_random()	((float)WELLRNG512()/MAX_RANDOM)


/* -------------------------------------------------------------------------- */

typedef float (*_calc_sample)(float *s, float g);
typedef void (*_mix_fn)(void*, size_t, float, float, unsigned char, float, float, _calc_sample);


#if 0
/* Wichmen-Hill Random Number Generator
 * http://support.microsoft.com/kb/828795/ru

 * The global variables long int ix, iy ,iu are given values in range between
 * 1 and 30000 until the first handling. The standart C/C++ function modf()
 * separates the fractional part and the integer part of the value assigned
 * to the variable amod
 */
static int ix = 2173;
static int iy = 28126;
static int iz = 9278;
float _rand_sample(float *s, float g)
{
   double amod, x;
   ix = (171*ix) % 30269;
   iy = (172*iy) % 30307;
   iz = (170*iz) % 30323;
   amod  =(double)ix/30269.0 + (double)iy/30307.0 + (double)iz/30323.0;
   return g*(float)modf(amod,&x);
}
#else

#define AVG	14
#define MAX_AVG	64
static float _rand_sample(VOID(float *s), float g)
{
   static unsigned int rvals[MAX_AVG];
   static int init = 1;
   unsigned int r, p;
   float rv;

   if (init)
   {
      p = MAX_AVG-1;
      do
      {
         r = AVG;
         rv = 0.0f;
         do {
            rv += WELLRNG512();
         } while(--r);
         r = (unsigned int)rintf(rv/AVG);
         rvals[p] = r;
      }
      while(p--);
      init = 0;
   }

   r = AVG;
   rv = 0.0f;
   do {
      rv += WELLRNG512();
   } while(--r);
   r = (unsigned int)rintf(rv/AVG);

   p = (r >> 2) & (MAX_AVG-1);
   rv = rvals[p];
   rvals[p] = r;

   rv = 2.0f*g*(-1.0f + rv/MAX_RANDOM_2);

   return rv;
}
#endif

static float _sin_sample(float *s, float g)
{
   *s = fmodf(*s, GMATH_2PI);
   return floorf(fast_sin(*s) * g);
}


#define MIX(a,b,c)		_MINMAX((a)+(b),-(c), (c))
#define RINGMODULATE(a,b,c,d)	((c)*(((float)(a)/(d))*((b)/(c))))

static void
_mul_8bps(void* data, size_t samples, float dt, float phase, unsigned char skip, float gain, float dc, _calc_sample fn)
{
   static const float max = 127.0f;
   float mul = _MINMAX(gain, -1.0f, 1.0f) * max;
   float rnd_skip = skip ? skip : 1.0f;
   size_t i = samples/skip;
   int8_t* ptr = data;
   float s = phase;
   do
   {
      float samp, fact;

      samp = fn(&s, mul);
      fact = ((0.5f + 0.5f*fast_sin(s)) <= dc) ? 1.0f : 0.0f;

      *ptr = (int8_t)RINGMODULATE(*ptr, fact*samp, mul, max);
      s = fmodf(s+dt, GMATH_2PI);

      ptr += (int)rnd_skip;
      i -= (int)rnd_skip;
      if (skip) rnd_skip = 1.0f + (2*skip-rnd_skip)*_aax_random();
   } while (i>=rnd_skip);
}

static void
_mix_8bps(void* data, size_t samples, float dt, float phase, unsigned char skip, float gain, float dc, _calc_sample fn)
{
   static const float max = 127.0f;
   float mul = _MINMAX(gain, -1.0f, 1.0f) * max;
   float rnd_skip = skip ? skip : 1.0f;
   size_t i = samples;
   int8_t* ptr = data;
   float s = phase;
   do
   {
      float samp, fact;

      samp = fn(&s, mul);
      fact = ((0.5f + 0.5f*fast_sin(s)) <= dc) ? 1.0f : 0.0f;

      *ptr = (int8_t)MIX(*ptr, fact*samp, max);
      s = fmodf(s+dt, GMATH_2PI);

      ptr += (int)rnd_skip;
      i -= (int)rnd_skip;
      if (skip) rnd_skip = 1.0f + (2*skip-rnd_skip)*_aax_random();
   } while (i>=rnd_skip);
}

static void
_mul_16bps(void* data, size_t samples, float dt, float phase, unsigned char skip, float gain, float dc, _calc_sample fn)
{
   static const float max = 32765.0f;
   float mul = _MINMAX(gain, -1.0f, 1.0f) * max;
   float rnd_skip = skip ? skip : 1.0f;
   size_t i = samples;
   int16_t* ptr = data;
   float s = phase;
   do
   {
      float samp, fact;

      samp = fn(&s, mul);
      fact = ((0.5f + 0.5f*fast_sin(s)) <= dc) ? 1.0f : 0.0f;

      *ptr = (int16_t)RINGMODULATE(*ptr, fact*samp, mul, max);
      s = fmodf(s+dt, GMATH_2PI);

      ptr += (int)rnd_skip;
      i -= (int)rnd_skip;
      if (skip) rnd_skip = 1.0f + (2*skip-rnd_skip)*_aax_random();
   } while (i>=rnd_skip);
}

static void
_mix_16bps(void* data, size_t samples, float dt, float phase, unsigned char skip, float gain, float dc, _calc_sample fn)
{
   static const float max = 32765.0f;
   float mul = _MINMAX(gain, -1.0f, 1.0f) * max;
   float rnd_skip = skip ? skip : 1.0f;
   size_t i = samples;
   int16_t* ptr = data;
   float s = phase;
   do
   {
      float samp, fact;

      samp = fn(&s, mul);
      fact = ((0.5f + 0.5f*fast_sin(s)) <= dc) ? 1.0f : 0.0f;

      *ptr = (int16_t)MIX(*ptr, fact * samp, max);
      s = fmodf(s+dt, GMATH_2PI);

      ptr += (int)rnd_skip;
      i -= (int)rnd_skip;
      if (skip) rnd_skip = 1.0f + (2*skip-rnd_skip)*_aax_random();
   } while (i>=rnd_skip);
}

static void
_mul_24bps(void* data, size_t samples, float dt, float phase, unsigned char skip, float gain, float dc, _calc_sample fn)
{
   static const float max = 255.0f*32765.0f;
   float mul = _MINMAX(gain, -1.0f, 1.0f) * max;
   float rnd_skip = skip ? skip : 1.0f;
   size_t i = samples;
   int32_t* ptr = data;
   float s = phase;
   do
   {
      float samp, fact;

      samp = fn(&s, mul);
      fact = ((0.5f + 0.5f*fast_sin(s)) <= dc) ? 1.0f : 0.0f;

      *ptr = (int32_t)RINGMODULATE(*ptr, fact*samp, mul, max);
      s = fmodf(s+dt, GMATH_2PI);

      ptr += (int)rnd_skip;
      i -= (int)rnd_skip;
      if (skip) rnd_skip = 1.0f + (2*skip-rnd_skip)*_aax_random();
   } while (i>=rnd_skip);
}

static void
_mix_24bps(void* data, size_t samples, float dt, float phase, unsigned char skip, float gain, float dc, _calc_sample fn)
{
   static const float max = 255.0f*32765.0f;
   float mul = _MINMAX(gain, -1.0f, 1.0f) * max;
   float rnd_skip = skip ? skip : 1.0f;
   size_t i = samples;
   int32_t* ptr = data;
   float s = phase;
   do
   {
      float samp, fact;

      samp = fn(&s, mul);
      fact = ((0.5f + 0.5f*fast_sin(s)) <= dc) ? 1.0f : 0.0f;

      *ptr = (int32_t)MIX(*ptr, fact*samp, max);
      s = fmodf(s+dt, GMATH_2PI);

      ptr += (int)rnd_skip;
      i -= (int)rnd_skip;
      if (skip) rnd_skip = 1.0f + (2*skip-rnd_skip)*_aax_random();
   } while (i>=skip);
}

static _mix_fn
_get_mixfn(char bps, float *gain)
{
   int ringmodulate = 0; // (*gain < 0.0f) ? 1 : 0;

   *gain = fabsf(*gain);
   if (bps == 1) {
      return  ringmodulate ? _mul_8bps : _mix_8bps;
   } else if (bps == 2) {
      return  ringmodulate ? _mul_16bps : _mix_16bps;
   } else if (bps == 3 || bps == 4) {
      return ringmodulate ? _mul_24bps : _mix_24bps;
   }
   return NULL;
}

#define AVERAGE_SAMPS		8
static void
_resample_32bps(int32_t *dptr, const int32_t *sptr, size_t dmax, float freq_factor)
{
   size_t i, smax = floorf(dmax * freq_factor);
   int32_t *s = (int32_t*)sptr;
   int32_t *d = dptr;
   float smu = 0.0f;
   int32_t samp, dsamp;

   samp = *(s+smax-1);
   dsamp = *(++s) - samp;
   i = dmax;
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

      for (i=0; i<AVERAGE_SAMPS; i++)
      {
         float fact = 0.5f - (float)i/(4.0f*AVERAGE_SAMPS);
         int32_t dst = dptr[dmax-i-1];
         int32_t src = dptr[i];

         dptr[dmax-i-1] = fact*src + (1.0f - fact)*dst;
         dptr[i] = (1.0f - fact)*src + fact*dst;
      }
   }
}

static void
_add_data(void_ptrptr data, const_void_ptr mix, int tracks, unsigned int no_samples, char bps)
{
   int track;
   for(track=0; track<tracks; track++)
   {
      unsigned int i = no_samples;
      if (bps == 1)
      {
         const int8_t *m = mix;
         int8_t *d = data[track];
         do {
            *d++ += *m++;
         } while (--i);
      }
      else if (bps == 2)
      {
         const int16_t *m = mix;
         int16_t *d = data[track];
         do {
            *d++ += *m++;
         } while (--i);
      }
      else if (bps == 3 || bps == 4)
      {
         const int32_t *m = mix;
         int32_t *d = data[track];
         do {
            *d++ += *m++;
         } while (--i);
      }
   }
}

static void
_mul_data(void_ptrptr data, const_void_ptr mix, int tracks, unsigned int no_samples, char bps)
{
   int track;
   for(track=0; track<tracks; track++)
   {
      unsigned int i = no_samples;
      if (bps == 1)
      {
         static const float max = 127.0f;
         const int8_t *m = mix;
         int8_t *d = data[track];
         do {
            *d = RINGMODULATE(*d, *m++, max, max);
            ++d;
         } while (--i);
      }
      else if (bps == 2)
      {
         static const float max = 32765.0f;
         const int16_t *m = mix;
         int16_t *d = data[track];
         do {
            *d = RINGMODULATE(*d, *m++, max, max);
            ++d;
         } while (--i);
      }
      else if (bps == 3 || bps == 4)
      {
         static const float max = 255.0f*32765.0f;
         const int32_t *m = mix;
         int32_t *d = data[track];
         do {
            *d = RINGMODULATE(*d, *m++, max, max);
            ++d;
         } while (--i);
      }
   }
}

static void
_add_noise(void_ptrptr data, const_int32_ptr ptr, int tracks, unsigned int no_samples, char bps)
{
   int track;
   for(track=0; track<tracks; track++)
   {
      unsigned int i = no_samples;
      if (bps == 1)
      {
         uint8_t *p = data[track];
         do {
            *p++ += (*ptr++ >> 16);
         } while (--i);
      }
      else if (bps == 2)
      {
         int16_t *p = data[track];
         do {
            *p++ += *ptr++ >> 8;
         } while (--i);
      }
      else if (bps == 3 || bps == 4)
      {
         int32_t *p = data[track];
         do {
            *p++ += *ptr++;
         } while (--i);
      }
   }
}

static void
_mul_noise(void_ptrptr data, const_int32_ptr ptr, int tracks, unsigned int no_samples, char bps)
{
   int track;
   for(track=0; track<tracks; track++)
   {
      static const float max32 = 255.0f*32765.0f;
      unsigned int i = no_samples;
      if (bps == 1)
      {
         static const float max = 127.0f;
         uint8_t *d = data[track];
         do {
            *d = RINGMODULATE(*d, *ptr++ >> 16, max32, max);
            ++d;
         } while (--i);
      }
      else if (bps == 2)
      {
         static const float max = 32765.0f;
         int16_t *d = data[track];
         do {
            *d = RINGMODULATE(*d, *ptr++ >> 8, max32, max);
            ++d;
         } while (--i);
      }
      else if (bps == 3 || bps == 4)
      {
         static const float max = 255.0f*32765.0f;
         int32_t *d = data[track];
         do {
            *d = RINGMODULATE(*d, *ptr++, max32, max);
            ++d;
         } while (--i);
      }
   }
}

#define NO_FILTER_STEPS         6
void
__bufferPinkNoiseFilter(int32_t *data, size_t no_samples, float fs)
{
   float f = (float)logf(fs/100.0f)/(float)NO_FILTER_STEPS;
   _aaxRingBufferFreqFilterData filter;
   unsigned int q = NO_FILTER_STEPS;
   int32_t *dst, *tmp, *ptr = data;
   dst = ptr + no_samples;

   filter.type = LOWPASS;
   filter.no_stages = 1;
   filter.Q = 1.0f;
   filter.fs = fs;

   do
   {
      float fc, v1, v2;

      v1 = powf(1.003f, q);
      v2 = powf(0.90f, q);
      filter.high_gain = v1-v2;
      filter.freqfilter_history[0][0] = 0.0f;
      filter.freqfilter_history[0][1] = 0.0f;
      filter.k = 1.0f;

      fc = expf((float)(q-1)*f)*100.0f;
      _aax_bessel_compute(fc, &filter);
      _batch_freqfilter(dst, ptr, 0, no_samples, &filter);
      _batch_imadd(dst, ptr, no_samples,  v2, 0.0);

      tmp = dst;
      dst = ptr;
      ptr = tmp;
   }
   while (--q);
}

void
_bufferMixWhiteNoise(void** data, VOID(void *scratch0), size_t no_samples, char bps, int tracks, float pitch, float gain, float dc, unsigned char skip)
{
   char ringmodulate = (gain < 0.0f) ? 1 : 0;
   _mix_fn mixfn = _get_mixfn(3, &gain);
   size_t noise_samples = pitch*no_samples;
   int32_t* scratch;

   scratch  = _aax_aligned_alloc((noise_samples+no_samples)*sizeof(int32_t));
   if (data && scratch)
   {
      int32_t *ptr, *ptr2;

      _aax_srandom();

      ptr = scratch;
      ptr2 = ptr + no_samples;
      memset(ptr2, 0, noise_samples*sizeof(int32_t));
      mixfn(ptr2, noise_samples, 0.0f, 1.0f, skip, gain, dc, _rand_sample);
      _resample_32bps(ptr, ptr2, no_samples, pitch);

      if (ringmodulate) {
         _mul_noise(data, ptr, tracks, no_samples, bps);
      } else {
         _add_noise(data, ptr, tracks, no_samples, bps);
      }

      _aax_aligned_free(scratch);
   }
}

void
_bufferMixPinkNoise(void** data, VOID(void *scratch0), size_t no_samples, char bps, int tracks, float pitch, float gain, float fs, float dc, unsigned char skip)
{
   char ringmodulate = (gain < 0.0f) ? 1 : 0;
   _mix_fn mixfn = _get_mixfn(3, &gain);
   size_t noise_samples = pitch*no_samples;
   int32_t* scratch;

   scratch = _aax_aligned_alloc((noise_samples+2*no_samples)*sizeof(int32_t));
   if (data && scratch)
   {
      int32_t *ptr, *ptr2;

      _aax_srandom();

      ptr = scratch;
      ptr2 = ptr + no_samples;
      memset(ptr2, 0, noise_samples*sizeof(int32_t));
      mixfn(ptr2, noise_samples, 0.0f, 1.0f, skip, gain, dc, _rand_sample);

      __bufferPinkNoiseFilter(ptr2, noise_samples, fs);
      _batch_imul_value(ptr2, sizeof(int32_t), no_samples, 1.5f);
      _resample_32bps(ptr, ptr2, no_samples, pitch);

      if (ringmodulate) {
         _mul_noise(data, ptr, tracks, no_samples, bps);
      } else {
         _add_noise(data, ptr, tracks, no_samples, bps);
      }
      _aax_aligned_free(scratch);
   }
}

void
_bufferMixBrownianNoise(void** data, VOID(void *scratch0), size_t no_samples, char bps, int tracks, float pitch, float gain, float fs, float dc, unsigned char skip)
{
   char ringmodulate = (gain < 0.0f) ? 1 : 0;
   _mix_fn mixfn = _get_mixfn(3, &gain);
   size_t noise_samples = pitch*no_samples;
   int32_t* scratch;

   scratch = _aax_aligned_alloc((noise_samples+2*no_samples)*sizeof(int32_t));
   if (data && scratch)
   {
      int32_t *ptr, *ptr2;
      float hist, k;

      _aax_srandom();

      ptr = scratch;
      ptr2 = ptr + no_samples;
      memset(ptr2, 0, noise_samples*sizeof(int32_t));
      mixfn(ptr2, noise_samples, 0.0f, 1.0f, skip, gain, dc, _rand_sample);

      hist = 0.0f;
      k = _aax_movingaverage_compute(100.0f, fs);
      _batch_movingaverage(ptr2, ptr2, no_samples, &hist, k);
      _batch_imul_value(ptr2, sizeof(int32_t), no_samples, 3.5f);
      _resample_32bps(ptr, ptr2, no_samples, pitch);

      if (ringmodulate) {
         _mul_noise(data, ptr, tracks, no_samples, bps);
      } else {
         _add_noise(data, ptr, tracks, no_samples, bps);
      }
      _aax_aligned_free(scratch);
   }
}

#define NO_IMPULSE_HARMONICS		9
void
_bufferMixImpulse(void** data, float freq, char bps, size_t no_samples, int tracks, float gain, VOID(float phase))
{
   char ringmodulate = (gain < 0.0f) ? 1 : 0;
   _mix_fn mixfn = _get_mixfn(bps, &gain);
   if (data && mixfn)
   {
      void *ptr = _aax_aligned_alloc(no_samples*bps);
      if (ptr)
      {
         unsigned int j = NO_IMPULSE_HARMONICS;
         float dt = GMATH_2PI/freq;
         float ngain = gain/NO_IMPULSE_HARMONICS;

         memset(ptr, 0, no_samples*bps);
         do
         {
            float ndt = dt*j;
            if (ndt <= GMATH_PI) {
               mixfn(ptr, no_samples, ndt, 0.0f, 0, ngain, 1.0f, _sin_sample);
            }
         } while (--j);

         if (ringmodulate) {
            _mul_data(data, ptr, tracks, no_samples, bps);
         } else {
            _add_data(data, ptr, tracks, no_samples, bps);
         }
         _aax_aligned_free(ptr);
      }
   }
}

void
_bufferMixSineWave(void** data, float freq, char bps, size_t no_samples, int tracks, float gain, float phase)
{
   char ringmodulate = (gain < 0.0f) ? 1 : 0;
   _mix_fn mixfn = _get_mixfn(bps, &gain);
   if (data && mixfn)
   {
      void *ptr = _aax_aligned_alloc(no_samples*bps);
      if (ptr)
      {
         float dt = GMATH_2PI/freq;

         memset(ptr, 0, no_samples*bps);
         mixfn(ptr, no_samples, dt, phase, 0, gain, 1.0f, _sin_sample);

         if (ringmodulate) {
            _mul_data(data, ptr, tracks, no_samples, bps);
         } else {
            _add_data(data, ptr, tracks, no_samples, bps);
         }
         _aax_aligned_free(ptr);
      }
   }
}


#define NO_SAWTOOTH_HARMONICS		11
void
_bufferMixSawtooth(void** data, float freq, char bps, size_t no_samples, int tracks, float gain, float phase)
{
   char ringmodulate = (gain < 0.0f) ? 1 : 0;
   _mix_fn mixfn = _get_mixfn(bps, &gain);
   if (data && mixfn)
   {
      void *ptr = _aax_aligned_alloc(no_samples*bps);
      if (ptr)
      {
         unsigned int j = NO_SAWTOOTH_HARMONICS;
         float dt = GMATH_2PI/freq;

         memset(ptr, 0, no_samples*bps);
         do
         {
            float ngain = 0.75f*gain/j;
            float ndt = dt*j;
            if (ndt <= GMATH_PI) {
               mixfn(ptr, no_samples, ndt, phase, 0, ngain, 1.0f, _sin_sample);
            }
         } while (--j);

         if (ringmodulate) {
            _mul_data(data, ptr, tracks, no_samples, bps);
         } else {
            _add_data(data, ptr, tracks, no_samples, bps);
         }
         _aax_aligned_free(ptr);
      }
   }
}

#define NO_SQUAREWAVE_HARMONICS		7
void
_bufferMixSquareWave(void** data, float freq, char bps, size_t no_samples, int tracks, float gain, float phase)
{
   char ringmodulate = (gain < 0.0f) ? 1 : 0;
   _mix_fn mixfn = _get_mixfn(bps, &gain);
   if (data && mixfn)
   {
      void *ptr = _aax_aligned_alloc(no_samples*bps);
      if (ptr)
      {
         unsigned int j = NO_SAWTOOTH_HARMONICS;

         memset(ptr, 0, no_samples*bps);
         do
         {
            float nfreq = freq/(2*j-1);
            float ngain = gain/(2*j-1);
            float ndt = GMATH_PI/nfreq;
            if (ndt <= GMATH_PI) {
               mixfn(ptr, no_samples, ndt, phase, 0, ngain, 1.0f, _sin_sample);
            }
         } while (--j);

         if (ringmodulate) {
            _mul_data(data, ptr, tracks, no_samples, bps);
         } else {
            _add_data(data, ptr, tracks, no_samples, bps);
         }
         _aax_aligned_free(ptr);
      }
   }
}

#define NO_TRIANGLEWAVE_HARMONICS	9
void
_bufferMixTriangleWave(void** data, float freq, char bps, size_t no_samples, int tracks, float gain, float phase)
{
   char ringmodulate = (gain < 0.0f) ? 1 : 0;
   _mix_fn mixfn = _get_mixfn(bps, &gain);
   if (data && mixfn)
   {
      void *ptr = _aax_aligned_alloc(no_samples*bps);
      if (ptr)
      {
         unsigned int j = NO_TRIANGLEWAVE_HARMONICS;
         float m = -1;

         gain *= 0.6f;
         memset(ptr, 0, no_samples*bps);
         do
         {
            float nfreq = freq/(2*j-1);
            float ngain = m*gain/(j*j);
            float ndt = GMATH_2PI/nfreq;
            if (ndt <= GMATH_PI) {
               mixfn(ptr, no_samples, ndt, phase, 0, ngain, 1.0f, _sin_sample);
            }
            m *= -1.0f;
         } while (--j);

         if (ringmodulate) {
            _mul_data(data, ptr, tracks, no_samples, bps);
         } else {
            _add_data(data, ptr, tracks, no_samples, bps);
         }
         _aax_aligned_free(ptr);
      }
   }
}
