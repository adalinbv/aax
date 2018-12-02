/*
 * Copyright (C) 2005-2018 by Erik Hofman <erik@ehofman.com>
 * Copyright (C) 2010-2018 by Adalin B.v.
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


#ifdef __ARM_VFPV2
void
vec3fNegate_vfpv2(vec3f_ptr d, const vec3f_ptr v)
{
   d->v3[0] = -v->v3[0];
   d->v3[1] = -v->v3[1];
   d->v3[2] = -v->v3[2];
}

void
vec4fNegate_vfpv2(vec4f_ptr d, const vec4f_ptr v)
{
   d->v4[0] = -v->v4[0];
   d->v4[1] = -v->v4[1];
   d->v4[2] = -v->v4[2];
   d->v4[3] = -v->v4[3];
}


void
vec4fScalarMul_vfpv2(vec4f_ptr r, float f)
{
   r->v4[0] *= f;
   r->v4[1] *= f;
   r->v4[2] *= f;
   r->v4[3] *= f;
}


void
_vec3fMulVec3_vfpv2(vec3f_ptr r, const vec3f_ptr v1, const vec3f_ptr v2)
{
   r->v3[0] = v1->v3[0]*v2->v3[0];
   r->v3[1] = v1->v3[1]*v2->v3[1];
   r->v3[2] = v1->v3[2]*v2->v3[2];
}

void
_vec4fMulVec4_vfpv2(vec4f_ptr r, const vec4f_ptr v1, const vec4f_ptr v2)
{
   r->v4[0] = v1->v4[0]*v2->v4[0];
   r->v4[1] = v1->v4[1]*v2->v4[1];
   r->v4[2] = v1->v4[2]*v2->v4[2];
   r->v4[3] = v1->v4[3]*v2->v4[3];
}

void
_vec4iMulVec4i_vfpv2(vec4i_ptr r, const vec4i_ptr v1, const vec4i_ptr v2)
{
   r->v4[0] = v1->v4[0]*v2->v4[0];
   r->v4[1] = v1->v4[1]*v2->v4[1];
   r->v4[2] = v1->v4[2]*v2->v4[2];
   r->v4[3] = v1->v4[3]*v2->v4[3];
}


float
_vec3fMagnitude_vfpv2(const vec3f_ptr v)
{
   float val = v->v3[0]*v->v3[0] + v->v3[1]*v->v3[1] + v->v3[2]*v->v3[2];
   return sqrtf(val);
}

double
_vec3dMagnitude_vfpv2(const vec3d_ptr v)
{
   double val = v->v3[0]*v->v3[0] + v->v3[1]*v->v3[1] + v->v3[2]*v->v3[2];
   return sqrt(val);
}

float
_vec3fMagnitudeSquared_vfpv2(const vec3f_ptr v)
{
   return (v->v3[0]*v->v3[0] + v->v3[1]*v->v3[1] + v->v3[2]*v->v3[2]);
}

float
_vec3fDotProduct_vfpv2(const vec3f_ptr v1, const vec3f_ptr v2)
{
   return  (v1->v3[0]*v2->v3[0] + v1->v3[1]*v2->v3[1] + v1->v3[2]*v2->v3[2]);
}

double
_vec3dDotProduct_vfpv2(const vec3d_ptr v1, const vec3d_ptr v2)
{
   return  (v1->v3[0]*v2->v3[0] + v1->v3[1]*v2->v3[1] + v1->v3[2]*v2->v3[2]);
}

void
_vec3fCrossProduct_vfpv2(vec3f_ptr d, const vec3f_ptr v1, const vec3f_ptr v2)
{
   d->v3[0] = v1->v3[1]*v2->v3[2] - v1->v3[2]*v2->v3[1];
   d->v3[1] = v1->v3[2]*v2->v3[0] - v1->v3[0]*v2->v3[2];
   d->v3[2] = v1->v3[0]*v2->v3[1] - v1->v3[1]*v2->v3[0];
}

float
_vec3fNormalize_vfpv2(vec3f_ptr d, const vec3f_ptr v)
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
_vec3dNormalize_vfpv2(vec3d_ptr d, const vec3d_ptr v)
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
vec3fMatrix3_vfpv2(vec3f_ptr d, const vec3f_ptr v, const mtx3f_ptr m)
{
   float v0 = v->v3[0], v1 = v->v3[1], v2 = v->v3[2];

   d->v3[0] = v0*m->m3[0][0] + v1*m->m3[1][0] + v2*m->m3[2][0];
   d->v3[1] = v0*m->m3[0][1] + v1*m->m3[1][1] + v2*m->m3[2][1];
   d->v3[2] = v0*m->m3[0][2] + v1*m->m3[1][2] + v2*m->m3[2][2];
}

void
mtx4fSetAbsolute_vfpv2(mtx4f_ptr d, char absolute)
{
   d->m4[3][3] = absolute ? 1.0f : 0.0f;
}

void
_mtx4fMulVec4_vfpv2(vec4f_ptr d, const mtx4f_ptr m, const vec4f_ptr v)
{
   float v0 = v->v4[0], v1 = v->v4[1], v2 = v->v4[2], v3 = v->v4[3];

   d->v4[0] = m->m4[0][0]*v0 + m->m4[1][0]*v1 + m->m4[2][0]*v2 + m->m4[3][0]*v3;
   d->v4[1] = m->m4[0][1]*v0 + m->m4[1][1]*v1 + m->m4[2][1]*v2 + m->m4[3][1]*v3;
   d->v4[2] = m->m4[0][2]*v0 + m->m4[1][2]*v1 + m->m4[2][2]*v2 + m->m4[3][2]*v3;
   d->v4[3] = m->m4[0][3]*v0 + m->m4[1][3]*v1 + m->m4[2][3]*v2 + m->m4[3][3]*v3;
}

void
_mtx4dMulVec4_vfpv2(vec4d_ptr d, const mtx4d_ptr m, const vec4d_ptr v)
{
   double v0 = v->v4[0], v1 = v->v4[1], v2 = v->v4[2], v3 = v->v4[3];

   d->v4[0] = m->m4[0][0]*v0 + m->m4[1][0]*v1 + m->m4[2][0]*v2 + m->m4[3][0]*v3;
   d->v4[1] = m->m4[0][1]*v0 + m->m4[1][1]*v1 + m->m4[2][1]*v2 + m->m4[3][1]*v3;
   d->v4[2] = m->m4[0][2]*v0 + m->m4[1][2]*v1 + m->m4[2][2]*v2 + m->m4[3][2]*v3;
   d->v4[3] = m->m4[0][3]*v0 + m->m4[1][3]*v1 + m->m4[2][3]*v2 + m->m4[3][3]*v3;
}


void
_mtx4fMul_vfpv2(mtx4f_ptr dst, const mtx4f_ptr mtx1, const mtx4f_ptr mtx2)
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
_mtx4dMul_vfpv2(mtx4d_ptr dst, const mtx4d_ptr mtx1, const mtx4d_ptr mtx2)
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
mtx4fInverseSimple_vfpv2(mtx4f_ptr dst, const mtx4f_ptr mtx)
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
mtx4dInverseSimple_vfpv2(mtx4d_ptr dst, const mtx4d_ptr mtx)
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
mtx4fTranslate_vfpv2(mtx4f_ptr m, float x, float y, float z)
{
   if (x || y || z)
   {
      m->m4[3][0] += x;
      m->m4[3][1] += y;
      m->m4[3][2] += z;
   }
}

void
mtx4dTranslate_vfpv2(mtx4d_ptr m, double x, double y, double z)
{
   if (x || y || z)
   {
      m->m4[3][0] += x;
      m->m4[3][1] += y;
      m->m4[3][2] += z;
   }
}

void
mtx4fRotate_vfpv2(mtx4f_ptr mtx, float angle_rad, float x, float y, float z)
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
mtx4dRotate_vfpv2(mtx4d_ptr mtx, double angle_rad, double x, double y, double z)
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

#else
typedef int make_iso_compilers_happy;
#endif /* __ARM_VFPV2 */
