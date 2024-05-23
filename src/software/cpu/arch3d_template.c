/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

#define __FN(NAME,ARCH) _##NAME##_##ARCH
#define FN(NAME,ARCH)   __FN(NAME,ARCH)

void
FN(vec3fNegate,A)(vec3f_ptr d, const vec3f_ptr v)
{
   d->v3[0] = -v->v3[0];
   d->v3[1] = -v->v3[1];
   d->v3[2] = -v->v3[2];
}

void
FN(vec4fNegate,A)(vec4f_ptr d, const vec4f_ptr v)
{
   d->v4[0] = -v->v4[0];
   d->v4[1] = -v->v4[1];
   d->v4[2] = -v->v4[2];
   d->v4[3] = -v->v4[3];
}


void
FN(vec4fScalarMul,A)(vec4f_ptr r, float f)
{
   r->v4[0] *= f;
   r->v4[1] *= f;
   r->v4[2] *= f;
   r->v4[3] *= f;
}


void
FN(vec3fMulVec3,A)(vec3f_ptr r, const vec3f_ptr v1, const vec3f_ptr v2)
{
   r->v3[0] = v1->v3[0]*v2->v3[0];
   r->v3[1] = v1->v3[1]*v2->v3[1];
   r->v3[2] = v1->v3[2]*v2->v3[2];
}

void
FN(vec4fMulVec4,A)(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2)
{
   r->v4[0] = v1->v4[0]*v2->v4[0];
   r->v4[1] = v1->v4[1]*v2->v4[1];
   r->v4[2] = v1->v4[2]*v2->v4[2];
   r->v4[3] = v1->v4[3]*v2->v4[3];
}

void
FN(vec4iMulVec4i,A)(vec4i_ptr r, const vec4i_ptr v1, const vec4i_ptr v2)
{
   r->v4[0] = v1->v4[0]*v2->v4[0];
   r->v4[1] = v1->v4[1]*v2->v4[1];
   r->v4[2] = v1->v4[2]*v2->v4[2];
   r->v4[3] = v1->v4[3]*v2->v4[3];
}


float
FN(vec3fMagnitude,A)(const vec3f_ptr v)
{
   float val = v->v3[0]*v->v3[0] + v->v3[1]*v->v3[1] + v->v3[2]*v->v3[2];
   return sqrtf(val);
}

double
FN(vec3dMagnitude,A)(const vec3d_ptr v)
{
   double val = v->v3[0]*v->v3[0] + v->v3[1]*v->v3[1] + v->v3[2]*v->v3[2];
   return sqrt(val);
}

float
FN(vec3fMagnitudeSquared,A)(const vec3f_ptr v)
{
   return (v->v3[0]*v->v3[0] + v->v3[1]*v->v3[1] + v->v3[2]*v->v3[2]);
}

float
FN(vec3fDotProduct,A)(const vec3f_ptr v1, const vec3f_ptr v2)
{
   return  (v1->v3[0]*v2->v3[0] + v1->v3[1]*v2->v3[1] + v1->v3[2]*v2->v3[2]);
}

double
FN(vec3dDotProduct,A)(const vec3d_ptr v1, const vec3d_ptr v2)
{
   return  (v1->v3[0]*v2->v3[0] + v1->v3[1]*v2->v3[1] + v1->v3[2]*v2->v3[2]);
}

void
FN(vec3fCrossProduct,A)(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2)
{
   d->v3[0] = v1->v3[1]*v2->v3[2] - v1->v3[2]*v2->v3[1];
   d->v3[1] = v1->v3[2]*v2->v3[0] - v1->v3[0]*v2->v3[2];
   d->v3[2] = v1->v3[0]*v2->v3[1] - v1->v3[1]*v2->v3[0];
}

float
FN(vec3fNormalize,A)(vec3f_ptr d, const vec3f_ptr v)
{
   float mag = vec3fMagnitude(v);
   if (mag)
   {
      d->v3[0] = v->v3[0] / mag;
      d->v3[1] = v->v3[1] / mag;
      d->v3[2] = v->v3[2] / mag;
   }
   else
   {
      d->v3[0] = 0.0f;
      d->v3[1] = 0.0f;
      d->v3[2] = 0.0f;
   }
   return mag;
}

