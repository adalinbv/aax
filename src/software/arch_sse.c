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

#include <api.h>

#include "arch_simd.h"

#ifdef __SSE__


FN_PREALIGN void 
_vec3CrossProduct_sse(vec3 d, const vec3 v1, const vec3 v2)
{
   __m128 xmm1 = _mm_set_ps(v1[0], v1[1], v1[2], 0);
   __m128 xmm2 = _mm_set_ps(v2[0], v2[1], v2[2], 0);
   __m128 xmm5 = _mm_shuffle_ps(xmm1, xmm1, _MM_SHUFFLE(3, 0, 2, 1));
   __m128 xmm0 = _mm_shuffle_ps(xmm2, xmm2, _MM_SHUFFLE(3, 1, 0, 2));
   __m128 xmm4 = _mm_shuffle_ps(xmm1, xmm1, _MM_SHUFFLE(3, 1, 0, 2));
   __m128 xmm3 = _mm_shuffle_ps(xmm2, xmm2, _MM_SHUFFLE(3, 0, 2, 1));
   vec4 r;

   _mm_store_ps(r, _mm_sub_ps(_mm_mul_ps(xmm5, xmm0), _mm_mul_ps(xmm4, xmm3)));
   _aax_memcpy(d, r, 3*sizeof(float));
}

FN_PREALIGN void
_vec4Copy_sse(vec4 d, const vec4 v)
{
   __m128 xmm1; 
   
   xmm1 = _mm_load_ps(v); 
   _mm_store_ps(d, xmm1); 
}

FN_PREALIGN void
_vec4Add_sse(vec4 d, const vec4 v)
{
   __m128 xmm1, xmm2; 
   
   xmm1 = _mm_load_ps(d); 
   xmm2 = _mm_load_ps(v); 
   xmm1 = _mm_add_ps(xmm1, xmm2);
   _mm_store_ps(d, xmm1);
}

FN_PREALIGN void
_vec4Sub_sse(vec4 d, const vec4 v)
{
   __m128 xmm1, xmm2;

   xmm1 = _mm_load_ps(d);
   xmm2 = _mm_load_ps(v);
   xmm1 = _mm_sub_ps(xmm1, xmm2);
   _mm_store_ps(d, xmm1);
}

FN_PREALIGN void
_vec4Devide_sse(vec4 v, float s)
{
   if (s)
   {
      __m128 xmm1, xmm2;

      xmm1 = _mm_load_ps(v);
      xmm2 = _mm_set1_ps(s);
      xmm1 = _mm_div_ps(xmm1, xmm2);
      _mm_store_ps(v, xmm1);
   }
}

FN_PREALIGN void
_vec4Mulvec4_sse(vec4 r, const vec4 v1, const vec4 v2)
{
   __m128 xmm1, xmm2;

   xmm1 = _mm_load_ps(v1);
   xmm2 = _mm_load_ps(v2);
   xmm1 = _mm_mul_ps(xmm1, xmm2);
   _mm_store_ps(r, xmm1);
}

FN_PREALIGN inline void
__vec4Matrix4_sse(vec4 d, const vec4 v, mtx4 m)
{
   __m128 a_line, b_line, r_line;
   const float *a = (const float *)m;
   const float *b = (const float *)v;
   float *r = (float *)d;
   int i;

  /*
   * unroll the first step of the loop to avoid having to initialize
   * r_line to zero
   */
   a_line = _mm_load_ps(a);		/* a_line = vec4(column(m, 0)) */
   b_line = _mm_set1_ps(b[0]);		/* b_line = vec4(v[0])         */
   r_line = _mm_mul_ps(a_line, b_line);	/* r_line = a_line * b_line    */
   for (i=1; i<4; i++)
   {
      a_line = _mm_load_ps(&a[i*4]);	/* a_line = vec4(column(m, i)) */
      b_line = _mm_set1_ps(b[i]);	/* b_line = vec4(v[i])         */
					/* r_line += a_line * b_line   */
      r_line = _mm_add_ps(_mm_mul_ps(a_line, b_line), r_line);
   }
   _mm_store_ps(r, r_line);		/* r = r_line                  */
}

void
_vec4Matrix4_sse(vec4 d, const vec4 v, mtx4 m)
{
   float *pv = (float*)v;
   pv[3] = 0.0f;
   __vec4Matrix4_sse(d, pv, m);
}

void
_pt4Matrix4_sse(vec4 d, const vec4 p, mtx4 m)
{
   float *pp = (float*)p;
   pp[3] = 1.0f;
   __vec4Matrix4_sse(d, pp, m);
}

FN_PREALIGN void
_mtx4Mul_sse(mtx4 d, mtx4 m1, mtx4 m2)
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

