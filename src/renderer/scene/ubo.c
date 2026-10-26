#include "renderer/scene/ubo.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>

#include "common.h"
#include "err.h"
#include "renderer/device.h"
#include "renderer/scene/buffer.h"
#include "renderer/scene/pipeline.h"
#include "renderer/scene/scene.h"
#include "wksp/parser.h"

static VkDeviceSize
align_up(VkDeviceSize size, VkDeviceSize alignment)
{
    return (size + (alignment - 1)) & ~(alignment -1);
}

WkResult
wk_ubo_registry_new(WkUboRegistry *registry, const WallkanDevice *wk_device,
    VkDeviceSize buf_size, uint32_t *out_index)
{
    uint32_t idx = bit_find_free_slot(registry->ubo_slots);
    if (idx == UINT32_MAX) {
        return WK_ERR(WK_ERR_MAX_AMOUNT_UBO, "Max amount of UBO reached??");
    }
    WkUBO *target_ubo = &registry->ubos[idx];

    WK_TRY(wk_buffer_new(wk_device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, buf_size,
        &target_ubo->buffer, &target_ubo->mem_size, &target_ubo->alignment,
        &target_ubo->memory_type_bits));
    target_ubo->size = buf_size;
    *out_index = idx;
    registry->ubo_slots |= (1u << idx);
    return WK_OK;
}

WkResult
wk_ubo_registry_calculate_offsets(WkUboRegistry *registry)
{
    uint8_t ubo_slots = registry->ubo_slots;

    VkDeviceSize offset = 0;
    while (ubo_slots) {
        uint32_t idx = bit_pop_lsb_u8(&ubo_slots);
        WkUBO *ubo = &registry->ubos[idx];
        registry->ubo_offsets[idx] = align_up(offset, ubo->alignment);
        offset = registry->ubo_offsets[idx] + ubo->mem_size;
    }
    registry->allocation_size = offset;
    return WK_OK;
}


static WkResult
find_memory_type(const WallkanDevice *wk_device, uint32_t memory_type_bits, VkMemoryPropertyFlags vk_props,
    uint32_t *out_index)
{
    WkResult wkres = WK_OK;
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(wk_device->physical_device, &mem_props);
    for (uint32_t i=0; i<mem_props.memoryTypeCount; i++) {
        if (memory_type_bits & (1u << i)) {
            if ((mem_props.memoryTypes[i].propertyFlags & vk_props) == vk_props){
                *out_index = i;
                return wkres;
            }
        }
    }
    return WK_ERR(WK_ERR_VK_REQUIRED_MEMORY_TYPE_NOT_FOUND, "Couldnt find required memory type!");
}

static WkResult
ubo_get_mem_data(WkUboRegistry *registry, const WallkanDevice *wk_device, uint32_t *out_mem_idx)
{
    uint8_t ubo_slots = registry->ubo_slots;
    // For the usage now, All UBO share same memory type and index.
    WkUBO *first = &registry->ubos[bit_pop_lsb_u8(&ubo_slots)];
    WK_TRY(find_memory_type(wk_device, first->memory_type_bits,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, out_mem_idx));
    return WK_OK;
}

WkResult
wk_ubo_registry_alloc(WkUboRegistry *registry, const WallkanDevice *wk_device)
{
    uint32_t mem_idx;
    WK_TRY(ubo_get_mem_data(registry, wk_device, &mem_idx));

    const VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = registry->allocation_size,
        .memoryTypeIndex = mem_idx,
    };
    WK_TRY(EXPECT_VK(
        vkAllocateMemory(wk_device->device, &alloc_info, NULL, &registry->memory),
        WK_ERR_VK_DEVICE_MEMORY_ALLOCATION_FAILURE, "Failed to allocate device memory!"
    ));

    uint8_t ubo_slots = registry->ubo_slots;
    while (ubo_slots) {
        uint32_t idx = bit_pop_lsb_u8(&ubo_slots);
        vkBindBufferMemory(wk_device->device, registry->ubos[idx].buffer, registry->memory,
            registry->ubo_offsets[idx]);
    }
    return WK_OK;
}

WkResult
wk_ubo_registry_map_all(WkUboRegistry *registry, const WallkanDevice *wk_device)
{
    uint8_t ubo_slots = registry->ubo_slots;
    void *mapped_mem;
    WK_TRY(EXPECT_VK(
        vkMapMemory(wk_device->device, registry->memory, 0,
            registry->allocation_size, 0, &mapped_mem),
        WK_ERR_VK_MAP_MEMORY_FAILED, "Failed to map memory for UBO!"
    ));
    while (ubo_slots) {
        uint32_t idx = bit_pop_lsb_u8(&ubo_slots);
        registry->ubos[idx].mapped_mem = ((uint8_t*)mapped_mem)+registry->ubo_offsets[idx];
    }
    return WK_OK;
}

uint32_t
wk_ubo_frame_data_size(void)
{
    // Currently its simple but who knows when it also have some heap pointers like pass data
    return sizeof(WkFrameData);
}

