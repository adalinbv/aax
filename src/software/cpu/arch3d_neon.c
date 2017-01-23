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

#ifdef __ARM_NEON__
# include <arm_neon.h>
#include "arch3d_simd.h"

# define vandq_f32(a,b) vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)))

static const uint32_t m3a32[] = { 0xffffffff,0xffffffff,0xffffffff,0 };
static const float32x4_t fmask3 = vld1q_f32((const float*)m3a32);
static const float32x2_t zero = vdup_n_f32(0);

static inline float32x4_t
load_vec3(const vec3_ptr v)
{  
   return vandq_f32(v->s4, fmask3);
}

static inline float
hsum_float32x4_neon(float32x4_t v) {
    float32x2_ptr r = vadd_f32(vget_high_f32(v->s4), vget_low_f32(v->s4));
    return vget_lane_f32(vpadd_f32(r, r), 0);
}

float
_vec3fMagnitudeSquared_neon(const vec3_ptr v3)
{
   float32x4_t v = load_vec3(v3);
   return hsum_float32x4_neon(vmulq_f32(v, v));
}

float
_vec3fMagnitude_neon(const vec3_ptr v3)
{
   float32x4_t v = load_vec3(v3);
   return sqrtf(hsum_float32x4_neon(vmulq_f32(v, v)));
}

float
_vec3fDotProduct_neon(const vec3_ptr v1, const vec3_ptr v2)
{
   return hsum_float32x4_neon(vmulq_f32(load_vec3(v1), load_vec3(v2)));
}

void
_vec3fCrossProduct_neon(vec3_ptr d, const vec3_ptr v1, const vec3_ptr v2)
{
   float32x2x2_t v1lh_2013 = vld2_f32(v1->s4);	// from 0123 to 0213
   float32x2x2_t v2lh_2013 = vld2_f32(v2->s4);

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
   float32x4_t vec = vld1q_f32(v);
   vst1q_f32(d, vec);
}

void
_vec4fMulvec4_neon(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2)
{
   float32x4_t vec1 = vld1q_f32(v1);
   float32x4_t vec2 = vld1q_f32(v2);
   vec1 = vmulq_f32(vec1, vec2);
   vst1q_f32(r, vec1);
}

void
_vec4fMatrix4_neon(vec4f_ptr d, const vec4f_ptr p, const mtx4_ptr m)
{
   float32x4_t mx = vld1q_f32((const float *)&m[0]);
   float32x4_t mv = vmulq_f32(mx, vdupq_n_f32(vi[0]));
   int i;
   for (i=1; i<3; ++i) {
      float32x4_t mx = vld1q_f32((const float *)&m[i]);
      float32x4_t row = vmulq_f32(mx, vdupq_n_f32(vi[i]));
      mv = vaddq_f32(mv, row);
   }
   vst1q_f32(d, mv);
}

void
_pt4fMatrix4_neon(vec4f_ptr d, const vec4f_ptr vi, const mtx4_ptr m)
{
   float32x4_t mx = vld1q_f32((const float *)&m[0]);
   float32x4_t mv = vmulq_f32(mx, vdupq_n_f32(vi[0]));
   int i;
   for (i=1; i<3; ++i) {
      float32x4_t mx = vld1q_f32((const float *)&m[i]);
      float32x4_t row = vmulq_f32(mx, vdupq_n_f32(vi[i]));
      mv = vaddq_f32(mv, row);
   }
   mx = vdupq_n_f32((const float *)&m[3]);
   mv = vaddq_f32(mv, row);
   vst1q_f32(d, mv);
}


void
_mtx4fMul_neon(mtx4_ptr d, const mtx4_ptr m1, const mtx4_ptr m2)
{
   int i;
   for (i=0; i<4; ++i) {
      float32x4_t m1x = vld1q_f32((const float *)&m1[0]);
      float32x4_t col = vdupq_n_f32(m2[i][0]);
      float32x4_t row = vmulq_f32(m1x, col);
      for (int j=1; j<4; ++j) {
         m1x = vld1q_f32((const float *)&m1[j]);
         col = vdupq_n_f32(m2[i][j]);
         row = vaddq_f32(row, vmulq_f32(m1x, col));
      }
      vst1q_f32(d[i], row);
   }
}

#endif /* NEON */

