#include "ipc_cmd/output.h"
#include "arena_alloc.h"
#include "err.h"
#include "events.h"
#include "ipc.h"
#include "subprojects/yyjson/yyjson.h"
#include "wallkan.h"
#include "window/outputs.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static yyjson_mut_val *create_output_obj(const Wallkan *wk, yyjson_mut_doc *resp_doc,
    uint32_t output_idx)
{
    const WallkanOutput *wk_output = &wk->window.wk_outputs[output_idx];
    if(!wk_output->got_details) return NULL;
    yyjson_mut_val *output_obj = yyjson_mut_obj(resp_doc);
    yyjson_mut_obj_add_str(resp_doc, output_obj, "name", wk_output->name);
    yyjson_mut_obj_add_int(resp_doc, output_obj, "index", output_idx);
    yyjson_mut_obj_add_int(resp_doc, output_obj, "wl_registry_name", wk_output->registry_id);
    yyjson_mut_obj_add_bool(resp_doc, output_obj, "is_active",
        wk->window.output_is_active_mask & (1 << output_idx));
    yyjson_mut_obj_add_int(resp_doc, output_obj, "width", wk_output->width);
    yyjson_mut_obj_add_int(resp_doc, output_obj, "height", wk_output->height);
    yyjson_mut_obj_add_int(resp_doc, output_obj, "scale", wk_output->scale);
    yyjson_mut_obj_add_int(resp_doc, output_obj, "frame_time_ms", wk_output->frame_time_ms);
    return output_obj;
}

WkResult
ipc_cmd_output_list(Wallkan *wk)
{
    // Setup yyjson
    yyjson_mut_doc *resp_doc = yyjson_mut_doc_new(&wk->ipc.cmd_processor.json_reply_alc);
    yyjson_mut_val *resp_root = yyjson_mut_obj(resp_doc);
    yyjson_mut_doc_set_root(resp_doc, resp_root);


    yyjson_mut_obj_add_str(resp_doc, resp_root, "status", "ok");
    yyjson_mut_val *outputs_arr_obj = yyjson_mut_obj_add_arr(resp_doc, resp_root, "response");
    for (uint32_t i=0; i<MAX_OUTPUTS; i++) {
        yyjson_mut_val *output_obj = create_output_obj(wk, resp_doc, i);
        if(output_obj) yyjson_mut_arr_append(outputs_arr_obj, output_obj);
    }
    wk_ipc_reply(&wk->ipc, &(WkIPCReply){
        .reply_code = WK_IPC_REPLY_OK,
        .response_doc = resp_doc
    });
    return WK_OK;
}

