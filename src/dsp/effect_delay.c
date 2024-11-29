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

#include <base/types.h>		/*  for rintf */
#include <base/gmath.h>
#include <base/random.h>

#include <software/rbuf_int.h>
#include "effects.h"
#include "arch.h"
#include "dsp.h"
#include "api.h"

/*
 * Implements phasing, chorus, flanging and delay-line
 *
 * phasing and chorus are now exactly the same except the time range of the
 * linear, typeless, values 0.0 .. 1.0
 *
 * flaning is converted o chorus by setting the feedback gain of the chorus
 * effect from the delay gain value of the flanger effect and setting the delay
 * level of the chorus effect to 0.0
 *
 * delay-line differs from chorus only by the maxmimum allowed delay and it's
 * corresponding linear, typeless, values 0.0 .. 1.0
 */

#define VERSION		1.2
#define DSIZE		sizeof(_aaxRingBufferDelayEffectData)

static void* _delay_create(void*, void*, bool, int, float);
static void _delay_swap(void*, void*);
static void _delay_destroy(void*);
static void _delay_reset(void*);
static size_t _delay_prepare(MIX_PTR_T, MIX_PTR_T, size_t, void*, unsigned int);
static int _delay_run(void*, MIX_PTR_T, MIX_PTR_T, MIX_PTR_T, size_t, size_t, size_t, size_t, void*, void*, unsigned int);


static aaxEffect
_aaxDelayEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 3, DSIZE);
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

