#ifndef SLAB_H
#define SLAB_H

#include "arenac_threads.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    unsigned char* block;      // pointer to the memory block
    size_t         object_size; // size of each slot
    size_t         block_size;  // total bytes of the block
    unsigned char* free_head;   // first free slot (each free slot stores a "next free" pointer inside itself)
    size_t         free_count;  // how many slots are currently free
} Slab;

Slab*  slab_create(size_t object_size, size_t objects_per_block);
void*  slab_alloc(Slab* s);
void   slab_free(Slab* s, void* ptr);
bool   slab_is_from(Slab* s, void* ptr);
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
