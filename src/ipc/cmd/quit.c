#include "ipc/cmd/quit.h"
#include "wallkan.h"

WkResult
ipc_cmd_quit(Wallkan *wk, uint32_t client_idx)
{
    wk_ipc_reply(&wk->ipc, client_idx, &(WkIPCReply){
        .reply_code = WK_IPC_REPLY_OK,
        .message = "Successfully closed daemon"
    });
    wallkan_stop(wk);
    return WK_OK;
}
