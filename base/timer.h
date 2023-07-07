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

#ifndef __OAL_TIMER_H
#define __OAL_TIMER_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "types.h"

#ifdef _WIN32

# ifdef HAVE_TIME_H
#  include <time.h>             /* for nanosleep */
# endif
# if HAVE_SYS_TIME_H
#  include <sys/time.h>         /* for struct timeval */
# endif

# ifndef ETIMEDOUT
#  define ETIMEDOUT			WSAETIMEDOUT
# endif
# define CLOCK_REALTIME			0
# define CLOCK_MONOTONIC		1

typedef enum {
    WAITABLE_TIMER_EVENT = 0,
    CONDITION_EVENT = 1
} _Event_e;

int gettimeofday(struct timeval*, void*);
int clock_gettime(int, struct timespec*);

typedef long    off_t;
typedef INT64   ssize_t;

#else	/* _WIN32 */
# if HAVE_SYS_TIME_H
#  include <sys/time.h>         /* for gettimeofday */
# endif
#endif	/* _WIN32 */

int usecSleep(unsigned int);
int msecSleep(unsigned int);
unsigned int getTimerResolution();
int setTimerResolution(unsigned int);
int resetTimerResolution(unsigned int);


/** highres timing code */
#ifdef _WIN32
typedef struct
{
   double tfreq;
   LARGE_INTEGER timerOverhead;
   LARGE_INTEGER prevTimerCount;
   LARGE_INTEGER timerCount;

   /* repeatable */
   HANDLE Event[2];	/* if Event != NULL the timer is repeatable */
   LARGE_INTEGER dueTime;
   LONG Period;

} _aaxTimer;
#else
typedef struct
{
   double tfreq;
   struct timespec timerOverhead;
   struct timespec prevTimerCount;
   struct timespec timerCount;

   /* repeatable */
   float dt;
   void *signal;		/* repeatable when signal->condition != NULL */
   char user_condition;

} _aaxTimer;
#endif

_aaxTimer* _aaxTimerCreate();
void _aaxTimerDestroy(_aaxTimer*);
double _aaxTimerGetFrequency(_aaxTimer*);
double _aaxTimerElapsed(_aaxTimer*);
void _aaxTimerStart(_aaxTimer*);

/* repeatable */
int _aaxTimerSetCondition(_aaxTimer*, void*);
int _aaxTimerStartRepeatable(_aaxTimer*, float);
int _aaxTimerStop(_aaxTimer*);
int _aaxTimerWait(_aaxTimer*, void*);

/* end of highres timing code */

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !__OAL_TIMER_H */

