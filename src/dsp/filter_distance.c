/*
 * Copyright 2007-2018 by Erik Hofman.
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

// "Attenuation is generally proportional to the square of sound frequency."
// https://www.nde-ed.org/EducationResources/CommunityCollege/Ultrasonics/Physics/attenuation.htm

// https://en.wikipedia.org/wiki/Attenuation#Attenuation_coefficient
// https://physics.stackexchange.com/questions/87751/do-low-frequency-sounds-really-carry-longer-distances

static aaxFilter
_aaxDistanceFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, 2);
   aaxFilter rv = NULL;

   if (flt)
   {
      _aaxRingBufferDistanceData *data;

      _aaxSetDefaultFilter3d(flt->slot[0], flt->pos, 0);
      flt->slot[0]->destroy = destroy;

      data = calloc(1, sizeof(_aaxRingBufferDistanceData));
      flt->slot[0]->data = data;
      if (data) {
         data->run = _aaxDistanceFn[1];
         data->prev.pa_kPa = 101.325f;
         data->prev.T_K = 293.15f;
         data->prev.hr_pct = 60.0f;
      }
      
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
      _aaxRingBufferDistanceData *data = filter->slot[0]->data;
      assert(data);

      int pos = state & ~AAX_DISTANCE_DELAY;
      if ((pos >= AAX_AL_INVERSE_DISTANCE) && (pos < AAX_AL_DISTANCE_MODEL_MAX))
      {
         pos -= AAX_AL_INVERSE_DISTANCE;
         filter->slot[0]->state = state;
         data->run = _aaxALDistanceFn[pos];
      }
      else if (pos < AAX_DISTANCE_MODEL_MAX)
      {
         filter->slot[0]->state = state;
         data->run = _aaxDistanceFn[pos];
         data->next.T_K = filter->slot[1]->param[AAX_TEMPERATURE - 0x10];
         data->next.pa_kPa = filter->slot[1]->param[AAX_ATMOSPHERIC_PRESSURE - 0x10];
         data->next.hr_pct = filter->slot[1]->param[AAX_RELATIVE_HUMIDITY - 0x10];
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
   _filter_t* rv = _aaxFilterCreateHandle(info, type, 2);

   if (rv)
   {
      unsigned int size = sizeof(_aaxFilterInfo);
      _aaxRingBufferDistanceData *data;

      memcpy(rv->slot[0], &p3d->filter[rv->pos], size);
      rv->slot[0]->destroy = destroy;

      data = calloc(1, sizeof(_aaxRingBufferDistanceData));
      rv->slot[0]->data = data;
      if (data) {
         data->run = _aaxDistanceFn[1];
      }
      rv->state = p3d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxDistanceFilterSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   switch (ptype)
   {
   case AAX_DEGREES_CELSIUS:
      rv = _K2degC(val);
      break;
   case AAX_DEGREES_FAHRENHEIT:
       rv = _K2degF(val);
       break;
   case AAX_ATMOSPHERE:
      rv = _kpa2atm(val);
      break;
   case AAX_BAR:
      rv = _kpa2bar(val);
      break;
   case AAX_PSI:
      rv = _kpa2psi(val);
      break;
   default:
      break;
   }
   return rv;
}

static float
_aaxDistanceFilterGet(float val, int ptype, UNUSED(unsigned char param))
{
   float rv = val;
   switch (ptype)
   {
   case AAX_DEGREES_CELSIUS:
      rv = _degC2K(val);
      break;
   case AAX_DEGREES_FAHRENHEIT:
       rv = _degF2K(val);
       break;
   case AAX_ATMOSPHERE:
      rv = _atm2kpa(val);
      break;
   case AAX_BAR:
      rv = _bar2kpa(val);
      break;
   case AAX_PSI:
      rv = _psi2kpa(val);
      break;
   default:
      break;
   }
   return rv;
}

static float
_aaxDistanceFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxDistanceRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.1f, 0.0f, 0.0f }, { FLT_MAX, FLT_MAX, 1.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {    0.0f,    0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {    0.0f,    0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {    0.0f,    0.0f, 0.0f, 0.0f } }
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
   "AAX_distance_filter_1.02", 1.02f,
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
static _aaxDistFn _aaxDistISO9613;

static _aaxDistFn _aaxALDistInv;
static _aaxDistFn _aaxALDistInvClamped;
static _aaxDistFn _aaxALDistLin;
static _aaxDistFn _aaxALDistLinClamped;
static _aaxDistFn _aaxALDistExp;
static _aaxDistFn _aaxALDistExpClamped;

_aaxDistFn *_aaxDistanceFn[AAX_DISTANCE_MODEL_MAX] =
{
   (_aaxDistFn *)&_aaxDistNone,
   (_aaxDistFn *)&_aaxDistInvExp,
   (_aaxDistFn *)&_aaxDistISO9613
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
_aaxDistNone(UNUSED(void *data))
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
_aaxDistInvExp(void *data)
{
   _aaxRingBufferDistanceData *d = (_aaxRingBufferDistanceData*)data;
   float fraction = 0.0f, gain = 1.0f;
   if (d->ref_dist) fraction = _MAX(d->dist, 1.0f) / _MAX(d->ref_dist, 1.0f);
   if (fraction) gain = powf(fraction, -d->rolloff);
   return gain;
}


// http://www.sengpielaudio.com/AirdampingFormula.htm
static float
_aaxDistISO9613(void *data)
{
    _aaxRingBufferDistanceData *d = (_aaxRingBufferDistanceData*)data;
    static float f = 5000.0f;		// Midband frequency in Hz
    static float a = 0.0f;
    float gain = 1.0f;

    if (fabsf(d->prev.T_K - d->next.T_K) > FLT_EPSILON ||
        fabsf(d->prev.pa_kPa - d->next.pa_kPa) > FLT_EPSILON ||
        fabsf(d->prev.hr_pct - d->next.hr_pct) > FLT_EPSILON)
    {
       static const float To1 = 273.16f;
       static const float To = 293.15f;
       static const float pr = 101.325f;
       float pa_pr, pr_pa, psat;
       float frO, frN, T_To;
       float T, pa, hr;
       float h, y, z;
       float f2 = f*f;

       if (d->next.T_K == 0.0f) d->next.T_K = d->prev.T_K;
       if (d->next.pa_kPa == 0.0f) d->next.pa_kPa = d->prev.pa_kPa;
       if (d->next.hr_pct == 0.0f) d->next.hr_pct = d->prev.hr_pct;
       d->prev = d->next;

       T = d->next.T_K;
       pa = d->next.pa_kPa;
       hr = d->next.hr_pct;

       T_To = T/To;
       pa_pr = pa/pr;
       pr_pa = pr/pa;

       psat = pr*powf(10.0f,-6.8346f*powf(To1/T,1.261f)+4.6151f);
       h = hr*(psat/pa);

       frO = pa_pr*(24.0f+4.04e4f*h*((0.02f+h)/(0.391f+h)));
       frN = pa_pr*powf(T_To,-0.5f)*(9.0f+280.0f*h*expf(-4.170f*(powf(T_To,-0.33333f)-1.0f)));

       z = 0.1068f*expf(-3352.0f/T)*(frN/(frN+f2));
       y = powf(T_To,-2.5f)*(0.01275f*exp(-2239.1f/T)*(frO/(frO*frO+f2))+z);
       a = 8.686f*f2*((1.84e-11f*pr_pa*powf(T_To,0.5f))+y);
    }

    gain = _db2lin(_MAX(d->dist - d->ref_dist, 0.0f) * -a*d->rolloff);

    return gain;
}


/* --- OpenAL support --- */

