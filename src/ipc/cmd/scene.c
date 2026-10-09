#include "ipc/cmd/scene.h"
#include "arena_alloc.h"
#include "err.h"
#include "subprojects/yyjson/yyjson.h"
#include "wallkan.h"
#include <yyjson.h>

static WkResult
ipc_cmd_scene_load_wksp(ArenaAllocator *alloc, Wallkan *wk, uint32_t client_idx, yyjson_val *cmd_root)
{
    WkResult wkres = WK_OK;

    yyjson_mut_doc *resp_doc = yyjson_mut_doc_new(&wk->ipc.json_alc);
    yyjson_mut_val *resp_root = yyjson_mut_obj(resp_doc);
    yyjson_mut_doc_set_root(resp_doc, resp_root);

    char *err_code = arena_alloc(alloc, 32);
    char *err_msg = arena_alloc(alloc, 128);

    const char *path = yyjson_get_str(yyjson_obj_get(cmd_root, "path"));
    if(!path){
        err_code = "invalid_data";
        err_msg = "'path' field is required and must be absolute!";
        goto err;
    }
    wkres = wallkan_load_wksp(wk, path);
    if(wkres != WK_OK){
        strcpy(err_code, "internal_error");
        sprintf(err_msg, "Failed with code: %d", wkres);
        goto err;
    }
    wk_ipc_reply(&wk->ipc, client_idx, &(WkIPCResponse){
        .status_code = WK_IPC_RESPONSE_STATUS_OK
    });
    return WK_OK;
    err:
        wk_ipc_reply(&wk->ipc, client_idx, &(WkIPCResponse){
            .status_code = WK_IPC_RESPONSE_STATUS_ERR,
            .err_response = (WkIPCErrResponse){
                .code = err_code,
                .message = err_msg
            },
        });
        return WK_OK;
}


WkResult
ipc_cmd_scene_handle(ArenaAllocator *alloc, Wallkan *wk, uint32_t client_idx, const char *action,
    yyjson_val *cmd_root)
{
    char *err_code = arena_alloc(alloc, 32);
    char *err_msg = arena_alloc(alloc, 128);

    if(strcmp(action, "load_wksp") == 0){
        ipc_cmd_scene_load_wksp(&wk->arena_alloc, wk, client_idx, cmd_root);
    }else{
        err_code = "invalid_data";
        err_msg = "Unknown command action for domain output!";
        goto err;
    }
    return WK_OK;
err:
    wk_ipc_reply(&wk->ipc, client_idx, &(WkIPCResponse){
        .status_code = WK_IPC_RESPONSE_STATUS_ERR,
        .err_response = (WkIPCErrResponse){
            .code = err_code,
            .message = err_msg
        },
    });
    return WK_OK;
}
