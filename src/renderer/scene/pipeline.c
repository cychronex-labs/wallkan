#include "arena_alloc.h"
#include "renderer/scene/pipeline.h"
#include "common.h"
#include "err.h"
#include "renderer/scene/descriptor.h"
#include "renderer/scene/scene.h"
#include "renderer/scene/texture.h"
#include "renderer/scene/ubo.h"
#include "wksp/parser.h"
#include <stdint.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>
#include "renderer/scene/push_constants.h"

static WkResult
init_pipeline_layout(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline,
    uint32_t pass_idx)
{
    VkDescriptorSetLayout desc_set_layouts[4] = {
        wk_pipeline->desc_set.frame_layout,
        wk_pipeline->desc_set.params_layout,
        wk_pipeline->passes_data[pass_idx].input_set_layout,
        wk_pipeline->desc_set.compute_storage_layout
    };
    uint32_t set_layout_count;
    if(wk_pipeline->passes_data[pass_idx].shader_stage == VK_SHADER_STAGE_COMPUTE_BIT){
        // Include the out image set layout that compute shaders need to write their output
        set_layout_count = 4;
    }else{
        set_layout_count = 3;
    }
    VkPushConstantRange pc_range = {
        .offset = 0,
        .size = sizeof(WkPushConstants),
        .stageFlags = wk_pipeline->passes_data[pass_idx].shader_stage
    };
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pSetLayouts = desc_set_layouts,
        .setLayoutCount = set_layout_count,
    };
    if(wk_pipeline->passes_frames[pass_idx].iterations > 1){
        pipeline_layout_create_info.pPushConstantRanges = &pc_range;
        pipeline_layout_create_info.pushConstantRangeCount = 1;
    }
    WK_TRY(EXPECT_VK(
        vkCreatePipelineLayout(wk_device->device, &pipeline_layout_create_info, NULL,
            &wk_pipeline->passes_data[pass_idx].pipeline_layout),
        WK_ERR_VK_CREATE_PIPELINE_LAYOUT_FAILED, "Failed to create pipeline layout!"
    ));
    return WK_OK;
}

static WkResult
vk_pipeline_init_compute(const WallkanDevice *wk_device, WkPipelinePass *pipeline_pass,
    VkShaderModule shader_module)
{
    VkPipelineShaderStageCreateInfo stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader_module,
        .pName = "main"
    };
    VkComputePipelineCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage_info,
        .layout = pipeline_pass->pipeline_layout
    };
    WK_TRY(EXPECT_VK(
        vkCreateComputePipelines(wk_device->device, NULL, 1, &create_info, NULL,
            &pipeline_pass->pipeline),
        WK_ERR_VK_CREATE_COMPUTE_PIPELINE_FAILED, "Failed to create compute pipeline!"
    ));
    return WK_OK;
}

static WkResult
vk_pipeline_init_graphics(const WallkanDevice *wk_device, WkPipelinePass *pipeline_pass,
    VkShaderModule vert_shader_module, VkShaderModule frag_shader_module)
{
    VkPipelineShaderStageCreateInfo stages_info[2] = {
        (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_shader_module,
            .pName = "main"
        },
        (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_shader_module,
            .pName = "main"
        }
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rasterization_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .lineWidth = 1.0f
    };
    VkPipelineMultisampleStateCreateInfo multisample_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo color_blend_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = (VkDynamicState[]){
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        }
    };

    VkPipelineRenderingCreateInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &pipeline_pass->image_format
    };
    VkGraphicsPipelineCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_info,
        .stageCount = 2,
        .pStages = stages_info,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState = &multisample_state,
        .pColorBlendState = &color_blend_state,
        .pDynamicState = &dynamic_state,
        .layout = pipeline_pass->pipeline_layout,
    };
    WK_TRY(EXPECT_VK(
        vkCreateGraphicsPipelines(wk_device->device, NULL, 1, &create_info, NULL,
            &pipeline_pass->pipeline),
        WK_ERR_VK_CREATE_GRAPHICS_PIPELINE_FAILED, "Failed to create graphics pipeline!"
    ));
    return WK_OK;
}

bool pipeline_is_main(WallkanPipeline *wk_pipeline, uint32_t idx){
    return idx == (wk_pipeline->pass_count - 1);
}

static WkResult
init_pipeline_textures(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline,
    VkExtent2D final_resolution)
{
    for (uint32_t i=0; i<wk_pipeline->pass_count; i++) {
        // Image pass textures are not managed by WallkanPipeline, But WallkanScene itself.
        if(pipeline_is_main(wk_pipeline, i)) continue;

        const WkPipelinePass *pass_data = &wk_pipeline->passes_data[i];
        WkPassFrames *passes_frames = &wk_pipeline->passes_frames[i];

        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        if(pass_data->shader_stage == VK_SHADER_STAGE_FRAGMENT_BIT){
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }else{
            usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        }
        VkExtent3D extent = {
            .width = scaled_res_component(final_resolution.width, pass_data->scale),
            .height = scaled_res_component(final_resolution.height, pass_data->scale),
            .depth = 1
        };
        WkTexture *textures = passes_frames->textures;
        WK_TRY(wk_texture_create(wk_device, pass_data->image_format, usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, extent, textures));
        if (pass_data->self_feedback) {
            WK_TRY(wk_texture_create(wk_device, pass_data->image_format, usage,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, extent, &textures[1]));
        }else{
            textures[1] = textures[0];
        }
    }
    return WK_OK;
}

// Requires pass ubo, so descriptors can be initialized for it
// About wk_scene parameter which may feel off at first:
//  So, UBO and data is managed by Scene which is because the Data required for
//  FrameData, ShaderParameters etc are not within pipeline scope
//  BUT pipeline holds the descriptors because descriptor is only required within pipeline scope
WkResult
wk_pipeline_init(ArenaAllocator *alloc, const WallkanDevice *wk_device,
    WallkanPipeline *wk_pipeline, const WKSPContainer *wksp, const WallkanScene *wk_scene,
    VkExtent2D final_resolution)
{
    wk_pipeline->pass_count = wksp->total_passes;
    wk_pipeline->passes_data = calloc(wk_pipeline->pass_count, sizeof(WkPipelinePass));
    wk_pipeline->passes_frames = calloc(wk_pipeline->pass_count, sizeof(WkPassFrames));

    if (!wk_pipeline->passes_data || !wk_pipeline->passes_frames) {
        return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Allocation failure!");
    }

    WK_TRY(descriptor_pool_init(wk_pipeline, wk_device, wksp));
    WK_TRY(descriptor_set_layouts_init(alloc, wk_pipeline, wk_device, wksp));
    for (uint32_t i=0; i<wk_pipeline->pass_count; i++) {
        WkPipelinePass *pipeline_pass = &wk_pipeline->passes_data[i];
        WkPassFrames *pass_frames = &wk_pipeline->passes_frames[i];

        pipeline_pass->shader_stage = wksp->passes[i].shader_stage;
        pipeline_pass->scale = wksp->passes[i].scale;
        pipeline_pass->image_format = wksp->passes[i].selected_img_format;
        pipeline_pass->self_feedback = wksp->passes[i].self_feedback;
        pipeline_pass->channel_count = wksp->passes[i].channel_count;

        pass_frames->iterations = wksp->passes[i].iterations;

        if (pipeline_pass->channel_count > 0) {
            pipeline_pass->channels = calloc(wksp->passes[i].channel_count, sizeof(WkPassChannel));
            if(!pipeline_pass->channels) return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Allocation failure!");

            for (uint32_t j=0; j<pipeline_pass->channel_count; j++) {
                pipeline_pass->channels[j] = (WkPassChannel){
                    .type = wksp->passes[i].channels[j].type,
                    .producer_idx = wksp->passes[i].channels[j].producer_idx,
                };
                WK_TRY(EXPECT_VK(
                    vkCreateSampler(wk_device->device,
                        &wksp->passes[i].channels[j].sampler_create_info, NULL,
                        &pipeline_pass->channels[j].sampler),
                    WK_ERR_VK_CREATE_SAMPLER_FAILED, "Failed to create channel sampler!"
                ));
            }
        }else{
            pipeline_pass->channels = NULL;
        }

        WK_TRY(init_pipeline_layout(wk_device, wk_pipeline, i));
        LOG("Init pipeline for %s, type: %d", wksp->passes[i].name, wksp->passes[i].shader_stage);
        if (pipeline_pass->shader_stage == VK_SHADER_STAGE_COMPUTE_BIT) {
            WK_TRY(vk_pipeline_init_compute(wk_device, pipeline_pass, wksp->shader_modules[i]));
        }else{
            WK_TRY(vk_pipeline_init_graphics(wk_device, pipeline_pass,
                wksp->shader_modules[wksp->vert_module_idx], wksp->shader_modules[i]));
        }
    }
    WK_TRY(init_pipeline_textures(wk_device, wk_pipeline, final_resolution));
    WK_TRY(descriptor_sets_alloc(wk_pipeline, wk_device, wksp));
    descriptor_sets_common_write(wk_scene, wk_device);
    descriptor_sets_passes_write(alloc, wk_pipeline, wk_device, wksp, wk_scene->ubo.pass);
    return WK_OK;
}

