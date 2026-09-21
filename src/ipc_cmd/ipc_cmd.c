#include "common.h"
#include "err.h"
#include "ipc_cmd/ipc_cmd.h"
#include "ipc_cmd.h"
#include "ipc_cmd/output.h"
#include "ipc_cmd/quit.h"
#include "subprojects/yyjson/yyjson.h"
#include "wallkan.h"

WkResult
ipc_cmd_handle(ArenaAllocator *alloc, yyjson_doc *doc, Wallkan *wk)
{
    if (!doc) {
        wk_ipc_reply(&wk->ipc, &(const WkIPCReply){
            .reply_code = WK_IPC_REPLY_INVALID_DATA,
            .message = "Invalid json format!",
        });
        return WK_OK;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    wk->ipc.reply_is_due = true;
    if (!yyjson_is_obj(root)) {
        wk_ipc_reply(&wk->ipc, &(const WkIPCReply){
            .reply_code = WK_IPC_REPLY_INVALID_DATA,
            .message = "Invalid json format!",
        });
        return WK_OK;
    }
    const char *cmd = yyjson_get_str(yyjson_obj_get(root, "cmd"));
    if(!cmd){
        wk_ipc_reply(&wk->ipc, &(const WkIPCReply){
            .reply_code = WK_IPC_REPLY_INVALID_DATA,
            .message = "Invalid json format, 'cmd' is missing!",
        });
        return WK_OK;
    }
    if(strcmp(cmd, "ping") == 0){
        wk_ipc_reply(&wk->ipc, &(const WkIPCReply){
            .reply_code = WK_IPC_REPLY_OK,
            .message = "Yeah. I am alive!",
        });
    }
    else if(strcmp(cmd, "quit") == 0){
        ipc_cmd_quit(wk);
    }
    else if(strcmp(cmd, "output_list") == 0){
        ipc_cmd_output_list(wk);
    }
    else if(strcmp(cmd, "output_enable") == 0){
        ipc_cmd_output_enable(alloc, wk, doc);
    }
    else if(strcmp(cmd, "output_disable") == 0){
        ipc_cmd_output_disable(wk, doc);
    }
    else{
        wk_ipc_reply(&wk->ipc, &(const WkIPCReply){
            .reply_code = WK_IPC_REPLY_UNKNOWN_COMMAND,
            .message = "Unknown command",
        });
    }

    return WK_OK;
}
