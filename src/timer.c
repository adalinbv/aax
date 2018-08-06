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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_TIME_H
# include <time.h>             /* for nanosleep */
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>         /* for struct timeval */
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include <assert.h>
#include <errno.h>
#include <math.h>

#include <aax/aax.h>
#include "timer.h"


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

int usecSleep(unsigned int dt_us)
{
   DWORD res = SleepEx((DWORD)dt_us*1000, 0);
   return (res != 0) ? -1 : 0;
}

/* end of highres timing code */

#else	/* _WIN32 */

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

#include <poll.h>
int usecSleep(unsigned int dt_us)
{
   struct timeval delay;
   delay.tv_sec = 0;
   delay.tv_usec = dt_us;
   do {
      (void) select(0, NULL, NULL, NULL, &delay);
   } while ((delay.tv_usec > 0) || (delay.tv_sec > 0));
   return 0;
}

/* end of highres timing code */

#endif	/* if defined(_WIN32) */


_aaxTimer* _aaxTimerCreate()
{
   return calloc(1, sizeof(_aaxTimer));  
}

void _aaxTimerDestroy(_aaxTimer* timer)
{
    free(timer);
}

int _aaxTimerStartRepeatable(_aaxTimer* timer, unsigned int us)
{
   assert(timer);
   timer->step_us = 1000;
   gettimeofday(&timer->start, NULL);
   return 0;
}

int _aaxTimerStop(_aaxTimer* timer)
{
   return 1;
}

int _aaxTimerWait(_aaxTimer* time)
{
   struct timeval now;
   ssize_t diff_us;
   int rv = 0;

   gettimeofday(&now, NULL);
   diff_us = now.tv_sec * 1000000 + now.tv_usec;
   diff_us -= time->start.tv_sec * 1000000 + time->start.tv_usec;
   time->start = now;

   diff_us = time->step_us - diff_us;
   if (diff_us > 0) {
      rv = usecSleep(diff_us);
   }
   return rv;
}

