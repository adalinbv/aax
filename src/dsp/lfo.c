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

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
# include <string.h>
#endif
#include <assert.h>

#include <base/geometry.h>
#include <base/random.h>
#include <arch.h>

#include "common.h"
#include "api.h"
#include "lfo.h"

static inline float _analog_sawtooth(float);
static inline float _analog_square(float);
static inline float _analog_triangle(float);
static inline float _analog_sin(float);
static inline float _analog_cycloid(float);
static inline float _analog_impulse(float);
static inline float _cycloid(float);

_aaxLFOData*
_lfo_create()
{
   _aaxLFOData *rv = _aax_aligned_alloc(sizeof(_aaxLFOData));
   if (rv) memset(rv, 0, sizeof(_aaxLFOData));
   return rv;
}

void
_lfo_destroy(void *data)
{
   _aaxLFOData *lfo = data;
   if (lfo)
   {
      lfo->envelope = false;
      _aax_aligned_free(lfo);
   }
}

void
_lfo_setup(_aaxLFOData *lfo, void *i, int state)
{
   _aaxMixerInfo *info = (_aaxMixerInfo*)i;
   int exponential = (state & AAX_LFO_EXPONENTIAL) ? true : false;
   int stereo = (state & AAX_LFO_STEREO) ? true : false;

   lfo->fs = info->frequency;
   lfo->period_rate = info->period_rate;
   lfo->convert = (exponential) ? _exponential : _linear;
   lfo->state = state & ~(AAX_LFO_STEREO|AAX_LFO_EXPONENTIAL|AAX_EFFECT_ORDER_MASK);
   lfo->inverse = (state & AAX_INVERSE) ? true : false;
   lfo->stereo_link = !stereo;
   lfo->depth = 1.0f;
   lfo->offset = 0.0f;
}

void
_lfo_swap(_aaxLFOData *dlfo, _aaxLFOData *slfo)
{
   if (dlfo && slfo)
   {
      dlfo->state = slfo->state;
      dlfo->offset = slfo->offset;
      dlfo->depth = slfo->depth;
      dlfo->min_sec = slfo->min_sec;
      dlfo->max_sec = slfo->max_sec;

      dlfo->f = slfo->f;
      dlfo->min = slfo->min;
      dlfo->max = slfo->max;
      dlfo->gate_threshold = slfo->gate_threshold;
      dlfo->gate_period = slfo->gate_period;

      memcpy(dlfo->step, slfo->step, sizeof(float[_AAX_MAX_SPEAKERS]));
      memcpy(dlfo->down, slfo->down, sizeof(float[_AAX_MAX_SPEAKERS]));

      dlfo->get = slfo->get;
      dlfo->convert = slfo->convert;
      dlfo->inverse = slfo->inverse;
      dlfo->envelope = slfo->envelope;
      dlfo->stereo_link = slfo->stereo_link;
   }
}

void
_lfo_reset(_aaxLFOData *lfo)
{
   if (lfo)
   {
      lfo->dt = 0.0f;
      if (lfo->get == _aaxLFOGetTimed)
      {
         int t;
         for (t=0; t<_AAX_MAX_SPEAKERS; t++) {
            lfo->value[t] = lfo->value0[t];
         }
      }
   }
}

int
_lfo_set_function(_aaxLFOData *lfo, int constant)
{
   int rv = true;
   if (constant) {
      lfo->get = _aaxLFOGetFixedValue;
   }
   else
   {
      enum aaxSourceType wave;

      wave = lfo->state & (AAX_SOURCE_MASK & ~AAX_PURE_WAVEFORM);
      switch (wave)
      {
      case AAX_CONSTANT: /* equals to true */
         lfo->get = _aaxLFOGetFixedValue;
         break;
      case AAX_SAWTOOTH:
         lfo->get = _aaxLFOGetSawtooth;
         break;
      case AAX_SQUARE:
         lfo->get = _aaxLFOGetSquare;
         break;
      case AAX_TRIANGLE:
         lfo->get = _aaxLFOGetTriangle;
         break;
      case AAX_SINE:
         lfo->get = _aaxLFOGetSine;
         break;
      case AAX_CYCLOID:
         lfo->get = _aaxLFOGetCycloid;
         break;
      case AAX_IMPULSE:
         lfo->get = _aaxLFOGetImpulse;
         break;
      case AAX_RANDOMNESS:
      case AAX_RANDOM_SELECT:
         lfo->get = _aaxLFOGetRandomness;
         break;
      case AAX_ENVELOPE_FOLLOW:
         lfo->get = _aaxLFOGetGainFollow;
         lfo->envelope = true;
         break;
      case AAX_TIMED_TRANSITION:
         lfo->get = _aaxLFOGetTimed;
         break;
      default:
         /* reaching here is actually a bug but prevent a segmentation fault */
         lfo->get = _aaxLFOGetFixedValue;
         rv = false;
         break;
      }
   }

   return rv;
}

