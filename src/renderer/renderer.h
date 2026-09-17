#ifndef WALLKAN_RENDERER_H
#define WALLKAN_RENDERER_H
#include "err.h"
#include "window.h"
#include "renderer/instance.h"

typedef struct WallkanRenderer {
    WallkanInstance wk_instance;
} WallkanRenderer;

WkResult
wk_renderer_init(WallkanRenderer *wk_renderer, const WallkanWindow *wk_window);

void
wk_renderer_cleanup(WallkanRenderer *wk_renderer);

#endif
