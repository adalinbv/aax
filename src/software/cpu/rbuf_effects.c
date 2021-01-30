/*
 * Copyright 2005-2020 by Erik Hofman.
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

#include <base/logging.h>
#include <base/geometry.h>

#include <dsp/filters.h>
#include <dsp/effects.h>

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <software/rbuf_int.h>
#include <software/renderer.h>


/**
 * 1nd order effects:
 *   Apply to registered emitters and registered sensors only.
 *
 * - dst and scratch point to the beginning of a buffer containing room for
 *   the delay effects prior to the pointer.
 * - start is the starting pointer
 * - end is the end pointer (end-start is the number of smaples)
 * - dmax does not include ds
 */
#define BUFSWAP(a, b) do { void* t = (a); (a) = (b); (b) = t; } while (0);

#if 0
void
_aaxRingBufferEffectsApply1st(_aaxRingBufferSample *rbd,
          MIX_PTR_T dst, MIX_PTR_T src, UNUSED(MIX_PTR_T scratch),
          size_t start, UNUSED(size_t end), size_t no_samples,
          size_t ddesamps, unsigned int track, _aax2dProps *p2d,
          UNUSED(unsigned char ctr), unsigned char mono)
{
   static const size_t bps = sizeof(MIX_T);
   _aaxRingBufferReflectionData *reflections = _EFFECT_GET_DATA(p2d, REVERB_EFFECT);
#ifndef NDEBUG
// _aaxRingBufferDelayEffectData *delay = _EFFECT_GET_DATA(p2d, DELAY_EFFECT);
// size_t ds = delay ? ddesamps : 0; /* 0 for frequency filtering */
#endif
   MIX_T *psrc, *pdst;

   src += start;
   dst += start;

   psrc = src; /* might change further in the code */
   pdst = dst; /* might change further in the code */

   if (reflections)
   {
      _aaxRingBufferOcclusionData *occlusion = reflections->reverb->occlusion;
      float v = (1.0f - occlusion->level);
      r = reflections->run(rbd, pdst, psrc, no_samples, ddesamps, track,
                           v, reflections, NULL, mono);
      if (r) BUFSWAP(pdst, psrc);
   }

   if (dst == pdst)	/* copy the data back to the dst buffer */
   {
//    DBG_MEMCLR(1, dst-ds, ds+end, bps);
      memcpy(dst, src, no_samples*bps);
   }
}
#endif

/**
 * 2nd order effects:
 *   Apply to all registered emitters, registered sensors and
 *   registered audio-frames.
 *
 * - dst and scratch point to the beginning of a buffer containing room for
 *   the delay effects prior to the pointer.
 * - start is the starting pointer
 * - end is the end pointer (end-start is the number of smaples)
 * - dmax does not include ds
 */
#define BUFSWAP(a, b) do { void* t = (a); (a) = (b); (b) = t; } while (0);

