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
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) return NULL;

    uintptr_t addr = (uintptr_t)(a->base + a->offset);
    if (addr > UINTPTR_MAX - (alignment - 1)) return NULL;
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

static _Thread_local Arena* tls_arena;

Arena* arena_tls_create(size_t initial_size)
{
    if (tls_arena)
    {
        arena_destroy(tls_arena);
    }
    tls_arena = arena_create(initial_size);
    return tls_arena;
}

void* arena_tls_alloc(size_t size)
{
    if (!tls_arena) return NULL;
    return arena_alloc(tls_arena, size);
}

void* arena_tls_alloc_aligned(size_t size, size_t alignment)
{
    if (!tls_arena) return NULL;
    return arena_alloc_aligned(tls_arena, size, alignment);
}

void arena_tls_reset(void)
{
    if (!tls_arena) return;
    arena_reset(tls_arena);
}

void arena_tls_destroy(void)
{
    if (!tls_arena) return;
    arena_destroy(tls_arena);
    tls_arena = NULL;
}

ArenaShared* arena_shared_create(size_t initial_size)
{
    ArenaShared* s = malloc(sizeof *s);
    if (!s) return NULL;

    if (mtx_init(&s->lock, mtx_plain) != thrd_success)
    {
        free(s);
        return NULL;
    }

    if (initial_size == 0)
        initial_size = 1;

    s->arena.base = malloc(initial_size);
    if (!s->arena.base)
    {
        mtx_destroy(&s->lock);
        free(s);
        return NULL;
    }

    s->arena.size = initial_size;
    s->arena.offset = 0;

    return s;
}

void* arena_shared_alloc(ArenaShared* s, size_t size)
{
    if (mtx_lock(&s->lock) != thrd_success) return NULL;
    void* p = arena_alloc(&s->arena, size);
    mtx_unlock(&s->lock);
    return p;
}

void* arena_shared_alloc_aligned(ArenaShared* s, size_t size, size_t alignment)
{
    if (mtx_lock(&s->lock) != thrd_success) return NULL;
    void* p = arena_alloc_aligned(&s->arena, size, alignment);
    mtx_unlock(&s->lock);
    return p;
}

void arena_shared_reset(ArenaShared* s)
{
    if (mtx_lock(&s->lock) != thrd_success) return;
    arena_reset(&s->arena);
    mtx_unlock(&s->lock);
}

void arena_shared_destroy(ArenaShared* s)
{
    mtx_destroy(&s->lock);
    free(s->arena.base);
    free(s);
}
