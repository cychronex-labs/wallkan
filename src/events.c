#include <stdint.h>
#include <stdlib.h>
#include "err.h"
#include "events.h"

WkResult
wk_ev_handler_init(WallkanEventHandler *wk_ev_handler)
{
    *wk_ev_handler = (WallkanEventHandler){0};
    wk_ev_handler->events = malloc(QUEUE_SIZE * sizeof(WkEvent));
    if(!wk_ev_handler->events){
        return WK_ERR(WK_ERR_ALLOCATION_ERROR, "Failed to malloc!");
    }
    wk_ev_handler->events_capacity = QUEUE_SIZE;
    return WK_OK;
}

static WkResult
wk_ev_handler_set_callback(WallkanEventHandler *wk_ev_handler, WkEventType event_type,
    WkEventCallback *event_callback, uint32_t callback_idx)
{
    if(event_type >= WK_EVENT_TYPE_COUNT){
        return WK_ERR(WK_ERR_EVENT_HANDLER_UNKNOWN_EVENT,
            "Unknown event type: %d!", event_type);
    }
    if(!event_callback || !event_callback->callback){
        return WK_ERR(WK_ERR_EVENT_HANDLER_CALLBACK_NULL,
            "No callback attached to event %d", event_type);
    }
    if(callback_idx >= MAX_CALLBACK_SLOTS){
        return WK_ERR(WK_ERR_EVENT_HANDLER_MAX_SLOTS,
            "Max amount of callback per event type reached!");
    }
    wk_ev_handler->callbacks[event_type][callback_idx] = *event_callback;
    return WK_OK;
}

WkResult
wk_ev_handler_bind(WallkanEventHandler *wk_ev_handler, WkEventType event_type,
    uint32_t *out_callback_idx, WkEventCallback *event_callback)
{
    WK_TRY(wk_ev_handler_set_callback(wk_ev_handler, event_type, event_callback,
        wk_ev_handler->callback_count[event_type]));
    if(out_callback_idx){
        *out_callback_idx = wk_ev_handler->callback_count[event_type];
    }
    wk_ev_handler->callback_count[event_type]++;
    return WK_OK;
}

WkResult
wk_ev_handler_rebind(WallkanEventHandler *wk_ev_handler, WkEventType event_type,
    uint32_t callback_idx, WkEventCallback *event_callback)
{
    WK_TRY(wk_ev_handler_set_callback(wk_ev_handler, event_type, event_callback,
        callback_idx));
    return WK_OK;
}

WkResult
wk_ev_handler_emit(WallkanEventHandler *wk_ev_handler, const WkEvent *event)
{
    if(event->type >= WK_EVENT_TYPE_COUNT){
        return WK_ERR(WK_ERR_EVENT_HANDLER_UNKNOWN_EVENT,
            "Unknown event type: %d!", event->type);
    }
    uint32_t event_len = wk_ev_handler->events_length+1;
    if(event_len >= wk_ev_handler->events_capacity){
        void *tmp = realloc(wk_ev_handler->events,
            (wk_ev_handler->events_capacity+QUEUE_SIZE) * sizeof(WkEvent));
        if(!tmp){
            return WK_ERR(WK_ERR_ALLOCATION_ERROR, "Failed to realloc!");
        }
        wk_ev_handler->events_capacity+=QUEUE_SIZE;
        wk_ev_handler->events = tmp;
    }
    wk_ev_handler->events[wk_ev_handler->events_length] = *event;
    wk_ev_handler->events_length = event_len;
    return WK_OK;
}

WkResult
wk_ev_handler_dispatch(WallkanEventHandler *wk_ev_handler)
{
    for(uint32_t i=0;i<wk_ev_handler->events_length;i++){
        WkEventType ev_type = wk_ev_handler->events[i].type;
        for(uint32_t j=0;j<wk_ev_handler->callback_count[ev_type];j++){
            WkEventCallback ev_callback = wk_ev_handler->callbacks[ev_type][j];
            WK_TRY(ev_callback.callback(&wk_ev_handler->events[i], ev_callback.data));
        }
    }
    wk_ev_handler_clear(wk_ev_handler);
    return WK_OK;
}

void
wk_ev_handler_clear(WallkanEventHandler *wk_ev_handler)
{
    wk_ev_handler->events_length = 0;
}

void
wk_ev_handler_cleanup(WallkanEventHandler *wk_ev_handler)
{
    if(wk_ev_handler->events){
        free(wk_ev_handler->events);
        wk_ev_handler->events = NULL;
    }
    *wk_ev_handler= (WallkanEventHandler){0};
}
