// window.c
// Responsbilities;
// 1. Wayland setup code
// 2. Wayland event broadcasting
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "common.h"
#include "err.h"
#include "window/outputs.h"
#include "window/window.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

// Wayland Pointer Callbacks & Listener
static void
cb_pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial,
    struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    (void)pointer;
    (void)serial;
    (void)surface;
    WallkanWindow *wk_win = (WallkanWindow *)data;
    wk_win->mouse.x = wl_fixed_to_double(surface_x);
    wk_win->mouse.y = wl_fixed_to_double(surface_y);
}

static void
cb_pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time,
    wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    (void)pointer;
    (void)time;
    WallkanWindow *wk_win = (WallkanWindow *)data;
    wk_win->mouse.x = wl_fixed_to_double(surface_x);
    wk_win->mouse.y = wl_fixed_to_double(surface_y);
}

static void
cb_pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial,
    uint32_t time, uint32_t button, uint32_t state)
{
    (void)pointer;
    (void)serial;
    (void)time;
    WallkanWindow *wk_win = (WallkanWindow *)data;
    if (button == BTN_LEFT) {
        if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
            wk_win->mouse.is_down = true;
            wk_win->mouse.click_x = wk_win->mouse.x;
            wk_win->mouse.click_y = wk_win->mouse.y;
        } else if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
            wk_win->mouse.is_down = false;
        }
    }
}
static void cb_pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface) {
    (void)data;
    (void)pointer;
    (void)serial;
    (void)surface;
}
static void cb_pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
    (void)data;
    (void)pointer;
    (void)time;
    (void)axis;
    (void)value;
}
static void cb_pointer_frame(void *data, struct wl_pointer *pointer) {
    (void)data;
    (void)pointer;
}
static void cb_pointer_axis_source(void *data, struct wl_pointer *pointer, uint32_t axis_source) {
    (void)data;
    (void)pointer;
    (void)axis_source;
}
static void cb_pointer_axis_stop(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis) {
    (void)data;
    (void)pointer;
    (void)time;
    (void)axis;
}
static void cb_pointer_axis_discrete(void *data, struct wl_pointer *pointer, uint32_t axis, int32_t discrete) {
    (void)data;
    (void)pointer;
    (void)axis;
    (void)discrete;
}
static struct wl_pointer_listener mouse_listener = {
    .enter         = &cb_pointer_enter,
    .leave         = &cb_pointer_leave,
    .motion        = &cb_pointer_motion,
    .button        = &cb_pointer_button,
    .axis          = &cb_pointer_axis,
    .frame         = &cb_pointer_frame,
    .axis_source   = &cb_pointer_axis_source,
    .axis_stop     = &cb_pointer_axis_stop,
    .axis_discrete = &cb_pointer_axis_discrete,
};
// Wayland wl_seat callbacks & listener
static void
cb_on_wl_seat_capability(void *data, struct wl_seat *wl_seat, uint32_t capabilities)
{
    WallkanWindow *wk_win = (WallkanWindow*)data;
    const bool has_pointer = (capabilities & WL_SEAT_CAPABILITY_POINTER) != 0;
    if (has_pointer && !wk_win->seat_caps.mouse) {
        wk_win->seat_caps.mouse = wl_seat_get_pointer(wl_seat);
        wl_pointer_add_listener(wk_win->seat_caps.mouse, &mouse_listener, wk_win);
        LOG("(CB)on_wl_seat_capability: Mouse pointer capability bound.");
    } else if (!has_pointer && wk_win->seat_caps.mouse) {
        wl_pointer_release(wk_win->seat_caps.mouse);
        wk_win->seat_caps.mouse = NULL;
        LOG("(CB)on_wl_seat_capability: Mouse pointer capability released.");
    }
}
static void
cb_on_wl_seat_name(void *data, struct wl_seat *wl_seat, const char *name)
{
    (void)data;
    (void)wl_seat;
    LOG("(CB)seat_name: Bound to seat '%s'", name);
}
struct wl_seat_listener wl_seat_listener_struct = {
    .capabilities = &cb_on_wl_seat_capability,
    .name = &cb_on_wl_seat_name
};

