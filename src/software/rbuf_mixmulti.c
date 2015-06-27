/*
 * Copyright 2005-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

/*
 * 2d M:N ringbuffer mixer functions ranging from extremely fast to extremely
 * precise.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <base/logging.h>
#include <base/threads.h>

#include <dsp/filters.h>
#include <dsp/effects.h>

#include <api.h>
#include <ringbuffer.h>

#include "rbuf_int.h"
#include "audio.h"

/**
 * Mix a multi track source buffer into a multi track destination buffer.
 * The result will be a flat (mono or stereo) destination buffer.
 *
 * When the source buffer has more tracks than the destination buffer the
 * first remaining source track will be mixed with the first destination 
 * buffer and so on.
 *
 * @drbi single or multi track destination buffer
 * @srbi single or multi track source buffer
 * @ep2d 2d emitter/sensor 2d properties
 * @fp2d mixer 2d properties
 */
int
_aaxRingBufferMixMulti16(_aaxRingBuffer *drb, _aaxRingBuffer *srb, const _aaxMixerInfo *info, _aax2dProps *ep2d, _aax2dProps *fp2d, unsigned char ctr)
{
   _aaxRingBufferSample *drbd, *srbd;
   size_t offs, dno_samples;
   _aaxRingBufferData *drbi, *srbi;
   _aaxRingBufferEnvelopeData* env;
   _aaxRingBufferLFOData *lfo;
   float svol, evol, max;
   float pitch, gain, nvel;
   CONST_MIX_PTRPTR_T sptr;
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

   /** Pitch */
   pitch = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_PITCH);

   env = _EFFECT_GET_DATA(ep2d, TIMED_PITCH_EFFECT);
   lfo = _EFFECT_GET_DATA(ep2d, DYNAMIC_PITCH_EFFECT);
   if (lfo)
   {
      float pval = lfo->get(lfo, env, NULL, 0, 0)-1.0f;
      if (fp2d) pval *= fp2d->final.pitch_lfo;
      pitch *= NORM_TO_PITCH(pval+1.0f);
   }

   if (fp2d) {
      pitch *= _EFFECT_GET(fp2d, PITCH_EFFECT, AAX_PITCH);
   }

   if (ep2d->note.velocity == 1.0f) {
      nvel = 1.0f;
   } else {
      nvel = powf(ep2d->note.velocity, ep2d->curr_pos_sec);
   }
   pitch *= _aaxRingBufferEnvelopeGet(env, srbi->stopped, nvel);
   pitch *= ep2d->note.pressure;

   max = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.01f, max);

   /** Resample */
   offs = 0;
#if 0
{
 size_t samps = srb->get_parami(srb, RB_NO_SAMPLES);
 int t;
 for(t=0; t<srb->get_parami(srb, RB_NO_TRACKS); t++) {
  DBG_TESTNAN((MIX_T*)srbd->track[t], samps);
  printf("%i | %x: mix, no_samples: %i\n", t, srbd->track[t], samps);
 }
}
#endif
   sptr = drbi->mix(drb, srb, ep2d, pitch, &offs, &dno_samples, ctr);
   if (sptr == NULL || dno_samples == 0)
   {
      if (srbi->playing == 0 && srbi->stopped == 1) {
         return -1;
      } else {
         return 0;
      }
   }

   /** Volume */
   env = _FILTER_GET_DATA(ep2d, TIMED_GAIN_FILTER);
   if (srbi->playing == 0 && srbi->stopped == 1)
   {
      /* the ringbuffer was already flagged as stopped */
      /* but sptr still needs to get mixed             */
      ret = -1;
   }
   else if (!env && srbi->stopped == 1)
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

   /* apply envelope filter */
   gain = _aaxRingBufferEnvelopeGet(env, srbi->stopped, nvel);
   gain *= ep2d->note.pressure;
   if (gain < -1e-3f) {
      ret = -1;
   }

   /* apply the parent mixer/audio-frame volume and tremolo-gain */
   max = 1.0f;
   if (fp2d)
   {
      gain *= _FILTER_GET(fp2d, VOLUME_FILTER, AAX_GAIN);
      max *= fp2d->final.gain_lfo;
   }

   /* tremolo, envelope following gain filter is applied below! */
   lfo = _FILTER_GET_DATA(ep2d, DYNAMIC_GAIN_FILTER);
   if (lfo && !lfo->envelope) {
      max *= lfo->get(lfo, env, NULL, 0, 0);
   }

   /* tremolo was defined */
   if (max != 1.0f) {
      gain *= 1.0f - max/2.0f;
   }

   /* final emitter volume */
   gain *= _FILTER_GET(ep2d, VOLUME_FILTER, AAX_GAIN);

   /** Automatic volume ramping to avoid clicking */
   svol = evol = 1.0f;
   if (!env && !srbi->streaming && (srbi->playing && srbi->stopped))
   {
      svol = (srbi->stopped || offs) ? 1.0f : 0.0f;
      evol = (srbi->stopped) ? 0.0f : 1.0f;
      srbi->playing = 0;
   }

   drbd->mixmn(drbd, srbd, sptr, info->router, ep2d, offs, dno_samples, gain, svol, evol, ctr);

   if (drbi->playing == 0 && drbi->stopped == 1) {
      ret = 0;
   }

   return ret;
}

