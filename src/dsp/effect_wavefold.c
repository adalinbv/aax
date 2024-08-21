/*
 * SPDX-FileCopyrightText: Copyright © 2007-2024 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2024 by Adalin B.V.
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

#include <base/random.h>
#include "common.h"
#include "effects.h"
#include "dsp.h"
#include "arch.h"
#include "api.h"

#define VERSION 1.0
#define DSIZE	sizeof(_aaxRingBufferWaveFoldData)

static int _wavefold_run(MIX_PTR_T, size_t, size_t, void*, void*, unsigned int);
static void _wavefold_swap(void*, void*);

static aaxEffect
_aaxWaveFoldEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 2, DSIZE);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxSetDefaultEffect2d(eff->slot[0], eff->pos, 0);
      eff->slot[0]->swap = _wavefold_swap;
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_wavefold_reset(void *data)
{
   _aaxRingBufferWaveFoldData *wavefold = data;
   if (wavefold) {
      _lfo_reset(&wavefold->offset);
      _lfo_reset(&wavefold->threshold);
   }

   return true;
}

void
_wavefold_swap(void *d, void *s)
{
   _aaxEffectInfo *dst = d, *src = s;

   if (src->data && src->data_size)
   {
      if (!dst->data) {
          dst->data = _aaxAtomicPointerSwap(&src->data, dst->data);
          dst->data_size = src->data_size;
      }
      else
      {
         _aaxRingBufferWaveFoldData *deff = dst->data;
         _aaxRingBufferWaveFoldData *seff = src->data;

         assert(dst->data_size == src->data_size);

         _lfo_swap(&deff->offset, &seff->offset);
         _lfo_swap(&deff->threshold, &seff->threshold);
      }
   }
   dst->destroy = src->destroy;
   dst->swap = src->swap;
}

static aaxEffect
_aaxWaveFoldEffectSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;
   aaxEffect rv = false;
   int wstate;

   assert(effect->info);

   state &= ~AAX_LFO_STEREO;

   if ((state & AAX_SOURCE_MASK) == 0) {
      state |= true;
   }

   effect->state = state;
   wstate = state & (AAX_SOURCE_MASK & ~AAX_PURE_WAVEFORM);
   switch (wstate)
   {
   case AAX_CONSTANT:
   case AAX_TRIANGLE:
   case AAX_SINE:
   case AAX_SQUARE:
   case AAX_IMPULSE:
   case AAX_SAWTOOTH:
   case AAX_CYCLOID:
   case AAX_RANDOMNESS:
   case AAX_RANDOM_SELECT:
   case AAX_ENVELOPE_FOLLOW:
   case AAX_TIMED_TRANSITION:
   {
      _aaxRingBufferWaveFoldData *wavefold = effect->slot[0]->data;
      if (wavefold == NULL)
      {
         wavefold = _aax_aligned_alloc(DSIZE);
         effect->slot[0]->data = wavefold;
         if (wavefold)
         {
            effect->slot[0]->data_size = DSIZE;
            memset(wavefold, 0, DSIZE);
         }
      }

      if (wavefold)
      {
         float min, max;
         int constant;

         wavefold->run = _wavefold_run;

         _lfo_setup(&wavefold->offset, effect->info, effect->state);
         if (wstate == AAX_CONSTANT)
         {
            min = effect->slot[0]->param[AAX_DC_OFFSET];
            max = 0.0f;
         }
         else
         {
            min = effect->slot[0]->param[AAX_LFO_MIN];
            max = effect->slot[0]->param[AAX_DC_OFFSET];
         }
         wavefold->offset.min_sec = min/wavefold->offset.fs;
         wavefold->offset.max_sec = max/wavefold->offset.fs;
         wavefold->offset.f = effect->slot[0]->param[AAX_LFO_FREQUENCY];

         constant = _lfo_set_timing(&wavefold->offset);
         if (!_lfo_set_function(&wavefold->offset, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }

         _lfo_setup(&wavefold->threshold, effect->info, effect->state);
         if (wstate == AAX_CONSTANT)
         {
            min = effect->slot[0]->param[AAX_LFO_MAX];
            max = 0.0f;
         }
         else
         {
            min = effect->slot[0]->param[AAX_LFO_MIN];
            max = effect->slot[0]->param[AAX_LFO_MAX];
         }
         wavefold->threshold.min_sec = min/wavefold->offset.fs;
         wavefold->threshold.max_sec = max/wavefold->offset.fs;
         wavefold->threshold.f = effect->slot[0]->param[AAX_LFO_FREQUENCY];

         constant = _lfo_set_timing(&wavefold->threshold);
         if (!_lfo_set_function(&wavefold->threshold, constant)) {
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
_aaxNewWaveFoldEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 2, 0);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[0], &p2d->effect[rv->pos]);
      rv->slot[0]->swap = _wavefold_swap;
      rv->state = p2d->effect[rv->pos].state;
   }
   return rv;
}

static float
_aaxWaveFoldEffectSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   if (param == AAX_DC_OFFSET && ptype == AAX_DECIBEL) {
      rv = _lin2db(val);
   }
   return rv;
}

static float
_aaxWaveFoldEffectGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   if (param == AAX_DC_OFFSET && ptype == AAX_DECIBEL) {
      rv = _db2lin(val);
   }
   return rv;
}

static float
_aaxWaveFoldEffectMinMax(float val, int slot, unsigned char param)
{
  static const _eff_minmax_tbl_t _aaxWaveFoldRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { -1.0f, 0.01f, 0.0f, 0.0f }, { 1.0f, 50.0f, 1.0f, 1.0f } },
    { {  0.0f,  0.0f, 0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } },
    { {  0.0f,  0.0f, 0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } },
    { {  0.0f,  0.0f, 0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxWaveFoldRange[slot].min[param],
                       _aaxWaveFoldRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxWaveFoldEffect =
{
   "AAX_wavefold_effect", VERSION,
   (_aaxEffectCreateFn*)&_aaxWaveFoldEffectCreate,
   (_aaxEffectDestroyFn*)&_aaxEffectDestroy,
   (_aaxEffectResetFn*)&_wavefold_reset,
   (_aaxEffectSetStateFn*)&_aaxWaveFoldEffectSetState,
   NULL,
   (_aaxNewEffectHandleFn*)&_aaxNewWaveFoldEffectHandle,
   (_aaxEffectConvertFn*)&_aaxWaveFoldEffectSet,
   (_aaxEffectConvertFn*)&_aaxWaveFoldEffectGet,
   (_aaxEffectConvertFn*)&_aaxWaveFoldEffectMinMax
};

int
_wavefold_run(MIX_PTR_T s, size_t end, size_t no_samples,
              void *data, void *env, unsigned int track)
{
   _aaxRingBufferWaveFoldData *wavefold = data;
   float threshold, offset;
   int rv = false;

   // the offset parameter shifts the DC-offset form zero to a different level,
   // resulting in an asymmetric amplitude domain.
   offset = wavefold->offset.get(&wavefold->offset, env, s, track, end);
   _batch_dc_shift(s, s, no_samples, offset);

   // values above an amplitude threshold level will be folded back towards zero
   threshold = wavefold->threshold.get(&wavefold->threshold, env, s, track, end);
   _batch_wavefold(s, s, no_samples, threshold);

   return rv;
}

