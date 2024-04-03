/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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
_aaxRingBufferMixMono16(_aaxRingBuffer *drb, _aaxRingBuffer *srb, _aax2dProps *ep2d, void *renderer, unsigned char track, unsigned char ctr, float buffer_gain, _history_t history)
{
   _aaxRendererData *data = renderer;
   const _aaxMixerInfo *info =  data->info;
   _aaxDelayed3dProps *fdp3d_m = data->fp3d->m_dprops3d;
   _aax2dProps *fp2d = data->fp2d;
   _aaxRingBufferData *drbi, *srbi;
   _aaxRingBufferSample *drbd;
   _aaxEnvelopeData *penv, *pslide;
   _aaxEnvelopeData *genv;
   _aaxLFOData *lfo;
   CONST_MIX_PTRPTR_T sptr;
   MIX_T **scratch;
   size_t offs, dno_samples;
   float gain, gain_emitter;
   float pnvel, gnvel;
   FLOAT pitch, min, max;
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

   // TODO: Why won't data->scratch work
   scratch = (MIX_T**)drbd->scratch;

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

   pnvel = 1.0f;
   if (penv) {
      pitch *= _aaxEnvelopeGet(penv, srbi->stopped, &pnvel, NULL);
   }

   if (pslide && _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_PITCH_RATE) > 0.0f)
   {
      pnvel = 1.0f;
      pitch *= _aaxEnvelopeGet(pslide, srbi->stopped, &pnvel, NULL);
   }

   min = 1e-3f;
   max = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch*ep2d->pitch_factor, min, max);

   /** DECODE, resample and apply effects */
   offs = 0;
   if (drbi->mode == AAX_MODE_WRITE_HRTF ||
       drbi->mode == AAX_MODE_WRITE_SPATIAL_SURROUND /* uses HRTF for up-down */
      )
   {
      offs = drbi->sample->dde_samples;
   }

   sptr = drbi->mix(scratch, drb, srb, ep2d, pitch, &offs, &dno_samples,
                    ctr, history);
   if (sptr == NULL || dno_samples == 0)
   {
      if (!dno_samples || (srbi->playing == 0 && srbi->stopped == 1)) {
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
   if (gain <= -1e-3f)
   {
      if (srbi->sampled_release) {
         gain = 1.0f/gnvel;
      } else {
         ret = -2;
      }
   }
   gain *= ep2d->note.soft * ep2d->note.pressure;

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
         float g = lfo->get(lfo, genv, sptr[track]+offs, 0, dno_samples);
         if (lfo->inverse) g = 1.0f/g;
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
   gain *= max;

   /* Final emitter volume */
   gain_emitter = _FILTER_GET(ep2d, VOLUME_FILTER, AAX_GAIN);
   if (genv) genv->value_total = gain*gain_emitter;

   /* 3d: distance, audio-cone and occlusion related gain */
   gain *= buffer_gain; // bring gain to a normalized level
   gain = _square(gain)*ep2d->final.gain;
   gain *= gain_emitter;
   ep2d->final.silence = (fabsf(gain) >= LEVEL_128DB) ? false : true;

   lfo = _FILTER_GET_DATA(ep2d, DYNAMIC_LAYER_FILTER);
   if (lfo)
   {
      unsigned char no_layers = srb->get_parami(srb, RB_NO_LAYERS);
      if (no_layers > 1)
      {
         unsigned char mix_layer = (track + 1) % no_layers;
         MIX_PTR_T s = (MIX_PTR_T)sptr[track] + offs;
         float mix = lfo->get(lfo, genv, s, 0, dno_samples);

         mix = _MINMAX(mix, 0.0f, 1.0f);
         if (mix == 1.0) {
            track = mix_layer;
         }
         else if (mix > 0.0f)
         {
            drbd->multiply(s, s, sizeof(MIX_T), dno_samples, 1.0f - mix);
            drbd->add(s, sptr[mix_layer]+offs, dno_samples, mix, 0.0f);
         }
      }
   }

// if (!ep2d->final.silence)
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
         MIX_PTR_T s = (MIX_PTR_T)sptr[track] + offs;

         _batch_movingaverage_float(s, s, dno_samples, hist+0, ep2d->final.k);
         _batch_movingaverage_float(s, s, dno_samples, hist+1, ep2d->final.k);
      }

      gain = _MINMAX(gain*gnvel, ep2d->final.gain_min, ep2d->final.gain_max);
      if (_PROP3D_MONO_IS_DEFINED(fdp3d_m))
      {
         drbd->mix1(drbd, sptr, info->router, ep2d, track, offs, dno_samples,
                    info->frequency, gain, svol, evol, ctr);
      }
      else
      {
         drbd->mix1n(drbd, sptr, info->router, ep2d, track, offs, dno_samples,
                     info->frequency, gain, svol, evol, ctr);
      }
   }

   if (ret >= -1 && drbi->playing == 0 && drbi->stopped == 1) {
      ret = 0;
   }

   return ret;
}

