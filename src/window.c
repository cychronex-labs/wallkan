// window.c
// Responsbilities;
// 1. Wayland setup code
// 2. Wayland event broadcasting
#include <errno.h>
#include <string.h>
#include "common.h"
#include "err.h"
#include "events.h"
#include "window.h"

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
// Wayland zwlr layer callbacks & listeners
static void
cb_on_layer_configure(void *data, struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1,
    uint32_t serial, uint32_t width, uint32_t height)
{
    (void)zwlr_layer_surface_v1;
    WallkanWindow *wk_win = (WallkanWindow*)data;
    if(wk_win->zwlr_layer.width != width || wk_win->zwlr_layer.height != height){
        if(wk_win->zwlr_layer.width != 0 && wk_win->zwlr_layer.height != 0){
            WkEvent event = {
                .type = WK_EVENT_RESIZE,
                .resize_event = (WkResizeEvent){
                    .width = width,
                    .height = height,
                }
            };
            wk_ev_handler_emit(wk_win->wk_ev_handler, &event);
        }
        wk_win->zwlr_layer.width = width;
        wk_win->zwlr_layer.height = height;
    }
    zwlr_layer_surface_v1_ack_configure(wk_win->zwlr_layer.surface, serial);
    LOG("(CB)on_layer_configure: Acknowledged layer configuration @%dx%d", width, height);
}

static void
cb_on_layer_closed(void *data, struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1)
{
    (void)zwlr_layer_surface_v1;
    WallkanWindow *wk_win = (WallkanWindow*)data;
    WkEvent event = {
        .type = WK_EVENT_CLOSE
    };
    wk_ev_handler_emit(wk_win->wk_ev_handler, &event);
    LOG("(CB)cb_on_layer_closed: ZWLR layer closed!");
}

struct zwlr_layer_surface_v1_listener zwlr_layer_surface_listener = {
    .configure = cb_on_layer_configure,
    .closed    = cb_on_layer_closed
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
        wk_win->zwlr_layer.shell = wl_registry_bind(wk_win->registry, name,
            &zwlr_layer_shell_v1_interface,
            MIN(zwlr_layer_shell_v1_interface.version, (int)version));
        LOG("cb_registry_global: Bind registry zwlr_layer_shell...");
    }
    if(strcmp(interface, wl_compositor_interface.name) == 0){
        wk_win->compositor = wl_registry_bind(wk_win->registry, name,&wl_compositor_interface,
            MIN(wl_compositor_interface.version, (int)version));
        LOG("cb_registry_global: Bind registry wl_compositor!");
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
}
static void
cb_registry_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name)
{
    (void)wl_registry;
    (void)data;
    LOG("(CB)registry_global_remove: name: %d", name);
}
static struct wl_registry_listener registry_listener = {
    .global = &cb_registry_global,
    .global_remove = &cb_registry_global_remove
};

static void
cb_on_frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
    WallkanWindow *win = (WallkanWindow *)data;
    wl_callback_destroy(cb);
    win->frame_cb = NULL;
    win->frame_ready = true;
    win->frame_time_ms = time;
}
static const struct wl_callback_listener frame_listener = {
    .done = cb_on_frame_done,
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
            "Wayland display roundtrip has failed during listening registry event!");
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
    if(wk_win->zwlr_layer.shell == NULL){
        return WK_ERR(WK_ERR_WL_GLOBAL_LAYER_SHELL_UNDEFINED,
            "Support for compositors that do not have zwlr-layer-shell is underway!");
    }
    if(wk_win->wl_seat == NULL){
        WARN("validate_registry: wl_seat not found. Mouse interactivity for shaders will not work!");
    }
    LOG("validate_registry: Valiation success!");
    return WK_OK;
}

