#include "renderer/scene/scene.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/timerfd.h>
#include <unistd.h>

#include <vulkan/vulkan.h>

#include "arena_alloc.h"
#include "common.h"
#include "err.h"
#include "renderer/device.h"
#include "renderer/scene/pipeline.h"
#include "renderer/scene/texture.h"
#include "renderer/scene/ubo.h"
#include "wksp/parser.h"

#define NS_PER_SEC 1000000000ULL
static WkResult
wk_scene_load_shader_params(WallkanScene *wk_scene, const WKSPParameter *params,
    uint32_t count, uint32_t total_size)
{
    wk_scene->ubo.total_parameters_size = total_size;
    wk_scene->ubo.parameters_count = count;

    if (!wk_scene->ubo.parameters_data) {
        wk_scene->ubo.parameters_data = malloc(total_size);
    }

    uint8_t *params_ptr = wk_scene->ubo.parameters_data;
    for (uint32_t i=0; i<count; i++) {
        const WKSPParameter *param = &params[i];
        switch (param->type) {
            case WKSP_PARAM_TYPE_BOOL:
                for (uint32_t j=0; j<param->components_count; j++){
                    memcpy(params_ptr, &param->bool_components[j], 4);
                    params_ptr += 4;
                }
                break;
            case WKSP_PARAM_TYPE_INT:
                for (uint32_t j=0; j<param->components_count; j++){
                    memcpy(params_ptr, &param->int32_components[j], 4);
                    params_ptr += 4;
                }
                break;
            case WKSP_PARAM_TYPE_UINT:
                for (uint32_t j=0; j<param->components_count; j++){
                    memcpy(params_ptr, &param->uint32_components[j], 4);
                    params_ptr += 4;
                }
                break;
            case WKSP_PARAM_TYPE_FLOAT:
                for (uint32_t j=0; j<param->components_count; j++){
                    memcpy(params_ptr, &param->float32_components[j], 4);
                    params_ptr += 4;
                }
                break;
            default:
                return WK_ERR(WK_ERR_INVALID_WKSP_FILE, "Invalid parameter type!");
        }
    }
    return WK_OK;
}

WkResult
wk_scene_init(ArenaAllocator *alloc, const WallkanDevice *wk_device, WallkanScene *wk_scene,
    WkSceneConfig *wk_scene_config)
{
    wk_scene->timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if(wk_scene->timerfd < 0){
        return WK_ERR(WK_ERR_TIMERFD_CREATE_FAILED, "Failed to create timerfd for scene!");
    }

    uint64_t interval_ns = NS_PER_SEC / wk_scene_config->fps;
    struct timespec fps_timespec = {
        .tv_sec  = (time_t)(interval_ns / NS_PER_SEC),
        .tv_nsec = (long)(interval_ns % NS_PER_SEC),
    };

    struct itimerspec timerspec = {
        .it_interval = fps_timespec,
        .it_value = fps_timespec
    };
    if(timerfd_settime(wk_scene->timerfd, 0, &timerspec, NULL) < 0){
        return WK_ERR(WK_ERR_TIMERFD_SET_FAILED, "Failed to initialize timerfd for scene!");
    }
    wk_scene->output.format = wk_scene_config->texture_config.format;
    wk_scene->output.extent = wk_scene_config->texture_config.extent;

    WK_TRY(wk_scene_load_shader_params(wk_scene, wk_scene_config->wksp->parameters,
        wk_scene_config->wksp->total_parameters, wk_scene_config->wksp->total_parameters_size));

    WK_TRY(wk_ubo_init_all(wk_scene, wk_device, wk_scene_config->wksp));

    WK_TRY(wk_pipeline_init(alloc, wk_device, &wk_scene->pipeline, wk_scene_config->wksp,
        wk_scene, wk_scene->output.extent));

    WK_TRY(wk_texture_create(wk_device, wk_scene->output.format,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, (VkExtent3D){
        .width = wk_scene->output.extent.width,
        .height = wk_scene->output.extent.height,
        .depth = 1
    }, &wk_scene->output.texture));

    return WK_OK;
}

WkResult
wk_scene_resize(const WallkanDevice *wk_device, WallkanScene *wk_scene, VkExtent2D extent)
{
    WK_TRY(wk_pipeline_resize_textures(wk_device, &wk_scene->pipeline, extent));
    wk_texture_destroy(wk_device, &wk_scene->output.texture);
    WK_TRY(wk_texture_create(wk_device, wk_scene->output.format,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, (VkExtent3D){
        .width = extent.width,
        .height = extent.height,
        .depth = 1
    }, &wk_scene->output.texture));

    return WK_OK;
}

WkResult
wk_scene_render(WallkanScene *wk_scene)
{
    (void)wk_scene;
    return WK_OK;
}

void
wk_scene_cleanup(const WallkanDevice *wk_device, WallkanScene *wk_scene)
{
    if(wk_scene->timerfd != -1){
        close(wk_scene->timerfd);
        wk_scene->timerfd = -1;
    }
    if (wk_scene->ubo.parameters_data) {
        free(wk_scene->ubo.parameters_data);
        wk_scene->ubo.parameters_data = NULL;
    }
    wk_texture_destroy(wk_device, &wk_scene->output.texture);
    wk_pipeline_cleanup(wk_device, &wk_scene->pipeline);
    wk_ubo_cleanup_all(wk_scene, wk_device);

    wk_scene->ubo.frame_data = (WkFrameData){0};
    wk_scene->ubo.pass_data = (WkPassData){0};
    wk_scene->ubo.pass = NULL;

    for (uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        wk_scene->ubo.frame[i] = NULL;
        wk_scene->ubo.shader_params[i] = NULL;
    }

}
