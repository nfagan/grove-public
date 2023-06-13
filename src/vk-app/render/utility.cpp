#include "utility.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

using CreateInfo = SimpleVertFragGraphicsPipelineCreateInfo;
using ConfigureParams = decltype(SimpleVertFragGraphicsPipelineCreateInfo().configure_params);
using ConfigurePipelineState = decltype(SimpleVertFragGraphicsPipelineCreateInfo().configure_pipeline_state);

Result<Pipeline>
vk::create_vert_frag_graphics_pipeline(VkDevice device, const CreateInfo* create_info) {
  VertexInputDescriptors input_descrs{};
  to_vk_vertex_input_descriptors(
    create_info->num_vertex_buffer_descriptors,
    create_info->vertex_buffer_descriptors,
    &input_descrs);

  DefaultConfigureGraphicsPipelineStateParams params{input_descrs};
  params.raster_samples = create_info->pipeline_render_pass_info->raster_samples;
  if (create_info->configure_params) {
    create_info->configure_params(params);
  }

  GraphicsPipelineStateCreateInfo state{};
  default_configure(&state, params);
  if (create_info->configure_pipeline_state) {
    create_info->configure_pipeline_state(state);
  }

  return create_vert_frag_graphics_pipeline(
    device,
    *create_info->vert_bytecode,
    *create_info->frag_bytecode,
    &state,
    create_info->pipeline_layout,
    create_info->pipeline_render_pass_info->render_pass,
    create_info->pipeline_render_pass_info->subpass);
}

void vk::configure_pipeline_create_info(SimpleVertFragGraphicsPipelineCreateInfo* dst,
                                        ArrayView<const VertexBufferDescriptor> vb_descs,
                                        const glsl::VertFragProgramSource& program_source,
                                        const PipelineRenderPassInfo& pass_info,
                                        VkPipelineLayout layout,
                                        ConfigureParams&& configure_params,
                                        ConfigurePipelineState&& configure_pipeline_state) {
  dst->vertex_buffer_descriptors = vb_descs.data();
  dst->num_vertex_buffer_descriptors = uint32_t(vb_descs.size());
  dst->vert_bytecode = &program_source.vert_bytecode;
  dst->frag_bytecode = &program_source.frag_bytecode;
  dst->configure_params = std::move(configure_params);
  dst->configure_pipeline_state = std::move(configure_pipeline_state);
  dst->pipeline_render_pass_info = &pass_info;
  dst->pipeline_layout = layout;
}

GROVE_NAMESPACE_END