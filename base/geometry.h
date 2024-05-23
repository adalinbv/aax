/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef _GEOMETRY_H
#define _GEOMETRY_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include <assert.h>

#if defined _MSC_VER 
# include <intrin.h>
#elif defined __GNUC__  && (defined __x86_64__  || defined __i386__) 
# include <x86intrin.h>
#elif defined __GNUC__  && defined __ARM_NEON
# include <arm_neon.h>
#endif

#include <aax/aax.h>

#include "types.h"

#define GMATH_4PI		 12.56637061435917246399f
#define GMATH_2PI		  6.28318530717958647692f
#define GMATH_PI		  3.14159265358979323846f
#define GMATH_PI_2		  1.57079632679489661923f
#define GMATH_PI_4		  0.78539816339744827900f
#define GMATH_1_PI                0.31830987334251403809f
#define GMATH_1_PI_2              0.63661977236758138243f
#define DIV4_GMATH_PI             1.27323954473516276487f
#define DIV4_GMATH_PI2           -0.40528473456935110164f
#define GMATH_E1		  2.71828182845904509080f
#define GMATH_1_E1                0.36787944117144233402f
#define GMATH_SQRT_2		  1.41421356237309514547f

#define GMATH_DEG_TO_RAD_2	  0.00872664625997164774f
#define GMATH_DEG_TO_RAD	  0.01745329251994329547f
#define GMATH_RAD_TO_DEG	 57.29577951308232286465f
#define GMATH_RAD_TO_DEG2	114.59155902616464572930f

#define FLOAT  double
#define MTX4_t mtx4d_t

typedef ALIGN16 int32_t	ix4_t[4] ALIGN16C;
typedef ALIGN16 float	fx4_t[4] ALIGN16C;
typedef ALIGN32 double	dx4_t[4] ALIGN32C;
typedef ALIGN16 float	fx4x4_t[4][4] ALIGN16C;
typedef ALIGN32 double	dx4x4_t[4][4] ALIGN32C;

#if defined __ARM_NEON
typedef union {
   float64x4_t f64x4;
   float64x2_t f64x2[2];
} simd4d_t;
typedef float32x4_t	simd4f_t;
typedef int32x4_t	simd4i_t;
#elif defined __x86_64__ || defined __i386__
typedef union {
   __m256d avx;
   __m128d sse[2];
} simd4d_t;
typedef __m128		simd4f_t;
typedef __m128i		simd4i_t;
#else
typedef ALIGN32 dx4_t	simd4d_t ALIGN32C;
typedef ALIGN16 fx4_t	simd4f_t ALIGN16C;
typedef ALIGN16 ix4_t	simd4i_t ALIGN16C;
#endif

typedef union {
    simd4i_t s4;
    ix4_t v3;
} vec3i_t;

typedef union {
    simd4f_t s4;
    fx4_t v3;
} vec3f_t;

typedef union {
    simd4d_t s4;
    dx4_t v3;
} vec3d_t;

typedef union {
    simd4i_t s4;
    vec3i_t v3;
    ix4_t v4;
} vec4i_t;

typedef union {
    simd4f_t s4;
    vec3f_t v3;
    fx4_t v4;
} vec4f_t;

typedef union {
    simd4d_t s4;
    vec3d_t v3;
    dx4_t v4;
} vec4d_t;

typedef union {
    simd4f_t s4x4[4];
    vec3f_t v34[4];
    fx4x4_t m3 ALIGN16C;
} mtx3f_t;

typedef union {
    simd4d_t s4x4[4];
    vec3d_t v34[4];
    dx4x4_t m3;
} mtx3d_t;

typedef union {
    simd4f_t s4x4[4];
    vec3f_t v34[4];
    vec4f_t v44[4];
    fx4x4_t m4;
} mtx4f_t;

typedef union {
    simd4d_t s4x4[4];
    vec3d_t v34[4]; 
    vec4d_t v44[4];
    dx4x4_t m4;
} mtx4d_t;

typedef vec3i_t* vec3i_ptr RESTRICT;
typedef vec3f_t* vec3f_ptr RESTRICT;
typedef vec3d_t* vec3d_ptr RESTRICT;
typedef vec4i_t* vec4i_ptr RESTRICT;
typedef vec4f_t* vec4f_ptr RESTRICT;
typedef vec4d_t* vec4d_ptr RESTRICT;
typedef mtx3f_t* mtx3f_ptr RESTRICT;
typedef mtx3d_t* mtx3d_ptr RESTRICT;
typedef mtx4f_t* mtx4f_ptr RESTRICT;
typedef mtx4d_t* mtx4d_ptr RESTRICT;


