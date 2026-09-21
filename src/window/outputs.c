#include "window/outputs.h"
#include "common.h"
#include "err.h"
#include "events.h"
#include "window/window.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <wayland-client-protocol.h>
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

void cb_output_geometry(void *data,
		 struct wl_output *wl_output,
		 int32_t x,
		 int32_t y,
		 int32_t physical_width,
		 int32_t physical_height,
		 int32_t subpixel,
		 const char *make,
		 const char *model,
		 int32_t transform)
{
    (void)wl_output;
    (void)x;
    (void)y;
    (void)physical_width;
    (void)physical_height;
    (void)subpixel;
    (void)transform;

    WallkanOutput *wk_output = (WallkanOutput *)data;
    LOG("(CB)output_geometry: Output %u: Make '%s', Model '%s'", wk_output->registry_id, make, model);
}

void cb_output_mode(void *data,
	     struct wl_output *wl_output,
	     uint32_t flags,
	     int32_t width,
	     int32_t height,
	     int32_t refresh)
{
    (void)wl_output;
    (void)refresh;
    WallkanOutput *wk_output = (WallkanOutput *)data;
    if(flags & WL_OUTPUT_MODE_CURRENT){
        LOG("(CB)output_mode: Current output mode for %s is %ux%u", wk_output->name, width, height);
        wk_output->width = width;
        wk_output->height = height;
    }
}

void cb_output_scale(void *data,
	      struct wl_output *wl_output,
	      int32_t factor)
{
    (void)wl_output;
    WallkanOutput *wk_output = (WallkanOutput *)data;
    LOG("(CB)output_mode: Current scale %d for output %s", factor, wk_output->name);
    wk_output->scale = factor;
}

void cb_output_name(void *data,
	     struct wl_output *wl_output,
	     const char *name)
{
    (void)wl_output;
    LOG("(CB)output_mode: Found output %s", name);
    WallkanOutput *wk_output = (WallkanOutput *)data;
    snprintf(wk_output->name, MAX_OUTPUT_NAME_SIZE, "%s", name);
}

void cb_output_description(void *data,
		    struct wl_output *wl_output,
		    const char *description)
{
    (void)data;
    (void)wl_output;
    (void)description;
}

void cb_output_done(void *data,
	     struct wl_output *wl_output)
{
    (void)wl_output;
    WallkanOutput *wk_output = (WallkanOutput *)data;

    if(wk_output->got_details){
        LOG("(CB)output_done: Updated properties of output %s!", wk_output->name);
        wk_ev_handler_emit(wk_output->wk_window->wk_ev_handler, &(WkEvent){
            .type = WK_EVENT_OUTPUT_RECONFIGURED,
            .output_event = (WkOutputEvent){
                .wk_output = wk_output,
            }
        });
        return;
    }
    wk_output->got_details = true;

    WkResult wkres = wk_ev_handler_emit(wk_output->wk_window->wk_ev_handler, &(WkEvent){
        .type = WK_EVENT_OUTPUT_READY,
        .output_event = (WkOutputEvent){
            .wk_output = wk_output,
        }
    });
    if(wkres != WK_OK) return;
    LOG("(CB)output_done: Filled details of output %s!", wk_output->name);
}

struct wl_output_listener output_listener = {
    .geometry = &cb_output_geometry,
    .mode = &cb_output_mode,
    .scale = &cb_output_scale,
    .name = &cb_output_name,
    .description = &cb_output_description,
    .done = &cb_output_done,
};

static void
cb_on_layer_configure(void *data,
                      struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1,
                      uint32_t serial,
                      uint32_t width,
                      uint32_t height)
{
    WallkanOutput *wk_output = (WallkanOutput *)data;

    // 1. Detect if this monitor's resolution actually changed
    if (wk_output->width != width || wk_output->height != height) {

        // If width/height were already non-zero, this is a runtime resize!
        if (wk_output->width != 0 && wk_output->height != 0) {
            WkEvent event = {
                .type = WK_EVENT_RESIZE,
                .resize_event = (WkResizeEvent){
                    .width = width,
                    .height = height,
                    .wk_output = wk_output
                }
            };
            wk_ev_handler_emit(wk_output->wk_window->wk_ev_handler, &event);
        }

        wk_output->width = width;
        wk_output->height = height;
    }

    // 2. Always acknowledge configure
    zwlr_layer_surface_v1_ack_configure(zwlr_layer_surface_v1, serial);
    LOG("Output %u: Acknowledged layer configure @ %ux%u",
        wk_output->registry_id, width, height);
}

