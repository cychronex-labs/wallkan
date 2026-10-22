#ifndef WALLKAN_YYJSON_ALC_H
#define WALLKAN_YYJSON_ALC_H

#include <yyjson.h>

#include "arena_alloc.h"

yyjson_alc
wk_yyjson_create_pool(ArenaAllocator *alloc);

#endif
