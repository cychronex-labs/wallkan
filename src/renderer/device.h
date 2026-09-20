#ifndef WALLKAN_RENDERER_DEVICE_H
#define WALLKAN_RENDERER_DEVICE_H
#include "arena_alloc.h"
#include "err.h"
#include "renderer/instance.h"

typedef struct HardwareInfo {
    char device_name[256];
    VkPhysicalDeviceType device_type;
} HardwareInfo;

typedef struct VideoCodecSupport {
    bool     supported;
    uint32_t queue_family_idx;
} VideoCodecSupport;

typedef struct VideoInfo {
    bool video_decode_supported;
    uint32_t video_decode_queue_idx;
    VideoCodecSupport h264;
    VideoCodecSupport h265;
    VideoCodecSupport vp9;
} VideoInfo;

typedef struct WallkanDevice{
    VkDevice device;
    VkPhysicalDevice physical_device;

    uint32_t graphics_queue_family_idx;
    VkQueue graphics_queue;

    VkQueue video_queue_handle;
    HardwareInfo hardware_info;
    VideoInfo video_info;
} WallkanDevice;

WkResult
wk_device_init(ArenaAllocator *alloc, WallkanDevice *wk_device, WallkanInstance *wk_instance,
    VkSurfaceKHR first_vk_surface);

void
wk_device_cleanup(WallkanDevice *wk_device);

#endif
