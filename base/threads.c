/*
 * SPDX-FileCopyrightText: Copyright © 2007-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#endif
#include <stdio.h>
#if HAVE_ASSERT_H
# include <assert.h>
#endif
#if HAVE_TIME_H
# include <time.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif
#if HAVE_VALUES_H
# include <values.h>     /* for EM_VALUE_MAX */
#endif
#include <errno.h>
#include <limits.h>

#include <aax/aax.h>
#include "threads.h"
#include "logging.h"
#include "types.h"

#define DEBUG_TIMEOUT		3

#if HAVE_PTHREAD_H	/* UNIX */

# include <sys/time.h>
# include <sys/resource.h>
# ifdef HAVE_RMALLOC_H
#  include <rmalloc.h>
# else
#  include <string.h>	/* for memcpy */
# endif
# include <sched.h>

// SCHED_ISO may not be defined as it is a reserved value not yet
// implemented in official kernel sources, see linux/sched.h.
#ifndef SCHED_ISO
# define SCHED_ISO 4
#endif


# ifdef __TINYC__
#  if defined( __x86_64__ ) || defined( __i386 )
int
__sync_fetch_and_add(int *variable, int value) {
   asm volatile("lock; xaddl %%eax, %2;"
                  :"=a" (value)                  //Output
                  :"a" (value), "m" (*variable)  //Input
                  :"memory");
   return value;
}
#  endif
# endif

int                     /* Prio is a value in the range -20 to 19 */
_aaxProcessSetPriority(int prio)
{
   pid_t pid = getpid();
   int curr_prio = getpriority(PRIO_PROCESS, pid);
   int rv = 0;

   if (curr_prio > prio)
   {
      errno = 0;
      rv = setpriority(PRIO_PROCESS, pid, prio);
   }

   return rv;
}


#define _TH_SYSLOG(a)	__aax_log(LOG_SYSLOG, 0, (a), 0, LOG_SYSLOG);
#define POLICY		SCHED_OTHER

void *
_aaxThreadCreate()
{
   void *ret  = malloc(sizeof(_aaxThread));
   return ret;
}

/* http://www.linuxjournal.com/article/6799 */
int
_aaxThreadSetAffinity(void *t, int core)
{
#if defined(IRIX)
  return pthread_setrunon_np(core);

#elif defined(CPU_ZERO) && defined(CPU_SET)
   pthread_t *id = t;
   cpu_set_t cpus;
   int rv;

   CPU_ZERO(&cpus);
   CPU_SET(core, &cpus);

   rv = pthread_setaffinity_np(*id, sizeof(cpu_set_t), &cpus);
   if (!pthread_getaffinity_np(*id, sizeof(cpu_set_t), &cpus) &&
       !CPU_ISSET(core, &cpus))
   {
      _TH_SYSLOG("setting thread affinity failed");
   }

   return rv;
#else
# pragma waring implement me
#endif
}

int
_aaxThreadSetPriority(void *t, int prio)
{
   pthread_t sid = pthread_self();
   pthread_t *id = t;
   int min, max, policy;
   int rv = 0;

   if (id == NULL) {
     id = &sid;
   }

   policy = SCHED_OTHER;
   if (prio < ((AAX_HIGH_PRIORITY+AAX_HIGHEST_PRIORITY)/2)) {
      policy = SCHED_RR;
   } else if (prio < ((AAX_NORMAL_PRIORITY+AAX_HIGH_PRIORITY)/2)) {
      policy = SCHED_FIFO;
   }

   min = sched_get_priority_min(policy);
   max = sched_get_priority_max(policy);
   if (min >= 0 && max >= 0)
   {
      struct sched_param sched_param;

      /*
       * The range of scheduling priorities may vary on other POSIX systems,
       * thus it is a good idea for portable applications to use a virtual
       * priority range and map it to the interval given by
       * sched_get_priority_max() and sched_get_priority_min().
       * POSIX.1-2001 requires a spread of at least 32 between the maximum and
       * the minimum values for SCHED_FIFO and SCHED_RR
       */
      prio -= AAX_TIME_CRITICAL_PRIORITY;
      prio = prio*(max-min)/(AAX_IDLE_PRIORITY-AAX_TIME_CRITICAL_PRIORITY);
      prio += min;

      sched_param.sched_priority = prio;
      rv = pthread_setschedparam(*id, policy, &sched_param);
      if (rv == 0)
      {
//       rv = pthread_getschedparam(*id, &policy, &sched_param);
         _TH_SYSLOG("using thread scheduling privilege");
      } else {
         _TH_SYSLOG("no thread scheduling privilege");
      }
   }

   return rv;
}


