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

static _oalRingBufferMixMNFunc _oalRingBufferMixMulti16Effects;
_oalRingBufferMixMNFunc *_oalRingBufferMixMulti16 = _oalRingBufferMixMulti16Effects;

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
 * @p2d 2d emitter/sensor 2d properties
 * @mix_p2d mixer 2d properties
 */
int
_oalRingBufferMixMulti16Effects(_oalRingBuffer *dest, _oalRingBuffer *src, _oalRingBuffer2dProps *p2d, _oalRingBuffer2dProps *mix_p2d, float pitch, float gain, unsigned char ctr, unsigned int n)
{
   unsigned int offs, dno_samples, track;
   _oalRingBufferLFOInfo *lfo;
   _oalRingBufferSample *rbd;
   float svol, evol, max;
   int32_t **sptr;
   void *env;
   int ret = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(src != 0);
   assert(dest != 0);
   assert(src->sample != 0);
   assert(dest->sample != 0);

   /** Pitch */
   if (mix_p2d)
   {
      pitch *= _EFFECT_GET(mix_p2d, PITCH_EFFECT, AAX_PITCH);
      pitch *= mix_p2d->final.pitch_lfo;
   }

   pitch *= _EFFECT_GET(p2d, PITCH_EFFECT, AAX_PITCH);
   lfo = _EFFECT_GET_DATA(p2d, DYNAMIC_PITCH_EFFECT);
   if (lfo) {
      pitch *= lfo->get(lfo, NULL, 0, 0);
   }

   env = _EFFECT_GET_DATA(p2d, TIMED_PITCH_EFFECT);
   pitch *= _oalRingBufferEnvelopeGet(env, src->stopped);

   max = _EFFECT_GET(p2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.0f, max);

#if 0
pitch = 0.5f;
printf("pitch: %f\n", pitch);
#endif
   /** Resample */
   offs = 0;
   sptr = _aaxProcessMixer(dest, src, p2d, pitch, &offs, &dno_samples, ctr, n);
   if (sptr == NULL)
   {
      if (src->playing == 0 && src->stopped == 1) {
         return -1;
      } else {
         return 0;
      }
   }

   /** Volume */
   env = _FILTER_GET_DATA(p2d, TIMED_GAIN_FILTER);
   if (!env && src->playing == 0 && src->stopped == 1) {
      ret = -1;
   }

   gain *= _oalRingBufferEnvelopeGet(env, src->stopped);
   if (gain < -1e-3f) {
      ret = -1;
   }

   max = 1.0f;
   if (mix_p2d)
   {
      gain *= _FILTER_GET(mix_p2d, VOLUME_FILTER, AAX_GAIN);
      max *= mix_p2d->final.gain_lfo;
   }

   lfo = _FILTER_GET_DATA(p2d, DYNAMIC_GAIN_FILTER);
   if (lfo && !lfo->envelope) {
      max *= lfo->get(lfo, NULL, 0, 0);
   }
   if (max != 1.0f) {
      gain *= 1.0f - max/2.0f;
   }

   gain *= _FILTER_GET(p2d, VOLUME_FILTER, AAX_GAIN);

   /** Automatic volume ramping to avoid clicking */
   svol = evol = 1.0f;
   if (!env && src->playing && src->stopped)
   {
      svol = (src->stopped || offs) ? 1.0f : 0.0f;
      evol = (src->stopped) ? 0.0f : 1.0f;
      src->playing = 0;
   }

   /** Mix */
   rbd = dest->sample;
   lfo = _FILTER_GET_DATA(p2d, DYNAMIC_GAIN_FILTER);
   for (track=0; track<rbd->no_tracks; track++)
   {
      _oalRingBufferSample *rbs = src->sample;
      unsigned int rbs_track = track % rbs->no_tracks;
      unsigned int rbd_track = track % rbd->no_tracks;
      int32_t *dptr = (int32_t *)rbd->track[rbd_track]+offs;
      float vstart, vend, vstep, g = 1.0f;

      if (lfo && lfo->envelope)
      {
         g = 1.0f - lfo->get(lfo, sptr[rbs_track]+offs, track, dno_samples);
         if (lfo->inv) g = 1.0f/g;
      }

      vstart = g*gain * svol * p2d->prev_gain[track];
      vend = g*gain * evol * gain;
      vstep = (vend - vstart) / dno_samples;

//    DBG_MEMCLR(!offs, rbd->track[track], rbd->no_samples, sizeof(int32_t));
      _batch_fmadd(dptr, sptr[rbs_track]+offs, dno_samples, vstart, vstep);

      p2d->prev_gain[track] = gain;
   }

   return ret;
}

