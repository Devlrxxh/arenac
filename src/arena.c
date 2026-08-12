#include "arena.h"

#include <stdint.h>
#include <stdlib.h>

Arena* arena_create(size_t initial_size)
{
    Arena* a = malloc(sizeof(Arena));
    if (!a) return NULL;

    if (initial_size == 0)
        initial_size = 1;

    a->base = malloc(initial_size);
    if (!a->base)
    {
        free(a);
        return NULL;
    }

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
    size_t pad = (size_t)(aligned - addr);

    if (pad > a->size - a->offset) return NULL;
    if (size > a->size - a->offset - pad) return NULL;

    a->offset += pad + size;
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
