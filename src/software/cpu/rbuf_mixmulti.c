/*
 * Copyright 2005-2021 by Erik Hofman.
 * Copyright 2009-2021 by Adalin B.V.
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
 * 2d M:N ringbuffer mixer function.
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
_aaxRingBufferMixStereo16(_aaxRingBufferSample *drbd, const _aaxRingBufferSample *srbd, CONST_MIX_PTRPTR_T sptr, const unsigned char *router, _aax2dProps *ep2d, size_t offs, size_t dno_samples, float gain, float svol, float evol, UNUSED(char cptr))
{
   int rbd_tracks = drbd->no_tracks;
   int rbs_tracks = srbd->no_tracks;
   int no_tracks = _MAX(rbs_tracks, rbd_tracks);
   int t;

   for (t=0; t<no_tracks; t++)
   {
      int track = t % rbd_tracks;
      int ch = t % rbs_tracks;
      MIX_T *dptr = (MIX_T*)drbd->track[router[track]] + offs;
      float vstart, vend, vstep;

      vstart = ep2d->prev_gain[t] * svol;
      vend = gain * evol;
      vstep = (vend - vstart) / dno_samples;

//    DBG_MEMCLR(!offs, drbd->track[t], drbd->no_samples, sizeof(int32_t));
      drbd->add(dptr, sptr[ch]+offs, dno_samples, vstart, vstep);

      ep2d->prev_gain[t] = vend;
   }
}

