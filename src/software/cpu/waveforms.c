/*
 * Copyright 2007-2023 by Erik Hofman.
 * Copyright 2009-2023 by Adalin B.V.
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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
#endif
#include <math.h>	/* for floorf() */
#include <time.h>	/* for time() */
#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>

#include <base/gmath.h>
#include <base/types.h>
#include <base/random.h>

#include <api.h>
#include <arch.h>
#include <software/audio.h>
#include <software/rbuf_int.h>
#include <dsp/dsp.h>

#include "arch2d_simd.h"
#include "waveforms.h"

static float _gains[AAX_MAX_WAVE+1];

static void _aax_pinknoise_filter(float32_ptr, size_t, float);
static void _aax_add_data(int32_t*, const_float32_ptr, unsigned int, char, float, limitType);
static void _aax_mul_data(int32_t*, const_float32_ptr, unsigned int, char, float, limitType);
static float* _aax_generate_noise_float(float*, size_t, uint64_t, unsigned char, float);

#if 0
static float* _aax_generate_sine(size_t, float, float);
static float* _aax_generate_sawtooth(size_t, float, float);
static float* _aax_generate_triangle(size_t, float, float);
static float* _aax_generate_square(size_t, float, float);
#endif

_aax_generate_waveform_proc _aax_generate_waveform_float = _aax_generate_waveform_cpu;

static float _aax_linear(float v) {
   return v;
}

static inline float fast_atanf(float x) {
   return x*((GMATH_PI_4+0.273f) - 0.273f*fabsf(x));
}

static float
_aax_atanf(float v) {
// return atanf(v)*GMATH_1_PI_2;
   return fast_atanf( _MINMAX(v*GMATH_1_PI_2, -1.94139795f, 1.94139795f) );
}

void
_bufferMixWaveform(int32_t* data, _data_t *scratch, enum wave_types wtype, float freq, char bps, size_t no_samples, float gain, float phase, unsigned char modulate, limitType limiter)
{
   gain *= _gains[wtype];
   if (data && gain && no_samples*sizeof(int32_t) < _aaxDataGetSize(scratch))
   {
      float *ptr = _aaxDataGetData(scratch, 0);
      _aax_generate_waveform_float(ptr, no_samples, freq, phase, wtype);
      if (modulate) {
         _aax_mul_data(data, ptr, no_samples, bps, fabsf(gain), limiter);
      } else {
         _aax_add_data(data, ptr, no_samples, bps, gain, limiter);
      }
   }
}

void
_bufferMixWhiteNoise(int32_t* data, _data_t *scratch, size_t no_samples, char bps, float pitch, float gain, float fs, uint64_t seed, unsigned char skip, unsigned char modulate, limitType limiter)
{
   gain = fabsf(gain);
   if (data && gain && no_samples*sizeof(int32_t) < _aaxDataGetSize(scratch))
   {
      size_t noise_samples = pitch*no_samples + NOISE_PADDING;
      float *ptr = _aaxDataGetData(scratch, 0);
      float *ptr2 = _aax_generate_noise_float(ptr, noise_samples, seed, skip, fs);
      if (ptr2)
      {
         ptr = _aaxDataGetData(scratch, 1);
         _batch_resample_float(ptr, ptr2, 0, no_samples, 0, pitch);
         if (modulate) {
            _aax_mul_data(data, ptr, no_samples, bps, fabsf(gain), limiter);
         } else {
            _aax_add_data(data, ptr, no_samples, bps, gain, limiter);
         }
      }
   }
}

void
_bufferMixPinkNoise(int32_t* data, _data_t *scratch, size_t no_samples, char bps, float pitch, float gain, float fs, uint64_t seed, unsigned char skip, unsigned char modulate, limitType limiter)
{
   gain = fabsf(gain);
   if (data && gain && no_samples*sizeof(int32_t) < _aaxDataGetSize(scratch))
   { // _aax_pinknoise_filter requires twice the noise_samples buffer space
      float *ptr2, *ptr = _aaxDataGetData(scratch, 0);
      size_t noise_samples;

      pitch = ((unsigned int)(pitch*no_samples/100.0f)*100.0f)/no_samples;

      noise_samples = pitch*no_samples + NOISE_PADDING;
      ptr2 = _aax_generate_noise_float(ptr, 2*noise_samples, seed, skip, fs);
      if (ptr2)
      {
         ptr = _aaxDataGetData(scratch, 1);
         _aax_pinknoise_filter(ptr2, noise_samples, fs);
         _batch_fmul_value(ptr2, ptr2, sizeof(float), noise_samples, 1.5f);
         _batch_resample_float(ptr, ptr2+NOISE_PADDING/2, 0, no_samples, 0, pitch);

         if (modulate) {
            _aax_mul_data(data, ptr, no_samples, bps, fabsf(gain), limiter);
         } else {
            _aax_add_data(data, ptr, no_samples, bps, gain, limiter);
         }
      }
   }
}

