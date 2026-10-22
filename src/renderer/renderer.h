#ifndef WALLKAN_RENDERER_RENDERER_H
#define WALLKAN_RENDERER_RENDERER_H

#include <stdint.h>

#include <vulkan/vulkan.h>

#include "arena_alloc.h"
#include "err.h"
#include "renderer/device.h"
#include "renderer/instance.h"
#include "renderer/scene/scene.h"

#define MAX_SCENES 64

typedef struct WallkanWindow WallkanWindow;
typedef struct WkSceneConfig WkSceneConfig;

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