void
_aaxThreadDestroy(void *t)
{
   assert(t);

   free(t);
   t = 0;
}

int
_aaxThreadStart(void *t,  void *(*handler)(void*), void *arg, UNUSED(unsigned int ms))
{
   struct sched_param sched_param;
   pthread_attr_t attr;
   int ret;

   assert(t != 0);
   assert(handler != 0);

   pthread_attr_init(&attr);

   sched_param.sched_priority = 0;
   pthread_attr_setschedparam(&attr, &sched_param);
#if 0
   ret = pthread_attr_setschedpolicy(&attr, POLICY);
   if (ret != 0) {
      _TH_SYSLOG("no thread scheduling privilege");
   } else {
      _TH_SYSLOG("using thread scheduling privilege");
   }
#endif

   ret = pthread_create(t, &attr, handler, arg);

   return ret;
}

int
_aaxThreadJoin(void *t)
{
   _aaxThread *thread = t;
   int ret;

   assert(t);

   ret = pthread_join(*thread, 0);

   return ret;
}


#if defined(NDEBUG)
void *
_aaxMutexCreate(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;

   if (!m)
   {
      m = calloc(1, sizeof(_aaxMutex));
      if (m) {
         pthread_mutex_init(&m->mutex, NULL);
      }
   }

   return m;
}

#else
void *
_aaxMutexCreateDebug(void *mutex, const char *name, const char *fn)
{
   _aaxMutex *m = (_aaxMutex *)mutex;

   if (!m)
   {
      m = calloc(1, sizeof(_aaxMutex));
      if (m)
      {
         pthread_mutex_init(&m->mutex, NULL);
         m->name = (char *)name;
         m->function = fn;
      }
   }

   return m;
}
#endif

#ifndef NDEBUG
_aaxMutex *
_aaxMutexCreateInt(_aaxMutex *m)
{
   if (m && m->initialized == 0)
   {
      pthread_mutexattr_t mta;
      int status;

      status = pthread_mutexattr_init(&mta);
      if (!status)
      {
# ifndef NDEBUG
         status = pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
# else
         status = pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_NORMAL);
# endif
         if (!status) {
            status = pthread_mutex_init(&m->mutex, &mta);
         }
      }

      if (!status) {
         m->initialized = 1;
      }
   }

   return m;
}
#else
_aaxMutex *
_aaxMutexCreateInt(_aaxMutex *m)
{
   if (m && m->initialized == 0)
   {
      int status = pthread_mutex_init(&m->mutex, NULL);
      if (!status) {
         m->initialized = 1;
      }
   }

   return m;
}
#endif

void
_aaxMutexDestroy(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;

   if (m)
   {
      pthread_mutex_destroy(&m->mutex);
      free(m);
   }

   m = 0;
}

#if defined(NDEBUG)
int
_aaxMutexLock(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (m)
   {
      if (m->initialized == 0) {
         m = _aaxMutexCreateInt(m);
      }

      if (m->initialized != 0) {
#ifndef TIMED_MUTEX
         r = pthread_mutex_lock(&m->mutex);
#else
         struct timespec to;
         to.tv_sec = time(NULL);
         to.tv_nsec = 9000000LL;			// 9 ms
         if (to.tv_nsec > 1000000000LL) {
            to.tv_nsec -= 1000000000LL;
            to.tv_sec++;
         }
         r = pthread_mutex_timedlock(&m->mutex, &to);
         if (r == ETIMEDOUT) {
            printf("mutex timed out after 9 miliseconds in %s line %i\n",
                   file, line);
            abort();
         } else if (r == EDEADLK) {
            printf("dealock in %s line %i\n", file, line);
            abort();
         } else if (r) {
            printf("mutex lock error %i in %s line %i\n", r, file, line);
         }
#endif
      }
   }
   return r;
}
#endif

