#pragma once

#include "../vk/vk.hpp"

namespace grove::glsl {
struct VertFragProgramSource;
}

namespace grove::vk {

struct SimpleVertFragGraphicsPipelineCreateInfo {
  const VertexBufferDescriptor* vertex_buffer_descriptors;
  uint32_t num_vertex_buffer_descriptors;
  const std::vector<uint32_t>* vert_bytecode;
  const std::vector<uint32_t>* frag_bytecode;
  std::function<void(DefaultConfigureGraphicsPipelineStateParams&)> configure_params;
  std::function<void(GraphicsPipelineStateCreateInfo&)> configure_pipeline_state;
  const PipelineRenderPassInfo* pipeline_render_pass_info;
  VkPipelineLayout pipeline_layout;
};

Result<Pipeline>
create_vert_frag_graphics_pipeline(VkDevice device, const SimpleVertFragGraphicsPipelineCreateInfo* create_info);

void configure_pipeline_create_info(SimpleVertFragGraphicsPipelineCreateInfo* dst,
                                    ArrayView<const VertexBufferDescriptor> vb_descs,
                                    const glsl::VertFragProgramSource& program_source,
                                    const PipelineRenderPassInfo& pass_info,
                                    VkPipelineLayout layout,
                                    decltype(SimpleVertFragGraphicsPipelineCreateInfo().configure_params)&& configure_params,
                                    decltype(SimpleVertFragGraphicsPipelineCreateInfo().configure_pipeline_state)&& configure_pipeline_state);

}