#ifndef SLAB_H
#define SLAB_H

#include <stddef.h>

typedef struct {
    unsigned char* block;      // pointer to the memory block
    size_t         object_size; // size of each slot
    unsigned char* free_head;   // first free slot (each free slot stores a "next free" pointer inside itself)
    size_t         free_count;  // how many slots are currently free
} Slab;

Slab*  slab_create(size_t object_size, size_t objects_per_block);
void*  slab_alloc(Slab* s);
void   slab_free(Slab* s, void* ptr);
void   slab_destroy(Slab* s);

#endif
