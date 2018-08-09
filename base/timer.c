/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2007-2017 by Adalin B.V.
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
#include <math.h>

#include <aax/aax.h>
#include <base/threads.h>
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

unsigned int
getTimerResolution()
{
   _aaxTimer* timer = _aaxTimerCreate();
   double dt;

   _aaxTimerStart(timer);
   SleepEx(1, 0);
   dt = _aaxTimerElapsed(timer);
   _aaxTimerDestroy(timer);

   return 1000*dt;
}

int
setTimerResolution(unsigned int dt_ms) {
   return timeBeginPeriod(dt_ms);
}

int
resetTimerResolution(unsigned int dt_ms) {
   return timeEndPeriod(dt_ms);
}

/** highres timing code */
_aaxTimer*
_aaxTimerCreate()
{
   LARGE_INTEGER timerFreq;
   _aaxTimer *rv = NULL;

   if (QueryPerformanceFrequency(&timerFreq))
   {
      rv = malloc(sizeof(_aaxTimer));
      if (rv)
      {
         DWORD_PTR threadMask = SetThreadAffinityMask(GetCurrentThread(), 0);
         threadMask = SetThreadAffinityMask(GetCurrentThread(), 0);
         QueryPerformanceCounter(&rv->timerCount);
         QueryPerformanceCounter(&rv->timerOverhead);
         SetThreadAffinityMask(GetCurrentThread(), threadMask);

         rv->timerOverhead.QuadPart -= rv->timerCount.QuadPart;

         rv->prevTimerCount.QuadPart = rv->timerCount.QuadPart;
         rv->tfreq = (double)timerFreq.QuadPart;

         rv->Event[WAITABLE_TIMER_EVENT] = NULL;
         rv->Event[CONDITION_EVENT] = NULL;
         rv->Period = 0;
      }
   }
   return rv;
}

double
_aaxTimerGetFrequency(_aaxTimer *tm)
{
   double rv = 0;
   if (tm) {
      rv = tm->tfreq;
   }
   return rv;
}

void
_aaxTimerStart(_aaxTimer* tm) {
   _aaxTimerElapsed(tm);
}

double
_aaxTimerElapsed(_aaxTimer* tm)
{
   double rv = 0;
   if (tm)
   {
      DWORD_PTR threadMask;

      tm->prevTimerCount.QuadPart = tm->timerCount.QuadPart;
      tm->prevTimerCount.QuadPart += tm->timerOverhead.QuadPart;

      threadMask = SetThreadAffinityMask(GetCurrentThread(), 0);
      QueryPerformanceCounter(&tm->timerCount);
      SetThreadAffinityMask(GetCurrentThread(), threadMask);

      rv = (tm->timerCount.QuadPart - tm->prevTimerCount.QuadPart)/tm->tfreq;
   }
   return rv;
}

void
_aaxTimerDestroy(_aaxTimer* tm)
{
   _aaxTimerStop(tm);
   if (tm)
   {
      if (tm->Event[WAITABLE_TIMER_EVENT]) {
         CloseHandle(tm->Event[WAITABLE_TIMER_EVENT]);
      }
      if (tm->Event[CONDITION_EVENT]) {
         CloseHandle(tm->Event[CONDITION_EVENT]);
      }
   }
   free(tm);
}

int
_aaxTimerSetCondition(_aaxTimer *tm, void *event)
{
    int rv = AAX_FALSE;
    if (tm && !tm->Event[CONDITION_EVENT])
    {
       tm->Event[CONDITION_EVENT] = event;
       rv = AAX_TRUE;
    }
    return rv;
}

int
_aaxTimerStartRepeatable(_aaxTimer* tm, float sec)
{
   int rv = AAX_FALSE;
   if (tm && sec > 1e-6f)
   {
      if (tm->Event[WAITABLE_TIMER_EVENT] == NULL) {
         tm->Event[WAITABLE_TIMER_EVENT] = CreateWaitableTimer(NULL,FALSE,NULL);
      }

      if (tm->Event[WAITABLE_TIMER_EVENT])
      {
         HRESULT hr;

         tm->Period = (LONG)(sec*1000);
         tm->dueTime.QuadPart = -(LONGLONG)(sec*10000*1000);
         hr = SetWaitableTimer(tm->Event[WAITABLE_TIMER_EVENT],
                               &tm->dueTime, 0, NULL, NULL, FALSE);
         if (hr)
         {
            setTimerResolution(1);
            rv = AAX_TRUE;
         }
      }
   }
   else {
      rv = _aaxTimerStop(tm);
   }
   return rv;
}

