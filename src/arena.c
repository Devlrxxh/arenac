#include "arena.h"

#include <stdlib.h>

Arena* arena_create(size_t initial_size)
{
    Arena* a = malloc(sizeof(Arena));
    a->base = malloc(initial_size);

    a->size = initial_size;
    a->offset = 0;
  
    return a;
}

void* arena_alloc(Arena* a, size_t size)
{
    if (a->offset + size > a->size) return NULL;

    void* address = a->base + a->offset;
    a->offset += size;
    return address;
}

void* arena_alloc_aligned(Arena* a, size_t size, size_t alignment)
{
    if (alignment == 0) return NULL;

    size_t aligned = (a->offset + alignment - 1) & ~(alignment - 1);
    if (aligned + size > a->size) return NULL;

    void* address = a->base + aligned;
    a->offset = aligned + size;
    return address;
}

void arena_reset(Arena* a)
{
    a->offset = 0;
}

void arena_destroy(Arena* a)
{
    free(a->base);
    free((void*) a);
}
