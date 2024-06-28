/*
 * Minimal impact C11 threads implementation for Windows.
 * Written 2007-2024 by Erik Hofman.
 */

#ifndef HAVE_THREADS_H
# if defined(WIN32)

#include <c11threads.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <math.h>

/* Threads
 *
 * Work on Windows thread handles directly so they can be used by other
 * functions that manipulate thread behavior.
 */
static DWORD WINAPI
_callback_handler(LPVOID t)
{
    struct callback_data *thread = t;
    return thread->callback_fn(thread->data);
}

int
thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
    struct callback_data cb;
    int res = thrd_error;

    cb.callback_fn = func;
    cb.data = arg;
    *thr = CreateThread(NULL, 0, _callback_handler, &cb, 0, NULL);
    if (*thr) res = thrd_success;
    else if (GetLastError() == ERROR_NOT_ENOUGH_MEMORY) res = thrd_nomem;
    else res = thrd_error;
 
    return res;
}

int
thrd_equal(thrd_t thr0, thrd_t thr1)
{
    return (thr0 == thr1) ? true : false;
}

thrd_t
thrd_current(void)
{
    return GetCurrentThread();
}

int
thrd_sleep(const struct timespec *ts, struct timespec *tsr)
{
    int dwMilliseconds;
    int res;

    dwMilliseconds = 1000*ts->tv_sec + ts->tv_nsec/1000000;
    if (tsr)
    {
        LARGE_INTEGER start, end, freq;
        double dt;

        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        res = WaitForSingleObject(GetCurrentThread(), dwMilliseconds);
        QueryPerformanceCounter(&end);

        // return the remaining time on interruption.
        dt = ((end.QuadPart - start.QuadPart) / (double)freq.QuadPart);
        tsr->tv_sec = floor(dt);
        dt -= (double)tsr->tv_sec;
        tsr->tv_nsec = dt*1e9f;
    }
    else {
        res = WaitForSingleObject(GetCurrentThread(), dwMilliseconds);
    }

    switch (res)
    {
    case WAIT_OBJECT_0:
        res = 0;
        break;
    case WAIT_TIMEOUT:
    case WAIT_ABANDONED:
    case WAIT_FAILED:
    default:
        res = -1;
        break;
    }

    return res;
}

void
thrd_yield(void)
{
    Sleep(0);
}

void
thrd_exit(int res)
{
// TODO
}

int
thrd_detach(thrd_t thr)
{
    return CloseHandle(thr) ? thrd_success : thrd_error;
}

int
thrd_join(thrd_t thr, int *result)
{
    int res;

    res = WaitForSingleObject(thr, INFINITE);
    switch (res)
    {
    case WAIT_OBJECT_0:
        res = thrd_success;
        break;
    case WAIT_TIMEOUT:
    case WAIT_ABANDONED:
    case WAIT_FAILED:
    default:
        res = thrd_error;
    }

    if (result)
    {
        DWORD lpExitCode;
        GetExitCodeThread(thr, &lpExitCode);
        *result = (int)lpExitCode;
    }

    return res;
}

/* Mutual exclusion
 *
 * Like MSVC: use Slim Reader/Writer (SRW) Locks
 */
int
mtx_init(mtx_t *mtx, int type)
{
    mtx_t* mutex = malloc(sizeof(mtx_t));
    if (mutex)
    {
        mutex->type = type;
        InitializeSRWLock(&mutex->mtx);
        InitializeConditionVariable(&mutex->cond);
        return thrd_success;
    }
    return thrd_error;
}

int
mtx_lock(mtx_t *mtx)
{
    if (mtx->type & mtx_recursive) {
        AcquireSRWLockShared(&mtx->mtx);
    } else {
        AcquireSRWLockExclusive(&mtx->mtx);
    }
    return thrd_success;
}

