/*
 * SPDX-FileCopyrightText: Copyright © 2007-2024 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2024 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
# include "config.h"
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

#include "xthreads.h"
#include "logging.h"
#include "types.h"

#define DEBUG_TIMEOUT		3
#define _TH_SYSLOG(a)		__aax_log(LOG_SYSLOG, 0, (a), 0, LOG_SYSLOG);

#if defined(__FreeBSD__)
void
atomic_pointer_swap(void **ptr1, void **ptr2) {
   *ptr2 = __sync_lock_test_and_set(ptr1, *ptr2);
}
#else
void
atomic_pointer_swap(void **ptr1, void **ptr2)
{
   _Atomic(void *) *atomic_ptr1 = (_Atomic(void *) *)ptr1;
   _Atomic(void *) *atomic_ptr2 = (_Atomic(void *) *)ptr2;
   atomic_store(atomic_ptr2, atomic_exchange(atomic_ptr1, *ptr2));
}
#endif

// Note: Requires Visual Studio version 17.8 Preview 2 or later.
//    It’s shipped as a new satellite DLL of vcruntime:
//    vcruntime140_threads.dll and vcruntime140_threadsd.dll.
//    If you use the dynamic version of the Visual C++ runtime (/MD or /MDd),
//    and you use the new threads facilities, then you need to either
//    redistribute this file with your app, or redistribute a Visual C++
//    runtime redist that is new enough to contain these files.
//
//    A key difference between Visual Studio's implementation and C11 threads
//    implementations based on pthreads is that threads can not detach
//    themselves using thrd_current() and thrd_detach().

#if HAVE_PTHREAD_H	/* UNIX */

# if HAVE_PTHREAD_NP_H
#  include <pthread_np.h>
# endif
# include <sys/time.h>
# include <sys/resource.h>
# ifdef HAVE_RMALLOC_H
#  include <rmalloc.h>
# else
#  include <string.h>	/* for memcpy */
# endif
# include <sched.h>

inline _aaxSemaphore *
_aaxSemaphoreCreate(unsigned initial)
{
   sem_t *rv = NULL;

   if (initial < SEM_VALUE_MAX)
   {
      rv = calloc(1, sizeof(sem_t));
      if (rv) {
         sem_init(rv, 0, initial);
      }
   }
   return rv;
}

inline int
_aaxSemaphoreDestroy(_aaxSemaphore *sem)
{
   int rv = sem_destroy(sem) ? false : true;
   if (rv) {
      free(sem);
   }
   return rv;
}

#ifndef NDEBUGTHREADS
inline int
_aaxSemaphoreWaitDebug(_aaxSemaphore *sem, char *file, int line)
{
   struct timespec to;
   int rv;

   timespec_get(&to, TIME_UTC);
   to.tv_sec += DEBUG_TIMEOUT;

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
   return rv ? false : true;
}
#endif

inline int
_aaxSemaphoreWaitNoTimeout(_aaxSemaphore *sem)
{
   return sem_wait(sem) ? false : true;
}

inline int
_aaxSemaphoreRelease(_aaxSemaphore *sem)
{
   return sem_post(sem) ? false : true;
}

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

#elif defined( WIN32 )	/* WINDOWS */

#include <processthreadsapi.h>
#include <base/dlsym.h>

/* http://www.slideshare.net/abufayez/pthreads-vs-win32-threads */
/* http://www.ibm.com/developerworks/linux/library/l-ipc2lin3/index.html */

DECL_FUNCTION(AvSetMmThreadCharacteristicsA);
DECL_FUNCTION(AvRevertMmThreadCharacteristics);
DECL_FUNCTION(AvSetMmThreadPriority);

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
   return true;
}

#ifndef NDEBUGTHREADS
inline int
_aaxSemaphoreWaitDebug(_aaxSemaphore *sem, char *file, int line)
{
   DWORD r = WaitForSingleObject(sem, INFINITE);
   return (r == WAIT_OBJECT_0) ? true : false;
}
#endif

