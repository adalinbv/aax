/*
 * Copyright 2005-2012 by Erik Hofman.
 * Copyright 2009-2012 by Adalin B.V.
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

#ifndef _MSC_VER
# if SIZEOF_SIZE_T == 4
#  pragma GCC target ("arch=prescott")
# endif
# pragma GCC target ("sse3","fpmath=sse")
#else
# define __SSE3__
#endif

#include "arch_simd.h"

#ifdef __SSE3__

#if 0
/* Warning!! GPL3 code from:
 * http://fastcpp.blogspot.nl/2011/03/matrix-vector-multiplication-using-sse3.html
 */
void
_vec4Matrix4_sse3(vec4 d, const vec4 v, mtx4 m)
{
   __m128 x =  _mm_load_ps((const float*)v);
   __m128 A0 = _mm_load_ps((const float*)(m+0));
   __m128 A1 = _mm_load_ps((const float*)(m+1));
   __m128 A2 = _mm_load_ps((const float*)(m+2));
   __m128 A3 = _mm_load_ps((const float*)(m+3));
 
   __m128 m0 = _mm_mul_ps(A0, x);
   __m128 m1 = _mm_mul_ps(A1, x);
   __m128 m2 = _mm_mul_ps(A2, x);
   __m128 m3 = _mm_mul_ps(A3, x);
 
   __m128 sum_01 = _mm_hadd_ps(m0, m1);
   __m128 sum_23 = _mm_hadd_ps(m2, m3);
   __m128 result = _mm_hadd_ps(sum_01, sum_23);
  
   _mm_store_ps((float*)d, result);
}
#else
void
_vec4Matrix4_sse3(vec4 d, const vec4 v, mtx4 m)
{
   float v0 = v[0], v1 = v[1], v2 = v[2], v3 = v[3];

   d[0] = v0*m[0][0] + v1*m[0][1] + v2*m[0][2] + v3*m[0][3];
   d[1] = v0*m[1][0] + v1*m[1][1] + v2*m[1][2] + v3*m[1][3];
   d[2] = v0*m[2][0] + v1*m[2][1] + v2*m[2][2] + v3*m[2][3];
   d[3] = v0*m[3][0] + v1*m[3][1] + v2*m[3][2] + v3*m[3][3];
}
#endif

#endif /* SSE3 */

