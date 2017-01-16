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

#ifdef __SSE4_1__

#include <smmintrin.h>

static inline __m128
load_vec3(const vec3_t v)
{
   __m128i xy = _mm_loadl_epi64((const __m128i*)&v);
   __m128 z = _mm_load_ss(&v[2]);
   return _mm_movelh_ps(_mm_castsi128_ps(xy), z);
}


FN_PREALIGN float
_vec3Magnitude_sse41(const vec3_t v3)
{
   __m128 v = load_vec3(v3);
   return _mm_cvtss_f32(_mm_sqrt_ss(_mm_dp_ps(v, v, 0x71)));
}

FN_PREALIGN float
_vec3MagnitudeSquared_sse41(const vec3_t v3)
{
    __m128 v = load_vec3(v3);
   return _mm_cvtss_f32(_mm_dp_ps(v, v, 0x71));
}

FN_PREALIGN float
_vec3DotProduct_sse41(const vec3_t v31, const vec3_t v32)
{
   __m128 v1 = load_vec3(v31);
   __m128 v2 = load_vec3(v32);
   return _mm_cvtss_f32(_mm_dp_ps(v1, v2,  0x71));
}

FN_PREALIGN float
_vec3Normalize_sse41(vec3_t d, const vec3_t v3)
{
   __m128 v = load_vec3(v3);
   __m128 inverse_norm = _mm_rsqrt_ps(_mm_dp_ps(v, v, 0x77));
   __m128 norm =_mm_mul_ps(v, inverse_norm);
   vec4_t r;

   _mm_store_ps(r, norm);
   _aax_memcpy(d, r, 3*sizeof(float));
 
   return _vec3Magnitude_sse41(v3);
}

#endif /* SSE4 */