int
_aaxMutexLockTimed(void *mutex, float dt)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (m)
   {
      if (m->initialized == 0) {
         m = _aaxMutexCreateInt(m);
      }

      if (m->initialized != 0) {
         struct timespec to;
         to.tv_sec = time(NULL);
         to.tv_nsec = dt*1e9f;
         if (to.tv_nsec > 1000000000LL) {
            to.tv_nsec -= 1000000000LL;
            to.tv_sec++;
         }
         r = pthread_mutex_timedlock(&m->mutex, &to);
      }
   }
   return r;
}

int
_aaxMutexLockDebug(void *mutex, char *file, int line)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (m)
   {
      if (m->initialized == 0) {
         mutex = _aaxMutexCreateInt(m);
      }

      if (m->initialized != 0)
      {
         struct timespec to;
#ifndef NDEBUG
# ifdef __GNUC__
         unsigned int mtx;

         mtx = m->mutex.__data.__count;	/* only works for recursive locks */
         if (mtx != 0 && mtx != 1) {
            printf("1. lock mutex = %i\n  %s line %i, for: %s in %s\n"
                   "last called from: %s line %zu\n", mtx, file, line,
                   m->name, m->function, m->last_file, m->last_line);
            r = -mtx;
            abort();
         }
# endif
#endif

         to.tv_sec = time(NULL) + DEBUG_TIMEOUT;
         to.tv_nsec = 0;
         r = pthread_mutex_timedlock(&m->mutex, &to);

#ifndef NDEBUG
         if (r == ETIMEDOUT) {
            printf("mutex timed out after %i seconds\n  %s line %i\n"
                   "  last call from\n  %s line %zu\n",
                   DEBUG_TIMEOUT, file, line,
                   m->last_file, m->last_line);
            abort();
         } else if (r == EDEADLK) {
            printf("dealock in %s line %i\n", file, line);
            abort();
         } else if (r) {
            printf("mutex lock error %i in %s line %i\n", r, file, line);
         }

# ifdef __GNUC__
         mtx = m->mutex.__data.__count;	/* only works for recursive locks */
         if (mtx != 1) {
            printf("2. lock mutex != 1 (%i)\n  %s line %i, for: %s in %s\n"
                    "last called from: %s line %zu\n", mtx, file, line,
                    m->name, m->function, m->last_file, m->last_line);
            r = -mtx;
            abort();
         }
# endif
         m->last_file = file;
         m->last_line = line;
#endif
      }
   }
   return r;
}

#if defined(NDEBUG)
int
_aaxMutexUnLock(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (m) {
      r = pthread_mutex_unlock(&m->mutex);
   }
   return r;
}

#else
int
_aaxMutexUnLockDebug(void *mutex, char *file, int line)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (m)
   {
#ifndef NDEBUG
# ifdef __GNUC__
      unsigned int mtx;

      mtx = m->mutex.__data.__count;
      if (mtx != 1) {
         if (mtx == 0)
            printf("mutex already unlocked in %s line %i, for: %s\n"
                    "last called from: %s line %zu\n",
                     file, line, m->name, m->last_file, m->last_line);
         else
            printf("unlock mutex != 1 (%i) in %s line %i, for: %s in %s\n"
                    "last called from: %s line %zu\n",
                    mtx, file, line, m->name, m->function,
                    m->last_file, m->last_line);
         r = -mtx;
         abort();
      }
# endif
#endif

      r = pthread_mutex_unlock(&m->mutex);
#ifndef NDEBUG
#endif

      m->last_file = file;
      m->last_line = line;
   }
   return r;
}
#endif


_aaxSignal*
_aaxSignalCreate()
{
   _aaxSignal *s = calloc(1, sizeof(_aaxSignal));
   if (s) _aaxSignalInit(s);
   return s;
}

void
_aaxSignalInit(_aaxSignal *signal)
{
   signal->condition = calloc(1, sizeof(_aaxCondition));
   if (!signal->condition) return;

   pthread_cond_init(signal->condition, 0);

   signal->mutex = _aaxMutexCreate(signal->mutex);
}

void
_aaxSignalFree(_aaxSignal *signal)
{
   _aaxMutexDestroy(signal->mutex);

   if (signal->condition)
   {
      pthread_cond_destroy(signal->condition);
      free(signal->condition);
      signal->condition = 0;
   }
}