int
_aaxTimerStop(_aaxTimer* tm)
{
   int rv = AAX_FALSE;
   if (tm && tm->Event[WAITABLE_TIMER_EVENT])
   {
      if (CancelWaitableTimer(tm->Event[WAITABLE_TIMER_EVENT]))
      {
         resetTimerResolution(1);
         CloseHandle(tm->Event[WAITABLE_TIMER_EVENT]);
         tm->Event[WAITABLE_TIMER_EVENT] = NULL;
         rv = AAX_TRUE;
      }
   }
   return rv;
}

int
_aaxTimerWait(_aaxTimer* tm, void* mutex)
{
   int rv = AAX_TRUE;

   if (tm->Event[WAITABLE_TIMER_EVENT])
   {
      DWORD hr, num = tm->Event[CONDITION_EVENT] ? 2 : 1;

      _aaxMutexUnLock(mutex);
      hr = WaitForMultipleObjects(num, tm->Event, FALSE, tm->Period);
      SetWaitableTimer(tm->Event[WAITABLE_TIMER_EVENT],
                       &tm->dueTime, 0, NULL, NULL, FALSE);
      _aaxMutexLock(mutex);

      switch(hr)
      {
      case WAIT_TIMEOUT:
      case WAIT_OBJECT_0 + WAITABLE_TIMER_EVENT:
         rv = AAX_TIMEOUT;
         break;
      case WAIT_OBJECT_0 + CONDITION_EVENT:
         rv = AAX_TRUE;
         break;
      default:
         rv = AAX_FALSE;
         break;
      }
   }

   return rv;
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
#if 1
   struct timeval delay;
   delay.tv_sec = dt_us / 1000000;
   delay.tv_usec = dt_us % 1000000;
   do {
      (void) select(0, NULL, NULL, NULL, &delay);
   } while ((delay.tv_usec > 0) || (delay.tv_sec > 0));
#else
   static struct timespec s;
   if (dt_us > 0)
   {
      s.tv_sec = (dt_us/1000000);
      s.tv_nsec = (dt_us % 1000000)*1000L;
      while(nanosleep(&s,&s)==-1 && errno == EINTR)
         continue;
   }
   else
   {
      s.tv_sec = 0;
      s.tv_nsec = 500000L;
      return nanosleep(&s, 0);
   }
#endif
   return 0;
}

unsigned int
getTimerResolution() {
   return 0;
}

int
setTimerResolution(UNUSED(unsigned int dt_ms)) {
   return 0;
}

int
resetTimerResolution(UNUSED(unsigned int dt_ms)) {
   return 0;
}

/** highres timing code */
static void
__aaxTimerSub(struct timespec *tso,
               struct timespec *ts1, struct timespec *ts2)
{
   double dt1, dt2;

   dt2 = ts2->tv_sec + ts2->tv_nsec/1000000000.0;
   dt1 = ts1->tv_sec + ts1->tv_nsec/1000000000.0;
   dt1 -= dt2;

   dt2 = floor(dt1);
   tso->tv_sec = dt2;
   tso->tv_nsec = rint((dt1-dt2)*1000000000.0);
}

static void
__aaxTimerAdd(struct timespec *tso,
               struct timespec *ts1, struct timespec *ts2)
{
   double dt1, dt2;

   dt2 = ts2->tv_sec + ts2->tv_nsec/1000000000.0;
   dt1 = ts1->tv_sec + ts1->tv_nsec/1000000000.0;
   dt1 -= dt2;

   dt2 = floor(dt1);
   tso->tv_sec = dt2;
   tso->tv_nsec = rint((dt1-dt2)*1000000000.0);

