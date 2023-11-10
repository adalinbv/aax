/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

