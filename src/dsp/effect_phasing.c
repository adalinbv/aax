/*
 * Copyright 2007-2018 by Erik Hofman.
 * Copyright 2009-2018 by Adalin B.V.
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
_aaxPhasingEffectCreate(_aaxMixerInfo *info, enum aaxEffectType type)
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
_aaxPhasingEffectDestroy(_effect_t* effect)
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
_aaxPhasingEffectSetState(_effect_t* effect, int state)
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
         int t, constant;

         data->run = _delay_run;
         data->loopback = AAX_FALSE;

         data->lfo.convert = _linear;
         data->lfo.state = effect->state;
         data->lfo.fs = effect->info->frequency;
         data->lfo.period_rate = effect->info->period_rate;

         data->lfo.min_sec = PHASING_MIN;
         data->lfo.max_sec = PHASING_MAX;
         data->lfo.depth = effect->slot[0]->param[AAX_LFO_DEPTH];
         data->lfo.offset = effect->slot[0]->param[AAX_LFO_OFFSET];
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
_aaxNewPhasingEffectHandle(const aaxConfig config, enum aaxEffectType type, _aax2dProps* p2d, UNUSED(_aax3dProps* p3d))
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
_aaxPhasingEffectSet(float val, int ptype, unsigned char param)
{  
   float rv = val;
   if ((param == AAX_DELAY_GAIN) && (ptype == AAX_DECIBEL)) {
      rv = _lin2db(val);
   }
   else if ((param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET) && 
            (ptype == AAX_MICROSECONDS)) {
       rv = (val*1e-6f)/PHASING_MAX;
   }
   return rv;
}
   
static float
_aaxPhasingEffectGet(float val, int ptype, unsigned char param)
{  
   float rv = val;
   if ((param == AAX_DELAY_GAIN) && (ptype == AAX_DECIBEL)) {
      rv = _db2lin(val);
   }
   else if ((param == AAX_LFO_DEPTH || param == AAX_LFO_OFFSET) && 
            (ptype == AAX_MICROSECONDS)) {
       rv = val*PHASING_MAX*1e6f;
   }
   return rv;
}

static float
_aaxPhasingEffectMinMax(float val, int slot, unsigned char param)
{
   static const _eff_minmax_tbl_t _aaxPhasingRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.01f, 0.0f, 0.0f }, { 1.0f, 10.0f, 1.0f, 1.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f,  0.0f, 0.0f }, { 0.0f,  0.0f, 0.0f, 0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxPhasingRange[slot].min[param],
                       _aaxPhasingRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_eff_function_tbl _aaxPhasingEffect =
{
   AAX_FALSE,
   "AAX_phasing_effect", 1.0f,
   (_aaxEffectCreate*)&_aaxPhasingEffectCreate,
   (_aaxEffectDestroy*)&_aaxPhasingEffectDestroy,
   (_aaxEffectSetState*)&_aaxPhasingEffectSetState,
   NULL,
   (_aaxNewEffectHandle*)&_aaxNewPhasingEffectHandle,
   (_aaxEffectConvert*)&_aaxPhasingEffectSet,
   (_aaxEffectConvert*)&_aaxPhasingEffectGet,
   (_aaxEffectConvert*)&_aaxPhasingEffectMinMax
};

void*
_delay_create(void *d, void *i)
{
   _aaxRingBufferDelayEffectData *data = d;
   _aaxMixerInfo *info = i;

   if (data == NULL)
   {
      data  = _aax_aligned_alloc(DSIZE);
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


   if (data && data->history == NULL)
   {
      unsigned int tracks = info->no_tracks;
      float fs = info->frequency;

      data->history_samples = TIME_TO_SAMPLES(fs, DELAY_EFFECTS_TIME);
      _aaxRingBufferCreateHistoryBuffer(&data->history,
                                        data->history_samples, tracks);

      if (!data->history)
      {
         free(data->offset);
         data->offset = NULL;

         _aax_aligned_free(data);
         data = NULL;
      }
   }

   return data;
}

void
_delay_swap(void *d, void *s)
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
         _aaxRingBufferDelayEffectData *ddef = dst->data;
         _aaxRingBufferDelayEffectData *sdef = src->data;

         assert(dst->data_size == src->data_size);

         _lfo_swap(&ddef->lfo, &sdef->lfo);
         ddef->delay = sdef->delay;
         ddef->history_samples = sdef->history_samples;
         ddef->loopback = sdef->loopback;
         ddef->run = sdef->run;
      }
   }
   dst->destroy = src->destroy;
   dst->swap = src->swap;
}

void
_delay_destroy(void *ptr)
{
   _aaxRingBufferDelayEffectData *data = ptr;
   if (data)
   {
      data->lfo.envelope = AAX_FALSE;
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
      _aax_aligned_free(data);
   }
}

/**
 * - d and s point to a buffer containing the delay effects buffer prior to
 *   the pointer.
 * - start is the starting pointer
 * - end is the end pointer (end-start is the number of samples)
 * - no_samples is the number of samples to process this run
 * - dmax does not include ds
 */
void
_delay_run(void *rb, MIX_PTR_T d, CONST_MIX_PTR_T s, MIX_PTR_T scratch,
             size_t start, size_t end, size_t no_samples, size_t ds,
             void *data, void *env, unsigned int track)
{
   static const size_t bps = sizeof(MIX_T);
   _aaxRingBufferSample *rbd = (_aaxRingBufferSample*)rb;
   _aaxRingBufferDelayEffectData* effect = data;
   size_t offs, noffs;
   float pitch, volume;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(s != 0);
   assert(d != 0);
   assert(start < end);
   assert(data != NULL);

   volume =  effect->delay.gain;
   offs = effect->delay.sample_offs[track];

   assert(start || (offs < ds));
   if (offs >= ds) offs = ds-1;

   if (start) {
      noffs = effect->offset->noffs[track];
   }
   else
   {
      noffs = (size_t)effect->lfo.get(&effect->lfo, env, s, track, end);
      effect->delay.sample_offs[track] = noffs;
      effect->offset->noffs[track] = noffs;
   }

   assert(s != d);
   
   if (offs && volume > LEVEL_96DB)
   {
      MIX_T *sptr, *dptr;
      ssize_t doffs;

      sptr = (MIX_T*)s + start;
      dptr = d + start;

      doffs = noffs - offs;
      pitch = _MAX(((float)end-(float)doffs)/(float)(end), 0.001f);

      _aax_memcpy(dptr, sptr, no_samples*bps);
      if (pitch == 1.0f) {
         rbd->add(dptr, sptr-offs, no_samples, volume, 0.0f);
      }
      else
      {
//       DBG_MEMCLR(1, scratch-ds, ds+end, sizeof(MIX_T));
         rbd->resample(scratch-ds, sptr-offs, 0, no_samples, 0.0f, pitch);
         rbd->add(dptr, scratch-ds, no_samples, volume, 0.0f);
      }
   }
}

