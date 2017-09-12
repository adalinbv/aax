/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef _AAX_SIMD3D_H
#define _AAX_SIMD3D_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#if defined(_MSC_VER)
# include <intrin.h>
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
# include <x86intrin.h>
#elif defined(__GNUC__) && defined(__ARM_NEON__)
# include <arm_neon.h>
#endif

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif

#include <arch.h>

#include "base/types.h"
#include "base/geometry.h"

#ifdef __MINGW32__
	// Force proper stack alignment for functions that use SSE
# define FN_PREALIGN	__attribute__((force_align_arg_pointer))
#else
# define FN_PREALIGN
#endif

/* SSE*/
float _vec3fMagnitude_sse(const vec3f_ptr v);
float _vec3fMagnitudeSquared_sse(const vec3f_ptr v);
float _vec3fDotProduct_sse(const vec3f_ptr v1, const vec3f_ptr v2);
void _vec3fCrossProduct_sse(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2);
void _vec4fCopy_sse(vec4f_ptr d, const vec4f_ptr v);
void _vec4fMulvec4_sse(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2);
void _vec4fMatrix4_sse(vec4f_ptr d, const vec4f_ptr v, const mtx4f_ptr m);
void _pt4fMatrix4_sse(vec4f_ptr d, const vec4f_ptr p, const mtx4f_ptr m);
void _mtx4fMul_sse(mtx4f_ptr d, const mtx4f_ptr m1, const mtx4f_ptr m2);

/* SSE3 */
float _vec3fMagnitude_sse3(const vec3f_ptr v);
float _vec3fMagnitudeSquared_sse3(const vec3f_ptr v);
float _vec3fDotProduct_sse3(const vec3f_ptr v1, const vec3f_ptr v2);
void _vec4fMatrix4_sse3(vec4f_ptr d, const vec4f_ptr pv, const mtx4f_ptr m);
void _mtx4dMul_sse3(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2);

/* SSE4 */
float _vec3fMagnitude_sse41(const vec3f_ptr v);
float _vec3fMagnitudeSquared_sse41(const vec3f_ptr v);
float _vec3fDotProduct_sse41(const vec3f_ptr v1, const vec3f_ptr v2);
float _vec3fNormalize_sse41(vec3f_ptr d, const vec3f_ptr v);

/* AVX * SSE/VEX */
float _vec3fMagnitude_sse_vex(const vec3f_ptr v);
float _vec3fMagnitudeSquared_sse_vex(const vec3f_ptr v);
float _vec3fDotProduct_sse_vex(const vec3f_ptr v1, const vec3f_ptr v2);
void _vec3fCrossProduct_sse_vex(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2);
void _vec4fCopy_sse_vex(vec4f_ptr d, const vec4f_ptr v);
void _vec4fMulvec4_sse_vex(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2);
void _vec4fMatrix4_sse_vex(vec4f_ptr d, const vec4f_ptr v, const mtx4f_ptr m);
void _pt4fMatrix4_sse_vex(vec4f_ptr d, const vec4f_ptr p, const mtx4f_ptr m);
void _mtx4fMul_sse_vex(mtx4f_ptr d, const mtx4f_ptr m1, const mtx4f_ptr m2);
void _mtx4dMul_sse_vex(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2);

/* AVX */
void _vec4dMatrix4_avx(vec4d_ptr d, const vec4d_ptr v, const mtx4d_ptr m);
void _pt4dMatrix4_avx(vec4d_ptr d, const vec4d_ptr p, const mtx4d_ptr m);
void _mtx4dMul_avx(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2);


/* VFPV2 */
void _vec3fNegate_vfpv2(vec3f_ptr d, const vec3f_ptr v);
void _vec4fNegate_vfpv2(vec4f_ptr d, const vec4f_ptr v);
void _vec4fScalarMul_vfpv2(vec4f_ptr r, float f);
void _vec3fMulvec3_vfpv2(vec3f_ptr r, const vec3f_ptr v1, const vec3f_ptr v2);
void _vec4fMulvec4_vfpv2(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2);
float _vec3fMagnitude_vfpv2(const vec3f_ptr v);
double _vec3dMagnitude_vfpv2(const vec3d_ptr v);
float _vec3fMagnitudeSquared_vfpv2(const vec3f_ptr v);
float _vec3fDotProduct_vfpv2(const vec3f_ptr v1, const vec3f_ptr v2);
double _vec3dDotProduct_vfpv2(const vec3d_ptr v1, const vec3d_ptr v2);
void _vec3fCrossProduct_vfpv2(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2);
float _vec3fNormalize_vfpv2(vec3f_ptr d, const vec3f_ptr v);
double _vec3dNormalize_vfpv2(vec3d_ptr d, const vec3d_ptr v);
//void _vec3fMatrix3_vfpv2(vec3f_ptr d, const vec3f_ptr v, mtx3 m);
void _vec4fMatrix4_vfpv2(vec4f_ptr d, const vec4f_ptr v, const mtx4f_ptr m);
void _pt4fMatrix4_vfpv2(vec4f_ptr d, const vec4f_ptr p, const mtx4f_ptr m);
void _mtx4fSetAbsolute_vfpv2(mtx4f_ptr d, char absolute);
//void _mtx4fMulVec4_vfpv2(vec4f_ptr d, mtx4_t m, const vec4f_ptr v);
void _mtx4fMul_vfpv2(mtx4f_ptr dst, const mtx4f_ptr mtx1, const mtx4f_ptr mtx2);
void _mtx4dMul_vfpv2(mtx4d_ptr dst, const mtx4d_ptr mtx1, const mtx4d_ptr mtx2);
void _mtx4fInverseSimple_vfpv2(mtx4f_ptr dst, const mtx4f_ptr mtx);
void _mtx4dInverseSimple_vfpv2(mtx4d_ptr dst, const mtx4d_ptr mtx);
void _mtx4fTranslate_vfpv2(mtx4f_ptr m, float x, float y, float z);
void _mtx4dTranslate_vfpv2(mtx4d_ptr m, double x, double y, double z);
void _mtx4fRotate_vfpv2(mtx4f_ptr mtx, float angle_rad, float x, float y, float z);
void _mtx4dRotate_vfpv2(mtx4d_ptr mtx, double angle_rad, double x, double y, double z);

