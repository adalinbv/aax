/*
 * Copyright 2005-2018 by Erik Hofman.
 * Copyright 2009-2018 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
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

#if defined __aarch64__
#include "arch3d_simd.h"

# define vandq_f64(a,b) vreinterpretq_f64_u64(vandq_u64(vreinterpretq_u64_f64(a), vreinterpretq_u64_f64(b)))

static inline float64x4_t
load_vec3d(const vec3d_ptr v)
{
   static const uint64_t m3a64[] = { 
        0xffffffffffffffff,0xffffffffffffffff,0xffffffffffffffff,0
   };
   return vandq_f64(v->s4, vld1q_f64((const float*)m3a64));
}

static inline float
hsum_float64x4_neon64(float64x4_t v) {
    float64x2_t r = vadd_f64(vget_high_f64(v), vget_low_f64(v));
    return vget_lane_f64(vpadd_f64(r, r), 0);
}

static inline void
_vec3dNegate_neon(vec3d_ptr d, const vec3d_ptr v) {
   d->s4 = vneg_f64(v->s4);
}

static inline void
_vec3dAdd_neon(vec3d_ptr d, const vec3d_ptr v1, const vec3d_ptr v2) {
   d->s4 = vadd_f64(v1->s4, v2->s4);
}

static inline void
_vec3dSub_neon(vec3d_ptr d, const vec3d_ptr v1, const vec3d_ptr v2) {
   d->s4 = vsub_f64(v1->s4, v2->s4);
}

static inline void
_vec3dScalarMul_neon(vec3d_ptr d, const vec3d_ptr r, float f) {
   d->s4 = vmul_f64(r->s4, vmovq_n_f64(f));
}

float
_vec3dMagnitudeSquared_neon64(const vec3d_ptr v)
{
   float64x4_t r = load_vec3d(v);
   return hsum_float64x4_neon64(vmulq_f64(r, r));
}

float
_vec3dMagnitude_neon64(const vec3d_ptr v3)
{
   float64x4_t v = load_vec3d(v3);
   return sqrtf(hsum_float64x4_neon64(vmulq_f64(v, v)));
}

float
_vec3dDotProduct_neon64(const vec3d_ptr v1, const vec3d_ptr v2)
{
   return hsum_float32x4_neon64(vmulq_f64(load_vec3d(v1), load_vec3d(v2)));
}

#if 0
void
_vec4dCopy_neon64(vec4d_ptr d, const vec4d_ptr v) {
   d->s4 = v->s4;
}
#endif

void
_mtx4dMul_neon64(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2)
{
   int i;
   for (i=0; i<4; ++i) {
      float64x4_t row = vmulq_f64(m1->s4x4[0], vdupq_n_f64(m2->m4[i][0]));
      for (int j=1; j<4; ++j) {
         float64x4_t col = vdupq_n_f64(m2->m4[i][j]);
         row = vaddq_f64(row, vmulq_f64(m1->s4x4[j], col));
      }
      d->s4x4[i] = row;
   }
}

void
_mtx4dMulVec4_neon64(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr v)
{
   int i;

#if 0
   d->s4 = vml (m->s4x4[0], v->v4);
   d->s4 = vmla (d->s4, m->s4x4[1], v->v4);
   d->s4 = vmla (d->s4, m->s4x4[2], v->v4);
   d->s4 = vmla (d->s4, m->s4x4[3], v->v4);
#else
   d->s4 = vmulq_f64(m->s4x4[0], vdupq_n_f64(v->v4[0]));
   for (i=1; i<4; ++i) {
      float64x4_t row = vmulq_f64(m->s4x4[i], vdupq_n_f64(v->v4[i]));
      d->s4 = vaddq_f64(d->s4, row);
   }
#endif
}

int
_vec3dAltitudeVector_neon64(vec3f_ptr altvec, const mtx4d_ptr ifmtx, const vec3d_ptr ppos, const vec3d_ptr epos, const vec3f_ptr afevec, vec3f_ptr fpvec)
{
   vec4d_t pevec, fevec;
   vec3d_t npevec, fpevec;
   double mag_pe, dot_fpe;
   int ahead;

   fevec.s4 = load_vec3d(epos);			// sets fevec.v4[3] to 0.0
   if (!ppos) {
      _vec3dNegate_neon64(&pevec.v3, &fevec.v3);
   }
   else
   {
      pevec.s4 = load_vec3d(ppos);
      _vec3dSub_neon64(&pevec.v3, &pevec.v3, &fevec.v3);
   }
   _mtx4dMulVec4_neon64(&pevec, ifmtx, &pevec);

   fevec.v4[3] = 1.0;
   _mtx4dMulVec4_neon64(&fevec, ifmtx, &fevec);

   mag_pe = _vec3dNormalize_neon64(&npevec, &pevec.v3);
   dot_fpe = _vec3dDotProduct_neon64(&fevec.v3, &npevec);

   _vec3dScalarMul_neon64(&fpevec, &npevec, dot_fpe);

   _vec3dSub_neon64(&fpevec, &fevec.v3, &fpevec);
   _vec3dAdd_neon64(&npevec, &fevec.v3, &pevec.v3);
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
#endif /* NEON */

