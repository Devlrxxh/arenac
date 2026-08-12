# arenac

Fast, Thread safe arena and slab allocators for C.

## Arena

```c
#include "arena.h"

Arena* a = arena_create(1024);

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
Arena* tla = arena_tls_create(1 << 20);        // per thread
ArenaShared* s = arena_shared_create(1 << 20); // shared

void* p = arena_tls_alloc(64);
void* q = arena_shared_alloc(s, 64);
```

Each thread must use its own instance of each allocator. Do not reset or destroy an instance while another thread is using it.

## Benchmarks

| Test | malloc | arenac | Speedup |
|------|--------|--------|---------|
| Batch allocs | 145.12 ms | 42.29 ms | 3.43x |
| Alloc/free cycles | 58.33 ms | 8.24 ms | 7.08x |

## Use in your project

Copy include/ and src/ into your project and compile the source files with your application.
