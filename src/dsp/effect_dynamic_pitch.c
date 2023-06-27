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

#include "lfo.h"
#include "effects.h"
#include "api.h"
#include "arch.h"

#define VERSION	1.02
#define DSIZE	sizeof(_aaxLFOData)

static aaxEffect
_aaxDynamicPitchEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 1, DSIZE);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxSetDefaultEffect2d(eff->slot[0], eff->pos, 0);
      eff->slot[0]->destroy = _lfo_destroy;
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxDynamicPitchEffectDestroy(_effect_t* effect)
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
_aaxDynamicPitchEffectSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;
   aaxEffect rv = AAX_FALSE;

   assert(effect->info);

   effect->state = state;
   switch (state & AAX_SOURCE_MASK)
   {
   case AAX_CONSTANT:
   case AAX_SAWTOOTH:
   case AAX_SQUARE:
   case AAX_TRIANGLE:
   case AAX_SINE:
   case AAX_CYCLOID:
   case AAX_IMPULSE:
   case AAX_RANDOMNESS:
   case AAX_ENVELOPE_FOLLOW:
   {
      _aaxLFOData* lfo = effect->slot[0]->data;
      if (lfo == NULL) {
         effect->slot[0]->data = lfo = _lfo_create();
      }

      if (lfo)
      {
         float depth = 0.5f*effect->slot[0]->param[AAX_LFO_DEPTH];
         int constant;

         _lfo_setup(lfo, effect->info, effect->state);

         lfo->envelope = AAX_FALSE;
         lfo->min_sec = (1.0f - depth)/lfo->fs;
         lfo->max_sec = (1.0f + depth)/lfo->fs;
         lfo->delay = -effect->slot[0]->param[AAX_INITIAL_DELAY];
         lfo->f = effect->slot[0]->param[AAX_LFO_FREQUENCY];

         constant = _lfo_set_timing(lfo);
         if (!_lfo_set_function(lfo, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      break;
   }
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      // inetnional fall-through
   case AAX_FALSE:
      if (effect->slot[0]->data)
      {
         effect->slot[0]->destroy(effect->slot[0]->data);
         effect->slot[0]->data = NULL;
      }
      break;
   }
   rv = effect;
   return rv;
}

static _effect_t*
_aaxNewDynamicPitchEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 1, 0);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[0], &p2d->effect[rv->pos]);
      rv->slot[0]->destroy = _lfo_destroy;
      rv->state = p2d->effect[rv->pos].state;
   }
   return rv;
}

static float
_aaxDynamicPitchEffectSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{  
   float rv = val;
   return rv;
}
   
static float
_aaxDynamicPitchEffectGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{  
   float rv = val;
   return rv;
}

static float
_aaxDynamicPitchEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxDynamicPitchRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.01f, 0.0f, 0.0f }, { 10.0f, 50.0f, 1.0f, 1.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {  0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {  0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {  0.0f,  0.0f, 0.0f, 0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxDynamicPitchRange[slot].min[param],
                       _aaxDynamicPitchRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxDynamicPitchEffect =
{
   AAX_FALSE,
   "AAX_dynamic_pitch_effect_"AAX_MKSTR(VERSION), VERSION,
   (_aaxEffectCreate*)&_aaxDynamicPitchEffectCreate,
   (_aaxEffectDestroy*)&_aaxDynamicPitchEffectDestroy,
   (_aaxEffectReset*)&_lfo_reset,
   (_aaxEffectSetState*)&_aaxDynamicPitchEffectSetState,
   NULL,
   (_aaxNewEffectHandle*)&_aaxNewDynamicPitchEffectHandle,
   (_aaxEffectConvert*)&_aaxDynamicPitchEffectSet,
   (_aaxEffectConvert*)&_aaxDynamicPitchEffectGet,
   (_aaxEffectConvert*)&_aaxDynamicPitchEffectMinMax
};

