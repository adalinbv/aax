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

#ifdef __SSE4__

FN_PREALIGN float
_vec3Magnitude_sse4(const vec3 v3)
{
   __m128 v = _mm_set_ps(v3[0], v3[1], v3[2], 0);
   return _mm_cvtss_f32(_mm_sqrt_ss(_mm_dp_ps(v, v, 0x71)));
}

FN_PREALIGN float
_vec3MagnitudeSquared_sse4(const vec3 v3)
{
    __m128 v = _mm_set_ps(v3[0], v3[1], v3[2], 0);
   return _mm_cvtss_f32(_mm_dp_ps(v, v, 0x71));
}

FN_PREALIGN float
_vec3DotProduct_sse4(const vec3 v31, const vec3 v32)
{
   __m128 v1 = _mm_set_ps(v31[0], v31[1], v31[2], 0);
   __m128 v2 = _mm_set_ps(v32[0], v32[1], v32[2], 0);
   return _mm_cvtss_f32(_mm_dp_ps(v1, v2,  0x71));
}

FN_PREALIGN float
_vec3Normalize_sse4(vec3 d, const vec3 v3)
{
   __m128 v = _mm_set_ps(v3[0], v3[1], v3[2], 0);
   __m128 inverse_norm = _mm_rsqrt_ps(_mm_dp_ps(v, v, 0x77));
   __m128 norm =_mm_mul_ps(v, inverse_norm);
   vec4_t r;

   _mm_store_ps(r, norm);
   _aax_memcpy(d, r, 3*sizeof(float));
 
   return _vec3Magnitude_sse4(v3);
}

#endif /* SSE4 */