void
wk_ubo_pass_data_size(const WKSPContainer *wksp, uint32_t *passes_res_size,
    uint32_t *channels_res_size, uint32_t *total_size)
{
    uint32_t _passes_res_size = 0;
    uint32_t _channels_res_size = 0;
    uint32_t _total_size = 0;
    for (uint32_t i=0; i<wksp->total_passes; i++) {
        // +Pass resolution
        _passes_res_size += SHADERTOY_RESOLUTION_SIZE;
        // +Each channel resolution
        _channels_res_size += SHADERTOY_RESOLUTION_SIZE * wksp->passes[i].channel_count;
    }
    _total_size = _passes_res_size + _channels_res_size;
    if (passes_res_size) {
        *passes_res_size = _passes_res_size;
    }
    if (channels_res_size) {
        *channels_res_size = _channels_res_size;
    }
    if (_total_size) {
        *total_size = _total_size;
    }
}

WkResult
wk_ubo_frame_data_init(const WKSPContainer *wksp, WkFrameData *frame_data)
{
    (void)wksp;
    (void)frame_data;
    return WK_OK;
}

static void
write_shadertoy_resolution_flat(float *array, uint32_t start_idx, VkExtent2D resolution, float scale)
{
    array[start_idx] = scaled_res_component(resolution.width, scale);
    array[start_idx+1] = scaled_res_component(resolution.height, scale);
    array[start_idx+2] = 1.0;
}

WkResult
wk_ubo_pass_data_init(const WKSPContainer *wksp, VkExtent2D final_resolution,
    WkPassData *pass_data, uint32_t pass_count)
{
    wk_ubo_pass_data_size(wksp, &pass_data->passes_res_size, &pass_data->channels_res_size,
        &pass_data->total_size);

    float *pass_resolutions = (float*)malloc(pass_data->passes_res_size);
    if(!pass_resolutions) return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Allocation failure!");
    for (uint32_t i=0; i<pass_count; i++) {
        // Each pass's resolution is 3 times 32bit therefore:
        uint32_t res_idx_start = i * 3;
        write_shadertoy_resolution_flat(pass_resolutions, res_idx_start, final_resolution,
            wksp->passes[i].scale);
    }
    pass_data->PassesResolution = pass_resolutions;

    float *ch_resolutions;
    if (pass_data->channels_res_size == 0) {
        ch_resolutions = NULL;
    }else{
        ch_resolutions = (float*)malloc(pass_data->channels_res_size);
        if(!ch_resolutions) return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Allocation failure!");
    }

    uint32_t channel_res_idx_start = 0;
    for (uint32_t i=0; i<pass_count; i++) {
        // Each pass's resolution is 3 times 32bit therefore:
        for (uint32_t j=0; j<wksp->passes[i].channel_count; j++) {
            WKSPPassChannel *channel = &wksp->passes[i].channels[j];
            if (channel->type != WK_CHANNEL_TYPE_SHADER_PASS) {
                channel_res_idx_start += 3;
                continue;
            }
            write_shadertoy_resolution_flat(ch_resolutions, channel_res_idx_start,
                final_resolution, wksp->passes[channel->producer_idx].scale);
            channel_res_idx_start += 3;
        }
    }
    pass_data->iChannelResolution = ch_resolutions;
    return WK_OK;
}

void
wk_ubo_frame_write(WkFrameData *input, WkUBO *ubo)
{
    uint8_t *mapped_mem = (uint8_t*)ubo->mapped_mem;
    memcpy(mapped_mem, input, sizeof(WkFrameData));
}


void
wk_ubo_pass_data_update(const WallkanPipeline *wk_pipeline, float *pass_resolutions,
    float *ch_resolutions, VkExtent2D final_resolution)
{
    for (uint32_t i=0; i<wk_pipeline->pass_count; i++) {
        // Each pass's resolution is 3 times 32bit therefore:
        uint32_t res_idx_start = i * 3;
        write_shadertoy_resolution_flat(pass_resolutions, res_idx_start, final_resolution,
            wk_pipeline->passes_data[i].scale);
    }

    uint32_t channel_res_idx_start = 0;
    for (uint32_t i=0; i<wk_pipeline->pass_count; i++) {
        // Each pass's resolution is 3 times 32bit therefore:
        for (uint32_t j=0; j<wk_pipeline->passes_data[i].channel_count; j++) {
            WkPassChannel *channel = &wk_pipeline->passes_data[i].channels[j];
            if (channel->type != WK_CHANNEL_TYPE_SHADER_PASS) {
                channel_res_idx_start += 3;
                continue;
            }
            write_shadertoy_resolution_flat(ch_resolutions, channel_res_idx_start, final_resolution,
                wk_pipeline->passes_data[channel->producer_idx].scale);
            channel_res_idx_start += 3;
        }
    }
}

