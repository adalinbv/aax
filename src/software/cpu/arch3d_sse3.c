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

#include "arch3d_simd.h"

#ifdef __SSE3__

static inline __m128
load_vec3(const vec3_t v)
{
   __m128i xy = _mm_loadl_epi64((const __m128i*)&v);
   __m128 z = _mm_load_ss(&v[2]);
   return _mm_movelh_ps(_mm_castsi128_ps(xy), z);
}

static inline float
hsum_ps_sse3(__m128 v) {
   __m128 shuf = _mm_movehdup_ps(v);
   __m128 sums = _mm_add_ps(v, shuf);
   shuf = _mm_movehl_ps(shuf, sums);
   sums = _mm_add_ss(sums, shuf);
   return _mm_cvtss_f32(sums);
}

FN_PREALIGN float
_vec3MagnitudeSquared_sse3(const vec3_t v3)
{   
   __m128 v = load_vec3(v3);
   return hsum_ps_sse3(_mm_mul_ps(v, v));
}

FN_PREALIGN float
_vec3Magnitude_sse3(const vec3_t v3)
{
   __m128 v = load_vec3(v3);
   return sqrtf(hsum_ps_sse3(_mm_mul_ps(v, v)));
}

FN_PREALIGN float
_vec3DotProduct_sse3(const vec3_t v1, const vec3_t v2)
{
   return hsum_ps_sse3(_mm_mul_ps(load_vec3(v1), load_vec3(v2)));
}

FN_PREALIGN void
_vec4Matrix4_sse3(vec4_t d, const vec4_t pv, const mtx4_t m)
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
_pt4Matrix4_sse3(vec4_t d, const vec4_t pv, const mtx4_t m)
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

