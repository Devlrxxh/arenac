#ifndef SLAB_H
#define SLAB_H

#include "arenac_threads.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ARENAC_ALLOCATOR_TYPES
#define ARENAC_ALLOCATOR_TYPES
typedef void* (*AcAllocFn)(void* ctx, size_t size);
typedef void  (*AcFreeFn)(void* ctx, void* ptr);
#endif

typedef struct {
    unsigned char* block;      // pointer to the memory block
    size_t         object_size; // size of each slot
    size_t         block_size;  // total bytes of the block
    unsigned char* free_head;   // first free slot (each free slot stores a "next free" pointer inside itself)
    unsigned char* free_bitmap; // one bit per slot: 1 = free, detects double-frees in slab_free
    size_t         free_count;  // how many slots are currently free
    bool           pow2;        // object_size is a power of two (fast slot check)
    AcAllocFn      alloc_fn;    // backing allocator
    AcFreeFn       free_fn;     // backing deallocator
    void*          ctx;         // opaque context passed to the backing allocator
} Slab;

Slab*  slab_create(size_t object_size, size_t objects_per_block);
Slab*  slab_create_with_allocator(size_t object_size, size_t objects_per_block,
                                  AcAllocFn alloc_fn, AcFreeFn free_fn,
                                  void* ctx);

#ifdef ARENAC_NO_INLINE
void*  slab_alloc(Slab* s);
void   slab_free(Slab* s, void* ptr);
bool   slab_is_from(Slab* s, void* ptr);
#else
static inline void* slab_alloc(Slab* s)
{
    if (s->free_count == 0) return NULL;

    unsigned char* slot = s->free_head;
    unsigned char* next;
    memcpy(&next, slot, sizeof next);
    s->free_head = next;

    size_t idx = (slot - s->block) / s->object_size;
    s->free_bitmap[idx >> 3] &= (unsigned char)~(1u << (idx & 7));
    s->free_count--;

    return slot;
}

static inline bool slab_is_from(Slab* s, void* ptr)
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

static inline void slab_free(Slab* s, void* ptr)
{
    if (!slab_is_from(s, ptr))
    {
        fprintf(stderr, "slab_free: not a slab slot\n");
        abort();
    }

    unsigned char* slot = ptr;
    size_t idx = (slot - s->block) / s->object_size;
    if (s->free_bitmap[idx >> 3] & (1u << (idx & 7)))
    {
        fprintf(stderr, "slab_free: double free of slot %p\n", (void*)slot);
        abort();
    }
    s->free_bitmap[idx >> 3] |= (unsigned char)(1u << (idx & 7));

    unsigned char* prev = s->free_head;
    memcpy(slot, &prev, sizeof prev);
    s->free_head = slot;
    s->free_count++;
}
#endif
size_t slab_get_free_count(const Slab* s);
size_t slab_get_active_count(const Slab* s);
void   slab_destroy(Slab* s);

Slab*  slab_tls_create(size_t object_size, size_t objects_per_block);
void*  slab_tls_alloc(void);
void   slab_tls_free(void* ptr);
void   slab_tls_destroy(void);

typedef struct {
    mtx_t lock;
    Slab  slab;
} SlabShared;

SlabShared* slab_shared_create(size_t object_size, size_t objects_per_block);
void*       slab_shared_alloc(SlabShared* s);
void        slab_shared_free(SlabShared* s, void* ptr);
void        slab_shared_destroy(SlabShared* s);

#endif