void
_aaxRingBufferEffectsApply2nd(_aaxRingBufferSample *rbd,
          MIX_PTR_T dst, MIX_PTR_T src, MIX_PTR_T scratch,
          size_t start, size_t end, size_t no_samples,
          size_t ddesamps, unsigned int track, _aax2dProps *p2d,
          unsigned char ctr, unsigned char mono)
{
   static const size_t bps = sizeof(MIX_T);
   void *env = _FILTER_GET_DATA(p2d, TIMED_GAIN_FILTER);
   MIX_T *psrc, *pdst;
   size_t ds = 0;
   int r, state;

   src += start;
   dst += start;

   // audio-frames, and streaming emitters, with delay effects need
   // the source history
   state = _EFFECT_GET_STATE(p2d, DELAY_EFFECT);
   state |= _EFFECT_GET_STATE(p2d, REVERB_EFFECT);
   if (state)
   {
      _aaxRingBufferDelayEffectData *delay;
      _aaxRingBufferReverbData *reverb;

      delay = _EFFECT_GET_DATA(p2d, DELAY_EFFECT);
      reverb = _EFFECT_GET_DATA(p2d, REVERB_EFFECT);

      if (delay) {
         ds = delay->prepare(dst, src, no_samples, delay, track);
      }

      if (reverb && reverb->reflections)
      {
//       ds = reverb->reflections->history_samples;
         reverb->reflections_prepare(dst, src, no_samples, reverb, track);
      }
   }

   psrc = src; /* might change further in the code */
   pdst = dst; /* might change further in the code */

   /* occlusion */
   state = _FILTER_GET_STATE(p2d, VOLUME_FILTER);
   if (state)
   {
      _aaxRingBufferOcclusionData *occlusion;

      occlusion = _FILTER_GET_DATA(p2d, VOLUME_FILTER);
      if (occlusion)
      {
         r = occlusion->run(rbd, pdst, psrc, scratch, no_samples, track, occlusion);
         if (r) BUFSWAP(pdst, psrc);
      }
   }

   /* bitcrusher filter */
   // Note: bitcrushing takes two steps.
   //       noise is added after the frequency filter.
   state = _FILTER_GET_STATE(p2d, BITCRUSHER_FILTER);
   if (state)
   {
      _aaxRingBufferBitCrusherData *bitcrush;

      bitcrush = _FILTER_GET_DATA(p2d, BITCRUSHER_FILTER);
      if (bitcrush) {
        bitcrush->run(psrc, end, no_samples, bitcrush, env, track);
      }
   }

   /* modulator effect */
   state = _EFFECT_GET_STATE(p2d, RINGMODULATE_EFFECT);
   if (state)
   {
      _aaxRingBufferModulatorData *modulator;

      modulator = _EFFECT_GET_DATA(p2d, RINGMODULATE_EFFECT);
      if (modulator) {
         modulator->run(psrc, end, no_samples, modulator, env, track);
      }
   }

   /* frequency filter */
   state = _FILTER_GET_STATE(p2d, FREQUENCY_FILTER);
   if (state)
   {
      _aaxRingBufferFreqFilterData *freq;

      freq =_FILTER_GET_DATA(p2d, FREQUENCY_FILTER);
      if (freq)
      {
         float v = p2d->note.velocity;
         r = freq->run(rbd, pdst, psrc, 0, end, ds, track, freq, env, v, ctr);
         if (r) BUFSWAP(pdst, psrc);
      }
   }

   /* bitcrusher: add noise */
   state = _FILTER_GET_STATE(p2d, BITCRUSHER_FILTER);
   if (state)
   {
      _aaxRingBufferBitCrusherData *bitcrush;

      bitcrush = _FILTER_GET_DATA(p2d, BITCRUSHER_FILTER);
      if (bitcrush) {
         bitcrush->add_noise(psrc, end, no_samples, bitcrush, env, track);
      }
   }

   /* distortion */
   state = _EFFECT_GET_STATE(p2d, DISTORTION_EFFECT);
   if (state)
   {
      _aaxRingBufferDistoritonData *distort;
      _aaxFilterInfo *dist_effect;

      dist_effect = &p2d->effect[DISTORTION_EFFECT];
      distort = dist_effect->data;
      if (distort)
      {
         r = distort->run(rbd, pdst, psrc, 0, end, ds, track, dist_effect, env);
         if (r) BUFSWAP(pdst, psrc);
      }
   }

   /* phasing, chorus, flanging or delay-line */
   state = _EFFECT_GET_STATE(p2d, DELAY_EFFECT);
   if (state)
   {
      _aaxRingBufferDelayEffectData *delay;

      delay = _EFFECT_GET_DATA(p2d, DELAY_EFFECT);
      if (delay)
      {
         r = delay->run(rbd, pdst, psrc, scratch, 0, end, no_samples, ds,
                        delay, env, track);
         if (r) BUFSWAP(pdst, psrc);
      }
   }

   /* reverb */
   state = _EFFECT_GET_STATE(p2d, REVERB_EFFECT);
   if (state)
   {
      _aaxRingBufferReverbData *reverb;

      reverb = _EFFECT_GET_DATA(p2d, REVERB_EFFECT);
      if (reverb)
      {
         _aaxRingBufferReverbData *parent_reverb = NULL;
         _aax2dProps *pp2d = p2d->parent;

         if (pp2d) {
            parent_reverb = _EFFECT_GET_DATA(pp2d, REVERB_EFFECT);
         }

         r = reverb->run(rbd, pdst, psrc, scratch, no_samples, ddesamps, track,
                         reverb, parent_reverb, NULL, mono, reverb->state, env,
                         ctr);
         if (r) BUFSWAP(pdst, psrc);
      }
   }

   /* copy the data back to the dst buffer, if necessary */
   if (dst == pdst)
   {
//    DBG_MEMCLR(1, dst-ds, ds+end, bps);
      memcpy(dst, src, no_samples*bps);
   }
}

