#include "arena_alloc.h"
#include "err.h"
#include "renderer/device.h"
#include "renderer/instance.h"
#include "renderer/scene/scene.h"
#include "window/outputs.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>
#include "renderer/renderer.h"

WkResult
wk_renderer_init(ArenaAllocator *alloc, WallkanRenderer *wk_renderer, WallkanWindow *wk_window)
{
    (void)alloc;
    (void)wk_renderer;

    wk_renderer->wk_scenes = malloc(sizeof(WallkanScene) * MAX_OUTPUTS);
    if(!wk_renderer->wk_scenes) return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Allocation failure!");

    WK_TRY(wk_instance_init(alloc, &wk_renderer->wk_instance));
    WK_TRY(wk_device_init(alloc, &wk_renderer->wk_device, &wk_renderer->wk_instance,
        wk_window->display));

    return WK_OK;
}

WkResult
wk_renderer_render(WallkanRenderer *wk_renderer)
{
    (void)wk_renderer;
    return WK_OK;
}

void
wk_renderer_cleanup(WallkanRenderer *wk_renderer)
{
    if(wk_renderer->wk_scenes){
        uint32_t scene_count = wk_renderer->scene_count;
        for (uint32_t i=0; i<scene_count; i++) {
            wk_scene_cleanup(&wk_renderer->wk_device, &wk_renderer->wk_scenes[i],
                &wk_renderer->scene_count);
        }
        free(wk_renderer->wk_scenes);
        wk_renderer->wk_scenes = NULL;
    }
    wk_device_cleanup(&wk_renderer->wk_device);
    wk_instance_cleanup(&wk_renderer->wk_instance);
}
