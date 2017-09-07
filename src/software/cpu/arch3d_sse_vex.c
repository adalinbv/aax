/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <api.h>

#include "arch3d_simd.h"

#ifdef __AVX__

static inline FN_PREALIGN __m128
load_vec3f(const vec3f_ptr v)
{
   static ALIGN16 const uint32_t m3a32[] ALIGN16C = {
        0xffffffff,0xffffffff,0xffffffff,0
   };
   return _mm_and_ps(v->s4, _mm_load_ps((const float*)m3a32));
}

static inline FN_PREALIGN float
hsum_ps_sse_vex(__m128 v) {
   __m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
   __m128 sums = _mm_add_ps(v, shuf);
   shuf = _mm_movehl_ps(shuf, sums);
   sums = _mm_add_ss(sums, shuf);
   return _mm_cvtss_f32(sums);
}

FN_PREALIGN float
_vec3fMagnitudeSquared_sse_vex(const vec3f_ptr v3)
{
   __m128 v = load_vec3f(v3);
   return hsum_ps_sse_vex(_mm_mul_ps(v, v));
}

FN_PREALIGN float
_vec3fMagnitude_sse_vex(const vec3f_ptr v3)
{  
   __m128 v = load_vec3f(v3);
   return sqrtf(hsum_ps_sse_vex(_mm_mul_ps(v, v)));
}

FN_PREALIGN float
_vec3fDotProduct_sse_vex(const vec3f_ptr v1, const vec3f_ptr v2)
{
   return hsum_ps_sse_vex(_mm_mul_ps(load_vec3f(v1), load_vec3f(v2)));
}

// http://threadlocalmutex.com/?p=8
FN_PREALIGN void 
_vec3fCrossProduct_sse_vex(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2)
{
   __m128 xmm1 = load_vec3f(v1);
   __m128 xmm2 = load_vec3f(v2);
   __m128 a = _mm_shuffle_ps(xmm1, xmm1, _MM_SHUFFLE(3, 0, 2, 1));
   __m128 b = _mm_shuffle_ps(xmm2, xmm2, _MM_SHUFFLE(3, 0, 2, 1));
   __m128 c = _mm_sub_ps(_mm_mul_ps(xmm1, b), _mm_mul_ps(a, xmm2));
   d->s4 = _mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 0, 2, 1));
}


FN_PREALIGN void
_vec4fCopy_sse_vex(vec4f_ptr d, const vec4f_ptr v)
{
   d->s4 = v->s4;
}


FN_PREALIGN void
_vec4fMulvec4_sse_vex(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2)
{
   r->s4 = _mm_mul_ps(v1->s4, v2->s4);
}

FN_PREALIGN void
_vec4fMatrix4_sse_vex(vec4f_ptr d, const vec4f_ptr vi, const mtx4f_ptr m)
{
   int i;

   d->s4 = _mm_mul_ps(m->s4x4[0], _mm_set1_ps(vi->v4[0]));
   for (i=1; i<3; ++i) {
      __m128 row = _mm_mul_ps(m->s4x4[i], _mm_set1_ps(vi->v4[i]));
      d->s4 = _mm_add_ps(d->s4, row);
   }
}

FN_PREALIGN void
_pt4fMatrix4_sse_vex(vec4f_ptr d, const vec4f_ptr vi, const mtx4f_ptr m)
{
   int i;

   d->s4 = _mm_mul_ps(m->s4x4[0], _mm_set1_ps(vi->v4[0]));
   for (i=1; i<3; ++i) {
      __m128 row = _mm_mul_ps(m->s4x4[i], _mm_set1_ps(vi->v4[i]));
      d->s4 = _mm_add_ps(d->s4, row);
   }
   d->s4 = _mm_add_ps(d->s4, m->s4x4[3]);
}

FN_PREALIGN void
_mtx4fMul_sse_vex(mtx4f_ptr d, const mtx4f_ptr m1, const mtx4f_ptr m2)
{
   int i;

   for (i=0; i<4; ++i) {
      __m128 row = _mm_mul_ps(m1->s4x4[0], _mm_set1_ps(m2->m4[i][0]));
      for (int j=1; j<4; ++j) {
          __m128 col = _mm_set1_ps(m2->m4[i][j]);
          row = _mm_add_ps(row, _mm_mul_ps(m1->s4x4[j], col));
      }
      d->s4x4[i] = row;
   }
}

FN_PREALIGN void
_mtx4dMul_sse_vex(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2)
{
   int i;

   for (i=0; i<4; ++i) {
      __m128d col = _mm_set1_pd(m2->m4[i][0]);
      __m128d row1 = _mm_mul_pd(m1->s4x4[0][0], col);
      __m128d row2 = _mm_mul_pd(m1->s4x4[0][1], col);
      for (int j=1; j<4; ++j) {
          col = _mm_set1_pd(m2->m4[i][j]);
          row1 = _mm_add_pd(row1, _mm_mul_pd(m1->s4x4[j][0], col));
          row2 = _mm_add_pd(row2, _mm_mul_pd(m1->s4x4[j][1], col));
      }
      d->s4x4[i][0] = row1;
      d->s4x4[i][1] = row2;
   }
}

#else
typedef int make_iso_compilers_happy;
#endif /* AVX */

