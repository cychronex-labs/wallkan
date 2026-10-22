#include "wallkan.h"

#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <vulkan/vulkan.h>
#include <wayland-client-core.h>

#include "arena_alloc.h"
#include "common.h"
#include "err.h"
#include "events.h"
#include "ipc/server.h"
#include "renderer/renderer.h"
#include "surface/surface.h"
#include "window/outputs.h"
#include "window/window.h"
#include "wksp/parser.h"

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
    wallkan_remove_output(data, event->output_event.wk_output);
    return WK_OK;
}

static void
setup_polling(Wallkan *wk)
{
    wk->poll_fds[POLL_WAYLAND] = (struct pollfd){
        .fd = wl_display_get_fd(wk->window.display),
        .events = POLLIN
    };
    wk->poll_fds[POLL_IPC_SERVER] = (struct pollfd){
        .fd = wk->ipc.server_fd,
        .events = POLLIN
    };
    for (uint32_t i=POLL_IPC_CLIENT_START; i<POLL_COUNT; i++) {
        wk->poll_fds[i] = (struct pollfd){
            .fd = -1,
            .events = POLLIN
        };
    }

}

static WkResult
check_for_wayland_events(Wallkan *wk)
{
    if (wk->poll_fds[POLL_WAYLAND].revents & (POLLHUP | POLLERR)) {
        LOG("Wayland compositor disconnected or crashed.");
        wl_display_cancel_read(wk->window.display);
        // Does not need any data. parameters are to satisfy the event handler
        wallkan_stop(wk);
        return WK_ERR(WK_ERR_WL_COMPOSITOR_DISCONNECTED,
            "Wayland disconnected while polling for events!");
    }
    if(wk->poll_fds[POLL_WAYLAND].revents & POLLIN){
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
check_for_ipc_events(ArenaAllocator *alloc, Wallkan *wk)
{
    if (wk->poll_fds[POLL_IPC_SERVER].revents & POLLIN) {
        uint32_t client_idx = UINT32_MAX;
        WkResult wkres = wk_ipc_accept_connection(wk, &client_idx);
        if(client_idx != UINT32_MAX){
            wk->poll_fds[POLL_IPC_CLIENT_START+client_idx] = (struct pollfd){
                .fd = wk->ipc.client_fd[client_idx],
                .events = POLLIN
            };
        }
        if(wkres != WK_OK) WARN("check_for_ipc_events: Client connection aborted with code %d", wkres);
    }

    uint8_t active_mask = wk->ipc.active_client_bits;
    while(active_mask != 0){
        uint32_t client_idx = bit_pop_lsb(&active_mask);
        uint32_t poll_idx = POLL_IPC_CLIENT_START+client_idx;
        if (wk->poll_fds[poll_idx].revents & POLLIN) {
            WkResult wkres = wk_ipc_read_client(alloc, wk, client_idx);
            if(wkres != WK_OK) WARN("check_for_ipc_events: Client %d read error!", client_idx);
        }
        // In case of client disconnection if we did not update wk->poll_fds but poll()
        // it will result in POLLNVAL which leads to poll to return immediately
        // causing single core usage to spike to 100%
        if(wk->ipc.client_fd[client_idx] == -1){
            wk->poll_fds[poll_idx] = (struct pollfd){
                .fd = -1
            };
        }
        else if (wk->poll_fds[poll_idx].revents & (POLLHUP | POLLERR)) {
            wk_ipc_disconnect_client(&wk->ipc, client_idx);
            wk->poll_fds[poll_idx] = (struct pollfd){
                .fd = -1
            };
        }

    }
}

static WkResult
get_scenes_to_render(Wallkan *wk, uint64_t *out_scenes_to_render)
{
    uint64_t active_mask = wk->renderer.active_scenes;
    while(active_mask != 0){
        uint32_t scene_idx = bit_pop_lsb(&active_mask);
        if (wk->poll_fds[POLL_SCENE_START+scene_idx].revents & POLLIN) {
            (*out_scenes_to_render) |= (1ULL << scene_idx);
            uint64_t rang;
            ssize_t n = read(wk->poll_fds[POLL_SCENE_START+scene_idx].fd, &rang, sizeof(rang));
            if(n < 0){
                if(errno != EINTR && errno != EAGAIN){
                    return WK_ERR(WK_ERR_TIMERFD_READ_FAILURE, "Failed to read timerfd: %d", POLL_SCENE_START+scene_idx);
                }
            }
        }
    }
    return WK_OK;
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

    wkres = wk_ipc_init(&wk.arena_alloc, &wk.ipc, &wk.event_handler);
    if(wkres != WK_OK) goto cleanup;

    wkres = bind_all_events(&wk);
    if(wkres != WK_OK) goto cleanup;

    wkres = wk_window_init(&wk.window, &wk.event_handler);
    if(wkres != WK_OK) goto cleanup;

    wkres = wk_renderer_init(&wk.arena_alloc, &wk.renderer, &wk.window);
    if(wkres != WK_OK) goto cleanup;

    wksp_parser_init(&wk.wksp_parser, &wk.arena_alloc);

    // Polling
    setup_polling(&wk);

    while(wk.running){
        arena_alloc_reset(&wk.arena_alloc);

        wkres = wk_window_wl_prepare_read(&wk.window);
        if(wkres != WK_OK) goto cleanup;

        if(poll(wk.poll_fds, POLL_COUNT, -1) < 0){
            wl_display_cancel_read(wk.window.display);
            if (errno == EINTR) continue;
            wkres = WK_ERR(WK_ERR_POLL_FAILURE,
                "Failure while polling errno: %d msg: %s", errno, strerror(errno));
            goto cleanup;
        }
        wkres = check_for_wayland_events(&wk);
        if(wkres != WK_OK) goto cleanup;

        check_for_ipc_events(&wk.arena_alloc, &wk);

        wkres = wk_ev_handler_dispatch(&wk.event_handler);
        if(wkres != WK_OK) goto cleanup;

        uint64_t render_scenes = 0;
        wkres = get_scenes_to_render(&wk, &render_scenes);
        if (wkres != WK_OK) goto cleanup;

        if (render_scenes == 0) {
            continue;
        }

        wkres = wk_renderer_render(&wk.renderer, render_scenes);
        if (wkres != WK_OK) goto cleanup;

        if (wk.window.output_frame_ready_mask == 0 || wk.window.output_is_active_mask == 0)
            continue;

        uint8_t output_frame_ready_mask = wk.window.output_frame_ready_mask;
        // Iterate maximum until all bits are zero
        while (output_frame_ready_mask != 0) {
            uint32_t output_idx = bit_pop_lsb(&output_frame_ready_mask);
            WallkanSurface *wk_surface = &wk.surfaces[output_idx];
            WallkanOutput *wk_output = &wk.window.wk_outputs[output_idx];

            LOG("Vsync frame for %d.", output_idx);
            if (wk_surface->scene_idx != UINT32_MAX) {
                wk_surface_update(&wk.renderer.wk_scenes[wk_surface->scene_idx]);
            }
            wk_output_request_frame(wk_output);
        }
    }
cleanup:
    wallkan_remove_outputs(&wk);
    wk_renderer_cleanup(&wk.renderer);
    wk_window_cleanup(&wk.window);
    wk_ipc_cleanup(&wk.ipc);
    wk_ev_handler_cleanup(&wk.event_handler);
    arena_alloc_free(&wk.arena_alloc);

    return (wkres == WK_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
