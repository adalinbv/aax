/*
 * SPDX-FileCopyrightText: Copyright © 2007-2024 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2024 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */
#pragma once

#ifdef NDEBUG
# define NDEBUGTHREADS
#else
# undef NDEBUGTHREADS
#endif

#if HAVE_TIME_H
#include <time.h>
#endif

#include <c11threads.h>
#include <stdatomic.h>

#include "types.h"
#include "timer.h"

#if 0
#define TIMED_MUTEX	1
#endif

#if defined(__x86_64__) || defined(__i386__) || defined(_WIN64)
# include <pmmintrin.h>
# include <xmmintrin.h>
#  define AAX_SET_FLUSH_ZERO_ON  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); \
                              _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON)
#else
# define AAX_SET_FLUSH_ZERO_ON
#endif


enum {
   AAX_TIME_CRITICAL_PRIORITY = -20,
   AAX_HIGHEST_PRIORITY = -16,
   AAX_HIGH_PRIORITY = -8,
   AAX_NORMAL_PRIORITY = 0,
   AAX_LOW_PRIORITY = 8,
   AAX_LOWEST_PRIORITY = 16,
   AAX_IDLE_PRIORITY = 19
};
int _aaxProcessSetPriority(int);

void atomic_pointer_swap(void**, void**);
# define _aaxAtomicIntIncrement(a)	(_aaxAtomicIntAdd((a),1)+1)
# define _aaxAtomicIntDecrement(a)	(_aaxAtomicIntAdd((a),-1)-1)
# define _aaxAtomicIntSub(a,b)		(_aaxAtomicIntAdd((a),-(b)))
#if defined(__FreeBSD__)
#  define _aaxAtomicIntAdd(a,b)		__sync_fetch_and_add((a),(b))
#  define _aaxAtomicIntSet(a,b)		__sync_lock_test_and_set((a),(b))
#  define _aaxAtomicPointerSwap(a,b)	__sync_lock_test_and_set((a),(b))
# else
#  define _aaxAtomicIntAdd(a,b)		atomic_fetch_add((a),(b))
# define _aaxAtomicIntSet(a,b)		atomic_exchange((a),(b))
#  define _aaxAtomicPointerSwap(a,b)	atomic_pointer_swap((a),(b))
# endif

#if HAVE_PTHREAD_H
# include <pthread.h>			/* UNIX */
# include <semaphore.h>

 typedef sem_t		_aaxSemaphore;

#elif defined( WIN32 )			/* WINDOWS */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

 typedef enum
 {
    AVRT_PRIORITY_LOW = -1,
    AVRT_PRIORITY_NORMAL,
    AVRT_PRIORITY_HIGH,
    AVRT_PRIORITY_CRITICAL
 } AVRT_PRIORITY;

 typedef HANDLE (WINAPI *AvSetMmThreadCharacteristicsA_proc)(LPCTSTR, LPDWORD);
 typedef BOOL   (WINAPI *AvRevertMmThreadCharacteristics_proc)(HANDLE);
 typedef BOOL   (WINAPI *AvSetMmThreadPriority_proc)(HANDLE, AVRT_PRIORITY);

 typedef HANDLE _aaxSemaphore;

extern AvSetMmThreadCharacteristicsA_proc pAvSetMmThreadCharacteristicsA;
extern AvRevertMmThreadCharacteristics_proc pAvRevertMmThreadCharacteristics;
extern AvSetMmThreadPriority_proc pAvSetMmThreadPriority;

#endif


/* -- Threads ---------------------------------------------------------- */
#ifdef __MINGW32__
 typedef struct _aaxThread
 {
   HANDLE handle;
   HANDLE task;
   LONG ms;

   int(*callback_fn)(void*);
   void *callback_data;
 } _aaxThread;
#else
typedef thrd_t	_aaxThread;
#endif

