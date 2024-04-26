/*
 * SPDX-FileCopyrightText: Copyright © 2007-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <aax/aax.h>

#include <base/types.h>		/* for rintf */
#include <base/gmath.h>

#include "common.h"
#include "filters.h"
#include "arch.h"
#include "api.h"

#define VERSION	1.02
#define DSIZE	sizeof(_aaxRingBufferDistanceData)

// "Attenuation is generally proportional to the square of sound frequency."
// https://www.nde-ed.org/EducationResources/CommunityCollege/Ultrasonics/Physics/attenuation.htm

// https://en.wikipedia.org/wiki/Attenuation#Attenuation_coefficient
// https://physics.stackexchange.com/questions/87751/do-low-frequency-sounds-really-carry-longer-distances

static aaxFilter
_aaxDistanceFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, 2, DSIZE);
   aaxFilter rv = NULL;

   if (flt)
   {
      _aaxRingBufferDistanceData *data = flt->slot[0]->data;

      _aaxSetDefaultFilter3d(flt->slot[0], flt->pos, 0);
      data->prev.unit_m = flt->info->unit_m;
      data->next.unit_m = flt->info->unit_m;

      flt->slot[0]->destroy = _distance_destroy;
      flt->slot[0]->swap = _distance_swap;
      rv = (aaxFilter)flt;
   }
   return rv;
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
         data->next.T_K = filter->slot[1]->param[AAX_TEMPERATURE & 0xF];
         data->next.pa_kPa = filter->slot[1]->param[AAX_ATMOSPHERIC_PRESSURE & 0xF];
         data->next.hr_pct = filter->slot[1]->param[AAX_RELATIVE_HUMIDITY & 0xF];
#if 0
 printf("Temp: %5.2f K, atm. pressure: %6.2f kPA, humidity: %f4.1%%\n", data->next.T_K, data->next.pa_kPa, data->next.hr_pct);
