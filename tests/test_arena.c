#include "arena.h"
#include "arenac_threads.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int tls_worker(void* arg)
{
    (void)arg;

    if (!arena_tls_create(8192, true))
        FAIL("arena_tls_create");

    void* prev = NULL;
    for (int i = 0; i < 100; i++)
    {
        void* p = arena_tls_alloc(32);
        if (!p) FAIL("arena_tls_alloc");
        if (prev && (uintptr_t)p <= (uintptr_t)prev) FAIL("overlapping alloc");
        prev = p;
    }

    for (int i = 0; i < 32; i++)
    {
        void* p = arena_tls_alloc_aligned(16, 64);
        if (!p || ((uintptr_t)p & 63)) FAIL("arena_tls_alloc_aligned");
    }

    arena_tls_reset();

    for (int i = 0; i < 200; i++)
        if (!arena_tls_alloc(32)) FAIL("alloc after reset");

    size_t grown = 0;
    while (grown < 5000)
    {
        void* p = arena_tls_alloc(8);
        if (!p) break;
        grown++;
    }
    if (grown != 5000) FAIL("grow on demand");

    arena_tls_destroy();
    if (!arena_tls_create(8192, false))
        FAIL("arena_tls_create (fixed)");
    if (arena_tls_alloc(1 << 20)) FAIL("tls fixed capacity");
    if (!arena_tls_alloc(8192)) FAIL("tls alloc within capacity");
    arena_tls_destroy();
    if (arena_tls_alloc(1)) FAIL("alloc after destroy");

    if (!arena_tls_create(4096, true))
        FAIL("arena_tls_create (2nd)");
    if (!arena_tls_alloc(1))
        FAIL("alloc after re-create");
    arena_tls_destroy();

    return 0;
}

static void test_arena_tls(void)
{
    enum { T = 8 };
    thrd_t th[T];

    if (arena_tls_alloc(1)) FAIL("alloc with no TLS arena");

    for (int i = 0; i < T; i++)
        thrd_create(&th[i], tls_worker, NULL);
    for (int i = 0; i < T; i++)
        thrd_join(th[i], NULL);

    if (!arena_tls_create(4096, true))
        FAIL("main create");
    for (int i = 0; i < 100; i++)
        if (!arena_tls_alloc(16)) FAIL("main alloc");
    arena_tls_destroy();
    if (arena_tls_alloc(1)) FAIL("main alloc after destroy");
}

typedef struct {
    ArenaShared* shared;
    size_t       id;
    size_t       per_thread;
    void**       out;
} ShaArgs;

static int shared_worker(void* arg)
{
    ShaArgs* a = arg;
    for (size_t i = 0; i < a->per_thread; i++)
        a->out[a->id * a->per_thread + i] = arena_shared_alloc(a->shared, 32);
    return 0;
}

static int shared_aligned_worker(void* arg)
{
    ShaArgs* a = arg;
    for (size_t i = 0; i < a->per_thread; i++)
    {
        void* p = arena_shared_alloc_aligned(a->shared, 24, 64);
        if (!p || ((uintptr_t)p & 63)) FAIL("arena_shared_alloc_aligned");
    }
    return 0;
}

static void run_workers(thrd_t th[], ShaArgs args[], size_t n, int (*fn)(void*))
{
    for (size_t i = 0; i < n; i++)
        thrd_create(&th[i], fn, &args[i]);
    for (size_t i = 0; i < n; i++)
        thrd_join(th[i], NULL);
}

