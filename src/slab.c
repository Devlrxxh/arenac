#include "slab.h"

#include <stdlib.h>

Slab* slab_create(size_t object_size, size_t objects_per_block)
{
    Slab* s = malloc(sizeof(Slab));
    if (!s) return NULL;

    if (object_size < sizeof(void*))
        object_size = sizeof(void*);
    object_size = (object_size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);

    s->block = malloc(object_size * objects_per_block);
    if (!s->block)
    {
        free(s);
        return NULL;
    }

    s->object_size = object_size;
    s->free_count = objects_per_block;

    unsigned char* slot = s->block;
    for (size_t i = 0; i < objects_per_block - 1; i++)
    {
        *(unsigned char**)slot = slot + object_size;
        slot += object_size;
    }
    *(unsigned char**)slot = NULL;
    s->free_head = s->block;

    return s;
}

void* slab_alloc(Slab* s)
{
    if (s->free_count == 0) return NULL;

    unsigned char* slot = s->free_head;
    s->free_head = *(unsigned char**)slot;
    s->free_count--;

    return slot;
}

void slab_free(Slab* s, void* ptr)
{
    unsigned char* slot = ptr;
    *(unsigned char**)slot = s->free_head;
    s->free_head = slot;
    s->free_count++;
}

void slab_destroy(Slab* s)
{
    free(s->block);
    free(s);
}
