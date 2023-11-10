/*
 * SPDX-FileCopyrightText: Copyright © 2007-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef __THREADS_H
#define __THREADS_H 1

#if defined(__cplusplus)
extern "C" {
#endif


#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_TIME_H
#include <time.h>
#endif

#include "types.h"
#include "timer.h"

#if 0
#define TIMED_MUTEX	1
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

typedef struct
{
   void *mutex;
   void *condition;
   char waiting;
   int triggered;
#if defined( WIN32 )
#else
   struct timespec ts;
#endif

} _aaxSignal;

int _aaxProcessSetPriority(int);

#if defined( __WIN32__ ) || defined( __TINYC__ )
# ifndef __MINGW32__
#  define __sync_fetch_and_add(a,b)		InterlockedExchangeAdd(a,b)
#  define __sync_lock_test_and_set((a,b)	InterlockedExchangePointer(a,b)
// int __sync_fetch_and_add(int*, int);
// void* __sync_lock_test_and_set(void**, void*);
# endif
#endif

# define _aaxAtomicIntIncrement(a)	(_aaxAtomicIntAdd((a),1)+1)
# define _aaxAtomicIntDecrement(a)	(_aaxAtomicIntAdd((a),-1)-1)
# define _aaxAtomicIntSub(a,b)		_aaxAtomicIntAdd((a),-(b))
# define _aaxAtomicIntAdd(a,b)		__sync_fetch_and_add((a),(b))
# define _aaxAtomicIntSet(a,b)		_aaxAtomicPointerSwap(a,b)
# define _aaxAtomicPointerSwap(a,b)	__sync_lock_test_and_set((a),(b))

#if HAVE_PTHREAD_H
# include <pthread.h>			/* UNIX */
# include <semaphore.h>

 typedef pthread_t	_aaxThread;
 typedef pthread_cond_t	_aaxCondition;
 typedef sem_t		_aaxSemaphore;

 typedef struct _aaxMutex
 { 
   char initialized;
   pthread_mutex_t mutex;
#ifndef NDEBUG
   const char *name;
   const char *function;
   const char *last_file;
   size_t last_line;
#endif
 } _aaxMutex;

#elif defined( WIN32 )

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

 typedef struct _aaxThread 
 {
   HANDLE handle;
   HANDLE task;
   LONG ms;

   void *(*callback_fn)(void*);
   void *callback_data;
 } _aaxThread;

 typedef HANDLE _aaxCondition;
 typedef HANDLE _aaxSemaphore;

 typedef struct _aaxMutex
 {
   char initialized;
   char waiting;
   HANDLE mutex;
# if defined(NDEBUG)
   CRITICAL_SECTION crit;
# else
   const char *name;
   const char *function;
   const char *last_file;
   size_t last_line;
# endif
 } _aaxMutex;

extern AvSetMmThreadCharacteristicsA_proc pAvSetMmThreadCharacteristicsA;
extern AvRevertMmThreadCharacteristics_proc pAvRevertMmThreadCharacteristics;
extern AvSetMmThreadPriority_proc pAvSetMmThreadPriority;

#endif


/* -- Threads ---------------------------------------------------------- */
void *_aaxThreadCreate();
int _aaxThreadSetAffinity(void *, int);
int _aaxThreadSetPriority(void *, int);
void _aaxThreadDestroy(void *);
int _aaxThreadStart(void *,  void *(*handler)(void*), void*, unsigned int);
// int _aaxThreadCancel(void *);
int _aaxThreadJoin(void *);


/* -- Mutexes ---------------------------------------------------------- */
#ifndef NDEBUG
#define _aaxMutexCreate(a) _aaxMutexCreateDebug(a, __FILE__, __func__)
#define _aaxMutexLock(a) _aaxMutexLockDebug(a, __FILE__, __LINE__)
#define _aaxMutexUnLock(a) _aaxMutexUnLockDebug(a, __FILE__, __LINE__)
void *_aaxMutexCreateDebug(void *, const char *, const char *);
int _aaxMutexUnLockDebug(void *, char *, int);
#else
# ifdef TIMED_MUTEX
#  define _aaxMutexLock(a) _aaxMutexLockDebug(a, __FILE__, __LINE__)
# else
int _aaxMutexLock(void *);
# endif
void *_aaxMutexCreate(void *);
int _aaxMutexUnLock(void *);
#endif
int _aaxMutexLockTimed(void *, float);
int _aaxMutexLockDebug(void *, char *, int);
void _aaxMutexDestroy(void *);


/* -- Signals ---------------------------------------------------------- */
_aaxSignal *_aaxSignalCreate();
void _aaxSignalInit(_aaxSignal *);
void _aaxSignalDestroy(_aaxSignal*);
void _aaxSignalFree(_aaxSignal*);
#ifndef NDEBUG
int _aaxSignalTriggerDebug(_aaxSignal*, char *, int);
#define _aaxSignalTrigger(a)	_aaxSignalTriggerDebug(a, __FILE__, __LINE__)
#else
int _aaxSignalTrigger(_aaxSignal*);
#endif
int _aaxSignalWait(_aaxSignal*);
int _aaxSignalWaitTimed(_aaxSignal*, float);

/* -- Semaphores ------------------------------------------------------- */
_aaxSemaphore *_aaxSemaphoreCreate(unsigned);
int _aaxSemaphoreDestroy(_aaxSemaphore*);
#ifndef NDEBUG
int _aaxSemaphoreWaitDebug(_aaxSemaphore*, char *, int);
#define _aaxSemaphoreWait(a)	_aaxSemaphoreWaitDebug(a, __FILE__, __LINE__)
#else
int _aaxSemaphoreWait(_aaxSemaphore*);
#endif
int _aaxSemaphoreRelease(_aaxSemaphore*);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !__THREADS_H */

