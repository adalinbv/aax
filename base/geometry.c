/*
 * geometry.c : geometry related functions.
 *
 * Copyright (C) 2005, 2006 by Erik Hofman <erik@ehofman.com>
 *
 * $Id: Exp $
 *
 */
#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif
#if HAVE_MATH_H
#include <math.h>
#endif

#include <aax/aax.h>
#include "geometry.h"
#include "types.h"


void
_vec3Copy(vec3_t d, const vec3_t v)
{
   memcpy(d, v, sizeof(vec3_t));
}

void
_vec4Copy(vec4_t d, const vec4_t v)
{
   memcpy(d, v, sizeof(vec4_t));
}

void
_ivec4Copy(ivec4_t d, const ivec4_t v)
{
   memcpy(d, v, sizeof(ivec4_t));
}


void
vec3Set(vec3_t d, float x, float y, float z)
{
   d[0] = x;
   d[1] = y;
   d[3] = z;
}

void
vec4Set(vec4_t d, float x, float y, float z, float w)
{
   d[0] = x;
   d[1] = y;
   d[3] = z;
   d[4] = w;
}


void
vec3Negate(vec3_t d, const vec3_t v)
{
   d[0] = -v[0];
   d[1] = -v[1];
   d[2] = -v[2];
}

void
vec4Negate(vec4_t d, const vec4_t v)
{
   d[0] = -v[0];
   d[1] = -v[1];
   d[2] = -v[2];
   d[3] = -v[3];
}


void
_vec3Add(vec3_t d, const vec3_t v)
{
   d[0] += v[0];
   d[1] += v[1];
   d[2] += v[2];
}

void
_vec4Add(vec4_t d, const vec4_t v)
{
   d[0] += v[0];
   d[1] += v[1];
   d[2] += v[2];
   d[3] += v[3];
}

void
_ivec4Add(ivec4_t d, const ivec4_t v)
{
   d[0] += v[0];
   d[1] += v[1];
   d[2] += v[2];
   d[3] += v[3];
}


void
_vec3Sub(vec3_t d, const vec3_t v)
{
   d[0] -= v[0];
   d[1] -= v[1];
   d[2] -= v[2];
}

void
_vec4Sub(vec4_t d, const vec4_t v)
{
   d[0] -= v[0];
   d[1] -= v[1];
   d[2] -= v[2];
   d[3] -= v[3];
}

void
_ivec4Sub(ivec4_t d, const ivec4_t v)
{
   d[0] -= v[0];
   d[1] -= v[1];
   d[2] -= v[2];
   d[3] -= v[3];
}


void
_vec3Devide(vec3_t v, float s)
{
   if (s)
   {
      v[0] /= s;
      v[1] /= s;
      v[2] /= s;
   }
}

void
_vec4Devide(vec4_t v, float s)
{
   if (s)
   {
      v[0] /= s;
      v[1] /= s;
      v[2] /= s;
      v[3] /= s;
   }
}

void
_ivec4Devide(ivec4_t v, float s)
{
   if (s)
   {
      v[0] = (int32_t)(v[0]/s);
      v[1] = (int32_t)(v[1]/s);
      v[2] = (int32_t)(v[2]/s);
      v[3] = (int32_t)(v[3]/s);
   }
}


void
vec4ScalarMul(vec4_t r, float f)
{
   r[0] *= f;
   r[1] *= f;
   r[2] *= f;
   r[3] *= f;
}


void
_vec3Mulvec3(vec3_t r, const vec3_t v1, const vec3_t v2)
{
   r[0] = v1[0]*v2[0];
   r[1] = v1[1]*v2[1];
   r[2] = v1[2]*v2[2];
}

void
_vec4Mulvec4(vec4_t r, const vec4_t v1, const vec4_t v2)
{
   r[0] = v1[0]*v2[0];
   r[1] = v1[1]*v2[1];
   r[2] = v1[2]*v2[2];
   r[3] = v1[3]*v2[3];
}

