/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * 2d M:N ringbuffer mixer functions ranging from extremely fast to extremely
 * precise.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <dsp/filters.h>
#include <dsp/effects.h>

#include <api.h>
#include <ringbuffer.h>

#include "software/rbuf_int.h"

void
_aaxRingBufferMixStereo16(_aaxRingBufferSample *drbd, const _aaxRingBufferSample *srbd, CONST_MIX_PTRPTR_T sptr, const unsigned char *router, _aax2dProps *ep2d, size_t offs, size_t dno_samples, float gain, float svol, float evol, UNUSED(char cptr))
{
   _aaxRingBufferLFOData *lfo;
   unsigned int rbd_tracks;
   unsigned int rbs_tracks;
   unsigned int track;
   float g;

   _AAX_LOG(LOG_DEBUG, __func__);

   rbs_tracks = srbd->no_tracks;
   rbd_tracks = drbd->no_tracks;

   /** Mix */
   g = 1.0f;
   lfo = _FILTER_GET_DATA(ep2d, DYNAMIC_GAIN_FILTER);
   if (lfo && lfo->envelope)				// envelope follow
   {
      void *env = _EFFECT_GET_DATA(ep2d, TIMED_PITCH_EFFECT);

      g = 0.0f;
      for (track=0; track<rbd_tracks; track++)
      {
         unsigned int rbs_track = track % rbs_tracks;
         float gain;

         DBG_TESTNAN(sptr[rbs_track]+offs, dno_samples);
         gain = 1.0f-lfo->get(lfo, env, sptr[rbs_track]+offs, track, dno_samples);
         if (lfo->inv) g = 1.0f/gain;
         g += gain;
      }
      g /= rbd_tracks;
   }

   for (track=0; track<rbd_tracks; track++)
   {
      unsigned int rbs_track = track % rbs_tracks;
      unsigned int rbd_track = track % rbd_tracks;
      MIX_T *dptr = (MIX_T*)drbd->track[router[rbd_track]] + offs;
      float vstart, vend, vstep;

      vstart = g*gain * svol * ep2d->prev_gain[track];
      vend = g*gain * evol * gain;
      vstep = (vend - vstart) / dno_samples;

      drbd->add(dptr, sptr[rbs_track]+offs, dno_samples, vstart, vstep);

      ep2d->prev_gain[track] = gain;
   }
}

