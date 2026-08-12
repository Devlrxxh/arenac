#include "slab.h"
#include "arenac_threads.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static int fails = 0;

#define FAIL(msg)                       \
    do {                                \
        printf("FAIL: %s\n", msg);      \
        fails++;                        \
    } while (0)

static int cmp_uintptr(const void* a, const void* b)
{
    uintptr_t x = *(const uintptr_t*)a;
    uintptr_t y = *(const uintptr_t*)b;
    return (x > y) - (x < y);
}

static size_t count_nonnull(void* const* arr, size_t n)
{
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        if (arr[i]) k++;
    return k;
}

static void verify_unique(void* const* arr, size_t n)
{
    uintptr_t* v = malloc(n * sizeof *v);
    if (!v) return;

    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        if (arr[i]) v[k++] = (uintptr_t)arr[i];

    qsort(v, k, sizeof *v, cmp_uintptr);
    for (size_t i = 1; i < k; i++)
        if (v[i] <= v[i - 1]) FAIL("overlapping allocation");

    free(v);
}

static void free_null(void* arg)
{
    slab_free(arg, NULL);
}

static void free_interior(void* arg)
{
    Slab* s = arg;
    slab_free(s, (unsigned char*)s->block + 1);
}

static void free_foreign(void* arg)
{
    Slab* s = arg;
    void* f = malloc(16);
    slab_free(s, f);
}

static int dies_with_abort(void (*fn)(void*), void* arg)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        fn(arg);
        _exit(0);
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

static int tls_worker(void* arg)
{
    (void)arg;

    void* slots[64];
    int n = 0;

    if (!slab_tls_create(64, 64))
        FAIL("slab_tls_create");

    while (n < 64)
    {
        void* p = slab_tls_alloc();
        if (!p) break;
        slots[n++] = p;
    }
    if (n != 64) FAIL("slab_tls alloc count");

    for (int i = 0; i < n; i++)
        slab_tls_free(slots[i]);
    for (int i = 0; i < n; i++)
        if (!slab_tls_alloc()) FAIL("slab_tls alloc after free");

    if (!slab_tls_create(16, 4))
        FAIL("slab_tls_create (2nd)");
    if (!slab_tls_alloc())
        FAIL("alloc after re-create");

    slab_tls_destroy();
    if (slab_tls_alloc()) FAIL("alloc after destroy");

    return 0;
}

static void test_slab_tls(void)
{
    enum { T = 8 };
    thrd_t th[T];

    if (slab_tls_alloc()) FAIL("alloc with no TLS slab");

    for (int i = 0; i < T; i++)
        thrd_create(&th[i], tls_worker, NULL);
    for (int i = 0; i < T; i++)
        thrd_join(th[i], NULL);

    if (!slab_tls_create(16, 4))
        FAIL("main create");
    if (!slab_tls_alloc())
        FAIL("main alloc");
    if (!slab_tls_create(32, 16))
        FAIL("main re-create");
    if (!slab_tls_alloc())
        FAIL("main alloc after re-create");
    slab_tls_destroy();
    if (slab_tls_alloc()) FAIL("main alloc after destroy");
}

typedef struct {
    SlabShared* shared;
    size_t      id;
    size_t      per_thread;
    void**      out;
} ShsArgs;

static int shared_alloc_worker(void* arg)
{
    ShsArgs* a = arg;
    for (size_t i = 0; i < a->per_thread; i++)
        a->out[a->id * a->per_thread + i] = slab_shared_alloc(a->shared);
    return 0;
}

static int shared_free_worker(void* arg)
{
    ShsArgs* a = arg;
    for (size_t i = 0; i < a->per_thread; i++)
        slab_shared_free(a->shared, a->out[a->id * a->per_thread + i]);
    return 0;
}

static int shared_stress_worker(void* arg)
{
    ShsArgs* a = arg;
    for (int iter = 0; iter < 200; iter++)
    {
        void* p = slab_shared_alloc(a->shared);
        if (p) slab_shared_free(a->shared, p);
    }
    return 0;
}

static void run_workers(thrd_t th[], ShsArgs args[], size_t n, int (*fn)(void*))
{
    for (size_t i = 0; i < n; i++)
        thrd_create(&th[i], fn, &args[i]);
    for (size_t i = 0; i < n; i++)
        thrd_join(th[i], NULL);
}

