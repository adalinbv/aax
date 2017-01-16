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

#if 0
// http://fastcpp.blogspot.nl/2011/04/vector-cross-product-using-sse-code.html
FN_PREALIGN void 
_vec3CrossProduct_sse(vec3_t d, const vec3_t v1, const vec3_t v2)
{
   __m128 xmm1 = _mm_setr_ps(v1[0], v1[1], v1[2], 0);
   __m128 xmm2 = _mm_setr_ps(v2[0], v2[1], v2[2], 0);
   __m128 result = _mm_sub_ps(
         _mm_mul_ps(xmm2, _mm_shuffle_ps(xmm1, xmm1, _MM_SHUFFLE(3, 0, 2, 1))),
         _mm_mul_ps(xmm1, _mm_shuffle_ps(xmm2, xmm2, _MM_SHUFFLE(3, 0, 2, 1)))
      );
   vec4_t r;

   _mm_store_ps(r, _mm_shuffle_ps(result, result, _MM_SHUFFLE(3, 0, 2, 1 )));
   _aax_memcpy(d, r, 3*sizeof(float));
}
#else

#define VECTOR3D_ROT1_MASK 0xC9
#define VECTOR3D_ROT2_MASK 0xD2

FN_PREALIGN void 
_vec3CrossProduct_sse(vec3_t d, const vec3_t v1, const vec3_t v2)
{
   __m128 L0 = _mm_set_ps(v1[0], v1[1], v1[2], 0);
   __m128 R0 = _mm_set_ps(v2[0], v2[1], v2[2], 0);
   __m128 L1 = L0;
   __m128 R1 = R0;
   vec4_t r;

   L0 = _mm_shuffle_ps(L0, L0, VECTOR3D_ROT1_MASK);
   R1 = _mm_shuffle_ps(R1, R1, VECTOR3D_ROT1_MASK);
   R0 = _mm_shuffle_ps(R0, R0, VECTOR3D_ROT2_MASK);
   L1 = _mm_shuffle_ps(L1, L1, VECTOR3D_ROT2_MASK);

   L0 = _mm_mul_ps(L0, R0);
   L1 = _mm_mul_ps(L1, R1);

   L0 = _mm_sub_ps(L0, L1);
   _mm_store_ps(r, L0);
   _aax_memcpy(d, r, 3*sizeof(float));
}
#endif

FN_PREALIGN void
_vec4Copy_sse(vec4_t d, const vec4_t v)
{
   __m128 xmm1; 
   
   xmm1 = _mm_load_ps(v); 
   _mm_store_ps(d, xmm1); 
}

FN_PREALIGN void
_vec4Add_sse(vec4_t d, const vec4_t v)
{
   __m128 xmm1, xmm2; 
   
   xmm1 = _mm_load_ps(d); 
   xmm2 = _mm_load_ps(v); 
   xmm1 = _mm_add_ps(xmm1, xmm2);
   _mm_store_ps(d, xmm1);
}

FN_PREALIGN void
_vec4Sub_sse(vec4_t d, const vec4_t v)
{
   __m128 xmm1, xmm2;

   xmm1 = _mm_load_ps(d);
   xmm2 = _mm_load_ps(v);
   xmm1 = _mm_sub_ps(xmm1, xmm2);
   _mm_store_ps(d, xmm1);
}

FN_PREALIGN void
_vec4Devide_sse(vec4_t v, float s)
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
   __m128 a_line, b_line, r_line;
   vec4_t v;
   const float *a, *b; 
   float *r;
   int i;

   vec4Copy(v, vi);
   v[3] = 0.0f;

   a = (const float *)m;
   b = (const float *)v;
   r = (float *)d;

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

FN_PREALIGN void
_pt4Matrix4_sse(vec4_t d, const vec4_t vi, const mtx4_t m)
{
   __m128 a_line, b_line, r_line;
   vec4_t v;
   const float *a, *b;
   float *r;
   int i;

   vec4Copy(v, vi);
   v[3] = 1.0f;

   a = (const float *)m;
   b = (const float *)v;
   r = (float *)d;

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

