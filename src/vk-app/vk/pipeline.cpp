#include "pipeline.hpp"
#include "common.hpp"
#include "grove/vk/shader.hpp"
#include "grove/common/common.hpp"
#include "grove/visual/types.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

VkPipelineShaderStageCreateInfo make_vertex_shader_stage_create_info(VkShaderModule module) {
  VkPipelineShaderStageCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  result.stage = VK_SHADER_STAGE_VERTEX_BIT;
  result.module = module;
  result.pName = "main";
  return result;
}

VkPipelineShaderStageCreateInfo make_fragment_shader_stage_create_info(VkShaderModule module) {
  VkPipelineShaderStageCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  result.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  result.module = module;
  result.pName = "main";
  return result;
}

VkPipelineShaderStageCreateInfo make_compute_shader_stage_create_info(VkShaderModule module) {
  VkPipelineShaderStageCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  result.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  result.module = module;
  result.pName = "main";
  return result;
}

} //  anon

VkGraphicsPipelineCreateInfo
vk::make_graphics_pipeline_create_info(const VkPipelineShaderStageCreateInfo* shader_stages,
                                       uint32_t num_stages,
                                       const GraphicsPipelineStateCreateInfo* state,
                                       VkPipelineLayout layout,
                                       VkRenderPass render_pass,
                                       uint32_t subpass,
                                       VkPipeline base_pipeline_handle,
                                       int32_t base_pipeline_index) {
  const bool has_color_blend = state->color_blend.attachmentCount > 0;
  const bool has_dynamic_state = state->dynamic_state.dynamicStateCount > 0;
  //
  VkGraphicsPipelineCreateInfo res = make_empty_graphics_pipeline_create_info();
  res.stageCount = num_stages;
  res.pStages = shader_stages;
  res.pVertexInputState = &state->vertex_input;
  res.pInputAssemblyState = &state->input_assembly;
  res.pViewportState = &state->viewport;
  res.pRasterizationState = &state->rasterization;
  res.pMultisampleState = &state->multisampling;
  res.pDepthStencilState = &state->depth_stencil;
  res.pColorBlendState = has_color_blend ? &state->color_blend : nullptr;
  res.pDynamicState = has_dynamic_state ? &state->dynamic_state : nullptr;
  res.layout = layout;
  res.renderPass = render_pass;
  res.subpass = subpass;
  res.basePipelineHandle = base_pipeline_handle;
  res.basePipelineIndex = base_pipeline_index;
  return res;
}

vk::VertFragPipelineShaderStageCreateInfo
vk::make_vert_frag_pipeline_shader_stage_create_info(VkShaderModule vert, VkShaderModule frag) {
  VertFragPipelineShaderStageCreateInfo result{};
  result.vert_frag[0] = make_vertex_shader_stage_create_info(vert);
  result.vert_frag[1] = make_fragment_shader_stage_create_info(frag);
  return result;
}

void vk::default_vertex_input(VkPipelineVertexInputStateCreateInfo* state,
                              const VkVertexInputBindingDescription* bindings,
                              uint32_t num_bindings,
                              const VkVertexInputAttributeDescription* attributes,
                              uint32_t num_attrs) {
  *state = make_vertex_input_state_create_info(
    bindings,
    num_bindings,
    attributes,
    num_attrs);
}

void vk::default_input_assembly(VkPipelineInputAssemblyStateCreateInfo* state,
                                VkPrimitiveTopology topology,
                                bool prim_restart_enabled) {
  *state = make_input_assembly_state_create_info(topology, prim_restart_enabled);
}

void vk::default_dynamic_viewport(VkPipelineViewportStateCreateInfo* state) {
  *state = make_dynamic_viewport_scissor_rect_pipeline_viewport_state_create_info();
}

void vk::default_rasterization(VkPipelineRasterizationStateCreateInfo* state,
                               VkCullModeFlags cull_mode,
                               VkFrontFace front_face) {
  *state = make_default_pipeline_rasterization_state_create_info(cull_mode, front_face);
}

void vk::default_multisampling(VkPipelineMultisampleStateCreateInfo* state,
                               VkSampleCountFlagBits samples) {
  *state = vk::make_default_pipeline_multisample_state_create_info(samples);
}

void vk::default_depth_stencil(VkPipelineDepthStencilStateCreateInfo* state) {
  *state = vk::make_default_pipeline_depth_stencil_state_create_info();
}

void vk::default_color_blend(VkPipelineColorBlendStateCreateInfo* state,
                         const VkPipelineColorBlendAttachmentState* attachments,
                         uint32_t num_attachments) {
  *state = make_default_pipeline_color_blend_state_create_info(attachments, num_attachments);
}