static void
cb_on_layer_closed(void *data, struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1)
{
    (void)zwlr_layer_surface_v1;
    WallkanOutput *wk_output = (WallkanOutput*)data;
    WkEvent event = {
        .type = WK_EVENT_CLOSE,
        .close_event = (WkCloseEvent){
            .wk_output = wk_output,
        }
    };
    wk_ev_handler_emit(wk_output->wk_window->wk_ev_handler, &event);
    LOG("(CB)cb_on_layer_closed: ZWLR layer closed!");
}
struct zwlr_layer_surface_v1_listener zwlr_layer_surface_listener = {
    .configure = cb_on_layer_configure,
    .closed    = cb_on_layer_closed
};


static void
cb_on_frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
    WallkanOutput *wk_output = (WallkanOutput *)data;
    ptrdiff_t wk_output_idx = wk_output - wk_output->wk_window->wk_outputs;
    wl_callback_destroy(cb);
    wk_output->frame_cb = NULL;
    wk_output->wk_window->output_frame_ready_mask |= (1 << wk_output_idx);
    wk_output->frame_time_ms = time;
}

static const struct wl_callback_listener frame_listener = {
    .done = cb_on_frame_done,
};

static WallkanOutput *
get_first_inactive_wk_output(WallkanWindow *wk_window)
{
    for (uint32_t i=0; i<MAX_OUTPUTS; i++) {
        if(wk_window->output_is_active_mask & (1 << i)){
            continue;
        }
        if(wk_window->wk_outputs[i].wl_output){
            continue;
        }
        return &wk_window->wk_outputs[i];
    }
    return NULL;
}

bool
wk_output_is_active(WallkanOutput *wk_output)
{
    ptrdiff_t wk_output_idx = wk_output - wk_output->wk_window->wk_outputs;
    return wk_output->wk_window->output_is_active_mask & (1 << wk_output_idx);
}

WkResult
wk_output_add(WallkanWindow *wk_window, uint32_t registry_id, uint32_t version){
    LOG("wk_output_add: Adding output %d...", registry_id);
    WallkanOutput *wk_output = get_first_inactive_wk_output(wk_window);
    if(!wk_output){
        // No error printing. Daemon will just ignore this monitor not crash!
        return WK_ERR_MAX_AMOUNT_MONITORS;
    }
    *wk_output = (WallkanOutput){
        .registry_id = registry_id,
        .scale = 1,
        .wk_window = wk_window
    };
    wk_output->wl_output = wl_registry_bind(wk_window->registry, registry_id, &wl_output_interface,
        MIN(version, (uint32_t)wl_output_interface.version));
    wl_output_add_listener(wk_output->wl_output, &output_listener, wk_output);
    return WK_OK;
}

static WkResult
setup_wayland_surface(WallkanWindow *wk_window, WallkanOutput *wk_output)
{
    LOG("setup_wl_surface: Setting up wayland surface...");
    wk_output->surface = wl_compositor_create_surface(wk_window->compositor);
    if(!wk_output->surface){
        return WK_ERR(WK_ERR_WL_SURFACE_CREATION_FAILURE, "Failed to create wl_surface for monitor %s!",
            wk_output->name);
    }
    return WK_OK;
}

