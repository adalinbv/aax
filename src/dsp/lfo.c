/*
 * Copyright 2005-2020 by Erik Hofman.
 * Copyright 2009-2020 by Adalin B.V.
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
# include <malloc.h>
# include <string.h>
#endif
#include <assert.h>

#include <base/geometry.h>
#include <base/random.h>
#include <arch.h>

#include "common.h"
#include "lfo.h"

float _linear(float v, float min, float depth) {
   return min + depth*v;
}

float _squared(float v, float min, float depth) {
   return min + depth*v*v;
}

float _logarithmic(float v, float min, float depth) {
   return _log2lin(min + depth*v);
}

float _exponential(float v, float min, float depth) {
   return min + depth*(expf(v)-1.0f)/(GMATH_E1-1.0f);
}

float _exp_distortion(float v, float min, float depth) {
   float x = v*v;
   return min + 0.5f*depth*(x*x-x+v);
}

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
      lfo->envelope = AAX_FALSE;
      _aax_aligned_free(lfo);
   }
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
      dlfo->inv = slfo->inv;
      dlfo->envelope = slfo->envelope;
      dlfo->stereo_lnk = slfo->stereo_lnk;
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
   int rv = AAX_TRUE;
   if (!constant)
   {
      switch (lfo->state & ~AAX_INVERSE)
      {
      case AAX_CONSTANT_VALUE: /* equals to AAX_TRUE */
         lfo->get = _aaxLFOGetFixedValue;
         break;
      case AAX_TRIANGLE_WAVE:
         lfo->get = _aaxLFOGetTriangle;
         break;
      case AAX_SINE_WAVE:
         lfo->get = _aaxLFOGetSine;
         break;
      case AAX_SQUARE_WAVE:
         lfo->get = _aaxLFOGetSquare;
         break;
      case AAX_IMPULSE_WAVE:
         lfo->get = _aaxLFOGetImpulse;
         break;
      case AAX_SAWTOOTH_WAVE:
         lfo->get = _aaxLFOGetSawtooth;
         break;
      case AAX_RANDOMNESS:
         lfo->get = _aaxLFOGetRandomness;
         break;
      case AAX_ENVELOPE_FOLLOW:
      case AAX_ENVELOPE_FOLLOW_LOG:
      case AAX_ENVELOPE_FOLLOW_MASK:
         lfo->get = _aaxLFOGetGainFollow;
         lfo->envelope = AAX_TRUE;
         break;
      case AAX_TIMED_TRANSITION:
      case (AAX_TIMED_TRANSITION|AAX_ENVELOPE_FOLLOW_LOG):
         lfo->get = _aaxLFOGetTimed;
         break;
      default:
         rv = AAX_FALSE;
         break;
      }
   }
   else {
      lfo->get = _aaxLFOGetFixedValue;
   }

   return rv;
}

