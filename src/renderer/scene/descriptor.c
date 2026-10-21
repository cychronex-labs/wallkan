#include "renderer/scene/descriptor.h"
#include "arena_alloc.h"
#include "common.h"
#include "err.h"
#include "renderer/device.h"
#include "renderer/scene/pipeline.h"
#include "renderer/scene/scene.h"
#include "renderer/scene/texture.h"
#include "renderer/scene/ubo.h"
#include "wksp/parser.h"
#include <stdint.h>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>

WkResult
descriptor_pool_init(WallkanPipeline *wk_pipeline, const WallkanDevice *wk_device,
    const WKSPContainer *wksp)
{
    LOG("init_descriptor_pool: Initializing scene descriptor pool...");
    LOG("init_descriptor_pool: Total types of descriptor: %d", wksp->desc_pool_size_count);
    LOG("init_descriptor_pool: Total pool max sets: %d", wksp->desc_max_sets);
    VkDescriptorPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pPoolSizes = wksp->desc_pool_sizes,
        .poolSizeCount = wksp->desc_pool_size_count,
        .maxSets = wksp->desc_max_sets,
    };
    WK_TRY(EXPECT_VK(
        vkCreateDescriptorPool(wk_device->device, &pool_create_info, NULL,
            &wk_pipeline->descriptor_pool),
        WK_ERR_VK_CREATE_DESCRIPTOR_POOL_FAILED, "Failed to create descriptor pool!"
    ));
    return WK_OK;
}


static WkResult
init_common_desc_layouts(const WallkanDevice *wk_device,
    WallkanPipeline *wk_pipeline)
{
    VkDescriptorSetLayoutBinding frame_ubo_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    };
    VkDescriptorSetLayoutCreateInfo frame_ubo_set = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &frame_ubo_binding
    };

    VkDescriptorSetLayoutBinding shader_params_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    };
    VkDescriptorSetLayoutCreateInfo shader_params_set = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &shader_params_binding
    };

    WK_TRY(EXPECT_VK(
        vkCreateDescriptorSetLayout(wk_device->device, &frame_ubo_set, NULL,
            &wk_pipeline->desc_set.frame_layout),
        WK_ERR_VK_CREATE_DESCRIPTOR_SET_LAYOUT_FAILED,
        "Failed to create frame UBO descriptor set layout"
    ));

    WK_TRY(EXPECT_VK(
        vkCreateDescriptorSetLayout(wk_device->device, &shader_params_set, NULL,
            &wk_pipeline->desc_set.params_layout),
        WK_ERR_VK_CREATE_DESCRIPTOR_SET_LAYOUT_FAILED,
        "Failed to create shader parameters UBO descriptor set layout"
    ));
    return WK_OK;
}

static WkResult
compute_desc_set_layout_init(WallkanPipeline *wk_pipeline,
    const WallkanDevice *wk_device)
{
    VkDescriptorSetLayoutBinding out_img_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
    };
    VkDescriptorSetLayoutCreateInfo out_img_set = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &out_img_binding,
    };
    WK_TRY(EXPECT_VK(
        vkCreateDescriptorSetLayout(wk_device->device, &out_img_set, NULL,
            &wk_pipeline->desc_set.compute_storage_layout),
        WK_ERR_VK_CREATE_DESCRIPTOR_SET_LAYOUT_FAILED,
        "Failed to create shader parameters UBO descriptor set layout"
    ));
    return WK_OK;
}

