/*
 * Copyright 2007-2017 by Erik Hofman.
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

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
#endif

#include <aax/aax.h>

#include <base/types.h>		/*  for rintf */
#include <base/gmath.h>

#include "effects.h"
#include "api.h"
#include "arch.h"


static aaxEffect
_aaxTimedPitchEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   unsigned int size = sizeof(_effect_t);
   _effect_t* eff = NULL;
   aaxEffect rv = NULL;

   size += (_MAX_ENVELOPE_STAGES/2)*sizeof(_aaxEffectInfo);
   eff = calloc(1, size);
   if (eff)
   {
      char *ptr;
      int i;

      eff->id = EFFECT_ID;
      eff->state = AAX_FALSE;
      eff->info = info;

      ptr = (char*)eff + sizeof(_effect_t);
      eff->slot[0] = (_aaxEffectInfo*)ptr;
      eff->pos = _eff_cvt_tbl[type].pos;
      eff->type = type;

      size = sizeof(_aaxEffectInfo);
      for (i=0; i<_MAX_ENVELOPE_STAGES/2; i++)
      {
         eff->slot[i] = (_aaxEffectInfo*)(ptr + i*size);
         _aaxSetDefaultEffect2d(eff->slot[i], eff->pos);
      }
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxTimedPitchEffectDestroy(_effect_t* effect)
{
   free(effect->slot[0]->data);
   effect->slot[0]->data = NULL;
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
      _aaxRingBufferEnvelopeData* env = effect->slot[0]->data;
      if (env == NULL)
      {
         env =  calloc(1, sizeof(_aaxRingBufferEnvelopeData));
         effect->slot[0]->data = env;
      }

      if (env)
      {
         float nextval = effect->slot[0]->param[AAX_LEVEL0];
         float period = effect->info->period_rate;
         float timestep = 1.0f / period;
         int i;

         env->value0 = env->value = nextval;

         env->max_stages = _MAX_ENVELOPE_STAGES;
         for (i=0; i<_MAX_ENVELOPE_STAGES/2; i++)
         {
            float dt, value = nextval;
            uint32_t max_pos;

            max_pos = (uint32_t)-1;
            dt = effect->slot[i]->param[AAX_TIME0];
            if (dt != MAXFLOAT)
            {
               if (dt < timestep && dt > EPS) dt = timestep;
               max_pos = rintf(dt * period);
            }
            if (max_pos == 0)
            {
               env->max_stages = 2*i;
               break;
            }

            nextval = effect->slot[i]->param[AAX_LEVEL1];
            if (nextval == 0.0f) nextval = -1e-2f;
            env->step[2*i] = (nextval - value)/max_pos;
            env->max_pos[2*i] = max_pos;

            /* prevent a core dump for accessing an illegal slot */
            if (i == (_MAX_ENVELOPE_STAGES/2)-1) break;

            max_pos = (uint32_t)-1;
            dt = effect->slot[i]->param[AAX_TIME1];
            if (dt != MAXFLOAT)
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
            nextval = effect->slot[i+1]->param[AAX_LEVEL0];
            if (nextval == 0.0f) nextval = -1e-2f;
            env->step[2*i+1] = (nextval - value)/max_pos;
            env->max_pos[2*i+1] = max_pos;
         }
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
   }
   else
   {
      free(effect->slot[0]->data);
      effect->slot[0]->data = NULL;
   }
   rv = effect;
   return rv;
}

static _effect_t*
_aaxNewTimedPitchEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   unsigned int size = sizeof(_effect_t);
   _effect_t* rv = NULL;

   size += (_MAX_ENVELOPE_STAGES/2)*sizeof(_aaxEffectInfo);
   rv = calloc(1, size);
   if (rv)
   {
      _handle_t *handle = get_driver_handle(config);
      _aaxMixerInfo* info = handle ? handle->info : _info;
      char *ptr = (char*)rv + sizeof(_effect_t);
      _aaxRingBufferEnvelopeData *env;
      unsigned int no_steps;
      float dt, value;
      int i, stages;

      rv->id = EFFECT_ID;
      rv->info = info;
      rv->handle = handle;
      rv->slot[0] = (_aaxEffectInfo*)ptr;
      rv->pos = _eff_cvt_tbl[type].pos;
      rv->state = p2d->effect[rv->pos].state;
      rv->type = type;

      size = sizeof(_aaxEffectInfo);
      env = (_aaxRingBufferEnvelopeData*)p2d->effect[rv->pos].data;
      memcpy(rv->slot[0], &p2d->effect[rv->pos], size);
      rv->slot[0]->data = NULL;

      i = 0;
      if (env->max_pos[1] > env->max_pos[0]) i = 1;
      dt = p2d->effect[rv->pos].param[2*i+1] / env->max_pos[i];

      no_steps = env->max_pos[1];
      value = p2d->effect[rv->pos].param[AAX_LEVEL1]; 
      value += env->step[1] * no_steps;

      stages = _MIN(1+env->max_stages/2, _MAX_ENVELOPE_STAGES/2);
      for (i=1; i<stages; i++)
      {
         _aaxEffectInfo* slot;

         slot = (_aaxEffectInfo*)(ptr + i*size);
         rv->slot[i] = slot;

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
    { {  0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, MAXFLOAT, 4.0f, MAXFLOAT } },
    { {  0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, MAXFLOAT, 4.0f, MAXFLOAT } },
    { {  0.0f, 0.0f, 0.0f, 0.0f }, { 4.0f, MAXFLOAT, 4.0f, MAXFLOAT } }
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
   (_aaxEffectSetState*)&_aaxTimedPitchEffectSetState,
   NULL,
   (_aaxNewEffectHandle*)&_aaxNewTimedPitchEffectHandle,
   (_aaxEffectConvert*)&_aaxTimedPitchEffectSet,
   (_aaxEffectConvert*)&_aaxTimedPitchEffectGet,
   (_aaxEffectConvert*)&_aaxTimedPitchEffectMinMax
};

