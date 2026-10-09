#ifndef WALLKAN_H
#define WALLKAN_H
#include <stdbool.h>
#include "arena_alloc.h"
#include "events.h"
#include "ipc/server.h"
#include "renderer/renderer.h"
#include "surface/surface.h"
#include "window/window.h"
#include "wksp/parser.h"

typedef struct Wallkan {
    ArenaAllocator      arena_alloc;
    WallkanWindow       window;
    WallkanSurface      surfaces[MAX_OUTPUTS];
    WallkanRenderer     renderer;
    WallkanEventHandler event_handler;
    WallkanIpc          ipc;
    WKSPParser          wksp_parser;
    bool                running;
} Wallkan;

WkResult
wallkan_stop(Wallkan *wk);

WkResult
wallkan_enable_output(Wallkan *wk, WallkanOutput *wk_output);

WkResult
wallkan_disable_output(Wallkan *wk, WallkanOutput *wk_output);

WkResult
wallkan_remove_output(Wallkan *wk, WallkanOutput *wk_output);

WkResult
wallkan_load_wksp(Wallkan *wk, const char *path);

#endif
