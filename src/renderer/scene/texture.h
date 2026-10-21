#ifndef WALLKAN_RENDERER_SCENE_TEXTURE
#define WALLKAN_RENDERER_SCENE_TEXTURE
#include "err.h"
#include "renderer/device.h"
#include <vulkan/vulkan_core.h>

typedef struct WkTexture {
    VkDeviceMemory      memory;
    VkImage             image;
    VkImageView         image_view;
} WkTexture;

WkResult
wk_texture_create(const WallkanDevice *wk_device, VkFormat format,
    VkImageUsageFlags usage, VkMemoryPropertyFlags mem_props, VkExtent3D extent,
    WkTexture *out_texture);

void
wk_texture_destroy(const WallkanDevice *wk_device, WkTexture *wk_texture);

#endif
