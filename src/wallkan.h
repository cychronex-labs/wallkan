#ifndef WALLKAN_H
#define WALLKAN_H
#include <stdbool.h>
#include "events.h"
#include "ipc.h"
#include "window.h"

typedef struct Wallkan {
    WallkanWindow       window;
    WallkanEventHandler event_handler;
    WallkanIpc          ipc;
    bool                running;
} Wallkan;

#endif
