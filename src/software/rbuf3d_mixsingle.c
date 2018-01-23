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

/*
 * 1:N ringbuffer mixer functions.
 */

/*
 * Sources:
 * http://en.wikipedia.org/wiki/Doppler_effect
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "rbuf_int.h"

/* Forward declartations */
static _aaxRingBufferDistFn _aaxRingBufferDistNone;
static _aaxRingBufferDistFn _aaxRingBufferDistInvExp;
static _aaxRingBufferPitchShiftFn _aaxRingBufferDopplerShift;

static _aaxRingBufferDistFn _aaxRingBufferALDistInv;
static _aaxRingBufferDistFn _aaxRingBufferALDistInvClamped;
static _aaxRingBufferDistFn _aaxRingBufferALDistLin;
static _aaxRingBufferDistFn _aaxRingBufferALDistLinClamped;
static _aaxRingBufferDistFn _aaxRingBufferALDistExp;
static _aaxRingBufferDistFn _aaxRingBufferALDistExpClamped;
/**
 * rpos: emitter position relative to the listener
 * dist_fact: the factor that translates the distance into meters/feet/etc.
 * speaker: the parents speaker positions
 * p2d: the emitters 2d properties structure
 * info: the mixers info structure
 */
void
_aaxSetupSpeakersFromDistanceVector(vec3f_t  rpos, float dist_fact,
                                    vec4f_t *speaker, _aax2dProps *p2d,
                                    const _aaxMixerInfo* info)
{
   unsigned int pos, i, t;
   float dp, offs, fact;

   switch (info->mode)
   {
   case AAX_MODE_WRITE_HRTF:
      for (t=0; t<info->no_tracks; t++)
      {
         for (i=0; i<3; i++)
         {
            dp = vec3fDotProduct(&speaker[3*t+i].v3, &rpos);
            dp *= speaker[t].v4[3];
            p2d->speaker[t].v4[i] = dp * dist_fact;		/* -1 .. +1 */

            offs = info->hrtf[HRTF_OFFSET].v4[i];
            fact = info->hrtf[HRTF_FACTOR].v4[i];

            pos = _AAX_MAX_SPEAKERS + 3*t + i;
            dp = vec3fDotProduct(&speaker[pos].v3, &rpos);
            p2d->hrtf[t].v4[i] = _MAX(offs + dp*fact, 0.0f);
         }
      }
      break;
   case AAX_MODE_WRITE_SURROUND:
      for (t=0; t<info->no_tracks; t++)
      {
#ifdef USE_SPATIAL_FOR_SURROUND
         dp = vec3fDotProduct(&speaker[t].v3, &rpos);
         dp *= speaker[t].v4[3];

         p2d->speaker[t].v4[0] = 0.5f + dp*dist_fact;
#else
         vec4fMulvec4(&p2d->speaker[t], &speaker[t], &rpos);
         vec4fScalarMul(&p2d->speaker[t], &p2d->speaker[t], dist_fact);
#endif
         i = DIR_UPWD;
         do				/* skip left-right and back-front */
         {
            offs = info->hrtf[HRTF_OFFSET].v4[i];
            fact = info->hrtf[HRTF_FACTOR].v4[i];

            pos = _AAX_MAX_SPEAKERS + 3*t + i;
            dp = vec3fDotProduct(&speaker[pos].v3, &rpos);
            p2d->hrtf[t].v4[i] = _MAX(offs + dp*fact, 0.0f);
         }
         while(0);
      }
      break;
   case AAX_MODE_WRITE_SPATIAL:
      for (t=0; t<info->no_tracks; t++)
      {						/* speaker == sensor_pos */
         dp = vec3fDotProduct(&speaker[t].v3, &rpos);
         dp *= speaker[t].v4[3];

         p2d->speaker[t].v4[0] = 0.5f + dp*dist_fact;
      }
      break;
   default: /* AAX_MODE_WRITE_STEREO */
      for (t=0; t<info->no_tracks; t++)
      {
         vec3fMulvec3(&p2d->speaker[t].v3, &speaker[t].v3, &rpos);
         vec4fScalarMul(&p2d->speaker[t], &p2d->speaker[t], dist_fact);
      }
   }
}

/* -------------------------------------------------------------------------- */

_aaxRingBufferDistFn *_aaxRingBufferDistanceFn[AAX_DISTANCE_MODEL_MAX] =
{
   (_aaxRingBufferDistFn *)&_aaxRingBufferDistNone,
   (_aaxRingBufferDistFn *)&_aaxRingBufferDistInvExp
};

