#ifndef WALLKAN_RENDERER_H
#define WALLKAN_RENDERER_H
#include "err.h"
#include "renderer/scene/scene.h"
#include "surface/surface.h"
#include "window/window.h"
#include "renderer/instance.h"
#include "renderer/device.h"
#include <stdint.h>
#include <vulkan/vulkan_core.h>

#define MAX_SCENES 64

typedef struct WallkanRenderer {
    WallkanInstance  wk_instance;
    WallkanDevice    wk_device;
    WallkanScene *wk_scenes;
    uint64_t active_scenes;
} WallkanRenderer;

WkResult
wk_renderer_init(ArenaAllocator *alloc, WallkanRenderer *wk_renderer, WallkanWindow *wk_window);

WkResult
wk_renderer_new_scene(ArenaAllocator *alloc, WallkanRenderer *wk_renderer,
    WkSceneConfig *wk_scene_config, uint32_t *out_scene_idx);

WkResult
wk_renderer_render(WallkanRenderer *wk_renderer, uint64_t scenes_render_mask);

void
wk_renderer_remove_scene(WallkanRenderer *wk_renderer, uint32_t scene_idx);

void
wk_renderer_cleanup(WallkanRenderer *wk_renderer);

#endif
