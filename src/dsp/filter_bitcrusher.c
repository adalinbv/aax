/*
 * SPDX-FileCopyrightText: Copyright © 2007-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
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
#include "filters.h"
#include "dsp.h"
#include "arch.h"
#include "api.h"

#define VERSION 1.1
#define DSIZE	sizeof(_aaxRingBufferBitCrusherData)

static aaxFilter
_aaxBitCrusherFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, 2, DSIZE);
   aaxFilter rv = NULL;

   if (flt)
   {
      _aaxSetDefaultFilter2d(flt->slot[0], flt->pos, 0);
      flt->slot[0]->swap = _bitcrusher_swap;
      rv = (aaxFilter)flt;
   }
   return rv;
}

int
_bitcrusher_reset(void *data)
{
   _aaxRingBufferBitCrusherData *bitcrush = data;
   if (bitcrush)
   {
      _lfo_reset(&bitcrush->lfo);
      _lfo_reset(&bitcrush->env);
   }

   return true;
}

void
_bitcrusher_swap(void *d, void *s)
{
   _aaxFilterInfo *dst = d, *src = s;

   if (src->data && src->data_size)
   {
      if (!dst->data) {
          dst->data = _aaxAtomicPointerSwap(&src->data, dst->data);
          dst->data_size = src->data_size;
      }
      else
      {
         _aaxRingBufferBitCrusherData *dflt = dst->data;
         _aaxRingBufferBitCrusherData *sflt = src->data;

         assert(dst->data_size == src->data_size);

         _lfo_swap(&dflt->lfo, &sflt->lfo);
         _lfo_swap(&dflt->env, &sflt->env);

         dflt->fs = sflt->fs;
         dflt->staticity = sflt->staticity;
      }
   }
   dst->destroy = src->destroy;
   dst->swap = src->swap;
}

static aaxFilter
_aaxBitCrusherFilterSetState(_filter_t* filter, int state)
{
   void *handle = filter->handle;
   aaxFilter rv = false;
   int stereo;

   assert(filter->info);

   stereo = (state & AAX_LFO_STEREO) ? true : false;
   state &= ~AAX_LFO_STEREO;

   if ((state & AAX_SOURCE_MASK) == 0) {
      state |= true;
   }

   filter->state = state;
   switch (state & (AAX_SOURCE_MASK & ~AAX_PURE_WAVEFORM))
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
      _aaxRingBufferBitCrusherData *bitcrush = filter->slot[0]->data;
      if (bitcrush == NULL)
      {
         bitcrush = _aax_aligned_alloc(DSIZE);
         filter->slot[0]->data = bitcrush;
         if (bitcrush)
         {
            filter->slot[0]->data_size = DSIZE;
            memset(bitcrush, 0, DSIZE);
         }
      }

      if (bitcrush)
      {
         float offset = filter->slot[0]->param[AAX_LFO_OFFSET];
         float depth = filter->slot[0]->param[AAX_LFO_DEPTH];
         float fs = 48000.0f;
         int constant;

         bitcrush->run = _bitcrusher_run;
         bitcrush->add_noise = _bitcrusher_add_noise;

         if (filter->info) {
            fs = filter->info->frequency;
         }

         bitcrush->alpha[0] = 0.0f;
         if (state & AAX_BROWNIAN_NOISE) {
            bitcrush->alpha[0] = _aax_movingaverage_compute(100.0f, fs);
            bitcrush->fact[0] = 0.33f/sqrtf(bitcrush->alpha[0]);
         }
         else if (state & AAX_PINK_NOISE)
         {
            bitcrush->alpha[0] = _aax_movingaverage_compute(20.0f, fs);
            bitcrush->alpha[1] = _aax_movingaverage_compute(200.0f, fs);
            bitcrush->alpha[2] = _aax_movingaverage_compute(2000.0f, fs);
            bitcrush->fact[0] = 0.11f/sqrtf(bitcrush->alpha[0]);
            bitcrush->fact[1] = 0.11f/sqrtf(bitcrush->alpha[1]);
            bitcrush->fact[2] = 0.11f/sqrtf(bitcrush->alpha[2]);
         }

         /* sample rate conversion */
         bitcrush->fs = filter->slot[1]->param[AAX_SAMPLE_RATE & 0xF];

         /* bit reduction */
         bitcrush->level = depth;
         if (((state & AAX_SOURCE_MASK) == AAX_ENVELOPE_FOLLOW ||
              (state & AAX_SOURCE_MASK) == AAX_TIMED_TRANSITION) &&
             (state & AAX_LFO_EXPONENTIAL))
         {
            bitcrush->lfo.convert = _squared;
         } else {
            bitcrush->lfo.convert = _linear;
         }
         bitcrush->lfo.state = filter->state;
         bitcrush->lfo.fs = fs;
         bitcrush->lfo.period_rate = filter->info->period_rate;
         bitcrush->lfo.stereo_link = !stereo;

         bitcrush->lfo.min_sec = 0.0f;
         bitcrush->lfo.max_sec = 1.0/fs;
         bitcrush->lfo.depth = 1.0f;
         bitcrush->lfo.offset = 0.0f;
         bitcrush->lfo.f = filter->slot[0]->param[AAX_LFO_FREQUENCY];
         bitcrush->lfo.inverse = (state & AAX_INVERSE) ? true : false;

         if ((bitcrush->lfo.offset + bitcrush->lfo.depth) > 1.0f) {
            bitcrush->lfo.depth = 1.0f - bitcrush->lfo.offset;
         }

         constant = _lfo_set_timing(&bitcrush->lfo);

         bitcrush->lfo.envelope = false;
         if (!_lfo_set_function(&bitcrush->lfo, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }

         /* noise */
         bitcrush->staticity = filter->slot[1]->param[AAX_STATICITY & 0xF];

         depth = filter->slot[0]->param[AAX_NOISE_LEVEL];
         if (state & AAX_LFO_EXPONENTIAL)
         {
            bitcrush->env.convert = _squared;
         } else {
            bitcrush->env.convert = _linear;
         }
         bitcrush->env.state = filter->state;
         bitcrush->env.fs = fs;
         bitcrush->env.period_rate = filter->info->period_rate;
         bitcrush->env.stereo_link = !stereo;

         bitcrush->env.min_sec = 0.0f;
         bitcrush->env.max_sec = depth/fs;
         bitcrush->env.depth = 1.0f;
         bitcrush->env.offset = 0.0f;
         bitcrush->env.f = filter->slot[0]->param[AAX_LFO_FREQUENCY];
         bitcrush->env.inverse = (state & AAX_INVERSE) ? false : true;

         if ((bitcrush->env.offset + bitcrush->env.depth) > 1.0f) {
            bitcrush->env.depth = 1.0f - bitcrush->env.offset;
         }

         constant = _lfo_set_timing(&bitcrush->env);

         bitcrush->env.envelope = false;
         if (!_lfo_set_function(&bitcrush->env, constant)) {
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
      if (filter->slot[0]->data)
      {
         filter->slot[0]->destroy(filter->slot[0]->data);
         filter->slot[0]->data_size = 0;
         filter->slot[0]->data = NULL;
      }
      break;
   }
   rv = filter;
   return rv;
}

