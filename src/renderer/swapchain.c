#include "renderer/swapchain.h"
#include "common.h"
#include "err.h"
#include "renderer/device.h"
#include "window/outputs.h"
#include <stdint.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) < (b)) ? (b) : (a))

static WkResult
get_surface_capabilities(WallkanDevice *wk_device, WallkanOutput *wk_output,
    VkSurfaceKHR vk_surface, VkSurfaceCapabilitiesKHR *out_surface_capabilities)
{
    WK_TRY(EXPECT_VK(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(wk_device->physical_device, vk_surface,
            out_surface_capabilities),
        WK_ERR_VK_CANNOT_GET_SURFACE_CAPABILITY, "Failed to get surface capabilities!"
    ));
    LOG("get_surface_capabilities: Got swapchain surface capabilities...");
    LOG("get_surface_capabilities: - OUTPUT: %s", wk_output->name);
    return WK_OK;
}

static void
calculate_image_count(VkSurfaceCapabilitiesKHR *surface_capabilities, uint32_t *out_image_count)
{
    if(surface_capabilities->maxImageCount == 0){
        *out_image_count = surface_capabilities->minImageCount+1;
        return;
    }
    *out_image_count = MAX(surface_capabilities->minImageCount,
        surface_capabilities->minImageCount+1);
    *out_image_count = MIN(*out_image_count, surface_capabilities->maxImageCount);
    LOG("calculate_image_count:  - Image count: %u", *out_image_count);
}

static void
calculate_image_extent(WallkanOutput *wk_output, VkSurfaceCapabilitiesKHR *surface_capabilities,
    VkExtent2D *out_extent)
{
    if(surface_capabilities->currentExtent.width != UINT32_MAX){
        out_extent->width = surface_capabilities->currentExtent.width;
    }else{
        out_extent->width = MIN(
            MAX(surface_capabilities->minImageExtent.width, wk_output->width),
            surface_capabilities->maxImageExtent.width);
    }

    if(surface_capabilities->currentExtent.height != UINT32_MAX){
        out_extent->height = surface_capabilities->currentExtent.height;
    }else{
        out_extent->height = MIN(
            MAX(surface_capabilities->minImageExtent.height, wk_output->height),
            surface_capabilities->maxImageExtent.height);
    }
    LOG("calculate_image_extent: - Image extent: %ux%u", out_extent->width, out_extent->height);
}

static bool
surface_format_exists(VkSurfaceFormatKHR *surface_formats, uint32_t surface_format_count,
    VkFormat target_surface_format, VkColorSpaceKHR target_colorspace)
{
    for(uint32_t i=0;i<surface_format_count;i++){
        VkSurfaceFormatKHR *surface_format = &surface_formats[i];
        if(surface_format->format == target_surface_format){
            if(surface_format->colorSpace == target_colorspace){
                return true;
            }
        }
    }
    return false;
}

static WkResult
choose_surface_format(WallkanDevice *wk_device, VkSurfaceKHR vk_surface,
    VkFormat *out_surface_format, VkColorSpaceKHR *out_colorspace)
{
    WkResult wkres = WK_OK;
    uint32_t surface_format_count = 0;
    WK_TRY(EXPECT_VK(
        vkGetPhysicalDeviceSurfaceFormatsKHR(wk_device->physical_device , vk_surface,
            &surface_format_count, NULL),
        WK_ERR_VK_CANNOT_GET_SURFACE_FORMATS, "Couldn't get physical device surface format count!"
    ));
    VkSurfaceFormatKHR *surface_formats = malloc(surface_format_count * sizeof(VkSurfaceFormatKHR));
    wkres = EXPECT_VK(
        vkGetPhysicalDeviceSurfaceFormatsKHR(wk_device->physical_device , vk_surface,
            &surface_format_count, surface_formats),
        WK_ERR_VK_CANNOT_GET_SURFACE_FORMATS, "Couldn't get physical device surface formats!"
    );
    if(wkres != WK_OK) goto cleanup;
    if(surface_format_exists(surface_formats, surface_format_count, VK_FORMAT_B8G8R8A8_UNORM,
        VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
    {
        *out_surface_format = VK_FORMAT_B8G8R8A8_UNORM;
        *out_colorspace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        LOG("surface_format_exists: - Image format: VK_FORMAT_B8G8R8A8_UNORM & VK_COLOR_SPACE_SRGB_NONLINEAR_KHR");
        goto cleanup;
    }
    *out_surface_format = surface_formats[0].format;
    *out_colorspace = surface_formats[0].colorSpace;
cleanup:
    free(surface_formats);
    return wkres;
}

static WkResult
init_swapchain_images(WallkanSwapchain *wk_swapchain, WallkanDevice *wk_device)
{
    LOG("init_swapchain_images: Getting swapchain images...");
    WK_TRY(EXPECT_VK(
        vkGetSwapchainImagesKHR(wk_device->device, wk_swapchain->vk_swapchain,
            &wk_swapchain->image_count, NULL),
        WK_ERR_VK_CANNOT_GET_SWAPCHAIN_IMAGES,"Failed to get swapchain images count!"
    ));
    LOG("init_swapchain_images: - Swapchain image count: %d...", wk_swapchain->image_count);
    wk_swapchain->images = malloc(wk_swapchain->image_count * sizeof(VkImage));
    WK_TRY(EXPECT_VK(
        vkGetSwapchainImagesKHR(wk_device->device, wk_swapchain->vk_swapchain,
            &wk_swapchain->image_count, wk_swapchain->images),
        WK_ERR_VK_CANNOT_GET_SWAPCHAIN_IMAGES,"Failed to get swapchain images!"
    ));
    return WK_OK;
}

static WkResult
init_swapchain_image_views(WallkanSwapchain *wk_swapchain, WallkanDevice *wk_device)
{
    LOG("init_swapchain_image_views: Creating swapchain image views...");
    wk_swapchain->image_views = calloc(wk_swapchain->image_count, sizeof(VkImageView));
    VkImageViewCreateInfo img_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel=0,
            .levelCount=1,
            .baseArrayLayer=0,
            .layerCount=1
        },
        .format = wk_swapchain->format
    };
    for (uint32_t i=0; i<wk_swapchain->image_count; i++) {
        img_view_create_info.image = wk_swapchain->images[i];
        WK_TRY(EXPECT_VK(
            vkCreateImageView(wk_device->device, &img_view_create_info, NULL,
                &wk_swapchain->image_views[i]),
            WK_ERR_VK_CANNOT_CREATE_IMAGE_VIEW, "Failed to create image view!"
        ));
    }
    return WK_OK;
}

