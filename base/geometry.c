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

#include <stdio.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif
#if HAVE_MATH_H
#include <math.h>
#endif

#include <aax/aax.h>
#include "logging.h"
#include "geometry.h"
#include "types.h"

void
vec3dFill(double d[3], double v[3])
{
   memcpy(d, v, sizeof(double[3]));
}

void
vec3fFill(float d[3], float v[3])
{
   memcpy(d, v, sizeof(float[3]));
}

void
vec3dFillf(double dst[3], float src[3])
{
   int i;
   for(i=0; i<3; ++i) {
      dst[i] = (double)src[i];
   }
}

void
vec3fFilld(float dst[3], double src[3])
{
   int i;
   for(i=0; i<3; ++i) {
      dst[i] = (float)src[i];
   }
}

void
vec3fZero(vec3f_ptr f)
{
   memset(f->v3, 0, sizeof(fx4_t));
}

void
vec3dZero(vec3d_ptr d)
{
   memset(d->v3, 0, sizeof(dx4_t));
}

void
_vec3fCopy_cpu(vec3f_ptr d, const vec3f_ptr v)
{
   memcpy(d->v3, v->v3, sizeof(fx4_t));
}

void
_vec3dCopy_cpu(vec3d_ptr d, const vec3d_ptr v)
{
   memcpy(d->v3, v->v3, sizeof(dx4_t));
}


void
vec4fFill(float d[4], float v[4])
{
   memcpy(d, v, sizeof(float[4]));
}

void
_vec4fCopy_cpu(vec4f_ptr d, const vec4f_ptr v)
{
   memcpy(d->v4, v->v4, sizeof(fx4_t));
}

void
_vec4dCopy_cpu(vec4d_ptr d, const vec4d_ptr v)
{
   memcpy(d->v4, v->v4, sizeof(dx4_t));
}

void
_vec4iCopy_cpu(vec4i_ptr d, const vec4i_ptr v)
{
   memcpy(d->v4, v->v4, sizeof(ix4_t));
}


void
vec3dAdd(vec3d_ptr d, const vec3d_ptr v1, const vec3d_ptr v2)
{
   d->v3[0] = v1->v3[0] + v2->v3[0];
   d->v3[1] = v1->v3[1] + v2->v3[1];
   d->v3[2] = v1->v3[2] + v2->v3[2];
}

void
vec3dSub(vec3d_ptr d, const vec3d_ptr v1, const vec3d_ptr v2)
{
   d->v3[0] = v1->v3[0] - v2->v3[0];
   d->v3[1] = v1->v3[1] - v2->v3[1];
   d->v3[2] = v1->v3[2] - v2->v3[2];
}

void
vec3dScalarMul(vec3d_ptr d, const vec3d_ptr r, float f)
{
   d->v3[0] = r->v3[0] * f;
   d->v3[1] = r->v3[1] * f;
   d->v3[2] = r->v3[2] * f;
}

void
vec3dNegate(vec3d_ptr d, const vec3d_ptr v)
{
   d->v3[0] = -v->v3[0];
   d->v3[1] = -v->v3[1];
   d->v3[2] = -v->v3[2];
}

void
vec3fAdd(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2)
{
   d->v3[0] = v1->v3[0] + v2->v3[0];
   d->v3[1] = v1->v3[1] + v2->v3[1];
   d->v3[2] = v1->v3[2] + v2->v3[2];
}

void
vec3fSub(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2)
{
   d->v3[0] = v1->v3[0] - v2->v3[0];
   d->v3[1] = v1->v3[1] - v2->v3[1];
   d->v3[2] = v1->v3[2] - v2->v3[2];
}

void
vec3fScalarMul(vec3f_ptr d, const vec3f_ptr r, float f)
{
   d->v3[0] = r->v3[0] * f;
   d->v3[1] = r->v3[1] * f;
   d->v3[2] = r->v3[2] * f;
}

void
vec3fNegate(vec3f_ptr d, const vec3f_ptr v)
{
   d->v3[0] = -v->v3[0];
   d->v3[1] = -v->v3[1];
   d->v3[2] = -v->v3[2];
}