// Wayland registry callbacks % listener
static void
cb_registry_global(void *data, struct wl_registry *wl_registry, uint32_t name,
    const char *interface, uint32_t version)
{
    (void)wl_registry;
    WallkanWindow *wk_win = (WallkanWindow*)data;
    LOG("(CB)registry_global: name: %d - interface: %s - version: %d", name, interface, version);
    if(strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0){
        wk_win->layer_shell = wl_registry_bind(wk_win->registry, name,
            &zwlr_layer_shell_v1_interface,
            MIN(zwlr_layer_shell_v1_interface.version, (int)version));
        LOG("(CB)registry_global: Bind registry zwlr_layer_shell...");
    }
    if(strcmp(interface, wl_compositor_interface.name) == 0){
        wk_win->compositor = wl_registry_bind(wk_win->registry, name,&wl_compositor_interface,
            MIN(wl_compositor_interface.version, (int)version));
        LOG("(CB)registry_global: Bind registry wl_compositor!");
    }
    if(strcmp(interface, wl_seat_interface.name) == 0){
        // Why 5 here? Apparently using the usual MIN resulted in:
        // listener function for opcode 10 of wl_pointer is NULL
        // Which that opcode/event was added in version 9. so to maintain compatibility force it to 5
        wk_win->wl_seat = wl_registry_bind(wk_win->registry, name,&wl_seat_interface,
            MIN(5, (int)version));
        wl_seat_add_listener(wk_win->wl_seat, &wl_seat_listener_struct, wk_win);
        LOG("(CB)registry_global: Bind registry wl_seat!");
    }
    if (strcmp(interface, wl_output_interface.name) == 0) {
        LOG("(CB)registry_global: Found output (name %u)", name);
        WkResult wkres = wk_output_add(wk_win, name, version);
        if(wkres == WK_ERR_MAX_AMOUNT_MONITORS){
            WARN("(CB)registry_global: Maximum supported amount of monitors reached. Monitor %u will be ignored!", name);
        }
    }
}
static void
cb_registry_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name)
{
    (void)wl_registry;
    WallkanWindow *wk_window = (WallkanWindow*)data;
    for (uint32_t i=0; i<MAX_OUTPUTS; i++) {
        WallkanOutput *wk_output = &wk_window->wk_outputs[i];
        if (wk_output->registry_id == name) {
            wk_output_cleanup(wk_output);
            break;
        }
    }
}
static struct wl_registry_listener registry_listener = {
    .global = &cb_registry_global,
    .global_remove = &cb_registry_global_remove
};

// Helper functions
static WkResult
connect_wayland_display(WallkanWindow *wk_win)
{
    wk_win->display = wl_display_connect(NULL);
    if(!wk_win->display){
        return WK_ERR(WK_ERR_WL_DISPLAY_CONNECT_FAILURE,
            "Failed to connect wayland display! Are you running wayland?");
    }
    LOG("connect_wayland_display: Connected to wayland display!");
    return WK_OK;
}

static WkResult
setup_registry(WallkanWindow *wk_win)
{
    LOG("setup_registry: Setting up registry...");
    wk_win->registry = wl_display_get_registry(wk_win->display);
    if(!wk_win->registry){
        return WK_ERR(WK_ERR_WL_NULL_REGISTRY, "Failed to get registry!");
    }
    wl_registry_add_listener(wk_win->registry, &registry_listener, wk_win);
    if(wl_display_roundtrip(wk_win->display) == -1){
        return WK_ERR(WK_ERR_WL_REGISTRY_ROUNDTRIP_FAILURE,
            "Wayland display roundtrip 1 has failed during listening events!");
    };
    if(wl_display_roundtrip(wk_win->display) == -1){
        return WK_ERR(WK_ERR_WL_REGISTRY_ROUNDTRIP_FAILURE,
            "Wayland display roundtrip 2 has failed during listening events!");
    };
    return WK_OK;
}