static WkResult
setup_zwlr_layer_surface(WallkanWindow *wk_window, WallkanOutput *wk_output)
{
    // Layer Surface initialization
    LOG("setup_zwlr_layer_surface: Assigning Background role to wl_surface");
    wk_output->layer_surface = zwlr_layer_shell_v1_get_layer_surface(wk_window->layer_shell,
        wk_output->surface, wk_output->wl_output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "wallkan");
    if(!wk_output->layer_surface){
        return WK_ERR(WK_ERR_WL_ZWLR_LAYER_SURFACE_ROLE_FAILURE,
            "Couldn't assign background role to wl_surface of monitor %s!", wk_output->name);
    }

    zwlr_layer_surface_v1_add_listener(wk_output->layer_surface, &zwlr_layer_surface_listener,
        wk_output);
    uint32_t anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                      ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                      ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                      ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
    zwlr_layer_surface_v1_set_anchor(wk_output->layer_surface, anchor);
    zwlr_layer_surface_v1_set_size(wk_output->layer_surface, 0, 0);
    zwlr_layer_surface_v1_set_exclusive_zone(wk_output->layer_surface, -1);
    wl_surface_commit(wk_output->surface);

    if (wl_display_roundtrip(wk_window->display) == -1){
        return WK_ERR(WK_ERR_WL_DISPLAY_ROUNDTRIP_FAILURE,
            "Wayland display roundtrip has failed during layer surface initialization event for monitor %s!",
            wk_output->name);
    }
    LOG("setup_zwlr_layer_surface: Finished setting up zwlr layer surface!");
    return WK_OK;
}

WkResult
wk_output_enable(WallkanOutput *wk_output)
{
    WkResult wkres = WK_OK;

    ptrdiff_t wk_output_idx = wk_output - wk_output->wk_window->wk_outputs;
    if(!wk_output->got_details){
        wkres = WK_ERR(WK_ERR_UNDISCOVERED_MONITOR, "Undiscovered monitor: %zu", wk_output_idx);
        goto err;
    }
    if(wk_output_is_active(wk_output)) return WK_OK;
    wkres = setup_wayland_surface(wk_output->wk_window, wk_output);
    if(wkres != WK_OK) goto err;
    wkres = setup_zwlr_layer_surface(wk_output->wk_window, wk_output);
    if(wkres != WK_OK) goto err;
    wk_output->wk_window->output_is_active_mask |= (1 << wk_output_idx);

    return WK_OK;
err:
    return wkres;
}

void
wk_output_request_frame(WallkanOutput *wk_output)
{
    if (wk_output->frame_cb != NULL) return;
    wk_output->frame_cb = wl_surface_frame(wk_output->surface);
    wl_callback_add_listener(wk_output->frame_cb, &frame_listener, wk_output);
    wl_surface_commit(wk_output->surface);
    return;
}

WkResult
wk_output_disable(WallkanOutput *wk_output)
{
    WkResult wkres = WK_OK;
    ptrdiff_t wk_output_idx = wk_output - wk_output->wk_window->wk_outputs;
    if(!wk_output->got_details){
        wkres = WK_ERR(WK_ERR_UNDISCOVERED_MONITOR, "Undiscovered monitor: %zu",
            wk_output_idx);
        goto err;
    }
    wk_output->wk_window->output_is_active_mask &= ~(1 << wk_output_idx);
    wk_output->wk_window->output_frame_ready_mask &= ~(1 << wk_output_idx);
    if (wk_output->frame_cb) {
        wl_callback_destroy(wk_output->frame_cb);
        wk_output->frame_cb = NULL;
    }
    if (wk_output->layer_surface) {
        zwlr_layer_surface_v1_destroy(wk_output->layer_surface);
        wk_output->layer_surface = NULL;
    }
    if (wk_output->surface) {
        wl_surface_destroy(wk_output->surface);
        wk_output->surface = NULL;
    }
    return WK_OK;
err:
    return wkres;
}

void
wk_output_cleanup(WallkanOutput *wk_output)
{
    if(!wk_output->wl_output) return;
    wk_output_disable(wk_output);
    if(wk_output->frame_cb){
        LOG("window_cleanup: Destroy frame callback...");
        wl_callback_destroy(wk_output->frame_cb);
        wk_output->frame_cb = NULL;
    }
    if(wk_output->layer_surface){
        LOG("window_cleanup: Destroying ZWLR layer surface...");
        zwlr_layer_surface_v1_destroy(wk_output->layer_surface);
        wk_output->layer_surface = NULL;
    }
    if(wk_output->surface){
        LOG("window_cleanup: Destroying Wayland surface...");
        wl_surface_destroy(wk_output->surface);
        wk_output->surface = NULL;
    }
    if (wk_output->wl_output) {
        wl_output_release(wk_output->wl_output);
        wk_output->wl_output = NULL;
    }
    *wk_output = (WallkanOutput){0};
}
