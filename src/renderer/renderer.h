#ifndef WALLKAN_RENDERER_H
#define WALLKAN_RENDERER_H
#include "err.h"
#include "renderer/swapchain.h"
#include "window/window.h"
#include "renderer/instance.h"
#include "renderer/device.h"

typedef struct WallkanRenderer {
    WallkanInstance  wk_instance;
    WallkanDevice    wk_device;
    VkSurfaceKHR     vk_surfaces[MAX_OUTPUTS];
    WallkanSwapchain wk_swapchain[MAX_OUTPUTS];
} WallkanRenderer;

WkResult
wk_renderer_init(WallkanRenderer *wk_renderer, WallkanWindow *wk_window);

WkResult
wk_renderer_render(WallkanRenderer *wk_renderer, WallkanWindow *wk_window);

void
wk_renderer_cleanup(WallkanRenderer *wk_renderer, WallkanWindow *wk_window);

#endif