static WkResult
validate_registry(WallkanWindow *wk_win)
{
    LOG("validate_registry: Valiating required registries...");
    if(wk_win->compositor == NULL){
        return WK_ERR(WK_ERR_WL_GLOBAL_COMPOSITOR_UNDEFINED,
            "Broken wayland compositor, wl_compositor was not announced!");
    }
    if(wk_win->layer_shell == NULL){
        return WK_ERR(WK_ERR_WL_GLOBAL_LAYER_SHELL_UNDEFINED,
            "Support for compositors that do not have zwlr-layer-shell is underway!");
    }
    if(wk_win->wl_seat == NULL){
        WARN("validate_registry: wl_seat not found. Mouse interactivity for shaders will not work!");
    }
    LOG("validate_registry: Valiation success!");
    return WK_OK;
}



// Global functions
WkResult
wk_window_init(WallkanWindow *wk_win, WallkanEventHandler *wk_ev_handler)
{
    wk_win->wk_ev_handler = wk_ev_handler;
    for (uint32_t i = 0; i < MAX_OUTPUTS; i++) {
        wk_win->wk_outputs[i].wk_window = wk_win;
    }
    WK_TRY(connect_wayland_display(wk_win));
    WK_TRY(setup_registry(wk_win));
    WK_TRY(validate_registry(wk_win));
    return WK_OK;
}

WkResult
wk_window_wl_prepare_read(WallkanWindow *wk_win)
{
    while (wl_display_prepare_read(wk_win->display) != 0) {
        if(errno != EAGAIN) goto err;
        wl_display_dispatch_pending(wk_win->display);
    }
    if(wl_display_flush(wk_win->display) < 0 && errno != EAGAIN){
        wl_display_cancel_read(wk_win->display);
        goto err;
    }
    return WK_OK;

err:
    return WK_ERR(WK_ERR_WL_COMPOSITOR_DISCONNECTED,
        "Wayland compositor crashed on wk_window_wl_prepare_read!");
}

void
wk_window_request_all_frames(WallkanWindow *wk_window)
{
    uint8_t active_mask = wk_window->output_is_active_mask;
    // Iterate maximum until all bits are zero
    while (active_mask != 0) {
        uint32_t output_idx = bit_pop_lsb(&active_mask);
        wk_output_request_frame(&wk_window->wk_outputs[output_idx]);
    }
}

void
wk_window_cleanup(WallkanWindow *wk_win)
{
    if (wk_win->seat_caps.mouse) {
        LOG("window_cleanup: Releasing mouse pointer...");
        wl_pointer_release(wk_win->seat_caps.mouse);
        wk_win->seat_caps.mouse = NULL;
    }
    if (wk_win->wl_seat) {
        LOG("window_cleanup: Releasing wl_seat...");
        wl_seat_release(wk_win->wl_seat);
        wk_win->wl_seat = NULL;
    }
    for (uint32_t i=0; i<MAX_OUTPUTS; i++) {
        wk_output_cleanup(&wk_win->wk_outputs[i]);
    }
    if(wk_win->layer_shell){
        LOG("window_cleanup: Destroying wlr_layer_shell...");
        zwlr_layer_shell_v1_destroy(wk_win->layer_shell);
        wk_win->layer_shell = NULL;
    }
    if(wk_win->registry){
        LOG("window_cleanup: Destroying wayland registry...");
        wl_registry_destroy(wk_win->registry);
        wk_win->registry = NULL;
    }
    if(wk_win->compositor){
        LOG("window_cleanup: Destroying wayland compositor...");
        wl_compositor_destroy(wk_win->compositor);
        wk_win->compositor = NULL;
    }
    if(wk_win->display){
        LOG("window_cleanup: Severing wayland connection...");
        wl_display_disconnect(wk_win->display);
        wk_win->display = NULL;
    }
}