void
_aaxSignalDestroy(_aaxSignal *signal)
{
   _aaxSignalFree(signal);
   free(signal);
}

int
_aaxSignalWait(_aaxSignal *signal)
{
   int rv;

   if (!signal->triggered)
   {
      _aaxMutex *m = (_aaxMutex *)signal->mutex;

      signal->waiting = AAX_TRUE;
      do {	// wait for _aaxSignalTrigger to set signal->waiting = AAX_FALSE
         rv = pthread_cond_wait(signal->condition, &m->mutex);
      }
      while (signal->waiting == AAX_TRUE);

      switch(rv)
      {
      case 0:
         signal->triggered--;
         rv = AAX_TRUE;
         break;
      default:
         rv = AAX_FALSE;
         break;
      }
   }
   else
   {
      signal->triggered--;
      rv = AAX_TRUE;
   }

   return rv;
}

int
_aaxSignalWaitTimed(_aaxSignal *signal, float timeout)
{
   int rv = AAX_FALSE;

   if (!signal->triggered && timeout > 0.0f)
   {
      _aaxMutex *m = (_aaxMutex *)signal->mutex;
      time_t secs;

      secs = (time_t)floorf(timeout);
      timeout -= secs;

      if(signal->ts.tv_sec == 0) {
         clock_gettime(CLOCK_REALTIME, &signal->ts);
      }

      signal->ts.tv_sec += secs;
      signal->ts.tv_nsec += (long)rintf(timeout*1e9f);
      if (signal->ts.tv_nsec >= 1000000000L)
      {
         signal->ts.tv_sec++;
         signal->ts.tv_nsec -= 1000000000L;
      }

      signal->waiting = AAX_TRUE;
      do {
         rv = pthread_cond_timedwait(signal->condition, &m->mutex, &signal->ts);
         if (rv == ETIMEDOUT) break;
      }
      while (signal->waiting == AAX_TRUE);

      switch(rv)
      {
      case ETIMEDOUT:
         rv = AAX_TIMEOUT;
         break;
      case 0:
         signal->triggered--;
         rv = AAX_TRUE;
         break;
      default:
         rv = AAX_FALSE;
         break;
      }
   }
   else if (signal->triggered > 0)
   {
      signal->triggered--;
      rv = AAX_TRUE;
   }

   return rv;
}


#ifndef NDEBUG
int
_aaxSignalTriggerDebug(_aaxSignal *signal, char *file, int line)
{
  int rv = 0;

   _aaxMutexLockDebug(signal->mutex, file, line);
   if (!signal->triggered)
   {
      /* signaling a condition which isn't waiting gets lost */
      /* try to prevent this situation anyhow.               */
      signal->triggered = 1;
      signal->waiting = AAX_FALSE;
      rv =  pthread_cond_signal(signal->condition);
   }
   _aaxMutexUnLock(signal->mutex);

   return rv;
}
#else
int
_aaxSignalTrigger(_aaxSignal *signal)
{
  int rv = 0;

   _aaxMutexLock(signal->mutex);
   if (!signal->triggered)
   {
      /* signaling a condition which isn't waiting gets lost */
      /* try to prevent this situation anyhow.               */
      signal->triggered = 1;
      signal->waiting = AAX_FALSE;
      rv =  pthread_cond_signal(signal->condition);
   }
   _aaxMutexUnLock(signal->mutex);

   return rv;
}
#endif


inline _aaxSemaphore *
_aaxSemaphoreCreate(unsigned initial)
{
   sem_t *rv = NULL;

   if (initial < SEM_VALUE_MAX)
   {
      rv = malloc(sizeof(sem_t));
      if (rv) {
         sem_init(rv, 0, initial);
      }
   }
   return rv;
}

inline int
_aaxSemaphoreDestroy(_aaxSemaphore *sem)
{
   int rv = sem_destroy(sem) ? AAX_FALSE : AAX_TRUE;
   if (rv) {
      free(sem);
   }
   return rv;
}

