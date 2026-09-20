#ifndef WALLKAN_WINDOW_H
#define WALLKAN_WINDOW_H
#include <stdint.h>
#include <stdbool.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <linux/input-event-codes.h>
#include "window/outputs.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

typedef struct WallkanEventHandler WallkanEventHandler;
typedef struct WallkanWindow{
    WallkanOutput wk_outputs[MAX_OUTPUTS];
    uint8_t output_frame_ready_mask;
    uint8_t output_is_active_mask;

    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_seat *wl_seat;
    struct zwlr_layer_shell_v1 *layer_shell;
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
} WallkanWindow;

WkResult wk_window_init(WallkanWindow *wk_win, WallkanEventHandler *wk_ev_handler);

WkResult wk_window_wl_prepare_read(WallkanWindow *wk_win);

void wk_window_request_all_frames(WallkanWindow *wk_window);

void wk_window_cleanup(WallkanWindow *wk_win);

#endif
