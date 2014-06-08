
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
get_pow2(unsigned n)
{
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
}

unsigned
log2i(unsigned x)
{
   int y = 0;
   while (x > 0)
   {
      x >>= 1;
      ++y;
   }
   return y;
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
detect_nan_vec3(const float vec[3])
{
   return is_nan(vec[0]) || is_nan(vec[1]) || is_nan(vec[2]);
}

int
detect_nan_vec4(const float vec[4])
{
   return is_nan(vec[0]) || is_nan(vec[1]) || is_nan(vec[2]) || is_nan(vec[3]);
}

int
detect_nan_vec4d(const double vec[4])
{
   return is_nan(vec[0]) || is_nan(vec[1]) || is_nan(vec[2]) || is_nan(vec[3]);
}

int
detect_nan_mtx4(const float mtx[4][4]){
   return detect_nan_vec4(mtx[0]) || detect_nan_vec4(mtx[1]) ||
          detect_nan_vec4(mtx[2]) || detect_nan_vec4(mtx[3]);
}

int
detect_nan_mtx4d(const double mtx[4][4])
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
detect_inf_vec3(const float vec[3])
{
   return !(is_inf(vec[0]) || is_inf(vec[1]) || is_inf(vec[2]));
}

int
detect_zero_vec3(const float vec[3])
{
   return ((fabs(vec[0]) + fabs(vec[1]) + fabs(vec[2])) < 0.05);
}
