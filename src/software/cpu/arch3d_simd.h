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
float _vec3Magnitude_sse(const vec3_t v);
float _vec3MagnitudeSquared_sse(const vec3_t v);
float _vec3DotProduct_sse(const vec3_t v1, const vec3_t v2);
void _vec3CrossProduct_sse(vec3_t d, const vec3_t v1, const vec3_t v2);
void _vec4Copy_sse(vec4_t d, const vec4_t v);
void _vec4Mulvec4_sse(vec4_t r, const vec4_t v1, const vec4_t v2);
void _vec4Matrix4_sse(vec4_t d, const vec4_t v, const mtx4_t m);
void _pt4Matrix4_sse(vec4_t d, const vec4_t p, const mtx4_t m);
void _mtx4Mul_sse(mtx4_t d, const mtx4_t m1, const mtx4_t m2);

/* SSE3 */
float _vec3Magnitude_sse3(const vec3_t v);
float _vec3MagnitudeSquared_sse3(const vec3_t v);
float _vec3DotProduct_sse3(const vec3_t v1, const vec3_t v2);
void _vec4Matrix4_sse3(vec4_t d, const vec4_t v, const mtx4_t m);

/* SSE4 */
float _vec3Magnitude_sse41(const vec3_t v);
float _vec3MagnitudeSquared_sse41(const vec3_t v);
float _vec3DotProduct_sse41(const vec3_t v1, const vec3_t v2);
float _vec3Normalize_sse41(vec3_t d, const vec3_t v);

/* VFPV2 */
void _vec3Negate_vfpv2(vec3_t d, const vec3_t v);
void _vec4Negate_vfpv2(vec4_t d, const vec4_t v);
void _vec4ScalarMul_vfpv2(vec4_t r, float f);
void _vec3Mulvec3_vfpv2(vec3_t r, const vec3_t v1, const vec3_t v2);
void _vec4Mulvec4_vfpv2(vec4_t r, const vec4_t v1, const vec4_t v2);
float _vec3Magnitude_vfpv2(const vec3_t v);
double _vec3dMagnitude_vfpv2(const vec3d_t v);
float _vec3MagnitudeSquared_vfpv2(const vec3_t v);
float _vec3DotProduct_vfpv2(const vec3_t v1, const vec3_t v2);
double _vec3dDotProduct_vfpv2(const vec3d_t v1, const vec3d_t v2);
void _vec3CrossProduct_vfpv2(vec3_t d, const vec3_t v1, const vec3_t v2);
float _vec3Normalize_vfpv2(vec3_t d, const vec3_t v);
double _vec3dNormalize_vfpv2(vec3d_t d, const vec3d_t v);
//void _vec3Matrix3_vfpv2(vec3_t d, const vec3_t v, mtx3 m);
void _vec4Matrix4_vfpv2(vec4_t d, const vec4_t v, const mtx4_t m);
void _pt4Matrix4_vfpv2(vec4_t d, const vec4_t p, const mtx4_t m);
void _mtx4SetAbsolute_vfpv2(mtx4_t d, char absolute);
//void _mtx4MulVec4_vfpv2(vec4_t d, mtx4_t m, const vec4_t v);
void _mtx4Mul_vfpv2(mtx4_t dst, const mtx4_t mtx1, const mtx4_t mtx2);
void _mtx4dMul_vfpv2(mtx4d_t dst, const mtx4d_t mtx1, const mtx4d_t mtx2);
void _mtx4InverseSimple_vfpv2(mtx4_t dst, const mtx4_t mtx);
void _mtx4dInverseSimple_vfpv2(mtx4d_t dst, const mtx4d_t mtx);
void _mtx4Translate_vfpv2(mtx4_t m, float x, float y, float z);
void _mtx4dTranslate_vfpv2(mtx4d_t m, double x, double y, double z);
void _mtx4Rotate_vfpv2(mtx4_t mtx, float angle_rad, float x, float y, float z);
void _mtx4dRotate_vfpv2(mtx4d_t mtx, double angle_rad, double x, double y, double z);

/* VFPV3 */
void _vec3Negate_vfpv3(vec3_t d, const vec3_t v);
void _vec4Negate_vfpv3(vec4_t d, const vec4_t v);
void _vec4ScalarMul_vfpv3(vec4_t r, float f);
void _vec3Mulvec3_vfpv3(vec3_t r, const vec3_t v1, const vec3_t v2);
void _vec4Mulvec4_vfpv3(vec4_t r, const vec4_t v1, const vec4_t v2);
float _vec3Magnitude_vfpv3(const vec3_t v);
double _vec3dMagnitude_vfpv3(const vec3d_t v);
float _vec3MagnitudeSquared_vfpv3(const vec3_t v);
float _vec3DotProduct_vfpv3(const vec3_t v1, const vec3_t v2);
double _vec3dDotProduct_vfpv3(const vec3d_t v1, const vec3d_t v2);
void _vec3CrossProduct_vfpv3(vec3_t d, const vec3_t v1, const vec3_t v2);
float _vec3Normalize_vfpv3(vec3_t d, const vec3_t v);
double _vec3dNormalize_vfpv3(vec3d_t d, const vec3d_t v);
//void _vec3Matrix3_vfpv3(vec3_t d, const vec3_t v, mtx3 m);
void _vec4Matrix4_vfpv3(vec4_t d, const vec4_t v, const mtx4_t m);
void _pt4Matrix4_vfpv3(vec4_t d, const vec4_t p, const mtx4_t m);
void _mtx4SetAbsolute_vfpv3(mtx4_t d, char absolute);
//void _mtx4MulVec4_vfpv3(vec4_t d, mtx4_t m, const vec4_t v);
void _mtx4Mul_vfpv3(mtx4_t dst, const mtx4_t mtx1, const mtx4_t mtx2);
void _mtx4dMul_vfpv3(mtx4d_t dst, const mtx4d_t mtx1, const mtx4d_t mtx2);
void _mtx4InverseSimple_vfpv3(mtx4_t dst, const mtx4_t mtx);
void _mtx4dInverseSimple_vfpv3(mtx4d_t dst, const mtx4d_t mtx);
void _mtx4Translate_vfpv3(mtx4_t m, float x, float y, float z);
void _mtx4dTranslate_vfpv3(mtx4d_t m, double x, double y, double z);
void _mtx4Rotate_vfpv3(mtx4_t mtx, float angle_rad, float x, float y, float z);
void _mtx4dRotate_vfpv3(mtx4d_t mtx, double angle_rad, double x, double y, double z);

/* NEON */
void _vec4Copy_neon(vec4_t d, const vec4_t v);
void _vec4Mulvec4_neon(vec4_t r, const vec4_t v1, const vec4_t v2);
void _vec4Matrix4_neon(vec4_t d, const vec4_t v, const mtx4_t m);
void _pt4Matrix4_neon(vec4_t d, const vec4_t p, const mtx4_t m);
void _mtx4Mul_neon(mtx4_t d, const mtx4_t m1, const mtx4_t m2);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_SIMD3D_H */