void vk::attachment_alpha_blend_disabled(VkPipelineColorBlendAttachmentState* state) {
  *state = make_alpha_blend_disabled_color_blend_attachment_state();
}

void vk::attachment_alpha_blend_enabled(VkPipelineColorBlendAttachmentState* state) {
  *state = make_alpha_blend_enabled_color_blend_attachment_state();
}

void vk::default_dynamic_state(VkPipelineDynamicStateCreateInfo* state) {
  *state = make_dynamic_viewport_scissor_rect_pipeline_dynamic_state_create_info();
}

void vk::default_configure(GraphicsPipelineStateCreateInfo* state,
                           const DefaultConfigureGraphicsPipelineStateParams& params) {
  default_vertex_input(
    &state->vertex_input,
    params.bindings,
    params.num_bindings,
    params.attributes,
    params.num_attributes);
  default_input_assembly(&state->input_assembly, params.topology);
  default_dynamic_viewport(&state->viewport);
  default_rasterization(&state->rasterization, params.cull_mode, params.front_face);
  default_multisampling(&state->multisampling, params.raster_samples);

  const uint32_t num_color_attach = params.num_color_attachments;
  assert(num_color_attach <= 16);
  for (uint32_t i = 0; i < num_color_attach; i++) {
    if (params.blend_enabled[i]) {
      attachment_alpha_blend_enabled(&state->color_blend_attachments[i]);
    } else {
      attachment_alpha_blend_disabled(&state->color_blend_attachments[i]);
    }
  }

  default_color_blend(&state->color_blend, state->color_blend_attachments, num_color_attach);
  default_depth_stencil(&state->depth_stencil);
  default_dynamic_state(&state->dynamic_state);
}

VkPipelineDepthStencilStateCreateInfo
vk::make_default_pipeline_depth_stencil_state_create_info(VkCompareOp compare_op,
                                                          VkBool32 enable_depth_test,
                                                          VkBool32 enable_depth_write,
                                                          float min_depth_bounds,
                                                          float max_depth_bounds,
                                                          VkBool32 enable_stencil_test) {
  VkPipelineDepthStencilStateCreateInfo depth_stencil_state{};
  depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil_state.depthCompareOp = compare_op;
  depth_stencil_state.depthTestEnable = enable_depth_test;
  depth_stencil_state.depthWriteEnable = enable_depth_write;
  depth_stencil_state.minDepthBounds = min_depth_bounds;
  depth_stencil_state.maxDepthBounds = max_depth_bounds;
  depth_stencil_state.stencilTestEnable = enable_stencil_test;
  return depth_stencil_state;
}

VkPipelineMultisampleStateCreateInfo
vk::make_default_pipeline_multisample_state_create_info(VkSampleCountFlagBits num_samples) {
  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = num_samples;
  multisampling.minSampleShading = 1.0f;
  multisampling.pSampleMask = nullptr;
  multisampling.alphaToCoverageEnable = VK_FALSE;
  multisampling.alphaToOneEnable = VK_FALSE;
  return multisampling;
}

VkPipelineRasterizationStateCreateInfo
vk::make_default_pipeline_rasterization_state_create_info(VkCullModeFlags cull_mode,
                                                          VkFrontFace front_face) {
  auto raster_state_info = vk::make_empty_rasterization_state_create_info();
  raster_state_info.depthClampEnable = VK_FALSE;
  raster_state_info.rasterizerDiscardEnable = VK_FALSE;
  raster_state_info.polygonMode = VK_POLYGON_MODE_FILL;
  raster_state_info.lineWidth = 1.0f;
  raster_state_info.cullMode = cull_mode;
  raster_state_info.frontFace = front_face;
  raster_state_info.depthBiasEnable = VK_FALSE;
  return raster_state_info;
}

VkPipelineViewportStateCreateInfo
vk::make_dynamic_viewport_scissor_rect_pipeline_viewport_state_create_info() {
  return vk::make_viewport_state_create_info(nullptr, 1, nullptr, 1);
}

VkPipelineDynamicStateCreateInfo
vk::make_dynamic_viewport_scissor_rect_pipeline_dynamic_state_create_info() {
  thread_local const VkDynamicState dynamic_states[2]{
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };
  return vk::make_pipeline_dynamic_state_create_info(dynamic_states, 2);
}

VkPipelineColorBlendStateCreateInfo
vk::make_default_pipeline_color_blend_state_create_info(const VkPipelineColorBlendAttachmentState* attachments,
                                                        uint32_t num_attachments) {
  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.logicOp = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = num_attachments;
  color_blending.pAttachments = attachments;
  return color_blending;
}

