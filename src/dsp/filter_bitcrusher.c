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

#include <base/random.h>
#include "common.h"
#include "filters.h"
#include "arch.h"
#include "api.h"

#define VERSION 1.1
#define DSIZE	sizeof(_aaxRingBufferBitCrusherData)

static int _bitcrusher_run(MIX_PTR_T, size_t, size_t, void*, void*, unsigned int);
static int _bitcrusher_add_noise(MIX_PTR_T, size_t, size_t, void*, void*, unsigned int);
static void _bitcrusher_swap(void*, void*);

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

static int
_aaxBitCrusherFilterDestroy(_filter_t* filter)
{
   if (filter->slot[0]->data)
   {
      filter->slot[0]->destroy(filter->slot[0]->data);
      filter->slot[0]->data = NULL;
   }
   free(filter);

   return AAX_TRUE;
}

static int
_bitcrusher_reset(void *data)
{
   _aaxRingBufferBitCrusherData *bitcrush = data;
   if (bitcrush)
   {
      _lfo_reset(&bitcrush->lfo);
      _lfo_reset(&bitcrush->env);
   }

   return AAX_TRUE;
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
   aaxFilter rv = AAX_FALSE;
   int stereo;

   assert(filter->info);

   stereo = (state & AAX_LFO_STEREO) ? AAX_TRUE : AAX_FALSE;
   state &= ~AAX_LFO_STEREO;

   filter->state = state;
   switch (state & ~AAX_INVERSE)
   {
   case AAX_CONSTANT_VALUE:
   case AAX_TRIANGLE_WAVE:
   case AAX_SINE_WAVE:
   case AAX_SQUARE_WAVE:
   case AAX_IMPULSE_WAVE:
   case AAX_SAWTOOTH_WAVE:
   case AAX_CYCLOID_WAVE:
   case AAX_RANDOMNESS:
   case AAX_TIMED_TRANSITION:
   case (AAX_TIMED_TRANSITION|AAX_ENVELOPE_FOLLOW_LOG):
   case AAX_ENVELOPE_FOLLOW:
   case AAX_ENVELOPE_FOLLOW_LOG:
   case AAX_ENVELOPE_FOLLOW_MASK:
   {
      _aaxRingBufferBitCrusherData *bitcrush = filter->slot[0]->data;
      if (bitcrush == NULL)
      {
         bitcrush = _aax_aligned_alloc(DSIZE);
         filter->slot[0]->data = bitcrush;
         if (bitcrush) memset(bitcrush, 0, DSIZE);
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

         /* sample rate conversion */
         bitcrush->fs = filter->slot[1]->param[AAX_CUTOFF_FREQUENCY_HF & 0xF];

         /* bit reduction */
         if ((state & (AAX_ENVELOPE_FOLLOW | AAX_TIMED_TRANSITION)) &&
             (state & AAX_ENVELOPE_FOLLOW_LOG))
         {
            bitcrush->lfo.convert = _squared;
         } else {
            bitcrush->lfo.convert = _linear;
         }
         bitcrush->lfo.state = filter->state;
         bitcrush->lfo.fs = fs;
         bitcrush->lfo.period_rate = filter->info->period_rate;
         bitcrush->lfo.stereo_lnk = !stereo;

         bitcrush->lfo.min_sec = offset/fs;
         bitcrush->lfo.max_sec = bitcrush->lfo.min_sec + depth/fs;
         bitcrush->lfo.depth = 1.0f;
         bitcrush->lfo.offset = 0.0f;
         bitcrush->lfo.f = filter->slot[0]->param[AAX_LFO_FREQUENCY];
         bitcrush->lfo.inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;

         if ((bitcrush->lfo.offset + bitcrush->lfo.depth) > 1.0f) {
            bitcrush->lfo.depth = 1.0f - bitcrush->lfo.offset;
         }

         constant = _lfo_set_timing(&bitcrush->lfo);

         bitcrush->lfo.envelope = AAX_FALSE;
         if (!_lfo_set_function(&bitcrush->lfo, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }

         /* noise */
         bitcrush->staticity = filter->slot[1]->param[AAX_STATICITY & 0xF];

         depth = filter->slot[0]->param[AAX_NOISE_LEVEL];
         if ((state & (AAX_ENVELOPE_FOLLOW | AAX_TIMED_TRANSITION)) &&
             (state & AAX_ENVELOPE_FOLLOW_LOG))
         {
            bitcrush->env.convert = _squared;
         } else {
            bitcrush->env.convert = _linear;
         }
         bitcrush->env.state = filter->state;
         bitcrush->env.fs = fs;
         bitcrush->env.period_rate = filter->info->period_rate;
         bitcrush->env.stereo_lnk = !stereo;

         bitcrush->env.min_sec = 0.0f;
         bitcrush->env.max_sec = depth/fs;
         bitcrush->env.depth = 1.0f;
         bitcrush->env.offset = 0.0f;
         bitcrush->env.f = filter->slot[0]->param[AAX_LFO_FREQUENCY];
         bitcrush->env.inv = (state & AAX_INVERSE) ? AAX_FALSE : AAX_TRUE;

         if ((bitcrush->env.offset + bitcrush->env.depth) > 1.0f) {
            bitcrush->env.depth = 1.0f - bitcrush->env.offset;
         }

         constant = _lfo_set_timing(&bitcrush->env);

         bitcrush->env.envelope = AAX_FALSE;
         if (!_lfo_set_function(&bitcrush->env, constant)) {
            _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
      }
      else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      break;
   }
   default:
      _aaxErrorSet(AAX_INVALID_PARAMETER);
      // inetnional fall-through
   case AAX_FALSE:
      if (filter->slot[0]->data)
      {
         filter->slot[0]->destroy(filter->slot[0]->data);
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
      rv->slot[1]->param[AAX_CUTOFF_FREQUENCY_HF & 0xF] = bitcrush->fs;
      rv->slot[1]->param[AAX_STATICITY & 0xF] = bitcrush->staticity;
      rv->state = p2d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxBitCrusherFilterSet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxBitCrusherFilterGet(float val, UNUSED(int ptype), UNUSED(unsigned char param))
{
   float rv = val;
   return rv;
}

static float
_aaxBitCrusherFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxBitCrusherRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.01f, 0.0f, 0.0f }, {     2.0f, 50.0f, 2.0f, 1.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, { 44100.0f,  1.0f, 0.0f, 0.0f } }, 
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
   AAX_TRUE,
   "AAX_bitcrusher_filter_"AAX_MKSTR(VERSION), VERSION,
   (_aaxFilterCreate*)&_aaxBitCrusherFilterCreate,
   (_aaxFilterDestroy*)&_aaxBitCrusherFilterDestroy,
   (_aaxFilterReset*)&_bitcrusher_reset,
   (_aaxFilterSetState*)&_aaxBitCrusherFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewBitCrusherFilterHandle,
   (_aaxFilterConvert*)&_aaxBitCrusherFilterSet,
   (_aaxFilterConvert*)&_aaxBitCrusherFilterGet,
   (_aaxFilterConvert*)&_aaxBitCrusherFilterMinMax
};

int
_bitcrusher_run(MIX_PTR_T s, size_t end, size_t no_samples,
                    void *data, void *env, unsigned int track)
{
   _aaxRingBufferBitCrusherData *bitcrush = data;
   int rv = AAX_FALSE;
   float level;

   level = bitcrush->lfo.get(&bitcrush->lfo, env, s, track, end);
   if (bitcrush->fs)
   {
      float smu, freq_fact;
      int i = no_samples;
      MIX_T val;

      if (bitcrush->lfo.get != _aaxLFOGetFixedValue) {
         freq_fact = 1.0f - level*(bitcrush->lfo.fs - bitcrush->fs)/bitcrush->lfo.fs;
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

   if (level > 0.01f)
   {
      unsigned bps = sizeof(MIX_T);

      level = powf(2.0f, 8+sqrtf(0.95f*level)*13.5f); // (24-bits/sample)
      _batch_fmul_value(s, s, bps, no_samples, 1.0f/level);
      _batch_roundps(s, s, no_samples);
      _batch_fmul_value(s, s, bps, no_samples, level);
      rv = AAX_TRUE;
   }
   return rv;
}

int
_bitcrusher_add_noise(MIX_PTR_T s, size_t end, size_t no_samples,
                    void *data, void *env, unsigned int track)
{
   _aaxRingBufferBitCrusherData *bitcrush = data;
   int rv = AAX_FALSE;
   float ratio;

   ratio = bitcrush->env.get(&bitcrush->env, env, s, track, end);
   if (ratio > 0.01f)
   {
      float rnd_skip, staticity;
      int i = no_samples;
      unsigned skip;

      staticity = _MINMAX(bitcrush->staticity, 0.0f, 1.0f);
      skip = (unsigned)(1.0f + 99.0f*staticity);
      rnd_skip = skip ? skip : 1.0f;
      ratio *= (1.0f + 10.0f*staticity*staticity);

      ratio *= ((double)(0.25f * AAX_PEAK_MAX)/(double)UINT64_MAX);
      do
      {
         *s += ratio*xoroshiro128plus();

         s += (int)rnd_skip;
         i -= (int)rnd_skip;

         if (skip > 1) {
            rnd_skip = 1.0f + fabsf((2*skip-rnd_skip)*_aax_rand_sample());
         }
      }
      while (i > 0);
      rv = AAX_TRUE;
   }
   return rv;
}