void
wk_pass_textures_cleanup(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline, uint32_t pipeline_idx)
{
    wk_texture_destroy(wk_device, &wk_pipeline->passes_frames[pipeline_idx].textures[0]);
    if (wk_pipeline->passes_data[pipeline_idx].self_feedback) {
        wk_texture_destroy(wk_device, &wk_pipeline->passes_frames[pipeline_idx].textures[1]);
    }else{
        wk_pipeline->passes_frames[pipeline_idx].textures[1].image = NULL;
        wk_pipeline->passes_frames[pipeline_idx].textures[1].image_view = NULL;
        wk_pipeline->passes_frames[pipeline_idx].textures[1].memory = NULL;
    }
}


WkResult
wk_pipeline_resize_textures(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline,
    VkExtent2D final_resolution)
{
    for (uint32_t i=0; i<wk_pipeline->pass_count; i++) {
        wk_pass_textures_cleanup(wk_device, wk_pipeline, i);
    }
    WK_TRY(init_pipeline_textures(wk_device, wk_pipeline, final_resolution));
    return WK_OK;
}

void
wk_pipeline_textures_cleanup(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline){
    for (uint32_t i=0; i<wk_pipeline->pass_count; i++) {
        wk_pass_textures_cleanup(wk_device, wk_pipeline, i);
    }
}

void
wk_pipeline_cleanup(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline)
{
    for (uint32_t i=0; i<wk_pipeline->pass_count; i++) {
        wk_pass_textures_cleanup(wk_device, wk_pipeline, i);
        if(wk_pipeline->passes_data[i].pipeline_layout){
            vkDestroyPipelineLayout(wk_device->device, wk_pipeline->passes_data[i].pipeline_layout, NULL);
            wk_pipeline->passes_data[i].pipeline_layout = NULL;
        }
        if (wk_pipeline->passes_data[i].pipeline) {
            vkDestroyPipeline(wk_device->device, wk_pipeline->passes_data[i].pipeline, NULL);
            wk_pipeline->passes_data[i].pipeline = NULL;
        }
        if(wk_pipeline->passes_data[i].channels){
            for (uint32_t j=0; j<wk_pipeline->passes_data[i].channel_count; j++) {
                if(wk_pipeline->passes_data[i].channels[j].sampler){
                    vkDestroySampler(wk_device->device,
                        wk_pipeline->passes_data[i].channels[j].sampler, NULL);
                    wk_pipeline->passes_data[i].channels[j].sampler = NULL;
                }
            }
            free(wk_pipeline->passes_data[i].channels);
            wk_pipeline->passes_data[i].channels = NULL;
        }
    }
    descriptor_set_layouts_cleanup(wk_device, wk_pipeline);
    descriptor_pool_cleanup(wk_pipeline, wk_device);

    wk_pipeline->pass_count = 0;
    if(wk_pipeline->passes_frames){
        free(wk_pipeline->passes_frames);
        wk_pipeline->passes_frames = NULL;
    }
    if(wk_pipeline->passes_data){
        free(wk_pipeline->passes_data);
        wk_pipeline->passes_data = NULL;
    }
}
