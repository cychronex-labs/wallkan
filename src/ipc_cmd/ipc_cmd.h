#ifndef WALLKAN_IPC_CMD_H
#define WALLKAN_IPC_CMD_H
#include "err.h"
#include "wallkan.h"

WkResult
ipc_cmd_handle(ArenaAllocator *alloc, yyjson_doc *doc, Wallkan *wk);

#endif