WkResult
ipc_cmd_output_enable(ArenaAllocator *alloc, Wallkan *wk, yyjson_doc *cmd_doc)
{
    // Why still receive alloc when Wallkan have it?
    // To mark that this function uses ArenaAllocator.
    (void)alloc;
    // Setup yyjson
    WkResult wkres = WK_OK;
    yyjson_mut_doc *resp_doc = yyjson_mut_doc_new(&wk->ipc.cmd_processor.json_reply_alc);
    yyjson_mut_val *resp_root = yyjson_mut_obj(resp_doc);
    WkIPCReply reply = {0};
    char *msg = "";
    yyjson_mut_doc_set_root(resp_doc, resp_root);

    yyjson_val *cmd_root = yyjson_doc_get_root(cmd_doc);
    yyjson_val *output_idx_obj = yyjson_obj_get(cmd_root, "index");
    if(!output_idx_obj){
        msg = "required key 'index' not found!";
        goto invalid_cmd;
    }
    uint32_t output_idx = yyjson_get_uint(output_idx_obj);
    if (output_idx >= MAX_OUTPUTS) {
        msg = "Index out of range";
        goto invalid_cmd;
    }
    if(wk_output_is_active(&wk->window.wk_outputs[output_idx])) goto already_active;

    yyjson_mut_obj_add_str(resp_doc, resp_root, "status", "ok");

    wkres = wallkan_enable_output(wk, &wk->window.wk_outputs[output_idx]);
    if(wkres != WK_OK) goto err;

    reply = (WkIPCReply){
        .reply_code = WK_IPC_REPLY_OK,
        .message = "Successfully enabled output!"
    };
    WK_TRY(wk_ipc_reply(&wk->ipc, &reply));
    return WK_OK;
invalid_cmd:
    yyjson_mut_obj_add_str(resp_doc, resp_root, "status", "error");
    yyjson_mut_obj_add_str(resp_doc, resp_root, "message", msg);
    reply = (WkIPCReply){
        .reply_code = WK_IPC_REPLY_INVALID_DATA,
    };
    strncpy(reply.message, msg, sizeof(reply.message));
    wk_ipc_reply(&wk->ipc, &reply);
    return WK_OK;
already_active:
    reply = (WkIPCReply){
        .reply_code = WK_IPC_REPLY_OK,
        .message =  "Output is already active!"
    };
    wk_ipc_reply(&wk->ipc, &reply);
    return WK_OK;
err:
    reply = (WkIPCReply){
        .reply_code = WK_IPC_REPLY_INTERNAL_ERROR,
    };
    snprintf(reply.message, sizeof(reply.message), "Failed to enable output %u, error: %d!",
        output_idx, wkres);
    WK_TRY(wk_ipc_reply(&wk->ipc, &reply));
    return WK_OK;
}

WkResult
ipc_cmd_output_disable(Wallkan *wk, yyjson_doc *cmd_doc)
{
    WkResult wkres = WK_OK;
    yyjson_mut_doc *resp_doc = yyjson_mut_doc_new(&wk->ipc.cmd_processor.json_reply_alc);
    yyjson_mut_val *resp_root = yyjson_mut_obj(resp_doc);
    WkIPCReply reply = {0};
    yyjson_val *cmd_root = yyjson_doc_get_root(cmd_doc);
    yyjson_val *output_idx_obj = yyjson_obj_get(cmd_root, "index");
    char *msg = "";
    if(!output_idx_obj){
        msg = "required key 'index' not found!";
        goto invalid_cmd;
    }
    uint32_t output_idx = yyjson_get_uint(output_idx_obj);
    if (output_idx >= MAX_OUTPUTS) {
        msg = "Index out of range";
        goto invalid_cmd;
    }
    if(!wk_output_is_active(&wk->window.wk_outputs[output_idx])) goto already_inactive;
    yyjson_mut_obj_add_str(resp_doc, resp_root, "status", "ok");

    wkres = wallkan_disable_output(wk, &wk->window.wk_outputs[output_idx]);
    if(wkres != WK_OK) goto err;

    reply = (WkIPCReply){
        .reply_code = WK_IPC_REPLY_OK,
        .message = "Successfully disabled output"
    };
    WK_TRY(wk_ipc_reply(&wk->ipc, &reply));
    return WK_OK;
invalid_cmd:
    yyjson_mut_obj_add_str(resp_doc, resp_root, "status", "error");
    yyjson_mut_obj_add_str(resp_doc, resp_root, "message", msg);
    reply = (WkIPCReply){
        .reply_code = WK_IPC_REPLY_INVALID_DATA,
    };
    strncpy(reply.message, msg, sizeof(reply.message));
    wk_ipc_reply(&wk->ipc, &reply);
    return WK_OK;
already_inactive:
    reply = (WkIPCReply){
        .reply_code = WK_IPC_REPLY_OK,
        .message =  "Output is already disabled!"
    };
    wk_ipc_reply(&wk->ipc, &reply);
    return WK_OK;
err:
    reply = (WkIPCReply){
        .reply_code = WK_IPC_REPLY_INTERNAL_ERROR,
    };
    snprintf(reply.message, sizeof(reply.message), "Failed to disable output %u, error: %d!",
        output_idx, wkres);
    WK_TRY(wk_ipc_reply(&wk->ipc, &reply));
    return WK_OK;
}
