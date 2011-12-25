/*
 * Copyright 2005-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
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
 * @p2d 2d emitter 2d properties
 * @mix_p2d mixter 2d properties
 */
int
_oalRingBufferMixMulti16Effects(_oalRingBuffer *dest, _oalRingBuffer *src, _oalRingBuffer2dProps *p2d, _oalRingBuffer2dProps *mix_p2d, float pitch, float gain)
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
   pitch *= _EFFECT_GET(mix_p2d, PITCH_EFFECT, AAX_PITCH);
   lfo = _EFFECT_GET_DATA(mix_p2d, VIBRATO_EFFECT);
   if (lfo) {
      pitch *= lfo->get(lfo, 0);
   }

   pitch *= _EFFECT_GET(p2d, PITCH_EFFECT, AAX_PITCH);
   lfo = _EFFECT_GET_DATA(p2d, VIBRATO_EFFECT);
   if (lfo) {
      pitch *= lfo->get(lfo, 0);
   }

   env = _EFFECT_GET_DATA(p2d, TIMED_PITCH_EFFECT);
   pitch *= _oalRingBufferEnvelopeGet(env, src->stopped);

   max = _EFFECT_GET(p2d, PITCH_EFFECT, AAX_MAX_PITCH);
   pitch = _MINMAX(pitch, 0.0f, max);

   /** Resample */
   offs = 0;
   sptr = _aaxProcessMixer(dest, src, p2d, pitch, &offs, &dno_samples);
   if (sptr == NULL) {
      return ret;
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

   gain *= _FILTER_GET(mix_p2d, VOLUME_FILTER, AAX_GAIN);
   lfo = _FILTER_GET_DATA(mix_p2d, TREMOLO_FILTER);
   if (lfo) {
      gain *= lfo->get(lfo, 0);
   }

   gain *= _FILTER_GET(p2d, VOLUME_FILTER, AAX_GAIN);
   lfo = _FILTER_GET_DATA(p2d, TREMOLO_FILTER);
   if (lfo) {
      gain *= lfo->get(lfo, 0);
   }

   /** Automatic volume ramping to avoid clicking */
   svol = evol = 1.0f;
   if (!env && src->playing && src->stopped)
   {
      svol = (src->stopped) ? 1.0f : 0.0f;
      evol = (src->stopped) ? 0.0f : 1.0f;
      src->playing = 0;
   }

   /** Mix */
   rbd = dest->sample;
   for (track=0; track<rbd->no_tracks; track++)
   {
      _oalRingBufferSample *rbs = src->sample;
      unsigned int rbs_track = track % rbs->no_tracks;
      unsigned int rbd_track = track % rbd->no_tracks;
      float vstart, vend, vstep;
      int32_t *dptr;

      vstart = gain * svol * p2d->prev_gain[track];
      vend = gain * evol * gain;
      vstep = (vend - vstart) / dno_samples;

      dptr = (int32_t *)rbd->track[rbd_track]+offs;
      _batch_fmadd(dptr, sptr[rbs_track]+offs, dno_samples, vstart, vstep);

      p2d->prev_gain[track] = gain;
   }

   return ret;
}

