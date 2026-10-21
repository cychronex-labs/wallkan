#ifndef WALLKAN_RENDERER_SCENE_BUFFER_H
#define WALLKAN_RENDERER_SCENE_BUFFER_H

#include "renderer/device.h"
#include <err.h>
#include <stdint.h>
#include <vulkan/vulkan_core.h>

WkResult
wk_buffer_new(const WallkanDevice *wk_device, VkBufferUsageFlags buf_usage,
    VkDeviceSize buf_size, VkBuffer *buffer, VkDeviceSize *out_mem_size,
    VkDeviceSize *out_alignment, uint32_t *out_mem_type_bits);
#endif
