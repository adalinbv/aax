/*
 * geometry.h : math related function definitions.
 *
 * Copyright (C) 2005, 2006 by Erik Hofman <erik@ehofman.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 *
 * $Id: Exp $
 *
 */

#ifndef _GEOMETRY_H
#define _GEOMETRY_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include <aax/aax.h>

#include "types.h"

#define GMATH_4PI		 12.56637061435917246399f
#define GMATH_2PI		  6.28318530717958647692f
#define GMATH_PI		  3.14159265358979323846f
#define GMATH_PI_2		  1.57079632679489661923f
#define GMATH_1_PI_2              0.63661977236758138243f
#define DIV4_GMATH_PI             1.27323950930404539150f
#define DIV4_GMATH_PI2           -0.40528474649314960576f
#define GMATH_E1		  2.71828182845904509080f
#define GMATH_SQRT_2		  1.41421356237309514547f

#define GMATH_DEG_TO_RAD_2	  0.00872664625997164774f
#define GMATH_DEG_TO_RAD	  0.01745329251994329547f
#define GMATH_RAD_TO_DEG	 57.29577951308232286465f
#define GMATH_RAD_TO_DEG2	114.59155902616464572930f


typedef ALIGN16 float vec3_t[4] ALIGN16C;
typedef ALIGN16 float mtx3_t[4][4] ALIGN16C;

typedef ALIGN16 int32_t ivec4_t[4] ALIGN16C;
typedef ALIGN16 float vec4_t[4] ALIGN16C;
typedef ALIGN16 float mtx4_t[4][4] ALIGN16C;

typedef ALIGN16 double vec3d_t[4] ALIGN16C;
typedef ALIGN16 double mtx4d_t[4][4] ALIGN16C;

extern aaxVec4f aaxZeroVector;
extern aaxVec4f aaxAxisUnitVec;
extern aaxMtx4f aaxIdentityMatrix;
extern aaxMtx4d aaxIdentityMatrix64;

typedef void (*vec3Copy_proc)(vec3_t d, const vec3_t v);
typedef void (*vec3Mulvec3_proc)(vec3_t r, const vec3_t v1, const vec3_t v2);

extern vec3Copy_proc vec3Copy;
extern vec3Mulvec3_proc vec3Mulvec3;
void vec3Negate(vec3_t d, const vec3_t v);
//void vec3Matrix3(vec3_t d, const vec3_t v, const mtx3_t m);

void mtx3Copy(mtx3_t d, mtx3_t m);

typedef float (*vec3Magnitude_proc)(const vec3_t v);
typedef float (*vec3MagnitudeSquared_proc)(const vec3_t v);
typedef float (*vec3DotProduct_proc)(const vec3_t v1, const vec3_t v2);
typedef float (*vec3Normalize_proc)(vec3_t d, const vec3_t v);
typedef void (*vec3CrossProduct_proc)(vec3_t d, const vec3_t v1, const vec3_t v2);

typedef double (*vec3dMagnitude_proc)(const vec3d_t v);
typedef double (*vec3dDotProduct_proc)(const vec3d_t v1, const vec3d_t v2);
typedef double (*vec3dNormalize_proc)(vec3d_t d, const vec3d_t v);

extern vec3Magnitude_proc vec3Magnitude;
extern vec3dMagnitude_proc vec3dMagnitude;
extern vec3MagnitudeSquared_proc vec3MagnitudeSquared;
extern vec3DotProduct_proc vec3DotProduct;
extern vec3dDotProduct_proc vec3dDotProduct;
extern vec3Normalize_proc vec3Normalize;
extern vec3dNormalize_proc vec3dNormalize;
extern vec3CrossProduct_proc vec3CrossProduct;

typedef void (*vec4Copy_proc)(vec4_t d, const vec4_t v);
typedef void (*vec4Mulvec4_proc)(vec4_t r, const vec4_t v1, const vec4_t v2);
typedef void (*vec4Matrix4_proc)(vec4_t d, const vec4_t v, const mtx4_t m);

extern vec4Copy_proc vec4Copy;
extern vec4Mulvec4_proc vec4Mulvec4;
extern vec4Matrix4_proc vec4Matrix4;
extern vec4Matrix4_proc pt4Matrix4;

void vec4ScalarMul(vec4_t r, float v);
void vec4Negate(vec4_t d, const vec4_t v);

typedef void (*mtx4Mul_proc)(mtx4_t d, const mtx4_t m1, const mtx4_t m2);
typedef void (*mtx4dMul_proc)(mtx4d_t d, const mtx4d_t m1, const mtx4d_t m2);

extern mtx4Mul_proc mtx4Mul;
extern mtx4dMul_proc mtx4dMul;

//void mtx4MulVec4(vec4_t d, mtx4_t m, const vec4_t v);
void mtx4Translate(mtx4_t m, float x, float y, float z);
void mtx4Rotate(mtx4_t m, float angle, float x, float y, float z);
void mtx4InverseSimple(mtx4_t d, const mtx4_t m);
void mtx4Copy(mtx4_t d, const void *);

void mtx4dCopy(mtx4d_t d, const void *);
void mtx4dTranslate(mtx4d_t m, double x, double y, double z);
void mtx4dRotate(mtx4d_t m, double angle, double x, double y, double z);
void mtx4dInverseSimple(mtx4d_t d, mtx4d_t m);

typedef void (*ivec4Copy_proc)(ivec4_t d, const ivec4_t v);
typedef void (*ivec4Mulivec4_proc)(ivec4_t r, const ivec4_t v1, const ivec4_t v2);

extern ivec4Copy_proc ivec4Copy;
extern ivec4Mulivec4_proc ivec4Mulivec4;

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_GEOMETRY_H */

