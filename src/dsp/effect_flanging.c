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
#include <base/random.h>

#include <software/rbuf_int.h>
#include "effects.h"
#include "arch.h"
#include "dsp.h"
#include "api.h"

#define VERSION	1.03
#define DSIZE	sizeof(_aaxRingBufferDelayEffectData)

static aaxEffect
_aaxFlangingEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
{
   _effect_t* eff = _aaxEffectCreateHandle(info, type, 2, DSIZE);
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

   assert(effect->info);

   state &= ~AAX_EFFECT_1ST_ORDER;
   state |= AAX_EFFECT_2ND_ORDER;

   effect->state = state;
   switch (state & AAX_WAVEFORM_MASK)
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
   case AAX_TIMED_TRANSITION:
   {
      _aaxRingBufferDelayEffectData* data = effect->slot[0]->data;

      data = _delay_create(data, effect->info, AAX_FALSE, AAX_TRUE, state, DELAY_EFFECTS_TIME);
      effect->slot[0]->data = data;
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

         if ((fc > MINIMUM_CUTOFF && fc < MAXIMUM_CUTOFF) ||
             ( fmax > MINIMUM_CUTOFF && fmax < MAXIMUM_CUTOFF))
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
         data->flanger = AAX_TRUE;
         data->feedback = effect->slot[0]->param[AAX_DELAY_GAIN];

         _lfo_setup(&data->lfo, effect->info, effect->state);

         data->lfo.min_sec = FLANGING_MIN;
         data->lfo.max_sec = FLANGING_MAX;
         data->lfo.depth = depth;
         data->lfo.offset = offset;

         data->lfo.f = effect->slot[0]->param[AAX_LFO_FREQUENCY];
         data->lfo.inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;

         if ((data->lfo.offset + data->lfo.depth) > 1.0f) {
            data->lfo.depth = 1.0f - data->lfo.offset;
         }

         constant = _lfo_set_timing(&data->lfo);

         data->delay.gain = 0.0f;
         for (t=0; t<_AAX_MAX_SPEAKERS; t++) {
            data->delay.sample_offs[t] = (size_t)data->lfo.value[t];
         }

         if (!_lfo_set_function(&data->lfo, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
         else if (flt) // add a frequecny filter
         {
            int stages;

            flt->fs = fs;
            flt->run = _freqfilter_run;

            flt->high_gain = data->delay.gain;
            flt->low_gain = 0.0f;

            if (state & AAX_48DB_OCT) stages = 4;
            else if (state & AAX_36DB_OCT) stages = 3;
            else if (state & AAX_24DB_OCT) stages = 2;
            else if (state & AAX_6DB_OCT) stages = 0;
            else stages = 1;

            flt->no_stages = stages;
            flt->state = (state & AAX_BESSEL) ? AAX_BESSEL : AAX_BUTTERWORTH;
            flt->Q = effect->slot[1]->param[AAX_DELAY_RESONANCE & 0xF];
            flt->type = (flt->high_gain >= flt->low_gain) ? LOWPASS : HIGHPASS;

            if (state & AAX_RANDOM_SELECT)
            {
               float lfc2 = _lin2log(fmax);
               float lfc1 = _lin2log(fc);

               flt->fc_low = fc;
               flt->fc_high = fmax;
               flt->random = 1;

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
                  lfo->envelope = AAX_FALSE;

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
_aaxNewFlangingEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _effect_t* rv = _aaxEffectCreateHandle(info, type, 2, 0);

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
_aaxFlangingEffectSet(float val, int ptype, unsigned char param)
{
   float rv = val;
   if ((param == AAX_DELAY_GAIN) && (ptype == AAX_DECIBEL)) {
      rv = _lin2db(val);
   }
   else if (param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET)
   {
      if (ptype == AAX_SECONDS) {
         rv = (FLANGING_MIN + val*FLANGING_MAX);
      } else if (ptype == AAX_MILLISECONDS) {
         rv = (FLANGING_MIN + val*FLANGING_MAX)*1e3f;
      } else if (ptype == AAX_MICROSECONDS) {
         rv = (FLANGING_MIN + val*FLANGING_MAX)*1e6f;
      }
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
   else if (param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET)
   {
      if (ptype == AAX_SECONDS) {
         rv = _MINMAX((val - FLANGING_MIN)/FLANGING_MAX, 0.0f, 1.0f);
      } else if (ptype == AAX_MILLISECONDS) {
         rv = _MINMAX((val*1e-3f - FLANGING_MIN)/FLANGING_MAX, 0.0f, 1.0f);
      } else if (ptype == AAX_MICROSECONDS) {
         rv = _MINMAX((val*1e-6f - FLANGING_MIN)/FLANGING_MAX, 0.0f, 1.0f);
      }
   }
   return rv;
}

static float
_aaxFlangingEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxFlangingRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { {-0.98f,  0.01f,  0.0f, 0.0f  }, {    0.98f,    10.0f,  1.0f,  1.0f } },
    { { 20.0f, 20.0f, -0.98f, 0.01f }, { 22050.0f, 22050.0f, 0.98f, 80.0f } },
    { {  0.0f,  0.0f,   0.0f, 0.0f  }, {     0.0f,     0.0f,  0.0f,  0.0f } },
    { {  0.0f,  0.0f,   0.0f, 0.0f  }, {     0.0f,     0.0f,  0.0f,  0.0f } }
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
   "AAX_flanging_effect_"AAX_MKSTR(VERSION), VERSION,
   (_aaxEffectCreate*)&_aaxFlangingEffectCreate,
   (_aaxEffectDestroy*)&_aaxFlangingEffectDestroy,
   (_aaxEffectReset*)&_delay_reset,
   (_aaxEffectSetState*)&_aaxFlangingEffectSetState,
   NULL,
   (_aaxNewEffectHandle*)&_aaxNewFlangingEffectHandle,
   (_aaxEffectConvert*)&_aaxFlangingEffectSet,
   (_aaxEffectConvert*)&_aaxFlangingEffectGet,
   (_aaxEffectConvert*)&_aaxFlangingEffectMinMax
};

