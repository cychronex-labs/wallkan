#ifndef WALLKAN_ARENA_ALLOCATOR_H
#define WALLKAN_ARENA_ALLOCATOR_H

#include <stdint.h>

#include "err.h"

typedef struct ArenaAllocator {
    void *addr;
    uint32_t capacity;
    uint32_t used;
} ArenaAllocator;

WkResult
arena_alloc_init(ArenaAllocator *allocator);

void *
arena_alloc(ArenaAllocator *allocator, uint32_t size);

void *
arena_calloc(ArenaAllocator *allocator, uint32_t size);

void
arena_alloc_reset(ArenaAllocator *allocator);

void
arena_alloc_free(ArenaAllocator *allocator);

#endif
