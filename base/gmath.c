/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <math.h>

#include "geometry.h"

// domain for x: 0 .. GMATH_2PI
float fast_sin(float x)
{
   x *= GMATH_1_PI;
   x -= 1.0f;
   return -4.0f*(x - x*fabsf(x));
}

unsigned
get_pow2(uint32_t n)
{
#if defined(__GNUC__)
    return 1 << (32 -__builtin_clz(n-1));
#else
   unsigned y, x = n;

   --x;
   x |= x >> 16;
   x |= x >> 8;
   x |= x >> 4;
   x |= x >> 2;
   x |= x >> 1;
   ++x;

   y = n >> 1;
   if (y < (x-n)) x >>= 1;

   return x;
#endif
}

unsigned
log2i(uint32_t x)
{
#if defined(__GNUC__)
   return 31 - __builtin_clz(x);
#else
   int y = 0;
   while (x > 0)
   {
      x >>= 1;
      ++y;
   }
   return y;
#endif
}

int
is_nan(float x)
{
#if HAVE_ISNAN
   return isnan(x);
#else
   volatile float temp = x;
   return (temp != x) ? 1 : 0;
#endif
}

int
is_nan64(double x)
{
#if HAVE_ISNAN
   return is_nan(x);
#else
   volatile double temp = x;
   return (temp != x) ? 1 : 0;
#endif
}

int
detect_nan_vec3(float vec[3])
{
   return is_nan(vec[0]) || is_nan(vec[1]) || is_nan(vec[2]);
}

int
detect_nan_vec3d(double vec[3])
{
   return is_nan64(vec[0]) || is_nan64(vec[1]) || is_nan64(vec[2]);
}

int
detect_nan_vec4(float vec[4])
{
   return is_nan(vec[0]) || is_nan(vec[1]) || is_nan(vec[2]) || is_nan(vec[3]);
}

int
detect_nan_vec4d(double vec[4])
{
   return is_nan(vec[0]) || is_nan(vec[1]) || is_nan(vec[2]) || is_nan(vec[3]);
}

int
detect_nan_mtx4(float mtx[4][4]){
   return detect_nan_vec4(mtx[0]) || detect_nan_vec4(mtx[1]) ||
          detect_nan_vec4(mtx[2]) || detect_nan_vec4(mtx[3]);
}

int
detect_nan_mtx4d(double mtx[4][4])
{
   return detect_nan_vec4d(mtx[0]) || detect_nan_vec4d(mtx[1]) ||
          detect_nan_vec4d(mtx[2]) || detect_nan_vec4d(mtx[3]);
}

int
is_inf(float x)
{
   volatile float temp = x;
   if ((temp == x) && ((temp - x) != 0.0)) {
      return (x < 0.0 ? -1 : 1);
   }
   else return 0;
}

int
detect_inf_vec3(float vec[3])
{
   return !(is_inf(vec[0]) || is_inf(vec[1]) || is_inf(vec[2]));
}

int
detect_zero_vec3(float vec[3])
{
   return ((fabsf(vec[0]) + fabsf(vec[1]) + fabsf(vec[2])) < 0.05);
}