#ifndef NDEBUG
inline int
_aaxSemaphoreWaitDebug(_aaxSemaphore *sem, char *file, int line)
{
   struct timespec to;
   int rv;

   to.tv_sec = time(NULL) + DEBUG_TIMEOUT;
   to.tv_nsec = 0;

   rv = sem_timedwait(sem, &to);
   if (rv != 0)
   {
      switch (errno)
      {
      case EINTR:
      case EINVAL:
      case EAGAIN:
      case ETIMEDOUT:
      default:
         printf("semaphore error %i: %s\n  %s line %i\n", errno, strerror(errno), file, line);
         break;
      }
   }
   return rv ? AAX_FALSE : AAX_TRUE;
}
#else
inline int
_aaxSemaphoreWait(_aaxSemaphore *sem)
{
   return sem_wait(sem) ? AAX_FALSE : AAX_TRUE;
}
#endif

inline int
_aaxSemaphoreRelease(_aaxSemaphore *sem)
{
   return sem_post(sem) ? AAX_FALSE : AAX_TRUE;
}


#elif defined( WIN32 )	/* HAVE_PTHREAD_H */

#include <base/dlsym.h>

# ifndef __MINGW32__
int
__sync_fetch_and_add(int *variable, int value) {
   return InterlockedExchangeAdd(variable, value);
}

void*
__sync_lock_test_and_set(void **ptr, void *value) {
   return InterlockedExchangePointer(ptr, value);
}
#endif

							/* --- WINDOWS --- */
int                     /* Prio is a value in the range -20 to 19 */
_aaxProcessSetPriority(int prio)
{
   int rv = 0;

   DWORD curr_priority = GetPriorityClass(GetCurrentProcess());
   DWORD new_priority;

   if (prio < ((AAX_HIGHEST_PRIORITY+AAX_TIME_CRITICAL_PRIORITY)/2)) {
      new_priority = HIGH_PRIORITY_CLASS;
   }
   else if (prio < ((AAX_HIGH_PRIORITY+AAX_HIGHEST_PRIORITY)/2)) {
      new_priority = ABOVE_NORMAL_PRIORITY_CLASS;
   }
   else if (prio < ((AAX_LOWEST_PRIORITY+AAX_HIGH_PRIORITY)/2)) {
      new_priority = NORMAL_PRIORITY_CLASS;
   }
   else if (prio < ((AAX_IDLE_PRIORITY+AAX_LOWEST_PRIORITY)/2)) {
      new_priority = BELOW_NORMAL_PRIORITY_CLASS;
   }
   else {
      new_priority = IDLE_PRIORITY_CLASS;
   }

   if (new_priority > curr_priority) {
      rv = SetPriorityClass(GetCurrentProcess(), new_priority);
   }

   return rv;
}


#define _TH_SYSLOG(a)

/* http://www.slideshare.net/abufayez/pthreads-vs-win32-threads */
/* http://www.ibm.com/developerworks/linux/library/l-ipc2lin3/index.html */

DECL_FUNCTION(AvSetMmThreadCharacteristicsA);
DECL_FUNCTION(AvRevertMmThreadCharacteristics);
DECL_FUNCTION(AvSetMmThreadPriority);

void *
_aaxThreadCreate()
{
   void *ret  = calloc(1, sizeof(_aaxThread));

   if (!pAvSetMmThreadCharacteristicsA)
   {
      void *audio = _aaxIsLibraryPresent("avrt", 0);
      if (audio)
      {
         TIE_FUNCTION(AvSetMmThreadCharacteristicsA);
         TIE_FUNCTION(AvRevertMmThreadCharacteristics);
         TIE_FUNCTION(AvSetMmThreadPriority);
      }
   }

   return ret;
}

int
_aaxThreadSetAffinity(void *t, int core)
{
   _aaxThread *thread = t;
   DWORD_PTR rv;

   rv = SetThreadAffinityMask(thread->handle, 1<<core);
   return rv ? 0 : EINVAL;
}

