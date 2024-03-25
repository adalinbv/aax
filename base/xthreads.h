/*
 * SPDX-FileCopyrightText: Copyright © 2007-2024 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2024 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */
#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#define NDEBUGTHREADS

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_TIME_H
#include <time.h>
#endif

#include <threads.h>
#include <stdatomic.h>

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
int _aaxProcessSetPriority(int);

# define _aaxAtomicIntIncrement(a)	(_aaxAtomicIntAdd((a),1)+1)
# define _aaxAtomicIntDecrement(a)	(_aaxAtomicIntAdd((a),-1)-1)
# define _aaxAtomicIntSub(a,b)		(_aaxAtomicIntAdd((a),-(b)))
# define _aaxAtomicIntAdd(a,b)		atomic_fetch_add((a),(b))
# define _aaxAtomicIntSet(a,b)		(_aaxAtomicPointerSwap(a,b))
# define _aaxAtomicPointerSwap(a,b)	atomic_exchange((a),(b))

#if HAVE_PTHREAD_H
# include <pthread.h>			/* UNIX */
# include <semaphore.h>

 typedef sem_t		_aaxSemaphore;

#elif defined( WIN32 )			/* WINDOWS */

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
typedef thrd_t	_aaxThread;

void *_aaxThreadCreate();
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
   int status;
   mtx_t mutex;

#ifndef NDEBUGTHREADS
   const char *name;
   const char *function;
   const char *last_file;
   size_t last_line;
#endif
} _aaxMutex;

void*_aaxMutexCreate(_aaxMutex *);
int _aaxMutexLock(_aaxMutex *);
int _aaxMutexUnLock(_aaxMutex *);
int _aaxMutexLockTimed(_aaxMutex*, float);
int _aaxMutexLockDebug(_aaxMutex*, char *, int);
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

_aaxSignal *_aaxSignalCreate();
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

#if defined(__cplusplus)
}  /* extern "C" */
#endif