#define AL_DISTANCE_MODEL_MAX AAX_AL_DISTANCE_MODEL_MAX-AAX_AL_INVERSE_DISTANCE
_aaxRingBufferDistFn *_aaxRingBufferALDistanceFn[AL_DISTANCE_MODEL_MAX] =
{
   (_aaxRingBufferDistFn *)&_aaxRingBufferALDistInv,
   (_aaxRingBufferDistFn *)&_aaxRingBufferALDistInvClamped,
   (_aaxRingBufferDistFn *)&_aaxRingBufferALDistLin,
   (_aaxRingBufferDistFn *)&_aaxRingBufferALDistLinClamped,
   (_aaxRingBufferDistFn *)&_aaxRingBufferALDistExp,
   (_aaxRingBufferDistFn *)&_aaxRingBufferALDistExpClamped
};

_aaxRingBufferPitchShiftFn *_aaxRingBufferDopplerFn[] =
{
   (_aaxRingBufferPitchShiftFn *)&_aaxRingBufferDopplerShift
};

static float
_aaxRingBufferDistNone(UNUSED(float dist), UNUSED(float ref_dist), UNUSED(float max_dist), UNUSED(float rolloff), UNUSED(float vsound), UNUSED(float Q))
{
   return 1.0f;
}


/**
 * http://www.engineeringtoolbox.com/outdoor-propagation-sound-d_64.html
 *
 * Lp = Lw + 10 log(Q/(4π r2) + 4/R)  (1b)
 *
 * where
 *
 * Lp = sound pressure level (dB)
 * Lw = sound power level source in decibel (dB)
 * Q = Q coefficient 
 *     1 if uniform spherical
 *     2 if uniform half spherical (single reflecting surface)
 *     4 if uniform radiation over 1/4 sphere (two reflecting surfaces, corner)
 * r = distance from source   (m)
 * R = room constant (m2)
 */
static float
_aaxRingBufferDistInvExp(float dist, float ref_dist, UNUSED(float max_dist), float rolloff, UNUSED(float vsound), UNUSED(float Q))
{
#if 1
   float fraction = 0.0f, gain = 1.0f;
   if (ref_dist) fraction = _MAX(dist, 0.01f) / _MAX(ref_dist, 0.01f);
   if (fraction) gain = powf(fraction, -rolloff);
   return gain;
#else
   return powf(dist/ref_dist, -rolloff);
#endif
}

static float
_aaxRingBufferDopplerShift(float vs, float ve, float vsound)
{
#if 1
   float vse, rv;

   /* relative speed */
   vse = _MIN(ve, vsound) - _MIN(vs, vsound);
   rv =  vsound/_MAX(vsound - vse, 1.0f);

   return rv;
#else
   float vss, ves;
   vss = vsound - _MIN(vs, vsound);
   ves = _MAX(vsound - _MIN(ve, vsound), 1.0f);
   return vss/ves;
#endif
}


/* --- OpenAL support --- */

static float
_aaxRingBufferALDistInv(float dist, float ref_dist, UNUSED(float max_dist), float rolloff, UNUSED(float vsound), UNUSED(float Q))
{
   float gain = 1.0f;
   float denom = ref_dist + rolloff * (dist - ref_dist);
   if (denom) gain = ref_dist/denom;
   return gain;
}

static float
_aaxRingBufferALDistInvClamped(float dist, float ref_dist, float max_dist, float rolloff, UNUSED(float vsound), UNUSED(float Q))
{
   float gain = 1.0f;
   float denom;
   dist = _MAX(dist, ref_dist);
   dist = _MIN(dist, max_dist);
   denom = ref_dist + rolloff * (dist - ref_dist);
   if (denom) gain = ref_dist/denom;
   return gain;
}

static float
_aaxRingBufferALDistLin(float dist, float ref_dist, float max_dist, float rolloff, UNUSED(float vsound), UNUSED(float Q))
{
   float gain = 1.0f;
   float denom = max_dist - ref_dist;
   if (denom) gain = (1-rolloff)*(dist-ref_dist)/denom;
   return gain;
}

static float
_aaxRingBufferALDistLinClamped(float dist, float ref_dist, float max_dist, float rolloff, UNUSED(float vsound), UNUSED(float Q))
{
   float gain = 1.0f;
   float denom = max_dist - ref_dist;
   dist = _MAX(dist, ref_dist);
   dist = _MIN(dist, max_dist);
   if (denom) gain = (1-rolloff)*(dist-ref_dist)/denom;
   return gain;
}

static float
_aaxRingBufferALDistExp(float dist, float ref_dist, UNUSED(float max_dist), float rolloff, UNUSED(float vsound), UNUSED(float Q))
{
   float fraction = 0.0f, gain = 1.0f;
   if (ref_dist) fraction = dist / ref_dist;
   if (fraction) gain = powf(fraction, -rolloff);
   return gain;
}

static float
_aaxRingBufferALDistExpClamped(float dist, float ref_dist, float max_dist, float rolloff, UNUSED(float vsound), UNUSED(float Q))
{
   float fraction = 0.0f, gain = 1.0f;
   dist = _MAX(dist, ref_dist);
   dist = _MIN(dist, max_dist);
   if (ref_dist) fraction = dist / ref_dist;
   if (fraction) gain = powf(fraction, -rolloff);

   return gain;
}
