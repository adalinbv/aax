/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
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
