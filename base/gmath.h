#ifndef __OAL_MATH_H
#define __OAL_MATH_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_VALUES_H
# include <values.h>	/* for MAXFLOAT */
#endif
#include <math.h>

#include "types.h"

#define __NAN__		(0.0f/0.0f)

#ifndef MAXFLOAT
# include <float.h>
# ifdef FLT_MAX
#  define MAXFLOAT FLT_MAX
# else
#  define MAXFLOAT 3.40282347e+38F
# endif
#endif

#define HALF2FLOAT(h)	(((h)&0x8000)<<16) | ((((h)&0x7c00)+0x1C000)<<13) | (((h)&0x03FF)<<13)
#define FLOAT2HALF(f)	(((f)>>16)&0x8000)|(((((f)&0x7f800000)-0x38000000)>>13)&0x7c00)|(((f)>>13)&0x03ff)

int is_nan(float);
int is_nan64(double);
int is_inf(float);
int detect_nan_vec3(const float[3]);
int detect_nan_vec4(const float[4]);
int detect_nan_vec4d(const double[4]);
int detect_nan_mtx4(const float[4][4]);
int detect_nan_mtx4d(const double[4][4]);
int detect_inf_vec3(const float[3]);
int detect_zero_vec3(const float[3]);

#if 0
#define fast_abs(a)            ((a) > 0) ? (a) : -(a)
float fast_abs(float);
#endif
float fast_sin(float);
unsigned get_pow2(uint32_t);
unsigned log2i(uint32_t);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !__OAL_MATH_H */

