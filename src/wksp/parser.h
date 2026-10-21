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


typedef enum WkChannelType {
    WK_CHANNEL_TYPE_SHADER_PASS,
    WK_CHANNEL_TYPE_SHADER_ASSET
} WkChannelType;

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
        uint32_t bool_components[4];
    };
} WKSPParameter;

typedef struct WKSPPassDescSet {
    uint32_t set_idx;
    VkDescriptorType type;
    uint32_t binding_count;
} WKSPPassDescSet;


typedef struct WKSPPassChannel {
    const char *name;
    WkChannelType type;
    VkDescriptorType desc_type;
    uint32_t producer_idx;
    // We store just the create info so each scene can own its own samplers
    // So we prevent ownership clashes between scenes
    VkSamplerCreateInfo sampler_create_info;
} WKSPPassChannel;

typedef struct WKSPPass {
    const char *name;
    VkShaderStageFlagBits shader_stage;
    uint32_t channel_count;
    float scale;
    VkFormat selected_img_format;
    uint32_t descriptor_set_count;
    WKSPPassChannel *channels;
    uint32_t iterations;
    bool self_feedback;
} WKSPPass;


typedef struct WKSPContainer {
    char name[64];
    WKSPPass *passes;
    WKSPParameter *parameters;
    uint32_t total_passes;
    uint32_t vert_module_idx;
    uint32_t total_parameters;
    uint32_t total_parameters_size;
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
