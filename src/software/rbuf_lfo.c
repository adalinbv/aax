/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <ringbuffer.h>
#include "rbuf_int.h"

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
_aaxRingBufferLFOGetFixedValue(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxRingBufferLFOData* lfo = (_aaxRingBufferLFOData*)data;
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
_aaxRingBufferLFOGetTriangle(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxRingBufferLFOData* lfo = (_aaxRingBufferLFOData*)data;
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
   float y = fmodf(x+0.5f, 2.0f) - 1.0f;
   return 0.5f + 2.0f*(y - y*fabsf(y));
}

float
_aaxRingBufferLFOGetSine(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxRingBufferLFOData* lfo = (_aaxRingBufferLFOData*)data;
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
_aaxRingBufferLFOGetSquare(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxRingBufferLFOData* lfo = (_aaxRingBufferLFOData*)data;
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
_aaxRingBufferLFOGetSawtooth(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t end))
{
   _aaxRingBufferLFOData* lfo = (_aaxRingBufferLFOData*)data;
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
_aaxRingBufferLFOGetGainFollow(void* data, UNUSED(void *env), UNUSED(const void *ptr), unsigned track, UNUSED(size_t num))
{
   _aaxRingBufferLFOData* lfo = (_aaxRingBufferLFOData*)data;
   static const float div = 1.0f / (float)0x000fffff;
   float rv = 1.0f;
   if (lfo && ptr && num)
   {
      float olvl = lfo->value[0];

      /* In stereo-link mode the left track (0) provides the data */
      if (track == 0 || lfo->stereo_lnk == AAX_FALSE)
      {
         float lvl, fact;
         float rms, peak;

         _batch_get_average_rms(ptr, num, &rms, &peak);
         lvl = _MINMAX(rms*div, 0.0f, 1.0f);

         olvl = lfo->value[track];
         fact = lfo->step[track];
         lfo->value[track] = _MINMAX(olvl + fact*(lvl - olvl), 0.01f, 0.99f);
      }

      rv = lfo->convert(olvl, lfo->max-lfo->min);
      rv = lfo->inv ? lfo->max-rv : lfo->min+rv;
      lfo->compression[track] = 1.0f - rv;
   }
   return rv;
}

float
_aaxRingBufferLFOGetCompressor(void* data, UNUSED(void *env), const void *ptr, unsigned track, size_t num)
{
   _aaxRingBufferLFOData* lfo = (_aaxRingBufferLFOData*)data;
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
      rv = lfo->inv ? 1.0f/(1.0f - 0.999f*rv) : 1.0f - rv;
   }

   return rv;
}

float
_aaxRingBufferEnvelopeGet(_aaxRingBufferEnvelopeData *env, char stopped, float *velocity)
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

         if ((env->pos == env->max_pos[stage])
             || (env->max_pos[stage] == (uint32_t)-1 && stopped))
         {
            env->pos = 0;
            env->stage++;
         }
      }

      if ((env->repeat > 1) &&
          ((env->stage == env->max_stages) || (rv < -1e-3f)))
      {
         if (rv < -1e-3f) rv = 0.0f;
         env->stage = 0;
         env->value = 0;
         env->stage = 0;
         env->pos = 0;
         env->repeat--;
      }

      *velocity = env->value;
   }
   return rv;
}