int
_aaxThreadSetPriority(void *t, int prio)
{
   _aaxThread *thread = t;
   int rv = 0;

// DWORD curr_priority = GetThreadPriority(GetCurrentThread());
   DWORD new_priority;

   if (prio < ((AAX_HIGHEST_PRIORITY+AAX_TIME_CRITICAL_PRIORITY)/2)) {
      new_priority = THREAD_PRIORITY_TIME_CRITICAL;
   }
   else if (prio < ((AAX_HIGH_PRIORITY+AAX_HIGHEST_PRIORITY)/2)) {
      new_priority = THREAD_PRIORITY_HIGHEST;
   }
   else if (prio < ((AAX_NORMAL_PRIORITY+AAX_HIGH_PRIORITY)/2)) {
      new_priority = THREAD_PRIORITY_ABOVE_NORMAL;
   }
   else if (prio < ((AAX_LOW_PRIORITY+AAX_NORMAL_PRIORITY)/2)) {
      new_priority = THREAD_PRIORITY_NORMAL;
   }
   else if (prio < ((AAX_LOWEST_PRIORITY+AAX_LOW_PRIORITY)/2)) {
      new_priority = THREAD_PRIORITY_BELOW_NORMAL;
   }
   else if (prio < ((AAX_IDLE_PRIORITY+AAX_LOWEST_PRIORITY)/2)) {
      new_priority = THREAD_PRIORITY_LOWEST;
   }
   else {
      new_priority = THREAD_PRIORITY_IDLE;
   }

   SetThreadPriority(thread, new_priority);
   if (!rv) {
      rv = GetLastError();
   }

   return rv;
}

void
_aaxThreadDestroy(void *t)
{
   _aaxThread *thread = t;

   assert(t);

   if (thread->handle)
   {
      CloseHandle(thread->handle);
      thread->handle = 0;
   }
   free(t);
   t = 0;
}

static DWORD WINAPI
_callback_handler(LPVOID t)
{
   _aaxThread *thread = t;

   if (pAvSetMmThreadCharacteristicsA)
   {
      DWORD tIdx = 0;
      if (thread->ms >= 10) {
         thread->task = pAvSetMmThreadCharacteristicsA("Audio", &tIdx);
      }
      else
      {
         thread->task = pAvSetMmThreadCharacteristicsA("Pro Audio", &tIdx);
         if (thread->task && pAvSetMmThreadPriority) {
            pAvSetMmThreadPriority(thread->task, AVRT_PRIORITY_HIGH);
         }
      }
   }

   thread->callback_fn(thread->callback_data);

   if (thread->task)
   {
     if (pAvRevertMmThreadCharacteristics) {
         pAvRevertMmThreadCharacteristics(thread->task);
      }
      thread->task = NULL;
   }

   return 0;
}

int
_aaxThreadStart(void *t,  void *(*handler)(void*), void *arg, unsigned int ms)
{
   _aaxThread *thread = t;
   int rv = -1;

   thread->ms = ms;
   thread->callback_fn = handler;
   thread->callback_data = arg;
   thread->handle = CreateThread(NULL, 0, _callback_handler, t, 0, NULL);
   if (thread->handle != NULL) {
     rv = 0;
   }

   return rv;
}

int
_aaxThreadJoin(void *t)
{
   _aaxThread *thread = t;
   int ret = 0;
   DWORD r;

   assert(t);

   r = WaitForSingleObject(thread->handle, INFINITE);
   switch (r)
   {
   case WAIT_OBJECT_0:
      break;
   case WAIT_TIMEOUT:
      r = ETIMEDOUT;
      break;
   case WAIT_ABANDONED:
      ret = EINVAL;
      break;
   case WAIT_FAILED:
   default:
      ret = ESRCH;
   }

   return ret;
}

/*
 * In debugging mode we use real mutexes with a timeout value.
 * In release mode use critical sections which could be way faster
 *    for single process applications.
 */
#if defined(NDEBUG)
void *
_aaxMutexCreate(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;

   if (!m)
   {
      m = calloc(1, sizeof(_aaxMutex));
      if (m)
      {
	 m->mutex = CreateMutex(NULL, FALSE, NULL);
         InitializeCriticalSection(&m->crit);
      }
   }

   return m;
}
#else
void *
_aaxMutexCreateDebug(void *mutex, const char *name, const char *fn)
{
   _aaxMutex *m = (_aaxMutex *)mutex;

   if (!m)
   {
      m = calloc(1, sizeof(_aaxMutex));
      if (m)
      {
         m->mutex = CreateMutex(NULL, FALSE, NULL);
         m->name = (char *)name;
         m->function = fn;
      }
   }

   return m;
}
#endif

