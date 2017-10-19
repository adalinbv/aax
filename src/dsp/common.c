/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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

#include <math.h>
#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
#endif

#include <base/types.h>
#include <base/geometry.h>

void destroy(void *ptr) { free(ptr); }

float _lin(float v) { return v; }
float _square(float v) { return v*v; }
float _lin2log(float v) { return log10f(v); }
float _log2lin(float v) { return powf(10.0f,v); }
float _lin2db(float v) { return 20.0f*log10f(v); }
float _db2lin(float v) { return _MINMAX(powf(10.0f,v/20.0f),0.0f,10.0f); }
float _rad2deg(float v) { return v*GMATH_RAD_TO_DEG; }
float _deg2rad(float v) { return fmodf(v, 360.0f)*GMATH_DEG_TO_RAD; }
float _cos_deg2rad_2(float v) { return cosf(_deg2rad(v)/2); }
float _2acos_rad2deg(float v) { return 2*_rad2deg(acosf(v)); }
float _cos_2(float v) { return cosf(v/2); }
float _2acos(float v) { return 2*acosf(v); }

