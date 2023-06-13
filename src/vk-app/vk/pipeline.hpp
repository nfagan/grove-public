#pragma once

#include "grove/vk/shader.hpp"
#include <array>

namespace grove {

struct AttributeDescriptor;
class VertexBufferDescriptor;

}

namespace grove::vk {

struct GraphicsPipelineStateCreateInfo {
  VkPipelineVertexInputStateCreateInfo vertex_input;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkPipelineViewportStateCreateInfo viewport;
  VkPipelineRasterizationStateCreateInfo rasterization;
  VkPipelineMultisampleStateCreateInfo multisampling;
  VkPipelineColorBlendAttachmentState color_blend_attachments[16];
  VkPipelineColorBlendStateCreateInfo color_blend;
  VkPipelineDepthStencilStateCreateInfo depth_stencil;
  VkPipelineDynamicStateCreateInfo dynamic_state;
};

struct PipelineRenderPassInfo {
  VkRenderPass render_pass;
  uint32_t subpass;
  VkSampleCountFlagBits raster_samples;
};

struct VertexInputDescriptors {
  std::array<VkVertexInputBindingDescription, 8> bindings;
  uint32_t num_bindings;
  std::array<VkVertexInputAttributeDescription, 64> attributes;
  uint32_t num_attributes;
};

struct DefaultConfigureGraphicsPipelineStateParams {
  DefaultConfigureGraphicsPipelineStateParams() = default;
  explicit DefaultConfigureGraphicsPipelineStateParams(const VertexInputDescriptors& descrs) :
  DefaultConfigureGraphicsPipelineStateParams{
    descrs.bindings.data(),
    descrs.num_bindings,
    descrs.attributes.data(),
    descrs.num_attributes
  } {}
  DefaultConfigureGraphicsPipelineStateParams(const VkVertexInputBindingDescription* bindings,
                                              uint32_t num_bindings,
                                              const VkVertexInputAttributeDescription* attrs,
                                              uint32_t num_attrs) :
                                              bindings{bindings},
                                              num_bindings{num_bindings},
                                              attributes{attrs},
                                              num_attributes{num_attrs} {}
  const VkVertexInputBindingDescription* bindings{};
  uint32_t num_bindings{};
  const VkVertexInputAttributeDescription* attributes{};
  uint32_t num_attributes{};
  VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  VkCullModeFlags cull_mode{VK_CULL_MODE_BACK_BIT};
  VkFrontFace front_face{VK_FRONT_FACE_COUNTER_CLOCKWISE};
  VkSampleCountFlagBits raster_samples{VK_SAMPLE_COUNT_1_BIT};
  uint32_t num_color_attachments{};
  bool blend_enabled[16]{};
};

struct VertFragPipelineShaderStageCreateInfo {
  VkPipelineShaderStageCreateInfo vert_frag[2];
};

VkGraphicsPipelineCreateInfo
make_graphics_pipeline_create_info(const VkPipelineShaderStageCreateInfo* shader_stages,
                                   uint32_t num_stages,
                                   const GraphicsPipelineStateCreateInfo* state,
                                   VkPipelineLayout layout,
                                   VkRenderPass render_pass,
                                   uint32_t subpass,
                                   VkPipeline base_pipeline_handle = VK_NULL_HANDLE,
                                   int32_t base_pipeline_index = -1);

VertFragPipelineShaderStageCreateInfo
make_vert_frag_pipeline_shader_stage_create_info(VkShaderModule vert, VkShaderModule frag);

void default_vertex_input(VkPipelineVertexInputStateCreateInfo* state,
                          const VkVertexInputBindingDescription* bindings,
                          uint32_t num_bindings,
                          const VkVertexInputAttributeDescription* attributes,
                          uint32_t num_attrs);
void default_input_assembly(VkPipelineInputAssemblyStateCreateInfo* state,
                            VkPrimitiveTopology topology,
                            bool prim_restart_enabled = false);
void default_dynamic_viewport(VkPipelineViewportStateCreateInfo* state);
void default_rasterization(VkPipelineRasterizationStateCreateInfo* state,
                           VkCullModeFlags cull_mode,
                           VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE);
void default_multisampling(VkPipelineMultisampleStateCreateInfo* state,
                           VkSampleCountFlagBits samples);
void default_depth_stencil(VkPipelineDepthStencilStateCreateInfo* state);
void default_color_blend(VkPipelineColorBlendStateCreateInfo* state,
                         const VkPipelineColorBlendAttachmentState* attachments,
                         uint32_t num_attachments);
void attachment_alpha_blend_disabled(VkPipelineColorBlendAttachmentState* state);
void attachment_alpha_blend_enabled(VkPipelineColorBlendAttachmentState* state);
void default_dynamic_state(VkPipelineDynamicStateCreateInfo* state);

void default_configure(GraphicsPipelineStateCreateInfo* state,
                       const DefaultConfigureGraphicsPipelineStateParams& params);

VkPipelineDepthStencilStateCreateInfo
make_default_pipeline_depth_stencil_state_create_info(VkCompareOp compare_op = VK_COMPARE_OP_GREATER,
                                                      VkBool32 enable_depth_test = VK_TRUE,
                                                      VkBool32 depth_write_enable = VK_TRUE,
                                                      float min_depth_bounds = 0.0f,
                                                      float max_depth_bounds = 1.0f,
                                                      VkBool32 enable_stencil_test = VK_FALSE);

VkPipelineMultisampleStateCreateInfo
make_default_pipeline_multisample_state_create_info(VkSampleCountFlagBits num_samples);

VkPipelineRasterizationStateCreateInfo
make_default_pipeline_rasterization_state_create_info(VkCullModeFlags cull_mode,
                                                      VkFrontFace front_face);

VkPipelineViewportStateCreateInfo make_dynamic_viewport_scissor_rect_pipeline_viewport_state_create_info();
VkPipelineDynamicStateCreateInfo make_dynamic_viewport_scissor_rect_pipeline_dynamic_state_create_info();
VkPipelineColorBlendStateCreateInfo
make_default_pipeline_color_blend_state_create_info(const VkPipelineColorBlendAttachmentState* attachments,
                                                    uint32_t num_attachments);

VkPipelineInputAssemblyStateCreateInfo
make_input_assembly_state_create_info(VkPrimitiveTopology topology, bool prim_restart_enabled);

VkPipelineVertexInputStateCreateInfo
make_vertex_input_state_create_info(const VkVertexInputBindingDescription* binding_descriptions,
                                    uint32_t num_bindings,
                                    const VkVertexInputAttributeDescription* attr_descriptions,
                                    uint32_t num_attrs);

VkVertexInputBindingDescription make_vertex_input_binding_description(uint32_t binding,
                                                                      uint32_t stride,
                                                                      VkVertexInputRate input_rate);
VkVertexInputAttributeDescription make_vertex_input_attribute_description(uint32_t binding,
                                                                          uint32_t location,
                                                                          VkFormat format,
                                                                          uint32_t offset);

inline VkVertexInputBindingDescription
make_rate_vertex_vertex_input_binding_description(uint32_t binding, uint32_t stride) {
  return make_vertex_input_binding_description(binding, stride, VK_VERTEX_INPUT_RATE_VERTEX);
}

inline VkVertexInputBindingDescription
make_rate_instance_vertex_input_binding_description(uint32_t binding, uint32_t stride) {
  return make_vertex_input_binding_description(binding, stride, VK_VERTEX_INPUT_RATE_INSTANCE);
}

VkPipelineViewportStateCreateInfo make_viewport_state_create_info(const VkViewport* viewports,
                                                                  uint32_t num_viewports,
                                                                  const VkRect2D* scissors,
                                                                  uint32_t num_scissors);

VkPipelineRasterizationStateCreateInfo make_empty_rasterization_state_create_info();
VkPipelineColorBlendAttachmentState make_alpha_blend_enabled_color_blend_attachment_state();
VkPipelineColorBlendAttachmentState make_alpha_blend_disabled_color_blend_attachment_state();

VkPipelineDynamicStateCreateInfo
make_pipeline_dynamic_state_create_info(const VkDynamicState* states, uint32_t num_states);

Result<Pipeline> create_vert_frag_graphics_pipeline(VkDevice device,
                                                    const std::vector<uint32_t>& vert_bytecode,
                                                    const std::vector<uint32_t>& frag_bytecode,
                                                    const GraphicsPipelineStateCreateInfo* state_create_info,
                                                    VkPipelineLayout layout,
                                                    VkRenderPass render_pass,
                                                    uint32_t subpass);

Result<Pipeline> create_compute_pipeline(VkDevice device, const std::vector<uint32_t>& bytecode,
                                         VkPipelineLayout layout);

VkVertexInputAttributeDescription
to_vk_vertex_input_attribute_description(const grove::AttributeDescriptor& desc,
                                         uint32_t binding, uint32_t off);

void to_vk_vertex_input_descriptors(uint32_t num_buffers,
                                    const VertexBufferDescriptor* buffer_descriptors,
                                    VkVertexInputBindingDescription* out_bindings,
                                    VkVertexInputAttributeDescription* out_attrs);

void to_vk_vertex_input_descriptors(uint32_t num_buffers,
                                    const VertexBufferDescriptor* buffer_descriptors,
                                    std::vector<VkVertexInputBindingDescription>* out_bindings,
                                    std::vector<VkVertexInputAttributeDescription>* out_attrs);

void to_vk_vertex_input_descriptors(uint32_t num_buffers,
                                    const VertexBufferDescriptor* buffer_descriptors,
                                    VertexInputDescriptors* descriptors);

inline VkViewport make_full_viewport(float width, float height,
                                     float min_depth, float max_depth) {
  VkViewport result{};
  result.width = width;
  result.height = height;
  result.minDepth = min_depth;
  result.maxDepth = max_depth;
  return result;
}

inline VkViewport make_full_viewport(VkExtent2D extent, float min_depth = 0.0f,
                                     float max_depth = 1.0f) {
  return make_full_viewport(float(extent.width), float(extent.height), min_depth, max_depth);
}

inline VkRect2D make_full_scissor_rect(VkExtent2D extent) {
  VkRect2D result{};
  result.extent = extent;
  return result;
}

}