/*
 * lfo->min_sec and lfo->max_sec define the boundaries (in seconds).
 *
 * lfo->offset defines a factor of those boundaries.
 * So lfo->offset = 1.0f means lfo->offset equals to lfo->max_sec
 *
 * lfo->depth defines a maximum offset to lfo->offset
 * which could be 0.0f in which case there will be a constant offet.
 */
int
_lfo_set_timing(_aaxLFOData *lfo)
{
   float range = lfo->max_sec - lfo->min_sec;
   float fs = lfo->fs;
   bool constant = true;
   bool wstate;

   lfo->min = fs*(lfo->min_sec + lfo->offset*range);
   lfo->max = lfo->min + fs*(lfo->depth*range);

#if 0
 printf("offset: %f, range: %f, fs: %f\n", lfo->offset, range, fs);
 printf("lfo min: %f, max: %f\n", lfo->min, lfo->max);
#endif

   constant = ((lfo->max - lfo->min) > 0.01f) ? false : true;
   if (!constant)
   {
      float sign, step;
      int t;
      for (t=0; t<_AAX_MAX_SPEAKERS; t++)
      {
         if (!lfo->stereo_link) {
            lfo->value0[t] = (t % 2) ? lfo->min : lfo->max;
         }

         // slowly work towards the new settings
         step = lfo->step[t];
         sign = step ? (step/fabsf(step)) : 1.0f;

         lfo->step[t] = 2.0f*sign * lfo->f;
         lfo->step[t] *= (lfo->max - lfo->min);
         lfo->step[t] /= lfo->period_rate;
         if (lfo->step[t] > lfo->max-lfo->min) {
            lfo->step[t] = lfo->max-lfo->min;
         }

         if ((lfo->value0[t] == 0) || (lfo->value0[t] < lfo->min)) {
            lfo->value0[t] = lfo->min;
         } else if (lfo->value0[t] > lfo->max) {
            lfo->value0[t] = lfo->max;
         }

         switch(lfo->state & AAX_SOURCE_MASK)
         {
         case AAX_SAWTOOTH:
            lfo->step[t] *= 0.5f;
            break;
         case AAX_ENVELOPE_FOLLOW:
            lfo->step[t] = ENVELOPE_FOLLOW_STEP_CVT(lfo->f);
            lfo->value0[t] = 0.0f;
            break;
         case AAX_RANDOMNESS:
         {
            float fs = lfo->period_rate;
            float fc = lfo->f;
            float cfc = cosf(GMATH_2PI*fc/fs);
            lfo->step[t] = -1.0f + cfc + sqrtf(cfc*cfc -4.0f*cfc + 3.0f);
            break;
         }
         default:
            break;
         }
         lfo->value[t] = lfo->value0[t];
      }
   }
   else
   {
      int t;
      for (t=0; t<_AAX_MAX_SPEAKERS; t++) {
         if (!lfo->stereo_link) {
            lfo->value[t] = lfo->value0[t] = (t % 2) ? lfo->min : lfo->max;
         } else {
            lfo->value[t] = lfo->value0[t] = lfo->min;
         }
      }
   }

   return constant;
}

int
_compressor_set_timing(_aaxLFOData *lfo)
{
   float dt = 3.16228f/lfo->period_rate;
   int t;

   for (t=0; t<_AAX_MAX_SPEAKERS; t++)
   {
      lfo->step[t] = 2.0f*lfo->max * lfo->f;
      lfo->step[t] *= (lfo->max - lfo->min);
      lfo->step[t] /= lfo->period_rate;
      lfo->value[t] = 1.0f;

      /*
       * We're implementing an upward dynamic range
       * compressor, which means that attack is down!
       */
      lfo->step[t] = _MIN(dt/lfo->min_sec, 2.0f);
      lfo->down[t] = _MIN(dt/lfo->max_sec, 2.0f);

   }

   dt = 1.0f/lfo->period_rate;
   lfo->gate_period = GMATH_E1 * _MIN(dt/lfo->offset, 2.0f);

   return 0;
}


