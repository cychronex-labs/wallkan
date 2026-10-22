#include "wksp/parser.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <vulkan/vulkan.h>
#include <yyjson.h>

#include "arena_alloc.h"
#include "common.h"
#include "err.h"
#include "renderer/device.h"
#include "yyjson_alc.h"

void
wksp_parser_init(WKSPParser *parser, ArenaAllocator *alloc)
{
    parser->json_alc = wk_yyjson_create_pool(alloc);
}

static WkResult
read_file(ArenaAllocator *alloc, const char *filename, size_t *out_size, void **out_data)
{
    WkResult wkres = WK_OK;
    FILE *file = fopen(filename, "rb");
    if (!file) {
        return WK_ERR(WK_ERR_FOPEN_FAILURE, "Failed to open file: %s", filename);
    }
    if(fseek(file, 0, SEEK_END) != 0){
        wkres = WK_ERR(WK_ERR_FOPEN_FAILURE, "fseek failed on: %s", filename);
        goto cleanup;
    }
    long ftell_out = ftell(file);
    if (ftell_out < 0) {
        wkres = WK_ERR(WK_ERR_FTELL_FAILURE, "ftell failed on: %s", filename);
        goto cleanup;
    }
    size_t size = (size_t)ftell_out;
    void *wksp_data_ptr = arena_alloc(alloc, size);
    if(fseek(file, 0, SEEK_SET) != 0){
        wkres = WK_ERR(WK_ERR_FOPEN_FAILURE, "fseek failed on: %s", filename);
        goto cleanup;
    }
    size_t bytes_read = fread(wksp_data_ptr, 1, size, file);
    if(bytes_read != size) {
        wkres = WK_ERR(WK_ERR_FILE_SIZE_MISMATCH, "Read bytes and the ftell size does not match!");
    }
    *out_size = size;
    *out_data = wksp_data_ptr;
cleanup:
    fclose(file);
    return wkres;
}

static WkResult
wksp_validate(uint32_t *wksp_data_ptr, size_t file_size, const char *filename, uint32_t *out_json_size,
    char **json_start_ptr, uint32_t **sprv_count_start)
{
    LOG("wksp_parse: Validating wksp file...");
    if(file_size== 0) return ERR_INV_WKSP(filename);

    uint32_t wksp_header = (
        (uint32_t)'P' << 24 |
        (uint32_t)'S' << 16 |
        (uint32_t)'K' << 8 |
        (uint32_t)'W' << 0
    );

    if(file_size<= 8) return ERR_INV_WKSP(filename);
    LOG("wksp_parse: %s", wksp_data_ptr[0] == wksp_header ? "Valid wksp file" : "Invalid wksp file");

    uint32_t wksp_json_size = wksp_data_ptr[1];
    LOG("wksp_parse: Json size: %d bytes", wksp_json_size);
    if(file_size<= wksp_json_size+8) return ERR_INV_WKSP(filename);

    *json_start_ptr = (char*)wksp_data_ptr + 8;
    uint32_t json_padded_len = (wksp_json_size + 3) &~3;
    *sprv_count_start = (uint32_t*)(*json_start_ptr + json_padded_len);
    *out_json_size = wksp_json_size;
    return WK_OK;
}

static WkResult
resolve_image_format(const WallkanDevice *wk_device, yyjson_val *formats_arr, WKSPPass *pass,
    const char *filename)
{
    if (!yyjson_is_arr(formats_arr)) return ERR_INV_WKSP(filename);

    uint32_t count = yyjson_arr_size(formats_arr);
    if(count == 0) return ERR_INV_WKSP(filename);

    for (uint32_t i = 0; i < count; i++) {
        yyjson_val *_format_id = yyjson_arr_get(formats_arr, i);
        if(!yyjson_is_int(_format_id)) return ERR_INV_WKSP(filename);
        VkFormat format_id = yyjson_get_int(_format_id);

        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(wk_device->physical_device, format_id, &props);

        VkFormatFeatureFlags required_features = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if (pass->shader_stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
            required_features |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        }else{
            required_features |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
        }
        if ((props.optimalTilingFeatures & required_features) == required_features) {
            pass->selected_img_format = format_id;
            break;
        }
    }
    if(pass->selected_img_format == VK_FORMAT_UNDEFINED){
        return WK_ERR(WK_ERR_VK_UNSUPPORTED_IMAGE_FORMAT,
            "This shader package required image formats are not unsupported by the selected device!");
    }
    return WK_OK;
}

