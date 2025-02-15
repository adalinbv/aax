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
#define	DSIZE	sizeof(_aaxEnvelopeData)

#define MAX_SLOTS	(_MAX_ENVELOPE_STAGES/2)

static aaxEffect
_aaxTimedPitchEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, MAX_SLOTS, DSIZE);
   aaxEffect rv = NULL;

   if (eff)
   {
      unsigned s;
      for (s=0; s<MAX_SLOTS; s++) {
         _aaxSetDefaultEffect2d(eff->slot[s], eff->pos, s);
      }
      rv = (aaxEffect)eff;
   }
   return rv;
}

static aaxEffect
_aaxTimedPitchEffectSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;
   aaxEffect rv = false;
   bool reverse;

   reverse = (state & AAX_REVERSE) ? true : false;
   state &= ~AAX_REVERSE;

   if TEST_FOR_TRUE(state)
   {
      _aaxEnvelopeData* env = effect->slot[0]->data;
      if (env == NULL)
      {
         env =  _aax_aligned_alloc(DSIZE);
         effect->slot[0]->data = env;
         if (env) memset(env, 0, DSIZE);
      }

      if (env)
      {
         float nextval = effect->slot[0]->param[AAX_LEVEL0];
         float period = effect->info->period_rate;
         float timestep = 1.0f / period;
         int i, stage;

         stage = 0;
         env->state = state;
         env->reverse = reverse;
         env->sustain = true;
         env->value0 = env->value = env->value_max = nextval;
         env->max_stages = _MAX_ENVELOPE_STAGES-1;
         for (i=0; i<MAX_SLOTS; i++)
         {
            float dt, value = nextval;
            uint32_t max_pos;

            max_pos = (uint32_t)-1;
            dt = effect->slot[i]->param[AAX_TIME0];
            if (dt != INFINITY)
            {
               if (dt < timestep && dt > EPS) dt = timestep;
               max_pos = rintf(dt * period);
            }
            if (max_pos == 0)
            {
               env->max_stages = stage;
               break;
            }

            nextval = effect->slot[i]->param[AAX_LEVEL1];
            if (nextval > env->value_max) env->value_max = nextval;
            env->step[stage] = (nextval - value)/max_pos;
            env->max_pos[stage] = max_pos;
            stage++;

            max_pos = (uint32_t)-1;
            dt = effect->slot[i]->param[AAX_TIME1];
            if (dt != INFINITY)
            {
               if (dt < timestep && dt > EPS) dt = timestep;
               max_pos = rintf(dt * period);
            }
            if (max_pos == 0)
            {
               env->max_stages = 2*i+1;
               break;
            }

            value = nextval;
            nextval = (i < (MAX_SLOTS-1)) ? effect->slot[i+1]->param[AAX_LEVEL0] : 0.0f;
            if (nextval > env->value_max) env->value_max = nextval;
            env->step[stage] = (nextval - value)/max_pos;
            env->max_pos[stage] = max_pos;
            stage++;
         }
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
   rv = effect;
   return rv;
}

static _effect_t*
_aaxNewTimedPitchEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, MAX_SLOTS,0);

   if (rv)
   {
      _aaxEnvelopeData *env;
      unsigned int no_steps;
      float dt, value;
      int i, stages;

      _aax_dsp_copy(rv->slot[0], &p2d->effect[rv->pos]);

      rv->state = p2d->effect[rv->pos].state;

      i = 0;
      env = (_aaxEnvelopeData*)p2d->effect[rv->pos].data;
      if (env)
      {
         if (env->max_pos[1] > env->max_pos[0]) i = 1;
         dt = p2d->effect[rv->pos].param[2*i+1] / env->max_pos[i];

         no_steps = env->max_pos[1];
         value = p2d->effect[rv->pos].param[AAX_LEVEL1];
         value += env->step[1] * no_steps;

         stages = _MIN(1+env->max_stages/2, MAX_SLOTS);
         for (i=1; i<stages; i++)
         {
            _aaxEffectInfo* slot = rv->slot[i];

            no_steps = env->max_pos[2*i];
            slot->param[0] = value;
            slot->param[1] = no_steps * dt;

            value += env->step[2*i] * no_steps;
            no_steps = env->max_pos[2*i+1];
            slot->param[2] = value;
            slot->param[3] = no_steps * dt;

            value += env->step[2*i+1] * no_steps;
         }
      }
   }
   return rv;
}

static float
_aaxTimedPitchEffectSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxTimedPitchEffectGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxTimedPitchEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxTimedPitchRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { {  0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, INFINITY, 4.0f, INFINITY } },
    { {  0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, INFINITY, 4.0f, INFINITY } },
    { {  0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, INFINITY, 4.0f, INFINITY } },
    { {  0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, INFINITY, 4.0f, INFINITY } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxTimedPitchRange[slot].min[param],
                       _aaxTimedPitchRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxTimedPitchEffect =
{
   "AAX_timed_pitch_effect_"AAX_MKSTR(VERSION), VERSION,
   (_aaxEffectCreateFn*)&_aaxTimedPitchEffectCreate,
   (_aaxEffectDestroyFn*)&_aaxEffectDestroy,
   (_aaxEffectResetFn*)&_env_reset,
   (_aaxEffectSetStateFn*)&_aaxTimedPitchEffectSetState,
   NULL,
   (_aaxNewEffectHandleFn*)&_aaxNewTimedPitchEffectHandle,
   (_aaxEffectConvertFn*)&_aaxTimedPitchEffectSet,
   (_aaxEffectConvertFn*)&_aaxTimedPitchEffectGet,
   (_aaxEffectConvertFn*)&_aaxTimedPitchEffectMinMax
};

