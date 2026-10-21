#ifndef WALLKAN_SCENE_UBO
#define WALLKAN_SCENE_UBO
#include "renderer/device.h"
#include <stdint.h>
#include <vulkan/vulkan_core.h>
#define MAX_UBO 8
#define SHADERTOY_RESOLUTION_SIZE sizeof(float[3])

enum WkFrameDataOffset {
    WK_FRAME_DATA_START_POSITION = 0,

    WK_FRAME_DATA_IMOUSE_POSITION = 0,
    WK_FRAME_DATA_IFRAME_POSITION = 16,
    WK_FRAME_DATA_ITIME_POSITION = 20,
};

typedef struct WkFrameData {
	float iMouse[4];
	int iFrame;
	float iTime;
} WkFrameData;

typedef struct WkPassData {
    // CamelCase => For glsl
    // snake_case => For wallkan's own purposes
    uint32_t passes_res_size;
    uint32_t channels_res_size;
    uint32_t total_size;
    float *PassesResolution;
    float *iChannelResolution;
} WkPassData;

typedef struct WkUBO {
    VkBuffer buffer;
    VkDeviceSize size;
    VkDeviceSize mem_size;
    VkDeviceSize alignment;
    uint32_t memory_type_bits;
    void *mapped_mem;
} WkUBO;


typedef struct WkUboRegistry {
    VkDeviceMemory memory;
    WkUBO ubos[MAX_UBO];
    VkDeviceSize ubo_offsets[MAX_UBO];
    uint8_t ubo_slots;
    VkDeviceSize allocation_size;
} WkUboRegistry;

WkResult
wk_ubo_registry_new(WkUboRegistry *registry, const WallkanDevice *wk_device,
    VkDeviceSize buf_size, uint32_t *out_index);

WkResult
wk_ubo_registry_calculate_offsets(WkUboRegistry *registry);

WkResult
wk_ubo_registry_alloc(WkUboRegistry *registry, const WallkanDevice *wk_device);

WkResult
wk_ubo_registry_map_all(WkUboRegistry *registry, const WallkanDevice *wk_device);


static inline WkUBO *
wk_ubo_registry_get(WkUboRegistry *registry, uint32_t idx)
{
    return &registry->ubos[idx];
}

uint32_t
wk_ubo_frame_data_size(void);

typedef struct WKSPContainer WKSPContainer;
void
wk_ubo_pass_data_size(const WKSPContainer *wksp, uint32_t *passes_res_size,
    uint32_t *channels_res_size, uint32_t *total_size);

WkResult
wk_ubo_frame_data_init(const WKSPContainer *wksp, WkFrameData *frame_data);

WkResult
wk_ubo_pass_data_init(const WKSPContainer *wksp, VkExtent2D final_resolution,
    WkPassData *pass_data, uint32_t pass_count);

void
wk_ubo_frame_write(WkFrameData *input, WkUBO *ubo);

void
wk_ubo_pass_write(WkPassData *input, WkUBO *ubo, uint32_t pass_count);

typedef struct WallkanPipeline WallkanPipeline;
void
wk_ubo_pass_data_update(const WallkanPipeline *wk_pipeline, float *pass_resolutions,
    float *ch_resolutions, VkExtent2D final_resolution);

typedef struct WallkanScene WallkanScene;
WkResult
wk_ubo_init_all(WallkanScene *wk_scene, const WallkanDevice *wk_device, const WKSPContainer *wksp);

void
wk_ubo_frame_data_cleanup(WkFrameData *frame_data);

void
wk_ubo_pass_data_cleanup(WkPassData *pass_data);

void
wk_ubo_registry_cleanup(WkUboRegistry *registry, const WallkanDevice *wk_device);

void
wk_ubo_cleanup_all(WallkanScene *wk_scene, const WallkanDevice *wk_device);

#endif
