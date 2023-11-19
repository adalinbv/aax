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

#include <base/types.h>		/*  for rintf */
#include <base/gmath.h>

#include "effects.h"
#include "api.h"
#include "arch.h"

#define DSIZE   sizeof(_aaxRingBufferVelocityEffectData)

static aaxEffect _aaxVelocityEffectSetState(_effect_t*, int state);

static aaxEffect
_aaxVelocityEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 1, DSIZE);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxSetDefaultEffect3d(eff->slot[0], eff->pos, 0);
      eff->slot[0]->destroy = _velocity_destroy;
      eff->slot[0]->swap = _velocity_swap;

      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxVelocityEffectDestroy(_effect_t* effect)
{
   if (effect->slot[0]->data)
   {
      effect->slot[0]->destroy(effect->slot[0]->data);
      effect->slot[0]->data = NULL;
   }
   free(effect);
   return true;
}

static aaxEffect
_aaxVelocityEffectSetState(_effect_t* effect, UNUSED(int state))
{
   _EFFECT_SET_SLOT_UPDATED(effect);
   return  effect;
}

static aaxEffect
_aaxVelocityEffectSetData(_effect_t* effect, aaxBuffer buffer)
{
   aaxEffect rv = false;
   return rv;
}

static _effect_t*
_aaxNewVelocityEffectHandle(const aaxConfig config, enum aaxEffectType type, UNUSED(_aax2dProps* p2d), _aax3dProps* p3d)
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 1, DSIZE);

   if (rv)
   {
      _aaxRingBufferVelocityEffectData *data = rv->slot[0]->data;

      data->dopplerfn = _aaxDopplerFn[0];
      data->prepare = _velocity_prepare;
      data->run = _velocity_run;

      _aax_dsp_copy(rv->slot[0], &p2d->effect[rv->pos]);
      rv->slot[0]->destroy = _velocity_destroy;
      rv->slot[0]->swap = _velocity_swap;

      rv->state = p3d->effect[rv->pos].state;
      if (_EFFECT_GET_UPDATED(p3d, rv->pos)) {
         _EFFECT_SET_SLOT_UPDATED(rv);
      }
   }
   return rv;
}

static float
_aaxVelocityEffectSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{  
   float rv = val;
   return rv;
}
   
static float
_aaxVelocityEffectGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{  
   float rv = val;
   return rv;
}