typedef void (*vec3fCopy_proc)(vec3f_ptr d, const vec3f_ptr v);
typedef void (*vec3dCopy_proc)(vec3d_ptr d, const vec3d_ptr v);
typedef void (*vec3fMulVec3f_proc)(vec3f_ptr r, const vec3f_ptr v1, const vec3f_ptr v2);
typedef void (*vec3dMulVec3d_proc)(vec3d_ptr r, const vec3d_ptr v1, const vec3d_ptr v2);
typedef void (*vec3fAbsolute_proc)(vec3f_ptr d, const vec3f_ptr v);
typedef void (*vec3dAbsolute_proc)(vec3d_ptr d, const vec3d_ptr v);

extern vec3fCopy_proc vec3fCopy;
extern vec3dCopy_proc vec3dCopy;
extern vec3fMulVec3f_proc vec3fMulVec3;
extern vec3dMulVec3d_proc vec3dMulVec3;
extern vec3fAbsolute_proc vec3fAbsolute;
extern vec3dAbsolute_proc vec3dAbsolute;

void vec3dZero(vec3d_ptr d);
void vec3dFill(double d[3], double v[3]);
void vec3dFillf(double d[3], float v[3]);
void vec3dAdd(vec3d_ptr d, const vec3d_ptr v1, const vec3d_ptr v2);
void vec3dSub(vec3d_ptr d, const vec3d_ptr v1, const vec3d_ptr v2);
void vec3dScalarMul(vec3d_ptr d, const vec3d_ptr r, float v);
void vec3dNegate(vec3d_ptr d, const vec3d_ptr v);
int vec3dLessThan(const vec3d_ptr v1, const vec3d_ptr v);

void vec3fZero(vec3f_ptr f);
void vec3fFill(float d[3], float v[3]);
void vec3fFilld(float d[3], double v[3]);
void vec3fAdd(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2);
void vec3fSub(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2);
void vec3fScalarMul(vec3f_ptr d, const vec3f_ptr r, float v);
void vec3fNegate(vec3f_ptr d, const vec3f_ptr v);
int vec3fLessThan(const vec3f_ptr v1, const vec3f_ptr v);

void mtx3fCopy(mtx3f_ptr d, const mtx3f_ptr m);

typedef float (*vec3fMagnitude_proc)(const vec3f_ptr v);
typedef float (*vec3fMagnitudeSquared_proc)(const vec3f_ptr v);
typedef float (*vec3fDotProduct_proc)(const vec3f_ptr v1, const vec3f_ptr v2);
typedef float (*vec3fNormalize_proc)(vec3f_ptr d, const vec3f_ptr v);
typedef void (*vec3fCrossProduct_proc)(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2);
typedef int (*vec3fAltitudeVector_proc)(vec3f_ptr vres, const mtx4f_ptr ifmtx, const vec3f_ptr ppos, const vec3f_ptr epos, const vec3f_ptr fevec, vec3f_ptr fpvec);

typedef double (*vec3dMagnitude_proc)(const vec3d_ptr v);
typedef double (*vec3dMagnitudeSquared_proc)(const vec3d_ptr v);
typedef double (*vec3dDotProduct_proc)(const vec3d_ptr v1, const vec3d_ptr v2);
typedef double (*vec3dNormalize_proc)(vec3d_ptr d, const vec3d_ptr v);
typedef int (*vec3dAltitudeVector_proc)(vec3f_ptr vres, const mtx4d_ptr ifmtx, const vec3d_ptr ppos, const vec3d_ptr epos, const vec3f_ptr fevec, vec3f_ptr fpvec);


extern vec3fMagnitude_proc vec3fMagnitude;
extern vec3dMagnitude_proc vec3dMagnitude;
extern vec3fMagnitudeSquared_proc vec3fMagnitudeSquared;
extern vec3dMagnitudeSquared_proc vec3dMagnitudeSquared;
extern vec3fDotProduct_proc vec3fDotProduct;
extern vec3dDotProduct_proc vec3dDotProduct;
extern vec3fNormalize_proc vec3fNormalize;
extern vec3dNormalize_proc vec3dNormalize;
extern vec3fCrossProduct_proc vec3fCrossProduct;
extern vec3fAltitudeVector_proc vec3fAltitudeVector;
extern vec3dAltitudeVector_proc vec3dAltitudeVector;

typedef void (*vec4fCopy_proc)(vec4f_ptr d, const vec4f_ptr v);
typedef void (*vec4dCopy_proc)(vec4d_ptr d, const vec4d_ptr v);
typedef void (*vec4fMulVec4f_proc)(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2);

extern vec4fCopy_proc vec4fCopy;
extern vec4dCopy_proc vec4dCopy;
extern vec4fMulVec4f_proc vec4fMulVec4;

void vec4fFill(float d[4], float v[4]);
void vec4fScalarMul(vec4f_ptr d, const vec4f_ptr r, float v);
void vec4fNegate(vec4f_ptr d, const vec4f_ptr v);