static void test_slab_shared(void)
{
    enum { T = 8, PER = 32, OVER = 128 };
    enum { SLOTS = T * PER };

    SlabShared* s = slab_shared_create(64, SLOTS);
    if (!s) FAIL("slab_shared_create");

    void** out = malloc(T * PER * sizeof *out);
    thrd_t th[T];
    ShsArgs args[T];

    for (size_t i = 0; i < T; i++)
    {
        args[i].shared = s;
        args[i].id = i;
        args[i].per_thread = PER;
        args[i].out = out;
    }

    run_workers(th, args, T, shared_alloc_worker);
    if (count_nonnull(out, T * PER) != SLOTS) FAIL("shared slab alloc count");
    verify_unique(out, T * PER);

    run_workers(th, args, T, shared_free_worker);
    run_workers(th, args, T, shared_alloc_worker);
    if (count_nonnull(out, T * PER) != SLOTS) FAIL("shared slab realloc count");
    verify_unique(out, T * PER);

    run_workers(th, args, T, shared_stress_worker);
    run_workers(th, args, T, shared_free_worker);

    void** out2 = malloc(T * OVER * sizeof *out2);
    ShsArgs args2[T];
    for (size_t i = 0; i < T; i++)
    {
        args2[i].shared = s;
        args2[i].id = i;
        args2[i].per_thread = OVER;
        args2[i].out = out2;
    }
    run_workers(th, args2, T, shared_alloc_worker);
    if (count_nonnull(out2, T * OVER) != SLOTS) FAIL("shared slab exhaustion");
    verify_unique(out2, T * OVER);

    slab_shared_destroy(s);
    free(out2);
    free(out);
}

int main(void)
{
    Slab* s = slab_create(32, 4);

    printf("Block: %p\n", (void*)s->block);
    printf("Object size: %llu\n", (unsigned long long)s->object_size);
    printf("Free count: %llu\n", (unsigned long long)s->free_count);

    void* a = slab_alloc(s);
    void* b = slab_alloc(s);
    printf("Alloc 1: %p\n", (void*)a);
    printf("Alloc 2: %p\n", (void*)b);
    printf("Free count after 2 allocs: %llu\n", (unsigned long long)s->free_count);

    slab_free(s, a);
    printf("Free count after free: %llu\n", (unsigned long long)s->free_count);

    void* a2 = slab_alloc(s);
    printf("Realloc: %p\n", (void*)a2);
    if (a2 != a)
        printf("FAIL: didn't reuse the freed slot!\n");

    slab_destroy(s);

    Slab* c = slab_create(4, 8);
    if (!c) FAIL("slab_create (small object)");
    if (c->object_size != sizeof(void*)) FAIL("object_size not clamped to pointer size");
    slab_destroy(c);

    if (slab_create(64, 0)) FAIL("slab_create zero objects not rejected");
    if (slab_create(SIZE_MAX, 2)) FAIL("slab_create rounded size overflow not rejected");
    if (slab_create(SIZE_MAX, SIZE_MAX)) FAIL("slab_create multiply overflow not rejected");

    Slab* d = slab_create(32, 8);
    if (!d) FAIL("slab_create (d)");
    if (slab_is_from(d, NULL)) FAIL("NULL is a slab slot");
    if (slab_is_from(d, (unsigned char*)d->block + 1)) FAIL("interior pointer is a slab slot");
    if (slab_is_from(d, (unsigned char*)d->block + d->block_size))
        FAIL("one-past-end is a slab slot");
    void* foreign = malloc(16);
    if (slab_is_from(d, foreign)) FAIL("foreign pointer is a slab slot");
    free(foreign);
    if (!slab_is_from(d, d->block)) FAIL("block pointer is not a slab slot");
    void* slot = slab_alloc(d);
    if (!slot) FAIL("slab_alloc (d)");
    if (!slab_is_from(d, slot)) FAIL("valid slot not recognized");
    slab_free(d, slot);
    if (!slab_is_from(d, slot)) FAIL("freed slot not recognized");
    if (!dies_with_abort(free_null, d)) FAIL("free(NULL) did not abort");
    if (!dies_with_abort(free_interior, d)) FAIL("free(interior) did not abort");
    if (!dies_with_abort(free_foreign, d)) FAIL("free(foreign) did not abort");
    slot = slab_alloc(d);
    if (!slot) FAIL("slab_alloc after abort tests");
    slab_free(d, slot);
    slab_destroy(d);

    test_slab_tls();
    test_slab_shared();

    if (fails)
    {
        printf("test_slab: %d thread-test failures\n", fails);
        return 1;
    }
    printf("test_slab: all tests passed\n");
    return 0;
}
