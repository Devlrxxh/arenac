# arenac

Fast, Thread safe arena and slab allocators for C.

## Arena

```c
#include "arena.h"

Arena* a = arena_create(1024, true); // true = doubles the size of the allocation once full

int*    nums = arena_alloc(a, sizeof(int) * 10);
double* vec  = arena_alloc_aligned(a, sizeof(double) * 4, 16);

arena_reset(a);   // all old allocations are now invalid
arena_destroy(a); // free the allocation
```

## Slab

```c
#include "slab.h"

Slab* s = slab_create(64, 100); // 100 slots of 64 bytes

void* obj = slab_alloc(s);
slab_free(s, obj);
slab_destroy(s);
```

Returns NULL when all slots are in use. `slab_free` aborts on pointers that are not slots of the slab (check with `slab_is_from`).

## Thread safety

`Arena` and `Slab` are not thread safe. Two wrappers are provided:

| API                          | Use case                                            |
|------------------------------|-----------------------------------------------------|
| `arena_tls_*` / `slab_tls_*` | one instance per thread, no locks                  |
| `arena_shared_*` / `slab_shared_*` | one instance shared between threads, mutex  |

```c
Arena* tla = arena_tls_create(1 << 20, true);        // per thread
ArenaShared* s = arena_shared_create(1 << 20, true); // shared

void* p = arena_tls_alloc(64);
void* q = arena_shared_alloc(s, 64);
```

Each thread must use its own instance of each allocator. Do not reset or destroy an instance while another thread is using it.

Thread local instances are destroyed automatically at thread exit.

## Monitoring

| API                                   | Returns                                   |
|---------------------------------------|-------------------------------------------|
| `arena_get_used_bytes(a)`             | bytes currently handed out across all blocks |
| `slab_get_free_count(s)`              | free slots remaining                      |
| `slab_get_active_count(s)`            | slots currently in use                    |

## Custom allocators

`arena_create_with_allocator` and `slab_create_with_allocator` use your own `AcAllocFn`/`AcFreeFn` for the backing memory (e.g. `mmap`).

```c
void* map(void* ctx, size_t size); // your allocator
void  unmap(void* ctx, void* ptr); // your deallocator

Arena* a = arena_create_with_allocator(1 << 20, true, map, unmap, NULL);
```

## Benchmarks

| Test | malloc | arenac | Speedup |
|------|--------|--------|---------|
| Batch allocs | 119.30 ms | 31.69 ms | 3.77x |
| Alloc/free cycles | 53.28 ms | 2.80 ms | 19.02x |

## Use in your project

Copy include/ and src/ into your project and compile the source files with your application.
