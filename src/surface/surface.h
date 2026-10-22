#ifndef WALLKAN_SURFACE_SURFACE_H
#define WALLKAN_SURFACE_SURFACE_H

#include <stdint.h>

#include <vulkan/vulkan.h>

#include "err.h"
#include "surface/swapchain.h"

typedef struct ArenaAllocator ArenaAllocator;
typedef struct WallkanDevice WallkanDevice;
typedef struct WallkanInstance WallkanInstance;
typedef struct WallkanOutput WallkanOutput;
typedef struct WallkanScene WallkanScene;

typedef struct WallkanSurface {
    WallkanOutput     *wk_output;
    VkSurfaceKHR       vk_surface;
    WallkanSwapchain   wk_swapchain;
    uint32_t scene_idx;
} WallkanSurface;

WkResult
wk_surface_init(ArenaAllocator *alloc, WallkanInstance *wk_instance, WallkanDevice *wk_device,
    WallkanOutput *wk_output, WallkanSurface *wk_surface);

WkResult
wk_surface_update(WallkanScene *wk_scene);

void
wk_surface_cleanup(WallkanInstance *wk_instance, WallkanDevice *wk_device,
    WallkanSurface *wk_surface);

#endif
