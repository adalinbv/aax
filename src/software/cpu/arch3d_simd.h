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
void _vec4Matrix4_sse(vec4 d, const vec4 v, const mtx4 m);
void _pt4Matrix4_sse(vec4 d, const vec4 p, const mtx4 m);
void _mtx4Mul_sse(mtx4 d, const mtx4 m1, const mtx4 m2);

/* SSE3 */
void _vec4Matrix4_sse3(vec4 d, const vec4 v, const mtx4 m);
void _pt4Matrix4_sse3(vec4 d, const vec4 p, const mtx4 m);

/* SSE4 */
float _vec3Magnitude_sse41(const vec3);
float _vec3MagnitudeSquared_sse41(const vec3);
float _vec3DotProduct_sse41(const vec3, const vec3);
float _vec3Normalize_sse41(vec3, const vec3);

/* VFPV2 */
void _vec3Set_vfpv2(vec3 d, float x, float y, float z);
void _vec4Set_vfpv2(vec4 d, float x, float y, float z, float w);
void _vec3Negate_vfpv2(vec3 d, const vec3 v);
void _vec4Negate_vfpv2(vec4 d, const vec4 v);
void _vec3Add_vfpv2(vec3 d, const vec3 v);
void _vec4Add_vfpv2(vec4 d, const vec4 v);
void _vec3Sub_vfpv2(vec3 d, const vec3 v);
void _vec4Sub_vfpv2(vec4 d, const vec4 v);
void _vec3Devide_vfpv2(vec3 v, float s);
void _vec4Devide_vfpv2(vec4 v, float s);
void _ivec4Devide_vfpv2(ivec4 v, float s);
void _vec4ScalarMul_vfpv2(vec4 r, float f);
void _vec3Mulvec3_vfpv2(vec3 r, const vec3 v1, const vec3 v2);
void _vec4Mulvec4_vfpv2(vec4 r, const vec4 v1, const vec4 v2);
float _vec3Magnitude_vfpv2(const vec3 v);
double _vec3dMagnitude_vfpv2(const vec3d v);
float _vec3MagnitudeSquared_vfpv2(const vec3 v);
float _vec3DotProduct_vfpv2(const vec3 v1, const vec3 v2);
double _vec3dDotProduct_vfpv2(const vec3d v1, const vec3d v2);
void _vec3CrossProduct_vfpv2(vec3 d, const vec3 v1, const vec3 v2);
float _vec3Normalize_vfpv2(vec3 d, const vec3 v);
double _vec3dNormalize_vfpv2(vec3d d, const vec3d v);
//void _vec3Matrix3_vfpv2(vec3 d, const vec3 v, mtx3 m);
void _vec4Matrix4_vfpv2(vec4 d, const vec4 v, const mtx4 m);
void _pt4Matrix4_vfpv2(vec4 d, const vec4 p, const mtx4 m);
void _mtx4SetAbsolute_vfpv2(mtx4 d, char absolute);
//void _mtx4MulVec4_vfpv2(vec4 d, mtx4 m, const vec4 v);
void _mtx4Mul_vfpv2(mtx4 dst, const mtx4 mtx1, const mtx4 mtx2);
void _mtx4dMul_vfpv2(mtx4d dst, const mtx4d mtx1, const mtx4d mtx2);
void _mtx4InverseSimple_vfpv2(mtx4 dst, const mtx4 mtx);
void _mtx4dInverseSimple_vfpv2(mtx4d dst, const mtx4d mtx);
void _mtx4Translate_vfpv2(mtx4 m, float x, float y, float z);
void _mtx4dTranslate_vfpv2(mtx4d m, double x, double y, double z);
void _mtx4Rotate_vfpv2(mtx4 mtx, float angle_rad, float x, float y, float z);
void _mtx4dRotate_vfpv2(mtx4d mtx, double angle_rad, double x, double y, double z);

/* VFPV3 */
void _vec3Set_vfpv3(vec3 d, float x, float y, float z);
void _vec4Set_vfpv3(vec4 d, float x, float y, float z, float w);
void _vec3Negate_vfpv3(vec3 d, const vec3 v);
void _vec4Negate_vfpv3(vec4 d, const vec4 v);
void _vec3Add_vfpv3(vec3 d, const vec3 v);
void _vec4Add_vfpv3(vec4 d, const vec4 v);
void _vec3Sub_vfpv3(vec3 d, const vec3 v);
void _vec4Sub_vfpv3(vec4 d, const vec4 v);
void _vec3Devide_vfpv3(vec3 v, float s);
void _vec4Devide_vfpv3(vec4 v, float s);
void _ivec4Devide_vfpv3(ivec4 v, float s);
void _vec4ScalarMul_vfpv3(vec4 r, float f);
void _vec3Mulvec3_vfpv3(vec3 r, const vec3 v1, const vec3 v2);
void _vec4Mulvec4_vfpv3(vec4 r, const vec4 v1, const vec4 v2);
float _vec3Magnitude_vfpv3(const vec3 v);
double _vec3dMagnitude_vfpv3(const vec3d v);
float _vec3MagnitudeSquared_vfpv3(const vec3 v);
float _vec3DotProduct_vfpv3(const vec3 v1, const vec3 v2);
double _vec3dDotProduct_vfpv3(const vec3d v1, const vec3d v2);
void _vec3CrossProduct_vfpv3(vec3 d, const vec3 v1, const vec3 v2);
float _vec3Normalize_vfpv3(vec3 d, const vec3 v);
double _vec3dNormalize_vfpv3(vec3d d, const vec3d v);
//void _vec3Matrix3_vfpv3(vec3 d, const vec3 v, mtx3 m);
void _vec4Matrix4_vfpv3(vec4 d, const vec4 v, const mtx4 m);
void _pt4Matrix4_vfpv3(vec4 d, const vec4 p, const mtx4 m);
void _mtx4SetAbsolute_vfpv3(mtx4 d, char absolute);
//void _mtx4MulVec4_vfpv3(vec4 d, mtx4 m, const vec4 v);
void _mtx4Mul_vfpv3(mtx4 dst, const mtx4 mtx1, const mtx4 mtx2);
void _mtx4dMul_vfpv3(mtx4d dst, const mtx4d mtx1, const mtx4d mtx2);
void _mtx4InverseSimple_vfpv3(mtx4 dst, const mtx4 mtx);
void _mtx4dInverseSimple_vfpv3(mtx4d dst, const mtx4d mtx);
void _mtx4Translate_vfpv3(mtx4 m, float x, float y, float z);
void _mtx4dTranslate_vfpv3(mtx4d m, double x, double y, double z);
void _mtx4Rotate_vfpv3(mtx4 mtx, float angle_rad, float x, float y, float z);
void _mtx4dRotate_vfpv3(mtx4d mtx, double angle_rad, double x, double y, double z);

/* NEON */
void _vec4Add_neon(vec4 d, const vec4 v);
void _vec4Copy_neon(vec4 d, const vec4 v);
void _vec4Devide_neon(vec4 d, float s);
void _vec4Mulvec4_neon(vec4 r, const vec4 v1, const vec4 v2);
void _vec4Sub_neon(vec4 d, const vec4 v);
void _vec4Matrix4_neon(vec4 d, const vec4 v, const mtx4 m);
void _pt4Matrix4_neon(vec4 d, const vec4 p, const mtx4 m);
void _mtx4Mul_neon(mtx4 d, const mtx4 m1, const mtx4 m2);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_SIMD3D_H */

