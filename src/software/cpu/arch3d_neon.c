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

void
_vec4Copy_neon(vec4 d, const vec4 v)
{
   float32x4_t vec = vld1q_f32(v);
   vst1q_f32(d, vec);
}

void
_vec4Mulvec4_neon(vec4 r, const vec4 v1, const vec4 v2)
{
   float32x4_t vec1 = vld1q_f32(v1);
   float32x4_t vec2 = vld1q_f32(v2);
   vec1 = vmulq_f32(vec1, vec2);
   vst1q_f32(r, vec1);
}

void
_vec4Matrix4_neon(vec4 d, const vec4 p, const mtx4 m)
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
_pt4Matrix4_neon(vec4 d, const vec4 vi, const mtx4 m)
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
_mtx4Mul_neon(mtx4 d, const mtx4 m1, const mtx4 m2)
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

