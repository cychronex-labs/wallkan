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

static WkResult
exit_wallkan(const WkEvent *event, void *data)
{
    (void)event;
    Wallkan *wk = (Wallkan*)data;
    wk->running = false;
    return WK_OK;
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
        exit_wallkan(NULL, wk);
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
check_for_ipc_events(Wallkan *wk, struct pollfd *poll_fds)
{
    if (poll_fds[POLL_IPC_SERVER].revents & POLLIN) {
        WkResult wkres = wk_ipc_handle_connection(&wk->ipc);
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
handle_ipc_cmd(const WkEvent *event, void *data)
{
    Wallkan *wk = (Wallkan*)data;
    switch (event->ipc_cmd_event.cmd_code){
        case WK_IPC_COMMAND_CODE_QUIT:
            if(wk_ipc_reply_pending(event->ipc_cmd_event.wk_ipc)) {
                wk_ipc_reply(event->ipc_cmd_event.wk_ipc, &(WkIPCReply){
                    .reply_code = WK_IPC_REPLY_OK,
                    .message = "Successfully closed daemon"
                });
            }
            exit_wallkan(NULL, wk);
            break;
        default:
            WARN("handle_ipc_cmd: Received unknown command code %d in WkIPCCommandEvent",
                event->ipc_cmd_event.cmd_code);
            if(wk_ipc_reply_pending(event->ipc_cmd_event.wk_ipc)) {
                wk_ipc_reply(event->ipc_cmd_event.wk_ipc, &(WkIPCReply){
                    .reply_code = WK_IPC_REPLY_INTERNAL_ERROR,
                    .message = "Internal error occurred! IPC command handler got unknown command code"
                });
            }
    }
    return WK_OK;
}

static WkResult
bind_all_events(Wallkan *wk)
{
    WK_TRY(wk_ev_handler_bind(&wk->event_handler, WK_EVENT_CLOSE, NULL,
        &(WkEventCallback){
            .callback = exit_wallkan,
            .data = wk
        }
    ));
    WK_TRY(wk_ev_handler_bind(&wk->event_handler, WK_EVENT_IPC_COMMAND, NULL,
        &(WkEventCallback){
            .callback = handle_ipc_cmd,
            .data = wk
        }
    ));
    return WK_OK;
}

int
main(void)
{
    Wallkan wk = {
        .running = true
    };
    ArenaAllocator arena_alloc = {0};
    WK_TRY(arena_alloc_init(&arena_alloc));
    WkResult wkres;

    // Initialize all components
    wkres = wk_ev_handler_init(&wk.event_handler);
    if(wkres != WK_OK) goto cleanup;

    wkres = wk_ipc_init(&wk.ipc, &wk.event_handler);
    if(wkres != WK_OK) goto cleanup;

    wkres = bind_all_events(&wk);
    if(wkres != WK_OK) goto cleanup;

    wkres = wk_window_init(&wk.window, &wk.event_handler);
    if(wkres != WK_OK) goto cleanup;

    wkres = wk_renderer_init(&arena_alloc, &wk.renderer, &wk.window);
    if(wkres != WK_OK) goto cleanup;

    arena_alloc_free(&arena_alloc);
    // Polling
    struct pollfd poll_fds[POLL_COUNT] = {0};
    setup_polling(&wk, poll_fds);

    wk_window_request_all_frames(&wk.window);
    while(wk.running){
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

        check_for_ipc_events(&wk, poll_fds);

        wkres = wk_ev_handler_dispatch(&wk.event_handler);
        if(wkres != WK_OK) goto cleanup;

        if (wk.window.output_frame_ready_mask == 0) continue;

        // Request the next frame
        wkres = wk_renderer_render(&wk.renderer, &wk.window);
        if (wkres != WK_OK) goto cleanup;
    }
cleanup:
    arena_alloc_free(&arena_alloc);
    wk_renderer_cleanup(&wk.renderer, &wk.window);
    wk_window_cleanup(&wk.window);
    wk_ipc_cleanup(&wk.ipc);
    wk_ev_handler_cleanup(&wk.event_handler);
    return (wkres == WK_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
