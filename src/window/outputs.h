#ifndef WALLKAN_OUTPUTS_H
#define WALLKAN_OUTPUTS_H
#include <stdint.h>
#include <stdbool.h>
#include <wayland-client-protocol.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "err.h"

#define MAX_OUTPUTS 8
#define MAX_OUTPUT_NAME_SIZE 128

typedef struct WallkanWindow WallkanWindow;
typedef struct WallkanIpc WallkanIpc;
typedef struct WallkanOutput {
    char name[MAX_OUTPUT_NAME_SIZE];
    uint32_t registry_id;
    WallkanWindow *wk_window;
    struct wl_output *wl_output;
    struct wl_surface *surface;

    struct zwlr_layer_surface_v1 *layer_surface;

    uint32_t width;
    uint32_t height;
    uint32_t scale;

    struct wl_callback *frame_cb;
    uint32_t frame_time_ms;
    bool got_details;
} WallkanOutput;

typedef struct WallkanWindow WallkanWindow;

bool wk_output_is_active(WallkanOutput *wk_output);

WkResult wk_output_add(WallkanWindow *win, uint32_t name, uint32_t version);
WkResult wk_output_enable(WallkanOutput *wk_output);
WkResult wk_output_disable(WallkanOutput *wk_output);
void wk_output_request_frame(WallkanOutput *wk_output);
WkResult wk_output_remove(WallkanWindow *wk_win, WallkanOutput *wk_output);
void wk_output_cleanup(WallkanOutput *wk_output);

#endif
