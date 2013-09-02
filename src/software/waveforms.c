/*
 * Copyright 2007-2011 by Erik Hofman.
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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
#endif
#include <math.h>	/* for floorf() */
#include <time.h>	/* for time() */

#include <base/gmath.h>

#include "api.h"
#include "arch.h"

/* -------------------------------------------------------------------------- */

#define GET_RANDOM(a,b)		((a) + ((b)*rand()/(RAND_MAX+(a))))

typedef float (*_calc_sample)(float *s, float g);
typedef void (*_mix_fn)(void*, unsigned int, float, float, unsigned char, float, float, _calc_sample);

static float _rand_sample(float *s, float g)
{
   return g*(-1.0f + GET_RANDOM(0.0f, 2.0f));
}

static float _sin_sample(float *s, float g)
{
   *s = fmodf(*s, GMATH_2PI);
   return floorf(fast_sin(*s) * g);
}

#if 0
static float _powsin_sample(float *s, float g)
{
   *s = fmodf(*s, GMATH_2PI);
   return floor(pow(fast_sin(*s), 143.0f) * g);
}
#endif

#define MIX(a,b,c)		_MINMAX((a)+(b),-(c), (c))
#define RINGMODULATE(a,b,c,d)	((c)*(((float)(a)/(d))*((b)/(c))))

void _mul_8bps(void* data, unsigned int samples, float dt, float phase, unsigned char skip, float gain, float dc, _calc_sample fn)
{
   static const float max = 127.0f;
   float mul = _MINMAX(gain, -1.0f, 1.0f) * max;
   float rnd_skip = skip ? skip : 1.0f;
   unsigned int i = samples/skip;
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
      if (skip) rnd_skip = 1.0f + (2*skip-rnd_skip)*(rand()/(RAND_MAX+1.0f));
   } while (i>=rnd_skip);
}

void _mix_8bps(void* data, unsigned int samples, float dt, float phase, unsigned char skip, float gain, float dc, _calc_sample fn)
{
   static const float max = 127.0f;
   float mul = _MINMAX(gain, -1.0f, 1.0f) * max;
   float rnd_skip = skip ? skip : 1.0f;
   unsigned int i = samples;
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
      if (skip) rnd_skip = 1.0f + (2*skip-rnd_skip)*(rand()/(RAND_MAX+1.0f));
   } while (i>=rnd_skip);
}

void _mul_16bps(void* data, unsigned int samples, float dt, float phase, unsigned char skip, float gain, float dc, _calc_sample fn)
{
   static const float max = 32765.0f;
   float mul = _MINMAX(gain, -1.0f, 1.0f) * max;
   float rnd_skip = skip ? skip : 1.0f;
   unsigned int i = samples;
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
      if (skip) rnd_skip = 1.0f + (2*skip-rnd_skip)*(rand()/(RAND_MAX+1.0f));
   } while (i>=rnd_skip);
}

void _mix_16bps(void* data, unsigned int samples, float dt, float phase, unsigned char skip, float gain, float dc, _calc_sample fn)
{
   static const float max = 32765.0f;
   float mul = _MINMAX(gain, -1.0f, 1.0f) * max;
   float rnd_skip = skip ? skip : 1.0f;
   unsigned int i = samples;
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
      if (skip) rnd_skip = 1.0f + (2*skip-rnd_skip)*(rand()/(RAND_MAX+1.0f));
   } while (i>=rnd_skip);
}

void _mul_24bps(void* data, unsigned int samples, float dt, float phase, unsigned char skip, float gain, float dc, _calc_sample fn)
{
   static const float max = 255.0f*32765.0f;
   float mul = _MINMAX(gain, -1.0f, 1.0f) * max;
   float rnd_skip = skip ? skip : 1.0f;
   unsigned int i = samples/skip;
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
      if (skip) rnd_skip = 1.0f + (2*skip-rnd_skip)*(rand()/(RAND_MAX+1.0f));
   } while (i>=rnd_skip);
}

void _mix_24bps(void* data, unsigned int samples, float dt, float phase, unsigned char skip, float gain, float dc, _calc_sample fn)
{
   static const float max = 255.0f*32765.0f;
   float mul = _MINMAX(gain, -1.0f, 1.0f) * max;
   unsigned int i = samples/skip;
   float rnd_skip = skip ? skip : 1.0f;
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
      if (skip) rnd_skip = 1.0f + (2*skip-rnd_skip)*(rand()/(RAND_MAX+1.0f));
   } while (i>=skip);
}

