#include "shader.hpp"
#include "grove/common/common.hpp"
#include <vector>

GROVE_NAMESPACE_BEGIN

vk::Result<vk::PipelineLayout> vk::create_pipeline_layout(VkDevice device,
                                                          const VkPipelineLayoutCreateInfo* info) {
  PipelineLayout result{};
  auto layout_res = vkCreatePipelineLayout(device, info, GROVE_VK_ALLOC, &result.handle);
  if (layout_res != VK_SUCCESS) {
    return {layout_res, "Failed to create pipeline layout."};
  } else {
    return result;
  }
}

VkPipelineLayoutCreateInfo
vk::make_pipeline_layout_create_info(const VkDescriptorSetLayout* set_layouts,
                                     uint32_t num_set_layouts,
                                     const VkPushConstantRange* push_constants,
                                     uint32_t num_pc_ranges,
                                     VkPipelineLayoutCreateFlags flags) {
  auto info = make_empty_pipeline_layout_create_info();
  info.flags = flags;
  info.setLayoutCount = num_set_layouts;
  info.pSetLayouts = set_layouts;
  info.pushConstantRangeCount = num_pc_ranges;
  info.pPushConstantRanges = push_constants;
  return info;
}

vk::Result<vk::Pipeline> vk::create_graphics_pipeline(VkDevice device,
                                                      const VkGraphicsPipelineCreateInfo* info,
                                                      VkPipelineCache pipeline_cache) {
  Pipeline pipeline{};
  auto pipe_res = vkCreateGraphicsPipelines(
    device, pipeline_cache, 1, info, GROVE_VK_ALLOC, &pipeline.handle);
  if (pipe_res != VK_SUCCESS) {
    return {pipe_res, "Failed to create graphics pipeline."};
  } else {
    return pipeline;
  }
}

vk::Result<vk::Pipeline> vk::create_compute_pipeline(VkDevice device,
                                                     const VkComputePipelineCreateInfo* info,
                                                     VkPipelineCache pipeline_cache) {
  Pipeline pipeline{};
  auto pipe_res = vkCreateComputePipelines(
    device, pipeline_cache, 1, info, GROVE_VK_ALLOC, &pipeline.handle);
  if (pipe_res != VK_SUCCESS) {
    return {pipe_res, "Failed to create compute pipeline."};
  } else {
    return pipeline;
  }
}

void vk::destroy_pipeline_layout(vk::PipelineLayout* layout, VkDevice device) {
  if (device != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, layout->handle, GROVE_VK_ALLOC);
    layout->handle = VK_NULL_HANDLE;
  } else {
    assert(layout->handle == VK_NULL_HANDLE);
  }
}

void vk::destroy_pipeline(vk::Pipeline* pipeline, VkDevice device) {
  if (device != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, pipeline->handle, GROVE_VK_ALLOC);
    pipeline->handle = VK_NULL_HANDLE;
  } else {
    assert(pipeline->handle == VK_NULL_HANDLE);
  }
}

vk::Result<VkShaderModule> vk::create_shader_module(VkDevice device,
                                                    const uint32_t* data,
                                                    size_t size) {
  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = size;
  create_info.pCode = data;
  VkShaderModule result;
  auto res = vkCreateShaderModule(device, &create_info, GROVE_VK_ALLOC, &result);
  if (res != VK_SUCCESS) {
    return {res, "Failed to create shader module."};
  } else {
    return result;
  }
}

vk::Result<VkShaderModule> vk::create_shader_module(VkDevice device,
                                                    const std::vector<uint32_t>& data) {
  return create_shader_module(device, data.data(), data.size() * sizeof(uint32_t));
}

vk::Result<Unique<VkShaderModule>>
vk::create_unique_shader_module(VkDevice device, const uint32_t* data, size_t size) {
  auto res = create_shader_module(device, data, size);
  if (!res) {
    return error_cast<Unique<VkShaderModule>>(res);
  } else {
    return Unique<VkShaderModule>{
      std::move(res.value),
      [device](VkShaderModule* module) {
        vk::destroy_shader_module(*module, device);
      }
    };
  }
}

vk::Result<Unique<VkShaderModule>>
vk::create_unique_shader_module(VkDevice device, const std::vector<uint32_t>& data) {
  return create_unique_shader_module(device, data.data(), data.size() * sizeof(uint32_t));
}

void vk::destroy_shader_module(VkShaderModule module, VkDevice device) {
  vkDestroyShaderModule(device, module, GROVE_VK_ALLOC);
}

GROVE_NAMESPACE_END
