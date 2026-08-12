#ifndef SLAB_H
#define SLAB_H

#include "arenac_threads.h"
#include <stdbool.h>
#include <stddef.h>

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
    size_t         free_count;  // how many slots are currently free
    AcAllocFn      alloc_fn;    // backing allocator
    AcFreeFn       free_fn;     // backing deallocator
    void*          ctx;         // opaque context passed to the backing allocator
} Slab;

Slab*  slab_create(size_t object_size, size_t objects_per_block);
Slab*  slab_create_with_allocator(size_t object_size, size_t objects_per_block,
                                  AcAllocFn alloc_fn, AcFreeFn free_fn,
                                  void* ctx);
void*  slab_alloc(Slab* s);
void   slab_free(Slab* s, void* ptr);
bool   slab_is_from(Slab* s, void* ptr);
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
