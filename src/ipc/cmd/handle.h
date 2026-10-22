#ifndef WALLKAN_IPC_CMD_HANDLE_H
#define WALLKAN_IPC_CMD_HANDLE_H

#include <stdint.h>

#include <yyjson.h>

#include "arena_alloc.h"
#include "err.h"

typedef struct Wallkan Wallkan;

WkResult
ipc_cmd_handle(ArenaAllocator *alloc, yyjson_doc *doc, Wallkan *wk, uint32_t client_idx);

#endif
