// Instead of numerous allocation just for a temporary initialization phase
// Allocate a large pool for all temporary allocations and free all of them at once when
// initialization phase ends
#ifndef WALLKAN_ARENA_ALLOCATOR
#define WALLKAN_ARENA_ALLOCATOR
#include "err.h"
#include <stdint.h>

typedef struct ArenaAllocator {
    void *addr;
    uint32_t used;
} ArenaAllocator;

WkResult
arena_alloc_init(ArenaAllocator *allocator);

void *
arena_alloc(ArenaAllocator *allocator, uint32_t size);

void
arena_alloc_free(ArenaAllocator *allocator);

#endif
