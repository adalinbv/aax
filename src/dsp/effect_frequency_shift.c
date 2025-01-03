/*
 * SPDX-FileCopyrightText: Copyright © 2024 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2024 by Adalin B.V.
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

#include "common.h"
#include "effects.h"
#include "dsp.h"
#include "api.h"
#include "arch.h"


/*
 * 6. Adjust the table in _aax<DSP>MinMax for the minimum and maximum
 *    allowed values vor every parameter in every slot
 */

#define VERSION 1.0
#define DSIZE   sizeof(_aaxRingBufferFrequencyShiftData)

static int _freqshift_run(MIX_PTR_T, CONST_MIX_PTR_T, size_t, size_t, void*, void*, unsigned int);
static void _freqshift_swap(void*, void*);

static aaxEffect
_aaxFrequencyShiftCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   size_t dsize = sizeof(_aaxRingBufferFrequencyShiftData);
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 1, dsize);
   aaxEffect rv = NULL;

   if (eff)
   {
      _aaxSetDefaultEffect2d(eff->slot[0], eff->pos, 0);
      eff->slot[0]->swap = _freqshift_swap;
      rv = (aaxEffect)eff;
   }
   return rv;
}

static int
_freqshift_reset(void *data)
{
   _aaxRingBufferFrequencyShiftData *freqshift = data;
   if (freqshift) {
      _lfo_reset(&freqshift->lfo);
   }

   return true;
}

void
_freqshift_swap(void *d, void *s)
{
   _aaxEffectInfo *dst = d, *src = s;

   if (src->data && src->data_size)
   {
      if (!dst->data) {
          _aaxAtomicPointerSwap(&src->data, &dst->data);
          dst->data_size = src->data_size;
      }
      else
      {
         _aaxRingBufferFrequencyShiftData *deff = dst->data;
         _aaxRingBufferFrequencyShiftData *seff = src->data;

         assert(dst->data_size == src->data_size);

         _lfo_swap(&deff->lfo, &seff->lfo);
      }
   }
   dst->destroy = src->destroy;
   dst->swap = src->swap;
}

static _effect_t*
_aaxNewFrequencyShiftHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 2, 0);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[0], &p2d->effect[rv->pos]);
      rv->slot[0]->swap = _freqshift_swap;
      rv->state = p2d->effect[rv->pos].state;
   }
   return rv;
}

static aaxEffect
_aaxFrequencyShiftSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;
   aaxEffect rv = false;
   int wstate;

   assert(effect->info);

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
      _aaxRingBufferFrequencyShiftData *freqshift = effect->slot[0]->data;
      if (freqshift == NULL)
      {
         freqshift = _aax_aligned_alloc(DSIZE);
         effect->slot[0]->data = freqshift;
         if (freqshift)
         {
            effect->slot[0]->data_size = DSIZE;
            memset(freqshift, 0, DSIZE);
         }
      }

      if (freqshift)
      {
         _aaxLFOData *lfo = &freqshift->lfo;
         int i, constant;
         float min, max;

         freqshift->run = _freqshift_run;

         for (i=0; i<RB_MAX_TRACKS; ++i) {
             freqshift->phase[i] = GMATH_PI_2;
         }

         // start at 90 degrees phase so we can subsitute fast_sin for cos
         freqshift->gain = effect->slot[0]->param[AAX_GAIN];

         _lfo_setup(&freqshift->lfo, effect->info, effect->state);
         if (wstate == AAX_CONSTANT)
         {
            min = GMATH_2PI*effect->slot[0]->param[AAX_LFO_MIN]/lfo->fs;
            max = 0.0f;
         }
         else
         {
            min = GMATH_2PI*effect->slot[0]->param[AAX_LFO_MIN]/lfo->fs;
            max = GMATH_2PI*effect->slot[0]->param[AAX_LFO_MAX]/lfo->fs;
         }

         // Dividing by freqshift->lfo.fs again is not a mistake here.
         // The previous cases were to normalize the frequency shift.
         freqshift->lfo.min_sec = min/freqshift->lfo.fs;
         freqshift->lfo.max_sec = max/freqshift->lfo.fs;
         freqshift->lfo.f = effect->slot[0]->param[AAX_LFO_FREQUENCY];

         constant = _lfo_set_timing(&freqshift->lfo);
         if (!_lfo_set_function(&freqshift->lfo, constant)) {
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

static float
_aaxFrequencyShiftSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxFrequencyShiftGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}


#define MAX	MAX_CUTOFF
static float
_aaxFrequencyShiftMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxFrequencyShiftRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.01f, -MAX, -MAX }, { 1.0f, 50.0f,  MAX,  MAX } },
    { { 0.0f,  0.0f, 0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f,  0.0f, 0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f,  0.0f, 0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxFrequencyShiftRange[slot].min[param],
                       _aaxFrequencyShiftRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxFrequencyShiftEffect =
{
   "AAX_frequency_shift", VERSION,
   (_aaxEffectCreateFn*)&_aaxFrequencyShiftCreate,
   (_aaxEffectDestroyFn*)&_aaxEffectDestroy,
   (_aaxEffectResetFn*)&_freqshift_reset,
   (_aaxEffectSetStateFn*)&_aaxFrequencyShiftSetState,
   NULL,
   (_aaxNewEffectHandleFn*)&_aaxNewFrequencyShiftHandle,
   (_aaxEffectConvertFn*)&_aaxFrequencyShiftSet,
   (_aaxEffectConvertFn*)&_aaxFrequencyShiftGet,
   (_aaxEffectConvertFn*)&_aaxFrequencyShiftMinMax
};

int
_freqshift_run(MIX_PTR_T dptr, CONST_MIX_PTR_T sptr, size_t end,
               size_t no_samples, void *data, void *env, unsigned int track)
{
   _aaxRingBufferFrequencyShiftData *freqshift = data;
   float phase = freqshift->phase[track]; 
   float gain, shift;
   int rv = false;

   gain = freqshift->gain;
   shift = freqshift->lfo.get(&freqshift->lfo, env, sptr, track, end);
   if (gain > LEVEL_128DB)
   {
      float mix = 1.0f-gain;
      int i = no_samples;
      MIX_PTR_T d = dptr;
      do
      {
         // phase started at 2*PI/4 to simulate cos
         *d++ = *sptr++ * (mix + gain*fast_sin(phase));

         // wrap between 0.0f and GMATH_2PI
         phase += shift;
         if (phase > GMATH_2PI) phase -= GMATH_2PI;
         else if (phase < 0.0) phase += GMATH_2PI;
      }
      while(--i);
      rv = true;
   }
   else {
      phase = _wrap_max(phase+no_samples*shift, GMATH_2PI);
   }
   assert(track < _AAX_MAX_SPEAKERS);
   freqshift->phase[track] = phase;

   return rv;
}