static WkResult
setup_wayland_surface(WallkanWindow *wk_win)
{
    LOG("setup_wl_surface: Setting up wayland surface...");
    wk_win->surface = wl_compositor_create_surface(wk_win->compositor);
    if(!wk_win->surface){
        return WK_ERR(WK_ERR_WL_SURFACE_CREATION_FAILURE, "Failed to create wl_surface!");
    }
    return WK_OK;
}

static WkResult
setup_zwlr_layer_surface(WallkanWindow *wk_win)
{
    // Layer Surface initialization
    LOG("setup_zwlr_layer_surface: Assigning Background role to wl_surface");
    wk_win->zwlr_layer.surface = zwlr_layer_shell_v1_get_layer_surface(wk_win->zwlr_layer.shell,
        wk_win->surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "wallkan");
    if(!wk_win->zwlr_layer.surface){
        return WK_ERR(WK_ERR_WL_ZWLR_LAYER_SURFACE_ROLE_FAILURE,
            "Couldn't assign background role to surface!");
    }

    zwlr_layer_surface_v1_add_listener(wk_win->zwlr_layer.surface, &zwlr_layer_surface_listener,
        wk_win);
    uint32_t anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                      ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                      ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                      ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
    zwlr_layer_surface_v1_set_anchor(wk_win->zwlr_layer.surface, anchor);
    zwlr_layer_surface_v1_set_size(wk_win->zwlr_layer.surface, 0, 0);
    zwlr_layer_surface_v1_set_exclusive_zone(wk_win->zwlr_layer.surface, -1);
    wl_surface_commit(wk_win->surface);
    if (wl_display_roundtrip(wk_win->display) == -1){
        return WK_ERR(WK_ERR_WL_DISPLAY_ROUNDTRIP_FAILURE,
            "Wayland display roundtrip has failed during layer surface initialization event!");
    }
    LOG("setup_zwlr_layer_surface: Finished setting up zwlr layer surface!");
    return WK_OK;
}

// Global functions
WkResult
window_init(WallkanWindow *wk_win, WallkanEventHandler *wk_ev_handler)
{
    wk_win->wk_ev_handler = wk_ev_handler;
    WK_TRY(connect_wayland_display(wk_win));
    WK_TRY(setup_registry(wk_win));
    WK_TRY(validate_registry(wk_win));
    WK_TRY(setup_wayland_surface(wk_win));
    WK_TRY(setup_zwlr_layer_surface(wk_win));
    return WK_OK;
}

WkResult
window_wl_prepare_read(WallkanWindow *wk_win)
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
        "Wayland compositor crashed on window_wl_prepare_read!");
}

WkResult
window_request_frame(WallkanWindow *win)
{
    if (win->frame_cb != NULL) {
        return WK_OK;
    }
    win->frame_cb = wl_surface_frame(win->surface);
    if (!win->frame_cb) {
        return WK_ERR(WK_ERR_WL_FRAME_CALLBACK_FAILED, "Failed to create wl_surface_frame");
    }
    wl_callback_add_listener(win->frame_cb, &frame_listener, win);
    wl_surface_commit(win->surface);
    return WK_OK;
}

void
window_cleanup(WallkanWindow *wk_win)
{
    if(wk_win->frame_cb){
        LOG("window_cleanup: Destroy frame callback...");
        wl_callback_destroy(wk_win->frame_cb);
        wk_win->frame_cb = NULL;
    }
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
    if(wk_win->zwlr_layer.surface){
        LOG("window_cleanup: Destroying ZWLR layer surface...");
        zwlr_layer_surface_v1_destroy(wk_win->zwlr_layer.surface);
        wk_win->zwlr_layer.surface = NULL;
    }
    if(wk_win->surface){
        LOG("window_cleanup: Destroying Wayland surface...");
        wl_surface_destroy(wk_win->surface);
        wk_win->surface = NULL;
    }
    if(wk_win->zwlr_layer.shell){
        LOG("window_cleanup: Destroying wlr_layer_shell...");
        zwlr_layer_shell_v1_destroy(wk_win->zwlr_layer.shell);
        wk_win->zwlr_layer.shell = NULL;
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