int
vec3fLessThan(const vec3f_ptr v1, const vec3f_ptr v2)
{
   if ((v1->v3[0]) <= (v2->v3[0]) &&
       (v1->v3[1]) <= (v2->v3[1]) &&
       (v1->v3[2]) <= (v2->v3[2]))
   {
      return 1;
   }
   return 0;
}

int
vec3dLessThan(const vec3d_ptr v1, const vec3d_ptr v2)
{
   if ((v1->v3[0]) < (v2->v3[0]) &&
       (v1->v3[1]) < (v2->v3[1]) &&
       (v1->v3[2]) < (v2->v3[2]))
   {
      return 1;
   }
   return 0;
}

void
vec4fNegate(vec4f_ptr d, const vec4f_ptr v)
{
   d->v4[0] = -v->v4[0];
   d->v4[1] = -v->v4[1];
   d->v4[2] = -v->v4[2];
   d->v4[3] = -v->v4[3];
}


void
vec4fScalarMul(vec4f_ptr d, const vec4f_ptr r, float f)
{
   d->v4[0] = r->v4[0] * f;
   d->v4[1] = r->v4[1] * f;
   d->v4[2] = r->v4[2] * f;
   d->v4[3] = r->v4[3] * f;
}


void
_vec3fAbsolute_cpu(vec3f_ptr d, const vec3f_ptr v)
{
   d->v3[0] = fabsf(v->v3[0]);
   d->v3[1] = fabsf(v->v3[1]);
   d->v3[2] = fabsf(v->v3[2]);
}

void
_vec3dAbsolute_cpu(vec3d_ptr d, const vec3d_ptr v)
{
   d->v3[0] = fabs(v->v3[0]);
   d->v3[1] = fabs(v->v3[1]);
   d->v3[2] = fabs(v->v3[2]);
}

void
_vec3fMulVec3_cpu(vec3f_ptr r, const vec3f_ptr v1, const vec3f_ptr v2)
{
   r->v3[0] = v1->v3[0]*v2->v3[0];
   r->v3[1] = v1->v3[1]*v2->v3[1];
   r->v3[2] = v1->v3[2]*v2->v3[2];
}

void
_vec3dMulVec3_cpu(vec3d_ptr r, const vec3d_ptr v1, const vec3d_ptr v2)
{
   r->v3[0] = v1->v3[0]*v2->v3[0];
   r->v3[1] = v1->v3[1]*v2->v3[1];
   r->v3[2] = v1->v3[2]*v2->v3[2];
}

void
_vec4fMulVec4_cpu(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2)
{
   r->v4[0] = v1->v4[0]*v2->v4[0];
   r->v4[1] = v1->v4[1]*v2->v4[1];
   r->v4[2] = v1->v4[2]*v2->v4[2];
   r->v4[3] = v1->v4[3]*v2->v4[3];
}

void
_vec4iMulVec4i_cpu(vec4i_ptr r, const vec4i_ptr v1, const vec4i_ptr v2)
{
   r->v4[0] = v1->v4[0]*v2->v4[0];
   r->v4[1] = v1->v4[1]*v2->v4[1];
   r->v4[2] = v1->v4[2]*v2->v4[2];
   r->v4[3] = v1->v4[3]*v2->v4[3];
}


float
_vec3fMagnitude_cpu(const vec3f_ptr v)
{
   float val = v->v3[0]*v->v3[0] + v->v3[1]*v->v3[1] + v->v3[2]*v->v3[2];
   return sqrtf(val);
}

double
_vec3dMagnitude_cpu(const vec3d_ptr v)
{
   double val = v->v3[0]*v->v3[0] + v->v3[1]*v->v3[1] + v->v3[2]*v->v3[2];
   return sqrt(val);
}

float
_vec3fMagnitudeSquared_cpu(const vec3f_ptr v)
{
   return (v->v3[0]*v->v3[0] + v->v3[1]*v->v3[1] + v->v3[2]*v->v3[2]);
}

double
_vec3dMagnitudeSquared_cpu(const vec3d_ptr v)
{
   return (v->v3[0]*v->v3[0] + v->v3[1]*v->v3[1] + v->v3[2]*v->v3[2]);
}

