#define ARENAC_NO_INLINE
#include "slab.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void slot_set_next(unsigned char* slot, unsigned char* next)
{
    unsigned char* tmp = next;
    memcpy(slot, &tmp, sizeof tmp);
}

static unsigned char* slot_get_next(unsigned char* slot)
{
    unsigned char* next;
    memcpy(&next, slot, sizeof next);
    return next;
}

static void* default_alloc(void* ctx, size_t size)
{
    (void)ctx;
    return malloc(size);
}

static void default_free(void* ctx, void* ptr)
{
    (void)ctx;
    free(ptr);
}

static Slab* slab_init(Slab* s, size_t object_size, size_t objects_per_block,
                       AcAllocFn alloc_fn, AcFreeFn free_fn, void* ctx)
{
    if (object_size < sizeof(void*))
        object_size = sizeof(void*);
    if (object_size > SIZE_MAX - (sizeof(void*) - 1))
        return NULL;
    object_size = (object_size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);

    if (objects_per_block == 0 || object_size > SIZE_MAX / objects_per_block)
        return NULL;

    s->block = alloc_fn(ctx, object_size * objects_per_block);
    if (!s->block)
        return NULL;

    s->object_size = object_size;
    s->block_size = object_size * objects_per_block;
    s->free_count = objects_per_block;
    s->pow2 = (object_size & (object_size - 1)) == 0;
    s->alloc_fn = alloc_fn;
    s->free_fn = free_fn;
    s->ctx = ctx;

    unsigned char* slot = s->block;
    for (size_t i = 0; i < objects_per_block - 1; i++)
    {
        slot_set_next(slot, slot + object_size);
        slot += object_size;
    }
    slot_set_next(slot, NULL);
    s->free_head = s->block;

    return s;
}

Slab* slab_create(size_t object_size, size_t objects_per_block)
{
    return slab_create_with_allocator(object_size, objects_per_block,
                                      default_alloc, default_free, NULL);
}

Slab* slab_create_with_allocator(size_t object_size, size_t objects_per_block,
                                 AcAllocFn alloc_fn, AcFreeFn free_fn,
                                 void* ctx)
{
    if (!alloc_fn || !free_fn) return NULL;

    Slab* s = malloc(sizeof(Slab));
    if (!s) return NULL;

    if (!slab_init(s, object_size, objects_per_block, alloc_fn, free_fn, ctx))
    {
        free(s);
        return NULL;
    }

    return s;
}

void* slab_alloc(Slab* s)
{
    if (s->free_count == 0) return NULL;

    unsigned char* slot = s->free_head;
    s->free_head = slot_get_next(slot);
    s->free_count--;

    return slot;
}

bool slab_is_from(Slab* s, void* ptr)
{
    if (!ptr) return false;

    uintptr_t p = (uintptr_t)ptr;
    uintptr_t base = (uintptr_t)s->block;
    if (p < base) return false;

    uintptr_t off = p - base;
    if (off >= s->block_size) return false;

    if (s->pow2)
        return (off & (s->object_size - 1)) == 0;
    return off % s->object_size == 0;
}

void slab_free(Slab* s, void* ptr)
{
    if (!slab_is_from(s, ptr))
    {
        fprintf(stderr, "slab_free: not a slab slot\n");
        abort();
    }

    unsigned char* slot = ptr;
    slot_set_next(slot, s->free_head);
    s->free_head = slot;
    s->free_count++;
}

size_t slab_get_free_count(const Slab* s)
{
    return s->free_count;
}

size_t slab_get_active_count(const Slab* s)
{
    return s->block_size / s->object_size - s->free_count;
}

void slab_destroy(Slab* s)
{
    s->free_fn(s->ctx, s->block);
    free(s);
}

#if ARENAC_HAS_TLS_DTOR
static arenac_tls_key  slab_tls_key;
static arenac_tls_once slab_tls_once = ARENAC_TLS_ONCE_INIT;

static void slab_tls_dtor(void* p)
{
    slab_destroy((Slab*)p);
}

static void slab_tls_key_init(void)
{
    arenac_tls_key_create(&slab_tls_key, slab_tls_dtor);
}

static Slab* slab_tls_get(void)
{
    arenac_tls_call_once(&slab_tls_once, slab_tls_key_init);
    return (Slab*)arenac_tls_get(slab_tls_key);
}

static void slab_tls_set(Slab* s)
{
    arenac_tls_call_once(&slab_tls_once, slab_tls_key_init);
    arenac_tls_set(slab_tls_key, s);
}
#else
static _Thread_local Slab* tls_slab;

#define slab_tls_get() (tls_slab)
#define slab_tls_set(s) (tls_slab = (s))
#endif

Slab* slab_tls_create(size_t object_size, size_t objects_per_block)
{
    Slab* prev = slab_tls_get();
    if (prev)
    {
        slab_tls_set(NULL);
        slab_destroy(prev);
    }

    Slab* s = slab_create(object_size, objects_per_block);
    slab_tls_set(s);
    return s;
}

void* slab_tls_alloc(void)
{
    Slab* s = slab_tls_get();
    if (!s) return NULL;
    return slab_alloc(s);
}

void slab_tls_free(void* ptr)
{
    Slab* s = slab_tls_get();
    if (!s) return;
    slab_free(s, ptr);
}

void slab_tls_destroy(void)
{
    Slab* s = slab_tls_get();
    if (!s) return;
    slab_tls_set(NULL);
    slab_destroy(s);
}

SlabShared* slab_shared_create(size_t object_size, size_t objects_per_block)
{
    SlabShared* s = malloc(sizeof *s);
    if (!s) return NULL;

    if (mtx_init(&s->lock, mtx_plain) != thrd_success)
    {
        free(s);
        return NULL;
    }

    if (!slab_init(&s->slab, object_size, objects_per_block,
                   default_alloc, default_free, NULL))
    {
        mtx_destroy(&s->lock);
        free(s);
        return NULL;
    }

    return s;
}

void* slab_shared_alloc(SlabShared* s)
{
    if (mtx_lock(&s->lock) != thrd_success) return NULL;
    void* p = slab_alloc(&s->slab);
    mtx_unlock(&s->lock);
    return p;
}

void slab_shared_free(SlabShared* s, void* ptr)
{
    if (mtx_lock(&s->lock) != thrd_success) return;
    slab_free(&s->slab, ptr);
    mtx_unlock(&s->lock);
}

void slab_shared_destroy(SlabShared* s)
{
    mtx_destroy(&s->lock);
    s->slab.free_fn(s->slab.ctx, s->slab.block);
    free(s);
}
