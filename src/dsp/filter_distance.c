/*
 * Copyright 2007-2018 by Erik Hofman.
 * Copyright 2009-2018 by Adalin B.V.
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

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
#endif

#include <aax/aax.h>

#include <base/types.h>		/* for rintf */
#include <base/gmath.h>

#include "common.h"
#include "filters.h"
#include "api.h"

// https://en.wikipedia.org/wiki/Attenuation#Attenuation_coefficient
// https://physics.stackexchange.com/questions/87751/do-low-frequency-sounds-really-carry-longer-distances

static aaxFilter
_aaxDistanceFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, 1);
   aaxFilter rv = NULL;

   if (flt)
   {
      flt->slot[0]->data = *(void**)&_aaxDistanceFn[1];
      _aaxSetDefaultFilter3d(flt->slot[0], flt->pos, 0);
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxDistanceFilterDestroy(_filter_t* filter)
{
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxDistanceFilterSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = NULL;

   if ((state & ~AAX_DISTANCE_DELAY) < AAX_AL_DISTANCE_MODEL_MAX)
   {
      int pos = state & ~AAX_DISTANCE_DELAY;
      if ((pos >= AAX_AL_INVERSE_DISTANCE)
          && (pos < AAX_AL_DISTANCE_MODEL_MAX))
      {
         pos -= AAX_AL_INVERSE_DISTANCE;
         filter->slot[0]->state = state;
         filter->slot[0]->data = *(void**)&_aaxALDistanceFn[pos];
      }
      else if (pos < AAX_DISTANCE_MODEL_MAX)
      {
         filter->slot[0]->state = state;
         filter->slot[0]->data = *(void**)&_aaxDistanceFn[pos];
      }
      else _aaxErrorSet(AAX_INVALID_PARAMETER);
   }
   else _aaxErrorSet(AAX_INVALID_PARAMETER);
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewDistanceFilterHandle(const aaxConfig config, enum aaxFilterType type, UNUSED(_aax2dProps* p2d), _aax3dProps* p3d)
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _filter_t* rv = _aaxFilterCreateHandle(info, type, 1);

   if (rv)
   {
      unsigned int size = sizeof(_aaxFilterInfo);

      memcpy(rv->slot[0], &p3d->filter[rv->pos], size);
      rv->slot[0]->data = *(void**)&_aaxDistanceFn[1];

      rv->state = p3d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxDistanceFilterSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxDistanceFilterGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxDistanceFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxDistanceRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.1f, 0.0f, 0.0f }, { MAXFLOAT, MAXFLOAT, 1.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxDistanceRange[slot].min[param],
                       _aaxDistanceRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxDistanceFilter =
{
   AAX_TRUE,
   "AAX_distance_filter", 1.0f,
   (_aaxFilterCreate*)&_aaxDistanceFilterCreate,
   (_aaxFilterDestroy*)&_aaxDistanceFilterDestroy,
   (_aaxFilterSetState*)&_aaxDistanceFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewDistanceFilterHandle,
   (_aaxFilterConvert*)&_aaxDistanceFilterSet,
   (_aaxFilterConvert*)&_aaxDistanceFilterGet,
   (_aaxFilterConvert*)&_aaxDistanceFilterMinMax
};

/* Forward declartations */
static _aaxDistFn _aaxDistNone;
static _aaxDistFn _aaxDistInvExp;

static _aaxDistFn _aaxALDistInv;
static _aaxDistFn _aaxALDistInvClamped;
static _aaxDistFn _aaxALDistLin;
static _aaxDistFn _aaxALDistLinClamped;
static _aaxDistFn _aaxALDistExp;
static _aaxDistFn _aaxALDistExpClamped;

_aaxDistFn *_aaxDistanceFn[AAX_DISTANCE_MODEL_MAX] =
{
   (_aaxDistFn *)&_aaxDistNone,
   (_aaxDistFn *)&_aaxDistInvExp
};

#define AL_DISTANCE_MODEL_MAX AAX_AL_DISTANCE_MODEL_MAX-AAX_AL_INVERSE_DISTANCE
_aaxDistFn *_aaxALDistanceFn[AL_DISTANCE_MODEL_MAX] =
{
   (_aaxDistFn *)&_aaxALDistInv,
   (_aaxDistFn *)&_aaxALDistInvClamped,
   (_aaxDistFn *)&_aaxALDistLin,
   (_aaxDistFn *)&_aaxALDistLinClamped,
   (_aaxDistFn *)&_aaxALDistExp,
   (_aaxDistFn *)&_aaxALDistExpClamped
};

static float
_aaxDistNone(UNUSED(float dist), UNUSED(float ref_dist), UNUSED(float max_dist), UNUSED(float rolloff))
{
   return 1.0f;
}

/**
 * http://www.engineeringtoolbox.com/outdoor-propagation-sound-d_64.html
 *
 * Lp = Ln + 10 log(Q/(4π*r*r) + 4/R)  (1b)
 *
 * where
 *
 * Lp = sound pressure level (dB)
 * Ln = sound power level source in decibel (dB)
 * Q = Q coefficient 
 *     1 if uniform spherical
 *     2 if uniform half spherical (single reflecting surface)
 *     4 if uniform radiation over 1/4 sphere (two reflecting surfaces, corner)
 * r = distance from source   (m)
 * R = room constant (m2)
 *
 * -- Single Sound Source - Spherical Propagation --
 * Lp = Ln - 10log(4π*r*r)
 * Lp = Ln - 20log(r) * K', where K' = -11
 */
static float
_aaxDistInvExp(float dist, float ref_dist, UNUSED(float max_dist), float rolloff)
{
#if 1
   float fraction = 0.0f, gain = 1.0f;
   if (ref_dist) fraction = _MAX(dist, 1.0f) / _MAX(ref_dist, 1.0f);
   if (fraction) gain = powf(fraction, -rolloff);
   return gain;
#else
   return powf(dist/ref_dist, -rolloff);
#endif
}

/* --- OpenAL support --- */

static float
_aaxALDistInv(float dist, float ref_dist, UNUSED(float max_dist), float rolloff)
{
   float gain = 1.0f;
   float denom = ref_dist + rolloff * (dist - ref_dist);
   if (denom) gain = ref_dist/denom;
   return gain;
}

static float
_aaxALDistInvClamped(float dist, float ref_dist, float max_dist, float rolloff)
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
_aaxALDistLin(float dist, float ref_dist, float max_dist, float rolloff)
{
   float gain = 1.0f;
   float denom = max_dist - ref_dist;
   if (denom) gain = (1-rolloff)*(dist-ref_dist)/denom;
   return gain;
}

static float
_aaxALDistLinClamped(float dist, float ref_dist, float max_dist, float rolloff)
{
   float gain = 1.0f;
   float denom = max_dist - ref_dist;
   dist = _MAX(dist, ref_dist);
   dist = _MIN(dist, max_dist);
   if (denom) gain = (1-rolloff)*(dist-ref_dist)/denom;
   return gain;
}

static float
_aaxALDistExp(float dist, float ref_dist, UNUSED(float max_dist), float rolloff)
{
   float fraction = 0.0f, gain = 1.0f;
   if (ref_dist) fraction = dist / ref_dist;
   if (fraction) gain = powf(fraction, -rolloff);
   return gain;
}

static float
_aaxALDistExpClamped(float dist, float ref_dist, float max_dist, float rolloff)
{
   float fraction = 0.0f, gain = 1.0f;
   dist = _MAX(dist, ref_dist);
   dist = _MIN(dist, max_dist);
   if (ref_dist) fraction = dist / ref_dist;
   if (fraction) gain = powf(fraction, -rolloff);

   return gain;
}

/**
 * rpos: emitter position relative to the listener
 * dist_fact: the factor that translates the distance into meters/feet/etc.
 * speaker: the parents speaker positions
 * ep2d: the emitters 2d properties structure
 * info: the mixers info structure
 */
void
_aaxSetupSpeakersFromDistanceVector(vec3f_ptr rpos, float dist_fact,
                                    vec4f_ptr speaker, _aax2dProps *ep2d,
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
            /*
             * IID; Interaural Intensitive Difference
             */
            pos = 3*t + i;
            dp = vec3fDotProduct(&speaker[pos].v3, rpos);
            dp *= speaker[t].v4[3];
            ep2d->speaker[t].v4[i] = dp * dist_fact;             /* -1 .. +1 */

            /*
             * ITD; Interaural Time Difference
             */
            offs = info->hrtf[HRTF_OFFSET].v4[i];
            fact = info->hrtf[HRTF_FACTOR].v4[i];

            pos = _AAX_MAX_SPEAKERS + 3*t + i;
            dp = vec3fDotProduct(&speaker[pos].v3, rpos);
            ep2d->hrtf[t].v4[i] = _MAX(offs + dp*fact, 0.0f);
         }
      }
      break;
   case AAX_MODE_WRITE_SURROUND:
      for (t=0; t<info->no_tracks; t++)
      {
#ifdef USE_SPATIAL_FOR_SURROUND
         dp = vec3fDotProduct(&speaker[t].v3, rpos);
         dp *= speaker[t].v4[3];

         ep2d->speaker[t].v4[0] = 0.5f + dp*dist_fact;
#else
         vec3fMulvec3(&ep2d->speaker[t].v3, &speaker[t].v3, rpos);
         vec3fScalarMul(&ep2d->speaker[t].v3, &ep2d->speaker[t].v3, dist_fact);
#endif
         i = DIR_UPWD;
         do                             /* skip left-right and back-front */
         {
            offs = info->hrtf[HRTF_OFFSET].v4[i];
            fact = info->hrtf[HRTF_FACTOR].v4[i];

            pos = _AAX_MAX_SPEAKERS + 3*t + i;
            dp = vec3fDotProduct(&speaker[pos].v3, rpos);
            ep2d->hrtf[t].v4[i] = _MAX(offs + dp*fact, 0.0f);
         }
         while(0);
      }
      break;
   case AAX_MODE_WRITE_SPATIAL:
      for (t=0; t<info->no_tracks; t++)
      {                                         /* speaker == sensor_pos */
         dp = vec3fDotProduct(&speaker[t].v3, rpos);
         dp *= speaker[t].v4[3];

         ep2d->speaker[t].v4[0] = 0.5f + dp*dist_fact;
      }
      break;
   default: /* AAX_MODE_WRITE_STEREO */
      for (t=0; t<info->no_tracks; t++)
      {
         vec3fMulvec3(&ep2d->speaker[t].v3, &speaker[t].v3, rpos);
         vec4fScalarMul(&ep2d->speaker[t], &ep2d->speaker[t], dist_fact);
      }
   }
}