_aaxMutex *
_aaxMutexCreateInt(_aaxMutex *m)
{
   if (m && m->initialized == 0)
   {
#if defined(NDEBUG)
      m->mutex = CreateMutex(NULL, FALSE, NULL);
      InitializeCriticalSection(&m->crit);
      m->initialized = 1;
#else
      m->mutex = CreateMutex(NULL, FALSE, NULL);
      m->initialized = (m->mutex) ? 1 : 0;
#endif
   }

   return m;
}

void
_aaxMutexDestroy(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;

   if (m)
   {
#if defined(NDEBUG)
      CloseHandle(m->mutex);
      DeleteCriticalSection(&m->crit);
#else
      CloseHandle(m->mutex);
#endif
      free(m);
   }

   m = 0;
}

#if defined(NDEBUG)
int
_aaxMutexLock(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (m)
   {
      if (m->initialized == 0) {
         m = _aaxMutexCreateInt(m);
      }

      if (m->initialized != 0) {
         EnterCriticalSection(&m->crit);
      }
   }
   return r;
}
#endif

int
_aaxMutexLockTimed(void *mutex, float dt)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (m)
   {
      if (m->initialized == 0) {
         m = _aaxMutexCreateInt(m);
      }

      if (m->initialized != 0)
      {
         m->waiting = 1;
         r = WaitForSingleObject(m->mutex, dt*1000);
         switch (r)
         {
         case WAIT_OBJECT_0:
            break;
         case WAIT_TIMEOUT:
            r = ETIMEDOUT;
            break;
         case WAIT_ABANDONED:
         case WAIT_FAILED:
         default:
	    break;
         }
      }
   }
   return r;
}

int
_aaxMutexLockDebug(void *mutex, char *file, int line)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (m)
   {
      if (m->initialized == 0) {
         m = _aaxMutexCreateInt(m);
      }

      if (m->initialized != 0)
      {
#if defined(NDEBUG)
         EnterCriticalSection(&m->crit);
         r = WAIT_OBJECT_0;
#else
         r = WaitForSingleObject(m->mutex, DEBUG_TIMEOUT*1000);
#endif
         switch (r)
         {
         case WAIT_OBJECT_0:
            break;
         case WAIT_TIMEOUT:
            printf("mutex timed out after %i seconds in %s line %i\n",
                    DEBUG_TIMEOUT, file, line);
            abort();
            r = ETIMEDOUT;
            break;
         case WAIT_ABANDONED:
         case WAIT_FAILED:
         default:
            printf("mutex lock error %i in %s line %i\n", r, file, line);
            abort();
         }
      }
   }
   return r;
}

#if defined(NDEBUG)
int
_aaxMutexUnLock(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (m)
   {
      if (m->waiting) {
         ReleaseMutex(m->mutex);
      } else {
         LeaveCriticalSection(&m->crit);
      }
      m->waiting = 0;
   }
   return r;
}

#else
int
_aaxMutexUnLockDebug(void *mutex, char *file, int line)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = EINVAL;

   if (m)
   {
      ReleaseMutex(m->mutex);
      r = 0;

      m->last_file = file;
      m->last_line = line;
   }
   return r;
}
#endif

_aaxSignal*
_aaxSignalCreate()
{
   _aaxSignal *s = calloc(1, sizeof(_aaxSignal));
   if (s) _aaxSignalInit(s);
   return s;
}

void
_aaxSignalInit(_aaxSignal *signal)
{
   signal->condition = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (!signal->condition) return;

   signal->mutex = _aaxMutexCreate(signal->mutex);
}

void
_aaxSignalFree(_aaxSignal *signal)
{
   _aaxMutexDestroy(signal->mutex);

   if (signal->condition)
   {
      CloseHandle(signal->condition);
      signal->condition = 0;
   }
}

void
_aaxSignalDestroy(_aaxSignal *signal)
{
   _aaxSignalFree(signal);
   free(signal);
}


/*
 * See:
 * http://www.codeproject.com/Articles/18371/Fast-critical-sections-with-timeout
 */