inline int
_aaxSemaphoreWaitNoTimeout(_aaxSemaphore *sem)
{
   DWORD r = WaitForSingleObject(sem, INFINITE);
   return (r == WAIT_OBJECT_0) ? true : false;
}

inline int
_aaxSemaphoreRelease(_aaxSemaphore *sem)
{
   return ReleaseSemaphore(sem, 1, NULL) ? true : false;
}

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

#ifdef __MINGW32__
# include "xthreads_win32.c"
#endif

#else
# error "threads not implemented for this platform"
#endif /* HAVE_PTHREAD_H */

#ifndef __MINGW32__
/** Threads */
void *
_aaxThreadCreate()
{
   void *ret  = calloc(1, sizeof(_aaxThread));
   return ret;
}

void
_aaxThreadDestroy(_aaxThread *t)
{
   assert(t);

   free(t);
   t = 0;
}

int
_aaxThreadStart(_aaxThread *t,  int(*handler)(void*), void *arg, UNUSED(unsigned int ms), const char *name)
{
   int ret = thrd_create(t, handler, arg);
   if (ret == thrd_success && name)
   {
#if HAVE_PTHREAD_H
      pthread_t *id = t;
      pthread_setname_np(*id, name);
#elif defined(WIN32)
      const size_t BUFSIZE = 100;
      wchar_t buf[BUFSIZE];
      MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, name, -1, buf, BUFSIZE);
      SetThreadDescription(t, buf);
#else
# pragma waring implement me
#endif

      _aaxThreadSetPriority(t, AAX_HIGH_PRIORITY);
     AAX_SET_FLUSH_ZERO_ON;
   }

   return ret;
}

int
_aaxThreadJoin(_aaxThread *t)
{
   assert(t);

   return thrd_join(*t, 0);
}

int
_aaxThreadSetAffinity(_aaxThread *t, int core)
{
   int rv = 0;

#if defined(WIN32)
   rv = SetThreadAffinityMask(*t, 1<<core) ? 0 : EINVAL;
#elif defined(CPU_ZERO) && defined(CPU_SET)
   pthread_t *id = t;
   cpu_set_t cpus;

   CPU_ZERO(&cpus);
   CPU_SET(core, &cpus);

   rv = pthread_setaffinity_np(*id, sizeof(cpu_set_t), &cpus);
   if (!pthread_getaffinity_np(*id, sizeof(cpu_set_t), &cpus) &&
       !CPU_ISSET(core, &cpus))
   {
      _TH_SYSLOG("setting thread affinity failed");
   }
#else
# pragma waring implement me
#endif
   return rv;
}

int
_aaxThreadSetPriority(_aaxThread *t, int prio)
{
   int rv = 0;
#if HAVE_PTHREAD_H
   pthread_t sid = pthread_self();
   pthread_t *id = t;
   int min, max, policy;

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
#elif defined(WIN32)
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

   SetThreadPriority(t, new_priority);
   if (!rv) {
      rv = GetLastError();
   }
#else
# pragma waring implement me
#endif

   return rv;
}

/** Mutex **/
#if defined(NDEBUGTHREADS)
void*
_aaxMutexCreate(_aaxMutex *m)
{
   if (!m)
   {
      m = calloc(1, sizeof(_aaxMutex));
      if (m)
      {
         int status = mtx_init(&m->mutex, mtx_plain);
         if (status == thrd_success) {
            m->status = AAX_THREAD_INITIALIZED;
         }
      }
   }
   return m;
}
#else
void *
_aaxMutexCreateDebug(_aaxMutex *m, const char *name, const char *fn)
{
   if (!m)
   {
      m = calloc(1, sizeof(_aaxMutex));
      if (m)
      {
         int status = mtx_init(&m->mutex, mtx_timed|mtx_recursive);
         if (status == thrd_success) {
            m->status = AAX_THREAD_INITIALIZED;
         }
         m->name = (char *)name;
         m->function = fn;
      }
   }

   return m;
}
#endif

void
_aaxMutexDestroy(_aaxMutex *m)
{
   if (m)
   {
      mtx_destroy(&m->mutex);
      m->status = AAX_THREAD_DESTROYED;
      free(m);
   }
}

