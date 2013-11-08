
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


#if HAVE_PTHREAD_H
# include <pthread.h>			/* UNIX */

 typedef pthread_t	_aaxThread;
 typedef pthread_cond_t	_aaxCondition;

 typedef struct _aaxMutex
 { 
   char initialized;
   pthread_mutex_t mutex;
   const char *name;
   const char *function;
 } _aaxMutex;

#elif defined( WIN32 )
# include <Windows.h>			/* WINDOWS */

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

 typedef struct _aaxMutex
 {
#ifndef NDEBUG
   HANDLE mutex;
#else
   CRITICAL_SECTION mutex;
   char ready;
#endif
 } _aaxMutex;

 AvSetMmThreadCharacteristicsA_proc pAvSetMmThreadCharacteristicsA;
 AvRevertMmThreadCharacteristics_proc pAvRevertMmThreadCharacteristics;
 AvSetMmThreadPriority_proc pAvSetMmThreadPriority;

#endif


void *_aaxThreadCreate();
int _aaxThreadSetAffinity(void *, int);
void _aaxThreadDestroy(void *);
int _aaxThreadStart(void *,  void *(*handler)(void*), void*, unsigned int);
// int _aaxThreadCancel(void *);
int _aaxThreadJoin(void *);
int _aaxThreadSwitch();


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


void *_aaxConditionCreate();
void _aaxConditionDestroy(void *);
int _aaxConditionWait(void *, void *);
int _aaxConditionWaitTimed(void *, void *, struct timespec *);
int _aaxConditionSignal(void *);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !__THREADS_H */

