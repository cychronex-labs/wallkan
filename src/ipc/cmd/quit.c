#include "ipc/cmd/quit.h"
#include "ipc/server.h"
#include "wallkan.h"

WkResult
ipc_cmd_quit(Wallkan *wk, uint32_t client_idx)
{
    wk_ipc_reply(&wk->ipc, client_idx, &(WkIPCResponse){
        .status_code = WK_IPC_RESPONSE_STATUS_OK,
    });
    wallkan_stop(wk);
    return WK_OK;
}
