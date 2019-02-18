/*
 * Copyright (C) 2005-2018 by Erik Hofman.
 * Copyright (C) 2007-2018 by Adalin B.V.
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

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
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

#ifdef _WIN32
# ifndef WIN32
#  define WIN32
# endif
#endif

#define _MIN(a,b)	(((a)<(b)) ? (a) : (b))
#define _MAX(a,b)       (((a)>(b)) ? (a) : (b))
#define _MINMAX(a,b,c)  (((a)>(c)) ? (c) : (((a)<(b)) ? (b) : (a)))

char* _aax_strcasestr(const char*, const char*);

#if _MSC_VER
# include <Windows.h>
# define strtoll _strtoi64
# define snprintf _snprintf
# define strcasecmp _stricmp
# define strncasecmp _strnicmp
# define rintf(v) (int)(v+0.5f)

struct timespec
{
  time_t tv_sec; /* seconds */
  long tv_nsec;  /* nanoseconds */
};

struct timezone
{
  int tz_minuteswest; /* of Greenwich */
  int tz_dsttime;     /* type of dst correction to apply */
};
int gettimeofday(struct timeval*, void*);

typedef long    off_t;
# if SIZEOF_SIZE_T == 4
typedef INT32   ssize_t;
#else
typedef INT64   ssize_t;
#endif
#endif


#endif /* !__OAL_TYPES_H */

