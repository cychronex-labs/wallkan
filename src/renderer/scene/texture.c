#include "err.h"
#include <renderer/scene/texture.h>
#include <stdint.h>
#include <vulkan/vulkan_core.h>

static WkResult
find_memory_type(const WallkanDevice *wk_device, uint32_t memoryTypeBits,
    VkMemoryPropertyFlags vk_props, uint32_t *out_index)
{
    WkResult wkres = WK_OK;
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(wk_device->physical_device, &mem_props);
    for (uint32_t i=0; i<mem_props.memoryTypeCount; i++) {
        if (memoryTypeBits & (1u << i)) {
            if ((mem_props.memoryTypes[i].propertyFlags & vk_props) == vk_props){
                *out_index = i;
                return wkres;
            }
        }
    }
    return WK_ERR(WK_ERR_VK_REQUIRED_MEMORY_TYPE_NOT_FOUND, "Couldnt find required memory type!");
}

static WkResult
create_image(const WallkanDevice *wk_device, VkFormat format, VkExtent3D extent,
    VkImageUsageFlags usage, VkImageLayout initialLayout, VkImage *out_image)
{
    VkImageCreateInfo img_create_info = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = extent,
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = usage,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = initialLayout
    };
    WK_TRY(EXPECT_VK(
        vkCreateImage(wk_device->device, &img_create_info, NULL, out_image),
        WK_ERR_VK_CREATE_IMAGE_FAILED, "Failed to create vulkan image!"
    ));

    return WK_OK;
}

static WkResult
alloc_image_memory(const WallkanDevice *wk_device, VkImage image,
    VkMemoryPropertyFlags mem_props, VkDeviceMemory *out_mem)
{
    VkMemoryRequirements mem_requirements;
    uint32_t mem_index;
    vkGetImageMemoryRequirements(wk_device->device, image, &mem_requirements);
    WK_TRY(find_memory_type(wk_device, mem_requirements.memoryTypeBits, mem_props, &mem_index));

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .memoryTypeIndex = mem_index,
        .allocationSize = mem_requirements.size
    };
    WK_TRY(EXPECT_VK(
        vkAllocateMemory(wk_device->device, &alloc_info, NULL, out_mem),
        WK_ERR_VK_DEVICE_MEMORY_ALLOCATION_FAILURE, "Failed to allocate vulkan device memory!"
    ));
    WK_TRY(EXPECT_VK(
        vkBindImageMemory(wk_device->device, image, *out_mem, 0),
        WK_ERR_VK_DEVICE_MEMORY_ALLOCATION_FAILURE, "Failed to bind image to vulkan device memory!"
    ));
    return WK_OK;
}

static WkResult
create_image_view(const WallkanDevice *wk_device, VkFormat format, VkImage image,
    VkImageView *out_image_view)
{
    VkImageViewCreateInfo img_view_create_info = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = image,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = format,
        .subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };
    WK_TRY(EXPECT_VK(
        vkCreateImageView(wk_device->device, &img_view_create_info, NULL, out_image_view),
        WK_ERR_VK_CREATE_IMAGE_VIEW_FAILED, "Failed to create vulkan image view!"
    ));
    return WK_OK;
}

WkResult
wk_texture_create(const WallkanDevice *wk_device, VkFormat format,
    VkImageUsageFlags usage, VkMemoryPropertyFlags mem_props, VkExtent3D extent,
    WkTexture *out_texture)
{
    WK_TRY(create_image(wk_device, format, extent, usage, VK_IMAGE_LAYOUT_UNDEFINED, &out_texture->image));
    WK_TRY(alloc_image_memory(wk_device, out_texture->image, mem_props, &out_texture->memory));
    WK_TRY(create_image_view(wk_device, format, out_texture->image, &out_texture->image_view));
    return WK_OK;
}

void
wk_texture_destroy(const WallkanDevice *wk_device, WkTexture *wk_texture)
{
    if(wk_texture->image_view){
        vkDestroyImageView(wk_device->device, wk_texture->image_view, NULL);
        wk_texture->image_view = NULL;
    }
    if(wk_texture->memory){
        vkFreeMemory(wk_device->device, wk_texture->memory, NULL);
        wk_texture->memory = NULL;
    }
    if(wk_texture->image){
        vkDestroyImage(wk_device->device, wk_texture->image, NULL);
        wk_texture->image = NULL;
    }
}
