#ifndef WALLKAN_H
#define WALLKAN_H
#include <stdbool.h>
#include "arena_alloc.h"
#include "events.h"
#include "ipc.h"
#include "renderer/renderer.h"
#include "window/window.h"

typedef struct Wallkan {
    ArenaAllocator      arena_alloc;
    WallkanWindow       window;
    WallkanRenderer     renderer;
    WallkanEventHandler event_handler;
    WallkanIpc          ipc;
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

#endif