#endif
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
   _filter_t* rv = _aaxFilterCreateHandle(info, type, 2, DSIZE);

   if (rv)
   {
      _aaxRingBufferDistanceData *data;

      _aax_dsp_copy(rv->slot[0], &p2d->filter[rv->pos]);

      data = rv->slot[0]->data;
      data->prev.unit_m = rv->info->unit_m;
      data->next.unit_m = rv->info->unit_m;

      rv->slot[0]->destroy = _distance_destroy;
      rv->slot[0]->swap = _distance_swap;
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
    { { 0.0f, 0.1f, 0.0f, 0.0f }, { FLT_MAX, FLT_MAX,   1.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { FLT_MAX, FLT_MAX, 100.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {    0.0f,    0.0f,   0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {    0.0f,    0.0f,   0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxDistanceRange[slot].min[param],
                       _aaxDistanceRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxDistanceFilter =
{
   "AAX_distance_filter_"AAX_MKSTR(VERSION), VERSION,
   (_aaxFilterCreateFn*)&_aaxDistanceFilterCreate,
   (_aaxFilterDestroyFn*)&_aaxFilterDestroy,
   NULL,
   (_aaxFilterSetStateFn*)&_aaxDistanceFilterSetState,
   (_aaxNewFilterHandleFn*)&_aaxNewDistanceFilterHandle,
   (_aaxFilterConvertFn*)&_aaxDistanceFilterSet,
   (_aaxFilterConvertFn*)&_aaxDistanceFilterGet,
   (_aaxFilterConvertFn*)&_aaxDistanceFilterMinMax
};

void
_distance_swap(void *d, void *s)
{
   _aaxFilterInfo *dst = d, *src = s;

   if (src->data && src->data_size)
   {
      if (!dst->data) {
          dst->data = _aaxAtomicPointerSwap(&src->data, dst->data);
          dst->data_size = src->data_size;
      }
      else
      {
         _aaxRingBufferDistanceData *drev = dst->data;
         _aaxRingBufferDistanceData *srev = src->data;

         assert(dst->data_size == src->data_size);

         drev->run = srev->run;
      }
   }
   dst->destroy = src->destroy;
   dst->swap = src->swap;
}

void
_distance_destroy(void *ptr)
{
   _aaxRingBufferDistanceData *distance = (_aaxRingBufferDistanceData*)ptr;
   if (distance) _aax_aligned_free(distance);
}

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
// Mars: frequencies above 240Hz travel more than 10m/s faster than low
// frequencies.
// https://www.hou.usra.edu/meetings/lpsc2022/pdf/1357.pdf
static float
_aaxDistISO9613(void *data)
{
    _aaxRingBufferDistanceData *d = (_aaxRingBufferDistanceData*)data;
    float gain = 1.0f;

    if (fabsf(d->prev.T_K - d->next.T_K) > FLT_EPSILON ||
        fabsf(d->prev.pa_kPa - d->next.pa_kPa) > FLT_EPSILON ||
        fabsf(d->prev.hr_pct - d->next.hr_pct) > FLT_EPSILON)
    {
       static const float To1 = 273.16f;
       static const float To = 293.15f;
       static const float pr = 101.325f;
       float f2 = d->f2;
       float pa_pr, pr_pa, psat;
       float frO, frN, T_To;
       float T, pa, hr;
       float h, y, z;
       float unit_m;

       if (d->next.T_K == 0.0f) d->next.T_K = d->prev.T_K;
       if (d->next.pa_kPa == 0.0f) d->next.pa_kPa = d->prev.pa_kPa;
       if (d->next.hr_pct == 0.0f) d->next.hr_pct = d->prev.hr_pct;
       d->prev = d->next;

       T = d->next.T_K;
       pa = d->next.pa_kPa;
       hr = d->next.hr_pct;
       unit_m = d->next.unit_m;

       T_To = T/To;
       pa_pr = pa/pr;
       pr_pa = pr/pa;

       psat = pr*powf(10.0f,-6.8346f*powf(To1/T,1.261f)+4.6151f);
       h = hr*(psat/pa);

       frO = pa_pr*(24.0f+4.04e4f*h*((0.02f+h)/(0.391f+h)));
       frN = pa_pr*powf(T_To,-0.5f)*(9.0f+280.0f*h*expf(-4.170f*(powf(T_To,-0.33333f)-1.0f)));

       z = 0.1068f*expf(-3352.0f/T)*(frN/(frN+f2));
       y = powf(T_To,-2.5f)*(0.01275f*exp(-2239.1f/T)*(frO/(frO*frO+f2))+z);
       d->next.a = 8.686f*f2*((1.84e-11f*pr_pa*powf(T_To,0.5f))+y);
       d->next.a *= -unit_m;
    }

    gain = _db2lin(_MAX(d->dist - d->ref_dist, 0.0f) * d->next.a*d->rolloff);

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
   float dp, offs, fact, gain;
   int i, t;

   switch (info->mode)
   {
   case AAX_MODE_WRITE_SPATIAL:
      for (t=0; t<info->no_tracks; t++)
      {					/* speaker == sensor_pos */
         dp = vec3fDotProduct(&speaker[t].v3, rpos);
         gain = dist_fact * fabsf(speaker[t].v4[GAIN]);

         ep2d->speaker[t].v4[DIR_RIGHT] = 0.5f + gain*dp;
      }
      break;
   case AAX_MODE_WRITE_SPATIAL_SURROUND:
      for (t=0; t<info->no_tracks; t++)
      {
         dp = vec3fDotProduct(&speaker[t].v3, rpos);
         gain = dist_fact * fabsf(speaker[t].v4[GAIN]);

         ep2d->speaker[t].v4[DIR_RIGHT] = 0.5f + gain*dp;

         i = DIR_UPWD;
         do /* skip left-right and back-front */
         {
            int pos = 3*t + i;
            dp = vec3fDotProduct(&speaker[pos].v3, rpos);

            offs = info->hrtf[HRTF_OFFSET].v4[i];
            fact = info->hrtf[HRTF_FACTOR].v4[i];

            ep2d->hrtf[t].v4[i] = _MAX(offs + dp*fact, 0.0f);
         }
         while(0);
      }
      break;
   case AAX_MODE_WRITE_HRTF:
      for (t=0; t<info->no_tracks; t++)
      {
         gain = dist_fact * fabsf(speaker[t].v4[GAIN]);

         for (i=0; i<3; i++)
         {
            int pos = 3*t + i;
            dp = vec3fDotProduct(&speaker[pos].v3, rpos);

            /*
             * ILD; Interaural Level Difference
             */
            ep2d->speaker[t].v4[i] = gain*dp;	/* -1 .. +1 */

            /*
             * ITD; Interaural Time Difference
             */
            offs = info->hrtf[HRTF_OFFSET].v4[i];
            fact = info->hrtf[HRTF_FACTOR].v4[i];

            pos = _AAX_MAX_SPEAKERS + pos;
            ep2d->hrtf[t].v4[i] = _MAX(offs + dp*fact, 0.0f);
         }
      }
      break;
   case AAX_MODE_WRITE_SURROUND:
   case AAX_MODE_WRITE_STEREO:
   default:
      for (t=0; t<info->no_tracks; t++)
      {
         vec3fMulVec3(&ep2d->speaker[t].v3, &speaker[t].v3, rpos);
         vec4fScalarMul(&ep2d->speaker[t], &ep2d->speaker[t], dist_fact);
      }
   }
}

float
_distance_prepare(_aax2dProps *ep2d, _aax3dProps *ep3d, _aaxDelayed3dProps *fdp3d_m, vec3f_ptr epos, float dist_ef, const vec4f_ptr speaker, const _aaxMixerInfo* info)
{
   _aaxRingBufferDistanceData *data;
   float refdist, maxdist, rolloff;

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
   data->next.unit_m = info->unit_m;
   return data->run(data);
}
