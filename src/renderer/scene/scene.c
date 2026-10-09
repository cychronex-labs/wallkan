#include "renderer/scene/scene.h"
#include "arena_alloc.h"
#include "renderer/scene/pipeline.h"
#include <stdint.h>
#include <vulkan/vulkan_core.h>

WkResult
wk_scene_init(ArenaAllocator *alloc, const WallkanDevice *wk_device,
    const WKSPContainer *wksp, WallkanScene *wk_scene, uint32_t *scene_counter)
{
    WK_TRY(wk_pipeline_init(alloc, wk_device, &wk_scene->pipeline, wksp));
    (*scene_counter)+=1;
    return WK_OK;
}

WkResult
wk_scene_resize(WallkanScene *wk_scene , const VkExtent2D extent)
{
    return WK_OK;
}

WkResult
wk_scene_set_render_target(WallkanScene *wk_scene, const WkSceneRenderTarget *render_target)
{

    return WK_OK;
}

void
wk_scene_cleanup(const WallkanDevice *wk_device, WallkanScene *wk_scene, uint32_t *scene_counter)
{
    wk_pipeline_cleanup(wk_device, &wk_scene->pipeline);
    (*scene_counter)-=1;
}
