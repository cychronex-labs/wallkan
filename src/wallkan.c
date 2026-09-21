#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>
#include <wayland-client-core.h>
#include "wallkan.h"
#include "common.h"
#include "err.h"
#include "events.h"
#include "ipc.h"
#include "renderer/renderer.h"
#include "window/outputs.h"
#include "window/window.h"
#include "arena_alloc.h"

enum {
    POLL_WAYLAND = 0,
    POLL_IPC_SERVER,
    POLL_IPC_CLIENT,
    POLL_COUNT,
};
// Callbacks
static WkResult
cb_on_quit(const WkEvent *event, void *data)
{
    (void)event;
    return wallkan_stop(data);
}
static WkResult
cb_on_output_ready(const WkEvent *event, void *data)
{
    return wallkan_enable_output(data, event->output_event.wk_output);
}
static WkResult
cb_on_output_disable(const WkEvent *event, void *data)
{
    return wallkan_disable_output(data, event->output_event.wk_output);
}
static WkResult
cb_on_output_unplug(const WkEvent *event, void *data)
{
    return wallkan_remove_output(data, event->output_event.wk_output);
}

static void
setup_polling(Wallkan *wk, struct pollfd *poll_fds)
{
    poll_fds[POLL_WAYLAND] = (struct pollfd){
        .fd = wl_display_get_fd(wk->window.display),
        .events = POLLIN
    };
    poll_fds[POLL_IPC_SERVER] = (struct pollfd){
        .fd = wk->ipc.server_fd,
        .events = POLLIN
    };
    poll_fds[POLL_IPC_CLIENT] = (struct pollfd){
        .fd = -1,
        .events = 0
    };
}


static WkResult
check_for_wayland_events(Wallkan *wk, struct pollfd *poll_fds)
{
    if (poll_fds[POLL_WAYLAND].revents & (POLLHUP | POLLERR)) {
        LOG("Wayland compositor disconnected or crashed.");
        wl_display_cancel_read(wk->window.display);
        // Does not need any data. parameters are to satisfy the event handler
        wallkan_stop(wk);
        return WK_ERR(WK_ERR_WL_COMPOSITOR_DISCONNECTED,
            "Wayland disconnected while polling for events!");
    }
    if(poll_fds[POLL_WAYLAND].revents & POLLIN){
        if(wl_display_read_events(wk->window.display) < 0){
            if(errno != EAGAIN){
                return WK_ERR(WK_ERR_WL_COMPOSITOR_DISCONNECTED,
                    "Wayland compositor crashed while reading wayland events!");
            }
        }
        wl_display_dispatch_pending(wk->window.display);
    }else{
        wl_display_cancel_read(wk->window.display);
    }
    return WK_OK;
}

static void
check_for_ipc_events(ArenaAllocator *alloc, Wallkan *wk, struct pollfd *poll_fds)
{
    if (poll_fds[POLL_IPC_SERVER].revents & POLLIN) {
        WkResult wkres = wk_ipc_handle_connection(alloc, wk);
        poll_fds[POLL_IPC_CLIENT] = (struct pollfd){
            .fd = wk->ipc.client_fd,
            .events = 0
        };
        if(wkres != WK_OK) WARN("check_for_ipc_events: Client connection aborted with code %d", wkres);
    }
    if (poll_fds[POLL_IPC_CLIENT].revents & (POLLHUP | POLLERR)) {
        wk_ipc_clean_client_data(&wk->ipc);
        poll_fds[POLL_IPC_CLIENT] = (struct pollfd){
            .fd = -1
        };
    }
}

static WkResult
bind_all_events(Wallkan *wk)
{
    WK_TRY(wk_ev_handler_bind(&wk->event_handler, WK_EVENT_CLOSE, NULL,
        &(WkEventCallback){
            .callback = cb_on_quit,
            .data = wk
        }
    ));
    WK_TRY(wk_ev_handler_bind(&wk->event_handler, WK_EVENT_OUTPUT_READY, NULL,
        &(WkEventCallback){
            .callback = cb_on_output_ready,
            .data = wk
        }
    ));
    WK_TRY(wk_ev_handler_bind(&wk->event_handler, WK_EVENT_OUTPUT_DISABLE, NULL,
        &(WkEventCallback){
            .callback = cb_on_output_disable,
            .data = wk
        }
    ));
    WK_TRY(wk_ev_handler_bind(&wk->event_handler, WK_EVENT_OUTPUT_UNPLUGGED, NULL,
        &(WkEventCallback){
            .callback = cb_on_output_unplug,
            .data = wk
        }
    ));
    return WK_OK;
}

