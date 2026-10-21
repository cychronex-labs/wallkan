#ifndef WALLKAN_H
#define WALLKAN_H
#include <stdbool.h>
#include <sys/poll.h>
#include "arena_alloc.h"
#include "events.h"
#include "ipc/server.h"
#include "renderer/renderer.h"
#include "surface/surface.h"
#include "window/window.h"
#include "wksp/parser.h"


enum {
    POLL_WAYLAND,
    POLL_IPC_SERVER,
    POLL_IPC_CLIENT_START,
    POLL_SCENE_START = POLL_IPC_CLIENT_START + MAX_IPC_CLIENTS,
    POLL_COUNT = POLL_SCENE_START + MAX_SCENES
};

typedef struct Wallkan {
    ArenaAllocator      arena_alloc;
    WallkanWindow       window;
    WallkanSurface      surfaces[MAX_OUTPUTS];
    WallkanRenderer     renderer;
    WallkanEventHandler event_handler;
    WallkanIpc          ipc;
    WKSPParser          wksp_parser;
    struct pollfd       poll_fds[POLL_COUNT];
    bool                running;
} Wallkan;

WkResult
wallkan_stop(Wallkan *wk);

WkResult
wallkan_enable_output(Wallkan *wk, WallkanOutput *wk_output);

WkResult
wallkan_disable_output(Wallkan *wk, WallkanOutput *wk_output);

void
wallkan_remove_output(Wallkan *wk, WallkanOutput *wk_output);

void
wallkan_remove_outputs(Wallkan *wk);

WkResult
wallkan_load_scene(Wallkan *wk, const char *wksp_path, uint32_t fps, VkExtent2D extent);

#endif
