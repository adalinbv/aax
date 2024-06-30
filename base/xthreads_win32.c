/*
 * SPDX-FileCopyrightText: Copyright © 2007-2024 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2024 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

/* http://www.slideshare.net/abufayez/pthreads-vs-win32-threads */
/* http://www.ibm.com/developerworks/linux/library/l-ipc2lin3/index.html */

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
_aaxThreadSetAffinity(_aaxThread *t, int core)
{
   DWORD_PTR rv;

   rv = SetThreadAffinityMask(t->handle, 1<<core);
   return rv ? 0 : EINVAL;
}

int
_aaxThreadSetPriority(_aaxThread *t, int prio)
{
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

   SetThreadPriority(t, new_priority);
   if (!rv) {
      rv = GetLastError();
   }

   return rv;
}

void
_aaxThreadDestroy(_aaxThread *t)
{
   assert(t);

   if (t->handle)
   {
      CloseHandle(t->handle);
      t->handle = 0;
   }
   free(t);
   t = 0;
}

static DWORD WINAPI
_callback_handler(LPVOID thread)
{
   _aaxThread *t = thread;
   int res;

   if (pAvSetMmThreadCharacteristicsA)
   {
      DWORD tIdx = 0;
      if (t->ms >= 10) {
         t->task = pAvSetMmThreadCharacteristicsA("Audio", &tIdx);
      }
      else
      {
         t->task = pAvSetMmThreadCharacteristicsA("Pro Audio", &tIdx);
         if (t->task && pAvSetMmThreadPriority) {
            pAvSetMmThreadPriority(t->task, AVRT_PRIORITY_HIGH);
         }
      }
   }

   res = t->callback_fn(t->callback_data);

   if (t->task)
   {
     if (pAvRevertMmThreadCharacteristics) {
         pAvRevertMmThreadCharacteristics(t->task);
      }
      t->task = NULL;
   }

   return res;
}

int
_aaxThreadStart(_aaxThread *t,  int(*handler)(void*), void *arg, unsigned int ms, const char *name)
{
   int rv = -1;

   t->ms = ms;
   t->callback_fn = handler;
   t->callback_data = arg;
   t->handle = CreateThread(NULL, 0, _callback_handler, t, 0, NULL);
   if (t->handle != NULL)
   {
      const size_t BUFSIZE = 100;
      wchar_t buf[BUFSIZE];
      MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, name, -1, buf, BUFSIZE);
      SetThreadDescription(t, buf);

      rv = 0;
   }

   return rv;
}

int
_aaxThreadJoin(_aaxThread *t)
{
   int ret = 0;
   DWORD r;

   assert(t);

   r = WaitForSingleObject(t->handle, INFINITE);
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
_aaxMutexCreate(_aaxMutex *m)
{
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
_aaxMutexCreateDebug(_aaxMutex *m, const char *name, const char *fn)
{
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
_aaxMutexDestroy(_aaxMutex *m)
{
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

int
_aaxMutexLock(_aaxMutex *m)
{
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

int
_aaxMutexLockTimed(_aaxMutex *m, float dt)
{
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
_aaxMutexLockDebug(_aaxMutex *m, char *file, int line)
{
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
_aaxMutexUnLock(_aaxMutex *m)
{
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
_aaxMutexUnLockDebug(_aaxMutex *m, char *file, int line)
{
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

int
_aaxSignalInit(_aaxSignal *signal)
{
   int rv = 0;

   signal->condition = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (signal->condition) {
      signal->mutex = _aaxMutexCreate(signal->mutex);
   } else {
       rv = EINVAL;
   }
   return rv;
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