_mix_fn _get_mixfn(char bps, float *gain)
{
   int ringmodulate = (*gain < 0.0f) ? 1 : 0;

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

#define NO_FILTER_STEPS         6
void
__bufferPinkNoiseFilter(int32_t *data, unsigned int no_samples, float fs)
{
   float f = (float)logf(fs/100.0f)/(float)NO_FILTER_STEPS;
   unsigned int q = NO_FILTER_STEPS;
   int32_t *dst, *tmp, *ptr = data;
   dst = ptr + no_samples;
   do
   {
      float cptr[4], hist[2];
      float fc, v1, v2;
      float k = 1.0f;
      float Q = 1.0f;

      v1 = powf(1.003f, q);
      v2 = powf(0.93f, q);
      fc = expf((float)(q-1)*f)*100.0f;
      hist[0] = 0.0f; hist[1] = 0.0f;
      iir_compute_coefs(fc, fs, cptr, &k, Q);
      _batch_freqfilter(dst, ptr, no_samples, hist, v1, v2, k, cptr);

      tmp = dst;
      dst = ptr;
      ptr = tmp;
   }
   while (--q);
}

void
_bufferMixWhiteNoise(void** data, unsigned int no_samples, char bps, int tracks, float gain, float dc, unsigned char skip)
{
   _mix_fn mixfn = _get_mixfn(bps, &gain);
   if (data && mixfn)
   {
      int track, stime;
      time_t ltime;

      ltime = time(NULL);
      stime = (unsigned) ltime/2;
      srand(stime);

      for(track=0; track<tracks; track++) {
         mixfn(data[track], no_samples, 0.0f, 1.0f, skip, gain, dc, _rand_sample);
      }
   }
}

void
_bufferMixPinkNoise(void** data, unsigned int no_samples, char bps, int tracks, float gain, float fs, float dc, unsigned char skip)
{
   _mix_fn mixfn = _get_mixfn(3, &gain);
   int32_t* scratch = malloc(2*no_samples*sizeof(int32_t));
   if (data && scratch)
   {
      int track, stime;
      time_t ltime;

      ltime = time(NULL);
      stime = (unsigned) ltime/2;
      srand(stime);

      for(track=0; track<tracks; track++)
      {
         unsigned int i;
         int32_t *ptr;

         ptr = scratch;
         memset(ptr, 0, no_samples*sizeof(int32_t));
         mixfn(ptr, no_samples, 0.0f, 1.0f, skip, gain, dc, _rand_sample);

         __bufferPinkNoiseFilter(ptr, no_samples, fs);
         // _batch_saturate24(ptr, no_samples);

         i = no_samples;
         if (bps == 1)
         {
            uint8_t* p = data[track];
            do {
               *p++ += (*ptr++ >> 16);
            } while (--i);
         }
         else if (bps == 2)
         {
            int16_t* p = data[track];
            do {
               *p++ += *ptr++ >> 8;
            } while (--i);
         }
         else if (bps == 3 || bps == 4)
         {
            int32_t* p = data[track];
            do {
               *p++ += *ptr++;
            } while (--i);
         }
      }
      free(scratch);
   }
}

void
_bufferMixBrownianNoise(void** data, unsigned int no_samples, char bps, int tracks, float gain, float fs, float dc, unsigned char skip)
{
   _mix_fn mixfn = _get_mixfn(3, &gain);
   int32_t* scratch = malloc(2*no_samples*sizeof(int32_t));
   if (data && scratch)
   {
      int track, stime;
      time_t ltime;

      ltime = time(NULL);
      stime = (unsigned) ltime/2;
      srand(stime);

      for(track=0; track<tracks; track++)
      {
         unsigned int i;
         int32_t *ptr;

         ptr = scratch;
         memset(ptr, 0, no_samples*sizeof(int32_t));
         mixfn(ptr, no_samples, 0.0f, 1.0f, skip, gain, dc, _rand_sample);

         __bufferPinkNoiseFilter(ptr, no_samples, fs);
         __bufferPinkNoiseFilter(ptr, no_samples, fs);
         _batch_saturate24(ptr, no_samples);

         i = no_samples;		/* convert and add */
         if (bps == 1) {
            uint8_t* p = data[track];
            do {
               *p++ += (*ptr++ >> 16);
            } while (--i);
         }
         else if (bps == 2)
         {
            int16_t* p = data[track];
            do {
               *p++ += *ptr++ >> 8;
            } while (--i);
         }
         else if (bps == 3 || bps == 4)
         {
            int32_t* p = data[track];
            do {
               *p++ += *ptr++;
            } while (--i);
         }
      }
      free(scratch);
   }
}

#if 0
void
_bufferMixImpulse(void** data, float freq, char bps, unsigned int no_samples, int tracks, float gain, float phase)
{
   _mix_fn mixfn = _get_mixfn(bps, &gain);
   if (data && mixfn)
   {
      unsigned int track;
      float dt;

      dt = GMATH_2PI/freq;
      for(track=0; track<tracks; track++) {
         mixfn(data[track], no_samples, dt, 0.0f, 0, gain, 1.0f, _powsin_sample);
      }
   }
}
#else

#define NO_IMPULSE_HARMONICS		9
void
_bufferMixImpulse(void** data, float freq, char bps, unsigned int no_samples, int tracks, float gain, float phase)
{
   _mix_fn mixfn = _get_mixfn(bps, &gain);
   if (data && mixfn)
   {
      int track;
      for(track=0; track<tracks; track++)
      {
         unsigned int j = NO_IMPULSE_HARMONICS;
         float dt = GMATH_2PI/freq;
         float ngain = gain/NO_IMPULSE_HARMONICS;
         do
         {
            float ndt = dt*j;
            if (ndt <= GMATH_PI) {
               mixfn(data[track], no_samples, ndt, 0.0f, 0, ngain, 1.0f,
                    _sin_sample);
            }
         } while (--j);
      }
   }
}
#endif

void
_bufferMixSineWave(void** data, float freq, char bps, unsigned int no_samples, int tracks, float gain, float phase)
{
   _mix_fn mixfn = _get_mixfn(bps, &gain);
   if (data && mixfn)
   {
      int track;
      float dt;

      dt = GMATH_2PI/freq;
      for(track=0; track<tracks; track++) {
         mixfn(data[track], no_samples, dt, phase, 0, gain, 1.0f, _sin_sample);
      }
   }
}


#define NO_SAWTOOTH_HARMONICS		11
void
_bufferMixSawtooth(void** data, float freq, char bps, unsigned int no_samples, int tracks, float gain, float phase)
{
   _mix_fn mixfn = _get_mixfn(bps, &gain);
   if (data && mixfn)
   {
      int track;
      for(track=0; track<tracks; track++)
      {
         unsigned int j = NO_SAWTOOTH_HARMONICS;
         float dt = GMATH_2PI/freq;
         do
         {
            float ngain = 0.75f*gain/j;
            float ndt = dt*j;
            if (ndt <= GMATH_PI) {
               mixfn(data[track], no_samples, ndt, phase, 0, ngain, 1.0f,
               _sin_sample);
            }
         } while (--j);
      }
   }
}

#define NO_SQUAREWAVE_HARMONICS		7
void
_bufferMixSquareWave(void** data, float freq, char bps, unsigned int no_samples, int tracks, float gain, float phase)
{
   _mix_fn mixfn = _get_mixfn(bps, &gain);
   if (data && mixfn)
   {
      int track;
      for(track=0; track<tracks; track++)
      {
         unsigned int j = NO_SAWTOOTH_HARMONICS;
         do
         {
            float nfreq = freq/(2*j-1);
            float ngain = gain/(2*j-1);
            float ndt = GMATH_PI/nfreq;
            if (ndt <= GMATH_PI) {
               mixfn(data[track], no_samples, ndt, phase, 0, ngain, 1.0f,
                     _sin_sample);
            }
         } while (--j);
      }
   }
}

#define NO_TRIANGLEWAVE_HARMONICS	9
void
_bufferMixTriangleWave(void** data, float freq, char bps, unsigned int no_samples, int tracks, float gain, float phase)
{
   _mix_fn mixfn = _get_mixfn(bps, &gain);
   if (data && mixfn)
   {
      int track;
      float m = -1;
      gain *= 0.6f;
      for(track=0; track<tracks; track++)
      {
         unsigned int j = NO_TRIANGLEWAVE_HARMONICS;
         do
         {
            float nfreq = freq/(2*j-1);
            float ngain = m*gain/(j*j);
            float ndt = GMATH_2PI/nfreq;
            if (ndt <= GMATH_PI) {
               mixfn(data[track], no_samples, ndt, phase, 0, ngain, 1.0f,
                     _sin_sample);
            }
            m *= -1.0f;
         } while (--j);
      }
   }
}