VkPipelineVertexInputStateCreateInfo
vk::make_vertex_input_state_create_info(const VkVertexInputBindingDescription* binding_descriptions,
                                        uint32_t num_bindings,
                                        const VkVertexInputAttributeDescription* attr_descriptions,
                                        uint32_t num_attrs) {
  VkPipelineVertexInputStateCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  result.vertexBindingDescriptionCount = num_bindings;
  result.pVertexBindingDescriptions = binding_descriptions;
  result.vertexAttributeDescriptionCount = num_attrs;
  result.pVertexAttributeDescriptions = attr_descriptions;
  return result;
}

VkVertexInputBindingDescription vk::make_vertex_input_binding_description(uint32_t binding,
                                                                          uint32_t stride,
                                                                          VkVertexInputRate input_rate) {
  VkVertexInputBindingDescription result{};
  result.binding = binding;
  result.stride = stride;
  result.inputRate = input_rate;
  return result;
}

VkVertexInputAttributeDescription vk::make_vertex_input_attribute_description(uint32_t binding,
                                                                              uint32_t location,
                                                                              VkFormat format,
                                                                              uint32_t offset) {
  VkVertexInputAttributeDescription result{};
  result.binding = binding;
  result.location = location;
  result.format = format;
  result.offset = offset;
  return result;
}

VkPipelineInputAssemblyStateCreateInfo
vk::make_input_assembly_state_create_info(VkPrimitiveTopology topology, bool prim_restart_enabled) {
  VkPipelineInputAssemblyStateCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  result.topology = topology;
  result.primitiveRestartEnable = prim_restart_enabled;
  return result;
}

VkPipelineViewportStateCreateInfo vk::make_viewport_state_create_info(const VkViewport* viewports,
                                                                      uint32_t num_viewports,
                                                                      const VkRect2D* scissors,
                                                                      uint32_t num_scissors) {
  VkPipelineViewportStateCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  result.viewportCount = num_viewports;
  result.pViewports = viewports;
  result.scissorCount = num_scissors;
  result.pScissors = scissors;
  return result;
}

VkPipelineRasterizationStateCreateInfo vk::make_empty_rasterization_state_create_info() {
  VkPipelineRasterizationStateCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  return result;
}

VkPipelineColorBlendAttachmentState vk::make_alpha_blend_enabled_color_blend_attachment_state() {
  VkPipelineColorBlendAttachmentState result{};
  result.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
    VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT |
    VK_COLOR_COMPONENT_A_BIT;
  result.blendEnable = VK_TRUE;
  result.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  result.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  result.colorBlendOp = VK_BLEND_OP_ADD;
  result.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  result.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  result.alphaBlendOp = VK_BLEND_OP_ADD;
  return result;
}

VkPipelineColorBlendAttachmentState vk::make_alpha_blend_disabled_color_blend_attachment_state() {
  VkPipelineColorBlendAttachmentState result{};
  result.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
    VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT |
    VK_COLOR_COMPONENT_A_BIT;
  result.blendEnable = VK_FALSE;
  result.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  result.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  result.colorBlendOp = VK_BLEND_OP_ADD;
  result.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  result.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  result.alphaBlendOp = VK_BLEND_OP_ADD;
  return result;
}

VkPipelineDynamicStateCreateInfo
vk::make_pipeline_dynamic_state_create_info(const VkDynamicState* states, uint32_t num_states) {
  VkPipelineDynamicStateCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  result.pDynamicStates = states;
  result.dynamicStateCount = num_states;
  return result;
}

Result<Pipeline> vk::create_vert_frag_graphics_pipeline(VkDevice device,
                                                        const std::vector<uint32_t>& vert_bytecode,
                                                        const std::vector<uint32_t>& frag_bytecode,
                                                        const GraphicsPipelineStateCreateInfo* state_create_info,
                                                        VkPipelineLayout layout,
                                                        VkRenderPass render_pass,
                                                        uint32_t subpass) {
  auto vert_module = create_unique_shader_module(device, vert_bytecode);
  auto frag_module = create_unique_shader_module(device, frag_bytecode);
  GROVE_ASSERT(vert_module && frag_module);
  auto stages = make_vert_frag_pipeline_shader_stage_create_info(
    vert_module.value.get(),
    frag_module.value.get());
  auto pipeline_info = make_graphics_pipeline_create_info(
    stages.vert_frag, 2, state_create_info, layout, render_pass, subpass);
  return vk::create_graphics_pipeline(device, &pipeline_info);
}

