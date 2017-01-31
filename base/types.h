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

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef O_BINARY
# define O_BINARY	0
#endif

#if SIZEOF_SIZE_T == 8
# define MEMALIGN	32
#else
# define MEMALIGN	16
#endif
#define MEMMASK		(MEMALIGN-1)
#define MEMMASK16	15

#ifdef _MSC_VER
# if SIZEOF_SIZE_T == 8
#  define ALIGN	__declspec(align(32))
# else
#  define ALIGN	__declspec(align(16))
# endif
# define ALIGNC
#elif defined(__GNUC__) || defined(__TINYC__)
# define ALIGN
# if SIZEOF_SIZE_T == 8
# define ALIGNC	__attribute__((aligned(32)))
# else
# define ALIGNC	__attribute__((aligned(32)))
# endif
#else
# define ALIGN
# define ALIGNC
#endif

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
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

#if defined(HAVE_RESTRICT) || defined(restrict)
# define RESTRICT restrict
#elif defined(HAVE___RESTRICT) || defined(__restrict)
# define RESTRICT __restrict
#else
# define RESTRICT
#endif

typedef char*RESTRICT		char_ptr;
typedef const char*RESTRICT	const_char_ptr;
typedef void*RESTRICT		void_ptr;
typedef const void*RESTRICT	const_void_ptr;
typedef int32_t*RESTRICT	int32_ptr;
typedef const int32_t*RESTRICT	const_int32_ptr;
typedef int32_t**RESTRICT	int32_ptrptr;
typedef const int32_t**RESTRICT	const_int32_ptrptr;
typedef float*RESTRICT		float32_ptr;
typedef const float*RESTRICT	const_float32_ptr;
typedef const float**RESTRICT	const_float32_ptrptr;
typedef double*RESTRICT		double64_ptr;
typedef const double*RESTRICT	const_double64_ptr;

#include "gmath.h"	/* for is_nan */

#define _MAX(a,b)	(((a)>(b)) ? (a) : (b))
#define _MIN(a,b)	(((a)<(b)) ? (a) : (b))
#define _MINMAX(a,b,c)	(((a)>(c)) ? (c) : (((a)<(b)) ? (b) : (a)))

#define FNMAX(a,b)	is_nan(a) ? (b) : _MAX((a),(b))
#define FNMIN(a,b)	is_nan(a) ? (b) : _MIN((a),(b))
#define FNMINMAX(a,b,c)	is_nan(a) ? (c) : _MINMAX((a),(b),(c))


uint16_t _aax_bswap16(uint16_t x);
uint32_t _aax_bswap32(uint32_t x);
uint32_t _aax_bswap32h(uint32_t x);
uint32_t _aax_bswap32w(uint32_t x);
uint64_t _aax_bswap64(uint64_t x);

#ifdef _WIN32
# ifndef WIN32
#  define WIN32
# endif
#endif

#if defined( WIN32 )
# undef __STRICT_ANSI__
# include <Windows.h>
# include <stdio.h>
# include <string.h>
# include <errno.h>
# include <ctype.h>

# define rintf(v) (int)(v+0.5f)
# ifdef _MSC_VER
#  define snprintf _aax_snprintf
# endif
# ifndef __GNUC__
#  define strtoll _strtoi64
#  define strcasecmp _stricmp
#  define strncasecmp _strnicmp
# endif
# define strcasestr _aax_strcasestr

char* strcasestr(const char*, const char*);
int _aax_snprintf(char*, size_t, const char*, ...);

typedef long	off_t;
# if SIZEOF_SIZE_T == 4
typedef INT32	ssize_t;
# else
typedef INT64	ssize_t;
# endif

struct timespec {
      time_t tv_sec;
      long   tv_nsec;
};
#endif		// WIN32

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !__OAL_TYPES_H */