static _filter_t*
_aaxNewBitCrusherFilterHandle(const aaxConfig config, enum aaxFilterType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _filter_t* rv = _aaxFilterCreateHandle(info, type, 2, 0);

   if (rv)
   {
      _aaxRingBufferBitCrusherData *bitcrush;

      _aax_dsp_copy(rv->slot[0], &p2d->filter[rv->pos]);
      rv->slot[0]->swap = _bitcrusher_swap;

      bitcrush = (_aaxRingBufferBitCrusherData*)p2d->filter[rv->pos].data;
      if (bitcrush)
      {
         rv->slot[1]->param[AAX_SAMPLE_RATE & 0xF] = bitcrush->fs;
         rv->slot[1]->param[AAX_STATICITY & 0xF] = bitcrush->staticity;
      }
      rv->state = p2d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxBitCrusherFilterSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   if (param == AAX_NOISE_LEVEL && ptype == AAX_DECIBEL) {
      rv = _lin2db(val);
   } else if ((param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET)
              && ptype == AAX_BITS_PER_SAMPLE) {
      rv = (1.0f - val)*MIX_BPS;
   }
   return rv;
}

static float
_aaxBitCrusherFilterGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   if (param == AAX_NOISE_LEVEL && ptype == AAX_DECIBEL) {
      rv = _db2lin(val);
   } else if ((param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET)
              && ptype == AAX_BITS_PER_SAMPLE) {
      rv = 1.0f - val/MIX_BPS;
   }
   return rv;
}

