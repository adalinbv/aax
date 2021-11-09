/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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
 * 2d M:N ringbuffer mixer functions ranging from extremely fast to extremely
 * precise.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <dsp/filters.h>
#include <dsp/effects.h>
#include <dsp/lfo.h>

#include <api.h>
#include <ringbuffer.h>

#include "software/rbuf_int.h"

void
_aaxRingBufferMixStereo16(_aaxRingBufferSample *drbd, const _aaxRingBufferSample *srbd, CONST_MIX_PTRPTR_T sptr, const unsigned char *router, _aax2dProps *ep2d, size_t offs, size_t dno_samples, float gain, UNUSED(float svol), float evol, UNUSED(char cptr))
{
   _aaxLFOData *lfo;
   int rbd_tracks;
   int rbs_tracks;
   int track;
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
         int rbs_track = track % rbs_tracks;
         float cgain = 1.0f;

         DBG_TESTNAN(sptr[rbs_track]+offs, dno_samples);
         cgain -= lfo->get(lfo, env, sptr[rbs_track]+offs, track, dno_samples);
         if (lfo->inv) g = 1.0f/cgain;
         g += cgain;
      }
      g /= rbd_tracks;
   }

   for (track=0; track<rbs_tracks; track++)
   {
      int rbs_track = track % rbs_tracks;
      int rbd_track = track % rbd_tracks;
      MIX_T *dptr = (MIX_T*)drbd->track[router[rbd_track]] + offs;
      float vstart, vend, vstep;

      vstart = ep2d->prev_gain[track];
      vend = gain * g*evol;
      vstep = (vend - vstart) / dno_samples;

      drbd->add(dptr, sptr[rbs_track]+offs, dno_samples, vstart, vstep);

      ep2d->prev_gain[track] = vend;
   }
}

