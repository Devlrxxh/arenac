#include "arena.h"
#include "slab.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ITER 1000000
#define OBJ_SIZE 64

static volatile unsigned char sink;

static double now_sec(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(void)
{
    void** ptrs = malloc(sizeof(void*) * ITER);
    if (!ptrs) return 1;

    double t0 = now_sec();
    for (int i = 0; i < ITER; i++)
    {
        ptrs[i] = malloc(OBJ_SIZE);
        sink = *(unsigned char*)ptrs[i];
    }
    for (int i = 0; i < ITER; i++)
        free(ptrs[i]);
    double malloc_batch = now_sec() - t0;

    Arena* a = arena_create(ITER * 256, true);
    if (!a) return 1;
    t0 = now_sec();
    for (int i = 0; i < ITER; i++)
    {
        void* p = arena_alloc(a, OBJ_SIZE);
        sink = *(unsigned char*)p;
    }
    arena_reset(a);
    double arena_batch = now_sec() - t0;
    arena_destroy(a);

    printf("1. Batch allocs\n");
    printf("   malloc: %8.2f ms\n", malloc_batch * 1000.0);
    printf("   arena:  %8.2f ms\n", arena_batch * 1000.0);
    printf("   arena %.2fx faster\n\n", malloc_batch / arena_batch);

    t0 = now_sec();
    for (int i = 0; i < ITER; i++)
    {
        void* p = malloc(OBJ_SIZE);
        sink = (unsigned char)((size_t)p & 0xff);
        free(p);
    }
    double malloc_cycle = now_sec() - t0;

    Slab* s = slab_create(OBJ_SIZE, ITER);
    if (!s) return 1;
    t0 = now_sec();
    for (int i = 0; i < ITER; i++)
    {
        void* p = slab_alloc(s);
        sink = (unsigned char)((size_t)p & 0xff);
        slab_free(s, p);
    }
    double slab_cycle = now_sec() - t0;
    slab_destroy(s);

    printf("2. Alloc/free cycles (fixed size)\n");
    printf("   malloc: %8.2f ms\n", malloc_cycle * 1000.0);
    printf("   slab:   %8.2f ms\n", slab_cycle * 1000.0);
    printf("   slab %.2fx faster\n\n", malloc_cycle / slab_cycle);

    free(ptrs);
    return 0;
}
