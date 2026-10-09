#ifndef WALLKAN_WKSP_PARSER_H
#define WALLKAN_WKSP_PARSER_H
#include "arena_alloc.h"
#include "renderer/device.h"
#include "err.h"
#include "subprojects/yyjson/yyjson.h"
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan_core.h>

#define ERR_INV_WKSP(filename) \
    WK_ERR(WK_ERR_INVALID_WKSP_FILE, "Corrupted WKSP file: %s", (filename))

typedef struct WKSPParser {
    yyjson_alc json_alc;
} WKSPParser;

typedef enum WKSPParamType {
    WKSP_PARAM_TYPE_INT,
    WKSP_PARAM_TYPE_UINT,
    WKSP_PARAM_TYPE_FLOAT,
    WKSP_PARAM_TYPE_BOOL,
    WKSP_PARAM_TYPE_TOTAL_COUNT,
} WKSPParamType;

typedef struct WKSPParameter{
    WKSPParamType type;
    uint32_t components_count;
    union {
        int32_t int32_components[4];
        uint32_t uint32_components[4];
        float float32_components[4];
        bool bool_components[4];
    };
} WKSPParameter;

typedef struct WKSPPass {
    char name[32];
    VkShaderStageFlagBits shader_stage;
    uint32_t channel_count;
    float scale;
    VkFormat selected_img_format;
} WKSPPass;

typedef struct WKSPContainer {
    char name[64];
    WKSPPass *passes;
    WKSPParameter *parameters;
    uint32_t total_passes;
    uint32_t vert_module_idx;
    uint32_t total_parameters;
    VkShaderModule *shader_modules;

    VkDescriptorPoolSize *desc_pool_sizes;
    uint32_t desc_pool_size_count;
    uint32_t desc_max_sets;
} WKSPContainer;

void
wksp_parser_init(WKSPParser *parser, ArenaAllocator *alloc);

WkResult
wksp_load(ArenaAllocator *alloc, const WallkanDevice *wk_device, WKSPParser *parser,
    const char *filename, WKSPContainer *wksp);

void
wksp_destroy(const WallkanDevice *wk_device, WKSPContainer *wksp);

#endif
