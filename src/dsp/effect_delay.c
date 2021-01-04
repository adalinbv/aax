/*
 * Copyright 2007-2020 by Erik Hofman.
 * Copyright 2009-2020 by Adalin B.V.
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

#include <base/types.h> 
#include <software/rbuf_int.h>

#include "effects.h"
#include "arch.h"
#include "dsp.h"

/*
 * This code is shared between the phasing, chorus and flanging effects
 */

#define DSIZE		sizeof(_aaxRingBufferDelayEffectData)


void*
_delay_create(void *d, void *i, char delay, char feedback)
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

   if (data)
   {
      int tracks = info->no_tracks;
      float fs = info->frequency;

      data->history_samples = TIME_TO_SAMPLES(fs, DELAY_EFFECTS_TIME);

      if (data->history == NULL) {
         _aaxRingBufferCreateHistoryBuffer(&data->history,
                                           data->history_samples, tracks);
      }
      if (data->feedback_history == NULL) {
         _aaxRingBufferCreateHistoryBuffer(&data->feedback_history,
                                           data->history_samples, tracks);
      }

      if (!data->history || !data->feedback_history)
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
         ddef->feedback = sdef->feedback;
         ddef->flanger = sdef->flanger;
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

void
_delay_reset(void *ptr)
{
   _aaxRingBufferDelayEffectData *data = ptr;

   if (data)
   {
      _lfo_reset(&data->lfo);
      if (data->freq_filter) _freqfilter_reset(data->freq_filter);
   }
}

size_t
_delay_prepare(MIX_PTR_T dst, MIX_PTR_T src, size_t no_samples, void *data, unsigned int track)
{
   static const size_t bps = sizeof(MIX_T);
   _aaxRingBufferDelayEffectData* delay = data;
   size_t ds;

   assert(delay);
   assert(delay->history);
   assert(delay->history->ptr);
   assert(bps <= sizeof(MIX_T));

   ds = delay->history_samples;

   // copy the delay effects history to src
// DBG_MEMCLR(1, src-ds, ds, bps);
   _aax_memcpy(src-ds, delay->history->history[track], ds*bps);

   // copy the new delay effects history back
   _aax_memcpy(delay->history->history[track], src+no_samples-ds, ds*bps);

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
int
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
   int rv = AAX_FALSE;
   float volume;

   _AAX_LOG(LOG_DEBUG, __func__);

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
      noffs = (size_t)effect->lfo.get(&effect->lfo, env, s, track, end);
      effect->delay.sample_offs[track] = noffs;
      effect->offset->noffs[track] = noffs;
   }

   assert(s != d);

   volume = effect->feedback;
   if (offs && volume > LEVEL_96DB)
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
         do
         {
            rbd->add(nsptr, nsptr-coffs, step, volume, 0.0f);

            nsptr += step;
            coffs += sign;
            i -= step;
         }
         while(i >= step);
      }
      if (i) {
         rbd->add(nsptr, nsptr-coffs, i, volume, 0.0f);
      }
      effect->offset->coffs[track] = coffs;

      _aax_memcpy(effect->feedback_history->history[track], sptr+no_samples-ds, ds*bps);

      nsptr = sptr;
   }

   volume =  effect->delay.gain;
   if (offs && volume > LEVEL_96DB)
   {
      _aaxRingBufferFreqFilterData *flt = effect->freq_filter;
      MIX_T *dptr = d + start;
      ssize_t doffs;

      doffs = noffs - offs;

      // first process the delayed (wet) signal
      // then add the original (dry) signal
      if (doffs == 0)
      {
         if (flt && ((flt->type == LOWPASS && flt->fc < MAXIMUM_CUTOFF) ||
                     (flt->type == HIGHPASS && flt->fc > MINIMUM_CUTOFF)))
         {
            flt->run(rbd, dptr, nsptr-offs, 0, no_samples, 0, track, flt, env, 1.0f, 0);
         }  else if (fabsf(volume - 1.0) > LEVEL_96DB) {
            rbd->multiply(dptr, nsptr-offs, bps, no_samples, volume);
         }
         rbd->add(dptr, sptr, no_samples, 1.0f, 0.0f);
         rv = AAX_TRUE;
      }
      else
      {
         float pitch = _MAX(((float)end-(float)doffs)/(float)(end), 0.001f);
         rbd->resample(dptr, nsptr-offs, 0, no_samples, 0.0f, pitch);

         if (flt && ((flt->type == LOWPASS && flt->fc < MAXIMUM_CUTOFF) ||
                     (flt->type == HIGHPASS && flt->fc > MINIMUM_CUTOFF)))
         {
            flt->run(rbd, dptr, dptr, 0, no_samples, 0, track, flt, env, 1.0f, 0);
         } else if (fabsf(volume - 1.0) > LEVEL_96DB) {
            rbd->multiply(dptr, dptr, bps, no_samples, volume);
         }
         rbd->add(dptr, sptr, no_samples, 1.0f, 0.0f);
         rv = AAX_TRUE;
      }
   }

   return rv;
}