void
_env_reset(_aaxEnvelopeData* env)
{
   if (env)
   {
      env->repeat = env->repeat0;
      env->value = env->value0;
      env->stage = 0;
      env->pos = 0;
   }
}


/* gradually fade-in if there is a delay offset set */
static inline float
_aaxLFODelay(_aaxLFOData* lfo, float rv)
{
   float delay = fabsf(lfo->delay);
   if (lfo->dt < delay)
   {
       float f;

       lfo->dt += 1.0f/lfo->period_rate;
       f = lfo->dt/delay;

       if (lfo->delay < 0.0f) rv = 1.0f - (1.0f - rv)*f;	// pitch
       else rv *= f;						// gain
   }
   return rv;
}

static inline float
_aaxLFOCalculate(_aaxLFOData *lfo, float val, unsigned track)
{
   float max = (lfo->max - lfo->min);
   float rv = lfo->min;
   if (max)
   {
      rv = max ? (val - lfo->min)/max : val;
      rv = _aaxLFODelay(lfo, rv);
   }
   lfo->compression[track] = 1.0f - rv;

   rv = lfo->convert(rv, lfo);

   return rv;
}

/*
 * Low Frequency Oscilator funtions
 *
 * Internally the oscillator always runs between 0.0f and 1.0f
 * The functions return a value between lfo->min and lfo->max which are
 * absolute values that may range beyond the internal limits.
 *
 * lfo->step is a user defined, time (refresh rate) compensated step value
 * that assures the oscillator will run one cycle in the desired frequency.
 *
 * lfo->inverse is an internal parameter that defines the counting direction.
 *
 * lfo->value is the current LFO output value in the used defined range
 * (between lfo->min and lfo->max).
 */
float
_aaxLFOGetFixedValue(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   float rv = 1.0f;
   if (lfo)
   {
      rv = _aaxLFOCalculate(lfo, lfo->value[track], track);
      lfo->compression[track] = 1.0f - rv;
   }
   return rv;
}

float
_aaxLFOGetTriangle(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   float rv = 1.0f;
   if (lfo)
   {
      float step = lfo->step[track];

      if (lfo->state & AAX_PURE_WAVEFORM) {
         rv = _aaxLFOCalculate(lfo, lfo->value[track], track);
      }
      else
      {
         float max = (lfo->max - lfo->min);

         assert(max);

         rv = (lfo->value[track] - lfo->min)/max;
         rv = _aaxLFODelay(lfo, rv);

         lfo->compression[track] = 1.0f-rv;

         rv = lfo->convert(_analog_triangle(rv), lfo);
      }

      lfo->value[track] += step;
      if (((lfo->value[track] <= lfo->min) && (step < 0))
          || ((lfo->value[track] >= lfo->max) && (step > 0)))
      {
         lfo->step[track] *= -1.0f;
         lfo->value[track] -= step;
      }
      lfo->compression[track] = 1.0f - rv;
   }
   return rv;
}

float
_aaxLFOGetSine(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   float rv = 1.0f;
   if (lfo)
   {
      float max = (lfo->max - lfo->min);
      float step = lfo->step[track];

      assert(max);

      rv = (lfo->value[track] - lfo->min)/max;
      rv = _aaxLFODelay(lfo, rv);

      lfo->compression[track] = 1.0f-rv;

      if (lfo->state & AAX_PURE_WAVEFORM) {
         rv = lfo->convert(sinf(rv), lfo);
      } else {
         rv = lfo->convert(_analog_sin(rv), lfo);
      }

      lfo->value[track] += step;
      if (((lfo->value[track] <= lfo->min) && (step < 0))
          || ((lfo->value[track] >= lfo->max) && (step > 0)))
      {
         lfo->step[track] *= -1.0f;
         lfo->value[track] -= step;
      }
   }
   return rv;
}

float
_aaxLFOGetSquare(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   float rv = 1.0f;
   if (lfo)
   {
      float max = (lfo->max - lfo->min);
      float step = lfo->step[track];

      assert(max);

      rv = (lfo->value[track] - lfo->min)/max;
      rv = _aaxLFODelay(lfo, rv);
      lfo->compression[track] = 1.0f-rv;

      if (lfo->state & AAX_PURE_WAVEFORM) {
         rv = lfo->convert((step >= 0.0f) ? 0.0f : 1.0f, lfo);
      } else {
         rv = lfo->convert(_analog_square(rv), lfo);
      }

      lfo->value[track] += step;
      if (((lfo->value[track] <= lfo->min) && (step < 0))
          || ((lfo->value[track] >= lfo->max) && (step > 0)))
      {
         lfo->step[track] *= -1.0f;
         lfo->value[track] -= step;
      }
   }
   return rv;
}

