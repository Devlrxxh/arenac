#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

typedef struct {
    unsigned char* base;   // pointer to start of the memory block
    size_t         size;   // total capacity of this block
    size_t         offset; // how much has been handed out so far
} Arena;

Arena* arena_create(size_t initial_size);
void*  arena_alloc(Arena* a, size_t size);
void*  arena_alloc_aligned(Arena* a, size_t size, size_t alignment);
void   arena_reset(Arena* a);
void   arena_destroy(Arena* a);

#endif