static float
_aaxVelocityEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxVelocityRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { FLT_MAX, 10.0f, FLT_MAX, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {    0.0f,  0.0f,    0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {    0.0f,  0.0f,    0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {    0.0f,  0.0f,    0.0f, 0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxVelocityRange[slot].min[param],
                       _aaxVelocityRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxVelocityEffect =
{
   true,
   "AAX_velocity_effect", 1.0f,
   (_aaxEffectCreate*)&_aaxVelocityEffectCreate,
   (_aaxEffectDestroy*)&_aaxVelocityEffectDestroy,
   NULL,
   (_aaxEffectSetState*)&_aaxVelocityEffectSetState,
   (_aaxEffectSetData*)&_aaxVelocityEffectSetData,
   (_aaxNewEffectHandle*)&_aaxNewVelocityEffectHandle,
   (_aaxEffectConvert*)&_aaxVelocityEffectSet,
   (_aaxEffectConvert*)&_aaxVelocityEffectGet,
   (_aaxEffectConvert*)&_aaxVelocityEffectMinMax
};

static _aaxPitchShiftFn _aaxDopplerShift;

_aaxPitchShiftFn *_aaxDopplerFn[] =
{
   (_aaxPitchShiftFn *)&_aaxDopplerShift
};

void
_velocity_swap(void *d, void *s)
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
         _aaxRingBufferVelocityEffectData *ddef = dst->data;
         _aaxRingBufferVelocityEffectData *sdef = src->data;

         assert(dst->data_size == src->data_size);

         ddef->dopplerfn = sdef->dopplerfn;
         ddef->prepare = sdef->prepare;
         ddef->run = sdef->run;
      }
   }
   dst->destroy = src->destroy;
   dst->swap = src->swap;
}

void
_velocity_destroy(void *ptr)
{
   _aaxRingBufferVelocityEffectData* data = ptr;
   if (data)
   {
      if (data->sample_ptr) free(data->sample_ptr);
      _aax_aligned_free(data);
   }
}

/*
 * Sources:
 * http://en.wikipedia.org/wiki/Doppler_effect
 */
static float
_aaxDopplerShift(float ve, float vsound)
{
#if 1
   float vse, rv;

   /* relative speed */
   vse = _MIN(ve, vsound);
   rv =  vsound/_MAX(vsound - vse, 1.0f);

   return rv;
#else
   float ves;
   ves = _MAX(vsound - _MIN(ve, vsound), 1.0f);
   return vsound/ves;
#endif
}

// http://www.who.int/occupational_health/publications/noise1.pdf?ua=1
float
_velocity_calculcate_vs(_aaxEnvData *data)
{
   static const float Rvapor = 461.52f;	// Water vapor: individual gas constant
   static const float Rair = 287.5f;	// Air: individual gas constant
   static const float y = 1.402f;	// Air: Ratio of specific heat
   float T, R, hr, rv;

   T = data->T_K;
   hr = 0.01f*data->hr_pct;
   R = Rair + 0.04f*hr*Rvapor;
   rv = sqrtf(y*R*T);			// speed of sound in m/s

   return rv*data->unit_m;
}

FLOAT
_velocity_prepare(_aax3dProps *ep3d, _aaxDelayed3dProps *edp3d, _aaxDelayed3dProps *edp3d_m, _aaxDelayed3dProps *fdp3d_m, vec3f_ptr epos, float dist_ef, float vs, float sdf)
{
   FLOAT df = 1.0;

   if (dist_ef > 1.0f)
   {
      _aaxRingBufferVelocityEffectData *velocity;
      float ve;
      FLOAT c;

      velocity = _EFFECT_GET_DATA(ep3d, VELOCITY_EFFECT);
      assert(velocity->dopplerfn);

      /* align velocity vectors with the modified emitter position
       * relative to the sensor
       */
      mtx4fMul(&edp3d_m->velocity, &fdp3d_m->velocity, &edp3d->velocity);

      df = 0.0f;
      c = _EFFECT_GET(ep3d, VELOCITY_EFFECT, AAX_LIGHT_VELOCITY);
      if (c > 0.0f)
      {
         FLOAT ve2 = vec3fMagnitudeSquared(&edp3d_m->velocity.v34[LOCATION]);
         df = _lorentz(ve2, c*c);
      }

      ve = vec3fDotProduct(&edp3d_m->velocity.v34[LOCATION], epos);
      df = _MAX(df + velocity->dopplerfn(ve, vs/sdf), 0.1f); // prevent negative pitch
#if 0
# if 1
 printf("position: ");
 PRINT_VEC3PTR(epos);
 printf("velocity: %3.2f, %3.2f, %3.2f\n",
            edp3d_m->velocity.v34[LOCATION].v3[0],
            edp3d_m->velocity.v34[LOCATION].v3[1],
            edp3d_m->velocity.v34[LOCATION].v3[2]);
 printf("velocity:\t\t\t\tparent velocity:\n");
 PRINT_MATRICES(edp3d->velocity, fdp3d_m->velocity);
 printf("modified velocity:\n");
 PRINT_MATRIX(edp3d_m->velocity);
 printf("doppler: %lf, ∆ve: %f, vs: %f\n\n", df, ve, vs/sdf);
# else
 printf("doppler: %lf, ve: %f, vs: %f\n", df, ve, vs/sdf);
# endif
#endif
      ep3d->buf3dq_step = df;
   }

   return df;
}

int
_velocity_run(void *rb, void *data)
{
   int rv = false;
   return rv;
}
