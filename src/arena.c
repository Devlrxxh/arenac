#include "arena.h"

#include <stdint.h>
#include <stdlib.h>

static Arena* arena_init(Arena* a, size_t initial_size, bool growable)
{
    if (initial_size == 0)
        initial_size = 1;

    a->base = malloc(initial_size);
    if (!a->base) return NULL;

    a->size = initial_size;
    a->offset = 0;
    a->growable = growable;
    a->prev = NULL;

    return a;
}

Arena* arena_create(size_t initial_size, bool growable)
{
    Arena* a = malloc(sizeof(Arena));
    if (!a) return NULL;

    if (!arena_init(a, initial_size, growable))
    {
        free(a);
        return NULL;
    }

    return a;
}

static int arena_grow(Arena* a, size_t needed)
{
    if (needed == 0)
        needed = 1;

    size_t new_size;
    if (a->size > SIZE_MAX / 2)
        new_size = needed;
    else
        new_size = a->size * 2;
    if (new_size < needed)
        new_size = needed;

    Arena* old = malloc(sizeof(Arena));
    if (!old) return 0;

    old->base = a->base;
    old->size = a->size;
    old->offset = a->offset;
    old->prev = a->prev;
    a->prev = old;

    unsigned char* new_base = malloc(new_size);
    if (!new_base)
    {
        a->prev = old->prev;
        free(old);
        return 0;
    }

    a->base = new_base;
    a->size = new_size;
    a->offset = 0;
    return 1;
}

void* arena_alloc(Arena* a, size_t size)
{
    if (size > a->size - a->offset)
    {
        if (!a->growable) return NULL;
        if (!arena_grow(a, size)) return NULL;
    }

    void* address = a->base + a->offset;
    a->offset += size;
    return address;
}

void* arena_alloc_array(Arena* a, size_t n, size_t elem_size)
{
    if (n != 0 && elem_size > SIZE_MAX / n)
        return NULL;
    return arena_alloc(a, n * elem_size);
}

void* arena_alloc_aligned(Arena* a, size_t size, size_t alignment)
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) return NULL;

    for (;;)
    {
        uintptr_t addr = (uintptr_t)(a->base + a->offset);
        if (addr > UINTPTR_MAX - (alignment - 1)) return NULL;
        uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
        size_t pad = (size_t)(aligned - addr);

        if (pad > a->size - a->offset || size > a->size - a->offset - pad)
        {
            if (!a->growable) return NULL;
            if (pad > SIZE_MAX - size) return NULL;
            if (!arena_grow(a, pad + size)) return NULL;
            continue;
        }

        a->offset += pad + size;
        return (void*)aligned;
    }
}

void arena_reset(Arena* a)
{
    a->offset = 0;
}

static void arena_destroy_blocks(Arena* a)
{
    Arena* b = a->prev;
    while (b)
    {
        Arena* next = b->prev;
        free(b->base);
        free(b);
        b = next;
    }
    free(a->base);
}

void arena_destroy(Arena* a)
{
    arena_destroy_blocks(a);
    free(a);
}

static _Thread_local Arena* tls_arena;

Arena* arena_tls_create(size_t initial_size, bool growable)
{
    if (tls_arena)
    {
        arena_destroy(tls_arena);
    }
    tls_arena = arena_create(initial_size, growable);
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

ArenaShared* arena_shared_create(size_t initial_size, bool growable)
{
    ArenaShared* s = malloc(sizeof *s);
    if (!s) return NULL;

    if (mtx_init(&s->lock, mtx_plain) != thrd_success)
    {
        free(s);
        return NULL;
    }

    if (!arena_init(&s->arena, initial_size, growable))
    {
        mtx_destroy(&s->lock);
        free(s);
        return NULL;
    }

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
    arena_destroy_blocks(&s->arena);
    free(s);
}