static WkResult
init_pass_desc_layouts(ArenaAllocator *alloc, WallkanPipeline *wk_pipeline,
    const WallkanDevice *wk_device, const WKSPContainer *wksp)
{
    WK_TRY(compute_desc_set_layout_init(wk_pipeline, wk_device));
    for (uint32_t i=0; i<wksp->total_passes; i++){
        WkPipelinePass *target_pass = &wk_pipeline->passes_data[i];
        VkDescriptorSetLayout *pass_set_layout = &target_pass->input_set_layout;
        // +1 -> WkPassData
        uint32_t bindings_count = wksp->passes[i].channel_count+1;

        VkDescriptorSetLayoutBinding *input_bindings = arena_alloc(alloc,
            sizeof(VkDescriptorSetLayoutBinding) * bindings_count);

        input_bindings[0] = (VkDescriptorSetLayoutBinding){
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = wksp->passes[i].shader_stage
        };
        // j=0 -> WkPassData
        for (uint32_t j=1; j<bindings_count; j++) {
            input_bindings[j] = (VkDescriptorSetLayoutBinding){
                .binding = j,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = wksp->passes[i].shader_stage
            };
        }
        VkDescriptorSetLayoutCreateInfo input_set = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = bindings_count,
            .pBindings    = input_bindings,
        };
        WK_TRY(EXPECT_VK(
            vkCreateDescriptorSetLayout(wk_device->device, &input_set, NULL,
                pass_set_layout),
            WK_ERR_VK_CREATE_DESCRIPTOR_SET_LAYOUT_FAILED,
            "Failed to create shader parameters UBO descriptor set layout"
        ));
    }
    return WK_OK;
}


WkResult
descriptor_set_layouts_init(ArenaAllocator *alloc, WallkanPipeline *wk_pipeline,
    const WallkanDevice *wk_device, const WKSPContainer *wksp)
{
    WK_TRY(init_common_desc_layouts(wk_device, wk_pipeline));
    WK_TRY(init_pass_desc_layouts(alloc, wk_pipeline, wk_device, wksp));
    return WK_OK;
}

static WkResult
common_desc_sets_alloc(WallkanPipeline *wk_pipeline,
    const WallkanDevice *wk_device, const WKSPContainer *wksp)
{
    // MAX_FRAMES_IN_FLIGHT * number of set layout that is common
    uint32_t count = 2;
    VkDescriptorSetLayout layouts[4] = {
        wk_pipeline->desc_set.frame_layout,
        wk_pipeline->desc_set.frame_layout,
    };
    VkDescriptorSet target_sets[4] = {
        wk_pipeline->desc_set.frame[0],
        wk_pipeline->desc_set.frame[1],
    };
    if(wksp->total_parameters > 0){
        layouts[count] = wk_pipeline->desc_set.params_layout;
        target_sets[count] = wk_pipeline->desc_set.params[0];
        count++;
        layouts[count] = wk_pipeline->desc_set.params_layout;
        target_sets[count] = wk_pipeline->desc_set.params[1];
        count++;
    }

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = wk_pipeline->descriptor_pool,
        .descriptorSetCount = count,
        .pSetLayouts = layouts
    };

    WK_TRY(EXPECT_VK(
        vkAllocateDescriptorSets(wk_device->device, &alloc_info, target_sets),
        WK_ERR_VK_DESCRIPTOR_SET_ALLOC_FAILED, "Failed to allocate descriptor sets!"
    ));
    if (count >= 2) {
        wk_pipeline->desc_set.frame[0] = target_sets[0];
        wk_pipeline->desc_set.frame[1] = target_sets[1];
    }
    if (count == 4) {
        wk_pipeline->desc_set.params[0] = target_sets[2];
        wk_pipeline->desc_set.params[1] = target_sets[3];
    }
    return WK_OK;
}

