/*
 * Copyright (C) 2005-2011 by Erik Hofman.
 * Copyright (C) 2007-2011 by Adalin B.V.
 *
 * This file is part of OpenAL-AeonWave.
 *
 *  OpenAL-AeonWave is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenAL-AeonWave is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenAL-AeonWave.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __OAL_TYPES_H
#define __OAL_TYPES_H 1

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef _MSC_VER
# define ALIGN16        __declspec(align(16))
#elif defined(__GNUC__)
# define ALIGN16        __attribute__((aligned(16)))
#else
# define ALIGN16
#endif

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_INTTYPES_H
#include <inttypes.h>
#elif HAVE_STDINT_H
#include <stdint.h>
#else
# ifdef _MSC_VER
typedef signed char      int8_t;
typedef signed short     int16_t;
typedef signed int       int32_t;
typedef signed __int64   int64_t;
typedef unsigned char    uint8_t;
typedef unsigned short   uint16_t;
typedef unsigned int     uint32_t;
typedef unsigned __int64 uint64_t;
typedef int size_t;
# endif
#endif

typedef int32_t*__restrict      int32_ptr ALIGN16;
typedef int32_t**__restrict     int32_ptrptr ALIGN16;
typedef float*__restrict        float32_ptr ALIGN16;
typedef double*__restrict       double64_ptr ALIGN16;


#include "gmath.h"	/* for is_nan */

#define _MAX(a,b)	(((a)>(b)) ? (a) : (b))
#define _MIN(a,b)	(((a)<(b)) ? (a) : (b))
#define _MINMAX(a,b,c)	(((a)>(c)) ? (c) : (((a)<(b)) ? (b) : (a)))

#define FNMAX(a,b)	is_nan(a) ? (b) : _MAX((a),(b))
#define FNMIN(a,b)	is_nan(a) ? (b) : _MIN((a),(b))
#define FNMINMAX(a,b,c)	is_nan(a) ? (c) : _MINMAX((a),(b),(c))


uint16_t _bswap16(uint16_t x);
uint32_t _bswap32(uint32_t x);
uint32_t _bswap32h(uint32_t x);
uint32_t _bswap32w(uint32_t x);
uint64_t _bswap64(uint64_t x);

#endif /* !__OAL_TYPES_H */

