#ifndef WALLKAN_RENDERER_SYNC_H
#define WALLKAN_RENDERER_SYNC_H
#include "err.h"
#include "window/outputs.h"
#include <vulkan/vulkan_core.h>

typedef struct WallkanSync {

} WallkanSync;

WkResult
wk_sync_init(WallkanSync *wk_sync, WallkanWindow *wk_output);

void
wk_sync_cleanup(WallkanSync *wk_sync, WallkanWindow *wk_output);

#endif
