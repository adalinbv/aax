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

/*
 * 1:N ringbuffer mixer functions.
 */

/*
 * Sources:
 * http://en.wikipedia.org/wiki/Doppler_effect
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _DEBUG
# define _DEBUG		0
#endif

#include <assert.h>

#include <base/logging.h>
#include <base/threads.h>

#include <software/renderer.h>
#include <dsp/filters.h>
#include <dsp/effects.h>
#include <dsp/lfo.h>

#include <api.h>

#include <ringbuffer.h>

#include "rbuf_int.h"
#include "audio.h"

/**
 * Mix a single track source buffer into a multi track destination buffer.
 * The result will be a stereo-mixed multitrack recording.
 *
 * When the source buffer has more tracks than the destination buffer the
 * first remaining source track will be mixed with the first destination
 * buffer and so on.
 *
 * @param drbi multi track destination buffer
 * @param srbi single track source buffer
 * @param mode requested mixing mode
 * @param ep2d 3d positioning information structure of the source
 * @param fp2f 3d positioning information structure of the parents frame
 * @param ch channel to use from the source buffer if it is multi-channel
 * #param ctr update-rate counter:
 *     - Rendering to the destination buffer is done every frame at the
 *       interval rate. Updating of 3d properties and the like is done
 *       once every 'ctr' frame updates. so if ctr == 1, updates are
 *       done every frame.
 */
int
_aaxRingBufferMixMono16(_aaxRingBuffer *drb, _aaxRingBuffer *srb, _aax2dProps *ep2d, void *data, unsigned char ch, unsigned char ctr, float gain_init, _history_t history)
{
   _aaxRendererData *renderer = data;
   const _aaxMixerInfo *info =  renderer->info;
   _aaxDelayed3dProps *fdp3d_m = renderer->fp3d->m_dprops3d;
   _aax2dProps *fp2d = renderer->fp2d;
   _aaxRingBufferData *drbi, *srbi;
   _aaxRingBufferSample *drbd;
   _aaxEnvelopeData *penv, *pslide;
   _aaxEnvelopeData *genv;
   _aaxLFOData *lfo;
   CONST_MIX_PTRPTR_T sptr;
   size_t offs, dno_samples;
   float pnvel, gnvel, gain;
   FLOAT pitch, max;
   int ret = 0;

   _AAX_LOG(LOG_DEBUG, __func__);

   assert(drb != NULL);
   assert(srb != NULL);

   drbi = drb->handle;
   srbi = srb->handle;
   assert(srbi != 0);
   assert(drbi != 0);
   assert(srbi->sample != 0);
   assert(drbi->sample != 0);
   assert(ep2d != 0);

   drbd = drbi->sample;

   /** Pitch */
   pitch = ep2d->final.pitch; /* Doppler effect */
   pitch *= _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_PITCH);

   pslide = _EFFECT_GET_DATA(ep2d, PITCH_EFFECT);
   penv = _EFFECT_GET_DATA(ep2d, TIMED_PITCH_EFFECT);
   lfo = _EFFECT_GET_DATA(ep2d, DYNAMIC_PITCH_EFFECT);
   if (lfo) 
   {
      float pval = lfo->get(lfo, penv, NULL, 0, 0)-1.0f;
      if (fp2d) pval *= fp2d->final.pitch_lfo;
      pitch *= NORM_TO_PITCH(pval+1.0f);
   }
   else if (fp2d && fabsf(fp2d->final.pitch_lfo - 1.0f) > 0.0f) {
      pitch *= NORM_TO_PITCH(fp2d->final.pitch_lfo);
   }

   if (fp2d) {
      pitch *= _EFFECT_GET(fp2d, PITCH_EFFECT, AAX_PITCH);
   }

   pnvel = gnvel = 1.0f;
   pitch *= _aaxEnvelopeGet(penv, srbi->stopped, &pnvel, NULL);

   if (pslide)
   {
      pnvel = 1.0f;
      pitch *= _aaxEnvelopeGet(pslide, srbi->stopped, &pnvel, NULL);
   }

   max = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch*ep2d->pitch_factor, 0.0f, max);

   /** DECODE, resample and apply effects */
   offs = 0;
   if (drbi->mode == AAX_MODE_WRITE_HRTF ||
       drbi->mode == AAX_MODE_WRITE_SURROUND /* also uses HRTF for up-down */)
   {
      offs = drbi->sample->dde_samples;
   }

   sptr = drbi->mix(drb, srb, ep2d, pitch, &offs, &dno_samples, ctr, history);
   if (sptr == NULL || dno_samples == 0)
   {
      if (srbi->playing == 0 && srbi->stopped == 1) {
         return -1;
      } else {
         return 0;
      }
   }

   /** Volume */
   genv = _FILTER_GET_DATA(ep2d, TIMED_GAIN_FILTER);
   if (!genv)
   {
      if (srbi->playing == 0 && srbi->stopped == 1)
      {
         /* The emitter was already flagged as stopped */
         ret = -1;
      }
      else if (srbi->stopped == 1)
      {
         /*
          * Distance delay induced stopping of playback
          * In the event that distance delay is not active dist_delay_sec equals
          * to 0 so detracting duration_sec instantly turns dist_delay_sec < 0.0
          */
         ep2d->dist_delay_sec -= drbi->sample->duration_sec;
         if (ep2d->dist_delay_sec <= 0.0f) {
            ret = -1;
         }
      }
   }

   /* Apply envelope filter */
   gnvel = ep2d->note.velocity;
   gain = _aaxEnvelopeGet(genv, srbi->stopped, &gnvel, penv);
   if (gain <= -1e-3f) {
      ret = -2;
   }

   /* Apply the parent mixer/audio-frame volume and tremolo-gain */
   max = 1.0f;
   if (fp2d) {
      gain *= _FILTER_GET(fp2d, VOLUME_FILTER, AAX_GAIN);
   }

   /* Tremolo and envelope following gain filter */
   lfo = _FILTER_GET_DATA(ep2d, DYNAMIC_GAIN_FILTER);
   if (lfo)
   {
      if (lfo->envelope)
      {
         float g = lfo->get(lfo, genv, sptr[ch]+offs, 0, dno_samples);
         if (lfo->inv) g = 1.0f/g;
         gain *= g;
      }
      else {
         max *= lfo->get(lfo, genv, NULL, 0, 0);
      }
      if (fp2d) max *= fp2d->final.gain_lfo;
   }
   else if (fp2d && fabsf(fp2d->final.gain_lfo - 1.0f) > 0.1f) {
       gain *= 1.0f - fp2d->final.gain_lfo;
   }

   /* Tremolo was defined */
   if (max != 1.0f) {
      gain *= 1.0f - max/2.0f;
   }

   /* Final emitter volume */
   gain *= _FILTER_GET(ep2d, VOLUME_FILTER, AAX_GAIN);
   if (genv) genv->value_total = gain;

   /* 3d: distance, audio-cone and occlusion related gain */
   gain *= gain_init;
   gain = _square(gain)*ep2d->final.gain;

