/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

#ifdef __ARM_NEON__
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
    float32x2_t r = vadd_f32(vget_high_f32(v), vget_low_f32(v));
    return vget_lane_f32(vpadd_f32(r, r), 0);
}

float
_vec3fMagnitudeSquared_neon(const vec3f_ptr v3)
{
   float32x4_t v = load_vec3f(v3);
   return hsum_float32x4_neon(vmulq_f32(v, v));
}

float
_vec3fMagnitude_neon(const vec3f_ptr v3)
{
   float32x4_t v = load_vec3f(v3);
   return sqrtf(hsum_float32x4_neon(vmulq_f32(v, v)));
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

   d->s4 = vmulq_f32(m->s4x4[0], vdupq_n_f32(v->v4[0]));
   for (i=1; i<4; ++i) {
      float32x4_t row = vmulq_f32(m->s4x4[i], vdupq_n_f32(v->v4[i]));
      d->s4 = vaddq_f32(d->s4, row);
   }
}

#else
typedef int make_iso_compilers_happy;
#endif /* NEON */