static WkResult
create_sampler_info_from_json(yyjson_val *sampler, const char *filename,
    VkSamplerCreateInfo *out_sampler_info)
{
    yyjson_val *magfilter = yyjson_obj_get(sampler, "magfilter");
    if (!yyjson_is_int(magfilter)) return ERR_INV_WKSP(filename);
    VkFilter mag_filter = yyjson_get_int(magfilter);

    yyjson_val *minfilter = yyjson_obj_get(sampler, "minfilter");
    if (!yyjson_is_int(minfilter)) return ERR_INV_WKSP(filename);
    VkFilter min_filter = yyjson_get_int(minfilter);

    yyjson_val *mipmapmode = yyjson_obj_get(sampler, "mipmapmode");
    if (!yyjson_is_int(mipmapmode)) return ERR_INV_WKSP(filename);
    VkSamplerMipmapMode mipmap_mode = yyjson_get_int(mipmapmode);

    yyjson_val *addressmodeu = yyjson_obj_get(sampler, "addressmodeu");
    if (!yyjson_is_int(addressmodeu)) return ERR_INV_WKSP(filename);
    VkSamplerAddressMode address_mode_u = yyjson_get_int(addressmodeu);

    yyjson_val *addressmodev = yyjson_obj_get(sampler, "addressmodev");
    if (!yyjson_is_int(addressmodev)) return ERR_INV_WKSP(filename);
    VkSamplerAddressMode address_mode_v = yyjson_get_int(addressmodev);

    yyjson_val *addressmodew = yyjson_obj_get(sampler, "addressmodew");
    if (!yyjson_is_int(addressmodew)) return ERR_INV_WKSP(filename);
    VkSamplerAddressMode address_mode_w = yyjson_get_int(addressmodew);

    yyjson_val *minlod = yyjson_obj_get(sampler, "minlod");
    if (!yyjson_is_num(minlod)) return ERR_INV_WKSP(filename);
    float min_lod = (float)yyjson_get_num(minlod);

    yyjson_val *maxlod = yyjson_obj_get(sampler, "maxlod");
    if (!yyjson_is_num(maxlod)) return ERR_INV_WKSP(filename);
    float max_lod = (float)yyjson_get_num(maxlod);

    *out_sampler_info = (VkSamplerCreateInfo){
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .minFilter = min_filter,
        .magFilter = mag_filter,
        .mipmapMode = mipmap_mode,
        .addressModeU = address_mode_u,
        .addressModeV = address_mode_v,
        .addressModeW = address_mode_w,
        .minLod = min_lod,
        .maxLod = max_lod
    };
    return WK_OK;
}

