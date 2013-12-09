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

FN_PREALIGN void
_vec4Matrix4_sse3(vec4_t d, const vec4_t pv, mtx4_t m)
{
   vec4_t vi;
   __m128 v;
   
   vec4Copy(vi, pv);
   vi[3] = 0.0f;

   v = _mm_load_ps((const float*)vi);
   __m128 vm0 = _mm_mul_ps(_mm_load_ps((const float*)(m+0)), v);
   __m128 vm1 = _mm_mul_ps(_mm_load_ps((const float*)(m+1)), v);
   __m128 vm2 = _mm_mul_ps(_mm_load_ps((const float*)(m+2)), v);
   __m128 vm3 = _mm_mul_ps(_mm_load_ps((const float*)(m+3)), v);
   _mm_store_ps(d, _mm_hadd_ps(_mm_hadd_ps(vm0, vm1), _mm_hadd_ps(vm2, vm3)));
}

FN_PREALIGN void
_pt4Matrix4_sse3(vec4_t d, const vec4_t pv, mtx4_t m)
{
   vec4_t vi;
   __m128 v;
   
   vec4Copy(vi, pv);
   vi[3] = 1.0f;

   v = _mm_load_ps((const float*)vi);
   __m128 vm0 = _mm_mul_ps(_mm_load_ps((const float*)(m+0)), v);
   __m128 vm1 = _mm_mul_ps(_mm_load_ps((const float*)(m+1)), v);
   __m128 vm2 = _mm_mul_ps(_mm_load_ps((const float*)(m+2)), v);
   __m128 vm3 = _mm_mul_ps(_mm_load_ps((const float*)(m+3)), v);
   _mm_store_ps(d, _mm_hadd_ps(_mm_hadd_ps(vm0, vm1), _mm_hadd_ps(vm2, vm3)));
}

#endif /* SSE3 */

