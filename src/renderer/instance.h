#ifndef WALLKAN_RENDERER_INSTANCE_H
#define WALLKAN_RENDERER_INSTANCE_H
#include <vulkan/vulkan.h>
#include "window.h"
#include "err.h"

typedef struct WallkanInstance{
    VkInstance               vk_instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkSurfaceKHR             vk_surface;
} WallkanInstance;


WkResult
wk_instance_init(WallkanInstance *wk_instance, const WallkanWindow *wk_window);

void
wk_instance_cleanup(WallkanInstance *wk_instance);

#endif
