#ifndef WALLKAN_RENDERER_H
#define WALLKAN_RENDERER_H
#include "err.h"
#include "renderer/scene/scene.h"
#include "window/window.h"
#include "renderer/instance.h"
#include "renderer/device.h"
#include <stdint.h>
#include <vulkan/vulkan_core.h>

typedef struct WallkanRenderer {
    WallkanInstance  wk_instance;
    WallkanDevice    wk_device;

    WallkanScene *wk_scenes;

    uint32_t scene_count;
} WallkanRenderer;

WkResult
wk_renderer_init(ArenaAllocator *alloc, WallkanRenderer *wk_renderer, WallkanWindow *wk_window);

WkResult
wk_renderer_render(WallkanRenderer *wk_renderer);

void
wk_renderer_cleanup(WallkanRenderer *wk_renderer);

#endif