typedef void (*mtx4fMul_proc)(mtx4f_ptr d, const mtx4f_ptr m1, const mtx4f_ptr m2);
typedef void (*mtx4dMul_proc)(mtx4d_ptr d, const mtx4d_ptr m1, const mtx4d_ptr m2);
typedef void (*mtx4dMulVec4_proc)(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr v);
typedef void (*mtx4fMulVec4_proc)(vec4f_ptr d, const mtx4f_ptr m, const vec4f_ptr v);
typedef void (*mtx4fCopy_proc)(mtx4f_ptr d, const mtx4f_ptr m);
typedef void (*mtx4dCopy_proc)(mtx4d_ptr d, const mtx4d_ptr m);

extern mtx4fMul_proc mtx4fMul;
extern mtx4dMul_proc mtx4dMul;
extern mtx4fMulVec4_proc mtx4fMulVec4;
extern mtx4dMulVec4_proc mtx4dMulVec4;
extern mtx4fCopy_proc mtx4fCopy;
extern mtx4dCopy_proc mtx4dCopy;

void mtx4fSetIdentity(float m[4][4]);
void mtx4fFill(float d[4][4], float m[4][4]);
void mtx4dFillf(double d[4][4], float m[4][4]);
void mtx4fFilld(float d[4][4], double m[4][4]);
void mtx4fTranspose(mtx4f_ptr d, const mtx4f_ptr m);
void mtx4fTranslate(mtx4f_ptr m, float x, float y, float z);
void mtx4fRotate(mtx4f_ptr m, float angle, float x, float y, float z);
void mtx4fInverseSimple(mtx4f_ptr d, const mtx4f_ptr m);

void mtx4dSetIdentity(double m[4][4]);
void mtx4dFill(double d[4][4], double m[4][4]);
void mtx4dTranspose(mtx4d_ptr d, const mtx4d_ptr m);
void mtx4dTranslate(mtx4d_ptr m, double x, double y, double z);
void mtx4dRotate(mtx4d_ptr m, double angle, double x, double y, double z);
void mtx4dInverseSimple(mtx4d_ptr d, const mtx4d_ptr m);

typedef void (*vec4iCopy_proc)(vec4i_ptr d, const vec4i_ptr v);
typedef void (*vec4iMulVec4if_proc)(vec4i_ptr r, const vec4i_ptr v1, const vec4i_ptr v2);

extern vec4iCopy_proc vec4iCopy;
extern vec4iMulVec4if_proc vec4iMulVec4i;


/* CPU implementTION */
void _vec3fCopy_cpu(vec3f_ptr d, const vec3f_ptr v);
void _vec3dCopy_cpu(vec3d_ptr d, const vec3d_ptr v);
void _vec3fMulVec3_cpu(vec3f_ptr r, const vec3f_ptr v1, const vec3f_ptr v2);

float _vec3fMagnitude_cpu(const vec3f_ptr v);
double _vec3dMagnitude_cpu(const vec3d_ptr v);
float _vec3fMagnitudeSquared_cpu(const vec3f_ptr v);
float _vec3fDotProduct_cpu(const vec3f_ptr v1, const vec3f_ptr v2);
double _vec3dDotProduct_cpu(const vec3d_ptr v1, const vec3d_ptr v2);
float _vec3fNormalize_cpu(vec3f_ptr d, const vec3f_ptr v);
double _vec3dNormalize_cpu(vec3d_ptr d, const vec3d_ptr v);
void _vec3fCrossProduct_cpu(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2);
void _vec3fAbsolute_cpu(vec3f_ptr d, const vec3f_ptr v);
void _vec3dAbsolute_cpu(vec3d_ptr d, const vec3d_ptr v);

void _vec4fCopy_cpu(vec4f_ptr d, const vec4f_ptr v);
void _vec4dCopy_cpu(vec4d_ptr d, const vec4d_ptr v);
void _mtx4fCopy_cpu(mtx4f_ptr d, const mtx4f_ptr m);
void _mtx4dCopy_cpu(mtx4d_ptr d, const mtx4d_ptr m);
void _vec4fMulVec4_cpu(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2);
void _mtx4fMul_cpu(mtx4f_ptr dst, const mtx4f_ptr mtx1, const mtx4f_ptr mtx2);
void _mtx4dMul_cpu(mtx4d_ptr dst, const mtx4d_ptr mtx1, const mtx4d_ptr mtx2);
void _mtx4fMulVec4_cpu(vec4f_ptr d, const mtx4f_ptr m, const vec4f_ptr v);
void _mtx4dMulVec4_cpu(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr v);

void _vec4iCopy_cpu(vec4i_ptr d, const vec4i_ptr v);
void _vec4iMulVec4i_cpu(vec4i_ptr r, const vec4i_ptr v1, const vec4i_ptr v2);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_GEOMETRY_H */

