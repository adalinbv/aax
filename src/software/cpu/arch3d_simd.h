/*
 * Copyright 2005-2018 by Erik Hofman.
 * Copyright 2009-2018 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
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

/* SSE*/
float _vec3fMagnitude_sse(const vec3f_ptr v);
float _vec3fMagnitudeSquared_sse(const vec3f_ptr v);
float _vec3fDotProduct_sse(const vec3f_ptr v1, const vec3f_ptr v2);
void _vec3fCrossProduct_sse(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2);
void _vec3fAbsolute_sse(vec3f_ptr d, const vec3f_ptr v);
void _vec4fCopy_sse(vec4f_ptr d, const vec4f_ptr v);
void _vec4fMulVec4_sse(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2);
void _mtx4fMul_sse(mtx4f_ptr d, const mtx4f_ptr m1, const mtx4f_ptr m2);
void _mtx4fMulVec4_sse(vec4f_ptr d, const mtx4f_ptr m, const vec4f_ptr v);
int _vec3fAltitudeVector_sse(vec3f_ptr vres, const mtx4f_ptr ifmtx, const vec3f_ptr ppos, const vec3f_ptr epos, const vec3f_ptr fevec, vec3f_ptr fpvec);

/* SSE2 */
void _mtx4dMul_sse2(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2);
void _mtx4dMulVec4_sse2(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr v);
int _vec3dAltitudeVector_sse2(vec3f_ptr altvec, const mtx4d_ptr ifmtx, const vec3d_ptr ppos, const vec3d_ptr epos, const vec3f_ptr afevec, vec3f_ptr fpvec);

/* SSE3 */
float _vec3fMagnitude_sse3(const vec3f_ptr v);
float _vec3fMagnitudeSquared_sse3(const vec3f_ptr v);
float _vec3fDotProduct_sse3(const vec3f_ptr v1, const vec3f_ptr v2);

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
void _vec3fAbsolute_sse_vex(vec3f_ptr d, const vec3f_ptr v);
void _vec4fCopy_sse_vex(vec4f_ptr d, const vec4f_ptr v);
void _vec4fMulVec4_sse_vex(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2);
void _mtx4fMul_sse_vex(mtx4f_ptr d, const mtx4f_ptr m1, const mtx4f_ptr m2);
void _mtx4fMulVec4_sse_vex(vec4f_ptr d, const mtx4f_ptr m, const vec4f_ptr v);
int _vec3fAltitudeVector_sse_vex(vec3f_ptr vres, const mtx4f_ptr ifmtx, const vec3f_ptr ppos, const vec3f_ptr epos, const vec3f_ptr fevec, vec3f_ptr fpvec);

/* AVX */
void _mtx4dMul_avx(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2);
void _mtx4dMulVec4_avx(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr v);
int _vec3dAltitudeVector_avx(vec3f_ptr altvec, const mtx4d_ptr ifmtx, const vec3d_ptr ppos, const vec3d_ptr epos, const vec3f_ptr afevec, vec3f_ptr fpvec);

/* VFPV3 */
void _vec3fNegate_vfpv3(vec3f_ptr d, const vec3f_ptr v);
void _vec4fNegate_vfpv3(vec4f_ptr d, const vec4f_ptr v);
void _vec4fScalarMul_vfpv3(vec4f_ptr r, float f);
void _vec3fMulVec3_vfpv3(vec3f_ptr r, const vec3f_ptr v1, const vec3f_ptr v2);
void _vec4fMulVec4_vfpv3(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2);
float _vec3fMagnitude_vfpv3(const vec3f_ptr v);
double _vec3dMagnitude_vfpv3(const vec3d_ptr v);
float _vec3fMagnitudeSquared_vfpv3(const vec3f_ptr v);
float _vec3fDotProduct_vfpv3(const vec3f_ptr v1, const vec3f_ptr v2);
double _vec3dDotProduct_vfpv3(const vec3d_ptr v1, const vec3d_ptr v2);
void _vec3fCrossProduct_vfpv3(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2);
float _vec3fNormalize_vfpv3(vec3f_ptr d, const vec3f_ptr v);
double _vec3dNormalize_vfpv3(vec3d_ptr d, const vec3d_ptr v);
//void _vec3fMatrix3_vfpv3(vec3f_ptr d, const vec3f_ptr v, mtx3 m);
void _mtx4fSetAbsolute_vfpv3(mtx4f_ptr d, char absolute);
void _mtx4fMul_vfpv3(mtx4f_ptr dst, const mtx4f_ptr mtx1, const mtx4f_ptr mtx2);
void _mtx4dMul_vfpv3(mtx4d_ptr dst, const mtx4d_ptr mtx1, const mtx4d_ptr mtx2);
void _mtx4fMulVec4_vfpv3(vec4f_ptr d, const mtx4f_ptr m, const vec4f_ptr v);
void _mtx4dMulVec4_vfpv3(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr v);
void _mtx4fInverseSimple_vfpv3(mtx4f_ptr dst, const mtx4f_ptr mtx);
void _mtx4dInverseSimple_vfpv3(mtx4d_ptr dst, const mtx4d_ptr mtx);
void _mtx4fTranslate_vfpv3(mtx4f_ptr m, float x, float y, float z);
void _mtx4dTranslate_vfpv3(mtx4d_ptr m, double x, double y, double z);
void _mtx4fRotate_vfpv3(mtx4f_ptr mtx, float angle_rad, float x, float y, float z);
void _mtx4dRotate_vfpv3(mtx4d_ptr mtx, double angle_rad, double x, double y, double z);

