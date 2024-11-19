/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if 0
#include <stdio.h>

#include <math.h>       /* floorf */
#include <assert.h>

#include <api.h>

#include <base/types.h>
#include <base/geometry.h>
#include <base/logging.h>

#include <ringbuffer.h>
#include <arch.h>

#include "audio.h"
#include "ringbuffer.h"
#include "cpu/arch2d_simd.h"
#endif

#include <base/types.h>

#include "software/rbuf_int.h"

extern const float _limiter_tbl[2][2048];

#define BITS		11
#define SHIFT		(31-BITS)
#define START		((1<<SHIFT)-1)
#define FACT		(float)(23-SHIFT)/(float)(1<<(31-SHIFT))
void
_aaxRingBufferLimiter(MIX_PTR_T d, size_t dmax, float clip, float asym)
{
   static const float df = (float)(65535.0f*256.0f);
   static const float rf = 1.0f/(65536.0f*12.0f);
   float osamp, mix;
   size_t j, max;
   MIX_T *ptr = d;
   MIX_T iasym;

   j = max = dmax;

   osamp = 0.0f;
   mix = _MINMAX(clip, 0.0f, 1.0f);
   iasym = asym*16*(1<<SHIFT);
   do
   {
      float fact1, fact2, sdf, rise;
      MIX_T asamp, samp;
      size_t pos;

      samp = *ptr;

      asamp = (samp < 0) ? fabsf(samp-iasym) : fabsf(samp);
      pos = ((int32_t)asamp >> SHIFT);
      sdf = _MINMAX(asamp*df, 0.0f, 1.0f);

      rise = _MINMAX((osamp-samp)*rf, 0.3f, 303.3f);
      pos = (size_t)_MINMAX(pos+asym*rise, 1, (1<<BITS))-1;
      osamp = samp;

      fact1 = (1.0f-sdf)*_limiter_tbl[0][pos-1];
      fact1 += sdf*_limiter_tbl[0][pos];

      fact2 = (1.0f-sdf)*_limiter_tbl[1][pos-1];
      fact2 += sdf*_limiter_tbl[1][pos];

      *ptr++ = samp*(mix*(fact2 - fact1) + fact1);
   }
   while (--j);
}

/* arctan */
void
_aaxRingBufferCompress(MIX_PTR_T d, size_t dmax, UNUSED(float clip), UNUSED(float asym))
{
   _batch_limit(d, d, dmax);
}
