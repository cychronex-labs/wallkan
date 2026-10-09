#include "arena_alloc.h"
#include "err.h"
#include "renderer/device.h"
#include "renderer/instance.h"
#include "window/outputs.h"
#include <surface/surface.h>

WkResult
wk_surface_init(ArenaAllocator *alloc, WallkanInstance *wk_instance, WallkanDevice *wk_device,
    WallkanOutput *wk_output, WallkanSurface *wk_surface)
{
    wk_surface->wk_output = wk_output;
    WK_TRY(
        wk_instance_init_surface(wk_instance, wk_output->wk_window,
            wk_output, &wk_surface->vk_surface)
    );
    WK_TRY(
        wk_swapchain_init(alloc, &wk_surface->wk_swapchain,wk_device, wk_output,
            wk_surface->vk_surface)
    );
    return WK_OK;
}


void
wk_surface_cleanup(WallkanInstance *wk_instance, WallkanDevice *wk_device,
    WallkanSurface *wk_surface)
{
    VkSurfaceKHR vk_surface = wk_surface->vk_surface;
    wk_swapchain_cleanup(&wk_surface->wk_swapchain, wk_device);
    if(vk_surface){
        vkDestroySurfaceKHR(wk_instance->vk_instance, vk_surface, NULL);
        wk_surface->vk_surface = VK_NULL_HANDLE;
    }
}
