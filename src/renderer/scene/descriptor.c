#include "renderer/scene/descriptor.h"
#include "arena_alloc.h"
#include "common.h"
#include "err.h"
#include "renderer/device.h"
#include "renderer/scene/pipeline.h"
#include <stdint.h>
#include <vulkan/vulkan_core.h>

WkResult
descriptor_pool_init(WallkanPipeline *wk_pipeline, const WallkanDevice *wk_device,
    const WKSPContainer *wksp)
{
    LOG("init_descriptor_pool: Initializing scene descriptor pool...");
    LOG("init_descriptor_pool: Total types of descriptor: %d", wksp->desc_pool_size_count);
    LOG("init_descriptor_pool: Total pool max sets: %d", wksp->desc_max_sets * MAX_FRAMES_IN_FLIGHT);
    VkDescriptorPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pPoolSizes = wksp->desc_pool_sizes,
        .poolSizeCount = wksp->desc_pool_size_count,
        .maxSets = wksp->desc_max_sets * MAX_FRAMES_IN_FLIGHT,
    };
    WK_TRY(EXPECT_VK(
        vkCreateDescriptorPool(wk_device->device, &pool_create_info, NULL,
            &wk_pipeline->descriptor_pool),
        WK_ERR_VK_CREATE_DESCRIPTOR_POOL_FAILED, "Failed to create descriptor pool!"
    ));
    return WK_OK;
}


static WkResult
init_global_desc_layout(const WallkanDevice *wk_device,
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
            &wk_pipeline->global_desc_layouts.frame_ubo),
        WK_ERR_VK_CREATE_DESCRIPTOR_SET_LAYOUT_FAILED,
        "Failed to create frame UBO descriptor set layout"
    ));

    WK_TRY(EXPECT_VK(
        vkCreateDescriptorSetLayout(wk_device->device, &shader_params_set, NULL,
            &wk_pipeline->global_desc_layouts.shader_params_ubo),
        WK_ERR_VK_CREATE_DESCRIPTOR_SET_LAYOUT_FAILED,
        "Failed to create shader parameters UBO descriptor set layout"
    ));
    return WK_OK;
}

static WkResult
init_pass_desc_layouts(ArenaAllocator *alloc, WallkanPipeline *wk_pipeline,
    const WallkanDevice *wk_device, const WKSPContainer *wksp)
{

    for (uint32_t i=0; i<wksp->total_passes; i++){
        WkPipelinePass *target_pass = &wk_pipeline->passes[i];
        VkDescriptorSetLayout *pass_set_layout = &target_pass->input_set_layout;

        VkDescriptorSetLayoutBinding *input_bindings = arena_alloc(alloc,
            sizeof(VkDescriptorSetLayoutBinding) * wksp->passes[i].channel_count);

        for (uint32_t j=0; j<wksp->passes[i].channel_count; j++) {
            input_bindings[j] = (VkDescriptorSetLayoutBinding){
                .binding = j,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = wksp->passes[i].shader_stage
            };
        }
        VkDescriptorSetLayoutCreateInfo input_set = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = wksp->passes[i].channel_count,
            .pBindings    = (wksp->passes[i].channel_count > 0) ? input_bindings : NULL,
        };
        WK_TRY(EXPECT_VK(
            vkCreateDescriptorSetLayout(wk_device->device, &input_set, NULL,
                pass_set_layout),
            WK_ERR_VK_CREATE_DESCRIPTOR_SET_LAYOUT_FAILED,
            "Failed to create shader parameters UBO descriptor set layout"
        ));

        if (wksp->passes[i].shader_stage == VK_SHADER_STAGE_COMPUTE_BIT) {
            VkDescriptorSetLayoutBinding out_img_binding = {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .stageFlags = wksp->passes[i].shader_stage
            };
            VkDescriptorSetLayoutCreateInfo out_img_set = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 1,
                .pBindings    = &out_img_binding,
            };
            WK_TRY(EXPECT_VK(
                vkCreateDescriptorSetLayout(wk_device->device, &out_img_set, NULL,
                    &target_pass->compute.out_img_set_layout),
                WK_ERR_VK_CREATE_DESCRIPTOR_SET_LAYOUT_FAILED,
                "Failed to create shader parameters UBO descriptor set layout"
            ));
        }
    }
    return WK_OK;
}

WkResult
descriptor_set_layouts_init(ArenaAllocator *alloc, WallkanPipeline *wk_pipeline,
    const WallkanDevice *wk_device, const WKSPContainer *wksp)
{
    WK_TRY(init_global_desc_layout(wk_device, wk_pipeline));
    WK_TRY(init_pass_desc_layouts(alloc, wk_pipeline, wk_device, wksp));
    return WK_OK;
}

void
descriptor_set_layouts_cleanup(const WallkanDevice *wk_device, WallkanPipeline *wk_pipeline)
{
    if(wk_pipeline->global_desc_layouts.frame_ubo){
        vkDestroyDescriptorSetLayout(wk_device->device, wk_pipeline->global_desc_layouts.frame_ubo, NULL);
        wk_pipeline->global_desc_layouts.frame_ubo = NULL;
    }
    if(wk_pipeline->global_desc_layouts.shader_params_ubo){
        vkDestroyDescriptorSetLayout(wk_device->device, wk_pipeline->global_desc_layouts.shader_params_ubo,
            NULL);
        wk_pipeline->global_desc_layouts.shader_params_ubo = NULL;
    }
    for (uint32_t i=0; i<wk_pipeline->pass_count; i++) {
        WkPipelinePass *pass = &wk_pipeline->passes[i];
        if(pass->input_set_layout){
            vkDestroyDescriptorSetLayout(wk_device->device, pass->input_set_layout, NULL);
            pass->input_set_layout = NULL;
        }
        if(pass->shader_stage == VK_SHADER_STAGE_COMPUTE_BIT){
            if(pass->compute.out_img_set_layout){
                vkDestroyDescriptorSetLayout(wk_device->device, pass->compute.out_img_set_layout, NULL);
                pass->compute.out_img_set_layout = NULL;
            }
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