void
_bufferMixBrownianNoise(int32_t* data, _data_t *scratch, size_t no_samples, char bps, float pitch, float gain, float fs, uint64_t seed, unsigned char skip, unsigned char modulate, limitType limiter)
{
   gain = fabsf(gain);
   if (data && gain && no_samples*sizeof(int32_t) < _aaxDataGetSize(scratch))
   {
      float *ptr2, *ptr = _aaxDataGetData(scratch, 0);
      size_t noise_samples;

      noise_samples = pitch*no_samples + 64;
      ptr2 = _aax_generate_noise_float(ptr, 2*noise_samples, seed, skip, fs);
      if (ptr2)
      {
         float k = _aax_movingaverage_compute(100.0f, fs);
         float hist = 0.0f;

         ptr = _aaxDataGetData(scratch, 1);
         _batch_movingaverage_float(ptr2, ptr2, noise_samples, &hist, k);
         _batch_fmul_value(ptr2, ptr2, sizeof(int32_t), noise_samples, 3.5f);
         _batch_resample_float(ptr, ptr2, 0, no_samples, 0, pitch);

         if (modulate) {
            _aax_mul_data(data, ptr, no_samples, bps, fabsf(gain), limiter);
         } else {
            _aax_add_data(data, ptr, no_samples, bps, gain, limiter);
         }
      }
   }
}

/* -------------------------------------------------------------------------- */

static float _gains[AAX_MAX_WAVE+1] = {
    1.0f, // AAX_CONSTANT_VALUE
    0.7f, // AAX_SAWTOOTH_WAVE
    0.95f, // AAX_SQUARE_WAVE
    0.9f, // AAX_TRIANGLE_WAVE
    1.0f, // AAX_SINE_WAVE
    1.0f, // AAX_CYCLOID_WAVE
    1.1f // AAX_IMPULSE_WAVE
};

ALIGN float _harmonics[AAX_MAX_WAVE+1][2*MAX_HARMONICS] =
{
  /* AAX_CONSTANT_VALUE */
  { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
    0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
    0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
    0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f
  },

  /* _SAWTOOTH_WAVE */
  { 1.f, 1.f/2.f, 1.f/3.f, 1.f/4.f, 1.f/5.f, 1.f/6.f, 1.f/7.f, 1.f/8.f,
    1.f/9.f, 1.f/10.f, 1.f/11.f, 1.f/12.f, 1.f/13.f, 1.f/14.f, 1.f/15.f,
    1.f/16.f, 1.f/17.f, 1.f/18.f, 1.f/19.f, 1.f/20.f, 1.f/21.f, 1.f/22.f,
    1.f/23.f, 1.f/24.f, 1.f/25.f, 1.f/26.f, 1.f/27.f, 1.f/28.f, 1.f/29.f,
    1.f/30.f, 1.f/31.f
  },

  /* _SQUARE_WAVE */
  { 1.f, 0.0f, 1.f/3.f, 0.f, 1.f/5.f, 0.f, 1.f/7.f, 0.f,
    1.f/9.f, 0.f, 1.f/11.f, 0.f, 1.f/13.f, 0.f, 1.f/15.f, 0.f,
    1.f/17.f, 0.f, 1.f/19.f, 0.f, 1.f/21.f, 0.f, 1.f/23.f, 0.f,
    1.f/25.f, 0.f, 1.f/27.f, 0.f, 1.f/29.f, 0.f, 1.f/31.f, 0.f
  },

  /* _TRIANGLE_WAVE */
  { 1.f, 0.f, -1.f/9.f, 0.f, 1.f/25.f, 0.f, -1.f/49.f, 0.f,
    1.f/81.f, 0.f, -1.f/121.f, 0.f, 1.f/169.f, 0.0f, -1.f/225.f, 0.f,
    1.f/289.f, 0.f, -1.f/361.f, 0.f, 1.f/441.f, 0.0f, -1.f/529.f, 0.f,
    1.f/625.f, 0.f, -1.f/729.f, 0.f, 1.f/841.f, 0.0f, -1.f/961.f, 0.f
  },

  /* _SINE_WAVE */
  { 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
    0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
    0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
    0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f
  },

  /* CYCLOID_WAVE */
  { 1.f, 1.f/4.f, 1.f/8.f, 1.f/12.f, 1.f/16.f, 1.f/20.f, 1.f/24.0f,
    1.f/28.f, 1.f/32.f, 1.f/36.f, 1.f/40.f, 1.f/44.f, 1.f/48.f, 1.f/52.f,
    1.f/56.f, 1.f/60.f, 1.f/64.f, 1.f/68.f, 1.f/72.f, 1.f/76.f, 1.f/80.f,
    1.f/84.f, 1.f/88.f, 1.f/92.f, 1.f/96.f, 1.f/100.f, 1.f/104.f, 1.f/108.f,
    1.f/112.f, 1.f/116.f, 1.f/120.f, 1.f/124.f
  },

  /* _IMPULSE_WAVE */
  { 1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f,
    1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f,
    1.f/16.f, 1.f/16.f,
    1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f,
    1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f, 1.f/16.f,
    1.f/16.f, 1.f/16.f
  }
};

/**
 * Generate an array of random samples
 * output range is -1.0 .. 1.0
 */
