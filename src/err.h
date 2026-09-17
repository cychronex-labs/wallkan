#ifndef WALLKAN_ERR_H
#define WALLKAN_ERR_H

#include <vulkan/vulkan_core.h>
typedef enum WkResult {
    WK_OK,
    WK_ERR_ALLOCATION_FAILURE,
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

    WK_ERR_VK_INSTANCE_EXT_ENUMERATION_FAILED,
    WK_ERR_VK_REQUIRED_EXTENSION_UNAVAILABLE,
    WK_ERR_VK_INSTANCE_LAYER_ENUMERATION_FAILED,
    WK_ERR_VK_INSTANCE_CREATION_FAILED,
    WK_ERR_VK_GET_INSTANCE_PROC_ADDR_FAILURE,
    WK_ERR_VK_CREATE_DEBUG_MESSENGER_FAILED,
    WK_ERR_VK_WL_SURFACE_CREATION_FAILURE
} WkResult;

__attribute__((format(printf, 5, 6)))
WkResult
wk_log_error(WkResult code, const char *code_name, const char *file, int line,
    const char *fmt,...);

__attribute__((format(printf, 6, 7)))
WkResult
vk_expect(VkResult vkres, WkResult code, const char *code_name, const char *file, int line, const char *fmt,...);

#define WK_ERR(code, fmt, ...) \
    wk_log_error(code, #code, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define WK_TRY(expr) \
    do {\
        WkResult _wkres = (expr);\
        if(_wkres != WK_OK) return _wkres;\
    } while(0)


#define EXPECT_VK(expr, code, fmt, ...) \
    vk_expect((expr), code, #code, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif
