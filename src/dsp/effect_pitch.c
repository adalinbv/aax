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

#define VERSION	1.01
#define DSIZE	sizeof(_aaxEnvelopeData)

static aaxEffect
_aaxPitchEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 1, DSIZE);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxEnvelopeData* env = eff->slot[0]->data;

      _aaxSetDefaultEffect2d(eff->slot[0], eff->pos, 0);
      if (env) {
         env->value0 = env->value = 1.0f;
      }
      rv = (aaxEffect)eff;
   }
   return rv;
}

static aaxEffect
_aaxPitchEffectSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;
   float dt = effect->slot[0]->param[AAX_PITCH_RATE];

   effect->state = state;
   if (state && dt > 0.0f)
   {
      _aaxEnvelopeData* env = effect->slot[0]->data;
      if (env == NULL)
      {
         env = _aax_aligned_alloc(DSIZE);
         if (env)
         {
            effect->slot[0]->data = env;
            effect->slot[0]->data_size = DSIZE;
            memset(env, 0, DSIZE);
         }
      }

      if (env)
      {		// based on the timed-pitch effect
         float value = effect->slot[0]->param[AAX_PITCH_START];
         float nextval = effect->slot[0]->param[AAX_PITCH];
         float period = effect->info->period_rate;
         uint32_t max_pos;

         // pitch is already applied elsewhere so we normalize
         value /= nextval;
         nextval /= nextval;

         env->sustain = true;
         env->value0 = env->value = 1.0f;
         env->value = value;
         env->value_max = nextval;
         env->max_stages = 1;
         env->state |= AAX_LFO_EXPONENTIAL;

         max_pos = rintf(dt * period);
         env->step[0] = fabsf(nextval - value)/max_pos;
         env->max_pos[0] = max_pos;
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
   }
   else
   {
      if (effect->slot[0]->data)
      {
         effect->slot[0]->destroy(effect->slot[0]->data);
         effect->slot[0]->data_size = 0;
         effect->slot[0]->data = NULL;
      }
   }
   return effect;
}

static _effect_t*
_aaxNewPitchEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 1, 0);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[0], &p2d->effect[rv->pos]);
      rv->state = p2d->effect[rv->pos].state;
   }
   return rv;
}

static float
_aaxPitchEffectSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxPitchEffectGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}


#define PMAX	(float)(1 << MAX_MIP_LEVELS)
static float
_aaxPitchEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxPitchRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { PMAX, PMAX, PMAX, FLT_MAX } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f,    0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f,    0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f,    0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxPitchRange[slot].min[param],
                       _aaxPitchRange[slot].max[param]);
}
#undef PMAX

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxPitchEffect =
{
   "AAX_pitch_effect_"AAX_MKSTR(VERSION), VERSION,
   (_aaxEffectCreateFn*)&_aaxPitchEffectCreate,
   (_aaxEffectDestroyFn*)&_aaxEffectDestroy,
   (_aaxEffectResetFn*)&_env_reset,
   (_aaxEffectSetStateFn*)&_aaxPitchEffectSetState,
   NULL,
   (_aaxNewEffectHandleFn*)&_aaxNewPitchEffectHandle,
   (_aaxEffectConvertFn*)&_aaxPitchEffectSet,
   (_aaxEffectConvertFn*)&_aaxPitchEffectGet,
   (_aaxEffectConvertFn*)&_aaxPitchEffectMinMax
};