float
_aaxLFOGetImpulse(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   float rv = 1.0f;
   if (lfo)
   {
      float max = (lfo->max - lfo->min);
      float step = lfo->step[track];

      assert(max);

      rv = (lfo->value[track] - lfo->min)/max;
      rv = _aaxLFODelay(lfo, rv);
      lfo->compression[track] = 1.0f-rv;

      if (lfo->state & AAX_PURE_WAVEFORM) {
         rv = lfo->convert((step >= 0.0f) ? 0.0f : 1.0f, lfo);
      } else {
         rv = lfo->convert(_analog_impulse(rv), lfo);
      }

      lfo->value[track] += step;
      if (((lfo->value[track] <= lfo->min) && (step < 0))
          || ((lfo->value[track] >= lfo->max) && (step > 0)))
      {
         lfo->step[track] *= -1.0f;
         lfo->value[track] -= step;
      }
   }
   return rv;
}

float
_aaxLFOGetSawtooth(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   float rv = 1.0f;
   if (lfo)
   {
      float max = (lfo->max - lfo->min);
      float step = lfo->step[track];

      assert(max);

      if (lfo->state & AAX_PURE_WAVEFORM)
      {
         rv = _aaxLFOCalculate(lfo, lfo->value[track], track);

         // backwards compatibility
         lfo->value[track] += 0.5f*step;
      }
      else
      {
         rv = (lfo->value[track] - lfo->min)/max;
         rv = _aaxLFODelay(lfo, rv);

         lfo->compression[track] = 1.0f-rv;

         rv = lfo->convert(_analog_sawtooth(rv), lfo);

         lfo->value[track] += step;
      }

      if (lfo->value[track] <= lfo->min) {
         lfo->value[track] += max;
      } else if (lfo->value[track] >= lfo->max) {
         lfo->value[track] -= max;
      }
      lfo->compression[track] = 1.0f - rv;
   }
   return rv;
}

float
_aaxLFOGetCycloid(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   float rv = 1.0f;
   if (lfo)
   {
      float max = (lfo->max - lfo->min);
      float step = lfo->step[track];

      assert(max);

      rv = (lfo->value[track] - lfo->min)/max;
      rv = _aaxLFODelay(lfo, rv);

      lfo->compression[track] = 1.0f-rv;

      if (lfo->state & AAX_PURE_WAVEFORM) {
         rv = lfo->convert(_cycloid(rv), lfo);
      } else {
         rv = lfo->convert(_analog_cycloid(rv), lfo);
      }

      lfo->value[track] += step;
      if (((lfo->value[track] <= lfo->min) && (step < 0))
          || ((lfo->value[track] >= lfo->max) && (step > 0)))
      {
         lfo->step[track] *= -1.0f;
         lfo->value[track] -= step;
      }
   }
   return rv;
}

float
_aaxLFOGetRandomness(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   float rv = 1.0f;
   if (lfo)
   {
      rv = lfo->value[0];

      /* In stereo-link mode the left track (0) provides the data */
      if (track == 0 || lfo->stereo_link == false)
      {
         float alpha = lfo->step[track];
         float olvl = lfo->value[track];

         rv = 0.5*xoroshiro128plus()/(double)INT64_MAX;
         rv = _aaxLFODelay(lfo, rv);

         lfo->compression[track] = 1.0f-rv;

         rv = lfo->convert(rv, lfo);

         rv = alpha*rv + (1.0f-alpha)*olvl;
         lfo->value[track] = rv;
      }
   }
   return rv;
}


float
_aaxLFOGetTimed(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   float rv = 1.0f;
   if (lfo)
   {
      float max = (lfo->max - lfo->min);
      float step = lfo->step[track];

      assert(max);

      rv = (lfo->value[track] - lfo->min)/max;
      rv = _aaxLFODelay(lfo, rv);

      lfo->compression[track] = 1.0f-rv;

      rv = lfo->convert(rv, lfo);

      lfo->value[track] += step;
      if (lfo->value[track] <= lfo->min) {
         lfo->value[track] = lfo->min;
      } else if (lfo->value[track] > lfo->max) {
         lfo->value[track] = lfo->max;
      }
   }

   return rv;
}

