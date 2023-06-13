#pragma once

#include "common.hpp"
#include "grove/common/Unique.hpp"
#include <vector>

namespace grove::vk {

struct PipelineLayout {
  bool is_valid() const {
    return handle != VK_NULL_HANDLE;
  }

  VkPipelineLayout handle{VK_NULL_HANDLE};
};

struct Pipeline {
  bool is_valid() const {
    return handle != VK_NULL_HANDLE;
  }

  VkPipeline handle{VK_NULL_HANDLE};
};

Result<PipelineLayout>
create_pipeline_layout(VkDevice device, const VkPipelineLayoutCreateInfo* create_info);

Result<Pipeline> create_graphics_pipeline(VkDevice device,
                                          const VkGraphicsPipelineCreateInfo* info,
                                          VkPipelineCache pipeline_cache = VK_NULL_HANDLE);
Result<Pipeline> create_compute_pipeline(VkDevice device,
                                         const VkComputePipelineCreateInfo* info,
                                         VkPipelineCache pipeline_cache = VK_NULL_HANDLE);

void destroy_pipeline_layout(PipelineLayout* layout, VkDevice device);
void destroy_pipeline(Pipeline* pipeline, VkDevice device);

Result<VkShaderModule> create_shader_module(VkDevice device, const uint32_t* data, size_t size);
Result<VkShaderModule> create_shader_module(VkDevice device, const std::vector<uint32_t>& data);

Result<Unique<VkShaderModule>>
create_unique_shader_module(VkDevice device, const uint32_t* data, size_t size);

Result<Unique<VkShaderModule>>
create_unique_shader_module(VkDevice device, const std::vector<uint32_t>& data);

void destroy_shader_module(VkShaderModule module, VkDevice device);

VkPipelineLayoutCreateInfo make_pipeline_layout_create_info(const VkDescriptorSetLayout* set_layouts,
                                                            uint32_t num_set_layouts,
                                                            const VkPushConstantRange* push_constants,
                                                            uint32_t num_pc_ranges,
                                                            VkPipelineLayoutCreateFlags flags = 0);

inline VkPipelineLayoutCreateInfo make_empty_pipeline_layout_create_info() {
  VkPipelineLayoutCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  return result;
}

inline VkGraphicsPipelineCreateInfo make_empty_graphics_pipeline_create_info() {
  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  return pipeline_info;
}

}