static void test_arena_shared(void)
{
    enum { T = 8, PER = 512, PER2 = 150, OVER = 600 };
    const size_t capacity = T * PER * 32;

    ArenaShared* s = arena_shared_create(capacity, false);
    if (!s) FAIL("arena_shared_create");

    size_t cap = 0;
    while (arena_shared_alloc(s, 32)) cap++;
    if (cap != capacity / 32) FAIL("shared arena fixed capacity");
    if (arena_shared_alloc(s, 32)) FAIL("shared arena past-capacity alloc");
    arena_shared_destroy(s);

    s = arena_shared_create(capacity, true);
    if (!s) FAIL("arena_shared_create (growth)");

    void** out = malloc(T * PER * sizeof *out);
    thrd_t th[T];
    ShaArgs args[T];

    for (size_t i = 0; i < T; i++)
    {
        args[i].shared = s;
        args[i].id = i;
        args[i].per_thread = PER;
        args[i].out = out;
    }

    run_workers(th, args, T, shared_worker);
    if (count_nonnull(out, T * PER) != T * PER) FAIL("shared arena count");
    verify_unique(out, T * PER);

    arena_shared_reset(s);
    run_workers(th, args, T, shared_worker);
    if (count_nonnull(out, T * PER) != T * PER) FAIL("shared arena count after reset");
    verify_unique(out, T * PER);

    arena_shared_reset(s);
    for (size_t i = 0; i < T; i++)
        args[i].per_thread = PER2;
    run_workers(th, args, T, shared_aligned_worker);

    arena_shared_reset(s);
    void** out2 = malloc(T * OVER * sizeof *out2);
    ShaArgs args2[T];
    for (size_t i = 0; i < T; i++)
    {
        args2[i].shared = s;
        args2[i].id = i;
        args2[i].per_thread = OVER;
        args2[i].out = out2;
    }
    run_workers(th, args2, T, shared_worker);
    if (count_nonnull(out2, T * OVER) != T * OVER) FAIL("shared arena growth");
    verify_unique(out2, T * OVER);

    arena_shared_destroy(s);
    free(out2);
    free(out);
}

int main(void)
{
    Arena* a = arena_create(1024, true);

    printf("Base: %p\n", (void*)a->base);
    printf("Size: %llu\n", (unsigned long long)a->size);
    printf("Offset: %llu\n", (unsigned long long)a->offset);

    void* address = arena_alloc(a, 100);
    printf("Address: %p\n", (void*)address);

    void* aligned = arena_alloc_aligned(a, 100, 16);
    printf("Aligned address: %p\n", (void*)aligned);
    if ((size_t)aligned % 16 != 0)
        printf("FAIL: not aligned!\n");

    printf("Offset before reset: %llu\n", (unsigned long long)a->offset);
    arena_reset(a);
    printf("Offset after reset: %llu\n", (unsigned long long)a->offset);

    void* again = arena_alloc(a, 100);
    printf("After reset, starts at: %p\n", (void*)again);
    if (again != a->base)
        printf("FAIL: reset didn't rewind!\n");

    arena_destroy(a);

    Arena* b = arena_create(64, true);
    if (!b) FAIL("arena_create (b)");

    void** ps = malloc(6 * sizeof *ps);
    if (!ps) FAIL("test malloc");
    for (int i = 0; i < 6; i++)
    {
        ps[i] = arena_alloc(b, 65);
        if (!ps[i]) FAIL("grow past initial size");
    }
    for (int i = 0; i < 6; i++)
        memset(ps[i], i, 65);
    for (int i = 0; i < 6; i++)
    {
        if (((unsigned char*)ps[i])[64] != (unsigned char)i)
            FAIL("block content corrupted after growth");
    }
    verify_unique(ps, 6);
    free(ps);

    void* big = arena_alloc_aligned(b, 100, 1024);
    if (!big || ((uintptr_t)big & 1023)) FAIL("aligned alloc after growth");

    if (arena_alloc_aligned(b, 16, 0)) FAIL("zero alignment not rejected");
    if (arena_alloc_aligned(b, 16, 15)) FAIL("non-power-of-two alignment not rejected");
    if (!arena_alloc_aligned(b, 16, 64)) FAIL("valid aligned alloc failed");

    int* arr = arena_alloc_array(b, 10, sizeof(int));
    if (!arr) FAIL("arena_alloc_array");
    arr[9] = 42;
    if (arr[9] != 42) FAIL("arena_alloc_array write");
    if (arena_alloc_array(b, SIZE_MAX, 2)) FAIL("arena_alloc_array overflow");
    if (arena_alloc_array(b, SIZE_MAX / 2 + 1, 2)) FAIL("arena_alloc_array partial overflow");
    arena_destroy(b);

    Arena* c = arena_create(64, false);
    if (!c) FAIL("arena_create (c)");
    if (!arena_alloc(c, 40)) FAIL("pre-alloc");
    if (arena_alloc(c, 1 << 20)) FAIL("alloc past size with growth disabled");
    if (!arena_alloc(c, 24)) FAIL("alloc within size");
    if (arena_alloc_aligned(c, 1 << 20, 4096))
        FAIL("aligned alloc past size with growth disabled");
    arena_destroy(c);

    test_arena_tls();
    test_arena_shared();

    if (fails)
    {
        printf("test_arena: %d thread-test failures\n", fails);
        return 1;
    }
    printf("test_arena: all tests passed\n");
    return 0;
}
