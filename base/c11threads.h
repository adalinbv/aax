/*
 * Minimal impact C11 threads implementation for Windows.
 * Written 2007-2024 by Erik Hofman.
 */

#if defined(__cplusplus)
extern "C" {
#endif

#pragma once

#include <config.h>

#if HAVE_THREADS_H
# include <threads.h>
#else

# if defined(WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <time.h>

// https://en.cppreference.com/w/c/thread
/* Threads */
enum {
    thrd_success,
    thrd_nomem,
    thrd_timedout,
    thrd_busy,
    thrd_error
};

typedef HANDLE thrd_t;
typedef int(*thrd_start_t)(void*);

struct callback_data {
    thrd_start_t callback_fn;
    void *data;
};

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
int thrd_equal(thrd_t thr0, thrd_t thr1);
thrd_t thrd_current(void);
int thrd_sleep(const struct timespec *duration,
               struct timespec *remaining);
void thrd_yield(void);
void thrd_exit(int res);
int thrd_detach(thrd_t thr);
int thrd_join(thrd_t thr, int *res);

/* Mutual exclusion */
enum mtx_e {
    mtx_plain = 0,
    mtx_timed,
    mtx_recursive
};

typedef struct {
    SRWLOCK mtx;
    CONDITION_VARIABLE cond;
    enum mtx_e type;
} mtx_t;

int mtx_init(mtx_t *mtx, int type);
int mtx_lock(mtx_t *mtx);
int mtx_timedlock(mtx_t *restrict mtx,
                  const struct timespec *restrict ts);
int mtx_trylock(mtx_t *mtx);
int mtx_unlock(mtx_t *mtx);
void mtx_destroy(mtx_t *mtx);

/* Condition variables */
typedef CONDITION_VARIABLE cnd_t;

int cnd_init(cnd_t *cond);
int cnd_signal(cnd_t *cond);
int cnd_broadcast(cnd_t *cond);
int cnd_wait(cnd_t *cond, mtx_t *mtx);
int cnd_timedwait(cnd_t *restrict cond, mtx_t *restrict mtx,
                  const struct timespec *restrict ts);
void cnd_destroy(cnd_t *cond);

/* Thread local */
#define TSS_DTOR_ITERATIONS 

typedef DWORD tss_t;
typedef void(*tss_dtor_t)(void*);

int tss_create(tss_t* tss_key, tss_dtor_t destructor);
void *tss_get(tss_t tss_key);
int tss_set(tss_t tss_id, void *val);
void tss_delete(tss_t tss_id);

# endif // defined(WIN32)

#endif // HAVE_THREAD_H
