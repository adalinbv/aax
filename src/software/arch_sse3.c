/*
 * Copyright 2005-2012 by Erik Hofman.
 * Copyright 2009-2012 by Adalin B.V.
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

#include "arch_simd.h"

#ifdef __SSE3__

void
_vec4Matrix4_sse3(vec4 d, const vec4 vi, mtx4 m)
{
   __m128 v = _mm_load_ps((const float*)vi);
   __m128 vm0 = _mm_mul_ps(_mm_load_ps((const float*)(m+0)), v);
   __m128 vm1 = _mm_mul_ps(_mm_load_ps((const float*)(m+1)), v);
   __m128 vm2 = _mm_mul_ps(_mm_load_ps((const float*)(m+2)), v);
   __m128 vm3 = _mm_mul_ps(_mm_load_ps((const float*)(m+3)), v);
   _mm_store_ps(d, _mm_hadd_ps(_mm_hadd_ps(vm0, vm1), _mm_hadd_ps(vm2, vm3)));
}

#endif /* SSE3 */

