#ifndef WALLKAN_EVENT_HANDLER_H
#define WALLKAN_EVENT_HANDLER_H
#include <stdint.h>
#include "err.h"
#define QUEUE_SIZE 16
#define MAX_CALLBACK_SLOTS 4

typedef enum WkEventType {
    WK_EVENT_NONE = 0,
    WK_EVENT_RESIZE,
    WK_EVENT_CLOSE,
    WK_EVENT_TYPE_COUNT
} WkEventType;

typedef struct WkResizeEvent {
    uint32_t width;
    uint32_t height;
} WkResizeEvent;

typedef struct WkEvent {
    WkEventType type;
    union {
        WkResizeEvent resize_event;
    };
} WkEvent;

typedef struct WkEventCallback WkEventCallback;

struct WkEventCallback {
    WkResult (*callback)(const WkEvent*, void*data);
    void *data;
};

typedef struct WallkanEventHandler {
    WkEvent *events;
    uint32_t events_length;
    uint32_t events_capacity;
    WkEventCallback callbacks[WK_EVENT_TYPE_COUNT][MAX_CALLBACK_SLOTS];
    uint32_t callback_count[WK_EVENT_TYPE_COUNT];
} WallkanEventHandler;

WkResult
wk_ev_handler_init(WallkanEventHandler *wk_ev_handler);

WkResult
wk_ev_handler_bind(WallkanEventHandler *wk_ev_handler, WkEventType event_type,
    WkEventCallback *event_callback, uint32_t *out_callback_idx);

WkResult
wk_ev_handler_rebind(WallkanEventHandler *wk_ev_handler, WkEventType event_type,
    WkEventCallback *event_callback, uint32_t callback_idx);

WkResult
wk_ev_handler_emit(WallkanEventHandler *wk_ev_handler, const WkEvent *event);

WkResult
wk_ev_handler_dispatch(WallkanEventHandler *wk_ev_handler);

void
wk_ev_handler_clear(WallkanEventHandler *wk_ev_handler);

void
wk_ev_handler_cleanup(WallkanEventHandler *wk_ev_handler);
#endif
