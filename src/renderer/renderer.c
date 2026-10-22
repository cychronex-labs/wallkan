#include "renderer/renderer.h"

#include <stdint.h>
#include <stdlib.h>

#include <vulkan/vulkan.h>

#include "arena_alloc.h"
#include "common.h"
#include "err.h"
#include "renderer/device.h"
#include "renderer/instance.h"
#include "renderer/scene/scene.h"
#include "window/window.h"

WkResult
wk_renderer_init(ArenaAllocator *alloc, WallkanRenderer *wk_renderer, WallkanWindow *wk_window)
{
    wk_renderer->wk_scenes = calloc(MAX_SCENES, sizeof(WallkanScene));
    if(!wk_renderer->wk_scenes) return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Allocation failure!");

    WK_TRY(wk_instance_init(alloc, &wk_renderer->wk_instance));
    WK_TRY(wk_device_init(alloc, &wk_renderer->wk_device, &wk_renderer->wk_instance,
        wk_window->display));

    return WK_OK;
}

WkResult
wk_renderer_new_scene(ArenaAllocator *alloc, WallkanRenderer *wk_renderer,
    WkSceneConfig *wk_scene_config, uint32_t *out_scene_idx)
{
    uint32_t slot_idx = bit_find_free_slot(wk_renderer->active_scenes);
    if (slot_idx == UINT32_MAX) {
        return WK_ERR(WK_ERR_MAX_AMOUNT_SCENES, "Maximum amount of scenes reached!");
    }
    *out_scene_idx = slot_idx;

    wk_renderer->active_scenes |= (1ULL << slot_idx);
    wk_renderer->wk_scenes[slot_idx] = (WallkanScene){ .timerfd = -1 };
    WK_TRY(wk_scene_init(alloc, &wk_renderer->wk_device, &wk_renderer->wk_scenes[slot_idx],
        wk_scene_config));
    return WK_OK;
}

void
wk_renderer_remove_scene(WallkanRenderer *wk_renderer, uint32_t scene_idx)
{
    if (wk_renderer->active_scenes & (1ULL << scene_idx)) {
        wk_renderer->active_scenes &= ~(1ULL << scene_idx);
        wk_scene_cleanup(&wk_renderer->wk_device, &wk_renderer->wk_scenes[scene_idx]);
    }
}

WkResult
wk_renderer_render(WallkanRenderer *wk_renderer, uint64_t scenes_render_mask)
{
    uint64_t render_mask = scenes_render_mask;
    while (render_mask) {
        uint32_t idx = bit_pop_lsb(&render_mask);
        wk_scene_render(&wk_renderer->wk_scenes[idx]);
    }
    return WK_OK;
}

void
wk_renderer_cleanup(WallkanRenderer *wk_renderer)
{
    if(wk_renderer->wk_scenes){
        uint64_t active_mask = wk_renderer->active_scenes;
        while (active_mask) {
            uint32_t idx = bit_pop_lsb(&active_mask);
            wk_renderer_remove_scene(wk_renderer, idx);
        }
        free(wk_renderer->wk_scenes);
        wk_renderer->wk_scenes = NULL;
    }
    wk_device_cleanup(&wk_renderer->wk_device);
    wk_instance_cleanup(&wk_renderer->wk_instance);
}
