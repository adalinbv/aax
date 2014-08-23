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

#ifndef _AAX_SIMD3D_H
#define _AAX_SIMD3D_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif

#include <arch.h>

#ifdef __SSE__
#include <xmmintrin.h>
#endif

#ifdef __SSE2__
#include <emmintrin.h>
#endif

#ifdef __SSE3__
#include <pmmintrin.h>
#endif

#ifdef __SSE4__
#include <smmintrin.h>
#endif

#ifdef __AVX__
#include <immintrin.h>
#endif

#include "base/types.h"
#include "base/geometry.h"

#ifdef __MINGW32__
	// Force proper stack alignment for functions that use SSE
# define FN_PREALIGN	__attribute__((force_align_arg_pointer))
#else
# define FN_PREALIGN
#endif

/* SSE*/
void _vec3CrossProduct_sse(vec3 d, const vec3 v1, const vec3 v2);
void _vec4Add_sse(vec4 d, const vec4 v);
void _vec4Copy_sse(vec4 d, const vec4 v);
void _vec4Devide_sse(vec4 d, float s);
void _vec4Mulvec4_sse(vec4 r, const vec4 v1, const vec4 v2);
void _vec4Sub_sse(vec4 d, const vec4 v);
void _vec4Matrix4_sse(vec4 d, const vec4 v, mtx4 m);
void _pt4Matrix4_sse(vec4 d, const vec4 p, mtx4 m);
void _mtx4Mul_sse(mtx4 d, mtx4 m1, mtx4 m2);

/* SSE3 */
void _vec4Matrix4_sse3(vec4 d, const vec4 v, mtx4 m);
void _pt4Matrix4_sse3(vec4 d, const vec4 p, mtx4 m);

/* SSE4 */
float _vec3Magnitude_sse41(const vec3);
float _vec3MagnitudeSquared_sse41(const vec3);
float _vec3DotProduct_sse41(const vec3, const vec3);
float _vec3Normalize_sse41(vec3, const vec3);


/* NEON */
void _vec4Add_neon(vec4 d, const vec4 v);
void _vec4Copy_neon(vec4 d, const vec4 v);
void _vec4Devide_neon(vec4 d, float s);
void _vec4Mulvec4_neon(vec4 r, const vec4 v1, const vec4 v2);
void _vec4Sub_neon(vec4 d, const vec4 v);
void _vec4Matrix4_neon(vec4 d, const vec4 v, mtx4 m);
void _pt4Matrix4_neon(vec4 d, const vec4 p, mtx4 m);
void _mtx4Mul_neon(mtx4 d, mtx4 m1, mtx4 m2);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_SIMD3D_H */

