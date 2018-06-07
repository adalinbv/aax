/*
 * Copyright 2005-2018 by Erik Hofman.
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
#include <math.h>	/* tan */

#include <base/logging.h>
#include <base/geometry.h>
#include <base/random.h>

#include <dsp/filters.h>
#include <dsp/effects.h>

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <software/rbuf_int.h>
#include <software/renderer.h>
#include <software/gpu/gpu.h>


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

void
_aaxRingBufferEffectsApply1st(_aaxRingBufferSample *rbd,
          MIX_PTR_T dst, MIX_PTR_T src, UNUSED(MIX_PTR_T scratch),
          size_t start, size_t end, size_t no_samples,
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
      reflections->run(rbd, pdst, psrc, no_samples, ddesamps, track,
                       v, reflections, NULL, mono);
      BUFSWAP(pdst, psrc);
   }

   if (dst == pdst)	/* copy the data back to the dst buffer */
   {
//    DBG_MEMCLR(1, dst-ds, ds+end, bps);
      _aax_memcpy(dst, src, no_samples*bps);
   }
}


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
   void *env = _FILTER_GET_DATA(p2d, TIMED_GAIN_FILTER);
   _aaxRingBufferOcclusionData *occlusion =_FILTER_GET_DATA(p2d, VOLUME_FILTER);
   _aaxRingBufferFreqFilterData *freq =_FILTER_GET_DATA(p2d, FREQUENCY_FILTER);
   _aaxRingBufferDelayEffectData *delay = _EFFECT_GET_DATA(p2d, DELAY_EFFECT);
   _aaxRingBufferReverbData *reverb = _EFFECT_GET_DATA(p2d, REVERB_EFFECT);
   _aaxRingBufferBitCrusherData *bitcrush = _FILTER_GET_DATA(p2d, BITCRUSHER_FILTER);
   static const size_t bps = sizeof(MIX_T);
   size_t ds = delay ? ddesamps : 0; /* 0 for frequency filtering */
   void *distort_data = NULL;
   MIX_T *psrc, *pdst;

   if (_EFFECT_GET_STATE(p2d, DISTORTION_EFFECT)) {
      distort_data = &p2d->effect[DISTORTION_EFFECT];
   }

   src += start;
   dst += start;

   /* streaming emitters with delay effects need the source history */
   if (delay && !delay->loopback && delay->history_ptr)
   {
      // copy the delay effects history to src
//    DBG_MEMCLR(1, src-ds, ds, bps);
      _aax_memcpy(src-ds, delay->delay_history[track], ds*bps);

      // copy the new delay effects history back
      _aax_memcpy(delay->delay_history[track], src+no_samples-ds, ds*bps);
   }

   psrc = src; /* might change further in the code */
   pdst = dst; /* might change further in the code */

   if (occlusion)
   {
      occlusion->run(rbd, pdst, psrc, scratch, no_samples, track, occlusion);
      BUFSWAP(pdst, psrc);
   }

   // Note: bitcrushing takes two steps.
   //       noise is added after the frequency filter.
   if (bitcrush)
   {
      float level = bitcrush->lfo.get(&bitcrush->lfo, NULL, NULL, 0, 0);
      if (level > 0.01f)
      {
         unsigned bps = sizeof(MIX_T);

         level = powf(2.0f, 8+sqrtf(level)*13.5f);      // (24-bits/sample)
         _batch_fmul_value(psrc, bps, no_samples, 1.0f/level);
         _batch_cvt24_ps24(psrc, psrc, no_samples);
         _batch_cvtps24_24(psrc, psrc, no_samples);
         _batch_fmul_value(psrc, bps, no_samples, level);
      }
   }

   if (freq)
   {
      freq->run(rbd, pdst, psrc, 0, end, ds, track, freq, env, ctr);
      BUFSWAP(pdst, psrc);
   }

   if (bitcrush)
   {
      _aaxEnvelopeData *genv = _FILTER_GET_DATA(p2d, TIMED_GAIN_FILTER);
      float ratio = _FILTER_GET(p2d, BITCRUSHER_FILTER, AAX_NOISE_LEVEL);
      if (ratio > 0.01f)
      {
         unsigned int i;

         ratio *= bitcrush->env.get(&bitcrush->env, genv, psrc, 0, end);
         ratio *= (0.25f * 8388608.0f)/UINT64_MAX;
         for (i=0; i<no_samples; ++i) {
            psrc[i] += ratio*xorshift128plus();
         }
      }
   }

   if (distort_data)
   {
      _aaxFilterInfo *dist_effect = (_aaxFilterInfo*)distort_data;
      _aaxRingBufferDistoritonData *distort = dist_effect->data;

      distort->run(rbd, pdst, psrc, 0, end, ds, track, distort_data, env);
      BUFSWAP(pdst, psrc);
   }

   if (delay)
   {
      /* Apply delay effects */
      if (delay->loopback) {		/*    flanging     */
         delay->run(rbd, psrc, psrc, scratch, 0, end, no_samples, ds,
                    delay, env, track);
      }
      else				/* phasing, chorus */
      {
         delay->run(rbd, pdst, psrc, scratch, 0, end, no_samples, ds,
                    delay, env, track);
         BUFSWAP(pdst, psrc);
      }
   }

   if (reverb)
   {
      reverb->run(rbd, pdst, psrc, scratch, no_samples, ddesamps, track,
                  reverb, NULL, mono);
      BUFSWAP(pdst, psrc);
   }

   if (dst == pdst)	/* copy the data back to the dst buffer */
   {
//    DBG_MEMCLR(1, dst-ds, ds+end, bps);
      _aax_memcpy(dst, src, no_samples*bps);
   }
}

/* -------------------------------------------------------------------------- */

// size is the number of sampler for every track
size_t
_aaxRingBufferCreateHistoryBuffer(void **hptr, int32_t *history[_AAX_MAX_SPEAKERS], size_t size, int tracks)
{
   char *ptr, *p;
   int i;

   size *= sizeof(MIX_T);
#if BYTE_ALIGN
   if (size & MEMMASK)
   {
      size |= MEMMASK;
      size++;
   }
   p = 0;
   ptr = _aax_calloc(&p, tracks, size);
#else
   ptr = p = calloc(tracks, size);
#endif
   if (ptr)
   {
      *hptr = ptr;
      for (i=0; i<tracks; ++i)
      {
         history[i] = (int32_t*)p;
         p += size;
      }
   }
   return size;
}