int
_lfo_set_timing(_aaxLFOData *lfo)
{
   float min = lfo->min_sec;
   float range = lfo->max_sec - lfo->min_sec;
   float depth = lfo->depth;
   float offset = lfo->offset;
   float fs = lfo->fs;
   int constant;

   depth *= range * fs;
   constant = (depth > 0.01f) ? AAX_FALSE : AAX_TRUE;

   lfo->min = (range * offset + min)*fs;
   lfo->max = lfo->min + depth;

#if 0
 printf("offset: %f, range: %f, min: %f, fs: %f\n", offset, range, min, fs);
 printf("lfo min: %f, max: %f\n", lfo->min, lfo->max);
#endif

   if (!constant)
   {
      float sign, step;
      int t;
      for (t=0; t<_AAX_MAX_SPEAKERS; t++)
      {
         if (!lfo->stereo_lnk) {
            lfo->value0[t] = (t % 2)*1e9f;
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

         switch (lfo->state & ~AAX_INVERSE)
         {
         case AAX_SAWTOOTH_WAVE:
            lfo->step[t] *= 0.5f;
            break;
         case AAX_ENVELOPE_FOLLOW:
         case AAX_ENVELOPE_FOLLOW_LOG:
         {
            lfo->step[t] = ENVELOPE_FOLLOW_STEP_CVT(lfo->f);
            lfo->value0[t] = 0.0f;
            break;
         }
         case AAX_RANDOMNESS:
         {
            float fs = lfo->period_rate;
            float fc = lfo->f;
            float cfc = cosf(GMATH_2PI*fc/fs);
            lfo->step[t] = -1.0f + cfc + sqrtf(cfc*cfc -4.0f*cfc + 3.0f);
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
         lfo->value[t] = lfo->value0[t] = lfo->min;
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


static inline float
_aaxLFODelay(_aaxLFOData* lfo, float rv)
{
   float delay = fabsf(lfo->delay);
   if (lfo->dt < delay)
   {
       float f;

       lfo->dt += 1.0f/lfo->period_rate;
       f = lfo->dt/delay;
       f = f*f;

       if (lfo->delay < 0.0f) rv = 1.0f - (1.0f - rv)*f;	// pitch
       else rv *= f;						// gain
   }
   return rv;
}

static inline float
_aaxLFOCalculate(_aaxLFOData *lfo, float val, unsigned track)
{
   float max = (lfo->max - lfo->min);
   float rv;

   assert(max);

   rv = (val - lfo->min)/max;
   rv = lfo->inv ? 1.0f-rv : rv;

   rv = _aaxLFODelay(lfo, rv);

   lfo->compression[track] = 1.0f - rv;
   rv = lfo->convert(rv, lfo->min, max);

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
 * lfo->inv is an internal parameter that defines the counting direction.
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
      rv = lfo->value[track];
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

      rv = _aaxLFOCalculate(lfo, lfo->value[track], track);

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


static float
_fast_sin1(float x)
{
#if 0
   float y = fmodf(x, 2.0f) - 1.0f;
   return -4.0f*(y - y*fabsf(y));
#else
   float y = fmodf(x+0.5f, 2.0f) - 1.0f;
   return 0.5f + 2.0f*(y - y*fabsf(y));
#endif
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
      rv = lfo->inv ? 1.0f-rv : rv;

      rv = _aaxLFODelay(lfo, rv);

      lfo->compression[track] = 1.0f-rv;
      rv = lfo->convert(_fast_sin1(rv), lfo->min, max);

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
      rv = lfo->inv ? 1.0f-rv : rv;

      rv = _aaxLFODelay(lfo, rv);

      lfo->compression[track] = 1.0f-rv;
      rv = lfo->convert((step >= 0.0f) ? 1.0f : 0.0f, lfo->min, max);

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
_aaxLFOGetImpulse(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   float rv = 1.0f;
   if (lfo)
   {
      float max = (lfo->max - lfo->min);
      float step = lfo->step[track];

      assert(max);

      if (step > 0) step *= 10.0f;	// 10% duty cycle
      else step *= 0.9f;

      rv = (lfo->value[track] - lfo->min)/max;
      rv = lfo->inv ? 1.0f-rv : rv;

      rv = _aaxLFODelay(lfo, rv);

      lfo->compression[track] = 1.0f-rv;
      rv = lfo->convert((step >= 0.0f) ? 1.0f : 0.0f, lfo->min, max);

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
_aaxLFOGetSawtooth(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   float rv = 1.0f;
   if (lfo)
   {
      float max = (lfo->max - lfo->min);
      float step = lfo->step[track];

      assert(max);

      rv = _aaxLFOCalculate(lfo, lfo->value[track], track);

      lfo->value[track] += step;
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
_aaxLFOGetRandomness(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   float rv = 1.0f;
   if (lfo)
   {
      rv = lfo->value[0];

      /* In stereo-link mode the left track (0) provides the data */
      if (track == 0 || lfo->stereo_lnk == AAX_FALSE)
      {
         float max = (lfo->max - lfo->min);
         float alpha = lfo->step[track];
         float olvl = lfo->value[track];

         rv = 0.5*xoroshiro128plus()/(double)INT64_MAX;
         rv = lfo->inv ? 1.0f-rv : rv;

         rv = _aaxLFODelay(lfo, rv);

         lfo->compression[track] = 1.0f-rv;
         rv = lfo->convert(rv, lfo->min, max);

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
      rv = lfo->inv ? 1.0f-rv : rv;

      rv = _aaxLFODelay(lfo, rv);

      lfo->compression[track] = 1.0f-rv;
      rv = lfo->convert(rv, lfo->min, max);

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
_aaxLFOGetGainFollow(void* data, void *env, const void *ptr, unsigned track, size_t num)
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   static const float div = 1.0f / (float)0x000FFFFF;
   float rv = 1.0f;
   if (lfo && ptr && num)
   {
      float max = (lfo->max - lfo->min);
      float olvl = lfo->value[0];

      assert(max);

      /* In stereo-link mode the left track (0) provides the data */
      if (track == 0 || lfo->stereo_lnk == AAX_FALSE)
      {
         float fact = lfo->step[track];
         float lvl;

         lvl = (track % 2) ? 1.05f : 0.95f;	// stereo effect
         if (!env)
         {
            float rms, peak;
            _batch_get_average_rms(ptr, num, &rms, &peak);
            lvl = rms*div;
         }
         else
         {
            _aaxEnvelopeData *genv = (_aaxEnvelopeData*)env;
            lvl = genv->value_total/genv->value_max;
         }

         olvl = lfo->value[track];
         lfo->value[track] = _MINMAX(olvl + fact*(lvl - olvl), 0.01f, 0.99f);
         olvl = lfo->value[track];
      }

      rv = olvl;
      rv = lfo->inv ? 1.0f-rv : rv;

      rv = _aaxLFODelay(lfo, rv);

      lfo->compression[track] = 1.0f-rv;
      rv = lfo->convert(rv, lfo->min, max);
   }

   return rv;
}

float
_aaxLFOGetCompressor(void* data, UNUSED(void *env), const void *ptr, unsigned track, size_t num)
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   static const float div = 1.0f / (float)0x007fffff;
   float rv = 1.0f;
   if (lfo && ptr && num)
   {
      float oavg = lfo->average[0];
      float olvl = lfo->value[0];
      float gf = 1.0f;

      assert(lfo->gate_threshold > 0.0f);

      /* In stereo-link mode the left track (0) provides the data        */
      /* If the left track nears 0.0f also calculate the orher tracks    */
      /* just to make sure those aren't still producing sound and hence  */
      /* are amplified to extreme values.                                */
      gf = _MIN(powf(oavg/lfo->gate_threshold, 10.0f), 1.0f);
      if (track == 0 || lfo->stereo_lnk == AAX_FALSE)
      {
         float lvl, fact = 1.0f;
         float rms, peak;

         _batch_get_average_rms(ptr, num, &rms, &peak);
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
      rv = lfo->convert(rv, 0.0f, 1.0f);

      assert(rv);
      lfo->compression[track] = 1.0f - (1.0f/rv);
      rv = lfo->inv ? 1.0f/(0.001+0.999f*rv) : rv;
   }

   return rv;
}

float
_aaxEnvelopeGet(_aaxEnvelopeData *env, char stopped, float *velocity, _aaxEnvelopeData *penv)
{
   float rv = 1.0f;
   if (env)
   {
      unsigned char stage = env->stage;
      rv = _MAX(env->value, -1e-2f);

      if (stage < 2) {
         rv *= *velocity;
      }

      if (stage <= env->max_stages)
      {
         float step = env->step[stage];
         float fact = 1.0f;

         if (fabsf(step) > LEVEL_128DB && env->state & AAX_ENVELOPE_FOLLOW_MASK)
         {
             if (rv > 1.0f) {
                fact = _MIN(powf(rv, GMATH_E1), GMATH_E1);
             } else if (rv > 0.0f) {
                fact = powf(rv, GMATH_1_E1);
             }
             if (step > 0.0f) fact = 1.0f/fact;
         }

         if (stopped && !env->sustain) env->value += env->step_finish*fact;
         else env->value += step*fact;

         // If the number-of-steps for this stage is reached go to the next.
         // If the duration of a stage == (uint32_t)-1 then we keep looping
         // the sample until stopped becomes true. After that the rest of the
         // stages get processed.
         if ((++env->pos == env->max_pos[stage])
             || (stopped && env->max_pos[stage] == (uint32_t)-1))
         {
            env->pos = 0;
            stage = ++env->stage;
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

      *velocity *= fabsf((rv != 0.0f) ? env->value/rv : env->value);
   }
   return rv;
}

