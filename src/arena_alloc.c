#include <sys/mman.h>
#include <unistd.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "err.h"
#include "arena_alloc.h"
#define ARENA_CAPACITY (12 * 1024 * 1024)
#define ARENA_MADVISE_THRESHOLD (6 * 1024 * 1024)

WkResult
arena_alloc_init(ArenaAllocator *allocator)
{
    allocator->capacity = ARENA_CAPACITY;
    allocator->addr = mmap(NULL, ARENA_CAPACITY, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (allocator->addr == MAP_FAILED) {
        return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "mmap failed!");
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
    uint32_t aligned_size = align_up(size);

    if (!allocator->addr) return NULL;
    if(aligned_size > allocator->capacity - allocator->used) return NULL;
    uint8_t *next_addr = (uint8_t*)allocator->addr + allocator->used;

    allocator->used+=aligned_size;
    return (void*)next_addr;
}

void
arena_alloc_reset(ArenaAllocator *allocator)
{
    if(allocator->used >= ARENA_MADVISE_THRESHOLD){
        madvise(allocator->addr, allocator->capacity, MADV_DONTNEED);
    }
    allocator->used = 0;
}

void
arena_alloc_free(ArenaAllocator *allocator)
{
    if (allocator->addr) {
        munmap(allocator->addr, allocator->capacity);
        allocator->addr = NULL;
    }
    allocator->capacity = 0;
    allocator->used = 0;
}
