#ifndef WALLKAN_ERR_H
#define WALLKAN_ERR_H

typedef enum WkResult {
    WK_OK,
    WK_ERR_ALLOCATION_ERROR,
    WK_ERR_POLL_FAILURE,

    WK_ERR_EVENT_HANDLER_UNKNOWN_EVENT,
    WK_ERR_EVENT_HANDLER_MAX_SLOTS,
    WK_ERR_EVENT_HANDLER_CALLBACK_NULL,

    WK_ERR_IPC_SOCKET_CREATION_FAILED,
    WK_ERR_IPC_SOCKET_BIND_FAILED,
    WK_ERR_IPC_SOCKET_LISTEN_FAILED,
    WK_ERR_IPC_SOCKET_ACCEPT_FAILURE,
    WK_ERR_IPC_SOCKET_READ_FAILURE,
    WK_ERR_IPC_ALREADY_RUNNING,

    WK_ERR_WL_DISPLAY_CONNECT_FAILURE,
    WK_ERR_WL_NULL_REGISTRY,
    WK_ERR_WL_REGISTRY_ROUNDTRIP_FAILURE,
    WK_ERR_WL_GLOBAL_COMPOSITOR_UNDEFINED,
    WK_ERR_WL_GLOBAL_LAYER_SHELL_UNDEFINED,
    WK_ERR_WL_SURFACE_CREATION_FAILURE,
    WK_ERR_WL_ZWLR_LAYER_SURFACE_ROLE_FAILURE,
    WK_ERR_WL_DISPLAY_ROUNDTRIP_FAILURE,
    WK_ERR_WL_FRAME_CALLBACK_FAILED,
    WK_ERR_WL_COMPOSITOR_DISCONNECTED,
} WkResult;

__attribute__((format(printf, 5, 6)))
WkResult
wk_log_error(WkResult code, const char *code_name, const char *file, int line, const char *fmt,...);

#define WK_ERR(code, fmt, ...) \
    wk_log_error(code, #code, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define WK_TRY(expr) \
    do {\
        WkResult _res = (expr);\
        if(_res != WK_OK) return _res;\
    } while(0)

#endif