static WkResult
wksp_json_load(const WallkanDevice *wk_device, ArenaAllocator *alloc, WKSPParser *parser,
    WKSPContainer *wksp, char *json, size_t len, const char *filename)
{
    yyjson_doc *meta_doc = yyjson_read_opts(json, len,  0, &parser->json_alc, NULL);
    if(!meta_doc) return ERR_INV_WKSP(filename);

    yyjson_val *root = yyjson_doc_get_root(meta_doc);

    yyjson_val *_meta_name = yyjson_ptr_get(root, "/meta/name");
    const char *meta_name = yyjson_get_str(_meta_name);
    if(!meta_name) return ERR_INV_WKSP(filename);
    uint32_t max_size = sizeof(wksp->name);
    strncpy(wksp->name, meta_name, max_size-1);
    wksp->name[max_size-1] = '\0';

    yyjson_val *passes = yyjson_obj_get(root, "passes");
    if(!yyjson_is_arr(passes)) return ERR_INV_WKSP(filename);
    wksp->total_passes = yyjson_arr_size(passes);
    wksp->passes = arena_alloc(alloc, wksp->total_passes*sizeof(WKSPPass));

    for (uint32_t i=0; i<wksp->total_passes; i++) {
        wksp->passes[i] = (WKSPPass){0};
        yyjson_val *pass = yyjson_arr_get(passes, i);

        const char *pass_name = yyjson_get_str(yyjson_obj_get(pass, "name"));
        if(!pass_name) return ERR_INV_WKSP(filename);
        wksp->passes[i].name = pass_name;

        yyjson_val *_iterations = yyjson_obj_get(pass, "iterations");
        if(!yyjson_is_uint(_iterations)) return ERR_INV_WKSP(filename);
        wksp->passes[i].iterations = yyjson_get_uint(_iterations);

        yyjson_val *_self_feedback = yyjson_obj_get(pass, "self_feedback");
        if(!yyjson_is_bool(_self_feedback)) return ERR_INV_WKSP(filename);
        wksp->passes[i].self_feedback = yyjson_get_bool(_self_feedback);

        yyjson_val *scale = yyjson_obj_get(pass, "scale");
        if(!yyjson_is_num(scale)) return ERR_INV_WKSP(filename);
        wksp->passes[i].scale = (float)yyjson_get_num(scale);

        yyjson_val *type = yyjson_obj_get(pass, "type");
        if(!yyjson_is_int(type)) return ERR_INV_WKSP(filename);
        wksp->passes[i].shader_stage = yyjson_get_int(type);

        yyjson_val *channels = yyjson_obj_get(pass, "channels");
        if(!yyjson_is_arr(channels)) return ERR_INV_WKSP(filename);
        wksp->passes[i].channel_count = yyjson_arr_size(channels);
        wksp->passes[i].channels = arena_alloc(alloc,

            sizeof(WKSPPassChannel) * wksp->passes[i].channel_count);
        for (uint32_t j=0; j<wksp->passes[i].channel_count; j++) {
            yyjson_val *channel = yyjson_arr_get(channels, j);

            const char *name = yyjson_get_str(yyjson_obj_get(channel, "name"));
            if(!name) return ERR_INV_WKSP(filename);

            yyjson_val *_type = yyjson_obj_get(channel, "type");
            if(!yyjson_is_uint(_type)) return ERR_INV_WKSP(filename);
            uint32_t type = yyjson_get_uint(_type);

            yyjson_val *_producer = yyjson_obj_get(channel, "producer");
            if(!yyjson_is_uint(_producer)) return ERR_INV_WKSP(filename);
            uint32_t producer = yyjson_get_uint(_producer);

            yyjson_val *_desc_type = yyjson_obj_get(channel, "desc_type");
            if(!yyjson_is_uint(_desc_type)) return ERR_INV_WKSP(filename);

            yyjson_val *sampler = yyjson_obj_get(channel, "sampler");
            if(!sampler) return ERR_INV_WKSP(filename);

            wksp->passes[i].channels[j] = (WKSPPassChannel){
                .name = name,
                .type = type,
                .producer_idx = producer,
                .desc_type = yyjson_get_uint(_desc_type)
            };
            WK_TRY(create_sampler_info_from_json(sampler, filename,
                &wksp->passes[i].channels[j].sampler_create_info));
        }

        yyjson_val *formats = yyjson_obj_get(pass, "formats");
        WK_TRY(resolve_image_format(wk_device, formats, &wksp->passes[i], filename));
    }

    yyjson_val *descriptors = yyjson_obj_get(root, "descriptors");
    if(!descriptors) return ERR_INV_WKSP(filename);

    yyjson_val *desc_pool = yyjson_obj_get(descriptors, "pool");
    if(!desc_pool) return ERR_INV_WKSP(filename);

    uint32_t desc_pool_size_count = yyjson_arr_size(desc_pool);
    if(desc_pool_size_count == 0) return ERR_INV_WKSP(filename);
    wksp->desc_pool_size_count = desc_pool_size_count;
    wksp->desc_pool_sizes = arena_alloc(alloc, sizeof(VkDescriptorPoolSize) *
        desc_pool_size_count);

    uint32_t desc_pool_maxsets = yyjson_get_uint(yyjson_obj_get(descriptors, "pool_maxsets"));
    if(desc_pool_maxsets == 0) return ERR_INV_WKSP(filename);

    wksp->desc_max_sets = desc_pool_maxsets;

    for(uint32_t i=0;i<desc_pool_size_count;i++){
        yyjson_val *descriptor = yyjson_arr_get(desc_pool, i);
        uint32_t type = yyjson_get_uint(yyjson_obj_get(descriptor, "type"));
        uint32_t count = yyjson_get_uint(yyjson_obj_get(descriptor, "count"));
        wksp->desc_pool_sizes[i] = (VkDescriptorPoolSize){
            .type = type,
            .descriptorCount = count
        };
    }

    yyjson_val *_param_count = yyjson_obj_get(root, "param_count");
    uint32_t param_count = (uint32_t)yyjson_get_uint(_param_count);
    wksp->total_parameters = param_count;

    yyjson_val *params = yyjson_obj_get(root, "params");
    if(!params) return ERR_INV_WKSP(filename);
    if (yyjson_arr_size(params) != param_count) return ERR_INV_WKSP(filename);

    wksp->parameters = arena_alloc(alloc, param_count * sizeof(WKSPParameter));
    if(!wksp->parameters) return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Allocation failure!");

    for (uint32_t i=0; i<param_count; i++) {
        yyjson_val *param = yyjson_arr_get(params, i);

        yyjson_val *_type = yyjson_obj_get(param, "type");
        // Because yyjson_get_int returns 0 if invalid we cannot know if
        // Type is WKSP_PARAM_TYPE_INT or its invalid
        if(!yyjson_is_int(_type)) return ERR_INV_WKSP(filename);
        uint32_t type = yyjson_get_int(_type);
        if(type > WKSP_PARAM_TYPE_TOTAL_COUNT) return ERR_INV_WKSP(filename);
        wksp->parameters[i] = (WKSPParameter){
            .type = type
        };

        yyjson_val *current_val = yyjson_obj_get(param, "current");
        if(!current_val) return ERR_INV_WKSP(filename);

        yyjson_val *_components_count = yyjson_obj_get(current_val, "components_count");
        if(!yyjson_is_uint(_components_count)) return ERR_INV_WKSP(filename);
        uint32_t components_count = yyjson_get_uint(_components_count);
        // A vector of xyzw
        if(components_count > 4) return ERR_INV_WKSP(filename);
        wksp->parameters[i].components_count = components_count;

        yyjson_val *components = yyjson_obj_get(current_val, "components");
        if(!yyjson_is_arr(components)) return ERR_INV_WKSP(filename);
        if (yyjson_arr_size(components) < components_count) return ERR_INV_WKSP(filename);


        for (uint32_t j = 0; j < components_count; j++) {
            yyjson_val *component = yyjson_arr_get(components, j);
            if(!component) return ERR_INV_WKSP(filename);
            switch (type){
                case WKSP_PARAM_TYPE_FLOAT:
                    if (yyjson_is_num(component)) {
                        wksp->parameters[i].float32_components[j] =
                            (float)yyjson_get_num(component);
                        wksp->total_parameters_size+=4;
                    }else return ERR_INV_WKSP(filename);
                    break;
                case WKSP_PARAM_TYPE_INT:
                    if (yyjson_is_int(component)) {
                        wksp->parameters[i].int32_components[j] =
                            yyjson_get_int(component);
                        wksp->total_parameters_size+=4;
                    }else return ERR_INV_WKSP(filename);
                    break;
                case WKSP_PARAM_TYPE_UINT:
                    if (yyjson_is_uint(component)) {
                        wksp->parameters[i].uint32_components[j] =
                            yyjson_get_uint(component);
                        wksp->total_parameters_size+=4;
                    }else return ERR_INV_WKSP(filename);
                    break;
                case WKSP_PARAM_TYPE_BOOL:
                    if (yyjson_is_bool(component)) {
                        uint32_t val = yyjson_get_bool(component) ? 1 : 0;
                        wksp->parameters[i].bool_components[j] = val;
                        wksp->total_parameters_size+=4;
                    }else return ERR_INV_WKSP(filename);
                    break;
                default:
                    return ERR_INV_WKSP(filename);
            }
        }
    }
    return WK_OK;
}

