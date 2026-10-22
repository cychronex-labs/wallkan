#ifndef WALLKAN_IPC_CMD_SCENE_H
#define WALLKAN_IPC_CMD_SCENE_H

#include <stdint.h>

#include <yyjson.h>

#include "arena_alloc.h"
#include "err.h"

typedef struct Wallkan Wallkan;

WkResult
ipc_cmd_scene_handle(ArenaAllocator *alloc, Wallkan *wk, uint32_t client_idx, const char *action,
    yyjson_val *cmd_root);

#endif
