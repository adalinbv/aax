
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
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
#include <errno.h>

#include "threads.h"
#include "logging.h"
#include "types.h"

#define REALTIME_PRIORITY	0
#define DEBUG_TIMEOUT		3
static char __threads_enabled = 0;

#if HAVE_PTHREAD_H
# include <string.h>	/* for memcpy */

#define _TH_SYSLOG(a) __oal_log(LOG_SYSLOG, 0, (a), 0, LOG_SYSLOG);

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

void
_aaxThreadDestroy(void *t)
{
   assert(t);

   free(t);
   t = 0;
}

int
_aaxThreadStart(void *t,  void *(*handler)(void*), void *arg)
{
   struct sched_param sched_param;
   pthread_attr_t attr;
   int ret;

   assert(t != 0);
   assert(handler != 0);

   pthread_attr_init(&attr);
#if REALTIME_PRIORITY
   sched_param.sched_priority = sched_get_priority_min(SCHED_RR);
#else
   sched_param.sched_priority = 0;
#endif
   pthread_attr_setschedparam(&attr, &sched_param);

   ret = pthread_create(t, &attr, handler, arg);
   if (ret == 0)
   {
      int rv;
      pthread_t *id = t;

#if REALTIME_PRIORITY
      rv = pthread_setschedparam(*id, SCHED_RR, &sched_param);
#else
      rv = pthread_setschedparam(*id, SCHED_OTHER, &sched_param);
#endif

      if (rv != 0) {
         _TH_SYSLOG("no thread scheduling privilege");
      } else {
         _TH_SYSLOG("using thread scheduling privilege");
      }
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

#ifdef NDEBUG
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
#ifndef NDEBUG
         status = pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
#else
         status = pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_NORMAL);
#endif
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

#ifdef NDEBUG
int
_aaxMutexLock(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (__threads_enabled && m)
   {
      if (m->initialized == 0) {
         m = _aaxMutexCreateInt(m);
      }

      if (m->initialized != 0)
      {
         r = pthread_mutex_lock(&m->mutex);
      }
   }
   return r;
}
#else

int
_aaxMutexLockDebug(void *mutex, char *file, int line)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (__threads_enabled && m)
   {
      if (m->initialized == 0) {
         mutex = _aaxMutexCreateInt(m);
      }

      if (m->initialized != 0)
      {
         struct timespec to;
#ifdef __GNUC__
         unsigned int mtx;
 
         mtx = m->mutex.__data.__count;
         
         if (mtx > 1) {
            printf("lock mutex > 1 (%i) in %s line %i, for: %s in %s\n",
                                         mtx, file, line, m->name, m->function);
            r = -mtx;
            abort();
         }
#endif

         to.tv_sec = time(NULL) + DEBUG_TIMEOUT;
         to.tv_nsec = 0;
         r = pthread_mutex_timedlock(&m->mutex, &to);
         // r = pthread_mutex_lock(&m->mutex);

         if (r == ETIMEDOUT) {
            printf("mutex timed out in %s line %i\n", file, line);
            abort();
         } else if (r == EDEADLK) {
            printf("dealock in %s line %i\n", file, line);
            abort();
         } else if (r) {
            printf("mutex lock error %i in %s line %i\n", r, file, line);
         }

#ifdef __GNUC__
         mtx = m->mutex.__data.__count;
         if (mtx != 1) {
            printf("lock mutex != 1 (%i) in %s line %i, for: %s in %s\n", mtx, file, line, m->name, m->function);
            r = -mtx;
            abort();
         }
#endif
      }
   }
   return r;
}
#endif

