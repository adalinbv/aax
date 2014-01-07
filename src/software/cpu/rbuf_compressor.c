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

extern const float _compress_tbl[2][2048];

#define BITS		11
#define SHIFT		(31-BITS)
#define START		((1<<SHIFT)-1)
#define FACT		(float)(23-SHIFT)/(float)(1<<(31-SHIFT))
#if 1
void
_aaxRingBufferCompress(MIX_PTR_T d, unsigned int *dmin, unsigned int *dmax, float clip, float asym)
{
   static const float df = (float)(int32_t)0x7FFFFFFF;
   static const float rf = 1.0f/(65536.0f*12.0f);
   float osamp, imix, mix;
   unsigned int j, max;
   MIX_T *ptr = d;
   MIX_T iasym;
   float peak;
   double rms;

   osamp = 0.0f;
   rms = peak = 0;
   mix = _MINMAX(clip, 0.0f, 1.0f);
   imix = (1.0f - mix);
   j = max = *dmax - *dmin;
   iasym = asym*16*(1<<SHIFT);
   do
   {
      float val, fact1, fact2, sdf, rise;
      MIX_T asamp, samp;
      unsigned int pos;

      samp = *ptr;
      val = (float)samp*samp;	// RMS
      rms += val;
      if (val > peak) peak = val;

      asamp = (samp < 0) ? abs(samp-iasym) : abs(samp);
      pos = ((int32_t)asamp >> SHIFT);
      sdf = _MINMAX(asamp*df, 0.0f, 1.0f);

      rise = _MINMAX((osamp-samp)*rf, 0.3f, 303.3f);
      pos = (unsigned int)_MINMAX(pos+asym*rise, 1, ((1<<BITS)));
      osamp = samp;

      fact1 = (1.0f-sdf)*_compress_tbl[0][pos-1];
      fact1 += sdf*_compress_tbl[0][pos];

      fact2 = (1.0f-sdf)*_compress_tbl[1][pos-1];
      fact2 += sdf*_compress_tbl[1][pos];

      *ptr++ = (MIX_T)((imix*fact1 + mix*fact2)*samp);
   }
   while (--j);
 
   *dmax = (unsigned int)sqrtf(peak);
   *dmin = (unsigned int)sqrt(rms/max);
}

#else
/* arctan */
void
bufCompress(void *d, unsigned int dmin, unsigned int dmax, float clip, float asym)
{
   static const float df = 1.0f/2147483648.0f;
   int32_t *ptr = (int32_t*)d;
   unsigned int j;
   float mix;

   mix = 256.0f; // * _MINMAX(clip, 0.0, 1.0);
   j = dmax-dmin;
   do
   {
      float samp = atan(*ptr*df*mix)*GMATH_1_PI_2;
      *ptr++ = samp*8388608;
   }
   while (--j);
}
#endif