#ifdef NDEBUGTHREADS
int
_aaxMutexLock(_aaxMutex *m)
{
   int r = EINVAL;
   if (m)
   {
      r = (mtx_lock(&m->mutex) != thrd_success);
      if (!r) m->status = AAX_THREAD_LOCKED;
   }
   return r;
}
#endif

int
_aaxMutexLockDebug(_aaxMutex *m, char *file, int line)
{
   int r = EINVAL;
   if (m)
   {
      if (m->status == AAX_THREAD_INITIALIZED ||
          m->status == AAX_THREAD_UNLOCKED ||
          m->status == AAX_THREAD_LOCKED)
      {
         struct timespec to;
         int res;
#ifdef __LINUX__
         unsigned int mtx_count;

         /* only works for recursive locks */
         pthread_mutex_t *mutex = (pthread_mutex_t*)&m->mutex;
         mtx_count = mutex->__data.__count;
         if (mtx_count != 0 && mtx_count != 1) {
            printf("1. lock mutex = %i\n  %s line %i, for: %s in %s\n"
                   "last called from: %s line %zu\n", mtx_count, file, line,
                   m->name, m->function, m->last_file, m->last_line);
            r = -mtx_count;
            abort();
         }
#endif

         to.tv_sec = time(NULL) + DEBUG_TIMEOUT;
         to.tv_nsec = 0;

         res = mtx_timedlock(&m->mutex, &to);
         if (res == thrd_success) {
            m->status = AAX_THREAD_LOCKED;
         }
         else if (res == thrd_timedout)
         {
            printf("mutex timed out after %i seconds\n  %s line %i\n"
                   "  last call from\n  %s line %zu\n",
                   DEBUG_TIMEOUT, file, line,
                   m->last_file, m->last_line);
            abort();
         }
         else if (res == thrd_busy)
         {
            printf("dealock in %s line %i\n", file, line);
            abort();
         }
         else if (res == thrd_error) {
            printf("mutex lock error %i in %s line %i\n", r, file, line);
         } else if (res == thrd_nomem) {
            printf("out of memory error %i in %s line %i\n", r, file, line);
         }

#ifdef __LINUX__
         /* only works for recursive locks */
         mtx_count = mutex->__data.__count;
         if (mtx_count != 1) {
            printf("2. lock mutex != 1 (%i)\n  %s line %i, for: %s in %s\n"
                    "last called from: %s line %zu\n", mtx_count, file,
                    line, m->name, m->function, m->last_file, m->last_line);
            r = -mtx_count;
            abort();
         }
#endif
         m->last_file = file;
         m->last_line = line;
      }
      else
      {
         printf("locking an uninitialized mutex in %s line %i\n", file, line);
         abort();
      }
   }
   return r;
}

int
_aaxMutexLockTimed(_aaxMutex *m, float dt)
{
   int r = 0;
   if (m)
   {
      struct timespec to;

#ifdef __MINGW32__
      clock_gettime(CLOCK_REALTIME, &to);
#else
      timespec_get(&to, TIME_UTC);
#endif
      to.tv_nsec += dt*1e9f;
      if (to.tv_nsec >= 1000000000LL) {
         to.tv_nsec -= 1000000000LL;
         to.tv_sec++;
      }

      r = (mtx_timedlock(&m->mutex, &to) != thrd_success);
      if (!r) m->status = AAX_THREAD_LOCKED;
   }
   return r;
}

