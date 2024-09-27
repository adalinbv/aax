/*
 * Copyright (C) 1999-2001  Brian Paul
 * Copyright (C) 2010  Kristian Høgsberg
 * Copyright (C) 2010  Chia-I Wu
 * Copyright (C) 2023  Collabora Ltd
 *
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <base/geometry.h>
#include <base/logging.h>

void
mat4_multiply(double *m, const double *n)
{
   double tmp[16];
   const double *row, *column;
   div_t d;
   int i, j;

   for (i = 0; i < 16; i++) {
      tmp[i] = 0;
      d = div(i, 4);
      row = n + d.quot * 4;
      column = m + d.rem;
      for (j = 0; j < 4; j++)
         tmp[i] += row[j] * column[j * 4];
   }
   memcpy(m, &tmp, sizeof tmp);
}

void
mat4_scale(double *m, double x, double y, double z)
{
   double s[16] = {
      x, 0, 0, 0,
      0, y, 0, 0,
      0, 0, z, 0,
      0, 0, 0, 1
   };
   mat4_multiply(m, s);
}

void
mat4_rotate(double *m, double angle, double x, double y, double z)
{
   double s, c;
#if HAVE_SINCOS
   sincos(angle, &s, &c);
#else
   s = sin(angle);
   c = cos(angle);
#endif
   double r[16] = {
      x * x * (1 - c) + c,     y * x * (1 - c) + z * s, x * z * (1 - c) - y * s, 0,
      x * y * (1 - c) - z * s, y * y * (1 - c) + c,     y * z * (1 - c) + x * s, 0,
      x * z * (1 - c) + y * s, y * z * (1 - c) - x * s, z * z * (1 - c) + c,     0,
      0, 0, 0, 1
   };

   mat4_multiply(m, r);
}

void
mat4_translate(double *m, double x, double y, double z)
{
   double t[16] = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  x, y, z, 1 };

   mat4_multiply(m, t);
}

void
mat4_identity(double *m)
{
   double t[16] = {
      1.0, 0.0, 0.0, 0.0,
      0.0, 1.0, 0.0, 0.0,
      0.0, 0.0, 1.0, 0.0,
      0.0, 0.0, 0.0, 1.0,
   };

   memcpy(m, t, sizeof(t));
}

void
mat4_transpose(double *m)
{
   double t[16] = {
      m[0], m[4], m[8],  m[12],
      m[1], m[5], m[9],  m[13],
      m[2], m[6], m[10], m[14],
      m[3], m[7], m[11], m[15]};

   memcpy(m, t, sizeof(t));
}

void
mat4_invert(double *m)
{
   double t[16];
   mat4_identity(t);

   // Extract and invert the translation part 't'. The inverse of a
   // translation matrix can be calculated by negating the translation
   // coordinates.
   t[12] = -m[12]; t[13] = -m[13]; t[14] = -m[14];

   // Invert the rotation part 'r'. The inverse of a rotation matrix is
   // equal to its transpose.
   m[12] = m[13] = m[14] = 0;
   mat4_transpose(m);

   // inv(m) = inv(r) * inv(t)
   mat4_multiply(m, t);
}

void
mat4_frustum_gl(double *m, double l, double r, double b, double t, double n, double f)
{
   double tmp[16];
   mat4_identity(tmp);

   double deltaX = r - l;
   double deltaY = t - b;
   double deltaZ = f - n;

   tmp[0] = (2 * n) / deltaX;
   tmp[5] = (2 * n) / deltaY;
   tmp[8] = (r + l) / deltaX;
   tmp[9] = (t + b) / deltaY;
   tmp[10] = -(f + n) / deltaZ;
   tmp[11] = -1;
   tmp[14] = -(2 * f * n) / deltaZ;
   tmp[15] = 0;

   memcpy(m, tmp, sizeof(tmp));
}

void
mat4_frustum_vk(double *m, double l, double r, double b, double t, double n, double f)
{
   double tmp[16];
   mat4_identity(tmp);

   double deltaX = r - l;
   double deltaY = t - b;
   double deltaZ = f - n;

   tmp[0] = (2 * n) / deltaX;
   tmp[5] = (-2 * n) / deltaY;
   tmp[8] = (r + l) / deltaX;
   tmp[9] = (t + b) / deltaY;
   tmp[10] = f / (n - f);
   tmp[11] = -1;
   tmp[14] = -(f * n) / deltaZ;
   tmp[15] = 0;

   memcpy(m, tmp, sizeof(tmp));
}

void
mat4_perspective_gl(double *m, double fovy, double aspect,
                    double zNear, double zFar)
{
   double tmp[16];
   mat4_identity(tmp);

   double sine, cosine, cotangent, deltaZ;
   double radians = fovy / 2 * M_PI / 180;

   deltaZ = zFar - zNear;
   sine = sin(radians);
   cosine = cos(radians);

   if ((deltaZ == 0) || (sine == 0) || (aspect == 0))
      return;

   cotangent = cosine / sine;

   tmp[0] = cotangent / aspect;
   tmp[5] = cotangent;
   tmp[10] = -(zFar + zNear) / deltaZ;
   tmp[11] = -1;
   tmp[14] = -2 * zNear * zFar / deltaZ;
   tmp[15] = 0;

   memcpy(m, tmp, sizeof(tmp));
}

int main()
{
   mtx4d_t m11, m12, rm1;
   mtx4d_t m21, m22;
   float rad;

   srand(time(NULL));
   rad = GMATH_2PI*rand()/(double)(RAND_MAX);

   mtx4dSetIdentity(m11.m4);
   mtx4dTranslate(&m11, -1.0, 0.0, 0.0);
   mtx4dInverseSimple(&m11, &m11);

   mtx4dSetIdentity(m12.m4);
   mtx4dRotate(&m12, rad, 0.0, 1.0, 0.0);
   
   mtx4dMul(&rm1, &m11, &m12);

   /** */

   mat4_identity(*m21.m4);
   mat4_translate(*m21.m4, -1.0, 0.0, 0.0);
   mat4_invert(*m21.m4);

   mat4_identity(*m22.m4);
   mat4_rotate(*m22.m4, rad, 0.0, 1.0, 0.0);

   mat4_multiply(*m21.m4, *m22.m4);

   printf("own:\t\t\t\tcontoll:\n");
   PRINT_MATRICES(rm1, m21);
}
