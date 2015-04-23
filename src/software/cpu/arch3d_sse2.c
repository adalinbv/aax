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

#include <math.h>	/* for floorf */


#include "software/rbuf_int.h"
#include "arch2d_simd.h"

#ifdef __SSE2__

FN_PREALIGN float
_vec3DotProduct_sse2(const vec3 v1, const vec3 v2)
{
   __m128 L0 = _mm_set_ps(v1[0], v1[1], v1[2], 0);
   __m128 R0 = _mm_set_ps(v2[0], v2[1], v2[2], 0);
   __m128 dot;
   float r;

   dot = _mm_mul_ps(L0, L1);
   dot = _mm_add_ps(_mm_movehl_ps(dot, dot), dot);
   dot = _mm_add_ss(_mm_shuffle_ps(dot, dot, 1), dot);
   _mm_store_ss(&r, dot); 

   return r;
}

#endif /* SSE2 */