WkResult
wk_swapchain_init(WallkanSwapchain *wk_swapchain, WallkanDevice *wk_device,
    WallkanOutput *wk_output, VkSurfaceKHR vk_surface)
{
    LOG("wk_swapchain_init: Initializing swapchain...");
    VkSurfaceCapabilitiesKHR surface_capabilities;
    WK_TRY(get_surface_capabilities(wk_device, wk_output, vk_surface, &surface_capabilities));
    uint32_t min_img_count = 0;
    calculate_image_count(&surface_capabilities, &min_img_count);
    calculate_image_extent(wk_output, &surface_capabilities, &wk_swapchain->image_extent);
    WK_TRY(choose_surface_format(wk_device, vk_surface, &wk_swapchain->format,
        &wk_swapchain->colorspace));
    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vk_surface,
        .minImageCount = min_img_count,
        .imageFormat = wk_swapchain->format,
        .imageColorSpace = wk_swapchain->colorspace,
        .imageExtent = wk_swapchain->image_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_MAILBOX_KHR,
        .clipped = VK_TRUE
    };
    WK_TRY(EXPECT_VK(
        vkCreateSwapchainKHR(wk_device->device, &swapchain_create_info, NULL,
            &wk_swapchain->vk_swapchain),
        WK_ERR_VK_SWAPCHAIN_CREATION_FAILURE, "Swapchain creation failed!"
    ));
    LOG("wk_swapchain_init: Created VkSwapchainKHR...");
    WK_TRY(init_swapchain_images(wk_swapchain, wk_device));
    WK_TRY(init_swapchain_image_views(wk_swapchain, wk_device));
    return WK_OK;
}

void
wk_swapchain_cleanup(WallkanSwapchain *wk_swapchain, WallkanDevice *wk_device)
{
    LOG("wk_swapchain_cleanup: Destroying swapchain...");
    if(wk_swapchain->image_views){
        LOG("wk_swapchain_cleanup: Destroying swapchain image views...");
        for (uint32_t i=0; i<wk_swapchain->image_count; i++) {
            if(wk_swapchain->image_views[i]){
                vkDestroyImageView(wk_device->device, wk_swapchain->image_views[i], NULL);
                wk_swapchain->image_views[i] = VK_NULL_HANDLE;
            }
        }
        free(wk_swapchain->image_views);
        wk_swapchain->image_views = NULL;
    }
    if(wk_swapchain->images){
        LOG("wk_swapchain_cleanup: Freeing swapchain images...");
        free(wk_swapchain->images);
        wk_swapchain->images = NULL;
    }
    if(wk_swapchain->vk_swapchain){
        LOG("wk_swapchain_cleanup: Destroying VkSwapchainKHR...");
        vkDestroySwapchainKHR(wk_device->device, wk_swapchain->vk_swapchain, NULL);
        wk_swapchain->vk_swapchain = VK_NULL_HANDLE;
    }
}
