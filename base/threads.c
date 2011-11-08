
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#ifndef NDEBUG
# include <stdio.h>
#endif

#include "threads.h"
#include "logging.h"

#if HAVE_PTHREAD_H

# if HAVE_ASSERT_H
#  include <assert.h>
# endif
# if HAVE_TIME_H
#  include <time.h>
# endif
#include <math.h>
#include <errno.h>
#include <string.h>	/* for memcpy */


#define	USE_REALTIME	1
#define _TH_SYSLOG(a) __oal_log(LOG_SYSLOG, 0, (a), 0, LOG_SYSLOG);


static char __threads_enabled = 0;

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
   sched_param.sched_priority = sched_get_priority_max(SCHED_RR);
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
         int mtx;
#endif

         to.tv_sec = time(NULL) + 2;
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
      int mtx;

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

#if 0
   return pthread_cond_broadcast(condition);
#else
   return pthread_cond_signal(condition);
#endif
}

#endif

