#include "ipc/cmd/output.h"
#include "arena_alloc.h"
#include "err.h"
#include "events.h"
#include "ipc/server.h"
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
ipc_cmd_output_handle(ArenaAllocator *alloc, Wallkan *wk, uint32_t client_idx, const char *action,
    yyjson_val *cmd_root)
{
    char *err_code = arena_alloc(alloc, 32);
    char *err_msg = arena_alloc(alloc, 128);

    if(strcmp(action, "list") == 0){
        ipc_cmd_output_list(wk, client_idx);
    }else if (strcmp(action, "enable") == 0) {
        ipc_cmd_output_enable(&wk->arena_alloc, wk, cmd_root, client_idx);
    }else if (strcmp(action, "disable") == 0) {
        ipc_cmd_output_disable(&wk->arena_alloc, wk, cmd_root, client_idx);
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

WkResult
ipc_cmd_output_list(Wallkan *wk, uint32_t client_idx)
{
    // Setup yyjson
    yyjson_mut_doc *resp_doc = yyjson_mut_doc_new(&wk->ipc.json_alc);
    yyjson_mut_val *resp_root = yyjson_mut_obj(resp_doc);
    yyjson_mut_doc_set_root(resp_doc, resp_root);

    yyjson_mut_val *result_obj = yyjson_mut_obj(resp_doc);

    yyjson_mut_val *outputs_arr_obj = yyjson_mut_obj_add_arr(resp_doc, result_obj, "outputs");
    for (uint32_t i=0; i<MAX_OUTPUTS; i++) {
        yyjson_mut_val *output_obj = create_output_obj(wk, resp_doc, i);
        if(output_obj) yyjson_mut_arr_append(outputs_arr_obj, output_obj);
    }

    wk_ipc_reply(&wk->ipc, client_idx, &(WkIPCResponse){
        .status_code = WK_IPC_RESPONSE_STATUS_OK,
        .result.doc = resp_doc,
        .result.root = resp_root,
        .result.object = result_obj,
    });

    return WK_OK;
}


static uint32_t
search_output(WallkanWindow *wk_window, const char *output_name)
{
    for (uint32_t i = 0; i < MAX_OUTPUTS; i++) {
        WallkanOutput *output = &wk_window->wk_outputs[i];
        if(!output->got_details) continue;
        if(strcmp(output_name, output->name) == 0){
            return i;
        }
    }
    return UINT32_MAX;
}

WkResult
ipc_cmd_output_enable(ArenaAllocator *alloc, Wallkan *wk, yyjson_val *cmd_root, uint32_t client_idx)
{
    // Why still receive alloc when Wallkan have it?
    // To mark that this function uses ArenaAllocator.
    // So it could be helpful for debugging because some bugs maybe because of trying to read
    // previous frame's arena allocated memory.
    (void)alloc;
    // Setup yyjson
    WkResult wkres = WK_OK;
    WkIPCResponse response;
    yyjson_mut_doc *resp_doc = yyjson_mut_doc_new(&wk->ipc.json_alc);
    yyjson_mut_val *resp_root = yyjson_mut_obj(resp_doc);
    char *err_code = arena_alloc(alloc, 32);
    char *err_msg = arena_alloc(alloc, 128);

    yyjson_mut_doc_set_root(resp_doc, resp_root);

    yyjson_val *target_obj = yyjson_obj_get(cmd_root, "target");
    if(!target_obj){
        strcpy(err_code, "invalid_data");
        err_msg = "required key 'target' not found!";
        goto err;
    }

    uint32_t output_idx = UINT32_MAX;
    if(yyjson_is_str(target_obj)){
        const char *output_name = yyjson_get_str(target_obj);

        output_idx = search_output(&wk->window, output_name);

        if(output_idx == UINT32_MAX){
            strcpy(err_code, "invalid_data");
            sprintf(err_msg, "Output name does not exist!");
            goto err;
        }
    }else if(yyjson_is_uint(target_obj)){
        output_idx = yyjson_get_uint(target_obj);
        if (output_idx >= MAX_OUTPUTS) {
            strcpy(err_code, "invalid_data");
            sprintf(err_msg, "Index out of bounds. total output slots: %d", MAX_OUTPUTS);
            goto err;
        }
    }else{
        strcpy(err_code, "invalid_data");
        sprintf(err_msg, "'target' field must either be an output name or its index");
        goto err;
    }
    WallkanOutput *wk_output = &wk->window.wk_outputs[output_idx];
    // An inactive output exist in Two conditions
    // - Wayland never found one
    // - Wallkan disabled it so nothing is rendered
    // So before enabling an output we need to make sure wayland have such a output
    // AND We have got required details
    if(!wk_output->got_details){
        strcpy(err_code, "invalid_data");
        strcpy(err_msg, "Wayland Output/Monitor does not exist!");
        goto err;
    }
    if(!wk_output_is_active(wk_output)){
        wkres = wallkan_enable_output(wk, &wk->window.wk_outputs[output_idx]);
        if(wkres != WK_OK){
            strcpy(err_code, "internal_error");
            sprintf(err_msg, "Failed to enable output, error code: %d", wkres);
            goto err;
        }
    }

    response = (WkIPCResponse){
        .status_code = WK_IPC_RESPONSE_STATUS_OK,
        .result.doc = resp_doc,
        .result.root = resp_root,
        .result.object = create_output_obj(wk, resp_doc, output_idx)
    };
    WK_TRY(wk_ipc_reply(&wk->ipc, client_idx, &response));
    return WK_OK;
err:
    response = (WkIPCResponse){
        .status_code = WK_IPC_RESPONSE_STATUS_ERR,
        .err_response = (WkIPCErrResponse){
            .code = err_code,
            .message = err_msg
        }
    };
    wk_ipc_reply(&wk->ipc, client_idx, &response);
    return WK_OK;
}

WkResult
ipc_cmd_output_disable(ArenaAllocator *alloc, Wallkan *wk, yyjson_val *cmd_root, uint32_t client_idx)
{
    // Why still receive alloc when Wallkan have it?
    // To mark that this function uses ArenaAllocator.
    // So it could be helpful for debugging because some bugs maybe because of trying to read
    // previous frame's arena allocated memory.
    (void)alloc;
    // Setup yyjson
    WkResult wkres = WK_OK;
    WkIPCResponse response;
    yyjson_mut_doc *resp_doc = yyjson_mut_doc_new(&wk->ipc.json_alc);
    yyjson_mut_val *resp_root = yyjson_mut_obj(resp_doc);
    char *err_code = arena_alloc(alloc, 32);
    char *err_msg = arena_alloc(alloc, 128);

    yyjson_mut_doc_set_root(resp_doc, resp_root);

    yyjson_val *target_obj = yyjson_obj_get(cmd_root, "target");
    if(!target_obj){
        strcpy(err_code, "invalid_data");
        err_msg = "required key 'target' not found!";
        goto err;
    }

    uint32_t output_idx = UINT32_MAX;
    if(yyjson_is_str(target_obj)){
        const char *output_name = yyjson_get_str(target_obj);
        output_idx = search_output(&wk->window, output_name);
        if(output_idx == UINT32_MAX){
            strcpy(err_code, "invalid_data");
            strcpy(err_msg, "Output name does not exist!");
            goto err;
        }
    }else if(yyjson_is_uint(target_obj)){
        output_idx = yyjson_get_uint(target_obj);
        if (output_idx >= MAX_OUTPUTS) {
            strcpy(err_code, "invalid_data");
            sprintf(err_msg, "Index out of bounds. total output slots: %d", MAX_OUTPUTS);
            goto err;
        }
    }else{
        strcpy(err_code, "invalid_data");
        strcpy(err_msg, "'target' field must either be an output name or its index");
        goto err;
    }

    WallkanOutput *wk_output = &wk->window.wk_outputs[output_idx];
    if(!wk_output->got_details){
        strcpy(err_code, "invalid_data");
        strcpy(err_msg, "Wayland Output/Monitor does not exist!");
        goto err;
    }
    if(wk_output_is_active(wk_output)){
        wkres = wallkan_disable_output(wk, wk_output);
        if(wkres != WK_OK){
            strcpy(err_code, "internal_error");
            sprintf(err_msg, "Failed to disable output, error code: %d", wkres);
            goto err;
        }
    }

    response = (WkIPCResponse){
        .status_code = WK_IPC_RESPONSE_STATUS_OK,
        .result.doc = resp_doc,
        .result.root = resp_root,
        .result.object = create_output_obj(wk, resp_doc, output_idx)
    };
    WK_TRY(wk_ipc_reply(&wk->ipc, client_idx, &response));
    return WK_OK;
err:
    response = (WkIPCResponse){
        .status_code = WK_IPC_RESPONSE_STATUS_ERR,
        .err_response = (WkIPCErrResponse){
            .code = err_code,
            .message = err_msg
        }
    };
    wk_ipc_reply(&wk->ipc, client_idx, &response);
    return WK_OK;
}