// if (gain >= LEVEL_128DB)
   {
      float svol, evol;

      /* Automatic volume ramping to avoid clicking */
      svol = evol = 1.0f;
      if (!genv && !srbi->streaming && (srbi->playing == srbi->stopped))
      {
         svol = (srbi->stopped || offs) ? 1.0f : 0.0f;
         evol = (srbi->stopped) ? 0.0f : 1.0f;
         srbi->playing = !srbi->stopped;
      }

      /* Distance attenutation frequency filtering */
      if (ep2d->final.k < 0.9f) // only filter when fc < 17600 Hz
      {
         float *hist = ep2d->final.freqfilter_history[0];
         MIX_PTR_T s = (MIX_PTR_T)sptr[ch] + offs;

#if RB_FLOAT_DATA
         _batch_movingaverage_float(s, s, dno_samples, hist+0, ep2d->final.k);
         _batch_movingaverage_float(s, s, dno_samples, hist+1, ep2d->final.k);
#else
         _batch_movingaverage(s, s, dno_samples, hist+0, ep2d->final.k);
         _batch_movingaverage(s, s, dno_samples, hist+1, ep2d->final.k);
#endif
      }

      if (_PROP3D_MONO_IS_DEFINED(fdp3d_m))
      {
         drbd->mix1(drbd, sptr, info->router, ep2d, ch, offs, dno_samples,
                    info->frequency, gain, svol, evol, ctr);
      }
      else
      {
         drbd->mix1n(drbd, sptr, info->router, ep2d, ch, offs, dno_samples,
                     info->frequency, gain, svol, evol, ctr);
      }
   }

   if (ret >= -1 && drbi->playing == 0 && drbi->stopped == 1) {
      ret = 0;
   }

   return ret;
}

