/*
 * Copyright 2005-2018 by Erik Hofman.
 * Copyright 2009-2018 by Adalin B.V.
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
#endif

#include <base/geometry.h>
#include <arch.h>

#include "common.h"
#include "lfo.h"


_aaxLFOData*
_lfo_create()
{
   return calloc(1, sizeof(_aaxLFOData));
}

void
_lfo_destroy(void *data)
{
   _aaxLFOData *lfo = data;
   if (lfo) lfo->envelope = AAX_FALSE;
   free(lfo);
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
      case AAX_SAWTOOTH_WAVE:
         lfo->get = _aaxLFOGetSawtooth;
         break;
      case AAX_ENVELOPE_FOLLOW:
         lfo->get = _aaxLFOGetGainFollow;
         lfo->envelope = AAX_TRUE;
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
   constant = (depth > 0.05f) ? AAX_FALSE : AAX_TRUE;
   
   lfo->min = (range * offset + min)*fs;
   lfo->max = lfo->min + depth;

   if (!constant)
   {
      float sign, step;
      int t;
      for (t=0; t<_AAX_MAX_SPEAKERS; t++)
      {
         // slowly work towards the new settings
         step = lfo->step[t];
         sign = step ? (step/fabsf(step)) : 1.0f;

         lfo->step[t] = 2.0f*sign * lfo->f;
         lfo->step[t] *= (lfo->max - lfo->min);
         lfo->step[t] /= lfo->period_rate;

         if ((lfo->value[t] == 0) || (lfo->value[t] < lfo->min)) {
            lfo->value[t] = lfo->min;
         } else if (lfo->value[t] > lfo->max) {
            lfo->value[t] = lfo->max;
         }

         switch (lfo->state & ~AAX_INVERSE)
         {
         case AAX_SAWTOOTH_WAVE:
            lfo->step[t] *= 0.5f;
            break;
         case AAX_ENVELOPE_FOLLOW:
         {
            lfo->step[t] = ENVELOPE_FOLLOW_STEP_CVT(lfo->f);
//          lfo->value[t] /= lfo->max;
            lfo->value[t] = 0.0f;
            break;
         }
         default:
            break;
         }
      }
   }
   else
   {
      int t;
      for (t=0; t<_AAX_MAX_SPEAKERS; t++) {
         lfo->value[t] = lfo->min;
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
      rv = lfo->convert(lfo->value[track], 1.0f);
      rv = lfo->inv ? lfo->max-rv : rv;
      lfo->compression[track] = rv;
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

      rv = lfo->convert(lfo->value[track], 1.0f);
      rv = lfo->inv ? lfo->max-(rv-lfo->min) : rv;

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
      float v = lfo->value[track];

      lfo->value[track] += step;
      if (((lfo->value[track] <= lfo->min) && (step < 0))
          || ((lfo->value[track] >= lfo->max) && (step > 0)))
      {
         lfo->step[track] *= -1.0f;
         lfo->value[track] -= step;
      }
      v = (v - lfo->min)/max;

      rv = lfo->convert(_fast_sin1(v), max);
      rv = lfo->inv ? lfo->max-rv : lfo->min+rv;
      lfo->compression[track] = 1.0f - rv;
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
      float step = lfo->step[track];

      rv = lfo->convert((step >= 0.0f ) ? lfo->max-lfo->min : 0, 1.0f);
      rv = lfo->inv ? lfo->max-rv : lfo->min+rv;

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

      rv = lfo->convert(lfo->value[track], 1.0f);
      rv = lfo->inv ? lfo->max-(rv-lfo->min) : rv;

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
_aaxLFOGetGainFollow(void* data, void *env, const void *ptr, unsigned track, size_t num)
{
   _aaxLFOData* lfo = (_aaxLFOData*)data;
   static const float div = 1.0f / (float)0x000fffff;
   float rv = 1.0f;
   if (lfo && ptr && num)
   {
      float olvl = lfo->value[0];

      /* In stereo-link mode the left track (0) provides the data */
      if (track == 0 || lfo->stereo_lnk == AAX_FALSE)
      {
         float lvl, fact;

         if (!env)
         {
            float rms, peak;
            _batch_get_average_rms(ptr, num, &rms, &peak);
            lvl = _MINMAX(rms*div, 0.0f, 1.0f);
         }
         else
         {  
            _aaxEnvelopeData *genv = (_aaxEnvelopeData*)env;
            lvl = genv->value_total;
         }

         olvl = lfo->value[track];
         fact = lfo->step[track];
         lfo->value[track] = _MINMAX(olvl + fact*(lvl - olvl), 0.01f, 0.99f);
         olvl = lfo->value[track];
      }

      rv = lfo->convert(olvl, lfo->max-lfo->min);
      rv = lfo->inv ? lfo->max-rv : lfo->min+rv;
      lfo->compression[track] = 1.0f - rv;
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
      rv = gf*_MINMAX(lfo->min/((1.0f-lfo->max) + lfo->max*olvl), 1.0f,1000.0f);

      rv = lfo->convert(rv, 1.0f);
      lfo->compression[track] = 1.0f - (1.0f/rv);
//    rv = lfo->inv ? 1.0f/(1.0f - 0.999f*rv) : 1.0f - rv;
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
      unsigned int stage = env->stage;
      rv = env->value;
      if (stage < env->max_stages)
      {
         env->value += env->step[stage];

         if (stage > 3) {
            env->pos++;
         }
         else
         {
            env->ctr += *velocity;
            if (env->ctr >= 1.0f)
            {
               env->pos++;
               env->ctr -= 1.0f;
            }
         }

         // If the number-of-steps for this stage is reached go to the next.
         // If the duration of a stage == (uint32_t)-1 then we keep looping
         // the sample until stopped becomes true. After that the rest of the
         // stages get processed.
         if ((env->pos == env->max_pos[stage])
             || (env->max_pos[stage] == (uint32_t)-1 && stopped))
         {
            env->pos = 0;
            env->stage++;
         }
      }

      // Only the timed-gain-filter supports env->repeat > 1
      if ((env->repeat > 1) &&
          ((env->stage == env->max_stages) || (rv < -1e-3f)))
      {
         if (rv < -1e-3f) rv = 0.0f;
         env->value = env->value0;
         env->stage = 0;
         env->stage = 0;
         env->pos = 0;
         env->ctr = 0.0f;
         env->repeat--;
         if (penv)
         {
            penv->value = penv->value0;
            penv->stage = 0;
            penv->stage = 0;
            penv->pos = 0;
            penv->ctr = 0.0f;
         }
      }

      *velocity = env->value;
   }
   return rv;
}

