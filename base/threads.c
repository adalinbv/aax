
#if HAVE_CONFIG_H
#include "config.h"
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
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <errno.h>

#include <aax/aax.h>
#include "threads.h"
#include "logging.h"
#include "types.h"

#define DEBUG_TIMEOUT		3
static char __threads_enabled = 0;

#if HAVE_PTHREAD_H
# include <sys/time.h>
# include <sys/resource.h>
# ifdef HAVE_RMALLOC_H
#  include <rmalloc.h>
# else
#  include <string.h>	/* for memcpy */
# endif

int                     /* Prio is a value in the range -20 to 19 */
_aaxProcessSetPriority(int prio)
{
   int rv = 0;
   int curr_prio = getpriority(PRIO_PROCESS, getpid());
   if (curr_prio < prio)
   {
      errno = 0;
      rv = setpriority(PRIO_PROCESS, getpid(), prio);
      if (rv < 0) {
         rv = errno;
     }
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
# include <sched.h>
int
_aaxThreadSetAffinity(void *t, int core)
{
#if defined(IRIX)
   pthread_setrunon_np(core);

#elif defined(CPU_ZERO) && defined(CPU_SET)
   pthread_t *id = t;
   cpu_set_t cpus;

   CPU_ZERO(&cpus);
   CPU_SET(core, &cpus);
   return pthread_setaffinity_np(*id, sizeof(cpu_set_t), &cpus);
#else
# pragma waring implement me
#endif
}

int
_aaxThreadSetPriority(void *t, int prio)
{
   pthread_t *id = t;
   int min, max, policy;
   int rv = 0;

   if (prio >= AAX_HIGHEST_PRIORITY) {
      policy = SCHED_FIFO;
   } else if (prio == AAX_HIGH_PRIORITY) {
      policy = SCHED_RR;
   } else {
      policy = SCHED_OTHER;
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
      prio = (prio-AAX_TIME_CRITICAL_PRIORITY);
      prio *= abs(max-min)/abs(AAX_TIME_CRITICAL_PRIORITY-AAX_IDLE_PRIORITY);
      prio += min;

      sched_param.sched_priority = prio;
//    rv = sched_setparam(*id, &sched_param);
      rv = pthread_setschedparam(*id, policy, &sched_param);
      if (rv != 0) {
         _TH_SYSLOG("no thread scheduling privilege");
      } else {
         _TH_SYSLOG("using thread scheduling privilege");
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
_aaxThreadStart(void *t,  void *(*handler)(void*), void *arg, unsigned int ms)
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
   if (ret == 0) {
      __threads_enabled = 1;
   }

   return ret;
}

int
_aaxThreadJoin(void *t)
{
   _aaxThread *thread = t;
   int ret;

   assert(t);

   __threads_enabled = 0;
   ret = pthread_join(*thread, 0);

   return ret;
}

int
_aaxThreadSwitch()
{
   return pthread_yield() ? -1 : 0;
}


#if defined(NDEBUG)
void *
_aaxMutexCreate(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;

   if (!m) {
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
      if (m) {
         pthread_mutex_init(&m->mutex, NULL);
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
      int status;
#if 1
      pthread_mutexattr_t mta;

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
#else
      status = pthread_mutex_init(&m->mutex, NULL);
#endif

      if (!status) {
         m->initialized = 1;
      }
   }

   return m;
}

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
            printf("mutex timed out in %s line %i\n", file, line);
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
            printf("1. lock mutex = %i in %s line %i, for: %s in %s\n",
                                         mtx, file, line, m->name, m->function);
            r = -mtx;
            abort();
         }
# endif
#endif

         to.tv_sec = time(NULL) + DEBUG_TIMEOUT;
         to.tv_nsec = 0;
         r = pthread_mutex_timedlock(&m->mutex, &to);

         if (r == ETIMEDOUT) {
            printf("mutex timed out in %s line %i\n", file, line);
            abort();
         } else if (r == EDEADLK) {
            printf("dealock in %s line %i\n", file, line);
            abort();
         } else if (r) {
            printf("mutex lock error %i in %s line %i\n", r, file, line);
         }

#ifndef NDEBUG
# ifdef __GNUC__
         mtx = m->mutex.__data.__count;	/* only works for recursive locks */
         if (mtx != 1) {
            printf("2. lock mutex != 1 (%i) in %s line %i, for: %s in %s\n", mtx, file, line, m->name, m->function);
            r = -mtx;
            abort();
         }
# endif
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
            printf("mutex already unlocked in %s line %i, for: %s\n",
                                              file, line, m->name);
         else
            printf("unlock mutex != 1 (%i) in %s line %i, for: %s in %s\n",
                                       mtx, file, line, m->name, m->function);
         r = -mtx;
         abort();
      }
# endif
#endif

      r = pthread_mutex_unlock(&m->mutex);
#ifndef NDEBUG
#endif
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
   _aaxMutex *m = (_aaxMutex *)signal->mutex;
   int rv;

   rv = pthread_cond_wait(signal->condition, &m->mutex);
   if (!rv) signal->triggered = 0;

   return rv;
}

int
_aaxSignalWaitTimed(_aaxSignal *signal, float timeout)
{
   _aaxMutex *m = (_aaxMutex *)signal->mutex;
   struct timespec ts;
   time_t secs;
   int rv;

   clock_gettime(CLOCK_REALTIME, &ts);

   secs = (time_t)floorf(timeout);
   timeout -= secs;

   ts.tv_sec += secs;
   ts.tv_nsec += (long)rintf(timeout*1e9f);
   if (ts.tv_nsec >= 1000000000L)
   {
      ts.tv_sec++;
      ts.tv_nsec -= 1000000000L;
   }
   
   rv = pthread_cond_timedwait(signal->condition, &m->mutex, &ts);
   switch(rv)
   {
   case ETIMEDOUT:
      signal->triggered = 0;
      rv = AAX_TIMEOUT;
      break;
   case 0:
      signal->triggered = 0;
      rv = AAX_TRUE;
      break;
   default:
      rv = AAX_FALSE;
      break;
   }

   return rv;
}

int
_aaxSignalTrigger(_aaxSignal *signal)
{
  int rv = 0;

   _aaxMutexLock(signal->mutex);
   if (!signal->triggered)
   {
      rv =  pthread_cond_signal(signal->condition);
      signal->triggered = rv ? 0 : 1;
   }
   _aaxMutexUnLock(signal->mutex);

   return rv;
}

#elif defined( WIN32 )	/* HAVE_PTHREAD_H */

#include <base/dlsym.h> 
							/* --- WINDOWS --- */
int                     /* Prio is a value in the range -20 to 19 */
_aaxProcessSetPriority(int prio)
{
   int rv = 0;

   DWORD curr_priority = GetPriorityClass(GetCurrentProcess());
   DWORD new_priority;

   if (prio <= AAX_HIGHEST_PRIORITY) {
      new_priority = HIGH_PRIORITY_CLASS;
   } else if (prio <= AAX_HIGH_PRIORITY) {
      new_priority = ABOVE_NORMAL_PRIORITY_CLASS;
   } else if (prio >= AAX_LOWEST_PRIORITY) {
      new_priority = IDLE_PRIORITY_CLASS;
   } else if (prio >= AAX_LOW_PRIORITY) {
      new_priority = BELOW_NORMAL_PRIORITY_CLASS;
   } else {
      new_priority = NORMAL_PRIORITY_CLASS;
   }

   if (new_priority > curr_priority)
   {
      rv = SetPriorityClass(GetCurrentProcess(), new_priority);
      if (!rv) {
         rv = GetLastError();
      }
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

   DWORD curr_priority = GetThreadPriority(GetCurrentThread());
   DWORD new_priority;

   if (prio == AAX_TIME_CRITICAL_PRIORITY) {
      new_priority = THREAD_PRIORITY_TIME_CRITICAL;
   } else if (prio == AAX_IDLE_PRIORITY) {
      new_priority = THREAD_PRIORITY_IDLE;
   } else if (prio <= AAX_HIGHEST_PRIORITY) {
      new_priority = THREAD_PRIORITY_HIGHEST;
   } else if (prio <= AAX_HIGH_PRIORITY) {
      new_priority = THREAD_PRIORITY_ABOVE_NORMAL;
   } else if (prio >= AAX_LOWEST_PRIORITY) {
      new_priority = THREAD_PRIORITY_LOWEST;
   } else if (prio >= AAX_LOW_PRIORITY) {
      new_priority = THREAD_PRIORITY_BELOW_NORMAL;
   } else {
      new_priority = THREAD_PRIORITY_NORMAL;
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

   if (thread->task)
   {
      if (pAvRevertMmThreadCharacteristics) {
         pAvRevertMmThreadCharacteristics(thread->task);
      }
      thread->task = 0;
   }

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
   if (thread->handle != NULL)
   {
      __threads_enabled = 1;
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

   __threads_enabled = 0;

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

int
_aaxThreadSwitch()
{
   return SwitchToThread() ? 0 : -1;
}

/*
 * In debugging mode we use real mutexes with a timeout value.
 * In release mode use critical sections which could be way faster
 *    for single process applications.
 */
#ifndef NDEBUG
void *
_aaxMutexCreateDebug(void *mutex, const char *name, const char *fn)
{
   _aaxMutex *m = (_aaxMutex *)mutex;

   if (!m) {
      m = calloc(1, sizeof(_aaxMutex));
   }

   if (m && !m->mutex) {
      m->mutex = CreateMutex(NULL, FALSE, NULL);
   }

   return m;
}
#else /* !NDEBUG */
void *
_aaxMutexCreate(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;

   if (!m) {
      m = calloc(1, sizeof(_aaxMutex));
   }

   if (m && !m->ready)
   {
      InitializeCriticalSection(&m->mutex);
      m->ready = 1;
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
#ifndef NDEBUG
      CloseHandle(m->mutex);
#else
      DeleteCriticalSection(&m->mutex);
      m->ready = 0;
#endif
      free(m);
   }

   m = 0;
}

#ifndef NDEBUG
int
_aaxMutexLockDebug(void *mutex, char *file, int line)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (__threads_enabled && m)
   {
      if (!m->mutex) {
         m = _aaxMutexCreate(m);
      }

      if (m->mutex) {
         r = WaitForSingleObject(m->mutex, DEBUG_TIMEOUT*1000);
         switch (r)
         {
         case WAIT_OBJECT_0:
            break;
         case WAIT_TIMEOUT:
            printf("mutex timed out in %s line %i\n", file, line);
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

int
_aaxMutexUnLockDebug(void *mutex, char *file, int line)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = EINVAL;

   if (__threads_enabled && m)
   {
      ReleaseMutex(m->mutex);
      r = 0;
   }
   return r;
}
#else	/* !NDEBUG */
int
_aaxMutexLock(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (__threads_enabled && m)
   {
      if (!m->ready) {
         m = _aaxMutexCreate(m);
      }

      if (m->ready) {
         EnterCriticalSection(&m->mutex);
      }
   }
   return r;
}

int
_aaxMutexUnLock(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (__threads_enabled && m) {
      LeaveCriticalSection(&m->mutex);
   }
   return r;
}
#endif

void *
_aaxConditionCreate()
{
   void *p = CreateEvent(NULL, FALSE, FALSE, NULL);
   return p;
}

void
_aaxConditionDestroy(void *c)
{
   assert(c);

   CloseHandle(c);
   c = 0;
}


/*
 * See:
 * http://www.codeproject.com/Articles/18371/Fast-critical-sections-with-timeout
 */
int
_aaxConditionWait(void *c, void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   DWORD hr;
   int r = 0;

   assert(mutex);
   
   _aaxMutexUnLock(m);
   hr = WaitForSingleObject(c, INFINITE);
   _aaxMutexLock(m);

   switch (hr)
   {
   case WAIT_OBJECT_0:	/* condiiton is signalled */
      r = 0;
      break;
   case WAIT_TIMEOUT:	/* time-out ocurred */
      r = ETIMEDOUT;
      break;
   default:
      r = EINVAL;
      break;
   }

   return r;
}

int
_aaxConditionWaitTimed(void *c, void *mutex, float timeout)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   struct timeval now_tv;
   DWORD hr;
   int r=0;

   assert(mutex);
   assert(ts);

   timeout *= 1000.0f; 		/* from seconds to ms */
   if (timeout > 0.0f)	
   {
      _aaxMutexUnLock(m);
      hr = WaitForSingleObject(c, (DWORD)rint(timeout));
      _aaxMutexLock(m);

      switch (hr)
      {
      case WAIT_OBJECT_0:	/* condiiton is signalled */
         r = 0;
         break;
      case WAIT_TIMEOUT:	/* time-out ocurred */
         r = ETIMEDOUT;
         break;
      default:
          r = EINVAL;
         break;
      }
   } else {		/* timeout */
      r = ETIMEDOUT;
   }
   return r;
}

int
_aaxConditionSignal(void *c)
{
   BOOL rv;

   assert(c);

   rv = SetEvent(c);
   return rv;
}


#else
# error "threads not implemented for this platform"
#endif /* HAVE_PTHREAD_H */

