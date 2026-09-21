#ifndef WALLKAN_H
#define WALLKAN_H
#include <stdbool.h>
#include "events.h"
#include "ipc.h"
#include "renderer/renderer.h"
#include "window/window.h"

typedef struct Wallkan {
    WallkanWindow       window;
    WallkanRenderer     renderer;
    WallkanEventHandler event_handler;
    WallkanIpc          ipc;
    bool                running;
} Wallkan;

WkResult
wallkan_stop(const WkEvent *event, void *data);

#endif