int
mtx_timedlock(mtx_t *restrict mtx, const struct timespec *restrict ts)
{
    int dwMilliseconds;
    ULONG Flags;
    int res;

    dwMilliseconds = 1000*ts->tv_sec + ts->tv_nsec/1000000;
    Flags = (mtx->type & mtx_recursive)? CONDITION_VARIABLE_LOCKMODE_SHARED : 0;
    res = SleepConditionVariableSRW(&mtx->cond,&mtx->mtx, dwMilliseconds, Flags);
    switch(res)
    {
    case 0:
        if (GetLastError() == ERROR_TIMEOUT) res = thrd_timedout;
        else res = thrd_error;
        break;
    default:
        res = thrd_success;
        break;
    };

    return res;
}

int
mtx_trylock(mtx_t *mtx)
{
    int res;

    if (mtx->type & mtx_recursive) {
        res = TryAcquireSRWLockShared(&mtx->mtx) ? thrd_success : thrd_busy;
    } else {
        res = TryAcquireSRWLockExclusive(&mtx->mtx) ? thrd_success : thrd_busy;
    }
    return res;
}

int
mtx_unlock(mtx_t *mtx)
{
    if (mtx->type & mtx_recursive) {
        ReleaseSRWLockShared(&mtx->mtx);
    } else {
        ReleaseSRWLockExclusive(&mtx->mtx);
    }
    return thrd_success;
}

void
mtx_destroy(mtx_t *mtx)
{
    free(mtx);
}

/* Call once */
void
call_once(once_flag* flag, void (*func)(void))
{
    if (atomic_fetch_add(flag, 1) == 1) {
        func ();
    } else {
        atomic_fetch_sub(flag, 1);
    }
}

/* Condition variables */
int
cnd_init(cnd_t *cond)
{
    InitializeConditionVariable(cond);

    return thrd_success;
}

int
cnd_signal(cnd_t *cond)
{
    WakeConditionVariable(cond);

    return thrd_success;
}

int
cnd_broadcast(cnd_t *cond)
{
    WakeAllConditionVariable(cond);

    return thrd_success;
}

int
cnd_wait(cnd_t *cond, mtx_t *mtx)
{
    ULONG Flags;
    int res;

    Flags = (mtx->type & mtx_recursive)? CONDITION_VARIABLE_LOCKMODE_SHARED : 0;
    res = SleepConditionVariableSRW(cond, &mtx->mtx, INFINITE, Flags);

    return res ? thrd_success : thrd_error;
}

int
cnd_timedwait(cnd_t *restrict cond, mtx_t *restrict mtx,
              const struct timespec *restrict ts)
{
    int dwMilliseconds;
    ULONG Flags;
    int res;

    dwMilliseconds = 1000*ts->tv_sec + ts->tv_nsec/1000000;
    Flags = (mtx->type & mtx_recursive)? CONDITION_VARIABLE_LOCKMODE_SHARED : 0;
    res = SleepConditionVariableSRW(cond, &mtx->mtx, dwMilliseconds, Flags);

    return res ? thrd_success : thrd_error;
}

void
cnd_destroy(cnd_t *cond)
{
}


/* Thread local storage
 *
 * Use fiber local storage (FLS).
 */
int
tss_create(tss_t* tss_key, tss_dtor_t destructor)
{
    int res = thrd_error;
    DWORD dwFlsIndex;

    if ((dwFlsIndex = FlsAlloc(destructor)) != FLS_OUT_OF_INDEXES)
    {
        *tss_key = dwFlsIndex;
        res = thrd_success;
    }

    return res;
}

void*
tss_get(tss_t tss_key)
{
    return FlsGetValue(tss_key);
}

int
tss_set(tss_t tss_id, void *val)
{
    return FlsSetValue(tss_id, val) ? thrd_success : thrd_error;
}

void
tss_delete(tss_t tss_id)
{
    FlsFree(tss_id);
}

# endif // defined(WIN32)
#endif // !defined(HAVE_THREAD_H)
