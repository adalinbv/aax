/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2007-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

#ifndef EREMCHG
# define EREMCHG	78
#endif

#ifndef EUSERS
# define EUSERS		87
#endif

#ifndef EREMOTEIO
# define EREMOTEIO	121
#endif

#ifndef OFF_T_MAX
# define OFF_T_MAX	(off_t)-1
#endif

#define MEMALIGN	64
#define MEMMASK		(MEMALIGN-1)
#define MEMALIGN16	16
#define MEMMASK16	(MEMALIGN16-1)

#ifdef _MSC_VER
# define ALIGN	__declspec(align(32))
# define ALIGNC
# define ALIGN16 __declspec(align(16))
# define ALIGN16C
# define ALIGN32 __declspec(align(32))
# define ALIGN32C
#elif defined(__GNUC__) || defined(__TINYC__)
# define ALIGN
# define ALIGNC __attribute__((aligned(32)))
# define ALIGN16
# define ALIGN16C __attribute__((aligned(16)))
# define ALIGN32
# define ALIGN32C __attribute__((aligned(32)))
#else
# define ALIGN
# define ALIGNC
# define ALIGN16
# define ALIGN16C
# define ALIGN32
# define ALIGN32C
#endif

#ifdef __MINGW32__
        // Force proper stack alignment for functions that use SSE
# define FN_PREALIGN	__attribute__((force_align_arg_pointer))
#else
# define FN_PREALIGN
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

typedef char*RESTRICT			char_ptr;
typedef const char*RESTRICT		const_char_ptr;
typedef void*RESTRICT			void_ptr;
typedef void**RESTRICT			void_ptrptr;
typedef const void*RESTRICT		const_void_ptr;
typedef int32_t*RESTRICT		int32_ptr;
typedef const int32_t*RESTRICT		const_int32_ptr;
typedef int32_t**RESTRICT		int32_ptrptr;
typedef const int32_t**RESTRICT		const_int32_ptrptr;
typedef uint16_t*RESTRICT		float16_ptr;
typedef const uint16_t*RESTRICT		const_float16_ptr;
typedef float*RESTRICT			float32_ptr;
typedef float**RESTRICT			float32_ptrptr;
typedef const float*RESTRICT		const_float32_ptr;
typedef const float**RESTRICT		const_float32_ptrptr;
typedef double*RESTRICT			double64_ptr;
typedef const double*RESTRICT		const_double64_ptr;

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
//# define _WIN32_WINNT 0x0500
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <stdio.h>
# include <string.h>
# include <errno.h>
# include <ctype.h>

# define DISREGARD(x) x
# define UNUSED(x) x

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
typedef INT64	ssize_t;

#else		// WIN32

# define DISREGARD(x) x __attribute__((unused))
# define UNUSED(x) x __attribute__((unused))

#endif		// WIN32

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !__OAL_TYPES_H */

