#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.h>
#include "arena_alloc.h"
#include "common.h"
#include "err.h"
#include "window/window.h"
#include "renderer/instance.h"

struct ExtensionList {
    bool surface;
    bool wayland_surface;
    bool debug_utils;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL
cb_vk_debug(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,void* pUserData)
{
    (void)messageType;
    (void)pUserData;
    if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT){
        fprintf(stderr, "%s[Validation Layer: ERROR] %s%s\n", COLOR_ERR, pCallbackData->pMessage, COLOR_RESET);
    }
    if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT){
        fprintf(stderr, "%s[Validation Layer: WARNING] %s%s\n", COLOR_WARN, pCallbackData->pMessage, COLOR_RESET);
    }
    return VK_FALSE;
}

static WkResult
scan_extensions(ArenaAllocator *alloc, struct ExtensionList *ext_list)
{
    LOG("scan_extensions: Scanning for extensions...");
    WkResult wkres;

    VkExtensionProperties *extensions = NULL;
    uint32_t total_extensions = 0;

    wkres = EXPECT_VK(
        vkEnumerateInstanceExtensionProperties(NULL,&total_extensions, NULL),
        WK_ERR_VK_INSTANCE_EXT_ENUMERATION_FAILED, "Failed to get instance extension count!"
    );
    if(wkres != WK_OK) goto err;
    if(total_extensions == 0) {
        wkres = WK_ERR(WK_ERR_VK_INSTANCE_EXT_ENUMERATION_FAILED, "Extension count = 0!");
        goto err;
    }

    extensions = arena_alloc(alloc, total_extensions * sizeof(VkExtensionProperties));
    if(!extensions) {
        WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Allocation failure!");
        goto err;
    }

    wkres = EXPECT_VK(
        vkEnumerateInstanceExtensionProperties(NULL,&total_extensions, extensions),
        WK_ERR_VK_INSTANCE_EXT_ENUMERATION_FAILED, "Failed to get instance extension count!"
    );
    if(wkres != WK_OK) goto err;

    for(uint32_t i=0;i<total_extensions;i++){
        VkExtensionProperties *extension_props = &extensions[i];
        // Iterate through necessary extensions
        if(strcmp(extension_props->extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0){
            LOG("scan_extensions: Found extension %s!", VK_KHR_SURFACE_EXTENSION_NAME);
            ext_list->surface = true;
        }
        if(strcmp(extension_props->extensionName, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME) == 0){
            LOG("scan_extensions: Found extension %s!", VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
            ext_list->wayland_surface = true;
        }
        if(strcmp(extension_props->extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0){
            LOG("scan_extensions: Found extension %s!", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            ext_list->debug_utils = true;
        }
    }

    // Validate
    if(!ext_list->surface){
        wkres = WK_ERR(WK_ERR_VK_REQUIRED_EXTENSION_UNAVAILABLE,
            "Missing mandatory extension: %s!", VK_KHR_SURFACE_EXTENSION_NAME);
        goto err;
    }
    if(!ext_list->wayland_surface){
        wkres = WK_ERR(WK_ERR_VK_REQUIRED_EXTENSION_UNAVAILABLE,
            "Missing mandatory extension: %s!", VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
        goto err;
    }

    #ifndef NDEBUG
    if(!ext_list->debug_utils){
        WARN("scan_extensions: Debug utils not found in debug build. Validation layers will be disabled!");
    }
    #endif
    return WK_OK;
err:
    return wkres;
}

static WkResult
scan_validation_layers(ArenaAllocator *alloc, bool *validation_layer_available)
{
    WkResult wkres;

    uint32_t total_layers = 0;
    VkLayerProperties *layers = NULL;

    wkres = EXPECT_VK(
        vkEnumerateInstanceLayerProperties(&total_layers, NULL),
        WK_ERR_VK_INSTANCE_LAYER_ENUMERATION_FAILED, "Failed to get instance layer count!"
    );
    if(wkres != WK_OK) goto err;

    if(total_layers == 0) {
        wkres = WK_ERR(WK_ERR_VK_INSTANCE_LAYER_ENUMERATION_FAILED, "Layer count = 0!");
        goto err;
    }

    layers = arena_alloc(alloc, sizeof(VkLayerProperties) * total_layers);
    if(!layers) {
        wkres = WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Allocation failure");
        goto err;
    }

    wkres = EXPECT_VK(
        vkEnumerateInstanceLayerProperties(&total_layers, layers),
        WK_ERR_VK_INSTANCE_LAYER_ENUMERATION_FAILED, "Failed to get instance layers data!"
    );
    if(wkres != WK_OK) goto err;

    for(uint32_t i=0;i<total_layers;i++){
        if(strcmp(layers[i].layerName, "VK_LAYER_KHRONOS_validation") == 0){
            LOG("configure_validation_layers: Found layer VK_LAYER_KHRONOS_validation...");
            *validation_layer_available = true;
            break;
        }
    }
    if (!*validation_layer_available)
        WARN("The Validation layers are missing but project is running in debug mode!");
    return WK_OK;
err:
    return wkres;
}

static WkResult
setup_debug_messenger(WallkanInstance *wk_instance)
{
    WkResult wkres;
    PFN_vkCreateDebugUtilsMessengerEXT vk_debug_create_func = NULL;

    vk_debug_create_func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        wk_instance->vk_instance,"vkCreateDebugUtilsMessengerEXT");
    if(!vk_debug_create_func){
        wkres = WK_ERR(WK_ERR_VK_GET_INSTANCE_PROC_ADDR_FAILURE,
            "Failed to get the address of vkCreateDebugUtilsMessengerEXT!");
        goto err;
    }
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = &cb_vk_debug,
    };

    wkres = EXPECT_VK(
        vk_debug_create_func(wk_instance->vk_instance, &debug_create_info,
            NULL, &(wk_instance->debug_messenger)),
        WK_ERR_VK_CREATE_DEBUG_MESSENGER_FAILED, "Failed to create debug messenger!"
    );
    if(wkres != WK_OK) goto err;

    return WK_OK;
    err:
        return wkres;
}

WkResult
wk_instance_init_surface(WallkanInstance *wk_instance, const WallkanWindow *wk_window,
    WallkanOutput *wk_output, VkSurfaceKHR *vk_surface)
{
    LOG("wk_instance_init_surface: Creating vulkan surface for wayland..");
    VkWaylandSurfaceCreateInfoKHR wl_surface_create_info = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = wk_window->display,
        .surface = wk_output->surface,
    };

    WK_TRY(EXPECT_VK(
        vkCreateWaylandSurfaceKHR(wk_instance->vk_instance,
            &wl_surface_create_info, NULL, vk_surface),
        WK_ERR_VK_WL_SURFACE_CREATION_FAILURE, "Failed to create Vulkan Wayland WSI surface!"
    ));
    return WK_OK;
}

static void
enable_extensions(VkInstanceCreateInfo *instance_create_info, struct ExtensionList *ext_list,
    const char **total_extensions, uint32_t *ext_count)
{

    if(ext_list->surface){
        total_extensions[*ext_count] = VK_KHR_SURFACE_EXTENSION_NAME;
        (*ext_count)++;
    }
    if(ext_list->wayland_surface){
        total_extensions[*ext_count] = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
        (*ext_count)++;
    }
    if(ext_list->debug_utils){
        total_extensions[*ext_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        (*ext_count)++;
    }
    instance_create_info->enabledExtensionCount = *ext_count;
    instance_create_info->ppEnabledExtensionNames = total_extensions;
}

WkResult
wk_instance_init(ArenaAllocator *alloc, WallkanInstance *wk_instance)
{
    LOG("wk_instance_init: Initializing vulkan instance...");
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Wallkan",
        .apiVersion = VK_API_VERSION_1_3
    };
    VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info
    };
    struct ExtensionList ext_list = {0};
    bool validation_layer_available = false;
    WK_TRY(scan_extensions(alloc, &ext_list));

    const char *total_extensions[3] = {0};
    const char *layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    uint32_t ext_count = 0;
    enable_extensions(&instance_create_info, &ext_list, total_extensions, &ext_count);

    if(ext_list.debug_utils)
        WK_TRY(scan_validation_layers(alloc, &validation_layer_available));

    if(validation_layer_available){
        instance_create_info.enabledLayerCount = 1;
        instance_create_info.ppEnabledLayerNames = layers;
    }

    WK_TRY(EXPECT_VK(
        vkCreateInstance(&instance_create_info, NULL, &(wk_instance->vk_instance)),
        WK_ERR_VK_INSTANCE_CREATION_FAILED,"Instance creation failed!"
    ));

    if(ext_list.debug_utils)
        WK_TRY(setup_debug_messenger(wk_instance));

    return WK_OK;
}

void
wk_instance_cleanup(WallkanInstance *wk_instance)
{
    if(wk_instance->debug_messenger != VK_NULL_HANDLE){
        LOG("wk_instance_cleanup: Retrieving address of vkDestroyDebugUtilsMessengerEXT...");
        PFN_vkDestroyDebugUtilsMessengerEXT vk_debug_destroy_func = NULL;
        vk_debug_destroy_func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            wk_instance->vk_instance,"vkDestroyDebugUtilsMessengerEXT");
        if(!vk_debug_destroy_func){
            WARN("wk_instance_cleanup: Could not get the address of vkDestroyDebugUtilsMessengerEXT!");
        }
        LOG("wk_instance_cleanup: Destroying debug messenger...");
        vk_debug_destroy_func(wk_instance->vk_instance,
            wk_instance->debug_messenger, NULL);
        wk_instance->debug_messenger = VK_NULL_HANDLE;
    }
    if(wk_instance->vk_instance != VK_NULL_HANDLE){
        LOG("wk_instance_cleanup: Destroying vulkan instance...");
        vkDestroyInstance(wk_instance->vk_instance, NULL);
        wk_instance->vk_instance = VK_NULL_HANDLE;
    }
}