float
_vec3fDotProduct_cpu(const vec3f_ptr v1, const vec3f_ptr v2)
{
   return  (v1->v3[0]*v2->v3[0] + v1->v3[1]*v2->v3[1] + v1->v3[2]*v2->v3[2]);
}

double
_vec3dDotProduct_cpu(const vec3d_ptr v1, const vec3d_ptr v2)
{
   return  (v1->v3[0]*v2->v3[0] + v1->v3[1]*v2->v3[1] + v1->v3[2]*v2->v3[2]);
}

void
_vec3fCrossProduct_cpu(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2)
{
   d->v3[0] = v1->v3[1]*v2->v3[2] - v1->v3[2]*v2->v3[1];
   d->v3[1] = v1->v3[2]*v2->v3[0] - v1->v3[0]*v2->v3[2];
   d->v3[2] = v1->v3[0]*v2->v3[1] - v1->v3[1]*v2->v3[0];
}

float
_vec3fNormalize_cpu(vec3f_ptr d, const vec3f_ptr v)
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
_vec3dNormalize_cpu(vec3d_ptr d, const vec3d_ptr v)
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
vec3fMatrix3(vec3f_ptr d, const vec3f_ptr v, const mtx3f_ptr m)
{
   float v0 = v->v3[0], v1 = v->v3[1], v2 = v->v3[2];

   d->v3[0] = v0*m->m3[0][0] + v1*m->m3[1][0] + v2*m->m3[2][0];
   d->v3[1] = v0*m->m3[0][1] + v1*m->m3[1][1] + v2*m->m3[2][1];
   d->v3[2] = v0*m->m3[0][2] + v1*m->m3[1][2] + v2*m->m3[2][2];
}

void
mtx3fCopy(mtx3f_ptr d, const mtx3f_ptr m)
{
   memcpy(d->m3, m->m3, sizeof(mtx3f_t));
}

void
_mtx4fCopy_cpu(mtx4f_ptr d, const mtx4f_ptr m)
{
   memcpy(d->m4, m->m4, sizeof(mtx4f_t));
}

void
mtx4fFill(float d[4][4], float m[4][4])
{
   memcpy(d, m, sizeof(float[4][4]));
}

void
mtx4dFillf(double d[4][4], float m[4][4])
{
   int i, j;
   for(i=0; i<4; ++i) {
      for (j=0; j<4; ++j) {
         d[i][j] = (double)m[i][j];
      }
   }
}

void
mtx4fFilld(float d[4][4], double m[4][4])
{  
   int i, j;
   for(i=0; i<4; ++i) {
      for (j=0; j<4; ++j) {
         d[i][j] = (float)m[i][j];
      }
   }
}


const fx4x4_t aaxIdentityMatrix = {
  { 1.0f, 0.0f, 0.0f, 0.0f },
  { 0.0f, 1.0f, 0.0f, 0.0f },
  { 0.0f, 0.0f, 1.0f, 0.0f },
  { 0.0f, 0.0f, 0.0f, 1.0f },
};


void
mtx4fSetIdentity(float m[4][4])
{
    memcpy(m, aaxIdentityMatrix, sizeof(float[4][4]));
}

void
_mtx4dCopy_cpu(mtx4d_ptr d, const mtx4d_ptr m)
{
   memcpy(d->m4, m->m4, sizeof(mtx4d_t));
}

const dx4x4_t aaxIdentityMatrix64 = {
  { 1.0, 0.0, 0.0, 0.0 },
  { 0.0, 1.0, 0.0, 0.0 },
  { 0.0, 0.0, 1.0, 0.0 },
  { 0.0, 0.0, 0.0, 1.0 },
};

void
mtx4dSetIdentity(double m[4][4])
{
    memcpy(m, aaxIdentityMatrix64, sizeof(double[4][4]));
}


void
mtx4dFill(double d[4][4], double m[4][4])
{
   memcpy(d, m, sizeof(double[4][4]));
}

void
mtx4fSetAbsolute(mtx4f_ptr d, char absolute)
{
   d->m4[3][3] = absolute ? 1.0f : 0.0f;
}


