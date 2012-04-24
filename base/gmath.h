#ifndef __OAL_MATH_H
#define __OAL_MATH_H 1

#define __NAN__		(0.0f/0.0f)

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