float
_aaxLFOGetGainFollow(void* data, void *env, const void *ptr, unsigned track, size_t no_samples)
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   static const float div = 1.0f / (float)0x000FFFFF;
   float rv = 1.0f;
   if (lfo && (env || (ptr && no_samples)))
   {
      float olvl = lfo->value[0];

      /* In stereo-link mode the left track (0) provides the data */
      if (track == 0 || lfo->stereo_link == false)
      {
         float fact = lfo->step[track];
         float lvl;

         lvl = (track % 2) ? 1.05f : 0.95f;	// stereo effect
         if (env)
         {
            _aaxEnvelopeData *genv = (_aaxEnvelopeData*)env;
            lvl = genv->value_total*genv->gain;
         }
         else if (ptr && no_samples)
         {
            float rms, peak;
            _batch_get_average_rms(ptr, no_samples, &rms, &peak);
            lvl = rms*div;
         }

         olvl = lfo->value[track];
         lfo->value[track] = _MINMAX(olvl + fact*(lvl - olvl), 0.01f, 0.99f);
         olvl = lfo->value[track];
      }

      rv = _aaxLFODelay(lfo, olvl);

      rv = lfo->inverse ? 1.0f-rv : rv;
      lfo->compression[track] = 1.0f-rv;

      rv = lfo->convert(rv, lfo);
   }

   return rv;
}

float
_aaxLFOGetCompressor(void* data, UNUSED(void *env), const void *ptr, unsigned track, size_t no_samples)
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   static const float div = 1.0f / (float)0x007fffff;
   float rv = 1.0f;
   if (lfo && ptr && no_samples)
   {
      float oavg = lfo->average[0];
      float olvl = lfo->value[0];
      float gf = 1.0f;
      _aaxLFOData l;

      assert(lfo->gate_threshold > 0.0f);

      /* In stereo-link mode the left track (0) provides the data        */
      /* If the left track nears 0.0f also calculate the orher tracks    */
      /* just to make sure those aren't still producing sound and hence  */
      /* are amplified to extreme values.                                */
      gf = _MIN(powf(oavg/lfo->gate_threshold, 10.0f), 1.0f);
      if (track == 0 || lfo->stereo_link == false)
      {
         float lvl, fact = 1.0f;
         float rms, peak;

         _batch_get_average_rms(ptr, no_samples, &rms, &peak);
         lvl = _MINMAX(rms*div, 0.0f, 1.0f);

         fact = lfo->gate_period;
         olvl = lfo->value[track];
         oavg = lfo->average[track];
         lfo->average[track] = (fact*oavg + (1.0f-fact)*lvl);
         gf = _MIN(powf(oavg/lfo->gate_threshold, 10.0f), 1.0f);

         fact = (lvl > olvl) ? lfo->step[track] : lfo->down[track];
         lfo->value[track] = gf*_MINMAX(olvl + fact*(lvl - olvl), 0.0f, 1.0f);
      }

	// lfo->min == AAX_THRESHOLD
	// lfo->max == AAX_COMPRESSION_RATIO
      assert((1.0f-lfo->max) + lfo->max*olvl);
      rv = gf*_MINMAX(lfo->min/((1.0f-lfo->max) + lfo->max*olvl),1.0f,1000.0f);

      l.min = 0.0f;
      l.max = 1.0f;
      l.inverse = false;
      rv = lfo->convert(rv, &l);

      assert(rv);

      lfo->compression[track] = 1.0f - (1.0f/rv);
      rv = lfo->inverse ? 1.0f/(0.001+0.999f*rv) : rv;
   }

   return rv;
}

