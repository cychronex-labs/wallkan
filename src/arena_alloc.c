#include "err.h"
#include "arena_alloc.h"
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#define ARENA_ALLOCATOR_POOL_SIZE 5 * 1024 * 1024

WkResult
arena_alloc_init(ArenaAllocator *allocator)
{
    allocator->addr = malloc(ARENA_ALLOCATOR_POOL_SIZE);
    if (!allocator->addr) {
        return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Allocation failure!");
    }
    return WK_OK;
}

static uint32_t
align_up(uint32_t size)
{
    uint32_t align = alignof(max_align_t);
    return (size + (align - 1)) & ~(align-1);
}

void *
arena_alloc(ArenaAllocator *allocator, uint32_t size)
{
    if(allocator->used + align_up(size) > ARENA_ALLOCATOR_POOL_SIZE) return NULL;
    uint8_t *next_addr = (uint8_t*)allocator->addr + allocator->used;
    allocator->used+=align_up(size);
    return (void*)next_addr;
}

void
arena_alloc_reset(ArenaAllocator *allocator)
{
    allocator->used = 0;
}

void
arena_alloc_free(ArenaAllocator *allocator)
{
    if(allocator->addr){
        free(allocator->addr);
        allocator->addr = NULL;
    }
    allocator->used = 0;
}
