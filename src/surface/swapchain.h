#ifndef WALLKAN_SURFACE_SWAPCHAIN_H
#define WALLKAN_SURFACE_SWAPCHAIN_H

#include <stdint.h>

#include <vulkan/vulkan.h>

#include "arena_alloc.h"
#include "err.h"

typedef struct WallkanDevice WallkanDevice;
typedef struct WallkanOutput WallkanOutput;

typedef struct WallkanSwapchain {
    VkSwapchainKHR vk_swapchain;
    VkFormat format;
    VkColorSpaceKHR colorspace;
    VkExtent2D image_extent;
    uint32_t image_count;
    VkImage *images;
    VkImageView *image_views;
} WallkanSwapchain;

WkResult
wk_swapchain_init(ArenaAllocator *alloc, WallkanSwapchain *wk_swapchain, WallkanDevice *wk_device,
    WallkanOutput *wk_output, VkSurfaceKHR vk_surface);

void
wk_swapchain_cleanup(WallkanSwapchain *wk_swapchain, WallkanDevice *wk_device);

#endif
