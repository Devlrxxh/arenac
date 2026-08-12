#include "slab.h"

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
        slot_set_next(slot, slot + object_size);
        slot += object_size;
    }
    slot_set_next(slot, NULL);
    s->free_head = s->block;

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

void slab_free(Slab* s, void* ptr)
{
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
