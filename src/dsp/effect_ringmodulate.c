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


static void _modulator_run(MIX_PTR_T, size_t, size_t, void*, void*, unsigned int);


static aaxEffect
_aaxModulatorEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 1);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxSetDefaultEffect2d(eff->slot[0], eff->pos, 0);
      eff->slot[0]->destroy = destroy;
      eff->slot[0]->swap = swap;
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_aaxModulatorEffectDestroy(_effect_t* effect)
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
_aaxModulatorEffectSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;
   aaxFilter rv = AAX_FALSE;
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
      _aaxRingBufferModulatorData *modulator = effect->slot[0]->data;
      if (modulator == NULL)
      {
         modulator = calloc(1, sizeof(_aaxRingBufferModulatorData));
         effect->slot[0]->data = modulator;
      }

      if (modulator)
      {
         int constant;
         float gain;

         modulator->run = _modulator_run;

         gain = effect->slot[0]->param[AAX_GAIN];
         modulator->amplitude = (gain < 0.0f) ? AAX_TRUE : AAX_FALSE;
         modulator->gain = fabsf(gain);

         modulator->lfo.convert = _linear;
         modulator->lfo.state = effect->state;
         modulator->lfo.fs = effect->info->frequency;
         modulator->lfo.period_rate = effect->info->period_rate;
         modulator->lfo.min = effect->slot[0]->param[AAX_LFO_OFFSET];
         modulator->lfo.max = modulator->lfo.min + effect->slot[0]->param[AAX_LFO_DEPTH];
         modulator->lfo.envelope = AAX_FALSE;
         modulator->lfo.stereo_lnk = !stereo;

         modulator->lfo.min_sec = modulator->lfo.min/modulator->lfo.fs;
         modulator->lfo.max_sec = modulator->lfo.max/modulator->lfo.fs;
         modulator->lfo.depth = 1.0f;
         modulator->lfo.offset = 0.0f;
         modulator->lfo.f = effect->slot[0]->param[AAX_LFO_FREQUENCY];
         modulator->lfo.inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;

         constant = _lfo_set_timing(&modulator->lfo);
         if (!_lfo_set_function(&modulator->lfo, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      break;
   }
   case AAX_FALSE:
      if (effect->slot[0]->data)
      {
         effect->slot[0]->destroy(effect->slot[0]->data);
         effect->slot[0]->data = NULL;
      }
      break;
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      break;
   }
   rv = effect;
   return rv;
}

static _effect_t*
_aaxNewModulatorEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 1);

   if (rv)
   {
      unsigned int size = sizeof(_aaxEffectInfo);

      memcpy(rv->slot[0], &p2d->effect[rv->pos], size);
      rv->slot[0]->destroy = destroy;
      rv->slot[0]->swap = swap;
      rv->slot[0]->data = NULL;

      rv->state = p2d->effect[rv->pos].state;
   }
   return rv;
}

static float
_aaxModulatorEffectSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{  
   float rv = val;
   return rv;
}
   
static float
_aaxModulatorEffectGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{  
   float rv = val;
   return rv;
}

static float
_aaxModulatorEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxModulatorRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                /* max[4] */
    { {-1.0f,  0.01f, 0.01f, 0.01f }, { 1.0f, 50.0f, 10000.0f, 10000.0f } },
    { { 0.0f,   0.0f,  0.0f,  0.0f }, { 0.0f,  0.0f,     0.0f,     0.0f } },
    { { 0.0f,   0.0f,  0.0f,  0.0f }, { 0.0f,  0.0f,     0.0f,     0.0f } },
    { { 0.0f,   0.0f,  0.0f,  0.0f }, { 0.0f,  0.0f,     0.0f,     0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxModulatorRange[slot].min[param],
                       _aaxModulatorRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxModulatorEffect =
{
   AAX_TRUE,
   "AAX_ringmodulator_effect", 1.0f,
   (_aaxEffectCreate*)&_aaxModulatorEffectCreate,
   (_aaxEffectDestroy*)&_aaxModulatorEffectDestroy,
   (_aaxEffectSetState*)&_aaxModulatorEffectSetState,
   NULL,
   (_aaxNewEffectHandle*)&_aaxNewModulatorEffectHandle,
   (_aaxEffectConvert*)&_aaxModulatorEffectSet,
   (_aaxEffectConvert*)&_aaxModulatorEffectGet,
   (_aaxEffectConvert*)&_aaxModulatorEffectMinMax
};

void
_modulator_run(MIX_PTR_T s, size_t end, size_t no_samples, void *data, void *env, unsigned int track)
{
      _aaxRingBufferModulatorData *modulate = data;
      float f, gain, p, step;
      unsigned int i;

      gain = modulate->gain;
      f = modulate->lfo.get(&modulate->lfo, env, s, track, end);
      step = f/(GMATH_2PI*no_samples);

      p = modulate->phase[track];
      if (modulate->amplitude)
      {
         gain *= 0.5f;
         for (i=0; i<end; ++i)
         {
            s[i] *= (1.0f - gain*(1.0f + fast_sin(p)));
            p = fmodf(p+step, GMATH_2PI);
         }
      }
      else
      {
         for (i=0; i<end; ++i)
         {
            s[i] *= gain*fast_sin(p);
            p = fmodf(p+step, GMATH_2PI);
         }
      }
      assert(track < _AAX_MAX_SPEAKERS);
      modulate->phase[track] = p;
}
