#include "PollenParticleRenderer.hpp"
#include "debug_label.hpp"
#include "grove/common/common.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/visual/Camera.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

struct InstanceData {
  Vec4f translation_scale;
};

struct PushConstantData {
  Mat4f projection_view;
};

PushConstantData make_push_constant_data(const Camera& camera) {
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  PushConstantData result;
  result.projection_view = proj * camera.get_view();
//  result.projection = proj;
//  result.view = camera.get_view();
  return result;
}

std::array<VertexBufferDescriptor, 2> vertex_buffer_descriptors() {
  std::array<VertexBufferDescriptor, 2> result;
  result[0].add_attribute(AttributeDescriptor::float3(0));    //  position
  result[1].add_attribute(AttributeDescriptor::float4(1, 1)); //  translation_scale
  return result;
}

Optional<glsl::VertFragProgramSource> create_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "particle/pollen-particles.vert";
  params.frag_file = "particle/pollen-particles.frag";
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
  GraphicsPipelineStateCreateInfo state{};
  default_configure(&state, params);
  return create_vert_frag_graphics_pipeline(
    device, source.vert_bytecode, source.frag_bytecode,
    &state, layout, pass_info.render_pass, pass_info.subpass);
}

} //  anon

bool PollenParticleRenderer::is_valid() const {
  return initialized;
}

bool PollenParticleRenderer::initialize(const InitInfo& info) {
  auto prog_source = create_program_source();
  if (!prog_source) {
    return false;
  }

  auto& layout_bindings = prog_source.value().descriptor_set_layout_bindings;
  if (!info.pipeline_system.require_layouts(
    info.core.device.handle,
    make_view(prog_source.value().push_constant_ranges),
    make_view(layout_bindings),
    &pipeline_layout,
    &desc_set_layouts)) {
    return false;
  }

  auto pipe_res = create_pipeline(
    info.core.device.handle, prog_source.value(), info.forward_pass_info, pipeline_layout);
  if (!pipe_res) {
    return false;
  } else {
    pipeline = info.pipeline_system.emplace(std::move(pipe_res.value));
  }

  {
    std::vector<float> geom = geometry::cube_positions();
    std::vector<uint16_t> inds = geometry::cube_indices();
    const size_t geom_size = geom.size() * sizeof(float);
    const size_t ind_size = inds.size() * sizeof(uint16_t);
    auto geom_res = create_device_local_vertex_buffer(info.allocator, geom_size, true);
    auto ind_res = create_device_local_index_buffer(info.allocator, ind_size, true);
    if (!geom_res || !ind_res) {
      return false;
    }

    auto upload_context = make_upload_from_staging_buffer_context(
      &info.core, info.allocator, &info.staging_buffer_system, &info.command_processor);

    const ManagedBuffer* dst_buffs[2] = {&geom_res.value, &ind_res.value};
    const void* src_data[2] = {geom.data(), inds.data()};
    if (!upload_from_staging_buffer_sync(
      src_data, dst_buffs, nullptr, 2, upload_context)) {
      return false;
    }

    geometry_buffer = info.buffer_system.emplace(std::move(geom_res.value));
    index_buffer = info.buffer_system.emplace(std::move(ind_res.value));
    draw_desc.num_indices = uint32_t(inds.size());
  }

  initialized = true;
  return true;
}

void PollenParticleRenderer::begin_update() {
  num_active_drawables = 0;
}

void PollenParticleRenderer::push_drawable(const DrawableParams& params) {
  if (!initialized) {
    return;
  }
  if (num_active_drawables == num_reserved_drawables) {
    const int init_reserve = 4;
    auto num_reserve = num_reserved_drawables == 0 ? init_reserve : num_reserved_drawables * 2;
    auto new_size = sizeof(InstanceData) * num_reserve;
    auto old_size = sizeof(InstanceData) * num_reserved_drawables;
    auto new_cpu_dat = std::make_unique<unsigned char[]>(new_size);
    if (num_reserved_drawables > 0) {
      memcpy(new_cpu_dat.get(), cpu_instance_data.get(), old_size);
    }
    num_reserved_drawables = num_reserve;
    cpu_instance_data = std::move(new_cpu_dat);
    need_remake_instance_buffer = true;
  }
  size_t inst_off = num_active_drawables * sizeof(InstanceData);
  InstanceData data{};
  data.translation_scale = Vec4f{params.translation, params.scale};
  memcpy(cpu_instance_data.get() + inst_off, &data, sizeof(InstanceData));
  num_active_drawables++;
}

void PollenParticleRenderer::begin_frame(const BeginFrameInfo& info) {
  if (need_remake_instance_buffer) {
    const size_t reserved_inst_size = sizeof(InstanceData) * num_reserved_drawables;
    size_t buff_size = reserved_inst_size * info.frame_info.frame_queue_depth;
    auto buff_res = create_host_visible_vertex_buffer(info.allocator, buff_size);
    if (!buff_res) {
      return;
    } else {
      instance_buffer = info.buffer_system.emplace(std::move(buff_res.value));
    }
    need_remake_instance_buffer = false;
  }
}

void PollenParticleRenderer::render(const RenderInfo& info) {
  if (num_active_drawables == 0 || need_remake_instance_buffer) {
    return;
  }

  auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "PollenParticles");
  (void) profiler;

  const size_t reserved_inst_size = sizeof(InstanceData) * num_reserved_drawables;
  const auto pc_data = make_push_constant_data(info.camera);

  size_t inst_frame_off = reserved_inst_size * info.frame_index;
  size_t active_size = sizeof(InstanceData) * num_active_drawables;
  instance_buffer.get().write(cpu_instance_data.get(), active_size, inst_frame_off);

  cmd::bind_graphics_pipeline(info.cmd, pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  vkCmdPushConstants(
    info.cmd, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantData), &pc_data);

  VkBuffer ind_buff = index_buffer.get().contents().buffer.handle;
  const VkBuffer vert_buffs[2] = {
    geometry_buffer.get().contents().buffer.handle,
    instance_buffer.get().contents().buffer.handle
  };
  const VkDeviceSize vb_offs[2] = {0, inst_frame_off};

  vkCmdBindVertexBuffers(info.cmd, 0, 2, vert_buffs, vb_offs);
  vkCmdBindIndexBuffer(info.cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);

  draw_desc.num_instances = uint32_t(num_active_drawables);
  cmd::draw_indexed(info.cmd, &draw_desc);
}

GROVE_NAMESPACE_END
