#ifndef WALLKAN_IPC_H
#define WALLKAN_IPC_H
#include <stdint.h>
#include <yyjson.h>
#include "events.h"
#define MAX_IPC_BUFFER_SIZE 512

typedef struct CommandProcessor {
    uint32_t json_pool_size;
    void *json_mem;
    yyjson_alc json_parse_alc;
    yyjson_alc json_reply_alc;
} CommandProcessor;

typedef struct WallkanIpc {
    int server_fd;
    char socket_path[108];
    WallkanEventHandler *wk_ev_handler;
    CommandProcessor cmd_processor;
} WallkanIpc;

WkResult
wk_ipc_init(WallkanIpc *wk_ipc, WallkanEventHandler *wk_ev_handler);

WkResult
wk_ipc_handle_connection(WallkanIpc *wk_ipc);

void
wk_ipc_cleanup(WallkanIpc *wk_ipc);

#endif
