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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_TIME_H
# include <time.h>             /* for nanosleep */
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>         /* for struct timeval */
#endif

#include "types.h"


#ifdef _WIN32
/*
   Implementation as per:
   The Open Group Base Specifications, Issue 6
   IEEE Std 1003.1, 2004 Edition

   The timezone pointer arg is ignored.  Errors are ignored.
*/
int gettimeofday(struct timeval* p, void* tz /* IGNORED */)
{
   union {
      long long ns100; /*time since 1 Jan 1601 in 100ns units */
      FILETIME ft;
   } now;

   GetSystemTimeAsFileTime( &(now.ft) );
   p->tv_usec=(long)((now.ns100 / 10LL) % 1000000LL );
   p->tv_sec= (long)((now.ns100-(116444736000000000LL))/10000000LL);
   return 0;
}

#else
int msecSleep(unsigned int tms)
{
   static struct timespec s;
   s.tv_sec = (tms/1000);
   s.tv_nsec = (tms-s.tv_sec*1000);
   return nanosleep(&s, 0);
}
#endif

#if 0
uint32_t _mem_size(void *p)
{
#if defined(__sgi)
   return mallocblksize(p);
#elif defined(__LINUX__)
   return malloc_usable_size(p);
#elif defined(WIN32)
   return _msize(p);
#endif
};
#endif

uint16_t _bswap16(uint16_t x)
{
   x = (x >> 8) | (x << 8);
   return x;
}

uint32_t _bswap32(uint32_t x)
{
   x = ((x >>  8) & 0x00FF00FFL) | ((x <<  8) & 0xFF00FF00L);
   x = (x >> 16) | (x << 16);
   return x;
}

uint32_t _bswap32h(uint32_t x)
{
   x = ((x >>  8) & 0x00FF00FFL) | ((x <<  8) & 0xFF00FF00L);
   return x;
}

uint32_t _bswap32w(uint32_t x)
{
   x = (x >> 16) | (x << 16);
   return x;
}

uint64_t _bswap64(uint64_t x)
{
   x = ((x >>  8) & 0x00FF00FF00FF00FFLL) | ((x <<  8) & 0xFF00FF00FF00FF00LL);
   x = ((x >> 16) & 0x0000FFFF0000FFFFLL) | ((x << 16) & 0xFFFF0000FFFF0000LL);
   x = (x >> 32) | (x << 32);
   return x;
}

