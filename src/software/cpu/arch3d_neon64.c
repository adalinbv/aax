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

#if defined __aarch64__
#include "arch3d_simd.h"

# define vandq_f64(a,b) vreinterpretq_f64_u64(vandq_u64(vreinterpretq_u64_f64(a), vreinterpretq_u64_f64(b)))

static inline float64x2_t
load_vec3d0(const vec3d_ptr v)
{
   return v->s4.f64x2[0];
}

static inline float64x2_t
load_vec3d1(const vec3d_ptr v)
{
   static ALIGN32 const uint64_t m3a64[] ALIGN16C = {
        0xffffffffffffffff,0
   };
   return vandq_f64(v->s4.f64x2[1], vld1q_f64((const double*)m3a64));
}

static inline float
hsum_float64x4_neon64(float64x2_t v[2]) {

    return vaddvq_f64(v[0]) + vaddvq_f64(v[1]);
}

static inline void
_vec3dNegate_neon64(vec3d_ptr d, const vec3d_ptr v) {
   d->s4.f64x2[0] = vnegq_f64(v->s4.f64x2[0]);
   d->s4.f64x2[1] = vnegq_f64(v->s4.f64x2[1]);
}

static inline void
_vec3dAdd_neon64(vec3d_ptr d, const vec3d_ptr v1, const vec3d_ptr v2) {
   d->s4.f64x2[0] = vaddq_f64(v1->s4.f64x2[0], v2->s4.f64x2[0]);
   d->s4.f64x2[1] = vaddq_f64(v1->s4.f64x2[1], v2->s4.f64x2[1]);
}

static inline void
_vec3dSub_neon64(vec3d_ptr d, const vec3d_ptr v1, const vec3d_ptr v2) {
   d->s4.f64x2[0] = vsubq_f64(v1->s4.f64x2[0], v2->s4.f64x2[0]);
   d->s4.f64x2[1] = vsubq_f64(v1->s4.f64x2[1], v2->s4.f64x2[1]);
}

static inline void
_vec3dAbsolute_neon64(vec3d_ptr d, const vec3d_ptr v) {
   d->s4.f64x2[0] = vabsq_f64(v->s4.f64x2[0]);
   d->s4.f64x2[1] = vabsq_f64(v->s4.f64x2[1]);
}

static inline void
_vec3dScalarMul_neon64(vec3d_ptr d, const vec3d_ptr r, float f) {
   d->s4.f64x2[0] = vmulq_f64(r->s4.f64x2[0], vmovq_n_f64(f));
   d->s4.f64x2[1] = vmulq_f64(r->s4.f64x2[1], vmovq_n_f64(f));
}

float
_vec3dMagnitudeSquared_neon64(const vec3d_ptr v)
{
   float64x2_t v3[2], mv[2];

   v3[0] = load_vec3d0(v);
   v3[1] = load_vec3d1(v);
   mv[0] = vmulq_f64(v3[0], v3[0]);
   mv[1] = vmulq_f64(v3[1], v3[1]);
   return hsum_float64x4_neon64(mv);
}

float
_vec3dMagnitude_neon64(const vec3d_ptr v) {
   return sqrtf(_vec3dMagnitudeSquared_neon64(v));
}

double
_vec3dNormalize_neon64(vec3d_ptr d, const vec3d_ptr v)
{
   double mag = _vec3dMagnitude_neon64(v);
   if (mag) {
      float64x2_t div = vdupq_n_f64(1.0/mag);
      d->s4.f64x2[0] = vmulq_f64(v->s4.f64x2[0], div);
      d->s4.f64x2[1] = vmulq_f64(v->s4.f64x2[1], div);
   } else {
      d->s4.f64x2[0] = d->s4.f64x2[1] = vdupq_n_f64(0.0);
   }
   return mag;
}

float
_vec3dDotProduct_neon64(const vec3d_ptr v1, const vec3d_ptr v2)
{
   float64x2_t mv[2];
   mv[0] = vmulq_f64(load_vec3d0(v1), load_vec3d0(v2));
   mv[1] = vmulq_f64(load_vec3d1(v1), load_vec3d1(v2));
   return hsum_float64x4_neon64(mv);
}

#if 0
void
_vec4dCopy_neon64(vec4d_ptr d, const vec4d_ptr v) {
   d->s4.f64x4 = v->s4.f64x4;
}
#endif

void
_mtx4dMul_neon64(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2)
{
   int i;

   for (i=0; i<4; ++i) {
      float64x2_t col = vdupq_n_f64(m2->m4[i][0]);
      float64x2_t row1 = vmulq_f64(m1->s4x4[0].f64x2[0], col);
      float64x2_t row2 = vmulq_f64(m1->s4x4[0].f64x2[1], col);
      for (int j=1; j<4; ++j) {
          col = vdupq_n_f64(m2->m4[i][j]);
          row1 = vaddq_f64(row1, vmulq_f64(m1->s4x4[j].f64x2[0], col));
          row2 = vaddq_f64(row2, vmulq_f64(m1->s4x4[j].f64x2[1], col));
      }
      d->s4x4[i].f64x2[0] = row1;
      d->s4x4[i].f64x2[1] = row2;
   }
}

void
_mtx4dMulVec4_neon64(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr vi)
{
   float64x2_t val;
   vec4d_t v;
   int i;

   vec4dCopy(&v, vi);
   val = vdupq_n_f64(v.v4[0]);
   d->s4.f64x2[0] = vmulq_f64(m->s4x4[0].f64x2[0], val);
   d->s4.f64x2[1] = vmulq_f64(m->s4x4[0].f64x2[1], val);
   for (i=1; i<4; ++i) {
      float64x2_t row[2];
      val = vdupq_n_f64(v.v4[i]);
      row[0] = vmulq_f64(m->s4x4[i].f64x2[0], val);
      row[1] = vmulq_f64(m->s4x4[i].f64x2[1], val);
      d->s4.f64x2[0] = vaddq_f64(d->s4.f64x2[0], row[0]);
      d->s4.f64x2[1] = vaddq_f64(d->s4.f64x2[1], row[1]);
   }


}

int
_vec3dAltitudeVector_neon64(vec3f_ptr altvec, const mtx4d_ptr ifmtx, const vec3d_ptr ppos, const vec3d_ptr epos, const vec3f_ptr afevec, vec3f_ptr fpvec)
{
   vec4d_t pevec, fevec;
   vec3d_t npevec, fpevec;
   double mag_pe, dot_fpe;
   int ahead;

   fevec.s4.f64x2[0] = load_vec3d0(epos);
   fevec.s4.f64x2[1] = load_vec3d1(epos);
   if (!ppos) {
      _vec3dNegate_neon64(&pevec.v3, &fevec.v3);
   }
   else
   {
      pevec.s4.f64x2[0] = load_vec3d0(ppos);
      pevec.s4.f64x2[1] = load_vec3d1(ppos);
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

   _vec3dAbsolute_neon64(&fpevec, &fpevec);
   _vec3dAbsolute_neon64(&npevec, &npevec);
   _vec3dAbsolute_neon64(&fevec.v3, &fevec.v3);

   vec3fFilld(afevec->v3, fevec.v4);
   vec3fFilld(altvec->v3, fpevec.v3);
   vec3fFilld(fpvec->v3, npevec.v3);

   ahead = (dot_fpe >= 0.0f || (mag_pe+dot_fpe) <= FLT_EPSILON);

   return ahead;
}

#else
typedef int make_iso_compilers_happy;
#endif /* NEON */

