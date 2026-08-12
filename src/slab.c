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

static Slab* slab_init(Slab* s, size_t object_size, size_t objects_per_block)
{
    if (object_size < sizeof(void*))
        object_size = sizeof(void*);
    if (object_size > SIZE_MAX - (sizeof(void*) - 1))
        return NULL;
    object_size = (object_size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);

    if (objects_per_block == 0 || object_size > SIZE_MAX / objects_per_block)
        return NULL;

    s->block = malloc(object_size * objects_per_block);
    if (!s->block)
        return NULL;

    s->object_size = object_size;
    s->block_size = object_size * objects_per_block;
    s->free_count = objects_per_block;

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
    Slab* s = malloc(sizeof(Slab));
    if (!s) return NULL;

    if (!slab_init(s, object_size, objects_per_block))
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

void slab_destroy(Slab* s)
{
    free(s->block);
    free(s);
}

static _Thread_local Slab* tls_slab;

Slab* slab_tls_create(size_t object_size, size_t objects_per_block)
{
    if (tls_slab)
    {
        slab_destroy(tls_slab);
    }
    tls_slab = slab_create(object_size, objects_per_block);
    return tls_slab;
}

void* slab_tls_alloc(void)
{
    if (!tls_slab) return NULL;
    return slab_alloc(tls_slab);
}

void slab_tls_free(void* ptr)
{
    if (!tls_slab) return;
    slab_free(tls_slab, ptr);
}

void slab_tls_destroy(void)
{
    if (!tls_slab) return;
    slab_destroy(tls_slab);
    tls_slab = NULL;
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

    if (!slab_init(&s->slab, object_size, objects_per_block))
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
    free(s->slab.block);
    free(s);
}
