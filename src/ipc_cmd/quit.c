#include "ipc_cmd/quit.h"
#include "wallkan.h"

WkResult
ipc_cmd_quit(Wallkan *wk)
{
    wk_ipc_reply(&wk->ipc, &(WkIPCReply){
        .reply_code = WK_IPC_REPLY_OK,
        .message = "Successfully closed daemon"
    });
    wallkan_stop(wk);
    return WK_OK;
}
