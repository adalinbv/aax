
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

#define DEBUG_TIMEOUT		3
static char __threads_enabled = 0;

#if HAVE_PTHREAD_H
#include <string.h>	/* for memcpy */

#define	USE_REALTIME	1
#define _TH_SYSLOG(a) __oal_log(LOG_SYSLOG, 0, (a), 0, LOG_SYSLOG);

void *
_aaxThreadCreate()
{
   void *ret  = malloc(sizeof(_aaxThread));

   return ret;
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
#if USE_REALTIME
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

#if USE_REALTIME
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

#if 0
/* not supported by Android's bionic and not used either */
int
_aaxThreadCancel(void *t)
{
   _aaxThread *thread = t;
   int ret;

   assert(t);

   __threads_enabled = 0;
   ret = pthread_cancel(*thread);

   return ret;
}
#endif

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
            printf("lock mutex != 1 (%i) in %s line %i, for: %s int %s\n", mtx, file, line, m->name, m->function);
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
   _aaxCondition condition;
   void *p = malloc(sizeof(_aaxCondition));
   if (!p) return 0;
   memcpy(p, &condition, sizeof(_aaxCondition));
#ifndef NDEBUG
   do
   {
      int status = pthread_cond_init(p, 0);
      assert(status == 0);
   }
   while(0);
#else
   pthread_cond_init(p, 0);
#endif
   return p;
}

void
_aaxConditionDestroy(void *c)
{
   _aaxCondition *condition = c;
   assert(condition);
   pthread_cond_destroy(condition);
   free(condition);
   c = 0;
}

int
_aaxConditionWait(void *c, void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   _aaxCondition *condition = c;
   assert(condition);
   assert(mutex);
   return pthread_cond_wait(condition, &m->mutex);
}

int
_aaxConditionWaitTimed(void *c, void *mutex, struct timespec *ts)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   _aaxCondition *condition = c;
   assert(condition);
   assert(mutex);
   assert(ts);
/* printf("*: %x/%x\n", m, m->mutex); */
   return pthread_cond_timedwait(condition, &m->mutex, ts);
}

int
_aaxConditionSignal(void *c)
{
  _aaxCondition *condition = c;

   assert(condition);

   return pthread_cond_signal(condition);
}

#elif defined( WIN32 )	/* HAVE_PTHREAD_H */
							/* --- WINDOWS --- */
#include <base/types.h>

/* http://www.slideshare.net/abufayez/pthreads-vs-win32-threads */
/* http://www.ibm.com/developerworks/linux/library/l-ipc2lin3/index.html */

void *
_aaxThreadCreate()
{
   void *ret  = calloc(1, sizeof(_aaxThread));

   return ret;
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
      __threads_enabled = 1;
     rv = 0;
   }

#if USE_REALTIME
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

#ifndef NDEBUG
void *
_aaxMutexCreateDebug(void *mutex, const char *name, const char *fn)
{
   _aaxMutex *m = (_aaxMutex *)mutex;

   if (!m)
   {
      m = calloc(1, sizeof(_aaxMutex));
      if (m) {
         m->mutex = CreateMutex(NULL, FALSE, NULL);
      }
   }

   return m;
}
#else
void *
_aaxMutexCreate(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;

   if (!m)
   {
      m = calloc(1, sizeof(_aaxMutex));
      if (m) {
         m->mutex = CreateMutex(NULL, FALSE, NULL);
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
      CloseHandle(m->mutex);
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
         m->mutex = CreateMutex(NULL, FALSE, NULL);
      }

      if (m->mutex) {
         r = WaitForSingleObject(m->mutex, DEBUG_TIMEOUT);
         switch (r)
         {
         case WAIT_OBJECT_0:
            break;
         case WAIT_TIMEOUT:
            printf("mutex timed out in %s line %i\n", file, line);
            r = ETIMEDOUT;
            break;
         case WAIT_ABANDONED:
         case WAIT_FAILED:
         default:
            printf("mutex lock error %i in %s line %i\n", r, file, line);
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
#else
int
_aaxMutexLock(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (__threads_enabled && m)
   {
      if (!m->mutex) {
         m->mutex = CreateMutex(NULL, FALSE, NULL);
      }

      if (m->mutex) {
         r = WaitForSingleObject(m->mutex, INFINITE);
         switch (r)
         {
         case WAIT_OBJECT_0:
            break;
         case WAIT_TIMEOUT:
            _TH_SYSLOG("mutex timed out");
            r = ETIMEDOUT;
            break;
         case WAIT_ABANDONED:
         case WAIT_FAILED:
         dwfault:
            _TH_SYSLOG("mutex lock error");
         }
      }
   }
   return r;
}

int
_aaxMutexUnLock(void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   int r = 0;

   if (__threads_enabled && m)
   {
      r = ReleaseMutex(m->mutex);
      r = ~r;
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

int
_aaxConditionWait(void *c, void *mutex)
{
   _aaxMutex *m = (_aaxMutex *)mutex;
   _aaxCondition *condition = c;
   DWORD rcode;
   int r = 0;

   assert(condition);
   assert(mutex);
   
   _aaxMutexUnLock(m);
   rcode = WaitForSingleObject(m->mutex, INFINITE);
   _aaxMutexLock(m);

   switch (rcode)
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
   _aaxCondition *condition = c;
   DWORD rcode, tnow_ms, treq_ms;
   struct timeval now;
   int r = 0;

   assert(condition);
   assert(mutex);
   assert(ts);

   treq_ms = 1000*(DWORD)ts->tv_sec + ts->tv_nsec/1000000;

   gettimeofday(&now, 0);
   tnow_ms = 1000*now.tv_sec + now.tv_usec/1000;

   if (treq_ms > tnow_ms)
   {
      _aaxMutexUnLock(m);
      rcode = WaitForSingleObject(c, treq_ms - tnow_ms);
      _aaxMutexLock(m);

      switch (rcode)
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
   assert(c);
// TODO: mutex mangling ?
   return SetEvent(c);
}


#else
# error "threads not implemented for this platform"
#endif /* HAVE_PTHREAD_H */

