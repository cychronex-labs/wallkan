#include "wallkan.h"

#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "err.h"
#include "surface/surface.h"
#include "window/outputs.h"

WkResult
wallkan_enable_output(Wallkan *wk, WallkanOutput *wk_output)
{
    WkResult wkres = WK_OK;
    if(!wk_output->got_details) return WK_ERR(WK_ERR_UNDISCOVERED_MONITOR, "Monitor/Output is not discovered!");
    WK_TRY(wk_output_enable(wk_output));

    ptrdiff_t output_idx = wk_output - wk_output->wk_window->wk_outputs;
    wkres = wk_surface_init(&wk->arena_alloc, &wk->renderer.wk_instance, &wk->renderer.wk_device,
        wk_output, &wk->surfaces[output_idx]);
    if(wkres != WK_OK) goto err;

    return WK_OK;
err:
    wk_surface_cleanup(&wk->renderer.wk_instance, &wk->renderer.wk_device, &wk->surfaces[output_idx]);
    wk_output_disable(wk_output);
    return wkres;
}

WkResult
wallkan_disable_output(Wallkan *wk, WallkanOutput *wk_output)
{
    ptrdiff_t output_idx = wk_output - wk_output->wk_window->wk_outputs;
    wk_surface_cleanup(&wk->renderer.wk_instance, &wk->renderer.wk_device,
        &wk->surfaces[output_idx]);
    WK_TRY(
        wk_output_disable(wk_output)
    );
    return WK_OK;
}

void
wallkan_remove_output(Wallkan *wk, WallkanOutput *wk_output)
{
    ptrdiff_t output_idx = wk_output - wk_output->wk_window->wk_outputs;
    wk_surface_cleanup(&wk->renderer.wk_instance, &wk->renderer.wk_device,
        &wk->surfaces[output_idx]);
    wk_output_cleanup(wk_output);
}

void
wallkan_remove_outputs(Wallkan *wk)
{
    uint8_t active_mask = wk->window.output_is_active_mask;
    while(active_mask){
        uint32_t output_idx = bit_pop_lsb(&active_mask);
        wallkan_remove_output(wk, &wk->window.wk_outputs[output_idx]);
    }
}
