#include "arena_alloc.h"
#include "common.h"
#include "err.h"
#include "renderer/instance.h"
#include "renderer/device.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>

static WkResult
scan_physical_devices(ArenaAllocator *alloc, const WallkanInstance *wk_instance,
    VkPhysicalDevice **out_devices, uint32_t *total_devices)
{
    *out_devices = NULL;
    WK_TRY(EXPECT_VK(
        vkEnumeratePhysicalDevices(wk_instance->vk_instance, total_devices, NULL),
        WK_ERR_VK_PHYSICAL_DEVICE_ENUMERATION_FAILURE, "Failed to get count of available GPUs!"
    ));
    if(*total_devices == 0){
        return WK_ERR(WK_ERR_VK_NO_PHYSICAL_DEVICE_FOUND,
            "Are you running this on a server with no GPU?");
    }
    *out_devices = arena_alloc(alloc, sizeof(VkPhysicalDevice) * (*total_devices));
    if (!*out_devices) {
        return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Allocation failure!");
    }
    WK_TRY(EXPECT_VK(
        vkEnumeratePhysicalDevices(wk_instance->vk_instance, total_devices, *out_devices),
        WK_ERR_VK_PHYSICAL_DEVICE_ENUMERATION_FAILURE, "Failed to get count of available GPUs!"
    ));
    return WK_OK;
}

static bool
check_device_properties(HardwareInfo *hardware_info, const VkPhysicalDevice physical_device)
{
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device, &props);
    LOG("check_device_properties: Checking basic properties...");
    LOG("check_device_properties: - %s", props.deviceName);
    if (props.apiVersion < VK_API_VERSION_1_3) {
        LOG("check_device_properties: - Vulkan 1.3 not supported (driver supports %u.%u.%u)",
            VK_API_VERSION_MAJOR(props.apiVersion),
            VK_API_VERSION_MINOR(props.apiVersion),
            VK_API_VERSION_PATCH(props.apiVersion));
        return false;
    }
    hardware_info->device_type = props.deviceType;
    strcpy(hardware_info->device_name, props.deviceName);
    return true;
}

static bool
check_device_features(const VkPhysicalDevice physical_device)
{
    LOG("check_device_features: Checking vulkan features...");
    bool supported = true;
    VkPhysicalDeviceVulkan13Features features13 = (VkPhysicalDeviceVulkan13Features){
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
    };
    VkPhysicalDeviceVulkan12Features features12 = (VkPhysicalDeviceVulkan12Features){
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &features13
    };
    VkPhysicalDeviceFeatures2 features2 = (VkPhysicalDeviceFeatures2){
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features12
    };
    vkGetPhysicalDeviceFeatures2(physical_device, &features2);
    if(features13.dynamicRendering != VK_TRUE){
        supported = false;
        LOG("check_device_features: - Dynamic Rendering not supported!");
    }
    if(features13.synchronization2 != VK_TRUE){
        supported = false;
        LOG("check_device_features: - Synchronization2 not supported!");
    }
    if(features12.scalarBlockLayout != VK_TRUE){
        supported = false;
        LOG("check_device_features: - scalarBlockLayout not supported!");
    }
    LOG("check_device_features: Verdict: %s!", supported ? "SUPPORTED" : "UNSUPPORTED");
    return supported;
}

static WkResult
scan_device_extensions(ArenaAllocator *alloc, const VkPhysicalDevice physical_device,
    VkExtensionProperties **out_extension_props, uint32_t *total_extensions)
{
    WkResult wkres;
    wkres = EXPECT_VK(
        vkEnumerateDeviceExtensionProperties(physical_device, NULL, total_extensions, NULL),
        WK_ERR_VK_DEVICE_EXT_ENUMERATION_FAILED, "Failed to get device extension count!"
    );
    if(wkres != WK_OK) goto err;

    *out_extension_props = arena_alloc(alloc, sizeof(VkExtensionProperties) * (*total_extensions));
    if (!*out_extension_props) {
        return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Allocation failure!");
    }
    wkres = EXPECT_VK(
        vkEnumerateDeviceExtensionProperties(physical_device, NULL, total_extensions,
            *out_extension_props),
        WK_ERR_VK_DEVICE_EXT_ENUMERATION_FAILED, "Failed to get device extension properties!"
    );
    if(wkres != WK_OK) goto err;

    return WK_OK;
err:
    return wkres;
}