WkResult
descriptor_sets_alloc(WallkanPipeline *wk_pipeline,
    const WallkanDevice *wk_device, const WKSPContainer *wksp)
{
    WK_TRY(common_desc_sets_alloc(wk_pipeline, wk_device, wksp));
    for (uint32_t i=0; i<wksp->total_passes; i++) {
        WkPipelinePass *pass_data = &wk_pipeline->passes_data[i];
        WkPassFrames *pass_frames = &wk_pipeline->passes_frames[i];
        const WKSPPass *wksp_pass = &wksp->passes[i];

        uint32_t count = 2;
        VkDescriptorSetLayout layouts[6] = {
            pass_data->input_set_layout,
            pass_data->input_set_layout,
            pass_data->input_set_layout,
            pass_data->input_set_layout
        };

        VkDescriptorSet target_sets[6] = {
            pass_frames->desc_set.inputs[0][0],
            pass_frames->desc_set.inputs[1][0],

            pass_frames->desc_set.inputs[0][1],
            pass_frames->desc_set.inputs[1][1]
        };

        if (wksp_pass->self_feedback) {
            count += 2;
        }

        if (wksp_pass->shader_stage == VK_SHADER_STAGE_COMPUTE_BIT) {
            layouts[count] = wk_pipeline->desc_set.compute_storage_layout;
            target_sets[count] = wk_pipeline->passes_frames[i].desc_set.img_output[0];
            count++;
            layouts[count] = wk_pipeline->desc_set.compute_storage_layout;
            target_sets[count] = wk_pipeline->passes_frames[i].desc_set.img_output[1];
            count++;
        }
        VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = wk_pipeline->descriptor_pool,
            .descriptorSetCount = count,
            .pSetLayouts = layouts
        };
        WK_TRY(EXPECT_VK(
            vkAllocateDescriptorSets(wk_device->device, &alloc_info, target_sets),
            WK_ERR_VK_DESCRIPTOR_SET_ALLOC_FAILED, "Failed to allocate descriptor sets!"
        ));

        pass_frames->desc_set.inputs[0][0] = target_sets[0];
        pass_frames->desc_set.inputs[1][0] = target_sets[1];
        if (wksp_pass->self_feedback) {
            pass_frames->desc_set.inputs[0][1] = target_sets[2];
            pass_frames->desc_set.inputs[1][1] = target_sets[3];
        }
        if (wksp_pass->shader_stage == VK_SHADER_STAGE_COMPUTE_BIT) {
            pass_frames->desc_set.img_output[0] = target_sets[count - 2];
            pass_frames->desc_set.img_output[1] = target_sets[count - 1];
        }

    }
    return WK_OK;
}

static void
desc_set_write_img_binding(VkWriteDescriptorSet *desc_writes, VkDescriptorImageInfo *img_infos,
    uint32_t arr_idx, VkImageView img_view, VkSampler sampler, uint32_t binding_idx, VkDescriptorSet desc_set)
{
    img_infos[arr_idx] = (VkDescriptorImageInfo){
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView = img_view,
        .sampler = sampler
    };
    desc_writes[arr_idx] = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .dstBinding = binding_idx,
        .dstSet = desc_set,
        .pImageInfo = &img_infos[arr_idx],
    };
}


static void
desc_set_write_pass_data(const WallkanDevice *wk_device, uint32_t binding_idx,
    VkDescriptorSet desc_set, const WkUBO *pass_ubo)
{
    VkDescriptorBufferInfo buf_info = {
        .buffer = pass_ubo->buffer,
        .offset = 0,
        .range = pass_ubo->size
    };
    VkWriteDescriptorSet desc_writes[2] = {
        (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .dstArrayElement = 0,
            .dstBinding = binding_idx,
            .dstSet = desc_set,
            .pBufferInfo = &buf_info
        },
    };
    vkUpdateDescriptorSets(wk_device->device, 1, desc_writes, 0, NULL);
}