Result<Pipeline> vk::create_compute_pipeline(VkDevice device,
                                             const std::vector<uint32_t>& bytecode,
                                             VkPipelineLayout layout) {
  auto module = create_unique_shader_module(device, bytecode);
  GROVE_ASSERT(module);

  //  @TODO
  VkPipeline base_pipeline_handle = VK_NULL_HANDLE;
  const int32_t base_pipeline_index = -1;

  VkComputePipelineCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  create_info.stage = make_compute_shader_stage_create_info(module.value.get());
  create_info.layout = layout;
  create_info.basePipelineHandle = base_pipeline_handle;
  create_info.basePipelineIndex = base_pipeline_index;
  return vk::create_compute_pipeline(device, &create_info);
}

VkVertexInputAttributeDescription
vk::to_vk_vertex_input_attribute_description(const grove::AttributeDescriptor& desc,
                                             uint32_t binding,
                                             uint32_t byte_offset) {
  VkVertexInputAttributeDescription result{};
  result.location = desc.location;
  result.binding = binding;
  result.offset = byte_offset;
  result.format = to_vk_format(desc.type, desc.size, IntConversion::None);
  return result;
}

void vk::to_vk_vertex_input_descriptors(uint32_t num_buffers,
                                        const VertexBufferDescriptor* buffer_descriptors,
                                        std::vector<VkVertexInputBindingDescription>* out_bindings,
                                        std::vector<VkVertexInputAttributeDescription>* out_attrs) {
  out_bindings->resize(num_buffers);
  uint32_t num_attrs{};
  for (uint32_t i = 0; i < num_buffers; i++) {
    num_attrs += uint32_t(buffer_descriptors[i].count_attributes());
  }
  out_attrs->resize(num_attrs);
  to_vk_vertex_input_descriptors(
    num_buffers,
    buffer_descriptors,
    out_bindings->data(),
    out_attrs->data());
}

void vk::to_vk_vertex_input_descriptors(uint32_t num_buffers,
                                        const VertexBufferDescriptor* buffer_descriptors,
                                        VertexInputDescriptors* descriptors) {
  uint32_t num_attrs{};
  for (uint32_t i = 0; i < num_buffers; i++) {
    num_attrs += uint32_t(buffer_descriptors[i].count_attributes());
  }
  GROVE_ASSERT(num_buffers < uint32_t(descriptors->bindings.size()));
  GROVE_ASSERT(num_attrs < uint32_t(descriptors->attributes.size()));
  descriptors->num_bindings = num_buffers;
  descriptors->num_attributes = num_attrs;
  to_vk_vertex_input_descriptors(
    num_buffers,
    buffer_descriptors,
    descriptors->bindings.data(),
    descriptors->attributes.data());
}

void vk::to_vk_vertex_input_descriptors(uint32_t num_buffers,
                                         const VertexBufferDescriptor* buffer_descriptors,
                                         VkVertexInputBindingDescription* out_bindings,
                                         VkVertexInputAttributeDescription* out_attrs) {
  uint32_t attr_count{};
  for (uint32_t i = 0; i < num_buffers; i++) {
    const uint32_t binding = i;
    auto& buff_desc = buffer_descriptors[i];
    const auto num_attrs = uint32_t(buff_desc.count_attributes());
    GROVE_ASSERT(num_attrs > 0);
    auto& attrs = buff_desc.get_attributes();

    const uint32_t stride = uint32_t(buff_desc.attribute_stride_bytes());
    VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX;
    uint32_t attr_off{};

    for (uint32_t j = 0; j < num_attrs; j++) {
      auto& attr = attrs[j];
      auto attr_rate = VK_VERTEX_INPUT_RATE_VERTEX;
      if (attr.divisor != -1) {
        GROVE_ASSERT(attr.divisor == 1 && "Instanced attributes with divisor != 1 not supported.");
        attr_rate = VK_VERTEX_INPUT_RATE_INSTANCE;
      }
      if (j == 0) {
        input_rate = attr_rate;
      } else {
        GROVE_ASSERT(attr_rate == input_rate &&
                    "Cannot mix instanced and non-instanced attributes in the same binding.");
      }
      out_attrs[attr_count++] = to_vk_vertex_input_attribute_description(attr, binding, attr_off);
      attr_off += uint32_t(attr.size_bytes());
    }

    GROVE_ASSERT(attr_off == stride);
    out_bindings[i] = make_vertex_input_binding_description(binding, stride, input_rate);
  }
}

GROVE_NAMESPACE_END

