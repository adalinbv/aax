/*
 * Copyright 2007-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
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

