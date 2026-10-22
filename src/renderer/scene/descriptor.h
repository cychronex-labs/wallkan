#ifndef WALLKAN_RENDERER_SCENE_DESCRIPTOR_H
#define WALLKAN_RENDERER_SCENE_DESCRIPTOR_H

#include <vulkan/vulkan.h>

#include "arena_alloc.h"
#include "err.h"
#include "renderer/device.h"
#include "renderer/scene/ubo.h"
#include "wksp/parser.h"

typedef struct WallkanPipeline WallkanPipeline;
typedef struct WallkanScene WallkanScene;

WkResult
descriptor_pool_init(WallkanPipeline *wk_pipeline, const WallkanDevice *wk_device, const WKSPContainer *wksp);

WkResult
descriptor_set_layouts_init(ArenaAllocator *alloc, WallkanPipeline *wk_pipeline,
    const WallkanDevice *wk_device, const WKSPContainer *wksp);

WkResult
descriptor_sets_alloc(WallkanPipeline *wk_pipeline,
    const WallkanDevice *wk_device, const WKSPContainer *wksp);

void
descriptor_sets_common_write(const WallkanScene *wk_scene,
    const WallkanDevice *wk_device);

void
descriptor_sets_passes_write(ArenaAllocator *alloc, WallkanPipeline *wk_pipeline,
    const WallkanDevice *wk_device, const WKSPContainer *wksp, const WkUBO *pass_ubo);

void
descriptor_set_layouts_cleanup(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline);

void
descriptor_pool_cleanup(WallkanPipeline *wk_pipeline, const WallkanDevice *wk_device);

#endif
