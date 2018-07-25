/*
 * Copyright 2018 by Erik Hofman.
 * Copyright 2018 by Adalin B.V.
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

#include <base/types.h>		/*  for rintf */
#include <base/gmath.h>

#include "effects.h"
#include "api.h"
#include "arch.h"


static aaxEffect
_aaxRingModulateEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 1);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxSetDefaultEffect2d(eff->slot[0], eff->pos, 0);
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxRingModulateEffectDestroy(_effect_t* effect)
{
   free(effect);
   return AAX_TRUE;
}

static aaxEffect
_aaxRingModulateEffectSetState(_effect_t* effect, int state)
{
   _aaxRingModulatorData *data = effect->slot[0]->data;
   if (state)
   {
      if (!data)
      {
         data = calloc(1, sizeof(_aaxRingModulatorData));
         effect->slot[0]->data = data;
      }
      if (data)
      {
         float f = effect->slot[0]->param[AAX_LFO_FREQUENCY];
         float fs = effect->info->frequency;
         data->step = GMATH_2PI*fs/f;
      }
   }
   else
   {
      free(data);
      effect->slot[0]->data = NULL;
   }

   return  effect;
}

static _effect_t*
_aaxNewRingModulateEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 1);

   if (rv)
   {
      unsigned int size = sizeof(_aaxEffectInfo);

      memcpy(rv->slot[0], &p2d->effect[rv->pos], size);
//    rv->slot[0]->data = ;

      rv->state = p2d->effect[rv->pos].state;
   }
   return rv;
}

static float
_aaxRingModulateEffectSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{  
   float rv = val;
   return rv;
}
   
static float
_aaxRingModulateEffectGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{  
   float rv = val;
   return rv;
}

static float
_aaxRingModulateEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxRingModulateRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                /* max[4] */
    { { 10.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 10000.0f, 0.0f, 0.0f } },
    { {  0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f,     0.0f, 0.0f, 0.0f } },
    { {  0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f,     0.0f, 0.0f, 0.0f } },
    { {  0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f,     0.0f, 0.0f, 0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxRingModulateRange[slot].min[param],
                       _aaxRingModulateRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxRingModulateEffect =
{
   AAX_TRUE,
   "AAX_ringmodulator_effect", 1.0f,
   (_aaxEffectCreate*)&_aaxRingModulateEffectCreate,
   (_aaxEffectDestroy*)&_aaxRingModulateEffectDestroy,
   (_aaxEffectSetState*)&_aaxRingModulateEffectSetState,
   NULL,
   (_aaxNewEffectHandle*)&_aaxNewRingModulateEffectHandle,
   (_aaxEffectConvert*)&_aaxRingModulateEffectSet,
   (_aaxEffectConvert*)&_aaxRingModulateEffectGet,
   (_aaxEffectConvert*)&_aaxRingModulateEffectMinMax
};

