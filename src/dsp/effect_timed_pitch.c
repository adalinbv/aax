/*
 * Copyright 2007-2023 by Erik Hofman.
 * Copyright 2009-2023 by Adalin B.V.
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

#include <aax/aax.h>

#include <base/types.h>		/*  for rintf */
#include <base/gmath.h>

#include "effects.h"
#include "api.h"
#include "arch.h"

#define	DSIZE	sizeof(_aaxEnvelopeData)

static aaxEffect
_aaxTimedPitchEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, _MAX_ENVELOPE_STAGES/2, DSIZE);
   aaxEffect rv = NULL;

   if (eff)
   {
      unsigned s;
      for (s=0; s<_MAX_ENVELOPE_STAGES/2; s++) {
         _aaxSetDefaultEffect2d(eff->slot[s], eff->pos, s);
      }
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxTimedPitchEffectDestroy(_effect_t* effect)
{
   if (effect->slot[0]->data)
   {
      effect->slot[0]->destroy(effect->slot[0]->data);
      effect->slot[0]->data = NULL;
   }
   free(effect);

   return AAX_TRUE;
}

static aaxEffect
_aaxTimedPitchEffectSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;
   aaxEffect rv = AAX_FALSE;

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
         env->sustain = AAX_TRUE;
         env->value0 = env->value = nextval;
         env->max_stages = _MAX_ENVELOPE_STAGES-1;
         for (i=0; i<_MAX_ENVELOPE_STAGES/2; i++)
         {
            float dt, value = nextval;
            uint32_t max_pos;

            max_pos = (uint32_t)-1;
            dt = effect->slot[i]->param[AAX_TIME0];
            if (dt != FLT_MAX)
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
            env->step[stage] = (nextval - value)/max_pos;
            env->max_pos[stage] = max_pos;
            stage++;

            max_pos = (uint32_t)-1;
            dt = effect->slot[i]->param[AAX_TIME1];
            if (dt != FLT_MAX)
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
            nextval = (i < 2) ? effect->slot[i+1]->param[AAX_LEVEL0] : 0.0f;
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
   _effect_t* rv = _aaxEffectCreateHandle(info, type, _MAX_ENVELOPE_STAGES/2,0);

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

         stages = _MIN(1+env->max_stages/2, _MAX_ENVELOPE_STAGES/2);
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
    { {  0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, FLT_MAX, 4.0f, FLT_MAX } },
    { {  0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, FLT_MAX, 4.0f, FLT_MAX } },
    { {  0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, FLT_MAX, 4.0f, FLT_MAX } },
    { {  0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, FLT_MAX, 4.0f, FLT_MAX } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxTimedPitchRange[slot].min[param],
                       _aaxTimedPitchRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxTimedPitchEffect =
{
   AAX_FALSE,
   "AAX_timed_pitch_effect", 1.0f,
   (_aaxEffectCreate*)&_aaxTimedPitchEffectCreate,
   (_aaxEffectDestroy*)&_aaxTimedPitchEffectDestroy,
   (_aaxEffectReset*)&_env_reset,
   (_aaxEffectSetState*)&_aaxTimedPitchEffectSetState,
   NULL,
   (_aaxNewEffectHandle*)&_aaxNewTimedPitchEffectHandle,
   (_aaxEffectConvert*)&_aaxTimedPitchEffectSet,
   (_aaxEffectConvert*)&_aaxTimedPitchEffectGet,
   (_aaxEffectConvert*)&_aaxTimedPitchEffectMinMax
};

