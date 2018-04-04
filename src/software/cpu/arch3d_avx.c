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

#ifdef __AVX__

static inline FN_PREALIGN __m256d
load_vec3d(const vec3d_ptr v)
{
   static ALIGN32 const uint64_t m3a64[] ALIGN32C = {
        0xffffffffffffffff,0xffffffffffffffff,0xffffffffffffffff,0
   };
   return _mm256_and_pd(v->s4.avx, _mm256_load_pd((const double*)m3a64));
}

// http://berenger.eu/blog/sseavxsimd-horizontal-sum-sum-simd-vector-intrinsic/
static inline FN_PREALIGN double
hsum_pd_avx(__m256d v) {
    const __m128d valupper = _mm256_extractf128_pd(v, 1);
    const __m128d vallower = _mm256_castpd256_pd128(v);
    _mm256_zeroupper();
    const __m128d valval = _mm_add_pd(valupper, vallower);
    const __m128d sums = _mm_add_pd(_mm_permute_pd(valval,1), valval);
    return _mm_cvtsd_f64(sums);
}

FN_PREALIGN void
_vec3dCopy_avx(vec3d_ptr d, const vec3d_ptr v) {
   d->s4.avx = v->s4.avx;
}

FN_PREALIGN void
_vec3dNegate_avx(vec3d_ptr d, const vec3d_ptr v) {
   d->s4.avx = _mm256_xor_pd(v->s4.avx, _mm256_set1_pd(-0.0));
}

FN_PREALIGN void
_vec3dAdd_avx(vec3d_ptr d, const vec3d_ptr v1, const vec3d_ptr v2) {
   d->s4.avx = _mm256_add_pd(v1->s4.avx, v2->s4.avx);
}

FN_PREALIGN void
_vec3dSub_avx(vec3d_ptr d, const vec3d_ptr v1, const vec3d_ptr v2) {
   d->s4.avx = _mm256_sub_pd(v1->s4.avx, v2->s4.avx);
}

FN_PREALIGN void
_vec3dScalarMul_avx(vec3d_ptr d, const vec3d_ptr r, float f) {
   d->s4.avx = _mm256_mul_pd(r->s4.avx, _mm256_set1_pd(f));
}

FN_PREALIGN float
_vec3dDotProduct_avx(const vec3d_ptr v1, const vec3d_ptr v2)
{
   return hsum_pd_avx(_mm256_mul_pd(load_vec3d(v1), load_vec3d(v2)));
}


FN_PREALIGN void
_mtx4dMul_avx(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2)
{
   __m256d row, col;
   int i, j;

   for (i=0; i<4; ++i) {
      row = _mm256_mul_pd(m1->s4x4[0].avx, _mm256_set1_pd(m2->m4[i][0]));
      for (j=1; j<4; ++j) {
          col = _mm256_set1_pd(m2->m4[i][j]);
          row = _mm256_add_pd(row, _mm256_mul_pd(m1->s4x4[j].avx, col));
      }
      d->s4x4[i].avx = row;
   }
}

FN_PREALIGN void
_mtx4dMulVec4_avx(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr vi)
{
   vec4d_t v;
   int i;

   vec4dCopy(&v, vi);
   d->s4.avx = _mm256_mul_pd(m->s4x4[0].avx, _mm256_set1_pd(v.v4[0]));
   for (i=1; i<4; ++i) {
      __m256d row = _mm256_mul_pd(m->s4x4[i].avx, _mm256_set1_pd(v.v4[i]));
      d->s4.avx = _mm256_add_pd(d->s4.avx, row);
   }
}

FN_PREALIGN int
_vec3dAltitudeVector_avx(vec3f_ptr altvec, const mtx4d_ptr ifmtx, const vec3d_ptr ppos, const vec3d_ptr epos, const vec3f_ptr afevec, vec3f_ptr fpvec)
{
   vec4d_t pevec, fevec;
   vec3d_t npevec, fpevec;
   double mag_pe, dot_fpe;
   int ahead;

   if (!ppos) {
      _vec3dNegate_avx(&pevec.v3, epos);
   } else {
      _vec3dSub_avx(&pevec.v3, ppos, epos);
   }
   pevec.v4[3] = 0.0;
   _mtx4dMulVec4_avx(&pevec, ifmtx, &pevec);

   _vec3dCopy_avx(&fevec.v3, epos);
   fevec.v4[3] = 1.0;
   _mtx4dMulVec4_avx(&fevec, ifmtx, &fevec);

   mag_pe = _vec3dNormalize_cpu(&npevec, &pevec.v3);
   dot_fpe = _vec3dDotProduct_avx(&fevec.v3, &npevec);
   _mm256_zeroupper();

   vec3fFilld(afevec->v3, fevec.v4);
   _vec3fAbsolute_sse_vex(afevec, afevec);

   _vec3dScalarMul_avx(&fpevec, &npevec, dot_fpe);

   _vec3dSub_avx(&fpevec, &fevec.v3, &fpevec);
   vec3fFilld(altvec->v3, fpevec.v3);
   _vec3fAbsolute_sse_vex(altvec, altvec);

   _vec3dAdd_avx(&npevec, &fevec.v3, &pevec.v3);
   vec3fFilld(fpvec->v3, npevec.v3);

   ahead = (dot_fpe >= 0.0f || (mag_pe+dot_fpe) <= FLT_EPSILON);

   return ahead;
}

#else
typedef int make_iso_compilers_happy;
#endif /* AVX */