static aaxEffect
_aaxDelayEffectSetState(_effect_t* effect, int state, float delay_gain, float feedback_gain, float max_delay)
{
   void *handle = effect->handle;
   aaxEffect rv = false;
   int stages, stereo;

   assert(effect->info);

   stereo = (state & AAX_LFO_STEREO) ? true : false;
   state &= ~AAX_LFO_STEREO;

   stages = (state & AAX_ORDER_MASK) >> 8;
   if (stages == 0) stages = 1;
   state &= ~AAX_ORDER_MASK;

   if ((state & (AAX_EFFECT_1ST_ORDER|AAX_EFFECT_2ND_ORDER)) == 0) {
      state |= (AAX_EFFECT_1ST_ORDER|AAX_EFFECT_2ND_ORDER);
   }

   if ((state & AAX_ALL_SOURCE_MASK) == 0) {
      state |= AAX_CONSTANT;
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
      _aaxRingBufferDelayEffectData* data = effect->slot[0]->data;
      bool fbhist = (feedback_gain > LEVEL_32DB) ? true : false;

      data = _delay_create(data, effect->info, fbhist, state, max_delay);
      if (data)
      {
         effect->slot[0]->data = data;
         effect->slot[0]->data_size = DSIZE;
      }

      if (data)
      {
         _aaxRingBufferFreqFilterData *flt = data->freq_filter;
         float fc = effect->slot[1]->param[AAX_DELAY_CUTOFF_FREQUENCY & 0xF];
         float fmax = effect->slot[1]->param[AAX_DELAY_CUTOFF_FREQUENCY_HF & 0xF];
         float offset = effect->slot[0]->param[AAX_LFO_OFFSET];
         float depth = effect->slot[0]->param[AAX_LFO_DEPTH];
         float fs = 48000.0f;
         int t, constant;

         if (effect->info) {
            fs = effect->info->frequency;
         }
         fc = CLIP_FREQUENCY(fc, fs);
         fmax = CLIP_FREQUENCY(fmax, fs);

         if ((fc > MINIMUM_CUTOFF && fc < HIGHEST_CUTOFF(fs)) ||
             (fmax > MINIMUM_CUTOFF && fmax < HIGHEST_CUTOFF(fs)))
         {
            if ((state & AAX_ORDER_MASK) == 0) {
               state |= AAX_2ND_ORDER;
            }

            if (!flt)
            {
               flt = _aax_aligned_alloc(sizeof(_aaxRingBufferFreqFilterData));
               if (flt)
               {
                  memset(flt, 0, sizeof(_aaxRingBufferFreqFilterData));
                  flt->freqfilter = _aax_aligned_alloc(sizeof(_aaxRingBufferFreqFilterHistoryData));
                  if (flt->freqfilter) {
                     memset(flt->freqfilter, 0, sizeof(_aaxRingBufferFreqFilterHistoryData));
                  }
               }
            }
            else
            {
               _aax_aligned_free(flt);
               flt = NULL;
            }
         }
         else if (flt)
         {
            _aax_aligned_free(flt);
            flt = NULL;
         }

         data->freq_filter = flt;
         data->prepare = _delay_prepare;
         data->run = _delay_run;

         _lfo_setup(&data->lfo, effect->info, effect->state);

         data->lfo.min_sec = _MIN(offset, max_delay);
         data->lfo.max_sec = _MIN(offset+depth, max_delay);

         data->lfo.f = effect->slot[0]->param[AAX_LFO_FREQUENCY];
         data->lfo.inverse = (state & AAX_INVERSE) ? true : false;

         if ((data->lfo.offset + data->lfo.depth) > 1.0f) {
            data->lfo.depth = 1.0f - data->lfo.offset;
         }

         data->bitcrush.run = _bitcrusher_run;

         /* sample rate conversion */
         data->bitcrush.fs = effect->slot[2]->param[AAX_FEEDBACK_SAMPLE_RATE & 0xF];

         /* bit reduction */
         offset = effect->slot[2]->param[AAX_FEEDBACK_BITCRUSH_LEVEL & 0xF];
         if (offset > LEVEL_32DB || fs > 1000.0f)
         {
            data->bitcrush.lfo.convert = _linear;
            data->bitcrush.lfo.state = effect->state;
            data->bitcrush.lfo.fs = fs;
            data->bitcrush.lfo.period_rate = effect->info->period_rate;
            data->bitcrush.lfo.stereo_link = !stereo;

            data->bitcrush.lfo.min_sec = offset/fs;
            data->bitcrush.lfo.max_sec = data->bitcrush.lfo.min_sec;
            data->bitcrush.lfo.depth = 1.0f;
            data->bitcrush.lfo.offset = 0.0f;

            data->bitcrush.lfo.f = 0.0f;
            data->bitcrush.lfo.inverse = (state & AAX_INVERSE) ? true : false;

            constant = _lfo_set_timing(&data->bitcrush.lfo);
            _lfo_set_function(&data->bitcrush.lfo, constant);
         }

         constant = _lfo_set_timing(&data->lfo);

         data->no_delays = stages;
         data->feedback = feedback_gain;
         data->delay.gain = delay_gain;
         for (t=0; t<_AAX_MAX_SPEAKERS; t++) {
            data->delay.sample_offs[t] = (size_t)data->lfo.value[t];
         }

         if (!_lfo_set_function(&data->lfo, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
         else if (flt) // add a frequency filter
         {
            int stages;

            flt->fs = fs;
            flt->run = _freqfilter_run;

            flt->high_gain = data->delay.gain;
            flt->low_gain = 0.0f;
            _freqfilter_normalize_gains(flt);

            if (state & AAX_48DB_OCT) stages = 4;
            else if (state & AAX_36DB_OCT) stages = 3;
            else if (state & AAX_24DB_OCT) stages = 2;
            else if (state & AAX_6DB_OCT) stages = 0;
            else stages = 1;

            flt->no_stages = stages;
            flt->state = (state & AAX_BESSEL) ? AAX_BESSEL : AAX_BUTTERWORTH;
            flt->Q = effect->slot[1]->param[AAX_DELAY_RESONANCE & 0xF];
            flt->type = (flt->high_gain >= flt->low_gain) ? LOWPASS : HIGHPASS;
            flt->fc_low = fc;
            flt->fc_high = fmax;

            if ((state & AAX_SOURCE_MASK) == AAX_RANDOM_SELECT)
            {
               float lfc2 = _lin2log(fmax);
               float lfc1 = _lin2log(fc);

               flt->random = true;

               lfc1 += (lfc2 - lfc1)*_aax_random();
               fc = _log2lin(lfc1);
            }

            if (flt->state == AAX_BESSEL) {
                _aax_bessel_compute(fc, flt);
            }
            else
            {
               if (flt->type == HIGHPASS)
               {
                  float g = flt->high_gain;
                  flt->high_gain = flt->low_gain;
                  flt->low_gain = g;
               }
               _aax_butterworth_compute(fc, flt);
            }

            if (data->lfo.f)
            {
               _aaxLFOData* lfo = flt->lfo;

               if (lfo == NULL) {
                  lfo = flt->lfo = _lfo_create();
               }
               else if (lfo)
               {
                  _lfo_destroy(flt->lfo);
                   lfo = flt->lfo = NULL;
               }

               if (lfo)
               {
                  int constant;

                  _lfo_setup(lfo, effect->info, state);

                  /* sweeprate */
                  lfo->min = fc;
                  lfo->max = fmax;

                  if (state & AAX_LFO_EXPONENTIAL)
                  {
                     lfo->convert = _logarithmic;
                     if (fabsf(lfo->max - lfo->min) < 200.0f)
                     {
                        lfo->min = 0.5f*(lfo->min + lfo->max);
                        lfo->max = lfo->min;
                     }
                     else if (lfo->max < lfo->min)
                     {
                        float f = lfo->max;
                        lfo->max = lfo->min;
                        lfo->min = f;
                        state ^= AAX_INVERSE;
                     }
                     lfo->min = _lin2log(lfo->min);
                     lfo->max = _lin2log(lfo->max);
                  }
                  else
                  {
                     if (fabsf(lfo->max - lfo->min) < 200.0f)
                     {
                        lfo->min = 0.5f*(lfo->min + lfo->max);
                        lfo->max = lfo->min;
                     }
                     else if (lfo->max < lfo->min)
                     {
                        float f = lfo->max;
                        lfo->max = lfo->min;
                        lfo->min = f;
                        state ^= AAX_INVERSE;
                     }
                  }

                  lfo->min_sec = lfo->min/lfo->fs;
                  lfo->max_sec = lfo->max/lfo->fs;
                  lfo->f = data->lfo.f;

                  constant = _lfo_set_timing(lfo);
                  lfo->envelope = false;

                  if (!_lfo_set_function(lfo, constant)) {
                     _aaxErrorSet(AAX_INVALID_PARAMETER);
                  }
               }
            }
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

static aaxEffect
_aaxDelayLineEffectSetState(_effect_t* effect, int state)
{
   float delay = effect->slot[0]->param[AAX_DELAY_GAIN];
   float feedback = effect->slot[1]->param[AAX_FEEDBACK_GAIN & 0xF];

   return _aaxDelayEffectSetState(effect, state, delay, feedback, DELAY_MAX);
}

static _effect_t*
_aaxNewDelayEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 3, 0);

   if (rv)
   {
      _aax_dsp_copy(rv->slot[0], &p2d->effect[rv->pos]);
      rv->slot[0]->destroy = _delay_destroy;
      rv->slot[0]->swap = _delay_swap;

      rv->state = p2d->effect[rv->pos].state;
   }
   return rv;
}

static float
_aaxDelayEffectGet(float val, int ptype, unsigned char param)
{
   float rv = val;
   if ((param == AAX_DELAY_GAIN) && (ptype == AAX_DECIBEL)) {
      rv = _lin2db(val);
   }
   else if (param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET)
   {
      // value is in seconds internally
      switch(ptype)
      {
      case AAX_MILLISECONDS:
         rv = 1e-3f*val;
         break;
      case AAX_MICROSECONDS:
         rv = 1e-6f*val;
         break;
      case AAX_LINEAR: // delay range 0.0 .. 5.0
         rv = val*DELAY_DEPTH;
         if (param == AAX_LFO_OFFSET) rv += DELAY_MIN;
         break;
      case AAX_SECONDS:
      default:
         break;
      }
   }
   return rv;
}

static float
_aaxDelayEffectSet(float val, int ptype, unsigned char param)
{
   float rv = val;
   if ((param == AAX_DELAY_GAIN) && (ptype == AAX_DECIBEL)) {
      rv = _db2lin(val);
   }
   else if (param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET)
   {
      // value is in seconds internally
      switch(ptype)
      {
      case AAX_MILLISECONDS:
         rv = 1e3f*val;
         break;
      case AAX_MICROSECONDS:
         rv = 1e6f*val;
         break;
      case AAX_LINEAR: //delay-line range 0.0 .. 5.0
         if (param == AAX_LFO_OFFSET) val -= DELAY_MIN;
         rv = val/DELAY_DEPTH;
         break;
      case AAX_SECONDS:
      default:
         break;
      }
   }
   return rv;
}


#define MAXL	(DELAY_MAX/DELAY_MAX_ORG)
static float
_aaxDelayEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxDelayRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { -2.0f,  0.01f,  0.0f, 0.0f  }, {     2.0f,    10.0f,  MAXL,  MAXL } },
    { { 20.0f, 20.0f, -0.98f, 0.01f }, { 22050.0f, 22050.0f, 0.98f, 80.0f } },
    { {  0.0f,  0.0f,   0.0f, 0.0f  }, { 22050.0f,     1.0f,  1.0f,  1.0f } },
    { {  0.0f,  0.0f,   0.0f, 0.0f  }, {     0.0f,     0.0f,  0.0f,  0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxDelayRange[slot].min[param],
                       _aaxDelayRange[slot].max[param]);
}

_eff_function_tbl _aaxDelayLineEffect =
{
   "AAX_delay_effect_"AAX_MKSTR(VERSION), VERSION,
   (_aaxEffectCreateFn*)&_aaxDelayEffectCreate,
   (_aaxEffectDestroyFn*)&_aaxEffectDestroy,
   (_aaxEffectResetFn*)&_delay_reset,
   (_aaxEffectSetStateFn*)&_aaxDelayLineEffectSetState,
   NULL,
   (_aaxNewEffectHandleFn*)&_aaxNewDelayEffectHandle,
   (_aaxEffectConvertFn*)&_aaxDelayEffectSet,
   (_aaxEffectConvertFn*)&_aaxDelayEffectGet,
   (_aaxEffectConvertFn*)&_aaxDelayEffectMinMax
};

/* -- Chorus ---------------------------------------------------------------- */

static aaxEffect
_aaxChorusEffectSetState(_effect_t* effect, int state)
{
   float delay = effect->slot[0]->param[AAX_DELAY_GAIN];
   float feedback = effect->slot[1]->param[AAX_FEEDBACK_GAIN & 0xF];

   return _aaxDelayEffectSetState(effect, state, delay, feedback, CHORUS_MAX);
}

static float
_aaxChorusEffectGet(float val, int ptype, unsigned char param)
{
   float rv = val;
   if ((param == AAX_DELAY_GAIN) && (ptype == AAX_DECIBEL)) {
      rv = _lin2db(val);
   }
   else if (param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET)
   {
      // value is in seconds internally
      switch(ptype)
      {
      case AAX_MILLISECONDS:
         rv = 1e-3f*val;
         break;
      case AAX_MICROSECONDS:
         rv = 1e-6f*val;
         break;
      case AAX_LINEAR: // chorus range 0.0 .. 1.0
         rv = val*CHORUS_DEPTH;
         if (param == AAX_LFO_OFFSET) rv += CHORUS_MIN;
         break;
      case AAX_SECONDS:
      default:
         break;
      }
   }
   return rv;
}

static float
_aaxChorusEffectSet(float val, int ptype, unsigned char param)
{
   float rv = val;
   if ((param == AAX_DELAY_GAIN) && (ptype == AAX_DECIBEL)) {
      rv = _db2lin(val);
   }
   else if (param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET)
   {
      // value is in seconds internally
      switch(ptype)
      {
      case AAX_MILLISECONDS:
         rv = 1e3f*val;
         break;
      case AAX_MICROSECONDS:
         rv = 1e6f*val;
         break;
      case AAX_LINEAR: // chorus range 0.0 .. 1.0
         if (param == AAX_LFO_OFFSET) val -= CHORUS_MIN;
         rv = val/CHORUS_DEPTH;
         break;
      case AAX_SECONDS:
      default:
         break;
      }
   }
   return rv;
}

#define MIN	PHASING_MIN
#define MAX1	DELAY_EFFECTS_TIME
#define MAX2	(MAX1-MIN)
static float
_aaxChorusEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxChorusRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { -2.0f,  0.01f,  0.0f, MIN   }, {     2.0f,    10.0f,  MAX2,  MAX1 } },
    { { 20.0f, 20.0f, -0.98f, 0.01f }, { 22050.0f, 22050.0f, 0.98f, 80.0f } },
    { {  0.0f,  0.0f,   0.0f, 0.0f  }, {     0.0f,     0.0f,  0.0f,  0.0f } },
    { {  0.0f,  0.0f,   0.0f, 0.0f  }, {     0.0f,     0.0f,  0.0f,  0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxChorusRange[slot].min[param],
                       _aaxChorusRange[slot].max[param]);
}

_eff_function_tbl _aaxChorusEffect =
{
   "AAX_chorus_effect_"AAX_MKSTR(VERSION), VERSION,
   (_aaxEffectCreateFn*)&_aaxDelayEffectCreate,
   (_aaxEffectDestroyFn*)&_aaxEffectDestroy,
   (_aaxEffectResetFn*)&_delay_reset,
   (_aaxEffectSetStateFn*)&_aaxChorusEffectSetState,
   NULL,
   (_aaxNewEffectHandleFn*)&_aaxNewDelayEffectHandle,
   (_aaxEffectConvertFn*)&_aaxChorusEffectSet,
   (_aaxEffectConvertFn*)&_aaxChorusEffectGet,
   (_aaxEffectConvertFn*)&_aaxChorusEffectMinMax
};

/* -- Phasing --------------------------------------------------------------- */

static float
_aaxPhasingEffectGet(float val, int ptype, unsigned char param)
{
   float rv = val;
   if ((param == AAX_DELAY_GAIN) && (ptype == AAX_DECIBEL)) {
      rv = _lin2db(val);
   }
   else if (param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET)
   {
      // value is in seconds internally
      switch(ptype)
      {
      case AAX_MILLISECONDS:
         rv = 1e-3f*val;
         break;
      case AAX_MICROSECONDS:
         rv = 1e-6f*val;
         break;
      case AAX_LINEAR: // phasing range 0.0 .. 1.0
         rv = val*PHASING_DEPTH;
         if (param == AAX_LFO_OFFSET) rv += PHASING_MIN;
         break;
      case AAX_SECONDS:
      default:
         break;
      }
   }
   return rv;
}

static float
_aaxPhasingEffectSet(float val, int ptype, unsigned char param)
{
   float rv = val;
   if ((param == AAX_DELAY_GAIN) && (ptype == AAX_DECIBEL)) {
      rv = _db2lin(val);
   }
   else if (param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET)
   {
      // value is in seconds internally
      switch(ptype)
      {
      case AAX_MILLISECONDS:
         rv = 1e3f*val;
         break;
      case AAX_MICROSECONDS:
         rv = 1e6f*val;
         break;
      case AAX_LINEAR: // phasing range 0.0 .. 1.0
         if (param == AAX_LFO_OFFSET) val -= PHASING_MIN;
         rv = val/PHASING_DEPTH;
         break;
      case AAX_SECONDS:
      default:
         break;
      }
   }
   return rv;
}

_eff_function_tbl _aaxPhasingEffect =
{
   "AAX_phasing_effect_"AAX_MKSTR(VERSION), VERSION,
   (_aaxEffectCreateFn*)&_aaxDelayEffectCreate,
   (_aaxEffectDestroyFn*)&_aaxEffectDestroy,
   (_aaxEffectResetFn*)&_delay_reset,
   (_aaxEffectSetStateFn*)&_aaxChorusEffectSetState,
   NULL,
   (_aaxNewEffectHandleFn*)&_aaxNewDelayEffectHandle,
   (_aaxEffectConvertFn*)&_aaxPhasingEffectSet,
   (_aaxEffectConvertFn*)&_aaxPhasingEffectGet,
   (_aaxEffectConvertFn*)&_aaxChorusEffectMinMax
};

/* -- Flanger --------------------------------------------------------------- */

static aaxEffect
_aaxFlangingEffectSetState(_effect_t* effect, int state)
{
   float delay = 0.0f;
   float feedback = effect->slot[0]->param[AAX_DELAY_GAIN];

   /* Convert to Chorus feedback. */
   state &= ~AAX_EFFECT_1ST_ORDER;
   state |= AAX_EFFECT_2ND_ORDER;

   return _aaxDelayEffectSetState(effect, state, delay, feedback, CHORUS_MAX);
}

_eff_function_tbl _aaxFlangingEffect =
{
   "AAX_flanging_effect_"AAX_MKSTR(VERSION), VERSION,
   (_aaxEffectCreateFn*)&_aaxDelayEffectCreate,
   (_aaxEffectDestroyFn*)&_aaxEffectDestroy,
   (_aaxEffectResetFn*)&_delay_reset,
   (_aaxEffectSetStateFn*)&_aaxFlangingEffectSetState,
   NULL,
   (_aaxNewEffectHandleFn*)&_aaxNewDelayEffectHandle,
   (_aaxEffectConvertFn*)&_aaxChorusEffectSet,
   (_aaxEffectConvertFn*)&_aaxChorusEffectGet,
   (_aaxEffectConvertFn*)&_aaxChorusEffectMinMax
};

/* -------------------------------------------------------------------------- */

/*
 * This code is shared between chorus, phasing, flanging and the delay_line
 * effects.
 */
static void*
_delay_create(void *d, void *i, bool feedback, int state, float delay_time)
{
   _aaxRingBufferDelayEffectData *data = d;
   _aaxMixerInfo *info = i;

   if (data == NULL)
   {
      data = _aax_aligned_alloc(DSIZE);
      if (data) memset(data, 0, DSIZE);
   }

   if (data && data->offset == NULL)
   {
      data->offset = calloc(1, sizeof(_aaxRingBufferOffsetData));
      if (!data->offset)
      {
         _aax_aligned_free(data);
         data = NULL;
      }
   }

   if (data)
   {
      int no_tracks = info->no_tracks;
      float fs = info->frequency;

      data->state = state;
      data->no_tracks = no_tracks;
      data->history_samples = TIME_TO_SAMPLES(fs, delay_time);

      if (data->history == NULL) {
         _aaxRingBufferCreateHistoryBuffer(&data->history,
                                           data->history_samples, no_tracks);
      }
      if (data->feedback_history == NULL) {
         _aaxRingBufferCreateHistoryBuffer(&data->feedback_history,
                                           data->history_samples, no_tracks);
      }

      if (!data->history || !data->feedback_history)
      {
         _delay_destroy(data);
         data = NULL;
      }
   }

   return data;
}

static void
_delay_swap(void *d, void *s)
{
   _aaxEffectInfo *dst = d, *src = s;

   if (src->data && src->data_size)
   {
      if (!dst->data)
      {
          dst->data = _aaxAtomicPointerSwap(&src->data, dst->data);
          dst->data_size = src->data_size;
      }
      else
      {
         _aaxRingBufferDelayEffectData *ddef = dst->data;
         _aaxRingBufferDelayEffectData *sdef = src->data;

         assert(dst->data_size == src->data_size);

         ddef->delay = sdef->delay;

         ddef->prepare = sdef->prepare;
         ddef->run = sdef->run;

         ddef->state = sdef->state;

         _lfo_swap(&ddef->lfo, &sdef->lfo);
         _bitcrusher_swap(&ddef->bitcrush, &sdef->bitcrush);
         ddef->offset = _aaxAtomicPointerSwap(&sdef->offset, ddef->offset);

         if (ddef->history_samples == sdef->history_samples) {
            ddef->history = _aaxAtomicPointerSwap(&sdef->history,
                                                  ddef->history);
         }
         else
         {
            int no_tracks = sdef->no_tracks;

            ddef->history_samples = sdef->history_samples;

            if (ddef->history)
            {
               free(ddef->history);
               _aaxRingBufferCreateHistoryBuffer(&ddef->history,
                                           ddef->history_samples, no_tracks);
            }
            if (ddef->feedback_history)
            {
               free(ddef->feedback_history);
               _aaxRingBufferCreateHistoryBuffer(&ddef->feedback_history,
                                           ddef->history_samples, no_tracks);
            }
         }

         if (sdef->freq_filter)
         {
            if (!ddef->freq_filter) {
               ddef->freq_filter = _aaxAtomicPointerSwap(&sdef->freq_filter,
                                                         ddef->freq_filter);
            } else {
               _freqfilter_data_swap(ddef->freq_filter, sdef->freq_filter);
            }
         }
         ddef->no_tracks = sdef->no_tracks;
         ddef->feedback = sdef->feedback;
      }
   }
   dst->destroy = src->destroy;
   dst->swap = src->swap;
}

static void
_delay_destroy(void *ptr)
{
   _aaxRingBufferDelayEffectData *data = ptr;
   if (data)
   {
      data->lfo.envelope = false;
      if (data->offset)
      {
         free(data->offset);
         data->offset = NULL;
      }
      if (data->history)
      {
         free(data->history);
         data->history = NULL;
      }
      if (data->feedback_history)
      {
         free(data->feedback_history);
         data->feedback_history = NULL;
      }
      if (data->freq_filter)
      {
         _freqfilter_destroy(data->freq_filter);
         data->freq_filter = NULL;
      }
      _aax_aligned_free(data);
   }
}

static void
_delay_reset(void *ptr)
{
   _aaxRingBufferDelayEffectData *data = ptr;

   if (data)
   {
      _lfo_reset(&data->lfo);
      _bitcrusher_reset(&data->bitcrush);
      if (data->freq_filter) _freqfilter_reset(data->freq_filter);
   }
}

static size_t
_delay_prepare(MIX_PTR_T dst, MIX_PTR_T src, size_t no_samples, void *data, unsigned int track)
{
   static const size_t bps = sizeof(MIX_T);
   _aaxRingBufferDelayEffectData* delay = data;
   size_t ds = 0;

   assert(delay);
   assert(delay->history);
   assert(delay->history->ptr);
   assert(bps <= sizeof(MIX_T));

   if (track < delay->no_tracks)
   {
      ds = delay->history_samples;

      // copy the delay effects history to src
// DBG_MEMCLR(1, src-ds, ds, bps);
      _aax_memcpy(src-ds, delay->history->history[track], ds*bps);

      // copy the new delay effects history back
      _aax_memcpy(delay->history->history[track], src+no_samples-ds, ds*bps);
   }

   return ds;
}

/**
 * - d and s point to a buffer containing the delay effects buffer prior to
 *   the pointer.
 * - start is the starting pointer
 * - end is the end pointer (end-start is the number of samples)
 * - no_samples is the number of samples to process this run
 * - dmax does not include ds
 */
static int
_delay_run(void *rb, MIX_PTR_T d, MIX_PTR_T s, MIX_PTR_T scratch,
             size_t start, size_t end, size_t no_samples, size_t ds,
             void *data, void *env, unsigned int track)
{
   static const size_t bps = sizeof(MIX_T);
   _aaxRingBufferSample *rbd = (_aaxRingBufferSample*)rb;
   _aaxRingBufferDelayEffectData* effect = data;
   MIX_T *sptr = s + start;
   MIX_T *nsptr = sptr;
   ssize_t offs, noffs;
   float dry_gain, wet_gain;
   float volume, lfo_fact;
   int rv = false;

   _AAX_LOG(LOG_DEBUG, __func__);

   lfo_fact = effect->lfo.get(&effect->lfo, env, s, track, end);

   assert(s != 0);
   assert(d != 0);
   assert(start < end);
   assert(data != NULL);

   offs = effect->delay.sample_offs[track];
   if (ds > effect->history_samples) {
      ds = effect->history_samples;
   }

   assert(start || (offs < (ssize_t)ds));
   if (offs >= (ssize_t)ds) offs = ds-1;

   if (start) {
      noffs = effect->offset->noffs[track];
   }
   else
   {
      noffs = (size_t)lfo_fact;
      effect->delay.sample_offs[track] = noffs;
      effect->offset->noffs[track] = noffs;
   }

   // normalize in case of a delayed signal and/or feedback signal which
   // is higher than the source.
   dry_gain = 1.0f;
   wet_gain = 1.0f;
   if (effect->state & AAX_EFFECT_2ND_ORDER) {
       wet_gain = fabsf(effect->feedback);
   }
   if (effect->state & AAX_EFFECT_1ST_ORDER) {
      wet_gain = _MAX(wet_gain, fabsf(effect->delay.gain));
   }
   if (wet_gain > 1.0f) { // adjust the source volume
      dry_gain = 1.0f/wet_gain;
   } else {
      wet_gain = 1.0f;
   }

   assert(s != d);

   if (effect->state & AAX_EFFECT_2ND_ORDER)
   {
      float gain = effect->feedback/wet_gain;
      volume = fabsf(gain);
      if (offs && volume > LEVEL_32DB)
      {
         ssize_t coffs, doffs;
         int i, step, sign;

         sign = (noffs < offs) ? -1 : 1;
         doffs = labs(noffs - offs);
         i = no_samples;
         coffs = offs;
         step = end;

         if (start)
         {
            step = effect->offset->step[track];
            coffs = effect->offset->coffs[track];
         }
         else
         {
            if (doffs)
            {
               step = end/doffs;
               if (step < 2) step = end;
            }
         }
         effect->offset->step[track] = step;

         _aax_memcpy(nsptr-ds, effect->feedback_history->history[track], ds*bps);
         if (i >= step)
         {
            _aaxRingBufferBitCrusherData *bitcrush = &effect->bitcrush;
            do
            {
               memcpy(scratch, nsptr-coffs, step*sizeof(float));
               if (bitcrush->lfo.get) {
                  bitcrush->run(scratch, 0, step, bitcrush, NULL, track);
               }
               rbd->add(nsptr, scratch, step, gain, 0.0f);

               nsptr += step;
               coffs += sign;
               i -= step;
            }
            while(i >= step);
         }
         if (i) {
            rbd->add(nsptr, nsptr-coffs, i, gain, 0.0f);
         }
         effect->offset->coffs[track] = coffs;

         _aax_memcpy(effect->feedback_history->history[track], sptr+no_samples-ds, ds*bps);

         nsptr = sptr;
      }
   }

   if (effect->state & AAX_EFFECT_1ST_ORDER)
   {
      MIX_T *dptr = d + start;
      float gain =  effect->delay.gain/wet_gain;
      volume = fabsf(gain);
      if (offs && volume > LEVEL_32DB)
      {
         _aaxRingBufferFreqFilterData *flt = effect->freq_filter;
         int i, stages = effect->no_delays;
         ssize_t doffs;

         memset(dptr, 0, no_samples*sizeof(float));
         doffs = noffs - offs; // difference between current and new offset

         // first process the delayed (wet) signal
         if (labs(doffs) > 0)
         {
            float samples = (float)no_samples;
            offs /= stages;
            doffs /= stages;
            for (i=1; i<=stages; ++i)
            {
               float smu = effect->delay.smu[i-1];
               float pitch = _MAX((samples-doffs*i)/samples, 0.001f);
               rbd->resample(scratch, nsptr-offs*i, 0, no_samples, smu, pitch);
               rbd->add(dptr, scratch, no_samples, gain, 0.0f);
               effect->delay.smu[i-1] = fmodf(smu+pitch*no_samples, 1.0f);
            }
         }
         else
         {
            offs /= stages;
            for (i=1; i<=stages; ++i) {
               rbd->add(dptr, nsptr-offs*i, no_samples, gain, 0.0f);
            }
         }

         if (flt)
         {
            float fact = lfo_fact/effect->lfo.max;
            float fc = flt->fc_low + fact*(flt->fc_high-flt->fc_low);
            fc = CLIP_FREQUENCY(fc, flt->fs);

            if (flt->resonance > 0.0f) {
               if (flt->type > BANDPASS) { // HIGHPASS
                  flt->Q = _MAX(flt->resonance*(flt->fc_high - fc), 1.0f);
               } else {
                  flt->Q = flt->resonance*fc;
               }
            }

            if (flt->state == AAX_BESSEL) {
               _aax_bessel_compute(fc, flt);
            } else {
               _aax_butterworth_compute(fc, flt);
            }
            flt->run(rbd, dptr, dptr, 0, no_samples, 0, track, flt, env, 1.0f);
         }
      }

      // then add the original (dry) signal
      rbd->add(dptr, sptr, no_samples, dry_gain, 0.0f);
      rv = true;
   }

   return rv;
}

