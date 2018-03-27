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

#else
typedef int make_iso_compilers_happy;
#endif /* SSE3 */

