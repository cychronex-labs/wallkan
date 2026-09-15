#include "wallkan.h"
#include "err.h"
#include "events.h"
#include "window.h"

int
main(void)
{
    Wallkan wk = {0};
    WkResult wkres;

    wkres = wk_ev_handler_init(&wk.event_handler);
    if(wkres != WK_OK) goto cleanup;
    wkres = window_init(&wk.window, &wk.event_handler);
    if(wkres != WK_OK) goto cleanup;

cleanup:
    window_cleanup(&wk.window);
    wk_ev_handler_cleanup(&wk.event_handler);
    return (wkres == WK_OK) ? 0 : 1;
}
