
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
} _aaxSignal;

int _aaxProcessSetPriority(int);


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

# define _aaxAtomicIntIncrement(a)	__sync_add_and_fetch((a),1)
# define _aaxAtomicIntDecrement(a)	__sync_sub_and_fetch((a),1)
# define _aaxAtomicIntAdd(a,b)		__sync_fetch_and_add((a),(b))
# define _aaxAtomicIntSub(a,b)		__sync_fetch_and_sub((a),(b))

#elif defined( WIN32 )
# include <windows.h>			/* WINDOWS */

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
# if defined(NDEBUG)
   CRITICAL_SECTION mutex;
# else
   HANDLE mutex;
   const char *name;
   const char *function;
   const char *last_file;
   size_t last_line;
# endif
 } _aaxMutex;

 AvSetMmThreadCharacteristicsA_proc pAvSetMmThreadCharacteristicsA;
 AvRevertMmThreadCharacteristics_proc pAvRevertMmThreadCharacteristics;
 AvSetMmThreadPriority_proc pAvSetMmThreadPriority;

# define _aaxAtomicIntIncrement(a)	InterlockedIncrement(a)
# define _aaxAtomicIntDecrement(a)	InterlockedDecrement(a)
# define _aaxAtomicIntAdd(a,b)          InterlockedExchangeAdd(a,b)
# define _aaxAtomicIntSub(a,b)          InterlockedExchangeAdd(a,-(b))

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
#define _aaxMutexCreate(a) _aaxMutexCreateDebug(a, __FILE__, __FUNCTION__)
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
int _aaxMutexLockDebug(void *, char *, int);
void _aaxMutexDestroy(void *);


/* -- Signals ---------------------------------------------------------- */
_aaxSignal *_aaxSignalCreate();
void _aaxSignalInit(_aaxSignal *);
void _aaxSignalDestroy(_aaxSignal*);
void _aaxSignalFree(_aaxSignal*);
int _aaxSignalTrigger(_aaxSignal*);
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

