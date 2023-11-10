/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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
hsum_pd_fma3(__m256d v) {
    const __m128d valupper = _mm256_extractf128_pd(v, 1);
    const __m128d vallower = _mm256_castpd256_pd128(v);
    _mm256_zeroupper();
    const __m128d valval = _mm_add_pd(valupper, vallower);
    const __m128d sums = _mm_add_pd(_mm_permute_pd(valval,1), valval);
    return _mm_cvtsd_f64(sums);
}

#if 0
static inline FN_PREALIGN void
_vec3dCopy_fma3(vec3d_ptr d, const vec3d_ptr v) {
   d->s4.avx = v->s4.avx;
}
#endif

static inline FN_PREALIGN void
_vec3dNegate_fma3(vec3d_ptr d, const vec3d_ptr v) {
   d->s4.avx = _mm256_xor_pd(v->s4.avx, _mm256_set1_pd(-0.0));
}

static inline FN_PREALIGN void
_vec3dAdd_fma3(vec3d_ptr d, const vec3d_ptr v1, const vec3d_ptr v2) {
   d->s4.avx = _mm256_add_pd(v1->s4.avx, v2->s4.avx);
}

static inline FN_PREALIGN void
_vec3dSub_fma3(vec3d_ptr d, const vec3d_ptr v1, const vec3d_ptr v2) {
   d->s4.avx = _mm256_sub_pd(v1->s4.avx, v2->s4.avx);
}

// TODO: For now the SSE2 version is more reliable thane the AVX version
#if 1
static inline FN_PREALIGN void
_vec3dAbsolute_sse2(vec3d_ptr d, const vec3d_ptr v) {
   d->s4.sse[0] = _mm_andnot_pd(_mm_set1_pd(-0.0), v->s4.sse[0]);
   d->s4.sse[1] = _mm_andnot_pd(_mm_set1_pd(-0.0), v->s4.sse[1]);
}

#else
static inline FN_PREALIGN void
_vec3dAbsolute_fma3(vec3d_ptr d, const vec3d_ptr v) {
   d->s4.avx = _mm256_andnot_pd(v->s4.avx, _mm256_set1_pd(-0.0));
}
#endif

static inline FN_PREALIGN void
_vec3dScalarMul_fma3(vec3d_ptr d, const vec3d_ptr r, float f) {
   d->s4.avx = _mm256_mul_pd(r->s4.avx, _mm256_set1_pd(f));
}

static inline FN_PREALIGN double
_vec3dMagnitudeSquared_fma3(const vec3d_ptr v) {
   __m256d v3 = load_vec3d(v);
   return hsum_pd_fma3(_mm256_mul_pd(v3, v3));
}

static inline FN_PREALIGN double
_vec3dMagnitude_fma3(const vec3d_ptr v) {
   return sqrt(_vec3dMagnitudeSquared_fma3(v));
}

double
_vec3dNormalize_fma3(vec3d_ptr d, const vec3d_ptr v)
{
   double mag = _vec3dMagnitude_fma3(v);
   if (mag) {
      d->s4.avx = _mm256_mul_pd(v->s4.avx, _mm256_set1_pd(1.0/mag));
   } else {
      d->s4.avx = _mm256_set1_pd(0.0);
   }
   return mag;
}

FN_PREALIGN float
_vec3dDotProduct_fma3(const vec3d_ptr v1, const vec3d_ptr v2)
{
   return hsum_pd_fma3(_mm256_mul_pd(load_vec3d(v1), load_vec3d(v2)));
}

FN_PREALIGN void
_mtx4fMul_fma3(mtx4f_ptr d, const mtx4f_ptr m1, const mtx4f_ptr m2)
{
   int i;

   for (i=0; i<4; ++i) {
      __m128 col = _mm_set1_ps(m2->m4[i][0]);
      __m128 row = _mm_mul_ps(m1->s4x4[0], col);
      for (int j=1; j<4; ++j) {
          col = _mm_set1_ps(m2->m4[i][j]);
          row = _mm_fmadd_ps(m1->s4x4[j], col, row);
      }
      d->s4x4[i] = row;
   }
}

