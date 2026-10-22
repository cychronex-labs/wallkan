#ifndef WALLKAN_IPC_CMD_QUIT_H
#define WALLKAN_IPC_CMD_QUIT_H

#include <stdint.h>

#include "err.h"

typedef struct Wallkan Wallkan;

WkResult
ipc_cmd_quit(Wallkan *wk, uint32_t client_idx);

#endif