void *_aaxThreadCreate(void);
int _aaxThreadSetAffinity(_aaxThread*, int);
int _aaxThreadSetPriority(_aaxThread*, int);
void _aaxThreadDestroy(_aaxThread*);
int _aaxThreadStart(_aaxThread*,  int(*handler)(void*), void*, unsigned int, const char*);
int _aaxThreadJoin(_aaxThread*);


/* -- Mutexes ---------------------------------------------------------- */
#define AAX_THREAD_INITIALIZED		0x9278ABA5
#define AAX_THREAD_LOCKED		0xBAB9121F
#define AAX_THREAD_UNLOCKED		0xCEC2BA59
#define AAX_THREAD_DESTROYED		0xFADEDBAD

#define AAX_THREAD_STATUS_TO_STRING(a) \
 ((a) == AAX_THREAD_INITIALIZED) ? "initialized" : \
  ((a) == AAX_THREAD_LOCKED) ? "locked" : \
   ((a) == AAX_THREAD_UNLOCKED) ? "unlocked" : \
    ((a) == AAX_THREAD_DESTROYED) ? "destroyed" : "unkown"
typedef struct _aaxMutex
{
#ifdef __MINGW32__
   char initialized;
   char waiting;
   HANDLE mutex;
   CRITICAL_SECTION crit;
#else
   int status;
   mtx_t mutex;
#endif

   const char *name;
   const char *function;
   const char *last_file;
   size_t last_line;
} _aaxMutex;

#ifndef NDEBUGTHREADS
int _aaxMutexLockDebug(_aaxMutex*, char *, int);
int _aaxMutexUnLockDebug(_aaxMutex *, char *, int);
void *_aaxMutexCreateDebug(_aaxMutex *, const char *, const char *);
# define _aaxMutexCreate(a) _aaxMutexCreateDebug(a, __FILE__, __func__)
# define _aaxMutexLock(a) _aaxMutexLockDebug(a, __FILE__, __LINE__)
# define _aaxMutexUnLock(a) _aaxMutexUnLockDebug(a, __FILE__, __LINE__)
#else
# ifdef TIMED_MUTEX
int _aaxMutexUnLockDebug(_aaxMutex *, char *, int);
#  define _aaxMutexLock(a) _aaxMutexLockDebug(a, __FILE__, __LINE__)
# else
int _aaxMutexLock(_aaxMutex *);
int _aaxMutexUnLock(_aaxMutex *);
# endif
void*_aaxMutexCreate(_aaxMutex *);
int _aaxMutexLock(_aaxMutex *);
#endif
int _aaxMutexLockTimed(_aaxMutex*, float);
void _aaxMutexDestroy(_aaxMutex*);

/* -- Conditions/Signals ----------------------------------------------- */
typedef struct
{
   _aaxMutex *mutex;
   cnd_t* condition;
   char waiting;
   int triggered;
   struct timespec ts;

} _aaxSignal;

_aaxSignal *_aaxSignalCreate(void);
int _aaxSignalInit(_aaxSignal *);
void _aaxSignalDestroy(_aaxSignal*);
void _aaxSignalFree(_aaxSignal*);
int _aaxSignalTrigger(_aaxSignal*);
int _aaxSignalWait(_aaxSignal*);
int _aaxSignalWaitTimed(_aaxSignal*, float);

/* -- Semaphores ------------------------------------------------------- */
_aaxSemaphore *_aaxSemaphoreCreate(unsigned);
int _aaxSemaphoreDestroy(_aaxSemaphore*);
#ifndef NDEBUGTHREADS
int _aaxSemaphoreWaitDebug(_aaxSemaphore*, char *, int);
#define _aaxSemaphoreWait(a)	_aaxSemaphoreWaitDebug(a, __FILE__, __LINE__)
#else
#define _aaxSemaphoreWait(a)	_aaxSemaphoreWaitNoTimeout(a)
#endif
int _aaxSemaphoreWaitNoTimeout(_aaxSemaphore*);
int _aaxSemaphoreRelease(_aaxSemaphore*);
