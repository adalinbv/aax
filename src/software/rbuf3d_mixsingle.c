/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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
 * 1:N ringbuffer mixer functions.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "rbuf_int.h"

/**
 * rpos: emitter position relative to the listener
 * dist_fact: the factor that translates the distance into meters/feet/etc.
 * speaker: the parents speaker positions
 * p2d: the emitters 2d properties structure
 * info: the mixers info structure
 */
void
_aaxSetupSpeakersFromDistanceVector(vec3f_t  rpos, float dist_fact,
                                    vec4f_t *speaker, _aax2dProps *p2d,
                                    const _aaxMixerInfo* info)
{
   unsigned int pos, i, t;
   float dp, offs, fact;

   switch (info->mode)
   {
   case AAX_MODE_WRITE_HRTF:
      for (t=0; t<info->no_tracks; t++)
      {
         for (i=0; i<3; i++)
         {
            dp = vec3fDotProduct(&speaker[3*t+i].v3, &rpos);
            dp *= speaker[t].v4[3];
            p2d->speaker[t].v4[i] = dp * dist_fact;		/* -1 .. +1 */

            offs = info->hrtf[HRTF_OFFSET].v4[i];
            fact = info->hrtf[HRTF_FACTOR].v4[i];

            pos = _AAX_MAX_SPEAKERS + 3*t + i;
            dp = vec3fDotProduct(&speaker[pos].v3, &rpos);
            p2d->hrtf[t].v4[i] = _MAX(offs + dp*fact, 0.0f);
         }
      }
      break;
   case AAX_MODE_WRITE_SURROUND:
      for (t=0; t<info->no_tracks; t++)
      {
#ifdef USE_SPATIAL_FOR_SURROUND
         dp = vec3fDotProduct(&speaker[t].v3, &rpos);
         dp *= speaker[t].v4[3];

         p2d->speaker[t].v4[0] = 0.5f + dp*dist_fact;
#else
         vec3fMulvec3(&p2d->speaker[t].v3, &speaker[t].v3, &rpos);
         vec3fScalarMul(&p2d->speaker[t].v3, &p2d->speaker[t].v3, dist_fact);
#endif
         i = DIR_UPWD;
         do				/* skip left-right and back-front */
         {
            offs = info->hrtf[HRTF_OFFSET].v4[i];
            fact = info->hrtf[HRTF_FACTOR].v4[i];

            pos = _AAX_MAX_SPEAKERS + 3*t + i;
            dp = vec3fDotProduct(&speaker[pos].v3, &rpos);
            p2d->hrtf[t].v4[i] = _MAX(offs + dp*fact, 0.0f);
         }
         while(0);
      }
      break;
   case AAX_MODE_WRITE_SPATIAL:
      for (t=0; t<info->no_tracks; t++)
      {						/* speaker == sensor_pos */
         dp = vec3fDotProduct(&speaker[t].v3, &rpos);
         dp *= speaker[t].v4[3];

         p2d->speaker[t].v4[0] = 0.5f + dp*dist_fact;
      }
      break;
   default: /* AAX_MODE_WRITE_STEREO */
      for (t=0; t<info->no_tracks; t++)
      {
         vec3fMulvec3(&p2d->speaker[t].v3, &speaker[t].v3, &rpos);
         vec4fScalarMul(&p2d->speaker[t], &p2d->speaker[t], dist_fact);
      }
   }
}

