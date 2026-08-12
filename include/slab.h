#ifndef SLAB_H
#define SLAB_H

#include <stddef.h>

typedef struct Slab Slab;

Slab*  slab_create(size_t object_size, size_t objects_per_block);
void*  slab_alloc(Slab* s);
void   slab_free(Slab* s, void* ptr);
void   slab_destroy(Slab* s);

#endif
