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

#include <api.h>

#include "arch3d_simd.h"

#ifdef __SSE__

static inline __m128
load_vec3(const vec3_t v)
{
   __m128 xy = _mm_loadl_pi(_mm_setzero_ps(), (const __m64*)v);
   __m128 z = _mm_load_ss(&v[2]);
   return _mm_movelh_ps(xy, z);
}

static inline float
hsum_ps_sse(__m128 v) {
   __m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));
   __m128 sums = _mm_add_ps(v, shuf);
   shuf = _mm_movehl_ps(shuf, sums);
   sums = _mm_add_ss(sums, shuf);
   return _mm_cvtss_f32(sums);
}

FN_PREALIGN float
_vec3MagnitudeSquared_sse(const vec3_t v3)
{
   __m128 v = load_vec3(v3);
   return hsum_ps_sse(_mm_mul_ps(v, v));
}

FN_PREALIGN float
_vec3Magnitude_sse(const vec3_t v3)
{  
   __m128 v = load_vec3(v3);
   return sqrtf(hsum_ps_sse(_mm_mul_ps(v, v)));
}

FN_PREALIGN float
_vec3DotProduct_sse(const vec3_t v1, const vec3_t v2)
{
   return hsum_ps_sse(_mm_mul_ps(load_vec3(v1), load_vec3(v2)));
}

// http://threadlocalmutex.com/?p=8
FN_PREALIGN void 
_vec3CrossProduct_sse(vec3_t d, const vec3_t v1, const vec3_t v2)
{
   __m128 xmm1 = load_vec3(v1);
   __m128 xmm2 = load_vec3(v2);
   __m128 a = _mm_shuffle_ps(xmm1, xmm1, _MM_SHUFFLE(3, 0, 2, 1));
   __m128 b = _mm_shuffle_ps(xmm2, xmm2, _MM_SHUFFLE(3, 0, 2, 1));
   __m128 c = _mm_sub_ps(_mm_mul_ps(xmm1, b), _mm_mul_ps(a, xmm2));
   _mm_store_ps(d, _mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 0, 2, 1)));
}


FN_PREALIGN void
_vec4Copy_sse(vec4_t d, const vec4_t v)
{
   __m128 xmm1; 
   
   xmm1 = _mm_load_ps(v); 
   _mm_store_ps(d, xmm1); 
}


FN_PREALIGN void
_vec4Mulvec4_sse(vec4_t r, const vec4_t v1, const vec4_t v2)
{
   __m128 xmm1, xmm2;

   xmm1 = _mm_load_ps(v1);
   xmm2 = _mm_load_ps(v2);
   xmm1 = _mm_mul_ps(xmm1, xmm2);
   _mm_store_ps(r, xmm1);
}

FN_PREALIGN void
_vec4Matrix4_sse(vec4_t d, const vec4_t vi, const mtx4_t m)
{
   __m128 mx = _mm_load_ps((const float *)&m[0]);
   __m128 mv = _mm_mul_ps(mx, _mm_set1_ps(vi[0]));
   int i;
   for (i=1; i<3; ++i) {
      __m128 mx = _mm_load_ps((const float *)&m[i]);
      __m128 row = _mm_mul_ps(mx, _mm_set1_ps(vi[i]));
      mv = _mm_add_ps(mv, row);
   }
   _mm_store_ps(d, mv);
}

FN_PREALIGN void
_pt4Matrix4_sse(vec4_t d, const vec4_t vi, const mtx4_t m)
{
   __m128 mx = _mm_load_ps((const float *)&m[0]);
   __m128 mv = _mm_mul_ps(mx, _mm_set1_ps(vi[0]));
   int i;
   for (i=1; i<3; ++i) {
      __m128 mx = _mm_load_ps((const float *)&m[i]);
      __m128 row = _mm_mul_ps(mx, _mm_set1_ps(vi[i]));
      mv = _mm_add_ps(mv, row);
   }
   mx = _mm_load_ps((const float *)&m[3]);
   mv = _mm_add_ps(mv, mx);
   _mm_store_ps(d, mv);
}

FN_PREALIGN void
_mtx4Mul_sse(mtx4_t d, const mtx4_t m1, const mtx4_t m2)
{
   __m128 a_line, b_line, r_line;
   const float *a = (const float *)m1;
   const float *b = (const float *)m2;
   float *r = (float *)d;
   int i;

   for (i=0; i<16; i += 4)
   {
     int j;
     /*
      * unroll the first step of the loop to avoid having to initialize
      * r_line to zero
      */
      a_line = _mm_load_ps(a);		   /* a_line = vec4(column(a, 0)) */
      b_line = _mm_set1_ps(b[i]);	   /* b_line = vec4(b[i][0])      */
      r_line = _mm_mul_ps(a_line, b_line); /* r_line = a_line * b_line    */
      for (j=1; j<4; j++)
      {
         a_line = _mm_load_ps(&a[j*4]);    /* a_line = vec4(column(a, j)) */
         b_line = _mm_set1_ps(b[i+j]);     /* b_line = vec4(b[i][j])      */
                                           /* r_line += a_line * b_line   */
         r_line = _mm_add_ps(_mm_mul_ps(a_line, b_line), r_line);
      }
      _mm_store_ps(&r[i], r_line);         /* r[i] = r_line               */
   }
}
#endif /* SSE */