double
FN(vec3dNormalize,A)(vec3d_ptr d, const vec3d_ptr v)
{
   double mag = vec3dMagnitude(v);
   if (mag)
   {
      d->v3[0] = v->v3[0] / mag;
      d->v3[1] = v->v3[1] / mag;
      d->v3[2] = v->v3[2] / mag;
   }
   else
   {
      d->v3[0] = 0.0;
      d->v3[1] = 0.0;
      d->v3[2] = 0.0;
   }
   return mag;
}


void
FN(vec3fMatrix3,A)(vec3f_ptr d, const vec3f_ptr v, const mtx3f_ptr m)
{
   float v0 = v->v3[0], v1 = v->v3[1], v2 = v->v3[2];

   d->v3[0] = v0*m->m3[0][0] + v1*m->m3[1][0] + v2*m->m3[2][0];
   d->v3[1] = v0*m->m3[0][1] + v1*m->m3[1][1] + v2*m->m3[2][1];
   d->v3[2] = v0*m->m3[0][2] + v1*m->m3[1][2] + v2*m->m3[2][2];
}

void
FN(mtx4fSetAbsolute,A)(mtx4f_ptr d, bool absolute)
{
   d->m4[3][3] = absolute ? 1.0f : 0.0f;
}

void
FN(mtx4fMulVec4,A)(vec4f_ptr d, const mtx4f_ptr m, const vec4f_ptr v)
{
   float v0 = v->v4[0], v1 = v->v4[1], v2 = v->v4[2], v3 = v->v4[3];

   d->v4[0] = m->m4[0][0]*v0 + m->m4[1][0]*v1 + m->m4[2][0]*v2 + m->m4[3][0]*v3;
   d->v4[1] = m->m4[0][1]*v0 + m->m4[1][1]*v1 + m->m4[2][1]*v2 + m->m4[3][1]*v3;
   d->v4[2] = m->m4[0][2]*v0 + m->m4[1][2]*v1 + m->m4[2][2]*v2 + m->m4[3][2]*v3;
   d->v4[3] = m->m4[0][3]*v0 + m->m4[1][3]*v1 + m->m4[2][3]*v2 + m->m4[3][3]*v3;
}

void
FN(mtx4dMulVec4,A)(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr v)
{
   double v0 = v->v4[0], v1 = v->v4[1], v2 = v->v4[2], v3 = v->v4[3];

   d->v4[0] = m->m4[0][0]*v0 + m->m4[1][0]*v1 + m->m4[2][0]*v2 + m->m4[3][0]*v3;
   d->v4[1] = m->m4[0][1]*v0 + m->m4[1][1]*v1 + m->m4[2][1]*v2 + m->m4[3][1]*v3;
   d->v4[2] = m->m4[0][2]*v0 + m->m4[1][2]*v1 + m->m4[2][2]*v2 + m->m4[3][2]*v3;
   d->v4[3] = m->m4[0][3]*v0 + m->m4[1][3]*v1 + m->m4[2][3]*v2 + m->m4[3][3]*v3;
}

void
FN(mtx4fMul,A)(mtx4f_ptr dst, const mtx4f_ptr mtx1, const mtx4f_ptr mtx2)
{
   const float *m20 = mtx2->m4[0], *m21 = mtx2->m4[1];
   const float *m22 = mtx2->m4[2], *m23 = mtx2->m4[3];
   float m10i, m11i, m12i, m13i;
   int i=3;
   do
   {
      m10i = mtx1->m4[0][i];
      m11i = mtx1->m4[1][i];
      m12i = mtx1->m4[2][i];
      m13i = mtx1->m4[3][i];

      dst->m4[0][i] = m10i*m20[0] + m11i*m20[1] + m12i*m20[2] + m13i*m20[3];
      dst->m4[1][i] = m10i*m21[0] + m11i*m21[1] + m12i*m21[2] + m13i*m21[3];
      dst->m4[2][i] = m10i*m22[0] + m11i*m22[1] + m12i*m22[2] + m13i*m22[3];
      dst->m4[3][i] = m10i*m23[0] + m11i*m23[1] + m12i*m23[2] + m13i*m23[3];
   }
   while (i-- != 0);
}

