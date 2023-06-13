#include "WindParticleRenderer.hpp"
#include "grove/common/common.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/visual/Camera.hpp"

#define DISABLE_BLEND (0)

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

using ParticleInstanceData = WindParticles::ParticleInstanceData;

std::array<VertexBufferDescriptor, 2> vertex_buffer_descriptors() {
  std::array<VertexBufferDescriptor, 2> result;
  result[0].add_attribute(AttributeDescriptor::float2(0));  //  position
  result[1].add_attribute(AttributeDescriptor::float3(1, 1)); //  instance translation
  result[1].add_attribute(AttributeDescriptor::float3(2, 1)); //  instance rotation, alpha, scale
  return result;
}

Optional<glsl::VertFragProgramSource> create_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "particle/wind-particles.vert";
  params.frag_file = "particle/wind-particles.frag";
#if DISABLE_BLEND
  glsl::PreprocessorDefinition disable_blend{"DISABLE_BLEND", "", false};
  params.compile.vert_defines.push_back(disable_blend);
  params.compile.frag_defines.push_back(disable_blend);
#endif
  return glsl::make_vert_frag_program_source(params);
}

Result<Pipeline> create_pipeline(VkDevice device,
                                 const glsl::VertFragProgramSource& source,
                                 const vk::PipelineRenderPassInfo& pass_info,
                                 VkPipelineLayout layout) {
  auto buff_descrs = vertex_buffer_descriptors();
  VertexInputDescriptors input_descrs{};
  to_vk_vertex_input_descriptors(uint32_t(buff_descrs.size()), buff_descrs.data(), &input_descrs);
  DefaultConfigureGraphicsPipelineStateParams params{input_descrs};
  params.num_color_attachments = 1;
  params.blend_enabled[0] = true;
  params.raster_samples = pass_info.raster_samples;
  params.cull_mode = VK_CULL_MODE_NONE;
  GraphicsPipelineStateCreateInfo state{};
  default_configure(&state, params);
  state.depth_stencil.depthWriteEnable = VK_FALSE;
  return create_vert_frag_graphics_pipeline(
    device, source.vert_bytecode, source.frag_bytecode,
    &state, layout, pass_info.render_pass, pass_info.subpass);
}

} //  anon

bool WindParticleRenderer::is_valid() const {
  return initialized;
}

bool WindParticleRenderer::initialize(const InitInfo& info) {
  VkDevice device_handle = info.core.device.handle;
  auto prog_source = create_program_source();
  if (!prog_source) {
    return false;
  }

  if (!info.pipeline_system.require_layouts(device_handle, prog_source.value(), &pipeline_data)) {
    return false;
  }

  auto pipe_res = create_pipeline(
    device_handle, prog_source.value(), info.pass_info, pipeline_data.layout);
  if (!pipe_res) {
    return false;
  } else {
    pipeline_data.pipeline = info.pipeline_system.emplace(std::move(pipe_res.value));
  }

  {
    std::vector<float> quad_pos = geometry::quad_positions(false);
    std::vector<uint16_t> quad_inds = geometry::quad_indices();
    auto geom_buff = create_device_local_vertex_buffer_sync(
      info.allocator,
      quad_pos.size() * sizeof(float),
      quad_pos.data(),
      &info.core,
      &info.uploader);
    if (!geom_buff) {
      return false;
    } else {
      geometry_buffer = info.buffer_system.emplace(std::move(geom_buff.value));
    }

    auto ind_buff = create_device_local_index_buffer_sync(
      info.allocator,
      quad_inds.size() * sizeof(uint16_t),
      quad_inds.data(),
      &info.core,
      &info.uploader);
    if (!ind_buff) {
      return false;
    } else {
      index_buffer = info.buffer_system.emplace(std::move(ind_buff.value));
      draw_desc.num_indices = uint32_t(quad_inds.size());
    }
  }

  initialized = true;
  return true;
}

void WindParticleRenderer::begin_frame_set_data(const SetDataContext& context,
                                                const ParticleInstanceData* instance_data,
                                                uint32_t num_instances) {
  if (draw_desc.num_instances != num_instances) {
    instance_buffers.clear();
    for (uint32_t i = 0; i < context.frame_info.frame_queue_depth; i++) {
      const size_t inst_size = num_instances * sizeof(ParticleInstanceData);
      if (auto buff_res = create_host_visible_vertex_buffer(context.allocator, inst_size)) {
        instance_buffers.push_back(context.buffer_system.emplace(std::move(buff_res.value)));
      }
    }
    draw_desc.num_instances = num_instances;
  }
  auto& vb = instance_buffers[context.frame_info.current_frame_index];
  vb.get().write(instance_data, num_instances * sizeof(ParticleInstanceData));
}

void WindParticleRenderer::render(const RenderInfo& info) {
  auto proj = info.camera.get_projection();
  proj[1] = -proj[1];
  auto proj_view = proj * info.camera.get_view();

  auto& pd = pipeline_data;
  cmd::bind_graphics_pipeline(info.cmd, pd.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  vkCmdPushConstants(
    info.cmd, pd.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4f), &proj_view);

  const VkBuffer vert_buffs[2] = {
    geometry_buffer.get().contents().buffer.handle,
    instance_buffers[info.frame_index].get().contents().buffer.handle
  };
  const VkDeviceSize vert_offs[2] = {};
  VkBuffer ind_buff = index_buffer.get().contents().buffer.handle;
  vkCmdBindVertexBuffers(info.cmd, 0, 2, vert_buffs, vert_offs);
  vkCmdBindIndexBuffer(info.cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);
  cmd::draw_indexed(info.cmd, &draw_desc);
}

GROVE_NAMESPACE_END