static void
desc_sets_inputs_write_frame(ArenaAllocator *alloc, WallkanPipeline *wk_pipeline,
    const WallkanDevice *wk_device, const WKSPPass *consumer_wksp_pass, const WkUBO *pass_ubo,
    uint32_t consumer_pass_idx, uint32_t frame_idx)
{
    // frame_idx is not like MAX_FRAMES_IN_FLIGHT
    // but instead its to track the latest texture index of producer pass that have self feedback
    WkPassFrames *consumer_pass = &wk_pipeline->passes_frames[consumer_pass_idx];
    desc_set_write_pass_data(wk_device, 0, consumer_pass->desc_set.inputs[frame_idx][0],
        pass_ubo);
    if(consumer_wksp_pass->self_feedback){
        desc_set_write_pass_data(wk_device, 0, consumer_pass->desc_set.inputs[frame_idx][1],
            pass_ubo);
    }

    VkWriteDescriptorSet *desc_writes = arena_calloc(alloc,
        sizeof(VkWriteDescriptorSet[PINGPONG_IMG_COUNT]) * consumer_wksp_pass->channel_count);
    VkDescriptorImageInfo *img_infos = arena_alloc(alloc,
        sizeof(VkDescriptorImageInfo[PINGPONG_IMG_COUNT]) * consumer_wksp_pass->channel_count);

    uint32_t desc_write_count = 0;
    for (uint32_t i=0; i < consumer_wksp_pass->channel_count; i++) {
        // binding idx 0 reserved for pass data
        WKSPPassChannel *wksp_channel = &consumer_wksp_pass->channels[i];
        WkPassFrames *producer_pass = &wk_pipeline->passes_frames[wksp_channel->producer_idx];


        if (wksp_channel->type != WK_CHANNEL_TYPE_SHADER_PASS) continue;

        uint32_t producer_iterations = wk_pipeline->passes_frames[wksp_channel->producer_idx].iterations;

        uint32_t binding_idx = i+1;

        uint32_t is_odd_frame  = frame_idx;
        uint32_t is_even_iters = 1 - (producer_iterations % 2);

        for (uint32_t j=0; j<PINGPONG_IMG_COUNT; j++) {
            if(j > 0 && !consumer_wksp_pass->self_feedback) continue;

            uint32_t write_idx = j;

            uint32_t last_tex_idx;
            if (wksp_channel->producer_idx < consumer_pass_idx) {
                last_tex_idx = is_odd_frame | is_even_iters;
            } else if (wksp_channel->producer_idx == consumer_pass_idx) {
                last_tex_idx = 1 - write_idx;
            } else {
                last_tex_idx = (1 - is_odd_frame) | is_even_iters;
            }

            WkTexture *texture = &producer_pass->textures[last_tex_idx];
            VkSampler sampler = wk_pipeline->passes_data[consumer_pass_idx].channels[i].sampler;

            desc_set_write_img_binding(desc_writes, img_infos, desc_write_count, texture->image_view, sampler,
                binding_idx,
                wk_pipeline->passes_frames[consumer_pass_idx].desc_set.inputs[frame_idx][write_idx]);
            desc_write_count++;
        }

    }
    vkUpdateDescriptorSets(wk_device->device, desc_write_count, desc_writes, 0, NULL);
}

static void
desc_sets_inputs_write(ArenaAllocator *alloc, WallkanPipeline *wk_pipeline,
    const WallkanDevice *wk_device, const WKSPPass *wksp_pass, const WkUBO *pass_ubo,
    uint32_t consumer_pass_idx)
{
    // Because odd,even frames causes changes in descriptor set's textures
    for (uint32_t i=0; i<2; i++) {
        desc_sets_inputs_write_frame(alloc, wk_pipeline, wk_device, wksp_pass, pass_ubo,
            consumer_pass_idx, i);
    }
}

void
descriptor_sets_common_write(const WallkanScene *wk_scene,
    const WallkanDevice *wk_device)
{
    for (uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        VkBuffer shader_param_buf = wk_scene->ubo.parameters_count > 0 ?
            wk_scene->ubo.shader_params[i]->buffer : NULL;

        VkDescriptorBufferInfo buf_infos[2] = {
            (VkDescriptorBufferInfo){
                .buffer = wk_scene->ubo.frame[i]->buffer,
                .offset = 0,
                .range = wk_ubo_frame_data_size()
            },
            (VkDescriptorBufferInfo){
                .buffer = shader_param_buf,
                .offset = 0,
                .range = wk_scene->ubo.total_parameters_size
            }
        };
        VkWriteDescriptorSet desc_writes[2] = {
            (VkWriteDescriptorSet){
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .dstArrayElement = 0,
                .dstBinding = 0,
                .dstSet = wk_scene->pipeline.desc_set.frame[i],
                .pBufferInfo = &buf_infos[0]
            },
            (VkWriteDescriptorSet){
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .dstArrayElement = 0,
                .dstBinding = 0,
                .dstSet = wk_scene->pipeline.desc_set.params[i],
                .pBufferInfo = &buf_infos[1]
            },
        };
        uint32_t desc_write_count = 1;
        if (wk_scene->ubo.parameters_count > 0) {
            desc_write_count++;
        }
        vkUpdateDescriptorSets(wk_device->device, desc_write_count, desc_writes, 0, NULL);
    }
}

