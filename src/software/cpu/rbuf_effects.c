/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
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

#include <base/logging.h>
#include <base/geometry.h>

#include <dsp/filters.h>
#include <dsp/effects.h>

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <software/rbuf_int.h>
#include <software/renderer.h>


#define BUFSWAP(a, b) do { void* t = (a); (a) = (b); (b) = t; } while (0);

void
_aaxRingBufferEffectsApply(_aaxRingBufferSample *rbd,
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
   state |= _EFFECT_GET_STATE(p2d, DELAY_LINE_EFFECT);
   state |= _EFFECT_GET_STATE(p2d, REVERB_EFFECT);
   if (state)
   {
      _aaxRingBufferDelayEffectData *delay, *delay_line;
      _aaxRingBufferReverbData *reverb;

      delay = _EFFECT_GET_DATA(p2d, DELAY_EFFECT);
      delay_line = _EFFECT_GET_DATA(p2d, DELAY_LINE_EFFECT);
      reverb = _EFFECT_GET_DATA(p2d, REVERB_EFFECT);

      if (delay && delay->state) {
         ds = delay->prepare(dst, src, no_samples, delay, track);
      }
      if (delay_line && delay_line->state) {
         ds = delay_line->prepare(dst, src, no_samples, delay_line, track);
      }

      if (reverb && reverb->state && reverb->reflections)
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

   /* phasing, chorus, flanging */
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

   /* delay-line */
   state = _EFFECT_GET_STATE(p2d, DELAY_LINE_EFFECT);
   if (state)
   {
      _aaxRingBufferDelayEffectData *delay_line;

      delay_line = _EFFECT_GET_DATA(p2d, DELAY_LINE_EFFECT);
      if (delay_line)
      {
         r = delay_line->run(rbd, pdst, psrc, scratch, 0, end, no_samples, ds,
                             delay_line, env, track);
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

