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

#include <api.h>
#include <ringbuffer.h>


void
_aaxRingBufferMixStereo16(_aaxRingBuffer *drb, const _aaxRingBuffer *srb, const int32_ptrptr sptr, _aax2dProps *ep2d, unsigned int offs, unsigned int dno_samples, float gain, float svol, float evol)
{
   _aaxRingBufferData *drbi, *srbi;
   _aaxRingBufferLFOData *lfo;
   unsigned int rbd_tracks;
   unsigned int rbs_tracks;
   unsigned int track;
   void **tracks;
   float g;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   drbi = drb->handle;
   srbi = srb->handle;
   rbs_tracks = srbi->sample->no_tracks;
   rbd_tracks = drbi->sample->no_tracks;

   /** Mix */
   g = 1.0f;
   lfo = _FILTER_GET_DATA(ep2d, DYNAMIC_GAIN_FILTER);
   if (lfo && lfo->envelope)				// envelope follow
   {
      g = 0.0f;
      for (track=0; track<rbd_tracks; track++)
      {
         unsigned int rbs_track = track % rbs_tracks;
         float gain;

         gain = 1.0f - lfo->get(lfo, sptr[rbs_track]+offs, track, dno_samples);
         if (lfo->inv) g = 1.0f/gain;
         g += gain;
      }
      g /= rbd_tracks;
   }

   tracks = drbi->sample->track;
   for (track=0; track<rbd_tracks; track++)
   {
      unsigned int rbs_track = track % rbs_tracks;
      unsigned int rbd_track = track % rbd_tracks;
      int32_t *dptr = tracks[rbd_track]+offs;
      float vstart, vend, vstep;

      vstart = g*gain * svol * ep2d->prev_gain[track];
      vend = g*gain * evol * gain;
      vstep = (vend - vstart) / dno_samples;

#if RB_FLOAT_DATA
      _batch_fmadd(dptr, sptr[rbs_track]+offs, dno_samples, vstart, vstep);
#else
      _batch_imadd(dptr, sptr[rbs_track]+offs, dno_samples, vstart, vstep);
#endif

      ep2d->prev_gain[track] = gain;
   }
}

