#ifndef WALLKAN_IPC_OUTPUT_H
#define WALLKAN_IPC_OUTPUT_H
#include "err.h"
#include "wallkan.h"

WkResult
ipc_cmd_output_list(Wallkan *wk);

WkResult
ipc_cmd_output_enable(ArenaAllocator *alloc, Wallkan *wk, yyjson_doc *cmd_doc);

WkResult
ipc_cmd_output_disable(Wallkan *wk, yyjson_doc *cmd_doc);

#endif
