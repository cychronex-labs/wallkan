#ifndef WALLKAN_RENDERER_SCENE_PIPELINE
#define WALLKAN_RENDERER_SCENE_PIPELINE
#include "common.h"
#include "err.h"
#include "renderer/device.h"
#include "renderer/scene/descriptor.h"
#include <stdint.h>
#include <vulkan/vulkan_core.h>

typedef struct WkPipelinePass {
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout input_set_layout;
    VkShaderStageFlagBits shader_stage;
    VkImage images[MAX_FRAMES_IN_FLIGHT];
    VkImageView image_views[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSet descriptor_sets[MAX_FRAMES_IN_FLIGHT];
    struct {
        VkDescriptorSetLayout out_img_set_layout;
    } compute;
} WkPipelinePass;

typedef struct WallkanPipeline{
    VkDescriptorPool descriptor_pool;
    WkGlobalDescriptorLayouts global_desc_layouts;
    WkPipelinePass *passes;
    uint32_t pass_count;
} WallkanPipeline;


WkResult
wk_pipeline_init(ArenaAllocator *alloc, const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline,
    const WKSPContainer *wksp);

void
wk_pipeline_cleanup(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline);

#endif
