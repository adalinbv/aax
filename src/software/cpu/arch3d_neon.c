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

#if defined __ARM_NEON
#include "arch3d_simd.h"

# define vandq_f32(a,b) vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)))

static inline float32x4_t
load_vec3f(const vec3f_ptr v)
{
   static const uint32_t m3a32[] = { 0xffffffff,0xffffffff,0xffffffff,0 };
   return vandq_f32(v->s4, vld1q_f32((const float*)m3a32));
}

static inline float
hsum_float32x4_neon(float32x4_t v) {
#if defined __aarch64__
   return vaddvq_f32(v);
#else
   float32x2_t r = vadd_f32(vget_high_f32(v), vget_low_f32(v));
   return vget_lane_f32(vpadd_f32(r, r), 0);
#endif
}

static inline void
_vec3fNegate_neon(vec3f_ptr d, const vec3f_ptr v) {
   d->s4 = vnegq_f32(v->s4);
}

static inline void
_vec3fAdd_neon(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2) {
   d->s4 = vaddq_f32(v1->s4, v2->s4);
}

static inline void
_vec3fSub_neon(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2) {
   d->s4 = vsubq_f32(v1->s4, v2->s4);
}

static inline void
_vec3fScalarMul_neon(vec3f_ptr d, const vec3f_ptr r, float f) {
   d->s4 = vmulq_f32(r->s4, vmovq_n_f32(f));
}

float
_vec3fMagnitudeSquared_neon(const vec3f_ptr v)
{
   float32x4_t r = load_vec3f(v);
   return hsum_float32x4_neon(vmulq_f32(r, r));
}

float
_vec3fMagnitude_neon(const vec3f_ptr v3)
{
   float32x4_t v = load_vec3f(v3);
   return sqrtf(hsum_float32x4_neon(vmulq_f32(v, v)));
}

static inline  float
_vec3fNormalize_neon(vec3f_ptr d, const vec3f_ptr v) {
   float mag = vec3fMagnitude(v);
   if (mag) {
      d->s4 = vmulq_f32(v->s4, vdupq_n_f32(1.0/mag));
   } else {
      d->s4 = vdupq_n_f32(0.0);
   }
   return mag;
}

void
_vec3fAbsolute_neon(vec3f_ptr d, const vec3f_ptr v) {
   d->s4 = vabsq_f32(v->s4);
}

float
_vec3fDotProduct_neon(const vec3f_ptr v1, const vec3f_ptr v2)
{
   return hsum_float32x4_neon(vmulq_f32(load_vec3f(v1), load_vec3f(v2)));
}

void
_vec3fCrossProduct_neon(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2)
{
   // from 0123 to 0213
   float32x2x2_t v1lh_2013 = vzip_f32(vget_low_f32(v1->s4), vget_high_f32(v1->s4));
   float32x2x2_t v2lh_2013 = vzip_f32(vget_low_f32(v2->s4), vget_high_f32(v2->s4));

   // from 0213 to 2013
   v1lh_2013.val[0] = vrev64_f32(v1lh_2013.val[0]);
   v2lh_2013.val[0] = vrev64_f32(v2lh_2013.val[0]);

   float32x4_t v1_2013 = vcombine_f32(v1lh_2013.val[0], v1lh_2013.val[1]);
   float32x4_t v2_2013 = vcombine_f32(v2lh_2013.val[0], v2lh_2013.val[1]);

   // from 2013 to 2103
   float32x2x2_t v1lh_1203 = vzip_f32(v1lh_2013.val[0], v1lh_2013.val[1]);
   float32x2x2_t v2lh_1203 = vzip_f32(v2lh_2013.val[0], v2lh_2013.val[1]);

   // from 2103 to 1203
   v1lh_1203.val[0] = vrev64_f32(v1lh_1203.val[0]);
   v2lh_1203.val[0] = vrev64_f32(v2lh_1203.val[0]);

   float32x4_t v1_1203 = vcombine_f32(v1lh_1203.val[0], v1lh_1203.val[1]);
   float32x4_t v2_1203 = vcombine_f32(v2lh_1203.val[0], v2lh_1203.val[1]);

   // calculate the cross product
   d->s4 = vsubq_f32(vmulq_f32(v1_1203,v2_2013),vmulq_f32(v1_2013,v2_1203));
}

void
_vec4fCopy_neon(vec4f_ptr d, const vec4f_ptr v)
{
   d->s4 = v->s4;
}

void
_vec4fMulVec4_neon(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2)
{
   r->s4 = vmulq_f32(v1->s4, v2->s4);
}

void
_mtx4fMul_neon(mtx4f_ptr d, const mtx4f_ptr m1, const mtx4f_ptr m2)
{
   int i;
   for (i=0; i<4; ++i) {
      float32x4_t row = vmulq_f32(m1->s4x4[0], vdupq_n_f32(m2->m4[i][0]));
      for (int j=1; j<4; ++j) {
         float32x4_t col = vdupq_n_f32(m2->m4[i][j]);
         row = vaddq_f32(row, vmulq_f32(m1->s4x4[j], col));
      }
      d->s4x4[i] = row;
   }
}

void
_mtx4fMulVec4_neon(vec4f_ptr d, const mtx4f_ptr m, const vec4f_ptr v)
{
   int i;

#if 0
   d->s4 = vml (m->s4x4[0], v->v4);
   d->s4 = vmla (d->s4, m->s4x4[1], v->v4);
   d->s4 = vmla (d->s4, m->s4x4[2], v->v4);
   d->s4 = vmla (d->s4, m->s4x4[3], v->v4);
#else
   d->s4 = vmulq_f32(m->s4x4[0], vdupq_n_f32(v->v4[0]));
   for (i=1; i<4; ++i) {
      float32x4_t row = vmulq_f32(m->s4x4[i], vdupq_n_f32(v->v4[i]));
      d->s4 = vaddq_f32(d->s4, row);
   }
#endif
}

int
_vec3fAltitudeVector_neon(vec3f_ptr altvec, const mtx4f_ptr ifmtx, const vec3f_ptr ppos, const vec3f_ptr epos, const vec3f_ptr afevec, vec3f_ptr fpvec)
{
   vec4f_t pevec, fevec;
   vec3f_t npevec, fpevec;
   float mag_pe, dot_fpe;
   int ahead;

   fevec.s4 = load_vec3f(epos);
   if (!ppos) {
      _vec3fNegate_neon(&pevec.v3, &fevec.v3);
   }
   else
   {
      pevec.s4 = load_vec3f(ppos);
      _vec3fSub_neon(&pevec.v3, &pevec.v3, &fevec.v3);
   }
   _mtx4fMulVec4_neon(&pevec, ifmtx, &pevec);

   fevec.v4[3] = 1.0;
   _mtx4fMulVec4_neon(&fevec, ifmtx, &fevec);

   mag_pe = _vec3fNormalize_neon(&npevec, &pevec.v3);
   dot_fpe = _vec3fDotProduct_neon(&fevec.v3, &npevec);

   _vec3fScalarMul_neon(&fpevec, &npevec, dot_fpe);

   _vec3fSub_neon(&fpevec, &fevec.v3, &fpevec);
   _vec3fAdd_neon(&npevec, &fevec.v3, &pevec.v3);

   _vec3fAbsolute_neon(afevec, &fevec.v3);
   _vec3fAbsolute_neon(altvec, &fpevec);
   _vec3fAbsolute_neon(fpvec, &npevec);

   ahead = (dot_fpe >= 0.0f || (mag_pe+dot_fpe) <= FLT_EPSILON);

   return ahead;
}

#else
typedef int make_iso_compilers_happy;
#endif /* NEON */

