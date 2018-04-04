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

#ifdef __SSE2__

static inline FN_PREALIGN __m128d
load_vec3d0(const vec3d_ptr v)
{
   return v->s4.sse[0];
}

static inline FN_PREALIGN __m128d
load_vec3d1(const vec3d_ptr v)
{
   static ALIGN32 const uint64_t m3a64[] ALIGN16C = {
        0xffffffffffffffff,0
   };
   return _mm_and_pd(v->s4.sse[1], _mm_load_pd((const double*)m3a64));
}

static double
hsum_pd_sse(const __m128d vd[2]) {
    __m128 undef    = _mm_setzero_ps();
    __m128 shuftmp1 = _mm_movehl_ps(undef, _mm_castpd_ps(vd[0]));
    __m128 shuftmp2 = _mm_movehl_ps(undef, _mm_castpd_ps(vd[1]));
    __m128d shuf1   = _mm_castps_pd(shuftmp1);
    __m128d shuf2   = _mm_castps_pd(shuftmp2);
    return  _mm_cvtsd_f64(_mm_add_sd(vd[0], shuf1)) +
            _mm_cvtsd_f64(_mm_add_sd(vd[1], shuf2));
}

FN_PREALIGN void
_vec3dCopy_sse2(vec3d_ptr d, const vec3d_ptr v) {
   d->s4.sse[0] = v->s4.sse[0];
   d->s4.sse[1] = v->s4.sse[1];
}

FN_PREALIGN void
_vec3dNegate_sse2(vec3d_ptr d, const vec3d_ptr v) {
   d->s4.sse[0] = _mm_xor_pd(v->s4.sse[0], _mm_set1_pd(-0.0));
   d->s4.sse[1] = _mm_xor_pd(v->s4.sse[1], _mm_set1_pd(-0.0));
}

FN_PREALIGN void
_vec3dAdd_sse2(vec3d_ptr d, const vec3d_ptr v1, const vec3d_ptr v2) {
   d->s4.sse[0] = _mm_add_pd(v1->s4.sse[0], v2->s4.sse[0]);
   d->s4.sse[1] = _mm_add_pd(v1->s4.sse[1], v2->s4.sse[1]);
}

FN_PREALIGN void
_vec3dSub_sse2(vec3d_ptr d, const vec3d_ptr v1, const vec3d_ptr v2) {
   d->s4.sse[0] = _mm_sub_pd(v1->s4.sse[0], v2->s4.sse[0]);
   d->s4.sse[1] = _mm_sub_pd(v1->s4.sse[1], v2->s4.sse[1]);
}

FN_PREALIGN void
_vec3dScalarMul_sse2(vec3d_ptr d, const vec3d_ptr r, float f) {
   d->s4.sse[0] = _mm_mul_pd(r->s4.sse[0], _mm_set1_pd(f));
   d->s4.sse[1] = _mm_mul_pd(r->s4.sse[1], _mm_set1_pd(f));
}

FN_PREALIGN float
_vec3dDotProduct_sse2(const vec3d_ptr v1, const vec3d_ptr v2)
{
   __m128d mv[2];
   mv[0] = _mm_mul_pd(load_vec3d0(v1), load_vec3d0(v2));
   mv[1] = _mm_mul_pd(load_vec3d1(v1), load_vec3d1(v2));
   return hsum_pd_sse(mv);
}

FN_PREALIGN void
_mtx4dMul_sse2(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2)
{
   int i;

   for (i=0; i<4; ++i) {
      __m128d col = _mm_set1_pd(m2->m4[i][0]);
      __m128d row1 = _mm_mul_pd(m1->s4x4[0].sse[0], col);
      __m128d row2 = _mm_mul_pd(m1->s4x4[0].sse[1], col);
      for (int j=1; j<4; ++j) {
          col = _mm_set1_pd(m2->m4[i][j]);
          row1 = _mm_add_pd(row1, _mm_mul_pd(m1->s4x4[j].sse[0], col));
          row2 = _mm_add_pd(row2, _mm_mul_pd(m1->s4x4[j].sse[1], col));
      }
      d->s4x4[i].sse[0] = row1;
      d->s4x4[i].sse[1] = row2;
   }
}

FN_PREALIGN void
_mtx4dMulVec4_sse2(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr vi)
{
   __m128d val;
   vec4d_t v;
   int i;
   
   vec4dCopy(&v, vi);
   val = _mm_set1_pd(v.v4[0]);
   d->s4.sse[0] = _mm_mul_pd(m->s4x4[0].sse[0], val);
   d->s4.sse[1] = _mm_mul_pd(m->s4x4[0].sse[1], val);
   for (i=1; i<4; ++i) {
      __m128d row[2];
      val = _mm_set1_pd(v.v4[i]);
      row[0] = _mm_mul_pd(m->s4x4[i].sse[0], val);
      row[1] = _mm_mul_pd(m->s4x4[i].sse[1], val);
      d->s4.sse[0] = _mm_add_pd(d->s4.sse[0], row[0]);
      d->s4.sse[1] = _mm_add_pd(d->s4.sse[1], row[1]);
   }
}

FN_PREALIGN int
_vec3dAltitudeVector_sse2(vec3f_ptr altvec, const mtx4d_ptr ifmtx, const vec3d_ptr ppos, const vec3d_ptr epos, const vec3f_ptr afevec, vec3f_ptr fpvec)
{
   vec4d_t pevec, fevec;
   vec3d_t npevec, fpevec;
   double mag_pe, dot_fpe;
   int ahead;

   if (!ppos) {
      _vec3dNegate_sse2(&pevec.v3, epos);
   } else {
      _vec3dSub_sse2(&pevec.v3, ppos, epos);
   }
   pevec.v4[3] = 0.0;
   _mtx4dMulVec4_sse2(&pevec, ifmtx, &pevec);

   _vec3dCopy_sse2(&fevec.v3, epos);
   fevec.v4[3] = 1.0;
   _mtx4dMulVec4_sse2(&fevec, ifmtx, &fevec);

   mag_pe = _vec3dNormalize_cpu(&npevec, &pevec.v3);
   dot_fpe = _vec3dDotProduct_sse2(&fevec.v3, &npevec);

   vec3fFilld(afevec->v3, fevec.v4);
   _vec3fAbsolute_sse(afevec, afevec);

   _vec3dScalarMul_sse2(&fpevec, &npevec, dot_fpe);

   _vec3dSub_sse2(&fpevec, &fevec.v3, &fpevec);
   vec3fFilld(altvec->v3, fpevec.v3);
   _vec3fAbsolute_sse(altvec, altvec);

   _vec3dAdd_sse2(&npevec, &fevec.v3, &pevec.v3);
   vec3fFilld(fpvec->v3, npevec.v3);

   ahead = (dot_fpe >= 0.0f || (mag_pe+dot_fpe) <= FLT_EPSILON);

   return ahead;
}

#else
typedef int make_iso_compilers_happy;
#endif /* SSE2 */

