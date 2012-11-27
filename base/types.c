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
#include <errno.h>

#include "types.h"


#ifdef WIN32

int _aax_snprintf(char *str,size_t size,const char *fmt,...)
{
   int ret;
   va_list ap;
    
   va_start(ap,fmt);
   ret = vsnprintf(str,size,fmt,ap);
   // Whatever happen in vsnprintf, what i'll do is just to null terminate it
   str[size-1] = '\0';      
   va_end(ap);   
   return ret;   
}

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
   p->tv_usec = (long)((now.ns100 / 10LL) % 1000000LL );
   p->tv_sec = (long)((now.ns100-(116444736000000000LL))/10000000LL);
   return 0;
}

int clock_gettime(int clk_id, struct timespec *p)
{
   union {
      long long ns100; /*time since 1 Jan 1601 in 100ns units */
      FILETIME ft;
   } now;

   GetSystemTimeAsFileTime( &(now.ft) );
   p->tv_nsec = (long)((now.ns100 * 100LL) % 1000000000LL );
   p->tv_sec = (long)((now.ns100-(116444736000000000LL))/10000000LL);
   return 0;
}

int msecSleep(unsigned int dt_ms)
{
   DWORD res = SleepEx((DWORD)dt_ms, 0);
   return (res != 0) ? -1 : 0;
}

int setTimerResolution(unsigned int dt_ms)
{
   return timeBeginPeriod(dt_ms);
}

int resetTimerResolution(unsigned int dt_ms)
{
   return timeEndPeriod(dt_ms);
}

#else

/*
 * dt_ms == 0 is a special case which make the time-slice available for other
 * waiting processes
 */
int msecSleep(unsigned int dt_ms)
{
   static struct timespec s;
   if (dt_ms > 0)
   {
      s.tv_sec = (dt_ms/1000);
      s.tv_nsec = (dt_ms % 1000)*1000000L;
      while(nanosleep(&s,&s)==-1 && errno == EINTR)
         continue;
   }
   else
   {
      s.tv_sec = 0;
      s.tv_nsec = 500000L;
      return nanosleep(&s, 0);
   }
   return 0;
}

int setTimerResolution(unsigned int dt_ms)
{
   return 0;
}

int resetTimerResolution(unsigned int dt_ms) {
   return 0;
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

