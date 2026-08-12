#include "arena.h"

#include <stdint.h>
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
    if (size > a->size - a->offset) return NULL;

    void* address = a->base + a->offset;
    a->offset += size;
    return address;
}

void* arena_alloc_aligned(Arena* a, size_t size, size_t alignment)
{
    if (alignment == 0) return NULL;

    uintptr_t addr = (uintptr_t)(a->base + a->offset);
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    if (aligned + size > (uintptr_t)a->base + a->size) return NULL;

    a->offset += (size_t)(aligned - addr) + size;
    return (void*)aligned;
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