/*
 * http://www.unknownroad.com/rtfm/graphics/rt_normals.html
 * If you are doing any real graphics, you are using homogenoeus vectors and 
 * matrices. That means 4 dimensions, vectors are [x y z w]. That fourth 
 * coordinate is the homogenoeous coordinate, it should be 1 for points and 0
 * for vectors.
 *
 * Note: this does not work for non-uniform scaling (that means scaling by
 * different amounts in the different axis)
 */
void
_mtx4fMulVec4_cpu(vec4f_ptr d, const mtx4f_ptr m, const vec4f_ptr v)
{
   float v0 = v->v4[0], v1 = v->v4[1], v2 = v->v4[2], v3 = v->v4[3];

   d->v4[0] = m->m4[0][0]*v0 + m->m4[1][0]*v1 + m->m4[2][0]*v2 + m->m4[3][0]*v3;
   d->v4[1] = m->m4[0][1]*v0 + m->m4[1][1]*v1 + m->m4[2][1]*v2 + m->m4[3][1]*v3;
   d->v4[2] = m->m4[0][2]*v0 + m->m4[1][2]*v1 + m->m4[2][2]*v2 + m->m4[3][2]*v3;
   d->v4[3] = m->m4[0][3]*v0 + m->m4[1][3]*v1 + m->m4[2][3]*v2 + m->m4[3][3]*v3;
}

void
_mtx4dMulVec4_cpu(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr v)
{
   double v0 = v->v4[0], v1 = v->v4[1], v2 = v->v4[2], v3 = v->v4[3];

   d->v4[0] = m->m4[0][0]*v0 + m->m4[1][0]*v1 + m->m4[2][0]*v2 + m->m4[3][0]*v3;
   d->v4[1] = m->m4[0][1]*v0 + m->m4[1][1]*v1 + m->m4[2][1]*v2 + m->m4[3][1]*v3;
   d->v4[2] = m->m4[0][2]*v0 + m->m4[1][2]*v1 + m->m4[2][2]*v2 + m->m4[3][2]*v3;
   d->v4[3] = m->m4[0][3]*v0 + m->m4[1][3]*v1 + m->m4[2][3]*v2 + m->m4[3][3]*v3;
}

void
_mtx4fMul_cpu(mtx4f_ptr dst, const mtx4f_ptr mtx1, const mtx4f_ptr mtx2)
{
   const float *m20 = mtx2->m4[0], *m21 = mtx2->m4[1];
   const float *m22 = mtx2->m4[2], *m23 = mtx2->m4[3];
   float m10i, m11i, m12i, m13i;
   int i=4;
   do
   {
      --i;

      m10i = mtx1->m4[0][i];
      m11i = mtx1->m4[1][i];
      m12i = mtx1->m4[2][i];
      m13i = mtx1->m4[3][i];

      dst->m4[0][i] = m10i*m20[0] + m11i*m20[1] + m12i*m20[2] + m13i*m20[3];
      dst->m4[1][i] = m10i*m21[0] + m11i*m21[1] + m12i*m21[2] + m13i*m21[3];
      dst->m4[2][i] = m10i*m22[0] + m11i*m22[1] + m12i*m22[2] + m13i*m22[3];
      dst->m4[3][i] = m10i*m23[0] + m11i*m23[1] + m12i*m23[2] + m13i*m23[3];
   }
   while (i != 0);
}

void
_mtx4dMul_cpu(mtx4d_ptr dst, const mtx4d_ptr mtx1, const mtx4d_ptr mtx2)
{
   const double *m20 = mtx2->m4[0], *m21 = mtx2->m4[1];
   const double *m22 = mtx2->m4[2], *m23 = mtx2->m4[3];
   double m10i, m11i, m12i, m13i;
   int i=4;
   do
   {
      --i;

      m10i = mtx1->m4[0][i];
      m11i = mtx1->m4[1][i];
      m12i = mtx1->m4[2][i];
      m13i = mtx1->m4[3][i];

      dst->m4[0][i] = m10i*m20[0] + m11i*m20[1] + m12i*m20[2] + m13i*m20[3];
      dst->m4[1][i] = m10i*m21[0] + m11i*m21[1] + m12i*m21[2] + m13i*m21[3];
      dst->m4[2][i] = m10i*m22[0] + m11i*m22[1] + m12i*m22[2] + m13i*m22[3];
      dst->m4[3][i] = m10i*m23[0] + m11i*m23[1] + m12i*m23[2] + m13i*m23[3];
   }
   while (i != 0);
}

