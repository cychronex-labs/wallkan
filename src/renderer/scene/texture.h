#ifndef WALLKAN_RENDERER_SCENE_TEXTURE_H
#define WALLKAN_RENDERER_SCENE_TEXTURE_H

#include <vulkan/vulkan.h>

#include "err.h"

typedef struct WallkanDevice WallkanDevice;

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