static float
_aax_seeded_random() {
   return (float)((double)_aax_rand()/(double)INT64_MAX);
}


#define FC	50.0f

static float *
_aax_generate_noise_float(float *rv, size_t no_samples, uint64_t seed, unsigned char skip, float fs)
{
   if (rv)
   {
      float (*rnd_fn)() = _aax_rand_sample;
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
      // exponential moving average (EMA) filter
      // to filter frequencies below FC (50Hz)
      _aax_EMA_compute(FC, fs, &alpha);

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

#if 0
#define AVERAGE_SAMPS          8
static void
_aax_resample_float(float32_ptr dptr, const_float32_ptr sptr, size_t dmin, size_t dmax, UNUSED(float smu), float freq_factor)
{
   size_t i, smax = floorf(dmax * freq_factor);
   const float *s = sptr + dmin;
   float *d = dptr;
   float smu = 0.0f;
   float samp, dsamp;

   samp = *(s+smax-1);
   dsamp = *(++s) - samp;
   i = dmax;
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

      for (i=0; i<AVERAGE_SAMPS; i++)
      {
         float fact = 0.5f - (float)i/(4.0f*AVERAGE_SAMPS);
         float dst = dptr[dmax-i-1];
         float src = dptr[i];

         dptr[dmax-i-1] = fact*src + (1.0f - fact)*dst;
         dptr[i] = (1.0f - fact)*src + fact*dst;
      }
   }
}
#endif

static void
_aax_add_data(int32_t* data, const_float32_ptr mix, unsigned int no_samples, char bps, float gain, limitType limiter)
{
   float (*_aax_limit)(float) = limiter ? _aax_linear : _aax_atanf;
   int i  = no_samples;
   const float *m = mix;
   if (bps == 1)
   {
      static const float div = 1.0f/128.0f;
      static const float mul = 127.0f;
      int8_t *d = (int8_t*)data;
      do {
         float v = (float)*d * div + *m++ * gain;
         *d++ = mul * _aax_limit(v);
      } while (--i);
   }
   else if (bps == 2)
   {
      static const float div = 1.0f/32768.0f;
      static const float mul = 32767.0f;
      int16_t *d = (int16_t*)data;
      do {
         float v = (float)*d * div + *m++ * gain;
         *d++ = mul * _aax_limit(v);
      } while (--i);
   }
   else if (bps == 3 || bps == 4)
   {
      static const float div = 1.0f/AAX_PEAK_MAX;
      static const float mul = AAX_PEAK_MAX;
      int32_t *d = (int32_t*)data;
      do {
         float v = (float)*d * div + *m++ * gain;
         *d++ = mul * _aax_limit(v);
      } while (--i);
   }
}


#define RINGMODULATE(a,b,c,d)	((c)*(((float)(a)/(d))*((b)/(c))))
static void
_aax_mul_data(int32_t* data, const_float32_ptr mix, unsigned int no_samples, char bps, float gain, limitType limiter)
{
   float (*_aax_limit)(float) = limiter ? _aax_linear : _aax_atanf;
   int i  = no_samples;
   const float *m = mix;
   if (bps == 1)
   {
      static const float div = 1.0f/127.0f;
      static const float mul = 127.0f;
      int8_t *d = (int8_t*)data;
      do {
         *d = mul * _aax_limit(*d*div * *m++ * gain);
         ++d;
      } while (--i);
   }
   else if (bps == 2)
   {
      static const float div = 1.0f/32765.0f;
      static const float mul = 32765.0f;
      int16_t *d = (int16_t*)data;
      do {
         *d = mul * _aax_limit(*d*div * *m++ * gain);
         ++d;
      } while (--i);
   }
   else if (bps == 3 || bps == 4)
   {
      static const float div = 1.0f/(255.0f*32765.0f);
      static const float mul = 255.0f*32765.0f;
      int32_t *d = (int32_t*)data;
      do {
         *d = mul * _aax_limit(*d*div * *m++ * gain);
         ++d;
      } while (--i);
   }
}

#define NO_FILTER_STEPS         6
void
_aax_pinknoise_filter(float32_ptr data, size_t no_samples, float fs)
{
   float f = (float)logf(fs/100.0f)/(float)NO_FILTER_STEPS;
   _aaxRingBufferFreqFilterHistoryData freqfilter;
   _aaxRingBufferFreqFilterData filter;
   int q = NO_FILTER_STEPS;
   float *dst, *tmp, *ptr = data;
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
      filter.low_gain = 0.0f;
      filter.state = AAX_TRUE;
      filter.k = 1.0f;

      freqfilter.history[0][0] = 0.0f;
      freqfilter.history[0][1] = 0.0f;
      filter.freqfilter = &freqfilter;

      fc = expf((float)(q-1)*f)*100.0f;
      _aax_bessel_compute(fc, &filter);
      _batch_freqfilter_float(dst, ptr, 0, no_samples, &filter);
      _batch_fmadd(dst, ptr, no_samples,  v2, 0.0);

      tmp = dst;
      dst = ptr;
      ptr = tmp;
   }
   while (--q);
}

