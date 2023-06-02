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

static int
_aaxPitchEffectDestroy(_effect_t* effect)
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
         env =  _aax_aligned_alloc(DSIZE);
         effect->slot[0]->data = env;
         if (env) memset(env, 0, DSIZE);
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

         env->sustain = AAX_TRUE;
         env->value0 = env->value = 1.0f;
         env->value = value;
         env->max_stages = 1;

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
   AAX_TRUE,
   "AAX_pitch_effect_"AAX_MKSTR(VERSION), VERSION,
   (_aaxEffectCreate*)&_aaxPitchEffectCreate,
   (_aaxEffectDestroy*)&_aaxPitchEffectDestroy,
   (_aaxEffectReset*)&_env_reset,
   (_aaxEffectSetState*)&_aaxPitchEffectSetState,
   NULL,
   (_aaxNewEffectHandle*)&_aaxNewPitchEffectHandle,
   (_aaxEffectConvert*)&_aaxPitchEffectSet,
   (_aaxEffectConvert*)&_aaxPitchEffectGet,
   (_aaxEffectConvert*)&_aaxPitchEffectMinMax
};