static bool
check_device_extensions(WallkanDevice *device,
    const VkExtensionProperties *extension_props, uint32_t total_extensions)
{
    bool supported = false;
    for (uint32_t i = 0; i<total_extensions; i++) {
        VkExtensionProperties ext_props = extension_props[i];
        if (strcmp(ext_props.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            supported = true;
        }
        if (strcmp(ext_props.extensionName, VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME) == 0) {
            device->video_info.video_decode_supported = true;
        }
        if (strcmp(ext_props.extensionName, VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME) == 0) {
            device->video_info.h264.supported = true;
        }
        if (strcmp(ext_props.extensionName, VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME) == 0) {
            device->video_info.h265.supported = true;
        }
        if (strcmp(ext_props.extensionName, VK_KHR_VIDEO_DECODE_VP9_EXTENSION_NAME) == 0) {
            device->video_info.vp9.supported = true;
        }
    }
    return supported;
}

static WkResult
get_device_queue_data(ArenaAllocator *alloc, WallkanDevice *wk_device,
    VkSurfaceKHR vk_surface)
{
    WkResult wkres = WK_OK;
    // Zero initialization sets everything 0 which technically could be a valid index
    // So we use UINT32_MAX
    wk_device->graphics_queue_family_idx = UINT32_MAX;
    wk_device->video_info.video_decode_queue_idx = UINT32_MAX;
    wk_device->video_info.h264.queue_family_idx = UINT32_MAX;
    wk_device->video_info.h265.queue_family_idx = UINT32_MAX;
    wk_device->video_info.vp9.queue_family_idx = UINT32_MAX;
    //
    VkQueueFamilyVideoPropertiesKHR *video_props = NULL;
    VkQueueFamilyProperties2 *family_props2 = NULL;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(wk_device->physical_device,
        &queue_family_count, NULL);

    video_props = arena_alloc(alloc, sizeof(VkQueueFamilyVideoPropertiesKHR) * queue_family_count);
    family_props2 = arena_alloc(alloc, sizeof(VkQueueFamilyProperties2) * queue_family_count);
    if(!video_props || !family_props2){
        WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Allocation failure!");
        goto err;
    }
    for (uint32_t i=0; i<queue_family_count; i++) {
        video_props[i] = (VkQueueFamilyVideoPropertiesKHR){
            .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR
        };
        family_props2[i] = (VkQueueFamilyProperties2){
            .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,
            .pNext = &video_props[i]
        };
    }

    vkGetPhysicalDeviceQueueFamilyProperties2(wk_device->physical_device, &queue_family_count,
        family_props2);

    for(uint32_t i=0;i<queue_family_count;i++){
        VkQueueFlags queue_flags = family_props2[i].queueFamilyProperties.queueFlags;
        VkVideoCodecOperationFlagsKHR vid_codec_ops = video_props[i].videoCodecOperations;
        VkQueueFlags unified_queue = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        if(((queue_flags & unified_queue) == unified_queue) &&
            wk_device->graphics_queue_family_idx == UINT32_MAX){
            // Compute+Graphics
            LOG("get_device_queue_data: Queue family index: %d is graphics!", i);

            VkBool32 presentation_supported = false;
            wkres = EXPECT_VK(
                vkGetPhysicalDeviceSurfaceSupportKHR(wk_device->physical_device, i,
                    vk_surface, &presentation_supported),
                WK_ERR_VK_DEVICE_SUPPORT_SURFACE_PRESENTATION_FAILURE,
                "Failure on checking if 'maybe suitable' queue family supports presentation..."
            );

            if(presentation_supported){
                LOG("get_device_queue_data: Queue %d supports presentation!", i);
                wk_device->graphics_queue_family_idx = i;
            }
            if(wkres != WK_OK) goto err;
        }
        if(queue_flags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) {
            wk_device->video_info.video_decode_queue_idx = i;
            LOG("get_device_queue_data: Queue family index: %d is video!", i);
            if(vid_codec_ops & (VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)){
                LOG("get_device_queue_data: - SUPPORTS H264!");
                wk_device->video_info.h264.queue_family_idx = i;
            }
            if(vid_codec_ops & (VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)){
                LOG("get_device_queue_data: - SUPPORTS H265!");
                wk_device->video_info.h265.queue_family_idx = i;
            }
            if(vid_codec_ops & (VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR)){
                LOG("get_device_queue_data: - SUPPORTS VP9!");
                wk_device->video_info.vp9.queue_family_idx = i;
            }
        }
    }
    return WK_OK;
err:
    return wkres;
}

static WkResult
choose_device(ArenaAllocator *alloc, const VkPhysicalDevice *devices, uint32_t total_devices,
    WallkanDevice *out_wk_device, VkSurfaceKHR first_vk_surface)
{
    uint32_t total_extensions = 0;
    bool found_supported = false;
    for (uint32_t i=0; i<total_devices; i++) {
        VkExtensionProperties *extension_props = NULL;
        VkPhysicalDevice physical_device = devices[i];
        WallkanDevice device = {
            .physical_device = physical_device
        };

        LOG("check_devices_capabilities: Checking the capabilities of device %d", i);
        bool supported = check_device_properties(&device.hardware_info, physical_device) &&
            check_device_features(physical_device);
        if(!supported) continue;
        // Extensions

        WK_TRY(scan_device_extensions(alloc, physical_device,
            &extension_props, &total_extensions));

        supported = supported && check_device_extensions(&device, extension_props,
            total_extensions);

        if(!supported) continue;

        WK_TRY(get_device_queue_data(alloc, &device, first_vk_surface));
        if (device.graphics_queue_family_idx == UINT32_MAX) {
            supported = false;
            continue;
        }

        if(!found_supported){
            // Set the default, first supported GPU device
            *out_wk_device = device;
            found_supported = true;
        }

        // By default choose a supporting Integrated GPU
        if(device.hardware_info.device_type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU){
            *out_wk_device = device;
            break;
        }
    }

    if (!found_supported) {
        return WK_ERR(WK_ERR_VK_NO_PHYSICAL_DEVICE_FOUND, "No suitable Vulkan 1.3 GPU found!");
    }
    return WK_OK;
}

static void
get_device_extensions(WallkanDevice *wk_device, const char **extensions, uint32_t *total_extensions)
{
    // If extensions are supported then queue families are checked
    // Therefore we check queue indexes
    // So even if a buggy case where extension exists but queue does wallkan just ignores video
    extensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    if(wk_device->video_info.video_decode_queue_idx != UINT32_MAX){
        extensions[*total_extensions] = VK_KHR_VIDEO_QUEUE_EXTENSION_NAME;
        (*total_extensions)++;
        extensions[*total_extensions] = VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME;
        (*total_extensions)++;
        if(wk_device->video_info.h264.queue_family_idx != UINT32_MAX){
            extensions[*total_extensions] = VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME;
            (*total_extensions)++;
        }
        if(wk_device->video_info.h265.queue_family_idx != UINT32_MAX){
            extensions[*total_extensions] = VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME;
            (*total_extensions)++;
        }
        if(wk_device->video_info.vp9.queue_family_idx != UINT32_MAX){
            extensions[*total_extensions] = VK_KHR_VIDEO_DECODE_VP9_EXTENSION_NAME;
            (*total_extensions)++;
        }
    }
}

static void
get_queue_create_infos(WallkanDevice *wk_device, uint32_t *queue_create_count,
    VkDeviceQueueCreateInfo *queue_create_infos, const float *queue_priority)
{
    *queue_create_count = 1;
    queue_create_infos[0] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = wk_device->graphics_queue_family_idx,
        .queueCount = 1,
        .pQueuePriorities = queue_priority
    };

    if(wk_device->video_info.video_decode_queue_idx != UINT32_MAX){
        if(wk_device->video_info.video_decode_queue_idx == wk_device->graphics_queue_family_idx){
            return;
        }
        queue_create_infos[1] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = wk_device->video_info.video_decode_queue_idx,
            .queueCount = 1,
            .pQueuePriorities = queue_priority
        };
        (*queue_create_count)++;
    }
}

