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

