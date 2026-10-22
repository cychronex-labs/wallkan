#ifndef WALLKAN_RENDERER_SCENE_PUSH_CONSTANTS_H
#define WALLKAN_RENDERER_SCENE_PUSH_CONSTANTS_H
// Technically speaking a bit overkill but well I like keeping organized as much as i can
// That doesn't mean my code is always organized though...
#include <stdint.h>

typedef struct WkPushConstants {
    uint32_t iterations;
} WkPushConstants;

#endif
