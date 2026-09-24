#ifndef IPC_YYJSON_ALC_H
#define IPC_YYJSON_ALC_H
#include "arena_alloc.h"
#include "subprojects/yyjson/yyjson.h"

yyjson_alc
wk_yyjson_create_pool(ArenaAllocator *alloc);

#endif