int
_aaxSignalWait(_aaxSignal *signal)
{
   int rv;

   if (!signal->triggered)
   {
      _aaxMutex *mutex = (_aaxMutex *)signal->mutex;
      DWORD hr;

      signal->waiting = AAX_TRUE;
      do
      {
         _aaxMutexUnLock(mutex);
         hr = WaitForSingleObject(signal->condition, INFINITE);
         _aaxMutexLock(mutex);
      }
      while (signal->waiting == AAX_TRUE);

      switch (hr)
      {
      case WAIT_OBJECT_0:
         signal->triggered--;
         rv = AAX_TRUE;
         break;
      default:
         rv = AAX_FALSE;
         break;
      }
   }
   else
   {
      signal->triggered--;
      rv = AAX_TRUE;
   }

   return rv;
}

int
_aaxSignalWaitTimed(_aaxSignal *signal, float timeout)
{
   int rv = AAX_FALSE;

   if (!signal->triggered && timeout > 0.0f)
   {
      _aaxMutex *mutex = (_aaxMutex *)signal->mutex;
      DWORD hr;

      if (timeout > 0.0f)	
      {
         timeout *= 1000.0f;		/* from seconds to ms */
         signal->waiting = AAX_TRUE;
         do
         {
            _aaxMutexUnLock(mutex);
            hr = WaitForSingleObject(signal->condition, (DWORD)floorf(timeout));
            _aaxMutexLock(mutex);
         }
         while (signal->waiting == AAX_TRUE);

         switch (hr)
         {
         case WAIT_TIMEOUT:
            rv = AAX_TIMEOUT;
            break;
         case WAIT_OBJECT_0:
            signal->triggered--;
            rv = AAX_TRUE;
            break;
         default:
            rv = AAX_FALSE;
            break;
         }
      } else {		/* timeout */
         signal->triggered--;
         rv = AAX_TIMEOUT;
      }
   }
   else if (signal->triggered > 0)
   {
      signal->triggered--;
      rv = AAX_TRUE;
   }

   return rv;
}

#ifndef NDEBUG
int
_aaxSignalTriggerDebug(_aaxSignal *signal, char *file, int line)
{
   BOOL rv = 0;

   _aaxMutexLockDebug(signal->mutex, file, line);
   if (!signal->triggered)
   {
      signal->triggered = 1;
      signal->waiting = AAX_FALSE;
      rv = SetEvent(signal->condition);

      /* same return value as pthread_cond_signal() */
      if (rv) rv = 0;
      else rv = GetLastError();
   }
   _aaxMutexUnLock(signal->mutex);

   return rv;
}
#else
int
_aaxSignalTrigger(_aaxSignal *signal)
{
   BOOL rv = 0;

   _aaxMutexLock(signal->mutex);
   if (!signal->triggered)
   {
      signal->triggered = 1;
      signal->waiting = AAX_FALSE;
      rv = SetEvent(signal->condition);

      /* same return value as pthread_cond_signal() */
      if (rv) rv = 0;
      else rv = GetLastError();
   }
   _aaxMutexUnLock(signal->mutex);

   return rv;
}
#endif

inline _aaxSemaphore *
_aaxSemaphoreCreate(unsigned initial)
{
   _aaxSemaphore *rv = CreateSemaphore(NULL, initial, 32765, NULL);
   return rv;
}

inline int
_aaxSemaphoreDestroy(_aaxSemaphore *sem)
{
   CloseHandle(sem);
   return AAX_TRUE;
}

#ifndef NDEBUG
inline int
_aaxSemaphoreWaitDebug(_aaxSemaphore *sem, char *file, int line)
{
   DWORD r = WaitForSingleObject(sem, INFINITE);
   return (r == WAIT_OBJECT_0) ? AAX_TRUE : AAX_FALSE;
}

#else
inline int
_aaxSemaphoreWait(_aaxSemaphore *sem)
{
   DWORD r = WaitForSingleObject(sem, INFINITE);
   return (r == WAIT_OBJECT_0) ? AAX_TRUE : AAX_FALSE;
}
#endif

inline int
_aaxSemaphoreRelease(_aaxSemaphore *sem)
{
   return ReleaseSemaphore(sem, 1, NULL) ? AAX_TRUE : AAX_FALSE;
}

#else
# error "threads not implemented for this platform"
#endif /* HAVE_PTHREAD_H */

