#ifndef __OAL_MATH_H
#define __OAL_MATH_H 1

#if HAVE_VALUES_H
# include <values.h>	/* for MAXFLOAT */
#endif
#include <math.h>

#define __NAN__		(0.0f/0.0f)

#ifndef MAXFLOAT
# include <float.h>
# ifdef FLT_MAX
#  define MAXFLOAT FLT_MAX
# else
#  define MAXFLOAT 3.40282347e+38F
# endif
#endif

int is_nan(float);
int is_inf(float);
int detect_nan_vec3(const float *);
int detect_nan_vec4(const float *);
int detect_inf_vec3(const float *);
int detect_zero_vec3(const float *);

#if 0
#define fast_abs(a)            ((a) > 0) ? (a) : -(a)
float fast_abs(float);
#endif
float fast_sin(float);
unsigned get_pow2(unsigned);
unsigned log2i(unsigned);

#endif /* !__OAL_MATH_H */

