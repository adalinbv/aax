/*
 * Copyright 2007-2021 by Erik Hofman.
 * Copyright 2009-2021 by Adalin B.V.
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

#define VERSION		1.13
#define DSIZE		sizeof(_aaxRingBufferDelayEffectData)

static aaxEffect
_aaxChorusEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
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
_aaxChorusEffectDestroy(_effect_t* effect)
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
_aaxChorusEffectSetState(_effect_t* effect, int state)
{
   void *handle = effect->handle;
   aaxEffect rv = AAX_FALSE;
   int mask, istate, wstate;
   int stereo;

   assert(effect->info);

   mask = AAX_TRIANGLE_WAVE|AAX_SINE_WAVE|AAX_SQUARE_WAVE|AAX_IMPULSE_WAVE|
          AAX_SAWTOOTH_WAVE|AAX_RANDOMNESS | AAX_TIMED_TRANSITION |
          AAX_ENVELOPE_FOLLOW_MASK | AAX_CONSTANT_VALUE;

   stereo = (state & AAX_LFO_STEREO) ? AAX_TRUE : AAX_FALSE;
   state &= ~AAX_LFO_STEREO;

   istate = state & ~(AAX_INVERSE|AAX_BUTTERWORTH|AAX_BESSEL|AAX_RANDOM_SELECT|AAX_ENVELOPE_FOLLOW_LOG);
   if (istate == 0) istate = AAX_12DB_OCT;
   wstate = istate & mask;

   effect->state = state;
   switch (state & ~AAX_INVERSE)
   {
   case AAX_CONSTANT_VALUE:
   case AAX_TRIANGLE_WAVE:
   case AAX_SINE_WAVE:
   case AAX_SQUARE_WAVE:
   case AAX_IMPULSE_WAVE:
   case AAX_SAWTOOTH_WAVE:
   case AAX_RANDOMNESS:
   case AAX_TIMED_TRANSITION:
   case (AAX_TIMED_TRANSITION|AAX_ENVELOPE_FOLLOW_LOG):
   case AAX_ENVELOPE_FOLLOW:
   case AAX_ENVELOPE_FOLLOW_LOG:
   case AAX_ENVELOPE_FOLLOW_MASK:
   {
      _aaxRingBufferDelayEffectData* data = effect->slot[0]->data;
      float feedback = effect->slot[1]->param[AAX_FEEDBACK_GAIN & 0xF];
      char fbhist = feedback ? AAX_TRUE : AAX_FALSE;

      data = _delay_create(data, effect->info, AAX_TRUE, fbhist, DELAY_EFFECTS_TIME);
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
         data->flanger = AAX_FALSE;
         data->feedback = feedback;

         if ((state & (AAX_ENVELOPE_FOLLOW | AAX_TIMED_TRANSITION)) &&
             (state & AAX_ENVELOPE_FOLLOW_LOG))
         {
            data->lfo.convert = _exponential;
         }
         else
         {
            data->lfo.convert = _linear;
         }

         data->lfo.state = effect->state;
         data->lfo.fs = fs;
         data->lfo.period_rate = effect->info->period_rate;

         if ((offset+depth)/CHORUS_MAX > CHORUS_MIN)
         {
            data->lfo.min_sec = CHORUS_MIN;
            data->lfo.max_sec = CHORUS_MAX;
            data->lfo.depth = depth;
            data->lfo.offset = offset;
         }
         else // switch to phasing
         {
            data->lfo.min_sec = PHASING_MIN;
            data->lfo.max_sec = PHASING_MAX;
            data->lfo.depth = depth*CHORUS_MAX/PHASING_MAX;
            data->lfo.offset = offset*CHORUS_MAX/PHASING_MAX;
         }
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

               /* sweeprate */
               lfo->state = wstate;
               lfo->fs = fs;
               lfo->period_rate = data->lfo.period_rate;

               lfo->min = fc;
               lfo->max = fmax;

               if (state & AAX_ENVELOPE_FOLLOW_LOG)
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
                  lfo->convert = _linear;
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

               lfo->depth = 1.0f;
               lfo->offset = 0.0f;
               lfo->min_sec = lfo->min/lfo->fs;
               lfo->max_sec = lfo->max/lfo->fs;

               lfo->f = data->lfo.f;
               lfo->inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;
               lfo->stereo_lnk = !stereo;

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
_aaxNewChorusEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
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
_aaxChorusEffectSet(float val, int ptype, unsigned char param)
{
   float rv = val;
   if ((param == AAX_DELAY_GAIN) && (ptype == AAX_DECIBEL)) {
      rv = _lin2db(val);
   }
  else if ((param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET) &&
            (ptype == AAX_MICROSECONDS)) {
       rv = (CHORUS_MIN + val*CHORUS_MAX)*1e6f;
   }
   return rv;
}

static float
_aaxChorusEffectGet(float val, int ptype, unsigned char param)
{
   float rv = val;
   if ((param == AAX_DELAY_GAIN) && (ptype == AAX_DECIBEL)) {
      rv = _db2lin(val);
   }
   else if ((param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET) &&
            (ptype == AAX_MICROSECONDS)) {
      rv = _MINMAX((val*1e-6f - CHORUS_MIN)/CHORUS_MAX, 0.0f, 1.0f);
   }
   return rv;
}

static float
_aaxChorusEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxChorusRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.001f,  0.01f, 0.0f, 0.0f  }, {     1.0f,    10.0f, 1.0f,  1.0f } },
    { {  20.0f, 20.0f,  0.0f, 0.01f }, { 22050.0f, 22050.0f, 1.0f, 80.0f } },
    { {   0.0f,  0.0f,  0.0f, 0.0f  }, {     0.0f,     0.0f, 0.0f,  0.0f } },
    { {   0.0f,  0.0f,  0.0f, 0.0f  }, {     0.0f,     0.0f, 0.0f,  0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxChorusRange[slot].min[param],
                       _aaxChorusRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxChorusEffect =
{
   AAX_FALSE,
   "AAX_chorus_effect_"AAX_MKSTR(VERSION), VERSION,
   (_aaxEffectCreate*)&_aaxChorusEffectCreate,
   (_aaxEffectDestroy*)&_aaxChorusEffectDestroy,
   (_aaxEffectReset*)&_delay_reset,
   (_aaxEffectSetState*)&_aaxChorusEffectSetState,
   NULL,
   (_aaxNewEffectHandle*)&_aaxNewChorusEffectHandle,
   (_aaxEffectConvert*)&_aaxChorusEffectSet,
   (_aaxEffectConvert*)&_aaxChorusEffectGet,
   (_aaxEffectConvert*)&_aaxChorusEffectMinMax
};

