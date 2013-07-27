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

#if HAVE_MATH_H
#include <math.h>
#endif
#include <string.h>


#include "geometry.h"


void
_vec3Copy(vec3 d, const vec3 v)
{
   memcpy(d, v, sizeof(vec3));
}

void
_vec4Copy(vec4 d, const vec4 v)
{
   memcpy(d, v, sizeof(vec4));
}

void
_ivec4Copy(ivec4 d, const ivec4 v)
{
   memcpy(d, v, sizeof(ivec4));
}


void
vec3Set(vec3 d, float x, float y, float z)
{
   d[0] = x;
   d[1] = y;
   d[3] = z;
}

void
vec4Set(vec4 d, float x, float y, float z, float w)
{
   d[0] = x;
   d[1] = y;
   d[3] = z;
   d[4] = w;
}


void
vec3Negate(vec3 d, const vec3 v)
{
   d[0] = -v[0];
   d[1] = -v[1];
   d[2] = -v[2];
}

void
vec4Negate(vec4 d, const vec4 v)
{
   d[0] = -v[0];
   d[1] = -v[1];
   d[2] = -v[2];
   d[3] = -v[3];
}


void
_vec3Add(vec3 d, vec3 v)
{
   d[0] += v[0];
   d[1] += v[1];
   d[2] += v[2];
}

void
_vec4Add(vec4 d, const vec4 v)
{
   d[0] += v[0];
   d[1] += v[1];
   d[2] += v[2];
   d[3] += v[3];
}

void
_ivec4Add(ivec4 d, ivec4 v)
{
   d[0] += v[0];
   d[1] += v[1];
   d[2] += v[2];
   d[3] += v[3];
}


void
_vec3Sub(vec3 d, vec3 v)
{
   d[0] -= v[0];
   d[1] -= v[1];
   d[2] -= v[2];
}

void
_vec4Sub(vec4 d, const vec4 v)
{
   d[0] -= v[0];
   d[1] -= v[1];
   d[2] -= v[2];
   d[3] -= v[3];
}

void
_ivec4Sub(ivec4 d, ivec4 v)
{
   d[0] -= v[0];
   d[1] -= v[1];
   d[2] -= v[2];
   d[3] -= v[3];
}


void
_vec3Devide(vec3 v, float s)
{
   if (s)
   {
      v[0] /= s;
      v[1] /= s;
      v[2] /= s;
   }
}

void
_vec4Devide(vec4 v, float s)
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
_ivec4Devide(ivec4 v, float s)
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
vec4ScalarMul(vec4 r, float f)
{
   r[0] *= f;
   r[1] *= f;
   r[2] *= f;
   r[3] *= f;
}


void
_vec3Mulvec3(vec3 r, const vec3 v1, const vec3 v2)
{
   r[0] = v1[0]*v2[0];
   r[1] = v1[1]*v2[1];
   r[2] = v1[2]*v2[2];
}

void
_vec4Mulvec4(vec4 r, const vec4 v1, const vec4 v2)
{
   r[0] = v1[0]*v2[0];
   r[1] = v1[1]*v2[1];
   r[2] = v1[2]*v2[2];
   r[3] = v1[3]*v2[3];
}

void
_ivec4Mulivec4(ivec4 r, const ivec4 v1, const ivec4 v2)
{
   r[0] = v1[0]*v2[0];
   r[1] = v1[1]*v2[1];
   r[2] = v1[2]*v2[2];
   r[3] = v1[3]*v2[3];
}


float
_vec3Magnitude(const vec3 v)
{
   float val = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
   return sqrtf(val);
}

