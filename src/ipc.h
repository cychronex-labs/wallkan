#ifndef WALLKAN_IPC_H
#define WALLKAN_IPC_H
#include <stdint.h>
#include <yyjson.h>
#include "arena_alloc.h"
#include "events.h"
#define MAX_IPC_BUFFER_SIZE 512
#define MAX_IPC_CLIENTS 8
typedef struct Wallkan Wallkan;

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
    int client_fd[MAX_IPC_CLIENTS];
    uint8_t active_client_bits;
    uint8_t reply_is_due_bits;
} WallkanIpc;

typedef enum IPCCommandCodes {
    WK_IPC_COMMAND_CODE_QUIT,
    WK_IPC_COMMAND_CODE_OUTPUT_LIST,
    WK_IPC_COMMAND_CODE_OUTPUT_ENABLE,
} IPCCommandCodes;

typedef enum ReplyCodes {
    WK_IPC_REPLY_OK,
    WK_IPC_REPLY_INTERNAL_ERROR,
    WK_IPC_REPLY_INVALID_DATA,
    WK_IPC_REPLY_UNKNOWN_COMMAND,
} ReplyCodes;

typedef struct WkIPCReply{
    char message[128];
    yyjson_mut_doc *response_doc;
    ReplyCodes reply_code;
} WkIPCReply;

WkResult
wk_ipc_init(WallkanIpc *wk_ipc, WallkanEventHandler *wk_ev_handler);

WkResult
wk_ipc_accept_connection(Wallkan *wk, uint32_t *out_client_idx);

WkResult
wk_ipc_read_client(ArenaAllocator *alloc, Wallkan *wk, uint32_t client_idx);

bool
wk_ipc_reply_pending(WallkanIpc *wk_ipc, uint32_t client_idx);

WkResult
wk_ipc_reply(WallkanIpc *wk_ipc, uint32_t client_idx, const WkIPCReply *reply);

void
wk_ipc_disconnect_client(WallkanIpc *wk_ipc, uint32_t client_idx);

void
wk_ipc_cleanup(WallkanIpc *wk_ipc);

#endif