FN_PREALIGN void
_mtx4fMulVec4_fma3(vec4f_ptr d, const mtx4f_ptr m, const vec4f_ptr vi)
{
   vec4f_t v;
   int i;

   vec4fCopy(&v, vi);
   d->s4 = _mm_mul_ps(m->s4x4[0], _mm_set1_ps(v.v4[0]));
   for (i=1; i<4; ++i) {
      d->s4 = _mm_fmadd_ps(m->s4x4[i], _mm_set1_ps(v.v4[i]), d->s4);
   }
}

FN_PREALIGN void
_mtx4dMul_fma3(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2)
{
   __m256d row, col;
   int i, j;

   for (i=0; i<4; ++i) {
      row = _mm256_mul_pd(m1->s4x4[0].avx, _mm256_set1_pd(m2->m4[i][0]));
      for (j=1; j<4; ++j) {
          col = _mm256_set1_pd(m2->m4[i][j]);
          row = _mm256_fmadd_pd(m1->s4x4[j].avx, col, row);
      }
      d->s4x4[i].avx = row;
   }
}

FN_PREALIGN void
_mtx4dMulVec4_fma3(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr vi)
{
   vec4d_t v;
   int i;

   vec4dCopy(&v, vi);
   d->s4.avx = _mm256_mul_pd(m->s4x4[0].avx, _mm256_set1_pd(v.v4[0]));
   for (i=1; i<4; ++i) {
      d->s4.avx = _mm256_fmadd_pd(m->s4x4[i].avx, _mm256_set1_pd(v.v4[i]), d->s4.avx);
   }
}

FN_PREALIGN int
_vec3dAltitudeVector_fma3(vec3f_ptr altvec, const mtx4d_ptr ifmtx, const vec3d_ptr ppos, const vec3d_ptr epos, const vec3f_ptr afevec, vec3f_ptr fpvec)
{
   vec4d_t pevec, fevec;
   vec3d_t npevec, fpevec;
   double mag_pe, dot_fpe;
   int ahead;

   fevec.s4.avx = load_vec3d(epos);		// sets fevec.v4[3] to 0.0
   if (!ppos) {
      _vec3dNegate_fma3(&pevec.v3, &fevec.v3);
   }
   else
   {
      pevec.s4.avx = load_vec3d(ppos);
      _vec3dSub_fma3(&pevec.v3, &pevec.v3, &fevec.v3);
   }
   _mtx4dMulVec4_fma3(&pevec, ifmtx, &pevec);

   fevec.v4[3] = 1.0;
   _mtx4dMulVec4_fma3(&fevec, ifmtx, &fevec);

   mag_pe = _vec3dNormalize_fma3(&npevec, &pevec.v3);
   dot_fpe = _vec3dDotProduct_fma3(&fevec.v3, &npevec);

   _vec3dScalarMul_fma3(&fpevec, &npevec, dot_fpe);

   _vec3dSub_fma3(&fpevec, &fevec.v3, &fpevec);
   _vec3dAdd_fma3(&npevec, &fevec.v3, &pevec.v3);
   _mm256_zeroupper();

   _vec3dAbsolute_sse2(&fpevec, &fpevec);
   _vec3dAbsolute_sse2(&npevec, &npevec);
   _vec3dAbsolute_sse2(&fevec.v3, &fevec.v3);

   vec3fFilld(afevec->v3, fevec.v4);
   vec3fFilld(altvec->v3, fpevec.v3);
   vec3fFilld(fpvec->v3, npevec.v3);

   ahead = (dot_fpe >= 0.0f || (mag_pe+dot_fpe) <= FLT_EPSILON);

   return ahead;
}

#else
typedef int make_iso_compilers_happy;
#endif /* AVX */

