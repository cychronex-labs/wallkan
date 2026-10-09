#ifndef WALLKAN_IPC_SERVER_H
#define WALLKAN_IPC_SERVER_H
#include <stdint.h>
#include <yyjson.h>
#include "arena_alloc.h"
#include "events.h"
#define MAX_IPC_BUFFER_SIZE (8 * 1024)
#define MAX_IPC_CLIENTS 8
typedef struct Wallkan Wallkan;

typedef struct WallkanIpc {
    int server_fd;
    char socket_path[108];
    char client_msg_buf[MAX_IPC_CLIENTS][MAX_IPC_BUFFER_SIZE];
    uint32_t client_msg_size[MAX_IPC_CLIENTS];
    WallkanEventHandler *wk_ev_handler;
    int client_fd[MAX_IPC_CLIENTS];
    uint8_t active_client_bits;
    uint8_t reply_is_due_bits;
    yyjson_alc json_alc;
} WallkanIpc;

typedef enum ResponseErrorCode {
    WK_IPC_RESPONSE_ERR_INTERNAL,
    WK_IPC_RESPONSE_ERR_INVALID_DATA,
   WK_IPC_RESPONSE_ERR_UNKNOWN_COMMAND,
} ResponseErrorCode;

typedef enum WkIPCStatusCode {
    WK_IPC_RESPONSE_STATUS_OK,
    WK_IPC_RESPONSE_STATUS_ERR
} WkIPCStatusCode;

typedef struct WkIPCErrResponse {
    char *code;
    char *message;
} WkIPCErrResponse;

typedef struct WkIPCResponse {
    union {
        struct {
            yyjson_mut_val *object;
            yyjson_mut_val *root;
            yyjson_mut_doc *doc;
        } result;
        WkIPCErrResponse err_response;
    };
    WkIPCStatusCode status_code;
} WkIPCResponse;

WkResult
wk_ipc_init(ArenaAllocator *alloc, WallkanIpc *wk_ipc, WallkanEventHandler *wk_ev_handler);

WkResult
wk_ipc_accept_connection(Wallkan *wk, uint32_t *out_client_idx);

WkResult
wk_ipc_read_client(ArenaAllocator *alloc, Wallkan *wk, uint32_t client_idx);

bool
wk_ipc_reply_pending(WallkanIpc *wk_ipc, uint32_t client_idx);

WkResult
wk_ipc_reply(WallkanIpc *wk_ipc, uint32_t client_idx, const WkIPCResponse *response);

void
wk_ipc_disconnect_client(WallkanIpc *wk_ipc, uint32_t client_idx);

void
wk_ipc_cleanup(WallkanIpc *wk_ipc);

#endif
