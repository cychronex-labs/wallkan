#ifndef WALLKAN_RENDERER_INSTANCE_H
#define WALLKAN_RENDERER_INSTANCE_H
#include <vulkan/vulkan.h>
#include "window/window.h"
#include "err.h"

typedef struct WallkanInstance{
    VkInstance               vk_instance;
    VkDebugUtilsMessengerEXT debug_messenger;
} WallkanInstance;


WkResult wk_instance_init(WallkanInstance *wk_instance);

WkResult
wk_instance_init_surface(WallkanInstance *wk_instance, const WallkanWindow *wk_window,
    WallkanOutput *wk_output, VkSurfaceKHR *vk_surface);

void
wk_instance_cleanup(WallkanInstance *wk_instance);

#endif
