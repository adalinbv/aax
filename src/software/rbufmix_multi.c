/*
 * Copyright 2005-2012 by Erik Hofman.
 * Copyright 2009-2012 by Adalin B.V.
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

#include <math.h>	/* floorf */
#include <assert.h>

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>
#include <base/logging.h>


/**
 * Mix a multi track source buffer into a multi track destination buffer.
 * The result will be a flat (mono or stereo) destination buffer.
 *
 * When the source buffer has more tracks than the destination buffer the
 * first remaining source track will be mixed with the first destination 
 * buffer and so on.
 *
 * @dest single or multi track destination buffer
 * @src single or multi track source buffer
 * @ep2d 2d emitter/sensor 2d properties
 * @fp2d mixer 2d properties
 */
int
_oalRingBufferMixMulti16(_oalRingBuffer *dest, _oalRingBuffer *src, _oalRingBuffer2dProps *ep2d, _oalRingBuffer2dProps *fp2d, unsigned char ctr, unsigned int nbuf)
{
   unsigned int offs, dno_samples, track;
   _oalRingBufferLFOInfo *lfo;
   _oalRingBufferSample *rbd;
   float svol, evol, max;
   float pitch, gain;
   int32_t **sptr;
   void *env;
   int ret = 0;
   float g;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(src != 0);
   assert(dest != 0);
   assert(src->sample != 0);
   assert(dest->sample != 0);

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
   pitch *= _oalRingBufferEnvelopeGet(env, src->stopped);

   max = _EFFECT_GET(ep2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.01f, max);

   /** Resample */
   offs = 0;
   sptr = _aaxProcessMixer(dest, src, ep2d, pitch, &offs, &dno_samples, ctr, nbuf);
   if (sptr == NULL || dno_samples == 0)
   {
      if (src->playing == 0 && src->stopped == 1) {
         return -1;
      } else {
         return 0;
      }
   }

   /** Volume */
   env = _FILTER_GET_DATA(ep2d, TIMED_GAIN_FILTER);
   if (src->playing == 0 && src->stopped == 1)
   {
      /* the emitter was already flagged as stopped */
      ret = -1;
   }
   else if (!env && src->stopped == 1)
   {
      /*
       * Distance delay induced stopping of playback
       * In the event that distance delay is not active dist_delay_sec equals
       * to 0 so detracting duration_sec instantly turns dist_delay_sec < 0.0
       */
      ep2d->dist_delay_sec -= dest->sample->duration_sec;
      if (ep2d->dist_delay_sec <= 0.0f) {
         ret = -1;
      }
   }

   /* apply envelope filter */
   gain = _oalRingBufferEnvelopeGet(env, src->stopped);
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
   if (!env && src->playing && src->stopped)
   {
      svol = (src->stopped || offs) ? 1.0f : 0.0f;
      evol = (src->stopped) ? 0.0f : 1.0f;
      src->playing = 0;
   }

   /** Mix */
   g = 1.0f;
   rbd = dest->sample;
   lfo = _FILTER_GET_DATA(ep2d, DYNAMIC_GAIN_FILTER);
   if (lfo && lfo->envelope)				// envelope follow
   {
      g = 0.0f;
      for (track=0; track<rbd->no_tracks; track++)
      {
         _oalRingBufferSample *rbs = src->sample;
         unsigned int rbs_track = track % rbs->no_tracks;
         float gain;

         gain = 1.0f - lfo->get(lfo, sptr[rbs_track]+offs, track, dno_samples);
         if (lfo->inv) g = 1.0f/gain;
         g += gain;
      }
      g /= rbd->no_tracks;
   }

   for (track=0; track<rbd->no_tracks; track++)
   {
      _oalRingBufferSample *rbs = src->sample;
      unsigned int rbs_track = track % rbs->no_tracks;
      unsigned int rbd_track = track % rbd->no_tracks;
      int32_t *dptr = (int32_t *)rbd->track[rbd_track]+offs;
      float vstart, vend, vstep;

      vstart = g*gain * svol * ep2d->prev_gain[track];
      vend = g*gain * evol * gain;
      vstep = (vend - vstart) / dno_samples;

//    DBG_MEMCLR(!offs, rbd->track[track], rbd->no_samples, sizeof(int32_t));
      _batch_fmadd(dptr, sptr[rbs_track]+offs, dno_samples, vstart, vstep);

      ep2d->prev_gain[track] = gain;
   }

   return ret;
}

