#include "arena_alloc.h"
#include "err.h"
#include "subprojects/yyjson/yyjson.h"
#include <string.h>
// if yyjson json data exceeds the size of initial pool size it will use malloc
// To prevent it we feed it custom wrapper around Arena Allocator
static void *wk_yyjson_arena_alloc(void *ctx, size_t size) {
    return arena_alloc(ctx, size);
}

static void *wk_yyjson_arena_realloc(void *ctx, void *ptr, size_t old_size, size_t size) {
    void *new_mem = arena_alloc(ctx, size);
    if(!new_mem){
        return NULL;
    }
    if(ptr && old_size > 0){
        size_t copy_size = (old_size > size) ? size : old_size;
        memcpy(new_mem, ptr, copy_size);
    }
    return new_mem;
}
// Meaningless and best if avoided because arena always resets itself on each frame
static void wk_yyjson_arena_free(void *ctx, void *ptr) {
    (void)ctx;
    (void)(ptr);
}

yyjson_alc
wk_yyjson_create_pool(ArenaAllocator *alloc)
{
    return (yyjson_alc){
        .malloc = wk_yyjson_arena_alloc,
        .realloc = wk_yyjson_arena_realloc,
        .free = wk_yyjson_arena_free,
        .ctx = alloc
    };
}
