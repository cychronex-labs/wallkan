#ifndef WALLKAN_WINDOW_H
#define WALLKAN_WINDOW_H
#include <stdint.h>
#include <stdbool.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <linux/input-event-codes.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "events.h"

typedef struct WallkanWindow{
    struct wl_display    *display;
    struct wl_registry   *registry;
    struct wl_surface    *surface;
    struct wl_compositor *compositor;
    struct wl_seat       *wl_seat;
    struct {
        struct zwlr_layer_shell_v1   *shell;
        struct zwlr_layer_surface_v1 *surface;
        uint32_t                      width;
        uint32_t                      height;
    } zwlr_layer;
    struct {
        struct wl_pointer *mouse;
    } seat_caps;
    struct {
        double x;
        double y;
        double click_x;
        double click_y;
        bool is_down;
    } mouse;
    // Reason: some wayland callback args require event handler but
    // Passing a separate struct for each callback function seemed inconvenient
    // EventHandler is a subsystem owned by the Root Struct
    WallkanEventHandler *wk_ev_handler;
    struct wl_callback *frame_cb;
    bool                frame_ready;
    uint32_t            frame_time_ms;
} WallkanWindow;

WkResult window_init(WallkanWindow *wk_win, WallkanEventHandler *wk_ev_handler);

WkResult window_wl_prepare_read(WallkanWindow *wk_win);

WkResult window_request_frame(WallkanWindow *win);

void window_cleanup(WallkanWindow *wk_win);

#endif
