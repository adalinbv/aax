/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

/*
 * 2d M:N ringbuffer mixer functions.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <base/logging.h>
#include <base/threads.h>

#include <dsp/filters.h>
#include <dsp/effects.h>
#include <dsp/lfo.h>

#include <api.h>
#include <ringbuffer.h>

#include "rbuf_int.h"
#include "renderer.h"
#include "audio.h"

/**
 * Mix a multi track source buffer into a multi track destination buffer.
 * The result will be a flat (mono or stereo) destination buffer.
 *
 * When the source buffer has more tracks than the destination buffer the
 * first remaining source track will be mixed with the first destination 
 * buffer and so on.
 *
 * @drb multi track destination ringbuffer
 * @srb multi track source ringbuffer
 * @info aeonwaves info structure
 * @ep2d 2d emitter/sensor 2d properties
 * @fp2d mixer 2d properties
 * @ctr update-rate counter as number of instanced of the refresh-rate
 * @buffer_gain gain as specified in the sources aaxBuffer
 * @history source history buffer
 */
int
_aaxRingBufferMixMulti16(_aaxRingBuffer *drb, _aaxRingBuffer *srb, const void *renderer, _aax2dProps *ep2d, unsigned char ctr, float buffer_gain, _history_t history)
{
    const _aaxRendererData *data = (const _aaxRendererData*)renderer;
    const _aaxMixerInfo *info = data->info;
    _aax2dProps *fp2d = data->fp2d;
   _aaxRingBufferData *drbi, *srbi;
   _aaxRingBufferSample *drbd, *srbd;
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
   assert(info != NULL);

   drbi = drb->handle;
   srbi = srb->handle;
   assert(srbi != 0);
   assert(drbi != 0);
   assert(srbi->sample != 0);
   assert(drbi->sample != 0);

   srbd = srbi->sample;
   drbd = drbi->sample;

   // TODO: Why won't data->scratch work?
   scratch = (MIX_T**)drbd->scratch;

   /** Pitch */
   pitch = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_PITCH);

   pslide = _EFFECT_GET_DATA(ep2d, PITCH_EFFECT);
   penv = _EFFECT_GET_DATA(ep2d, TIMED_PITCH_EFFECT);
   lfo = _EFFECT_GET_DATA(ep2d, DYNAMIC_PITCH_EFFECT);
   if (lfo)
   {
      float pval = lfo->get(lfo, penv, NULL, 0, 0)-1.0f;
      if (fp2d) pval *= fp2d->final.pitch_lfo;
      pitch *= NORM_TO_PITCH(pval+1.0f);
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

   /** Resample */
   offs = 0;
   if (drbi->mode == AAX_MODE_WRITE_HRTF ||
       drbi->mode == AAX_MODE_WRITE_SURROUND /* also uses HRTF for up-down */)
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
         /* the ringbuffer was already flagged as stopped */
         /* but sptr still needs to get mixed             */
         ret = -1;
      }
      else if (srbi->stopped == 1)
      {
         /*
          * Distance delay induced stopping of playback
          * In the event that distance delay is not active dist_delay_sec equals
          * to 0 so detracting duration_sec instantly turns dist_delay_sec < 0.0
          */
         ep2d->dist_delay_sec -= drbd->duration_sec;
         if (ep2d->dist_delay_sec <= 0.0f) {
            ret = -1;
         }
      }
   }

   /* apply envelope filter */
   gnvel = ep2d->note.velocity;
   gain = _aaxEnvelopeGet(genv, srbi->stopped, &gnvel, penv); // gain0;
   if (gain <= -1e-3f) {
      ret = -2;
   }
   gain *= ep2d->note.soft * ep2d->note.pressure;

   /* Apply the parent mixer/audio-frame volume and tremolo-gain */
   max = 1.0f;
   if (fp2d) {
      gain *= _FILTER_GET(fp2d, VOLUME_FILTER, AAX_GAIN);
   }

   /* tremolo, envelope following gain filter is applied below! */
   lfo = _FILTER_GET_DATA(ep2d, DYNAMIC_GAIN_FILTER);
   if (lfo)
   {
      if (!lfo->envelope) {
         max *= lfo->get(lfo, genv, NULL, 0, 0);
      }
      if (fp2d) max *= fp2d->final.gain_lfo;
   }
   else if (fp2d && fabsf(fp2d->final.gain_lfo - 1.0f) > 0.1f) {
       gain *= 1.0f - fp2d->final.gain_lfo;
   }

   /* tremolo was defined */
   gain *= max;

   /* Final emitter volume */
   gain_emitter = _FILTER_GET(ep2d, VOLUME_FILTER, AAX_GAIN);
   if (genv) genv->value_total = gain*gain_emitter;

   gain *= buffer_gain; // bring gain to a normalized level
   gain = _square(gain)*ep2d->final.gain;
   gain *= gain_emitter;
   ep2d->final.silence = (fabsf(gain) >= LEVEL_128DB) ? 0 : 1;

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

      gain *= gnvel;
      drbd->mixmn(drbd, srbd, sptr, info->router, ep2d, offs, dno_samples,
                  gain, svol, evol, ctr);
   }

   if (ret >= -1 && drbi->playing == 0 && drbi->stopped == 1) {
      ret = 0;
   }

   return ret;
}