void
mtx4fInverseSimple(mtx4f_ptr dst, const mtx4f_ptr mtx)
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
mtx4dInverseSimple(mtx4d_ptr dst, const mtx4d_ptr mtx)
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
mtx4fTranslate(mtx4f_ptr m, float x, float y, float z)
{
   if (x || y || z)
   {
      m->m4[3][0] += x;
      m->m4[3][1] += y;
      m->m4[3][2] += z;
   }
}

void
mtx4dTranslate(mtx4d_ptr m, double x, double y, double z)
{
   if (x || y || z)
   {
      m->m4[3][0] += x;
      m->m4[3][1] += y;
      m->m4[3][2] += z;
   }
}

void
mtx4fRotate(mtx4f_ptr mtx, float angle_rad, float x, float y, float z)
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
mtx4dRotate(mtx4d_ptr mtx, double angle_rad, double x, double y, double z)
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

// Calculate the altvec vector of the frame on the frame-emitter vector
// which is useful for detecting whether a line hits a bounding box.
//
// Returns true if the fpvec and the fevec are at the same side, but
// outside of, the bounding box.
//
// Since the frame should be axis-aligned, and positioned at the origin
// at this point:
//  - altvec is the altitude of the frame over the parent-emitter vector.
//  - afevec is the emitter position retlative to the frame position.
//  - fpvec is the parent_frame position retlative to the frame position.
int
_vec3dAltitudeVector_cpu(vec3f_ptr altvec, const mtx4d_ptr ifmtx, const vec3d_ptr ppos, const vec3d_ptr epos, const vec3f_ptr afevec, vec3f_ptr fpvec)
{
   vec4d_t pevec, fevec;
   vec3d_t npevec, fpevec;
   double mag_pe, dot_fpe;
   int ahead;

   if (!ppos) {				// parent position is at the origin
      vec3dNegate(&pevec.v3, epos);	// parent-emitter vector is emtter pos.
   } else {
      vec3dSub(&pevec.v3, ppos, epos);
   }
   pevec.v4[3] = 0.0;
   _mtx4dMulVec4_cpu(&pevec, ifmtx, &pevec);

   vec3dCopy(&fevec.v3, epos);
   fevec.v4[3] = 1.0;
   _mtx4dMulVec4_cpu(&fevec, ifmtx, &fevec);

   // Get the projection length of the frame-to-emitter vector on
   // the parent_frame-to-emitter unit vector.
   mag_pe = _vec3dNormalize_cpu(&npevec, &pevec.v3);
   dot_fpe = _vec3dDotProduct_cpu(&fevec.v3, &npevec);

   // Scale the parent_frame-to-emitter unit vector.
   vec3dScalarMul(&fpevec, &npevec, dot_fpe);

   // Get the perpendicular vector from the frame position to the
   // parent_frame-to-emitter vector (altitude).
   vec3dSub(&fpevec, &fevec.v3, &fpevec);
   _vec3dAbsolute_cpu(&fpevec, &fpevec);

   // Calculate the frame-parent vector which is used outside this function.
   vec3dAdd(&npevec, &fevec.v3, &pevec.v3);
   _vec3dAbsolute_cpu(&npevec, &npevec);

   _vec3dAbsolute_cpu(&fevec.v3, &fevec.v3);
   vec3fFilld(afevec->v3, fevec.v4);
   vec3fFilld(altvec->v3, fpevec.v3);
   vec3fFilld(fpvec->v3, npevec.v3);

   // If dot_fpe < 0.0f then the emitter is between the frame and the
   // parent-frame meaning there is a clean path to the parent-frame.
   ahead = (dot_fpe >= 0.0f || (mag_pe+dot_fpe) <= FLT_EPSILON);

   return ahead;
}