   if (tso->tv_nsec >= 1000000000L)
   {
      tso->tv_sec++;
      tso->tv_nsec -= 1000000000L;
   }
}

_aaxTimer*
_aaxTimerCreate()
{
   _aaxTimer *rv = calloc(1, sizeof(_aaxTimer));
   if (rv)
   {
      int res = clock_getres(CLOCK_MONOTONIC, &rv->timerCount);
      if (!res)
      {
         time_t sec = rv->timerCount.tv_sec;
         long nsec = rv->timerCount.tv_nsec;

         rv->tfreq = 1.0/(sec + nsec/1000000000.0);

         res = clock_gettime(CLOCK_MONOTONIC, &rv->prevTimerCount);
         res |= clock_gettime(CLOCK_MONOTONIC, &rv->timerCount);
         if (!res) {
            __aaxTimerSub(&rv->timerOverhead,
                           &rv->timerCount, &rv->prevTimerCount);
         }

         rv->signal = _aaxSignalCreate();
         rv->user_condition = AAX_FALSE;
         rv->dt = 0.0f;
      }
      
      if (res == -1)
      {
         free(rv);
         rv = NULL;
      }
   }
   return rv;
}

double
_aaxTimerGetFrequency(_aaxTimer *tm)
{
   double rv = 0;
   if (tm) {
      rv = tm->tfreq;
   }
   return rv;
}

void
_aaxTimerStart(_aaxTimer *tm) {
   _aaxTimerElapsed(tm);
}

double
_aaxTimerElapsed(_aaxTimer *tm)
{
   double rv = 0;
   if (tm)
   {
      int res;

      __aaxTimerAdd(&tm->prevTimerCount, &tm->timerCount, &tm->timerOverhead);

      res = clock_gettime(CLOCK_MONOTONIC, &tm->timerCount);
      if (!res)
      {
         double t1, t2;

         t1 = tm->prevTimerCount.tv_sec+tm->prevTimerCount.tv_nsec/1000000000.0;
         t2 = tm->timerCount.tv_sec + tm->timerCount.tv_nsec/1000000000.0;
         rv = (t2-t1);
      }
   }
   return rv;
}

void
_aaxTimerDestroy(_aaxTimer *tm)
{
   _aaxTimerStop(tm);
   if (tm)
   {
      if (tm->user_condition)
      {
         _aaxSignal *signal = tm->signal;
         signal->condition = NULL;
      }
      _aaxSignalDestroy(tm->signal);
   }
   free(tm);
}


int
_aaxTimerSetCondition(_aaxTimer *tm, void *condition)
{
    int rv = AAX_FALSE;
    if(tm)
    {
       _aaxSignal *signal = tm->signal;
       if (!signal->condition)
       {
          signal->condition = condition;
          tm->user_condition = AAX_TRUE;
          rv = AAX_TRUE;
       }
    }
    return rv;
}

int
_aaxTimerStartRepeatable(_aaxTimer* tm, float sec)
{
   int rv = AAX_FALSE;
   if (tm && sec > 1e-6f)
   {
      _aaxSignal *signal = tm->signal;

      if (signal->condition == NULL) {
         _aaxSignalInit(tm->signal);
      }

      if (signal->condition)
      {
         tm->dt = sec;
         rv = AAX_TRUE;
      }
   }
   else {
      rv = _aaxTimerStop(tm);
   }
   return rv;
}

int
_aaxTimerStop(UNUSED(_aaxTimer* tm)) {
   return AAX_TRUE;
}

int
_aaxTimerWait(_aaxTimer* tm, void* mutex)
{
   _aaxSignal *signal = tm->signal;
   int rv = AAX_TRUE;

   if (signal->condition)
   {
      void *mtx = signal->mutex;
      int res;

      signal->mutex = mutex;
      res = _aaxSignalWaitTimed(signal, tm->dt);
      signal->mutex = mtx;

      switch(res)
      {
      case ETIMEDOUT:
         rv = AAX_TIMEOUT;
         break;
      case 0:
         rv = AAX_TRUE;
         break;
      default:
         rv = AAX_FALSE;
         break;
      }
   }

   return rv;
}
/* end of highres timing code */

#endif	/* if defined(_WIN32) */

