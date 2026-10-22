#include "wallkan.h"

#include <stdint.h>

#include <vulkan/vulkan.h>

#include "common.h"
#include "err.h"
#include "renderer/renderer.h"
#include "renderer/scene/scene.h"
#include "surface/surface.h"
#include "wksp/parser.h"

WkResult
wallkan_load_scene(Wallkan *wk, const char *wksp_path, uint32_t fps, VkExtent2D extent)
{
    uint32_t scene_idx = UINT32_MAX;
    WKSPContainer wksp = {0};
    WkResult wkres = WK_OK;
    uint32_t output_idx = UINT32_MAX;

    wkres = wksp_load(&wk->arena_alloc, &wk->renderer.wk_device, &wk->wksp_parser, wksp_path, &wksp);
    if(wkres != WK_OK) goto err;

    WkSceneConfig scene_config = {
        .wksp = &wksp,
        .texture_config = {
            .format = wksp.passes[wksp.total_passes-1].selected_img_format,
            .extent = extent,
        },
        .fps = fps
    };

    uint8_t active_mask = wk->window.output_is_active_mask;
    while (active_mask) {
        output_idx = bit_pop_lsb(&active_mask);
        scene_config.texture_config.extent = wk->surfaces[output_idx].wk_swapchain.image_extent;
        if (scene_config.fps == 0) {
            scene_config.fps = wk->window.wk_outputs[output_idx].refresh_rate;
        }
        wkres = wk_renderer_new_scene(&wk->arena_alloc, &wk->renderer, &scene_config, &scene_idx);
        if(wkres != WK_OK) goto err;
        wk->poll_fds[POLL_SCENE_START+scene_idx].fd = wk->renderer.wk_scenes[scene_idx].timerfd;
        wk->surfaces[output_idx].scene_idx = scene_idx;
    }
    wksp_destroy(&wk->renderer.wk_device, &wksp);
    return wkres;
err:
    if (scene_idx != UINT32_MAX && wkres != WK_ERR_MAX_AMOUNT_SCENES) {
        wk_renderer_remove_scene(&wk->renderer, scene_idx);
        wk->poll_fds[POLL_SCENE_START+scene_idx].fd = -1;
        if(output_idx != UINT32_MAX){
            wk->surfaces[output_idx].scene_idx = UINT32_MAX;
        }
    }
    wksp_destroy(&wk->renderer.wk_device, &wksp);
    return wkres;
}