static float
_aaxBitCrusherFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxBitCrusherRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.01f, 0.0f, 0.0f }, {     2.0f, 50.0f, 2.0f, 1.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, { 22050.0f,  1.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,  0.0f, 0.0f, 0.0f } }
   };

   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);

   return _MINMAX(val, _aaxBitCrusherRange[slot].min[param],
                       _aaxBitCrusherRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxBitCrusherFilter =
{
   "AAX_bitcrusher_filter_"AAX_MKSTR(VERSION), VERSION,
   (_aaxFilterCreateFn*)&_aaxBitCrusherFilterCreate,
   (_aaxFilterDestroyFn*)&_aaxFilterDestroy,
   (_aaxFilterResetFn*)&_bitcrusher_reset,
   (_aaxFilterSetStateFn*)&_aaxBitCrusherFilterSetState,
   (_aaxNewFilterHandleFn*)&_aaxNewBitCrusherFilterHandle,
   (_aaxFilterConvertFn*)&_aaxBitCrusherFilterSet,
   (_aaxFilterConvertFn*)&_aaxBitCrusherFilterGet,
   (_aaxFilterConvertFn*)&_aaxBitCrusherFilterMinMax
};

int
_bitcrusher_run(MIX_PTR_T s, size_t end, size_t no_samples,
                    void *data, void *env, unsigned int track)
{
   _aaxRingBufferBitCrusherData *bitcrush = data;
   int rv = false;
   float lfo;

   /* sample rate decimation */
   lfo = _ln(bitcrush->lfo.get(&bitcrush->lfo, env, s, track, end));
   if (bitcrush->fs)
   {
      float smu, freq_fact;
      int i = no_samples;
      MIX_T val;

      if (bitcrush->lfo.get != _aaxLFOGetFixedValue)
      {
         float fact = (bitcrush->lfo.fs - bitcrush->fs)/bitcrush->lfo.fs;
         freq_fact = 1.0f - lfo*fact;
      } else {
         freq_fact = bitcrush->fs/bitcrush->lfo.fs;
      }

      smu = 0;
      val = *s;
      do
      {
          *s++ = val;

          smu += freq_fact;
          if (smu > 1.0f)
          {
             val = *s;
             smu -= 1.0f;
          }
      }
      while (--i);
   }

   /* bitcrushing */
   if (bitcrush->level > 0.01f)
   {
      float level = lfo*bitcrush->level;
      level = powf(2.0f, 8+sqrtf(0.95f*level)*13.5f); // (24-bits/sample)
      _batch_fmul_value(s, s, no_samples, 1.0f, level);
      _batch_roundps(s, s, no_samples);
      _batch_fmul_value(s, s, no_samples, level, 1.0f);
      rv = true;
   }
   return rv;
}

int
_bitcrusher_add_noise(MIX_PTR_T s, size_t end, size_t no_samples,
                    void *data, void *env, unsigned int track)
{
   _aaxRingBufferBitCrusherData *bitcrush = data;
   int rv = false;
   float ratio;

   ratio = bitcrush->env.get(&bitcrush->env, env, s, track, end);
   if (ratio > 0.01f) // noise is active
   {
      float *prev, *alpha, *fact;
      float rnd_skip, staticity;
      int i = no_samples;
      unsigned skip;

      staticity = bitcrush->staticity;
      ratio *= (1.0f + 10.0f*staticity*staticity);
      ratio *= ((double)(0.25f * AAX_PEAK_MAX)/(double)UINT64_MAX);

      skip = (unsigned)(1.0f + 99.0f*staticity);
      rnd_skip = skip ? skip : 1.0f;

      prev = bitcrush->prev;
      fact = bitcrush->fact;
      alpha = bitcrush->alpha;
      if (alpha[0] == 0.0f)
      {
         do
         {
            float rnd = ratio*xoroshiro128plus(); // get the next sample
            *s += rnd;

            s += (int)rnd_skip;
            i -= (int)rnd_skip;

            if (skip > 1) {
               rnd_skip = 1.0f + fabsf((2*skip-rnd_skip)*_aax_rand_sample());
            }
         }
         while (i > 0);
      }
      else if ( alpha[1] == 0.0f)
      {
         do
         {
            float rnd = ratio*xoroshiro128plus(); // get the next sample

            // exponential moving average filter (-6dB/oct)
            prev[0] = (1.0f-alpha[0])*prev[0] + fact[0]*alpha[0]*rnd;
            *s += prev[0];

            s += (int)rnd_skip;
            i -= (int)rnd_skip;

            if (skip > 1) {
               rnd_skip = 1.0f + fabsf((2*skip-rnd_skip)*_aax_rand_sample());
            }
         }
         while (i > 0);
      }
      else
      {
         do
         {
            float rnd = ratio*xoroshiro128plus(); // get the next sample

            // exponential moving average filter (-3dB/oct)
            prev[0] = (1.0f-alpha[0])*prev[0] + fact[0]*alpha[0]*rnd;
            *s += prev[0];

            prev[1] = (1.0f-alpha[1])*prev[1] + fact[1]*alpha[1]*rnd;
            *s += prev[1];

            prev[2] = (1.0f-alpha[2])*prev[2] + fact[2]*alpha[2]*rnd;
            *s += prev[2];

            s += (int)rnd_skip;
            i -= (int)rnd_skip;

            if (skip > 1) {
               rnd_skip = 1.0f + fabsf((2*skip-rnd_skip)*_aax_rand_sample());
            }
         }
         while (i > 0);
      }

      rv = true;
   }
   return rv;
}
