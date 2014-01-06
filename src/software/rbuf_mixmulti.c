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

#include <api.h>
#include <ringbuffer.h>

#include "ringbuffer.h"

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
_aaxRingBufferMixMulti16(_aaxRingBuffer *drb, _aaxRingBuffer *srb, _aax2dProps *ep2d, _aax2dProps *fp2d, unsigned char ctr, unsigned int nbuf)
{
   unsigned int offs, dno_samples;
   _aaxRingBufferData *drbi, *srbi;
   _aaxRingBufferLFOData *lfo;
   float svol, evol, max;
   float pitch, gain;
   int32_t **sptr;
   void *env;
   int ret = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(drb != NULL);
   assert(srb != NULL);

   drbi = drb->handle;
   srbi = srb->handle;
   assert(srbi != 0);
   assert(drbi != 0);
   assert(srbi->sample != 0);
   assert(drbi->sample != 0);

   /** Pitch */
   pitch = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_PITCH);
   lfo = _EFFECT_GET_DATA(ep2d, DYNAMIC_PITCH_EFFECT);
   if (lfo) {
      pitch *= lfo->get(lfo, NULL, 0, 0);
   }

   if (fp2d)
   {
      float lfo = 1.5f-fp2d->final.pitch_lfo;
      float opitch = pitch-0.5f;
      pitch *= _EFFECT_GET(fp2d, PITCH_EFFECT, AAX_PITCH);
      pitch = lfo*opitch + (1.0f-lfo)*0.5f + 0.5f;

      /* pitch (factor) moves around 1.0. But 1.0/0.5 equals to 2.0 */
      /* instead to 1.5, so we have to correct for that             */
//    if (pitch > 1.1f) pitch *= 1.3333333f;
   }

   env = _EFFECT_GET_DATA(ep2d, TIMED_PITCH_EFFECT);
   pitch *= _aaxRingBufferEnvelopeGet(env, srbi->stopped);

   max = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.01f, max);

   /** Resample */
   offs = 0;
   sptr = drbi->mix(drbi, srbi, ep2d, pitch, &offs, &dno_samples, ctr, nbuf);
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
      /* the emitter was already flagged as stopped */
      ret = -1;
   }
   else if (!env && srbi->stopped == 1)
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

   /* apply envelope filter */
   gain = _aaxRingBufferEnvelopeGet(env, srbi->stopped);
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
      max *= lfo->get(lfo, NULL, 0, 0);
   }

   /* tremolo was defined */
   if (max != 1.0f) {
      gain *= 1.0f - max/2.0f;
   }

   /* final emitter volume */
   gain *= _FILTER_GET(ep2d, VOLUME_FILTER, AAX_GAIN);

   /** Automatic volume ramping to avoid clicking */
   svol = evol = 1.0f;
   if (!env && srbi->playing && srbi->stopped)
   {
      svol = (srbi->stopped || offs) ? 1.0f : 0.0f;
      evol = (srbi->stopped) ? 0.0f : 1.0f;
      srbi->playing = 0;
   }

   drbi->mixmn(drb, srb, sptr, ep2d, offs, dno_samples, gain, svol, evol);

   return ret;
}