int
_vec3fAltitudeVector_cpu(vec3f_ptr altvec, const mtx4f_ptr ifmtx, const vec3f_ptr ppos, const vec3f_ptr epos, const vec3f_ptr afevec, vec3f_ptr fpvec)
{
   vec4f_t pevec, fevec;
   vec3f_t npevec, fpevec;
   float mag_pe, dot_fpe;
   int ahead;

   if (!ppos) {
      vec3fNegate(&pevec.v3, epos);
   } else {
      vec3fSub(&pevec.v3, ppos, epos);
   }
   pevec.v4[3] = 0.0;
   _mtx4fMulVec4_cpu(&pevec, ifmtx, &pevec);

   _vec3fCopy_cpu(&fevec.v3, epos);
   fevec.v4[3] = 1.0;
   _mtx4fMulVec4_cpu(&fevec, ifmtx, &fevec);
   _vec3fAbsolute_cpu(afevec, &fevec.v3);

   mag_pe = _vec3fNormalize_cpu(&npevec, &pevec.v3);
   dot_fpe = _vec3fDotProduct_cpu(&fevec.v3, &npevec);

   vec3fScalarMul(&fpevec, &npevec, dot_fpe);

   vec3fSub(&fpevec, &fevec.v3, &fpevec);
   _vec3fAbsolute_cpu(altvec, &fpevec);

   vec3fAdd(&npevec, &fevec.v3, &pevec.v3);
   _vec3fAbsolute_cpu(fpvec, &npevec);

   ahead = (dot_fpe >= 0.0f || (mag_pe+dot_fpe) <= FLT_EPSILON);

   return ahead;
}

/* -------------------------------------------------------------------------- */

vec3fCopy_proc vec3fCopy = _vec3fCopy_cpu;
vec3dCopy_proc vec3dCopy = _vec3dCopy_cpu;
vec3fMulVec3f_proc vec3fMulVec3 = _vec3fMulVec3_cpu;
vec3dMulVec3d_proc vec3dMulVec3 = _vec3dMulVec3_cpu;
vec3fAbsolute_proc vec3fAbsolute = _vec3fAbsolute_cpu;
vec3dAbsolute_proc vec3dAbsolute = _vec3dAbsolute_cpu;

vec3fMagnitude_proc vec3fMagnitude = _vec3fMagnitude_cpu;
vec3dMagnitude_proc vec3dMagnitude = _vec3dMagnitude_cpu;
vec3fMagnitudeSquared_proc vec3fMagnitudeSquared = _vec3fMagnitudeSquared_cpu;
vec3dMagnitudeSquared_proc vec3dMagnitudeSquared = _vec3dMagnitudeSquared_cpu;
vec3fDotProduct_proc vec3fDotProduct = _vec3fDotProduct_cpu;
vec3dDotProduct_proc vec3dDotProduct = _vec3dDotProduct_cpu;
vec3fNormalize_proc vec3fNormalize = _vec3fNormalize_cpu;
vec3dNormalize_proc vec3dNormalize = _vec3dNormalize_cpu;
vec3fCrossProduct_proc vec3fCrossProduct = _vec3fCrossProduct_cpu;
vec3fAltitudeVector_proc vec3fAltitudeVector = _vec3fAltitudeVector_cpu;
vec3dAltitudeVector_proc vec3dAltitudeVector = _vec3dAltitudeVector_cpu;

vec4fCopy_proc vec4fCopy = _vec4fCopy_cpu;
vec4dCopy_proc vec4dCopy = _vec4dCopy_cpu;
mtx4fCopy_proc mtx4fCopy = _mtx4fCopy_cpu;
mtx4dCopy_proc mtx4dCopy = _mtx4dCopy_cpu;
vec4fMulVec4f_proc vec4fMulVec4 = _vec4fMulVec4_cpu;
mtx4fMul_proc mtx4fMul = _mtx4fMul_cpu;
mtx4dMul_proc mtx4dMul = _mtx4dMul_cpu;
mtx4fMulVec4_proc mtx4fMulVec4 = _mtx4fMulVec4_cpu;
mtx4dMulVec4_proc mtx4dMulVec4 = _mtx4dMulVec4_cpu;

vec4iCopy_proc vec4iCopy = _vec4iCopy_cpu;
vec4iMulVec4if_proc vec4iMulVec4i = _vec4iMulVec4i_cpu;