void
FN(mtx4dMul,A)(mtx4d_ptr dst, const mtx4d_ptr mtx1, const mtx4d_ptr mtx2)
{
   const double *m20 = mtx2->m4[0], *m21 = mtx2->m4[1];
   const double *m22 = mtx2->m4[2], *m23 = mtx2->m4[3];
   double m10i, m11i, m12i, m13i;
   int i=3;
   do
   {
      m10i = mtx1->m4[0][i];
      m11i = mtx1->m4[1][i];
      m12i = mtx1->m4[2][i];
      m13i = mtx1->m4[3][i];

      dst->m4[0][i] = m10i*m20[0] + m11i*m20[1] + m12i*m20[2] + m13i*m20[3];
      dst->m4[1][i] = m10i*m21[0] + m11i*m21[1] + m12i*m21[2] + m13i*m21[3];
      dst->m4[2][i] = m10i*m22[0] + m11i*m22[1] + m12i*m22[2] + m13i*m22[3];
      dst->m4[3][i] = m10i*m23[0] + m11i*m23[1] + m12i*m23[2] + m13i*m23[3];
   }
   while (i-- != 0);
}

#if 0
// Multiply against a pre-transposed mtx1 which could be faster for SIMD
void
mtx4Mul2(double dst[4][4], double mtx1[4][4], double mtx2[4][4])
{
   int i=3;
   do
   {
      double v10 = mtx1[i][0];
      double v11 = mtx1[i][1];
      double v12 = mtx1[i][2];
      double v13 = mtx1[i][3];

      dst[0][i] = v10*mtx2[0][0] + v11*mtx2[0][1] +
                  v12*mtx2[0][2] + v13*mtx2[0][3];
      dst[1][i] = v10*mtx2[1][0] + v11*mtx2[1][1] +
                  v12*mtx2[1][2] + v13*mtx2[1][3];
      dst[2][i] = v10*mtx2[2][0] + v11*mtx2[2][1] +
                  v12*mtx2[2][2] + v13*mtx2[2][3];
      dst[3][i] = v10*mtx2[3][0] + v11*mtx2[3][1] +
                  v12*mtx2[3][2] + v13*mtx2[3][3];

   }
   while (i-- != 0);
}
#endif

void
FN(mtx4fInverseSimple,A)(mtx4f_ptr dst, const mtx4f_ptr mtx)
{
   /* side */
   dst->m4[0][0] = mtx->m4[0][0];
   dst->m4[1][0] = mtx->m4[0][1];
   dst->m4[2][0] = mtx->m4[0][2];
   dst->m4[3][0] = -vec3fDotProduct(&mtx->v34[3], &mtx->v34[0]);

   /* up */
   dst->m4[0][1] = mtx->m4[1][0];
   dst->m4[1][1] = mtx->m4[1][1];
   dst->m4[2][1] = mtx->m4[1][2];
   dst->m4[3][1] = -vec3fDotProduct(&mtx->v34[3], &mtx->v34[1]);

   /* at */
   dst->m4[0][2] = mtx->m4[2][0];
   dst->m4[1][2] = mtx->m4[2][1];
   dst->m4[2][2] = mtx->m4[2][2];
   dst->m4[3][2] = -vec3fDotProduct(&mtx->v34[3], &mtx->v34[2]);

   dst->m4[0][3] = 0.0f;
   dst->m4[1][3] = 0.0f;
   dst->m4[2][3] = 0.0f;
   dst->m4[3][3] = 1.0f;
}

void
FN(mtx4dInverseSimple,A)(mtx4d_ptr dst, const mtx4d_ptr mtx)
{
   /* side */
   dst->m4[0][0] = mtx->m4[0][0];
   dst->m4[1][0] = mtx->m4[0][1];
   dst->m4[2][0] = mtx->m4[0][2];
   dst->m4[3][0] = -vec3dDotProduct(&mtx->v34[3], &mtx->v34[0]);

   /* up */
   dst->m4[0][1] = mtx->m4[1][0];
   dst->m4[1][1] = mtx->m4[1][1];
   dst->m4[2][1] = mtx->m4[1][2];
   dst->m4[3][1] = -vec3dDotProduct(&mtx->v34[3], &mtx->v34[1]);

   /* at */
   dst->m4[0][2] = mtx->m4[2][0];
   dst->m4[1][2] = mtx->m4[2][1];
   dst->m4[2][2] = mtx->m4[2][2];
   dst->m4[3][2] = -vec3dDotProduct(&mtx->v34[3], &mtx->v34[2]);

   dst->m4[0][3] = 0.0;
   dst->m4[1][3] = 0.0;
   dst->m4[2][3] = 0.0;
   dst->m4[3][3] = 1.0;
}

