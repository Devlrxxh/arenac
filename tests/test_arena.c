#include "arena.h"

#include <stdio.h>

int main(void)
{
    Arena* a = arena_create(1024);

    printf("Base: %p\n", (void*)a->base);
    printf("Size: %p\n", (void*)a->size);
    printf("Offset: %p\n", (void*)a->offset);

    void* address =arena_alloc(a, 100);
    printf("Address: %p\n", (void*)address);

    arena_destroy(a);
    
    return 0;
}
