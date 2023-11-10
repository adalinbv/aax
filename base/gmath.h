/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef __OAL_MATH_H
#define __OAL_MATH_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include <float.h>	/* for FLT_MAX */
#include <math.h>

#include "types.h"

#define __NAN__		(0.0f/0.0f)

#ifndef FLT_MAX
# define FLT_MAX 3.40282347e+38F
#endif

int is_nan(float);
int is_nan64(double);
int is_inf(float);
int detect_nan_vec3(float[3]);
int detect_nan_vec3d(double[3]);
int detect_nan_vec4(float[4]);
int detect_nan_vec4d(double[4]);
int detect_nan_mtx4(float[4][4]);
int detect_nan_mtx4d(double[4][4]);
int detect_inf_vec3(float[3]);
int detect_zero_vec3(float[3]);

float fast_sin(float);

unsigned get_pow2(uint32_t);
unsigned log2i(uint32_t);

/* http://www.devmaster.net/forums/showthread.php?t=5784 */
/* Do not replace! */
float fast_sin(float x);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !__OAL_MATH_H */