WkResult
wksp_load(ArenaAllocator *alloc, const WallkanDevice *wk_device, WKSPParser *parser, const char *filename,
    WKSPContainer *wksp)
{
    size_t size;
    void *read_bytes;
    // A failed wksp_load means wksp_destroy must be called by the caller
    // But if wksp_load fails midway wksp->vert_module_idx
    // Which will be 0, pointing to a fragment shader module instead.
    wksp->vert_module_idx = UINT32_MAX;

    WK_TRY(read_file(alloc, filename, &size, &read_bytes));
    uint32_t *wksp_data_ptr = (uint32_t*)read_bytes;

    char *json_start_ptr = NULL;
    uint32_t wksp_json_block_size = 0;
    uint32_t *spirv_count_start = 0;

    WK_TRY(wksp_validate(wksp_data_ptr, size, filename, &wksp_json_block_size, &json_start_ptr,
        &spirv_count_start));
    LOG("wksp_parse: Json size: %d bytes", wksp_json_block_size);

    WK_TRY(wksp_json_load(wk_device, alloc, parser, wksp, json_start_ptr, wksp_json_block_size, filename));
    wksp_data_ptr = spirv_count_start;

    uint32_t frag_spirv_count = wksp_data_ptr[0];
    wksp_data_ptr += 1;

    VkShaderModule *wksp_shader_modules = arena_calloc(alloc, (frag_spirv_count+1) * sizeof(VkShaderModule));
    LOG("wksp_parse: Total shader passes: %d (excluding vertex shader)", frag_spirv_count);
    for(uint32_t i=0;i<frag_spirv_count;i++){
        LOG("wksp_parse: Shader Pass %d", i);
        uint32_t spirv_size = wksp_data_ptr[0];
        wksp_data_ptr+=1;

        VkShaderModuleCreateInfo module_create_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv_size,
            .pCode = wksp_data_ptr
        };

        WK_TRY(EXPECT_VK(
            vkCreateShaderModule(wk_device->device, &module_create_info, NULL,
                &wksp_shader_modules[i]),
            WK_ERR_VK_CREATE_SHADER_MODULE_FAILED, "Failed to create shader[%d] module!", i
        ));
        wksp_data_ptr += spirv_size / 4;
    }
    uint32_t vert_spirv_size = wksp_data_ptr[0];
    wksp_data_ptr+=1;
    VkShaderModuleCreateInfo vert_module_create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vert_spirv_size,
        .pCode = wksp_data_ptr,
    };
    wksp->vert_module_idx = frag_spirv_count;
    WK_TRY(EXPECT_VK(
        vkCreateShaderModule(wk_device->device, &vert_module_create_info, NULL,
            &wksp_shader_modules[wksp->vert_module_idx]),
        WK_ERR_VK_CREATE_SHADER_MODULE_FAILED, "Failed to create vertex shader module!"
    ));
    wksp->shader_modules = wksp_shader_modules;
    return WK_OK;
}

void
wksp_destroy(const WallkanDevice *wk_device, WKSPContainer *wksp)
{
    if (!wksp) {
        return;
    }
    for (uint32_t i=0; i<wksp->total_passes; i++) {
        if(!wksp->shader_modules[i]) continue;
        vkDestroyShaderModule(wk_device->device, wksp->shader_modules[i], NULL);
        wksp->shader_modules[i] = NULL;
    }
    // The vertex shader
    if(wksp->vert_module_idx != UINT32_MAX && wksp->shader_modules[wksp->vert_module_idx]){
        vkDestroyShaderModule(wk_device->device, wksp->shader_modules[wksp->vert_module_idx], NULL);
    }
    *wksp = (WKSPContainer){0};
}
