#ifndef ARENAC_THREADS_H
#define ARENAC_THREADS_H

#if defined(ARENAC_THREADS_FORCE_FALLBACK)
#  define ARENAC_THREADS_FALLBACK 1
#elif defined(__has_include)
#  if __has_include(<threads.h>)
#    include <threads.h>
#  else
#    define ARENAC_THREADS_FALLBACK 1
#  endif
#else
#  define ARENAC_THREADS_FALLBACK 1
#endif

#if defined(ARENAC_THREADS_FALLBACK)

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

typedef pthread_t       thrd_t;
typedef pthread_mutex_t mtx_t;

#define mtx_plain       0
#define mtx_recursive   1
#define mtx_timed       2

#define thrd_success    0
#define thrd_busy       1
#define thrd_error      2
#define thrd_nomem      3
#define thrd_timedout   4

static inline int mtx_init(mtx_t* m, int type)
{
#if defined(PTHREAD_MUTEX_RECURSIVE)
    if (type == mtx_recursive)
    {
        pthread_mutexattr_t attr;
        if (pthread_mutexattr_init(&attr) != 0) return thrd_error;
        if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
        {
            pthread_mutexattr_destroy(&attr);
            return thrd_error;
        }
        int r = pthread_mutex_init(m, &attr);
        pthread_mutexattr_destroy(&attr);
        return r == 0 ? thrd_success : thrd_error;
    }
#else
    (void)type;
#endif
    return pthread_mutex_init(m, NULL) == 0 ? thrd_success : thrd_error;
}

static inline int mtx_lock(mtx_t* m)
{
    return pthread_mutex_lock(m) == 0 ? thrd_success : thrd_error;
}

static inline int mtx_unlock(mtx_t* m)
{
    return pthread_mutex_unlock(m) == 0 ? thrd_success : thrd_error;
}

static inline void mtx_destroy(mtx_t* m)
{
    pthread_mutex_destroy(m);
}

typedef struct {
    int (*fn)(void*);
    void* arg;
} arenac_thrd_pack;

static inline void* arenac_thrd_proc(void* p)
{
    arenac_thrd_pack* k = p;
    int r = k->fn(k->arg);
    free(k);
    return (void*)(intptr_t)r;
}

static inline int thrd_create(thrd_t* t, int (*fn)(void*), void* arg)
{
    arenac_thrd_pack* k = malloc(sizeof *k);
    if (!k) return thrd_nomem;
    k->fn = fn;
    k->arg = arg;
    if (pthread_create(t, NULL, arenac_thrd_proc, k) != 0)
    {
        free(k);
        return thrd_error;
    }
    return thrd_success;
}

static inline int thrd_join(thrd_t t, int* res)
{
    void* r;
    if (pthread_join(t, &r) != 0) return thrd_error;
    if (res) *res = (int)(intptr_t)r;
    return thrd_success;
}

#endif

#if defined(ARENAC_THREADS_FALLBACK) || (defined(__has_include) && __has_include(<pthread.h>))
#  define ARENAC_HAS_TLS_DTOR 1
#else
#  define ARENAC_HAS_TLS_DTOR 0
#endif

#if ARENAC_HAS_TLS_DTOR

#include <pthread.h>

typedef pthread_key_t   arenac_tls_key;
typedef pthread_once_t  arenac_tls_once;

#define ARENAC_TLS_ONCE_INIT PTHREAD_ONCE_INIT

typedef void (*ArenacTlsDtor)(void*);

static inline void arenac_tls_call_once(arenac_tls_once* once, void (*init)(void))
{
    pthread_once(once, init);
}

static inline int arenac_tls_key_create(arenac_tls_key* key, ArenacTlsDtor dtor)
{
    return pthread_key_create(key, dtor);
}

static inline void* arenac_tls_get(arenac_tls_key key)
{
    return pthread_getspecific(key);
}

static inline void arenac_tls_set(arenac_tls_key key, void* value)
{
    pthread_setspecific(key, value);
}

#endif

#endif