void
desc_sets_img_output_write(WallkanPipeline *wk_pipeline, const WallkanDevice *wk_device,
    const WKSPPass *wksp_pass, uint32_t pass_index)
{
    VkDescriptorImageInfo img_infos[2] = {
        (VkDescriptorImageInfo){
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            .imageView = wk_pipeline->passes_frames[pass_index].textures[0].image_view,
            .sampler = VK_NULL_HANDLE
        },
        (VkDescriptorImageInfo){
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            .imageView = wk_pipeline->passes_frames[pass_index].textures[1].image_view,
            .sampler = VK_NULL_HANDLE
        }
    };
    VkWriteDescriptorSet desc_writes[2] = {
        (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .dstArrayElement = 0,
            .dstBinding = 0,
            .dstSet = wk_pipeline->passes_frames[pass_index].desc_set.img_output[0],
            .pImageInfo = &img_infos[0]
        },
        (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .dstArrayElement = 0,
            .dstBinding = 0,
            .dstSet = wk_pipeline->passes_frames[pass_index].desc_set.img_output[1],
            .pImageInfo = &img_infos[1]
        },
    };
    uint32_t desc_write_count = 1;
    if (wksp_pass->self_feedback) {
        desc_write_count++;
    }
    vkUpdateDescriptorSets(wk_device->device, desc_write_count, desc_writes, 0, NULL);
}

void
descriptor_sets_passes_write(ArenaAllocator *alloc, WallkanPipeline *wk_pipeline,
    const WallkanDevice *wk_device, const WKSPContainer *wksp, const WkUBO *pass_ubo)
{
    for (uint32_t i=0; i<wksp->total_passes; i++) {
        const WKSPPass *wksp_pass = &wksp->passes[i];
        desc_sets_inputs_write(alloc, wk_pipeline, wk_device, wksp_pass, pass_ubo, i);
        if (wksp_pass->shader_stage == VK_SHADER_STAGE_COMPUTE_BIT) {
            desc_sets_img_output_write(wk_pipeline, wk_device, wksp_pass, i);
        }
    }
}

void
descriptor_set_layouts_cleanup(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline)
{
    if(wk_pipeline->desc_set.frame_layout){
        vkDestroyDescriptorSetLayout(wk_device->device, wk_pipeline->desc_set.frame_layout, NULL);
        wk_pipeline->desc_set.frame_layout = NULL;
    }
    if(wk_pipeline->desc_set.params_layout){
        vkDestroyDescriptorSetLayout(wk_device->device, wk_pipeline->desc_set.params_layout,
            NULL);
        wk_pipeline->desc_set.params_layout = NULL;
    }
    if(wk_pipeline->desc_set.compute_storage_layout){
        vkDestroyDescriptorSetLayout(wk_device->device,
            wk_pipeline->desc_set.compute_storage_layout, NULL);
        wk_pipeline->desc_set.compute_storage_layout = NULL;
    }
    for (uint32_t i=0; i<wk_pipeline->pass_count; i++) {
        WkPipelinePass *pass = &wk_pipeline->passes_data[i];
        if(pass->input_set_layout){
            vkDestroyDescriptorSetLayout(wk_device->device, pass->input_set_layout, NULL);
            pass->input_set_layout = NULL;
        }
    }
}

void
descriptor_pool_cleanup(WallkanPipeline *wk_pipeline, const WallkanDevice *wk_device){
    if(wk_pipeline->descriptor_pool){
        LOG("descriptor_pool_cleanup: Destroy descriptor pool!");
        vkDestroyDescriptorPool(wk_device->device, wk_pipeline->descriptor_pool, NULL);
        wk_pipeline->descriptor_pool = NULL;
    }
}
