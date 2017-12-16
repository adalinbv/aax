/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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

#include <math.h>
#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
#endif

#include <base/types.h>
#include <base/geometry.h>

#include "common.h"

void destroy(void *ptr) { free(ptr); }

float _lin(float v) { return v; }
float _square(float v) { return v*v; }
float _lin2log(float v) { return log10f(v); }
float _log2lin(float v) { return powf(10.0f,v); }
float _lin2db(float v) { return 20.0f*log10f(v); }
float _db2lin(float v) { return _MINMAX(powf(10.0f,v/20.0f),0.0f,10.0f); }
float _rad2deg(float v) { return v*GMATH_RAD_TO_DEG; }
float _deg2rad(float v) { return fmodf(v, 360.0f)*GMATH_DEG_TO_RAD; }
float _cos_deg2rad_2(float v) { return cosf(_deg2rad(v)/2); }
float _2acos_rad2deg(float v) { return 2*_rad2deg(acosf(v)); }
float _cos_2(float v) { return cosf(v/2); }
float _2acos(float v) { return 2*acosf(v); }

int
_lfo_set_timing(_aaxRingBufferLFOData *lfo)
{
   float min = lfo->min_sec;
   float range = lfo->range_sec;
   float depth = lfo->depth;
   float offset = lfo->offset;
   float fs = lfo->fs;
   int constant;

   if ((offset + depth) > 1.0f) {
      depth = 1.0f - offset;
   }
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
            float fact = lfo->f;
            lfo->value[t] /= lfo->max;
            lfo->step[t] = ENVELOPE_FOLLOW_STEP_CVT(fact);
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

