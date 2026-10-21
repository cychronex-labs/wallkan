#include "err.h"
#include "renderer/device.h"
#include <stdint.h>
#include <vulkan/vulkan_core.h>

WkResult
wk_buffer_new(const WallkanDevice *wk_device, VkBufferUsageFlags buf_usage,
    VkDeviceSize buf_size, VkBuffer *buffer, VkDeviceSize *out_mem_size,
    VkDeviceSize *out_alignment, uint32_t *out_mem_type_bits)
{
    uint32_t queue_indices[1] = {
        wk_device->graphics_queue_family_idx
    };
    VkBufferCreateInfo ubo_create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pQueueFamilyIndices = queue_indices,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = buf_usage,
        .size = buf_size
    };
    VkDeviceBufferMemoryRequirements buf_mem_req = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS,
        .pCreateInfo = &ubo_create_info,
    };
    VkMemoryRequirements2 mem_reqs = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };
    vkGetDeviceBufferMemoryRequirements(wk_device->device, &buf_mem_req, &mem_reqs);
    *out_mem_size = mem_reqs.memoryRequirements.size;
    *out_alignment = mem_reqs.memoryRequirements.alignment;
    *out_mem_type_bits = mem_reqs.memoryRequirements.memoryTypeBits;
    WK_TRY(EXPECT_VK(
        vkCreateBuffer(wk_device->device, &ubo_create_info, NULL, buffer),
        WK_ERR_VK_CREATE_BUFFER_FAILED, "Failed to create buffer!"
    ));

    return WK_OK;
}