float
_vec3MagnitudeSquared(const vec3 v)
{
   return (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

float
_vec3DotProduct(const vec3 v1, const vec3 v2)
{
   return  (v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2]);
}

void
_vec3CrossProduct(vec3 d, const vec3 v1, const vec3 v2)
{
   d[0] = v1[1]*v2[2] - v1[2]*v2[1];
   d[1] = v1[2]*v2[0] - v1[0]*v2[2];
   d[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

float
_vec3Normalize(vec3 d, const vec3 v)
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


void
vec3Matrix3(vec3 d, const vec3 v, mtx3 m)
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
_vec4Matrix4(vec4 d, const vec4 v, mtx4 m)
{
   float v0 = v[0], v1 = v[1], v2 = v[2]; // v3 = 0.0f;

   d[0] = v0*m[0][0] + v1*m[1][0] + v2*m[2][0]; // + v3*m[3][0];
   d[1] = v0*m[0][1] + v1*m[1][1] + v2*m[2][1]; // + v3*m[3][1];
   d[2] = v0*m[0][2] + v1*m[1][2] + v2*m[2][2]; // + v3*m[3][2];
   d[3] = v0*m[0][3] + v1*m[1][3] + v2*m[2][3]; // + v3*m[3][3];
}

void
_pt4Matrix4(vec4 d, const vec4 p, mtx4 m)
{
   float p0 = p[0], p1 = p[1], p2 = p[2]; // p3 = 1.0f;

   d[0] = p0*m[0][0] + p1*m[1][0] + p2*m[2][0] + m[3][0]; // *p3
   d[1] = p0*m[0][1] + p1*m[1][1] + p2*m[2][1] + m[3][1]; // *p3
   d[2] = p0*m[0][2] + p1*m[1][2] + p2*m[2][2] + m[3][2]; // *p3
   d[3] = p0*m[0][3] + p1*m[1][3] + p2*m[2][3] + m[3][3]; // *p3
}


void
mtx3Copy(mtx3 d, mtx3 m)
{
   memcpy(d, m, sizeof(mtx3));
}

void
mtx4Copy(mtx4 d, void *m)
{
   memcpy(d, m, sizeof(mtx4));
}

void
mtx4SetAbsolute(mtx4 d, char absolute)
{
   d[3][3] = absolute ? 1.0f : 0.0f;
}

void
mtx4MulVec4(vec4 d, mtx4 m, const vec4 v)
{
   float v0 = v[0], v1 = v[1], v2 = v[2], v3 = v[3];

   d[0] = m[0][0]*v0 + m[1][0]*v1 + m[2][0]*v2 + m[3][0]*v3;
   d[1] = m[0][1]*v0 + m[1][1]*v1 + m[2][1]*v2 + m[3][1]*v3;
   d[2] = m[0][2]*v0 + m[1][2]*v1 + m[2][2]*v2 + m[3][2]*v3;
   d[3] = m[0][3]*v0 + m[1][3]*v1 + m[2][3]*v2 + m[3][3]*v3;
}

void
_mtx4Mul(mtx4 dst, mtx4 mtx1, mtx4 mtx2)
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
mtx4InverseSimple(mtx4 dst, mtx4 mtx)
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

   dst[0][3] = 0.0;
   dst[1][3] = 0.0;
   dst[2][3] = 0.0;
   dst[3][3] = 1.0f;
}

void
mtx4Translate(mtx4 m, float x, float y, float z)
{
   if (x || y || z)
   {
      m[3][0] += x;
      m[3][1] += y;
      m[3][2] += z;
   }
}

void
mtx4Rotate(mtx4 mtx, float angle_rad, float x, float y, float z)
{
   if (angle_rad)
   {
      float s = sinf(angle_rad);
      float c = cosf(angle_rad);
      float t = 1.0f - c;
      vec3 axis, tmp;
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

/* -------------------------------------------------------------------------- */

AAX_API aaxMtx4f aaxIdentityMatrix = {
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
vec3MagnitudeSquared_proc vec3MagnitudeSquared = _vec3MagnitudeSquared;
vec3DotProduct_proc vec3DotProduct = _vec3DotProduct;
vec3Normalize_proc vec3Normalize = _vec3Normalize;
vec3CrossProduct_proc vec3CrossProduct = _vec3CrossProduct;

vec4Add_proc vec4Add = _vec4Add;
vec4Copy_proc vec4Copy = _vec4Copy;
vec4Devide_proc vec4Devide = _vec4Devide;
vec4Mulvec4_proc vec4Mulvec4 = _vec4Mulvec4;
vec4Sub_proc vec4Sub = _vec4Sub;
vec4Matrix4_proc vec4Matrix4 = _vec4Matrix4;
vec4Matrix4_proc pt4Matrix4 = _pt4Matrix4;
mtx4Mul_proc mtx4Mul = _mtx4Mul;

ivec4Add_proc ivec4Add = _ivec4Add;
ivec4Copy_proc ivec4Copy = _ivec4Copy;
ivec4Devide_proc ivec4Devide = _ivec4Devide;
ivec4Mulivec4_proc ivec4Mulivec4 = _ivec4Mulivec4;
ivec4Sub_proc ivec4Sub=  _ivec4Sub;