WkResult
wallkan_stop(Wallkan *wk)
{
    wk->running = false;
    return WK_OK;
}

WkResult
wallkan_enable_output(Wallkan *wk, WallkanOutput *wk_output)
{
    WkResult wkres = WK_OK;
    wkres = wk_output_enable(wk_output);
    if(wkres != WK_OK) goto err;
    wkres = wk_renderer_output_init(&wk->arena_alloc, &wk->renderer, wk_output);
    if(wkres != WK_OK) goto err;
    return WK_OK;
err:
    wk_renderer_output_cleanup(&wk->renderer, wk_output);
    wk_output_disable(wk_output);
    return wkres;
}

WkResult
wallkan_disable_output(Wallkan *wk, WallkanOutput *wk_output)
{
    wk_renderer_output_cleanup(&wk->renderer, wk_output);
    WK_TRY(
        wk_output_disable(wk_output)
    );
    return WK_OK;
}

WkResult
wallkan_remove_output(Wallkan *wk, WallkanOutput *wk_output)
{
    wk_renderer_output_cleanup(&wk->renderer, wk_output);
    wk_output_cleanup(wk_output);
    return WK_OK;
}

int
main(void)
{
    Wallkan wk = {
        .running = true
    };

    WkResult wkres = WK_OK;

    wkres = arena_alloc_init(&wk.arena_alloc);
    if(wkres != WK_OK) goto cleanup;

    // Initialize all components
    wkres = wk_ev_handler_init(&wk.event_handler);
    if(wkres != WK_OK) goto cleanup;

    wkres = wk_ipc_init(&wk.ipc, &wk.event_handler);
    if(wkres != WK_OK) goto cleanup;

    wkres = bind_all_events(&wk);
    if(wkres != WK_OK) goto cleanup;

    wkres = wk_instance_init(&wk.arena_alloc, &wk.renderer.wk_instance);
    if(wkres != WK_OK) goto cleanup;

    wkres = wk_window_init(&wk.window, &wk.event_handler);
    if(wkres != WK_OK) goto cleanup;

    // Window initialization generates events related to output
    // It must be consumed before renderer initialization
    wkres = wk_ev_handler_dispatch(&wk.event_handler);
    if(wkres != WK_OK) goto cleanup;


    wkres = wk_renderer_init(&wk.arena_alloc, &wk.renderer, &wk.window);
    if(wkres != WK_OK) goto cleanup;

    // Polling
    struct pollfd poll_fds[POLL_COUNT] = {0};
    setup_polling(&wk, poll_fds);

    wk_window_request_all_frames(&wk.window);
    while(wk.running){
        arena_alloc_reset(&wk.arena_alloc);

        poll_fds[POLL_IPC_CLIENT].fd = wk.ipc.client_fd;
        wkres = wk_window_wl_prepare_read(&wk.window);
        if(wkres != WK_OK) goto cleanup;

        if(poll(poll_fds, POLL_COUNT, -1) < 0){
            wl_display_cancel_read(wk.window.display);
            if (errno == EINTR) continue;
            wkres = WK_ERR(WK_ERR_POLL_FAILURE,
                "Failure while polling errno: %d msg: %s", errno, strerror(errno));
            goto cleanup;
        }

        wkres = check_for_wayland_events(&wk, poll_fds);
        if(wkres != WK_OK) goto cleanup;

        check_for_ipc_events(&wk.arena_alloc, &wk, poll_fds);

        wkres = wk_ev_handler_dispatch(&wk.event_handler);
        if(wkres != WK_OK) goto cleanup;

        if (wk.window.output_frame_ready_mask == 0 || wk.window.output_is_active_mask == 0)
            continue;

        // Request the next frame
        wkres = wk_renderer_render(&wk.renderer, &wk.window);
        if (wkres != WK_OK) goto cleanup;
    }
cleanup:
    arena_alloc_free(&wk.arena_alloc);
    wk_renderer_cleanup(&wk.renderer, &wk.window);
    wk_window_cleanup(&wk.window);
    wk_ipc_cleanup(&wk.ipc);
    wk_ev_handler_cleanup(&wk.event_handler);
    return (wkres == WK_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
