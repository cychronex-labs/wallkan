#ifndef WALLKAN_RENDERER_SCENE_H
#define WALLKAN_RENDERER_SCENE_H
#include "err.h"
#include "renderer/device.h"
#include "renderer/scene/descriptor.h"
#include "renderer/scene/pipeline.h"
#include "wksp/parser.h"
#include <vulkan/vulkan_core.h>

typedef struct WkSceneRenderTarget {
    VkImage image;
    VkImageView image_view;
    VkExtent2D image_extent;
} WkSceneRenderTarget;

typedef struct WallkanScene {
    WallkanPipeline pipeline;
    WkSceneRenderTarget render_target;
} WallkanScene;

WkResult
wk_scene_init(ArenaAllocator *alloc, const WallkanDevice *wk_device,
    const WKSPContainer *wksp, WallkanScene *wk_scene, uint32_t *scene_counter);

WkResult
wk_scene_set_render_target(WallkanScene *wk_scene, const WkSceneRenderTarget *render_target);

WkResult
wk_scene_render(WallkanScene *wk_scene);

void
wk_scene_cleanup(const WallkanDevice *wk_device, WallkanScene *wk_scene,
    uint32_t *scene_counter);


#endif
