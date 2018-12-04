/*
 * Written by Erik Hofman
 *
 * Public Domain (www.unlicense.org)
 *
 * This is free and unencumbered software released into the public domain.

 * Anyone is free to copy, modify, publish, use, compile, sell, or distribute
 * this software, either in source code form or as a compiled binary, for any
 * purpose, commercial or non-commercial, and by any means.
 * 
 * In jurisdictions that recognize copyright laws, the author or authors of this
 * software dedicate any and all copyright interest in the software to the
 * public domain. We make this dedication for the benefit of the public at
 * large and to the detriment of our heirs and successors. We intend this
 * dedication to be an overt act of relinquishment in perpetuity of all present
 * and future rights to this software under copyright law.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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

# include <Windows.h>

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

enum {
    WAITABLE_TIMER_EVENT = 0,
    CONDITION_EVENT = 1
} _Event_e;

int gettimeofday(struct timeval*, void*);
int clock_gettime(int, struct timespec*);

# if defined(_MSC_VER)
typedef long    off_t;
#  if SIZEOF_SIZE_T == 4
typedef INT32   ssize_t;
#  else
typedef INT64   ssize_t;
#  endif
# endif

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


typedef struct
{
   struct timeval start;
   unsigned int step_us;

} _aaxTimer;

_aaxTimer* _aaxTimerCreate();
void _aaxTimerDestroy(_aaxTimer*);
int _aaxTimerStartRepeatable(_aaxTimer*, unsigned int);
int _aaxTimerStop(_aaxTimer*);
int _aaxTimerWait(_aaxTimer*);

/* end of highres timing code */

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !__OAL_TIMER_H */

