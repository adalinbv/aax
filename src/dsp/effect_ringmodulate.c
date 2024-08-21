/*
 * SPDX-FileCopyrightText: Copyright © 2021-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2021-2023 by Adalin B.V.
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

#define VERSION	1.02
#define DSIZE	sizeof(_aaxRingBufferModulatorData)

static int _modulator_run(MIX_PTR_T, size_t, size_t, void*, void*, unsigned int);


static aaxEffect
_aaxModulatorEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 1, DSIZE);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxSetDefaultEffect2d(eff->slot[0], eff->pos, 0);
      rv = (aaxEffect)eff;
   }
   return rv;
}

static void
_aaxModulatorEffectReset(void *data)
{
}

static aaxEffect
_aaxModulatorEffectSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;
   aaxFilter rv = false;
   int stereo;

   assert(effect->info);

   stereo = (state & AAX_LFO_STEREO) ? true : false;
   state &= ~AAX_LFO_STEREO;

   if ((state & AAX_SOURCE_MASK) == 0) {
      state |= true;
   }

   effect->state = state;
   switch (state & (AAX_SOURCE_MASK & ~AAX_PURE_WAVEFORM))
   {
   case AAX_CONSTANT:
   case AAX_SAWTOOTH:
   case AAX_SQUARE:
   case AAX_TRIANGLE:
   case AAX_SINE:
   case AAX_CYCLOID:
   case AAX_IMPULSE:
   case AAX_RANDOMNESS:
   case AAX_RANDOM_SELECT:
   case AAX_ENVELOPE_FOLLOW:
   case AAX_TIMED_TRANSITION:
   {
      _aaxRingBufferModulatorData *modulator = effect->slot[0]->data;
      if (modulator == NULL)
      {
         modulator = _aax_aligned_alloc(DSIZE);
         effect->slot[0]->data = modulator;
         if (modulator)
         {
            effect->slot[0]->data_size = DSIZE;
            memset(modulator, 0, DSIZE);
         }
      }

      if (modulator)
      {
         int constant;
         float gain;

         modulator->run = _modulator_run;

         gain = effect->slot[0]->param[AAX_GAIN];
         modulator->amplitude = (gain < 0.0f) ? true : false;
         modulator->gain = fabsf(gain);

         modulator->lfo.convert = _linear;
         modulator->lfo.state = effect->state;
         modulator->lfo.fs = effect->info->frequency;
         modulator->lfo.period_rate = effect->info->period_rate;
         modulator->lfo.min = effect->slot[0]->param[AAX_LFO_OFFSET];
         modulator->lfo.max = modulator->lfo.min + effect->slot[0]->param[AAX_LFO_DEPTH];
         modulator->lfo.envelope = false;
         modulator->lfo.stereo_link = !stereo;

         modulator->lfo.min_sec = modulator->lfo.min/modulator->lfo.fs;
         modulator->lfo.max_sec = modulator->lfo.max/modulator->lfo.fs;
         modulator->lfo.depth = 1.0f;
         modulator->lfo.offset = 0.0f;
         modulator->lfo.f = effect->slot[0]->param[AAX_LFO_FREQUENCY];
         modulator->lfo.inverse = (state & AAX_INVERSE) ? true : false;

         constant = _lfo_set_timing(&modulator->lfo);
         if (!_lfo_set_function(&modulator->lfo, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      break;
   }
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      // intentional fall-through
   case AAX_FALSE:
      if (effect->slot[0]->data)
      {
         effect->slot[0]->destroy(effect->slot[0]->data);
         effect->slot[0]->data_size = 0;
         effect->slot[0]->data = NULL;
      }
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
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 1, 0);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[0], &p2d->effect[rv->pos]);
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
   "AAX_ringmodulator_effect_"AAX_MKSTR(VERSION), VERSION,
   (_aaxEffectCreateFn*)&_aaxModulatorEffectCreate,
   (_aaxEffectDestroyFn*)&_aaxEffectDestroy,
   (_aaxEffectResetFn*)&_aaxModulatorEffectReset,
   (_aaxEffectSetStateFn*)&_aaxModulatorEffectSetState,
   NULL,
   (_aaxNewEffectHandleFn*)&_aaxNewModulatorEffectHandle,
   (_aaxEffectConvertFn*)&_aaxModulatorEffectSet,
   (_aaxEffectConvertFn*)&_aaxModulatorEffectGet,
   (_aaxEffectConvertFn*)&_aaxModulatorEffectMinMax
};

static int
_modulator_run(MIX_PTR_T s, size_t end, size_t no_samples,
               void *data, void *env, unsigned int track)
{
   _aaxRingBufferModulatorData *modulate = data;
   float f, gain, p, step;
   int rv = false;
   int i;

   gain = modulate->gain;
   f = modulate->lfo.get(&modulate->lfo, env, s, track, end);
   if (f != 0.0f && gain > LEVEL_128DB)
   {
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

      rv = true;
   }
   return rv;
}
