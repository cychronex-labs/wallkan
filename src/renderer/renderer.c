#include "common.h"
#include "err.h"
#include "renderer/instance.h"
#include <renderer/renderer.h>

WkResult
wk_renderer_init(WallkanRenderer *wk_renderer, const WallkanWindow *wk_window)
{
    WK_TRY(wk_instance_init(&wk_renderer->wk_instance, wk_window));
    return WK_OK;
}

void
wk_renderer_cleanup(WallkanRenderer *wk_renderer)
{
    wk_instance_cleanup(&wk_renderer->wk_instance);
}