static float
_aaxALDistInv(void *data)
{
   _aaxRingBufferDistanceData *d = (_aaxRingBufferDistanceData*)data;
   float gain = 1.0f;
   float denom = d->ref_dist + d->rolloff * (d->dist - d->ref_dist);
   if (denom) gain = d->ref_dist/denom;
   return gain;
}

static float
_aaxALDistInvClamped(void *data)
{
   _aaxRingBufferDistanceData *d = (_aaxRingBufferDistanceData*)data;
   float gain = 1.0f;
   float denom;
   d->dist = _MAX(d->dist, d->ref_dist);
   d->dist = _MIN(d->dist, d->max_dist);
   denom = d->ref_dist + d->rolloff * (d->dist - d->ref_dist);
   if (denom) gain = d->ref_dist/denom;
   return gain;
}

static float
_aaxALDistLin(void *data)
{
   _aaxRingBufferDistanceData *d = (_aaxRingBufferDistanceData*)data;
   float gain = 1.0f;
   float denom = d->max_dist - d->ref_dist;
   if (denom) gain = (1.0f - d->rolloff)*(d->dist - d->ref_dist)/denom;
   return gain;
}

static float
_aaxALDistLinClamped(void *data)
{
   _aaxRingBufferDistanceData *d = (_aaxRingBufferDistanceData*)data;
   float gain = 1.0f;
   float denom = d->max_dist - d->ref_dist;
   d->dist = _MAX(d->dist, d->ref_dist);
   d->dist = _MIN(d->dist, d->max_dist);
   if (denom) gain = (1.0f - d->rolloff)*(d->dist - d->ref_dist)/denom;
   return gain;
}

