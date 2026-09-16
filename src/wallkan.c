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
#include "window.h"

enum {
    POLL_WAYLAND = 0,
    POLL_IPC,
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
    poll_fds[POLL_IPC] = (struct pollfd){
        .fd = wk->ipc.server_fd,
        .events = POLLIN
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
    if (poll_fds[POLL_IPC].revents & POLLIN) {
        WkResult wkres = wk_ipc_handle_connection(&wk->ipc);
        if(wkres != WK_OK) WARN("check_for_ipc_events: Client connection aborted with code %d", wkres);
    }
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
    return WK_OK;
}

int
main(void)
{
    Wallkan wk = {
        .running = true
    };
    WkResult wkres;

    // Initialize all components
    wkres = wk_ev_handler_init(&wk.event_handler);
    if(wkres != WK_OK) goto cleanup;

    wkres = bind_all_events(&wk);
    if(wkres != WK_OK) goto cleanup;

    wkres = window_init(&wk.window, &wk.event_handler);
    if(wkres != WK_OK) goto cleanup;

    wkres = wk_ipc_init(&wk.ipc, &wk.event_handler);
    if(wkres != WK_OK) goto cleanup;

    // Polling
    struct pollfd poll_fds[POLL_COUNT] = {0};
    setup_polling(&wk, poll_fds);

    // Request the first frame
    wkres = window_request_frame(&wk.window);
    if (wkres != WK_OK) goto cleanup;

    while(wk.running){
        wkres = window_wl_prepare_read(&wk.window);
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

        if (!wk.window.frame_ready) continue;
        wk.window.frame_ready = false;
        LOG("Vsync frame. Timestamp: %u", wk.window.frame_time_ms);
        // Request the next frame
        wkres = window_request_frame(&wk.window);
        if (wkres != WK_OK) goto cleanup;
    }
cleanup:
    window_cleanup(&wk.window);

    wk_ipc_cleanup(&wk.ipc);
    wk_ev_handler_cleanup(&wk.event_handler);
    return (wkres == WK_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
