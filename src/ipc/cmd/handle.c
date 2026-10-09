#include "arena_alloc.h"
#include "common.h"
#include "err.h"
#include "ipc/cmd/handle.h"
#include "ipc/cmd/output.h"
#include "ipc/cmd/quit.h"
#include "ipc/cmd/scene.h"
#include "ipc/server.h"
#include "subprojects/yyjson/yyjson.h"
#include "wallkan.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>


WkResult
ipc_cmd_handle(ArenaAllocator *alloc, yyjson_doc *doc, Wallkan *wk, uint32_t client_idx)
{
    wk->ipc.reply_is_due_bits |= (1 << client_idx);
    char *err_code = NULL;
    char *err_msg = NULL;

    if (!doc) {
        err_code = "invalid_data";
        err_msg = "Corrupted json!";
        goto err;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root) {
        err_code = "invalid_data";
        err_msg = "Corrupted json!";
        goto err;
    }
    yyjson_val *_cmd = yyjson_obj_get(root, "cmd");
    const char *cmd = yyjson_get_str(_cmd);
    if(!cmd){
        err_code = "invalid_data";
        err_msg = "Invalid json 'cmd' is a required field and must be a string!";
        goto err;
    }

    uint32_t total_cmd_len = yyjson_get_len(_cmd);
    const char *dot = memchr(cmd, '.', total_cmd_len);

    if(!dot){
        if(strcmp(cmd, "quit") == 0){
            ipc_cmd_quit(wk, client_idx);
        }else{
            err_code = "invalid_data";
            err_msg = "Unknown command!";
            goto err;
        }
    }else{
        ptrdiff_t domain_len = dot - cmd;
        const char *action = dot+1;

        if(strncmp(cmd, "output", domain_len) == 0){
            ipc_cmd_output_handle(alloc, wk, client_idx, action, root);
        }
        else if(strncmp(cmd, "scene", domain_len) == 0){
            ipc_cmd_scene_handle(alloc, wk, client_idx, action, root);
        }else{
            err_code = "invalid_data";
            err_msg = "Unknown command domain!";
            goto err;
        }

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