void
wk_ubo_pass_write(WkPassData *input, WkUBO *ubo, uint32_t pass_count)
{
    uint8_t *mapped_mem = (uint8_t*)ubo->mapped_mem;

    uint32_t offset = 0;
    memcpy(mapped_mem + offset, input->PassesResolution, input->passes_res_size);

    if (input->channels_res_size > 0 && input->iChannelResolution) {
        offset = pass_count * SHADERTOY_RESOLUTION_SIZE;
        memcpy(mapped_mem + offset, input->iChannelResolution, input->channels_res_size);
    }
}

void
wk_ubo_shader_params_write(uint8_t *params, WkUBO *ubo, uint32_t total_param_size)
{
    memcpy(ubo->mapped_mem, params, total_param_size);
}

void
wk_ubo_frame_data_cleanup(WkFrameData *frame_data)
{
    // Same comment, refer to wk_ubo_frame_data_size
    (void)frame_data;
}

void
wk_ubo_pass_data_cleanup(WkPassData *pass_data)
{
    if(pass_data->PassesResolution){
        free(pass_data->PassesResolution);
        pass_data->PassesResolution = NULL;
    }
    if(pass_data->iChannelResolution){
        free(pass_data->iChannelResolution);
        pass_data->iChannelResolution = NULL;
    }
}

void
wk_ubo_registry_cleanup(WkUboRegistry *registry, const WallkanDevice *wk_device)
{
    uint8_t ubo_slots = registry->ubo_slots;
    while (ubo_slots) {
        uint32_t idx = bit_pop_lsb_u8(&ubo_slots);

        registry->ubo_slots &= ~(1u << idx);
        registry->ubos[idx].mapped_mem = NULL;
        if(registry->ubos[idx].buffer){
            vkDestroyBuffer(wk_device->device, registry->ubos[idx].buffer, NULL);
            registry->ubos[idx].buffer = NULL;
        }
    }
    if (registry->memory) {
        vkFreeMemory(wk_device->device, registry->memory, NULL);
        registry->memory = NULL;
    }
}


WkResult
wk_ubo_init_all(WallkanScene *wk_scene, const WallkanDevice *wk_device, const WKSPContainer *wksp)
{
    uint32_t frame_data_size = wk_ubo_frame_data_size();
    uint32_t pass_data_size;
    wk_ubo_pass_data_size(wksp, NULL, NULL, &pass_data_size);

    uint32_t pass_ubo_idx;
    WK_TRY(wk_ubo_registry_new(&wk_scene->ubo.registry, wk_device,
        pass_data_size, &pass_ubo_idx));
    wk_scene->ubo.pass = wk_ubo_registry_get(&wk_scene->ubo.registry, pass_ubo_idx);
    WK_TRY(wk_ubo_pass_data_init(wksp, wk_scene->output.extent, &wk_scene->ubo.pass_data,
        wksp->total_passes));

    for (uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        uint32_t frame_ubo_idx;
        uint32_t shader_param_ubo_idx;

        WK_TRY(wk_ubo_registry_new(&wk_scene->ubo.registry, wk_device,
            frame_data_size, &frame_ubo_idx));

        wk_scene->ubo.frame[i] = wk_ubo_registry_get(&wk_scene->ubo.registry, frame_ubo_idx);
        WK_TRY(wk_ubo_frame_data_init(wksp, &wk_scene->ubo.frame_data));

        if(wksp->total_parameters > 0){
            WK_TRY(wk_ubo_registry_new(&wk_scene->ubo.registry, wk_device,
                wk_scene->ubo.total_parameters_size, &shader_param_ubo_idx));
            wk_scene->ubo.shader_params[i] = wk_ubo_registry_get(&wk_scene->ubo.registry,
                shader_param_ubo_idx);
            // "wk_ubo_shader_params_data_init" is already done
            // Unlike others this one is loaded when WKSP is loaded and only modified via IPC
        }
    }

    WK_TRY(wk_ubo_registry_calculate_offsets(&wk_scene->ubo.registry));
    WK_TRY(wk_ubo_registry_alloc(&wk_scene->ubo.registry, wk_device));
    WK_TRY(wk_ubo_registry_map_all(&wk_scene->ubo.registry, wk_device));

    wk_ubo_pass_write(&wk_scene->ubo.pass_data, wk_scene->ubo.pass, wksp->total_passes);
    for (uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        wk_ubo_frame_write(&wk_scene->ubo.frame_data, wk_scene->ubo.frame[i]);
        if(wksp->total_parameters > 0){
            wk_ubo_shader_params_write(wk_scene->ubo.parameters_data, wk_scene->ubo.shader_params[i],
                wk_scene->ubo.total_parameters_size);
        }
    }
    return WK_OK;
}

void
wk_ubo_cleanup_all(WallkanScene *wk_scene, const WallkanDevice *wk_device)
{
    wk_ubo_pass_data_cleanup(&wk_scene->ubo.pass_data);
    wk_ubo_frame_data_cleanup(&wk_scene->ubo.frame_data);
    wk_ubo_registry_cleanup(&wk_scene->ubo.registry, wk_device);
}
