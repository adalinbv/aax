/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <math.h>

#include "geometry.h"

#if 0
/* http://devmaster.net/forums/topic/4998-the-ultimate-fast-absolute-value/ */
float fast_fabs(float x)
{
    int y = (int&)x & 0x7FFFFFFF;
    return (float&)y;
}
#endif

#if 1
/* http://www.devmaster.net/forums/showthread.php?t=5784 */
float
fast_sin(float x)
{
   float y;

   x = fmodf(x+GMATH_PI, GMATH_2PI) - GMATH_PI;
   y = DIV4_GMATH_PI*x + DIV4_GMATH_PI2*x*fabsf(x);

   return y;
}
#else
float
fast_sin(float v)
{
   float rv, rv2;

   rv = fabs(1.0 - fmod(v+GMATH_PI_2, GMATH_2PI)/GMATH_PI);
   if (rv < 0.5f)
   {
      rv2 = 4.0f*rv*rv;
   }
   else
   {
      float val = 1.0f - rv;
      rv2 = 2.0f - 4.0f*val*val;
   }

   return 1.0f - rv2;
}
#endif

unsigned
get_pow2(uint32_t n)
{
#if defined(__GNUC__)
    return 1 << (32 -__builtin_clzl(n-1));
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
# if __x86_64__ || __ppc64__
   return 63 -__builtin_clzl(x);
# else
   return 31 -__builtin_clzl(x);
# endif
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
   return ((fabs(vec[0]) + fabs(vec[1]) + fabs(vec[2])) < 0.05);
}
