#ifndef WALLKAN_RENDERER_SCENE_H
#define WALLKAN_RENDERER_SCENE_H

#include <stdint.h>

#include <vulkan/vulkan.h>

#include "arena_alloc.h"
#include "err.h"
#include "renderer/device.h"
#include "renderer/scene/pipeline.h"
#include "renderer/scene/texture.h"
#include "renderer/scene/ubo.h"
#include "wksp/parser.h"

typedef struct WallkanScene {
    int timerfd;
    WallkanPipeline pipeline;
    struct {
        VkExtent2D extent;
        VkFormat format;
        WkTexture texture;
    } output;

    struct {
        WkUboRegistry registry;

        WkUBO *frame[MAX_FRAMES_IN_FLIGHT];
        WkFrameData frame_data;

        WkUBO *pass;
        WkPassData pass_data;

        WkUBO *shader_params[MAX_FRAMES_IN_FLIGHT];

        uint8_t *parameters_data;
        uint32_t parameters_count;
        uint32_t total_parameters_size;
    } ubo;

} WallkanScene;

typedef struct WkSceneConfig {
    const WKSPContainer *wksp;
    struct {
        VkExtent2D extent;
        VkFormat format;
    } texture_config;
    uint32_t fps;
} WkSceneConfig;

WkResult
wk_scene_init(ArenaAllocator *alloc, const WallkanDevice *wk_device, WallkanScene *wk_scene,
    WkSceneConfig *wk_scene_config);

WkResult
wk_scene_resize(const WallkanDevice *wk_device, WallkanScene *wk_scene, VkExtent2D extent);

WkResult
wk_scene_render(WallkanScene *wk_scene);

void
wk_scene_cleanup(const WallkanDevice *wk_device, WallkanScene *wk_scene);


#endif
