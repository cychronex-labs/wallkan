#include "arena_alloc.h"
#include "common.h"
#include "err.h"
#include "renderer/device.h"
#include "renderer/instance.h"
#include "renderer/swapchain.h"
#include "window/outputs.h"
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan_core.h>
#include "renderer/renderer.h"

WkResult
wk_renderer_init(ArenaAllocator *alloc, WallkanRenderer *wk_renderer, WallkanWindow *wk_window)
{
    WK_TRY(wk_instance_init(alloc, &wk_renderer->wk_instance));
    if(wk_window->output_is_active_mask==0){
        return WK_ERR(WK_ERR_NO_ACTIVE_MONITORS_FOUND,
            "No active monitors found");
    }
    uint8_t active_mask = wk_window->output_is_active_mask;
    // Initialize vk surface for each active monitor
    while (active_mask != 0) {
        uint32_t output_idx = bit_pop_lsb(&active_mask);
        WK_TRY(wk_instance_init_surface(&wk_renderer->wk_instance, wk_window,
            &wk_window->wk_outputs[output_idx], &wk_renderer->vk_surfaces[output_idx]));
    }

    uint32_t first_active_idx = (uint32_t)__builtin_ctz(wk_window->output_is_active_mask);
    WK_TRY(wk_device_init(alloc, &wk_renderer->wk_device, &wk_renderer->wk_instance,
        wk_renderer->vk_surfaces[first_active_idx]));

    // Initialize swapchain for each active monitor
    active_mask = wk_window->output_is_active_mask;
    while (active_mask != 0) {
        uint32_t output_idx = bit_pop_lsb(&active_mask);
        WK_TRY(
            wk_swapchain_init(alloc, &wk_renderer->wk_swapchain[output_idx],
                &wk_renderer->wk_device,&wk_window->wk_outputs[output_idx],
                wk_renderer->vk_surfaces[output_idx])
        );
    }
    return WK_OK;
}

WkResult
wk_renderer_output_init(ArenaAllocator *alloc, WallkanRenderer *wk_renderer,
    WallkanOutput *wk_output)
{
    ptrdiff_t output_idx = wk_output - wk_output->wk_window->wk_outputs;

    WK_TRY(
        wk_instance_init_surface(&wk_renderer->wk_instance, wk_output->wk_window,
            wk_output, &wk_renderer->vk_surfaces[output_idx])
    );
    WK_TRY(
        wk_swapchain_init(alloc, &wk_renderer->wk_swapchain[output_idx],
            &wk_renderer->wk_device, wk_output, wk_renderer->vk_surfaces[output_idx])
    );
    return WK_OK;
}

void
wk_renderer_output_cleanup(WallkanRenderer *wk_renderer, WallkanOutput *wk_output)
{
    ptrdiff_t output_idx = wk_output - wk_output->wk_window->wk_outputs;
    VkSurfaceKHR vk_surface = wk_renderer->vk_surfaces[output_idx];
    wk_swapchain_cleanup(&wk_renderer->wk_swapchain[output_idx], &wk_renderer->wk_device);
    if(vk_surface){
        vkDestroySurfaceKHR(wk_renderer->wk_instance.vk_instance, vk_surface, NULL);
        wk_renderer->vk_surfaces[output_idx] = VK_NULL_HANDLE;
    }
}

WkResult
wk_renderer_render(WallkanRenderer *wk_renderer, WallkanWindow *wk_window)
{
    uint8_t active_mask = wk_window->output_is_active_mask;
    // Iterate maximum until all bits are zero
    while (active_mask != 0) {
        uint32_t output_idx = bit_pop_lsb(&active_mask);
        WallkanOutput *wk_output = &wk_window->wk_outputs[output_idx];
        if(!(wk_window->output_frame_ready_mask & (1 << output_idx))){
            continue;
        }
        LOG("Vsync frame. Timestamp: %u", wk_output->frame_time_ms);
        // Switch this output bit into zero
        wk_window->output_frame_ready_mask &= ~(1 << output_idx);
        wk_output_request_frame(wk_output);
    }
    return WK_OK;
}

void
wk_renderer_cleanup(WallkanRenderer *wk_renderer, WallkanWindow *wk_window)
{
    uint8_t active_mask = wk_window->output_is_active_mask;
    // Iterate maximum until all bits are zero
    while (active_mask != 0) {
        uint32_t output_idx = bit_pop_lsb(&active_mask);
        wk_swapchain_cleanup(&wk_renderer->wk_swapchain[output_idx], &wk_renderer->wk_device);
        if(wk_renderer->vk_surfaces[output_idx]){
            LOG("wk_renderer_cleanup: Destroying vulkan KHR surface...");
            vkDestroySurfaceKHR(wk_renderer->wk_instance.vk_instance,
                wk_renderer->vk_surfaces[output_idx], NULL);
            wk_renderer->vk_surfaces[output_idx] = VK_NULL_HANDLE;
        }
    }
    wk_device_cleanup(&wk_renderer->wk_device);
    wk_instance_cleanup(&wk_renderer->wk_instance);
}
