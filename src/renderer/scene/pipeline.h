#ifndef WALLKAN_RENDERER_SCENE_PIPELINE
#define WALLKAN_RENDERER_SCENE_PIPELINE
#include <math.h>
#include "common.h"
#include "err.h"
#include "renderer/device.h"
#include "renderer/scene/descriptor.h"
#include "renderer/scene/ubo.h"
#include "renderer/scene/texture.h"
#include <stdint.h>
#include <vulkan/vulkan_core.h>

#define PINGPONG_IMG_COUNT 2

typedef struct WkPassChannel {
    WkChannelType type;
    uint32_t producer_idx;
    VkSampler sampler;
} WkPassChannel;

typedef struct WkPipelinePass {
    float scale;
    VkFormat image_format;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout input_set_layout;
    VkShaderStageFlagBits shader_stage;
    WkPassChannel *channels;
    uint32_t channel_count;
    bool self_feedback;
} WkPipelinePass;

typedef struct WkPassFrames {
    WkTexture textures[PINGPONG_IMG_COUNT];
    struct {
        // In each frame, an external self feedback pass changes its textures so
        // we need to take it into account. So we need 4 descriptor sets
        // One to use on frame % 2 and 1 - frame % 2
        VkDescriptorSet inputs[PINGPONG_IMG_COUNT][PINGPONG_IMG_COUNT];
        // Used only by compute shader pass
        VkDescriptorSet img_output[PINGPONG_IMG_COUNT];
    } desc_set;
    uint32_t iterations;
} WkPassFrames;

typedef struct WallkanPipeline{
    VkDescriptorPool descriptor_pool;
    struct {
        VkDescriptorSetLayout frame_layout;
        VkDescriptorSetLayout params_layout;
        VkDescriptorSetLayout compute_storage_layout;
        // These are shared across all pipelines therefore
        // In an iteration of mainloop there are two Shader passes reading it at
        // different frames (In the GPU)
        // If there is only single buffer and descriptor set CPU might copy
        // And one of the frames might get corrupted data
        VkDescriptorSet frame[MAX_FRAMES_IN_FLIGHT];
        VkDescriptorSet params[MAX_FRAMES_IN_FLIGHT];
    } desc_set;
    // Mostly initialization phase data
    WkPipelinePass *passes_data;
    // Separate the runtime data for maximum cache friendliness
    WkPassFrames *passes_frames;
    uint32_t pass_count;
} WallkanPipeline;

static inline uint32_t
scaled_res_component(uint32_t final_x, float scale)
{
    if(scale == 1.0f) return final_x;
    return (uint32_t)roundf(final_x * scale);
}

WkResult
wk_pipeline_init(ArenaAllocator *alloc, const WallkanDevice *wk_device,
    WallkanPipeline *wk_pipeline, const WKSPContainer *wksp, const WallkanScene *wk_scene,
    VkExtent2D final_resolution);

WkResult
wk_pipeline_resize_textures(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline,
    VkExtent2D final_resolution);

void
wk_pipeline_cleanup(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline);

#endif