void
FN(mtx4fTranslate,A)(mtx4f_ptr m, float x, float y, float z)
{
   if (x || y || z)
   {
      m->m4[3][0] += x;
      m->m4[3][1] += y;
      m->m4[3][2] += z;
   }
}

void
FN(mtx4dTranslate,A)(mtx4d_ptr m, double x, double y, double z)
{
   if (x || y || z)
   {
      m->m4[3][0] += x;
      m->m4[3][1] += y;
      m->m4[3][2] += z;
   }
}

void
FN(mtx4fRotate,A)(mtx4f_ptr mtx, float angle_rad, float x, float y, float z)
{
   if (angle_rad)
   {
      float s = sinf(angle_rad);
      float c = cosf(angle_rad);
      float t = 1.0f - c;
      vec3f_t axis, tmp;
      mtx4f_t m, o;

      tmp.v3[0] = x;
      tmp.v3[1] = y;
      tmp.v3[2] = z;
      vec3fNormalize(&axis, &tmp);

 	     /* rotation matrix */
      m.m4[0][0] = axis.v3[0]*axis.v3[0]*t + c;
      m.m4[0][1] = axis.v3[0]*axis.v3[1]*t + axis.v3[2]*s;
      m.m4[0][2] = axis.v3[0]*axis.v3[2]*t - axis.v3[1]*s;
      m.m4[0][3] = 0.0f;

      m.m4[1][0] = axis.v3[1]*axis.v3[0]*t - axis.v3[2]*s;
      m.m4[1][1] = axis.v3[1]*axis.v3[1]*t + c;
      m.m4[1][2] = axis.v3[1]*axis.v3[2]*t + axis.v3[0]*s;
      m.m4[1][3] = 0.0f;
        
      m.m4[2][0] = axis.v3[2]*axis.v3[0]*t + axis.v3[1]*s;
      m.m4[2][1] = axis.v3[2]*axis.v3[1]*t - axis.v3[0]*s;
      m.m4[2][2] = axis.v3[2]*axis.v3[2]*t + c;
      m.m4[2][3] = 0.0f;

      m.m4[3][0] = 0.0f;
      m.m4[3][1] = 0.0f;
      m.m4[3][2] = 0.0f;
      m.m4[3][3] = 1.0f;

      mtx4fCopy(&o, mtx);
      mtx4fMul(mtx, &o, &m);
   }
}

void
FN(mtx4dRotate,A)(mtx4d_ptr mtx, double angle_rad, double x, double y, double z)
{
   if (angle_rad)
   {
      double s = sin(angle_rad);
      double c = cos(angle_rad);
      double t = 1.0 - c;
      vec3d_t axis, tmp;
      mtx4d_t m, o;

      tmp.v3[0] = x;
      tmp.v3[1] = y;
      tmp.v3[2] = z;
      vec3dNormalize(&axis, &tmp);

             /* rotation matrix */
      m.m4[0][0] = axis.v3[0]*axis.v3[0]*t + c;
      m.m4[0][1] = axis.v3[0]*axis.v3[1]*t + axis.v3[2]*s;
      m.m4[0][2] = axis.v3[0]*axis.v3[2]*t - axis.v3[1]*s;
      m.m4[0][3] = 0.0;

      m.m4[1][0] = axis.v3[1]*axis.v3[0]*t - axis.v3[2]*s;
      m.m4[1][1] = axis.v3[1]*axis.v3[1]*t + c;
      m.m4[1][2] = axis.v3[1]*axis.v3[2]*t + axis.v3[0]*s;
      m.m4[1][3] = 0.0;

      m.m4[2][0] = axis.v3[2]*axis.v3[0]*t + axis.v3[1]*s;
      m.m4[2][1] = axis.v3[2]*axis.v3[1]*t - axis.v3[0]*s;
      m.m4[2][2] = axis.v3[2]*axis.v3[2]*t + c;
      m.m4[2][3] = 0.0;

      m.m4[3][0] = 0.0;
      m.m4[3][1] = 0.0;
      m.m4[3][2] = 0.0;
      m.m4[3][3] = 1.0;

      mtx4dCopy(&o, mtx);
      mtx4dMul(mtx, &o, &m);
   }
}

