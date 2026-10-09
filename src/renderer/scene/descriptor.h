#ifndef WALLKAN_RENDERER_SCENE_DESCRIPTOR
#define WALLKAN_RENDERER_SCENE_DESCRIPTOR
#include "err.h"
#include "renderer/device.h"
#include "wksp/parser.h"
#include <vulkan/vulkan_core.h>

typedef struct WallkanPipeline WallkanPipeline;

typedef struct WkGlobalDescriptorLayouts {
    VkDescriptorSetLayout frame_ubo;
    VkDescriptorSetLayout shader_params_ubo;
} WkGlobalDescriptorLayouts;


WkResult
descriptor_pool_init(WallkanPipeline *wk_pipeline, const WallkanDevice *wk_device, const WKSPContainer *wksp);

WkResult
descriptor_set_layouts_init(ArenaAllocator *alloc, WallkanPipeline *wk_pipeline,
    const WallkanDevice *wk_device, const WKSPContainer *wksp);

void
descriptor_set_layouts_cleanup(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline);

void
descriptor_pool_cleanup(WallkanPipeline *wk_pipeline, const WallkanDevice *wk_device);

#endif
