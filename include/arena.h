#ifndef ARENA_H
#define ARENA_H

#include "arenac_threads.h"
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

Arena* arena_tls_create(size_t initial_size);
void*  arena_tls_alloc(size_t size);
void*  arena_tls_alloc_aligned(size_t size, size_t alignment);
void   arena_tls_reset(void);
void   arena_tls_destroy(void);

typedef struct {
    mtx_t lock;
    Arena arena;
} ArenaShared;

ArenaShared* arena_shared_create(size_t initial_size);
void*        arena_shared_alloc(ArenaShared* s, size_t size);
void*        arena_shared_alloc_aligned(ArenaShared* s, size_t size, size_t alignment);
void         arena_shared_reset(ArenaShared* s);
void         arena_shared_destroy(ArenaShared* s);

#endif