float
_distance_prepare(_aax2dProps *ep2d, _aax3dProps *ep3d, _aaxDelayed3dProps *fdp3d_m, vec3f_ptr epos, float dist_ef, vec4f_ptr speaker, const _aaxMixerInfo* info)
{
   _aaxDistFn* distfn;
   float refdist, maxdist, rolloff;

   *(void**)(&distfn) = _FILTER_GET_DATA(ep3d, DISTANCE_FILTER);
   assert(distfn);

   /*
    * Distance queues for every speaker (volume)
    */
   refdist = _FILTER_GET(ep3d, DISTANCE_FILTER, AAX_REF_DISTANCE);
   maxdist = _FILTER_GET(ep3d, DISTANCE_FILTER, AAX_MAX_DISTANCE);
   rolloff = _FILTER_GET(ep3d, DISTANCE_FILTER, AAX_ROLLOFF_FACTOR);

   // If the parent frame is defined indoor then directional sound
   // propagation goes out the door. Note that the scenery frame is
   // never defined as indoor so emitters registered with the mixer
   // will always be directional.
   if (!_PROP3D_MONO_IS_DEFINED(fdp3d_m))
   {
      float dfact = _MIN(dist_ef/refdist, 1.0f);
      _aaxSetupSpeakersFromDistanceVector(epos, dfact, speaker, ep2d, info);
   }

   return distfn(dist_ef, refdist, maxdist, rolloff);
}
