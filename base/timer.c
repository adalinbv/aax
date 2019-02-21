/*
 * Written by Erik Hofman.
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
#    include <strings.h>
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

    The timezone pointer arg is ignored.    Errors are ignored.
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

// https://gist.github.com/Youka/4153f12cf2e17a77314c
/* Windows sleep in 100ns units */
int usecSleep(unsigned int dt_us)
{
    /* Declarations */
    LONGLONG ns = dt_us*10;
    HANDLE timer;	/* Timer handle */
    LARGE_INTEGER li;	/* Time defintion */
    /* Create timer */
    if(!(timer = CreateWaitableTimer(NULL, TRUE, NULL))) {
    	return -1;
    }
    /* Set timer properties */
    li.QuadPart = -ns;
    if(!SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE)) {
    	CloseHandle(timer);
    	return -1;
    }
    /* Start & wait for timer */
    WaitForSingleObject(timer, INFINITE);
    /* Clean resources */
    CloseHandle(timer);
    /* Slept without problems */
    return 0;
}

/* end of highres timing code */

#else	/* _WIN32 */

/*
 * dt_ms == 0 is a special case which make the time-slice available for other
 * waiting processes
 */
#include <poll.h>
#include <unistd.h>
int msecSleep(unsigned int dt_ms)
{
    if (dt_ms > 0)
    {
        struct timeval delay;
        delay.tv_sec = 0;
        delay.tv_usec = dt_ms*1000;
        do {
            (void) select(0, NULL, NULL, NULL, &delay);
        } while ((delay.tv_usec > 0) || (delay.tv_sec > 0));
        return 0;
    }
    else {
        return sleep(0);
    }
    return 0;
}

int usecSleep(unsigned int dt_us)
{
    static struct timespec s;
    if (dt_us > 0)
    {
        s.tv_sec = (dt_us/1000000);
        s.tv_nsec = (dt_us % 1000000)*1000L;
        while(nanosleep(&s,&s)==-1 && errno == EINTR)
        continue;
    }
    else {
        return sleep(0);
    }
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
    assert(us == us); // keep the compiler happy in strict mode

    timer->step_us = 1000;
    gettimeofday(&timer->start, NULL);
    return 0;
}

int _aaxTimerStop(_aaxTimer* timer)
{
    assert(timer);
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