/* VFPV3 */
void _vec3fNegate_vfpv3(vec3f_ptr d, const vec3f_ptr v);
void _vec4fNegate_vfpv3(vec4f_ptr d, const vec4f_ptr v);
void _vec4fScalarMul_vfpv3(vec4f_ptr r, float f);
void _vec3fMulvec3_vfpv3(vec3f_ptr r, const vec3f_ptr v1, const vec3f_ptr v2);
void _vec4fMulvec4_vfpv3(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2);
float _vec3fMagnitude_vfpv3(const vec3f_ptr v);
double _vec3dMagnitude_vfpv3(const vec3d_ptr v);
float _vec3fMagnitudeSquared_vfpv3(const vec3f_ptr v);
float _vec3fDotProduct_vfpv3(const vec3f_ptr v1, const vec3f_ptr v2);
double _vec3dDotProduct_vfpv3(const vec3d_ptr v1, const vec3d_ptr v2);
void _vec3fCrossProduct_vfpv3(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2);
float _vec3fNormalize_vfpv3(vec3f_ptr d, const vec3f_ptr v);
double _vec3dNormalize_vfpv3(vec3d_ptr d, const vec3d_ptr v);
//void _vec3fMatrix3_vfpv3(vec3f_ptr d, const vec3f_ptr v, mtx3 m);
void _vec4fMatrix4_vfpv3(vec4f_ptr d, const vec4f_ptr v, const mtx4f_ptr m);
void _pt4fMatrix4_vfpv3(vec4f_ptr d, const vec4f_ptr p, const mtx4f_ptr m);
void _mtx4fSetAbsolute_vfpv3(mtx4f_ptr d, char absolute);
//void _mtx4fMulVec4_vfpv3(vec4f_ptr d, mtx4_t m, const vec4f_ptr v);
void _mtx4fMul_vfpv3(mtx4f_ptr dst, const mtx4f_ptr mtx1, const mtx4f_ptr mtx2);
void _mtx4dMul_vfpv3(mtx4d_ptr dst, const mtx4d_ptr mtx1, const mtx4d_ptr mtx2);
void _mtx4fInverseSimple_vfpv3(mtx4f_ptr dst, const mtx4f_ptr mtx);
void _mtx4dInverseSimple_vfpv3(mtx4d_ptr dst, const mtx4d_ptr mtx);
void _mtx4fTranslate_vfpv3(mtx4f_ptr m, float x, float y, float z);
void _mtx4dTranslate_vfpv3(mtx4d_ptr m, double x, double y, double z);
void _mtx4fRotate_vfpv3(mtx4f_ptr mtx, float angle_rad, float x, float y, float z);
void _mtx4dRotate_vfpv3(mtx4d_ptr mtx, double angle_rad, double x, double y, double z);

/* NEON */
float _vec3fMagnitude_neon(const vec3f_ptr v);
float _vec3fMagnitudeSquared_neon(const vec3f_ptr v);
float _vec3fDotProduct_neon(const vec3f_ptr v1, const vec3f_ptr v2);
void _vec3fCrossProduct_neon(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2);
void _vec4fCopy_neon(vec4f_ptr d, const vec4f_ptr v);
void _vec4fMulvec4_neon(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2);
void _vec4fMatrix4_neon(vec4f_ptr d, const vec4f_ptr v, const mtx4f_ptr m);
void _pt4fMatrix4_neon(vec4f_ptr d, const vec4f_ptr p, const mtx4f_ptr m);
void _mtx4fMul_neon(mtx4f_ptr d, const mtx4f_ptr m1, const mtx4f_ptr m2);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_SIMD3D_H */