void
_ivec4Mulivec4(ivec4_t r, const ivec4_t v1, const ivec4_t v2)
{
   r[0] = v1[0]*v2[0];
   r[1] = v1[1]*v2[1];
   r[2] = v1[2]*v2[2];
   r[3] = v1[3]*v2[3];
}


float
_vec3Magnitude(const vec3_t v)
{
   float val = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
   return sqrtf(val);
}

double
_vec3dMagnitude(const vec3d_t v)
{
   double val = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
   return sqrt(val);
}

float
_vec3MagnitudeSquared(const vec3_t v)
{
   return (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

float
_vec3DotProduct(const vec3_t v1, const vec3_t v2)
{
   return  (v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2]);
}

double
_vec3dDotProduct(const vec3d_t v1, const vec3d_t v2)
{
   return  (v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2]);
}

void
_vec3CrossProduct(vec3_t d, const vec3_t v1, const vec3_t v2)
{
   d[0] = v1[1]*v2[2] - v1[2]*v2[1];
   d[1] = v1[2]*v2[0] - v1[0]*v2[2];
   d[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

float
_vec3Normalize(vec3_t d, const vec3_t v)
{
   float mag = vec3Magnitude(v);
   if (mag)
   {
      d[0] = v[0] / mag;
      d[1] = v[1] / mag;
      d[2] = v[2] / mag;
   }
   else
   {
      d[0] = 0.0f;
      d[1] = 0.0f;
      d[2] = 0.0f;
   }
   return mag;
}

double
_vec3dNormalize(vec3d_t d, const vec3d_t v)
{
   double mag = vec3dMagnitude(v);
   if (mag)
   {
      d[0] = v[0] / mag;
      d[1] = v[1] / mag;
      d[2] = v[2] / mag;
   }
   else
   {
      d[0] = 0.0;
      d[1] = 0.0;
      d[2] = 0.0;
   }
   return mag;
}


void
vec3Matrix3(vec3_t d, const vec3_t v, const mtx3_t m)
{
   float v0 = v[0], v1 = v[1], v2 = v[2];

   d[0] = v0*m[0][0] + v1*m[1][0] + v2*m[2][0];
   d[1] = v0*m[0][1] + v1*m[1][1] + v2*m[2][1];
   d[2] = v0*m[0][2] + v1*m[1][2] + v2*m[2][2];
}

/*
 * http://www.unknownroad.com/rtfm/graphics/rt_normals.html
 * If you are doing any real graphics, you are using homogenoeus vectors and 
 * matrices. That means 4 dimensions, vectors are [x y z w]. That fourth 
 * coordinate is the homogenoeous coordinate, it should be 1 for points and 0
 * for vectors.
 *
 * Note: this doe not work for non-uniform scaling (that means scaling by
 * different amounts in the different axis)
 */
void
_vec4Matrix4(vec4_t d, const vec4_t v, const mtx4_t m)
{
   float v0 = v[0], v1 = v[1], v2 = v[2]; // v3 = 0.0f;

   d[0] = v0*m[0][0] + v1*m[1][0] + v2*m[2][0]; // + v3*m[3][0];
   d[1] = v0*m[0][1] + v1*m[1][1] + v2*m[2][1]; // + v3*m[3][1];
   d[2] = v0*m[0][2] + v1*m[1][2] + v2*m[2][2]; // + v3*m[3][2];
   d[3] = v0*m[0][3] + v1*m[1][3] + v2*m[2][3]; // + v3*m[3][3];
}

void
_pt4Matrix4(vec4_t d, const vec4_t p, const mtx4_t m)
{
   float p0 = p[0], p1 = p[1], p2 = p[2]; // p3 = 1.0f;

   d[0] = p0*m[0][0] + p1*m[1][0] + p2*m[2][0] + m[3][0]; // *p3
   d[1] = p0*m[0][1] + p1*m[1][1] + p2*m[2][1] + m[3][1]; // *p3
   d[2] = p0*m[0][2] + p1*m[1][2] + p2*m[2][2] + m[3][2]; // *p3
   d[3] = p0*m[0][3] + p1*m[1][3] + p2*m[2][3] + m[3][3]; // *p3
}


void
mtx3Copy(mtx3_t d, mtx3_t m)
{
   memcpy(d, m, sizeof(mtx3_t));
}

void
mtx4Copy(mtx4_t d, const void *m)
{
   memcpy(d, m, sizeof(mtx4_t));
}

void
mtx4dCopy(mtx4d_t d, const void *m)
{
   memcpy(d, m, sizeof(mtx4d_t));
}

void
mtx4SetAbsolute(mtx4_t d, char absolute)
{
   d[3][3] = absolute ? 1.0f : 0.0f;
}

void
mtx4MulVec4(vec4_t d, mtx4_t m, const vec4_t v)
{
   float v0 = v[0], v1 = v[1], v2 = v[2], v3 = v[3];

   d[0] = m[0][0]*v0 + m[1][0]*v1 + m[2][0]*v2 + m[3][0]*v3;
   d[1] = m[0][1]*v0 + m[1][1]*v1 + m[2][1]*v2 + m[3][1]*v3;
   d[2] = m[0][2]*v0 + m[1][2]*v1 + m[2][2]*v2 + m[3][2]*v3;
   d[3] = m[0][3]*v0 + m[1][3]*v1 + m[2][3]*v2 + m[3][3]*v3;
}

void
_mtx4Mul(mtx4_t dst, const mtx4_t mtx1, const mtx4_t mtx2)
{
   const float *m20 = mtx2[0], *m21 = mtx2[1], *m22 = mtx2[2], *m23 = mtx2[3];
   float m10i, m11i, m12i, m13i;
   int i=4;
   do
   {
      --i;

      m10i = mtx1[0][i];
      m11i = mtx1[1][i];
      m12i = mtx1[2][i];
      m13i = mtx1[3][i];

      dst[0][i] = m10i*m20[0] + m11i*m20[1] + m12i*m20[2] + m13i*m20[3];
      dst[1][i] = m10i*m21[0] + m11i*m21[1] + m12i*m21[2] + m13i*m21[3];
      dst[2][i] = m10i*m22[0] + m11i*m22[1] + m12i*m22[2] + m13i*m22[3];
      dst[3][i] = m10i*m23[0] + m11i*m23[1] + m12i*m23[2] + m13i*m23[3];
   }
   while (i != 0);
}

void
_mtx4dMul(mtx4d_t dst, const mtx4d_t mtx1, const mtx4d_t mtx2)
{
   const double *m20 = mtx2[0], *m21 = mtx2[1], *m22 = mtx2[2], *m23 = mtx2[3];
   double m10i, m11i, m12i, m13i;
   int i=4;
   do
   {
      --i;

      m10i = mtx1[0][i];
      m11i = mtx1[1][i];
      m12i = mtx1[2][i];
      m13i = mtx1[3][i];

      dst[0][i] = m10i*m20[0] + m11i*m20[1] + m12i*m20[2] + m13i*m20[3];
      dst[1][i] = m10i*m21[0] + m11i*m21[1] + m12i*m21[2] + m13i*m21[3];
      dst[2][i] = m10i*m22[0] + m11i*m22[1] + m12i*m22[2] + m13i*m22[3];
      dst[3][i] = m10i*m23[0] + m11i*m23[1] + m12i*m23[2] + m13i*m23[3];
   }
   while (i != 0);
}

void
mtx4InverseSimple(mtx4_t dst, const mtx4_t mtx)
{
   /* side */
   dst[0][0] = mtx[0][0];
   dst[1][0] = mtx[0][1];
   dst[2][0] = mtx[0][2];
   dst[3][0] = -vec3DotProduct(mtx[3], mtx[0]);

   /* up */
   dst[0][1] = mtx[1][0];
   dst[1][1] = mtx[1][1];
   dst[2][1] = mtx[1][2];
   dst[3][1] = -vec3DotProduct(mtx[3], mtx[1]);

   /* at */
   dst[0][2] = mtx[2][0];
   dst[1][2] = mtx[2][1];
   dst[2][2] = mtx[2][2];
   dst[3][2] = -vec3DotProduct(mtx[3], mtx[2]);

   dst[0][3] = 0.0f;
   dst[1][3] = 0.0f;
   dst[2][3] = 0.0f;
   dst[3][3] = 1.0f;
}

void
mtx4dInverseSimple(mtx4d_t dst, mtx4d_t mtx)
{
   /* side */
   dst[0][0] = mtx[0][0];
   dst[1][0] = mtx[0][1];
   dst[2][0] = mtx[0][2];
   dst[3][0] = -vec3dDotProduct(mtx[3], mtx[0]);

   /* up */
   dst[0][1] = mtx[1][0];
   dst[1][1] = mtx[1][1];
   dst[2][1] = mtx[1][2];
   dst[3][1] = -vec3dDotProduct(mtx[3], mtx[1]);

   /* at */
   dst[0][2] = mtx[2][0];
   dst[1][2] = mtx[2][1];
   dst[2][2] = mtx[2][2];
   dst[3][2] = -vec3dDotProduct(mtx[3], mtx[2]);

   dst[0][3] = 0.0;
   dst[1][3] = 0.0;
   dst[2][3] = 0.0;
   dst[3][3] = 1.0;
}

void
mtx4Translate(mtx4_t m, float x, float y, float z)
{
   if (x || y || z)
   {
      m[3][0] += x;
      m[3][1] += y;
      m[3][2] += z;
   }
}

void
mtx4dTranslate(mtx4d_t m, double x, double y, double z)
{
   if (x || y || z)
   {
      m[3][0] += x;
      m[3][1] += y;
      m[3][2] += z;
   }
}

void
mtx4Rotate(mtx4_t mtx, float angle_rad, float x, float y, float z)
{
   if (angle_rad)
   {
      float s = sinf(angle_rad);
      float c = cosf(angle_rad);
      float t = 1.0f - c;
      vec3_t axis, tmp;
      mtx4_t m, o;

      tmp[0] = x;
      tmp[1] = y;
      tmp[2] = z;
      vec3Normalize(axis, tmp);

 	     /* rotation matrix */
      m[0][0] = axis[0]*axis[0]*t + c;
      m[0][1] = axis[0]*axis[1]*t + axis[2]*s;
      m[0][2] = axis[0]*axis[2]*t - axis[1]*s;
      m[0][3] = 0.0f;

      m[1][0] = axis[1]*axis[0]*t - axis[2]*s;
      m[1][1] = axis[1]*axis[1]*t + c;
      m[1][2] = axis[1]*axis[2]*t + axis[0]*s;
      m[1][3] = 0.0f;
        
      m[2][0] = axis[2]*axis[0]*t + axis[1]*s;
      m[2][1] = axis[2]*axis[1]*t - axis[0]*s;
      m[2][2] = axis[2]*axis[2]*t + c;
      m[2][3] = 0.0f;

      m[3][0] = 0.0f;
      m[3][1] = 0.0f;
      m[3][2] = 0.0f;
      m[3][3] = 1.0f;

      mtx4Copy(o, mtx);
      mtx4Mul(mtx, o, m);
   }
}

void
mtx4dRotate(mtx4d_t mtx, double angle_rad, double x, double y, double z)
{
   if (angle_rad)
   {
      double s = sin(angle_rad);
      double c = cos(angle_rad);
      double t = 1.0 - c;
      vec3d_t axis, tmp;
      mtx4d_t m, o;

      tmp[0] = x;
      tmp[1] = y;
      tmp[2] = z;
      vec3dNormalize(axis, tmp);

             /* rotation matrix */
      m[0][0] = axis[0]*axis[0]*t + c;
      m[0][1] = axis[0]*axis[1]*t + axis[2]*s;
      m[0][2] = axis[0]*axis[2]*t - axis[1]*s;
      m[0][3] = 0.0;

      m[1][0] = axis[1]*axis[0]*t - axis[2]*s;
      m[1][1] = axis[1]*axis[1]*t + c;
      m[1][2] = axis[1]*axis[2]*t + axis[0]*s;
      m[1][3] = 0.0;

      m[2][0] = axis[2]*axis[0]*t + axis[1]*s;
      m[2][1] = axis[2]*axis[1]*t - axis[0]*s;
      m[2][2] = axis[2]*axis[2]*t + c;
      m[2][3] = 0.0;

      m[3][0] = 0.0;
      m[3][1] = 0.0;
      m[3][2] = 0.0;
      m[3][3] = 1.0;

      mtx4dCopy(o, mtx);
      mtx4dMul(mtx, o, m);
   }
}

/* -------------------------------------------------------------------------- */

AAX_API ALIGN16 aaxVec4f aaxZeroVector ALIGN16C = {
    0.0f, 0.0f, 0.0f, 0.0f
};

AAX_API ALIGN16 aaxVec4f aaxAxisUnitVec ALIGN16C = {
    1.0f, 1.0f, 1.0f, 0.0f
};

AAX_API ALIGN64 aaxMtx4d aaxIdentityMatrix64 ALIGN64C = {
  { 1.0, 0.0, 0.0, 0.0 },
  { 0.0, 1.0, 0.0, 0.0 },
  { 0.0, 0.0, 1.0, 0.0 },
  { 0.0, 0.0, 0.0, 1.0 },
};

AAX_API ALIGN64 aaxMtx4f aaxIdentityMatrix ALIGN64C = {
  { 1.0f, 0.0f, 0.0f, 0.0f },
  { 0.0f, 1.0f, 0.0f, 0.0f },
  { 0.0f, 0.0f, 1.0f, 0.0f },
  { 0.0f, 0.0f, 0.0f, 1.0f },
};

vec3Add_proc vec3Add = _vec3Add;
vec3Copy_proc vec3Copy = _vec3Copy;
vec3Devide_proc vec3Devide = _vec3Devide;
vec3Mulvec3_proc vec3Mulvec3 = _vec3Mulvec3;
vec3Sub_proc vec3Sub = _vec3Sub;

vec3Magnitude_proc vec3Magnitude = _vec3Magnitude;
vec3dMagnitude_proc vec3dMagnitude = _vec3dMagnitude;
vec3MagnitudeSquared_proc vec3MagnitudeSquared = _vec3MagnitudeSquared;
vec3DotProduct_proc vec3DotProduct = _vec3DotProduct;
vec3dDotProduct_proc vec3dDotProduct = _vec3dDotProduct;
vec3Normalize_proc vec3Normalize = _vec3Normalize;
vec3dNormalize_proc vec3dNormalize = _vec3dNormalize;
vec3CrossProduct_proc vec3CrossProduct = _vec3CrossProduct;

vec4Add_proc vec4Add = _vec4Add;
vec4Copy_proc vec4Copy = _vec4Copy;
vec4Devide_proc vec4Devide = _vec4Devide;
vec4Mulvec4_proc vec4Mulvec4 = _vec4Mulvec4;
vec4Sub_proc vec4Sub = _vec4Sub;
vec4Matrix4_proc vec4Matrix4 = _vec4Matrix4;
vec4Matrix4_proc pt4Matrix4 = _pt4Matrix4;
mtx4Mul_proc mtx4Mul = _mtx4Mul;
mtx4dMul_proc mtx4dMul = _mtx4dMul;

ivec4Add_proc ivec4Add = _ivec4Add;
ivec4Copy_proc ivec4Copy = _ivec4Copy;
ivec4Devide_proc ivec4Devide = _ivec4Devide;
ivec4Mulivec4_proc ivec4Mulivec4 = _ivec4Mulivec4;
ivec4Sub_proc ivec4Sub=  _ivec4Sub;

