/*
 * Copyright 2007-2019 by Erik Hofman.
 * Copyright 2009-2019 by Adalin B.V.
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

#include <software/rbuf_int.h>
#include "effects.h"
#include "arch.h"
#include "dsp.h"
#include "api.h"

#define DSIZE		sizeof(_aaxRingBufferDelayEffectData)

static aaxEffect
_aaxFlangingEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 1, DSIZE);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxSetDefaultEffect2d(eff->slot[0], eff->pos, 0);
      eff->slot[0]->destroy = _delay_destroy;
      eff->slot[0]->swap = _delay_swap;
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxFlangingEffectDestroy(_effect_t* effect)
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
_aaxFlangingEffectSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;
   aaxEffect rv = AAX_FALSE;
   int stereo;

   assert(effect->info);

   stereo = (state & AAX_LFO_STEREO) ? AAX_TRUE : AAX_FALSE;
   state &= ~AAX_LFO_STEREO;

   effect->state = state;
   switch (state & ~AAX_INVERSE)
   {
   case AAX_CONSTANT_VALUE:
   case AAX_TRIANGLE_WAVE:
   case AAX_SINE_WAVE:
   case AAX_SQUARE_WAVE:
   case AAX_IMPULSE_WAVE:
   case AAX_SAWTOOTH_WAVE:
   case AAX_ENVELOPE_FOLLOW:
   {
      _aaxRingBufferDelayEffectData* data = effect->slot[0]->data;

      data = _delay_create(data, effect->info);
      effect->slot[0]->data = data;
      if (data)
      {
         float offset = effect->slot[0]->param[AAX_LFO_OFFSET];
         float depth = effect->slot[0]->param[AAX_LFO_DEPTH];
         float fs = 48000.0f;
         int t, constant;

         if (effect->info) {
            fs = effect->info->frequency;
         }

         data->run = _delay_run;
         data->flanger = AAX_TRUE;
         data->feedback = effect->slot[0]->param[AAX_DELAY_GAIN];

         data->lfo.convert = _linear;
         data->lfo.state = effect->state;
         data->lfo.fs = fs;
         data->lfo.period_rate = effect->info->period_rate;

         data->lfo.min_sec = FLANGING_MIN;
         data->lfo.max_sec = FLANGING_MAX;
         data->lfo.depth = depth;
         data->lfo.offset = offset;
         data->lfo.f = effect->slot[0]->param[AAX_LFO_FREQUENCY];
         data->lfo.inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;
         data->lfo.stereo_lnk = !stereo;

         if ((data->lfo.offset + data->lfo.depth) > 1.0f) {
            data->lfo.depth = 1.0f - data->lfo.offset;
         }

         constant = _lfo_set_timing(&data->lfo);

         data->delay.gain = effect->slot[0]->param[AAX_DELAY_GAIN];
         for (t=0; t<_AAX_MAX_SPEAKERS; t++) {
            data->delay.sample_offs[t] = (size_t)data->lfo.value[t];
         }

         if (!_lfo_set_function(&data->lfo, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      break;
   }
   case AAX_FALSE:
   {
      if (effect->slot[0]->data)
      {
         effect->slot[0]->destroy(effect->slot[0]->data);
         effect->slot[0]->data = NULL;
      }
      break;
   }
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      break;
   }
   rv = effect;
   return rv;
}

static _effect_t*
_aaxNewFlangingEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 1, DSIZE);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[0], &p2d->effect[rv->pos]);
      rv->slot[0]->destroy = _delay_destroy;
      rv->slot[0]->swap = _delay_swap;
      rv->slot[0]->data = NULL;

      rv->state = p2d->effect[rv->pos].state;
   }
   return rv;
}

static float
_aaxFlangingEffectSet(float val, int ptype, unsigned char param)
{  
   float rv = val;
   if ((param == AAX_DELAY_GAIN) && (ptype == AAX_DECIBEL)) {
      rv = _lin2db(val);
   }
   else if ((param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET) && 
            (ptype == AAX_MICROSECONDS)) {
       rv = (val*1e-6f)/FLANGING_MAX;
   }
   return rv;
}
   
static float
_aaxFlangingEffectGet(float val, int ptype, unsigned char param)
{  
   float rv = val;
   if ((param == AAX_DELAY_GAIN) && (ptype == AAX_DECIBEL)) {
      rv = _db2lin(val);
   }
   else if ((param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET) &&
            (ptype == AAX_MICROSECONDS)) {
       rv = val*FLANGING_MAX*1e6f;
   }
   return rv;
}

static float
_aaxFlangingEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxFlangingRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.01f, 0.0f, 0.0f }, { 1.0f, 10.0f, 1.0f, 1.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxFlangingRange[slot].min[param],
                       _aaxFlangingRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxFlangingEffect =
{
   AAX_FALSE,
   "AAX_flanging_effect", 1.0f,
   (_aaxEffectCreate*)&_aaxFlangingEffectCreate,
   (_aaxEffectDestroy*)&_aaxFlangingEffectDestroy,
   (_aaxEffectSetState*)&_aaxFlangingEffectSetState,
   NULL,
   (_aaxNewEffectHandle*)&_aaxNewFlangingEffectHandle,
   (_aaxEffectConvert*)&_aaxFlangingEffectSet,
   (_aaxEffectConvert*)&_aaxFlangingEffectGet,
   (_aaxEffectConvert*)&_aaxFlangingEffectMinMax
};