float
_aaxEnvelopeGet(_aaxEnvelopeData *env, bool stopped, float *velocity, _aaxEnvelopeData *penv)
{
   float rv = 1.0f;
   if (env)
   {
      unsigned char stage = env->stage;
      if (env->reverse) stage = env->max_stages - stage;

      rv = _MAX(env->value, -1e-2f);

      if (stage < 2) {
         rv *= *velocity;
      }

      if (stage <= env->max_stages)
      {
         float step = env->step[stage];
         if (env->reverse) step = -step;
         if ((env->state == AAX_LFO_EXPONENTIAL) && (rv > LEVEL_60DB))
         {
            float fact;
            if (penv || env->gain) // gain: exponential
            {
               if (rv < 1.0f)
               {
                  if (step < 0.0f) fact = _ln(rv);
                  else fact = 1.0f - _exp(1.0f - rv);
                  rv *= fact;
               }
            }
            else // pitch: logarithmic
            {
               fact = _log(rv);
               rv *= fact;
            }
         }

         if (stopped && !env->sustain) env->value += env->step_finish;
         else env->value += step;

         // If the number-of-steps for this stage is reached go to the next.
         // If the duration of a stage == (uint32_t)-1 then we keep looping
         // the sample until stopped becomes true. After that the rest of the
         // stages get processed.
         if ((++env->pos == env->max_pos[stage])
             || (stopped && env->max_pos[stage] == (uint32_t)-1))
         {
            env->pos = 0;
            stage = ++env->stage;
            if (env->stage > env->max_stages) env->stage = env->max_stages;
            if (env->reverse) stage = env->max_stages - stage;
         }
      }
      else {
         rv = -0.1f;
      }

      // Only the timed-gain-filter supports env->repeat > 1
      if (env->repeat > 1)
      {
         if ((stage == env->max_stages) || (rv < -1e-3f))
         {
            if (!stopped || (env->repeat == AAX_REPEAT-1))
            {
               if (rv < -1e-3f) rv = 0.0f;
               env->value = env->value0;
               env->stage = 0;
               env->pos = 0;
               env->repeat--;
               if (penv)
               {
                  penv->value = penv->value0;
                  penv->stage = 0;
                  penv->stage = 0;
                  penv->pos = 0;
               }
            }
         }
      }

      *velocity *= fabsf((rv > FLT_EPSILON) ? env->value/rv : env->value);
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

/*
 * Periodic waveforms
 * Should start at 0.0f and start to increase over time
 * until a maximum of 1.0 is reached.
 */
float
_linear(float v, _aaxLFOData *lfo)
{
   float depth = (lfo->max-lfo->min);
   float rv;

   if (lfo->inverse) {
      rv = lfo->max - depth*v;
   } else {
      rv = lfo->min + depth*v;
   }
   return rv;
}

float
_squared(float v, _aaxLFOData *lfo)
{
   float depth = (lfo->max-lfo->min);
   float rv;

   if (lfo->inverse) {
      rv = lfo->max - depth*v*v;
   } else {
      rv = lfo->min + depth*v*v;
   }
   return rv;
}

float
_logarithmic(float v, _aaxLFOData *lfo)
{
   float depth = (lfo->max-lfo->min);
   float rv;

   if (lfo->inverse) {
      rv = _log2lin(lfo->max - depth*v);
   } else {
      rv = _log2lin(lfo->min + depth*v);
   }
   return rv;
}

float
_exponential(float v, _aaxLFOData *lfo)
{
   float fact, depth;
   float rv;

   v = _MINMAX(v, 0.0f, 1.0f);
   fact = (expf(v)-1.0f)/(GMATH_E1-1.0f);
   depth = (lfo->max-lfo->min);

   if (lfo->inverse) {
      rv = lfo->max - depth*fact;
   } else {
      rv = lfo->min + depth*fact;
   }

   return rv;
}

float
_exp_distortion(float v, _aaxLFOData *lfo)
{
   float depth = (lfo->max-lfo->min);
   float x = v*v;
   float rv;

   if (lfo->inverse) {
      rv = lfo->max - 0.5f*depth*(x*x-x+v);
   } else {
      rv = lfo->min + 0.5f*depth*(x*x-x+v);
   }
   return rv;
}

/*
 * Analog equivalents of standard waveforms
 * Should start at 0.0f and start to increase over time
 * until a maximum of 1.0 is reached.
 *
 * domain for x: 0.0 .. 1.0
 */
static inline float
_analog_sawtooth(float x)
{
   float y = 1.263f*fmodf(x, 1.0f);
   return sinf(tanf(y));
}

static inline float
_analog_square(float x)
{
   float y = GMATH_PI*(1.0f-x);
   float y2 = y*y;
   return cosf(atanf(y2*y2));
}

static inline float
_analog_triangle(float x)
{
   float y = GMATH_PI*fmodf(0.5f*x, 1.0f);
   return tanf(sinf(y))/1.557f;
}

static inline float
_analog_sin(float x)
{
   float y = fmodf(x-0.5f, 1.0f);
   return 0.5f + 2.0f*(y - y*fabsf(y));
}

static float
_analog_cycloid(float x)
{
   float y = 1.263f*fmodf(x, 1.0f);
   return 0.5f-0.5f*cosf(tanf(y));
}

static float
_cycloid(float x)
{
   float y = fmodf(x, 1.0f);
   return 1.0f-sqrtf(1.0f-y*y);
}

static float
_analog_impulse(float x)
{
   float y = 2.0f*GMATH_2PI*(1.0f-x);
   return cos(atan(y*y));
}