/* VFPV4 */
void _vec3fNegate_vfpv4(vec3f_ptr d, const vec3f_ptr v);
void _vec4fNegate_vfpv4(vec4f_ptr d, const vec4f_ptr v);
void _vec4fScalarMul_vfpv4(vec4f_ptr r, float f);
void _vec3fMulVec3_vfpv4(vec3f_ptr r, const vec3f_ptr v1, const vec3f_ptr v2);
void _vec4fMulVec4_vfpv4(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2);
float _vec3fMagnitude_vfpv4(const vec3f_ptr v);
double _vec3dMagnitude_vfpv4(const vec3d_ptr v);
float _vec3fMagnitudeSquared_vfpv4(const vec3f_ptr v);
float _vec3fDotProduct_vfpv4(const vec3f_ptr v1, const vec3f_ptr v2);
double _vec3dDotProduct_vfpv4(const vec3d_ptr v1, const vec3d_ptr v2);
void _vec3fCrossProduct_vfpv4(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2);
float _vec3fNormalize_vfpv4(vec3f_ptr d, const vec3f_ptr v);
double _vec3dNormalize_vfpv4(vec3d_ptr d, const vec3d_ptr v);
//void _vec3fMatrix3_vfpv4(vec3f_ptr d, const vec3f_ptr v, mtx3 m);
void _mtx4fSetAbsolute_vfpv4(mtx4f_ptr d, char absolute);
void _mtx4fMul_vfpv4(mtx4f_ptr dst, const mtx4f_ptr mtx1, const mtx4f_ptr mtx2);
void _mtx4dMul_vfpv4(mtx4d_ptr dst, const mtx4d_ptr mtx1, const mtx4d_ptr mtx2);
void _mtx4fMulVec4_vfpv4(vec4f_ptr d, const mtx4f_ptr m, const vec4f_ptr v);
void _mtx4dMulVec4_vfpv4(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr v);
void _mtx4fInverseSimple_vfpv4(mtx4f_ptr dst, const mtx4f_ptr mtx);
void _mtx4dInverseSimple_vfpv4(mtx4d_ptr dst, const mtx4d_ptr mtx);
void _mtx4fTranslate_vfpv4(mtx4f_ptr m, float x, float y, float z);
void _mtx4dTranslate_vfpv4(mtx4d_ptr m, double x, double y, double z);
void _mtx4fRotate_vfpv4(mtx4f_ptr mtx, float angle_rad, float x, float y, float z);
void _mtx4dRotate_vfpv4(mtx4d_ptr mtx, double angle_rad, double x, double y, double z);

/* NEON */
float _vec3fMagnitude_neon(const vec3f_ptr v);
float _vec3fMagnitudeSquared_neon(const vec3f_ptr v);
float _vec3fDotProduct_neon(const vec3f_ptr v1, const vec3f_ptr v2);
void _vec3fCrossProduct_neon(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2);
void _vec4fCopy_neon(vec4f_ptr d, const vec4f_ptr v);
void _vec4fMulVec4_neon(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2);
void _mtx4fMul_neon(mtx4f_ptr d, const mtx4f_ptr m1, const mtx4f_ptr m2);
void _mtx4fMulVec4_neon(vec4f_ptr d, const mtx4f_ptr m, const vec4f_ptr v);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_SIMD3D_H */

