#ifndef WALLKAN_RENDERER_SWAPCHAIN_H
#define WALLKAN_RENDERER_SWAPCHAIN_H
#include "err.h"
#include "renderer/device.h"

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
wk_swapchain_init(WallkanSwapchain *wk_swapchain, WallkanDevice *wk_device,
    WallkanOutput *wk_output, VkSurfaceKHR vk_surface);

void
wk_swapchain_cleanup(WallkanSwapchain *wk_swapchain, WallkanDevice *wk_device);

#endif