static WkResult
create_logical_device(WallkanDevice *wk_device)
{
    const float queue_priority = 0.2f;
    uint32_t total_extensions = 1;
    const char *extensions[12] = {0};

    get_device_extensions(wk_device, extensions, &total_extensions);
    uint32_t queue_create_count = 0;
    VkDeviceQueueCreateInfo queue_create_infos[2];
    get_queue_create_infos(wk_device, &queue_create_count, queue_create_infos, &queue_priority);
    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .dynamicRendering = VK_TRUE,
        .synchronization2 = VK_TRUE,
    };
    VkPhysicalDeviceVulkan12Features features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .scalarBlockLayout = VK_TRUE,
        .pNext = &features13
    };
    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features12,
    };
    const VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .enabledExtensionCount = total_extensions,
        .ppEnabledExtensionNames = extensions,
        .pNext = &features,
        .queueCreateInfoCount = queue_create_count,
        .pQueueCreateInfos = queue_create_infos,
    };

    WK_TRY(EXPECT_VK(
        vkCreateDevice(wk_device->physical_device, &device_create_info, NULL, &wk_device->device),
        WK_ERR_VK_LOGICAL_DEVICE_CREATION_FAILURE, "Failed to create logical device!"
    ));
    vkGetDeviceQueue(wk_device->device, wk_device->graphics_queue_family_idx, 0,
        &wk_device->graphics_queue);
    if(!wk_device->graphics_queue){
        return WK_ERR(WK_ERR_VK_FAILED_TO_GET_QUEUE, "Failed to get device graphics queue!");
    }

    if (wk_device->video_info.video_decode_queue_idx == UINT32_MAX) {
        wk_device->video_queue_handle = VK_NULL_HANDLE;
        return WK_OK;
    }

    vkGetDeviceQueue(wk_device->device, wk_device->video_info.video_decode_queue_idx,
        0, &wk_device->video_queue_handle);
    if(!wk_device->video_queue_handle){
        return WK_ERR(WK_ERR_VK_FAILED_TO_GET_QUEUE, "Failed to get device video queue!");
    }
    return WK_OK;
}

WkResult
wk_device_init(ArenaAllocator *alloc, WallkanDevice *wk_device, WallkanInstance *wk_instance,
    VkSurfaceKHR first_vk_surface)
{
    WkResult wkres;
    uint32_t total_devices = 0;
    VkPhysicalDevice *devices = NULL;

    wkres = scan_physical_devices(alloc, wk_instance, &devices, &total_devices);
    if(wkres != WK_OK) goto err;

    wkres = choose_device(alloc, devices, total_devices, wk_device, first_vk_surface);
    if(wkres != WK_OK) goto err;

    wkres = create_logical_device(wk_device);
    if(wkres != WK_OK) goto err;
    return WK_OK;
err:
    return wkres;
}

void
wk_device_cleanup(WallkanDevice *wk_device)
{
    if (wk_device->device != VK_NULL_HANDLE) {
        LOG("wk_device_cleanup: Destroying Vulkan logical device...");
        vkDestroyDevice(wk_device->device, NULL);
        wk_device->device = VK_NULL_HANDLE;
    }
}