#if defined(NDEBUGTHREADS)
int
_aaxMutexUnLock(_aaxMutex *m)
{
   int r = EINVAL;
   if (m)
   {
      r = (mtx_unlock(&m->mutex) != thrd_success);
      if (!r) m->status = AAX_THREAD_UNLOCKED;
   }
   return r;
}
#else
int
_aaxMutexUnLockDebug(_aaxMutex *m, char *file, int line)
{
   int r = EINVAL;
   if (m)
   {
# ifdef __LINUX__
      unsigned int mtx_count;
      pthread_mutex_t *mutex = (pthread_mutex_t*)&m->mutex;
      mtx_count = mutex->__data.__count;
      if (mtx_count != 1) {
         if (mtx_count == 0) {
            printf("mutex already unlocked in %s line %i, for: %s\n"
                    "last called from: %s line %zu\n",
                     file, line, m->name, m->last_file, m->last_line);
         } else {
            printf("unlock mutex != 1 (%i) in %s line %i, for: %s in %s\n"
                    "last called from: %s line %zu\n",
                    mtx_count, file, line, m->name, m->function,
                    m->last_file, m->last_line);
         }
         r = -mtx_count;
         abort();
      }
# endif

      r = (mtx_unlock(&m->mutex) != thrd_success);
      if (!r) m->status = AAX_THREAD_UNLOCKED;

      m->last_file = file;
      m->last_line = line;
   }
   return r;
}
#endif

/** Conditions/Signals **/
_aaxSignal*
_aaxSignalCreate()
{
   _aaxSignal *s = calloc(1, sizeof(_aaxSignal));
   if (s) _aaxSignalInit(s);
   return s;
}

int
_aaxSignalInit(_aaxSignal *signal)
{
   int rv = thrd_error;

   if (!signal->condition) {
      signal->condition = calloc(1, sizeof(cnd_t));
   }

   if (signal->condition)
   {
      rv = cnd_init(signal->condition);
      if (rv == thrd_success) {
         signal->mutex = _aaxMutexCreate(signal->mutex);
      }
   }

   return (rv == thrd_success) ? 0 : EINVAL;
}

void
_aaxSignalFree(_aaxSignal *signal)
{
   _aaxMutexDestroy(signal->mutex);
   signal->mutex = NULL;

   if (signal->condition)
   {
      cnd_destroy(signal->condition);
      free(signal->condition);
      signal->condition = NULL;
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
   int rv = thrd_success;

   if (!signal->triggered)
   {
      _aaxMutex *m = signal->mutex;
      signal->waiting = true;
      do {      // wait for _aaxSignalTrigger to set signal->waiting = false
         rv = cnd_wait(signal->condition, &m->mutex);
      }
      while (signal->waiting == true);

      switch(rv)
      {
      case thrd_success:
         signal->triggered--;
         break;
      default:
         break;
      }
   }
   else {
      signal->triggered--;
   }

   return (rv == thrd_success) ? 0 : EINVAL;
}

int
_aaxSignalWaitTimed(_aaxSignal *signal, float dt)
{
   int rv = false;

   if (!signal->triggered && dt > 0.0f)
   {
      _aaxMutex *m = signal->mutex;
      struct timespec to;

#ifdef __MINGW32__
      clock_gettime(CLOCK_REALTIME, &to);
#else
      timespec_get(&to, TIME_UTC);
#endif
      to.tv_nsec += dt*1e9f;
      if (to.tv_nsec >= 1000000000LL) {
         to.tv_nsec -= 1000000000LL;
         to.tv_sec++;
      }

      signal->waiting = true;
      do { // wait for _aaxSignalTrigger to set signal->waiting = false
         rv = cnd_timedwait(signal->condition, &m->mutex, &to);
         if (rv == thrd_timedout) break;
      }
      while (signal->waiting == true);

      switch(rv)
      {
      case thrd_timedout:
         rv = AAX_TIMEOUT;
         break;
      case thrd_success:
         signal->triggered--;
         rv = true;
         break;
      default:
         rv = false;
         break;
      }
   }
   else if (signal->triggered > 0) {
      signal->triggered--;
   }

   return rv;
}

int
_aaxSignalTrigger(_aaxSignal *signal)
{
  int rv = thrd_success;

   _aaxMutexLock(signal->mutex);
   if (!signal->triggered)
   {
      /* signaling a condition which isn't waiting gets lost */
      /* try to prevent this situation anyhow.               */
      signal->triggered = 1;
      signal->waiting = false;
      rv = cnd_signal(signal->condition);
   }
   _aaxMutexUnLock(signal->mutex);

   return (rv == thrd_success) ? 0 : EINVAL;
}

#endif /* __MINGW32__ */
