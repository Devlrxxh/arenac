#include "arena.h"

#include <stdio.h>

int main(void)
{
    Arena* a = arena_create(1024);

    printf("Base: %p\n", (void*)a->base);
    printf("Size: %p\n", (void*)a->size);
    printf("Offset: %p\n", (void*)a->offset);

    void* address = arena_alloc(a, 100);
    printf("Address: %p\n", (void*)address);

    void* aligned = arena_alloc_aligned(a, 100, 16);
    printf("Aligned address: %p\n", (void*)aligned);
    if ((size_t)aligned % 16 != 0)
        printf("FAIL: not aligned!\n");

    printf("Offset before reset: %lu\n", a->offset);
    arena_reset(a);
    printf("Offset after reset: %lu\n", a->offset);

    void* again = arena_alloc(a, 100);
    printf("After reset, starts at: %p\n", (void*)again);
    if (again != a->base)
        printf("FAIL: reset didn't rewind!\n");

    arena_destroy(a);
    
    return 0;
}
