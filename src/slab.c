#include "slab.h"

Slab* slab_create(size_t object_size, size_t objects_per_block)
{
    (void)object_size;
    (void)objects_per_block;
    return NULL;
}

void* slab_alloc(Slab* s)
{
    (void)s;
    return NULL;
}

void slab_free(Slab* s, void* ptr)
{
    (void)s;
    (void)ptr;
}

void slab_destroy(Slab* s)
{
    (void)s;
}
