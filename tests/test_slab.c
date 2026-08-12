#include "slab.h"

#include <stdio.h>

int main(void)
{
    Slab* s = slab_create(32, 4);

    printf("Block: %p\n", (void*)s->block);
    printf("Object size: %lu\n", s->object_size);
    printf("Free count: %lu\n", s->free_count);

    void* a = slab_alloc(s);
    void* b = slab_alloc(s);
    printf("Alloc 1: %p\n", (void*)a);
    printf("Alloc 2: %p\n", (void*)b);
    printf("Free count after 2 allocs: %lu\n", s->free_count);

    slab_free(s, a);
    printf("Free count after free: %lu\n", s->free_count);

    void* a2 = slab_alloc(s);
    printf("Realloc: %p\n", (void*)a2);
    if (a2 != a)
        printf("FAIL: didn't reuse the freed slot!\n");

    slab_destroy(s);

    return 0;
}
