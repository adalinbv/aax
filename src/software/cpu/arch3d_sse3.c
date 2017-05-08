/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#include <api.h>

#include "arch3d_simd.h"

#ifdef __SSE3__

static inline FN_PREALIGN __m128
load_vec3f(const vec3f_ptr v)
{
   static ALIGN16 const uint32_t m3a32[] ALIGN16C = {
       0xffffffff,0xffffffff,0xffffffff,0
   };
   return _mm_and_ps(v->s4, _mm_load_ps((const float*)m3a32));
}

static inline FN_PREALIGN float
hsum_ps_sse3(__m128 v) {
   __m128 shuf = _mm_movehdup_ps(v);
   __m128 sums = _mm_add_ps(v, shuf);
   shuf = _mm_movehl_ps(shuf, sums);
   sums = _mm_add_ss(sums, shuf);
   return _mm_cvtss_f32(sums);
}

FN_PREALIGN float
_vec3fMagnitudeSquared_sse3(const vec3f_ptr v3)
{   
   __m128 v = load_vec3f(v3);
   return hsum_ps_sse3(_mm_mul_ps(v, v));
}

FN_PREALIGN float
_vec3fMagnitude_sse3(const vec3f_ptr v3)
{
   __m128 v = load_vec3f(v3);
   return sqrtf(hsum_ps_sse3(_mm_mul_ps(v, v)));
}

FN_PREALIGN float
_vec3fDotProduct_sse3(const vec3f_ptr v1, const vec3f_ptr v2)
{
   return hsum_ps_sse3(_mm_mul_ps(load_vec3f(v1), load_vec3f(v2)));
}

FN_PREALIGN void
_vec4fMatrix4_sse3(vec4f_ptr d, const vec4f_ptr pv, const mtx4f_ptr m)
{
   __m128 v = load_vec3f(&pv->v3);
   __m128 s0 = _mm_mul_ps(v, m->s4x4[0]);
   __m128 s1 = _mm_mul_ps(v, m->s4x4[1]);
   __m128 s2 = _mm_mul_ps(v, m->s4x4[2]);
   __m128 s3 = _mm_setzero_ps();
   d->s4 = _mm_hadd_ps(_mm_hadd_ps(s0, s1), _mm_hadd_ps(s2, s3));
}

#else
typedef int make_iso_compilers_happy;
#endif /* SSE3 */

