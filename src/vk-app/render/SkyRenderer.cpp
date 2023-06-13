#include "SkyRenderer.hpp"
#include "grove/common/common.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/math/vector.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

struct PushConstantData {
  Mat4f view;
  Mat4f projection;
};

struct Vertex {
  static VertexBufferDescriptor buffer_descriptor() {
    VertexBufferDescriptor descriptor;
    descriptor.add_attribute(AttributeDescriptor::float3(0));
    descriptor.add_attribute(AttributeDescriptor::float2(1));
    return descriptor;
  }
};

PushConstantData make_push_constant_data(const Camera& camera) {
  PushConstantData result{};
  result.view = camera.get_view();
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  result.projection = proj;
  return result;
}

Optional<glsl::VertFragProgramSource> create_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "sky/sky.vert";
  params.frag_file = "sky/sky.frag";
  return glsl::make_vert_frag_program_source(params);
}

Result<Pipeline> create_pipeline(VkDevice device,
                                 const glsl::VertFragProgramSource& source,
                                 const PipelineRenderPassInfo& pass_info,
                                 VkPipelineLayout layout) {
  auto buff_descr = Vertex::buffer_descriptor();
  VertexInputDescriptors input_descrs{};
  to_vk_vertex_input_descriptors(1, &buff_descr, &input_descrs);
  DefaultConfigureGraphicsPipelineStateParams params{input_descrs};
  params.num_color_attachments = 1;
  params.raster_samples = pass_info.raster_samples;
  params.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

  GraphicsPipelineStateCreateInfo state{};
  default_configure(&state, params);
  state.depth_stencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;  //  @NOTE

  return create_vert_frag_graphics_pipeline(
    device, source.vert_bytecode, source.frag_bytecode,
    &state, layout, pass_info.render_pass, pass_info.subpass);
}

} //  anon

bool SkyRenderer::is_valid() const {
  return pipeline_handle.get().is_valid();
}

bool SkyRenderer::initialize(const InitInfo& info) {
  auto source_res = create_program_source();
  if (!source_res) {
    return false;
  }

  const auto& layout_bindings = source_res.value().descriptor_set_layout_bindings;
  VkDevice device_handle = info.core.device.handle;
  if (!info.pipeline_system.require_layouts(
    device_handle,
    make_view(source_res.value().push_constant_ranges),
    make_view(layout_bindings),
    &pipeline_layout,
    &desc_set_layouts)) {
    return false;
  }

  auto pipe_res = create_pipeline(
    device_handle, source_res.value(), info.pass_info, pipeline_layout);
  if (!pipe_res) {
    return false;
  } else {
    pipeline_handle = info.pipeline_system.emplace(std::move(pipe_res.value));
  }

  {
    auto& desc_system = info.desc_system;
    vk::DescriptorPoolAllocator::PoolSizes pool_sizes;
    auto get_size = [](ShaderResourceType) { return 2; };
    push_pool_sizes_from_layout_bindings(pool_sizes, make_view(layout_bindings), get_size);
    desc_pool_allocator = desc_system.create_pool_allocator(make_view(pool_sizes), 4);
    desc_set0_allocator = desc_system.create_set_allocator(desc_pool_allocator.get());
  }

  {
    const auto vertex_count = 64;
    const bool include_uv = true;
    const auto sphere_data = grove::geometry::triangle_strip_sphere_data(vertex_count, include_uv);
    const auto indices = geometry::triangle_strip_indices(vertex_count);
    if (auto vert_buff = create_device_local_vertex_buffer_sync(
      info.allocator,
      sphere_data.size() * sizeof(float),
      sphere_data.data(),
      &info.core,
      &info.uploader)) {
      vertex_buffer = info.buffer_system.emplace(std::move(vert_buff.value));
    } else {
      return false;
    }
    if (auto ind_buff = create_device_local_index_buffer_sync(
      info.allocator,
      indices.size() * sizeof(uint16_t),
      indices.data(),
      &info.core,
      &info.uploader)) {
      index_buffer = info.buffer_system.emplace(std::move(ind_buff.value));
    } else {
      return false;
    }

    draw_desc.num_instances = 1;
    draw_desc.num_indices = uint32_t(indices.size());
  }

  return true;
}

void SkyRenderer::render(const RenderInfo& info) {
  Optional<vk::DynamicSampledImageManager::ReadInstance> color_im;
  if (color_image) {
    if (auto im = info.dynamic_sampled_image_manager.get(color_image.value())) {
      if (im.value().is_2d() && im.value().fragment_shader_sample_ok()) {
        color_im = im.value();
      }
    }
  }
  if (!color_im) {
    return;
  }

  Optional<vk::SampledImageManager::ReadInstance> bayer_im;
  if (bayer_image) {
    if (auto im = info.sampled_image_manager.get(bayer_image.value())) {
      if (im.value().is_2d() && im.value().fragment_shader_sample_ok()) {
        bayer_im = im.value();
      }
    }
  }
  if (!bayer_im) {
    return;
  }

  DescriptorPoolAllocator* pool_alloc{};
  DescriptorSetAllocator* set0_alloc{};
  if (!info.desc_system.get(desc_pool_allocator.get(), &pool_alloc) ||
      !info.desc_system.get(desc_set0_allocator.get(), &set0_alloc)) {
    return;
  }

  VkDescriptorSet desc_set0{};
  {
    VkSampler bayer_sampler = info.sampler_system.require_linear_repeat(info.core.device.handle);
    VkSampler color_sampler = bayer_sampler;

    vk::DescriptorSetScaffold scaffold;
    scaffold.set = 0;
    uint32_t bind{};
    push_combined_image_sampler(  //  color texture
      scaffold, bind++, color_im.value().view, color_sampler, color_im.value().layout);
    push_combined_image_sampler(  //  bayer texture
      scaffold, bind++, bayer_im.value().view, bayer_sampler, bayer_im.value().layout);

    if (auto err = set0_alloc->require_updated_descriptor_set(
      info.core.device.handle,
      *desc_set_layouts.find(0),
      *pool_alloc,
      scaffold,
      &desc_set0)) {
      GROVE_ASSERT(false);
      return;
    }
  }

  cmd::bind_graphics_pipeline(info.cmd, pipeline_handle.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  cmd::bind_graphics_descriptor_sets(info.cmd, pipeline_layout, 0, 1, &desc_set0);

  const auto pc_data = make_push_constant_data(info.camera);
  vkCmdPushConstants(
    info.cmd,
    pipeline_layout,
    VK_SHADER_STAGE_VERTEX_BIT,
    0,
    sizeof(PushConstantData),
    &pc_data);

  auto vb = vertex_buffer.get().contents().buffer.handle;
  VkDeviceSize vb_off{};
  auto ib = index_buffer.get().contents().buffer.handle;
  vkCmdBindIndexBuffer(info.cmd, ib, 0, VK_INDEX_TYPE_UINT16);
  vkCmdBindVertexBuffers(info.cmd, 0, 1, &vb, &vb_off);
  cmd::draw_indexed(info.cmd, &draw_desc);
}

GROVE_NAMESPACE_END
