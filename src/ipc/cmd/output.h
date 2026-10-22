#ifndef WALLKAN_IPC_CMD_OUTPUT_H
#define WALLKAN_IPC_CMD_OUTPUT_H

#include <stdint.h>

#include <yyjson.h>

#include "arena_alloc.h"
#include "err.h"

typedef struct Wallkan Wallkan;

WkResult
ipc_cmd_output_handle(ArenaAllocator *alloc, Wallkan *wk, uint32_t client_idx, const char *action,
    yyjson_val *cmd_root);

WkResult
ipc_cmd_output_list(Wallkan *wk, uint32_t client_idx);

WkResult
ipc_cmd_output_enable(ArenaAllocator *alloc, Wallkan *wk, yyjson_val *cmd_root, uint32_t client_idx);

WkResult
ipc_cmd_output_disable(ArenaAllocator *alloc, Wallkan *wk, yyjson_val *cmd_root, uint32_t client_idx);

#endif
