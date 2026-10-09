#include "arena_alloc.h"
#include "renderer/scene/pipeline.h"
#include "common.h"
#include "err.h"
#include "renderer/scene/descriptor.h"
#include "wksp/parser.h"
#include <stdint.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>

static WkResult
init_pipeline_layout(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline,
    uint32_t pass_idx)
{
    VkDescriptorSetLayout desc_set_layouts[4] = {
        wk_pipeline->global_desc_layouts.frame_ubo,
        wk_pipeline->global_desc_layouts.shader_params_ubo,
        wk_pipeline->passes[pass_idx].input_set_layout,
        wk_pipeline->passes[pass_idx].compute.out_img_set_layout
    };
    uint32_t set_layout_count;
    if(wk_pipeline->passes[pass_idx].shader_stage == VK_SHADER_STAGE_COMPUTE_BIT){
        // Include the out image set layout that compute shaders need to write their output
        set_layout_count = 4;
    }else{
        set_layout_count = 3;
    }
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pSetLayouts = desc_set_layouts,
        .setLayoutCount = set_layout_count,
    };
    WK_TRY(EXPECT_VK(
        vkCreatePipelineLayout(wk_device->device, &pipeline_layout_create_info, NULL,
            &wk_pipeline->passes[pass_idx].pipeline_layout),
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
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO
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

WkResult
wk_pipeline_init(ArenaAllocator *alloc, const WallkanDevice *wk_device,
    WallkanPipeline *wk_pipeline, const WKSPContainer *wksp)
{
    wk_pipeline->pass_count = wksp->total_passes;
    wk_pipeline->passes = calloc(wk_pipeline->pass_count, sizeof(WkPipelinePass));
    if (!wk_pipeline->passes) {
        return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Allocation failure!");
    }
    WK_TRY(descriptor_pool_init(wk_pipeline, wk_device, wksp));
    WK_TRY(descriptor_set_layouts_init(alloc, wk_pipeline, wk_device, wksp));
    for (uint32_t i=0; i<wk_pipeline->pass_count; i++) {
        WkPipelinePass *pipeline_pass = &wk_pipeline->passes[i];
        pipeline_pass->shader_stage = wksp->passes[i].shader_stage;
        WK_TRY(init_pipeline_layout(wk_device, wk_pipeline, i));
        LOG("Init pipeline for %s, type: %d", wksp->passes[i].name, wksp->passes[i].shader_stage);
        if (pipeline_pass->shader_stage == VK_SHADER_STAGE_COMPUTE_BIT) {
            WK_TRY(vk_pipeline_init_compute(wk_device, pipeline_pass, wksp->shader_modules[i]));
        }else{
            WK_TRY(vk_pipeline_init_graphics(wk_device, pipeline_pass,
                wksp->shader_modules[wksp->vert_module_idx], wksp->shader_modules[i]));
        }
    }
    return WK_OK;
}

void
wk_pipeline_cleanup(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline)
{
    for (uint32_t i=0; i<wk_pipeline->pass_count; i++) {
        if(wk_pipeline->passes[i].pipeline_layout){
            vkDestroyPipelineLayout(wk_device->device, wk_pipeline->passes[i].pipeline_layout, NULL);
            wk_pipeline->passes[i].pipeline_layout = NULL;
        }
        if (wk_pipeline->passes[i].pipeline) {
            vkDestroyPipeline(wk_device->device, wk_pipeline->passes[i].pipeline, NULL);
            wk_pipeline->passes[i].pipeline = NULL;
        }
    }
    descriptor_set_layouts_cleanup(wk_device, wk_pipeline);
    descriptor_pool_cleanup(wk_pipeline, wk_device);

    wk_pipeline->pass_count = 0;
    if(wk_pipeline->passes){
        free(wk_pipeline->passes);
        wk_pipeline->passes = NULL;
    }

}