#ifdef NDEBUG
int
_aaxMutexUnLock(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (__threads_enabled && m)
   {
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

   if (__threads_enabled && m)
   {
#ifdef __GNUC__
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
#endif

      r = pthread_mutex_unlock(&m->mutex);
#ifndef NDEBUG
#endif
   }
   return r;
}
#endif


void *
_aaxConditionCreate()
{
   _aaxCondition *cptr;

   cptr = (_aaxCondition*)calloc(1, sizeof(_aaxCondition));
   if (cptr)
   {
      int status = pthread_cond_init(&cptr->condition, 0);
      if (!status)
      {
         status = pthread_mutex_init(&cptr->mutex, 0);
         if (status)
         {
            pthread_cond_destroy(&cptr->condition);
            free(cptr);
            cptr = NULL;
         }
      }
      else
      {
         free(cptr);
         cptr = NULL;
      }
   }
   return cptr;
}

void
_aaxConditionDestroy(void *c)
{
   _aaxCondition *cptr = c;

   assert(condition);

   pthread_mutex_lock(&cptr->mutex);
   cptr->triggered = 0;
   pthread_mutex_unlock(&cptr->mutex);
   pthread_mutex_destroy(&cptr->mutex);

   pthread_cond_destroy(&cptr->condition);
   free(cptr);
   c = 0;
}

int
_aaxConditionWait(void *c, void *mtx)
{
   _aaxMutex *m = (_aaxMutex *)mtx;
   _aaxCondition *cptr = c;
   pthread_mutex_t *mutex;
   int rv = 0;

   assert(cptr);

   if (!m) 
   {
      mutex = &cptr->mutex;
      pthread_mutex_lock(mutex);
      while (!cptr->triggered) {
         rv = pthread_cond_wait(&cptr->condition, mutex);
      }
      pthread_mutex_unlock(&cptr->mutex);
   }
   else 
   {
      mutex = &m->mutex;
      rv = pthread_cond_wait(&cptr->condition, mutex);
   }
   return rv;
}

int
_aaxConditionWaitTimed(void *c, void *mtx, struct timespec *ts)
{
   _aaxMutex *m = (_aaxMutex *)mtx;
   _aaxCondition *cptr = c;
   pthread_mutex_t *mutex;
   int rv = 0;

   assert(cptr);
   assert(ts);
/* printf("*: %x/%x\n", m, m->mutex); */

   if (!m)
   {
      mutex = &cptr->mutex;
      pthread_mutex_lock(mutex);
      while (!cptr->triggered)
      {
         rv = pthread_cond_timedwait(&cptr->condition, mutex, ts);
         if (rv == ETIMEDOUT) break;
      }
      pthread_mutex_unlock(&cptr->mutex);
   }
   else
   {
      mutex = &m->mutex;
      rv = pthread_cond_timedwait(&cptr->condition, mutex, ts);
   }
   return rv;
}

int
_aaxConditionSignal(void *c)
{
  _aaxCondition *cptr = c;
  int rv;

   assert(cptr);

   pthread_mutex_lock(&cptr->mutex);
   cptr->triggered = 1;
   rv = pthread_cond_signal(&cptr->condition);
   pthread_mutex_unlock(&cptr->mutex);

   return rv;
}

#elif defined( WIN32 )	/* HAVE_PTHREAD_H */
							/* --- WINDOWS --- */
#include <base/dlsym.h>
#include <base/types.h>

typedef HANDLE (WINAPI *AvSetMmThreadCharacteristicsA_proc)(LPCTSTR, LPDWORD);
typedef BOOL   (WINAPI *AvRevertMmThreadCharacteristics_proc)(HANDLE);

DECL_FUNCTION(AvSetMmThreadCharacteristicsA);
DECL_FUNCTION(AvRevertMmThreadCharacteristics);

#define _TH_SYSLOG(a)

/* http://www.slideshare.net/abufayez/pthreads-vs-win32-threads */
/* http://www.ibm.com/developerworks/linux/library/l-ipc2lin3/index.html */

void *
_aaxThreadCreate()
{
   void *ret  = calloc(1, sizeof(_aaxThread));

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
   thread->callback_fn(thread->callback_data);
   return 0;
}

int
_aaxThreadStart(void *t,  void *(*handler)(void*), void *arg)
{
   _aaxThread *thread = t;
   int rv = -1;

   thread->callback_fn = handler;
   thread->callback_data = arg;
   thread->handle = CreateThread(0, 0, _callback_handler, t, 0, 0);
   if (thread->handle != INVALID_HANDLE_VALUE)
   {
#if 1
      static void *audio = NULL;

      if (!audio) {
         audio = _oalIsLibraryPresent("avrt", 0);
      }

      if (audio)
      {
         TIE_FUNCTION(AvSetMmThreadCharacteristicsA);
         TIE_FUNCTION(AvRevertMmThreadCharacteristics);
         if (pAvSetMmThreadCharacteristicsA)
         {
            DWORD taskIndex = 0;
            thread->task = pAvSetMmThreadCharacteristicsA("Pro Audio",
                                                         &taskIndex);
            if (!thread->task) thread->taskCount++;
         }
      }
#endif
      __threads_enabled = 1;
     rv = 0;
   }

#if REALTIME_PRIORITY
   // REALTIME_PRIORITY_CLASS or HIGH_PRIORITY_CLASS
   // Process that has the highest possible priority. The threads of the
   // process preempt the threads of all other processes, including operating
   // system processes performing important tasks. For example, a real-time
   // process that executes for more than a very brief interval can cause disk
   // caches not to flush or cause the mouse to be unresponsive.
   SetThreadPriority(thread->handle, THREAD_PRIORITY_TIME_CRITICAL);
#endif

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

#if 0
   if (pAvRevertMmThreadCharacteristics)
   {
      thread->taskCount--;
      if (!thread->taskCount)
      {
         pAvRevertMmThreadCharacteristics(thread->task);
         thread->task = 0;
      }
   }
#endif

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
   
   if (m) _aaxMutexUnLock(m);
   hr = WaitForSingleObject(c, INFINITE);
   if (m) _aaxMutexLock(m);

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
_aaxConditionWaitTimed(void *c, void *mutex, struct timespec *ts)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   struct timeval now_tv;
   int64_t dt_ms;
   DWORD hr;
   int r=0;

   assert(ts);

   /* some time in the future (hopefuly) */
   dt_ms = (1000*ts->tv_sec + ts->tv_nsec/1000000);

   gettimeofday(&now_tv, 0);
   dt_ms -= (1000*now_tv.tv_sec + now_tv.tv_usec/1000);

   if (dt_ms > 0)
   {
      if (m) _aaxMutexUnLock(m);
      hr = WaitForSingleObject(c, (DWORD)dt_ms);
      if (m) _aaxMutexLock(m);

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