static float
_aaxALDistExp(void *data)
{
   _aaxRingBufferDistanceData *d = (_aaxRingBufferDistanceData*)data;
   float fraction = 0.0f, gain = 1.0f;
   if (d->ref_dist) fraction = d->dist / d->ref_dist;
   if (fraction) gain = powf(fraction, -d->rolloff);
   return gain;
}

static float
_aaxALDistExpClamped(void *data)
{
   _aaxRingBufferDistanceData *d = (_aaxRingBufferDistanceData*)data;
   float fraction = 0.0f, gain = 1.0f;
   d->dist = _MAX(d->dist, d->ref_dist);
   d->dist = _MIN(d->dist, d->max_dist);
   if (d->ref_dist) fraction = d->dist / d->ref_dist;
   if (fraction) gain = powf(fraction, -d->rolloff);

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
         vec3fMulVec3(&ep2d->speaker[t].v3, &speaker[t].v3, rpos);
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
         vec3fMulVec3(&ep2d->speaker[t].v3, &speaker[t].v3, rpos);
         vec4fScalarMul(&ep2d->speaker[t], &ep2d->speaker[t], dist_fact);
      }
   }
}

float
_distance_prepare(_aax2dProps *ep2d, _aax3dProps *ep3d, _aaxDelayed3dProps *fdp3d_m, vec3f_ptr epos, float dist_ef, vec4f_ptr speaker, const _aaxMixerInfo* info)
{
   _aaxRingBufferDistanceData *data;
   float refdist, maxdist, rolloff;

   assert(info->unit_m > 0.0f);

   data = _FILTER_GET_DATA(ep3d, DISTANCE_FILTER);

   /*
    * Distance queues for every speaker (volume)
    */
   rolloff = _FILTER_GET(ep3d, DISTANCE_FILTER, AAX_ROLLOFF_FACTOR);
   maxdist = _FILTER_GET(ep3d, DISTANCE_FILTER, AAX_MAX_DISTANCE);
   refdist = _FILTER_GET(ep3d, DISTANCE_FILTER, AAX_REF_DISTANCE);

   // If the parent frame is defined indoor then directional sound
   // propagation goes out the door. Note that the scenery frame is
   // never defined as indoor so emitters registered with the mixer
   // will always be directional.
   if (!_PROP3D_MONO_IS_DEFINED(fdp3d_m))
   {
      float dfact = _MIN(dist_ef/refdist, 1.0f);
      _aaxSetupSpeakersFromDistanceVector(epos, dfact, speaker, ep2d, info);
   }

   data->dist = dist_ef;
   data->ref_dist = refdist;
   data->max_dist = maxdist;
   data->rolloff = rolloff;
   data->unit_m = info->unit_m;
   return data->run(data);
}
