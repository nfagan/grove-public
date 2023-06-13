#include "RainParticleRenderer.hpp"
#include "utility.hpp"
#include "memory.hpp"
#include "graphics_context.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace vk;

using DrawableHandle = RainParticleRenderer::DrawableHandle;
using Drawable = RainParticleRenderer::Drawable;
using InstanceVertexBufferIndices = RainParticleRenderer::InstanceVertexBufferIndices;
using InstanceData = RainParticleRenderer::InstanceData;

struct GlobalUniformData {
  Mat4f projection;
  Mat4f view;
  Vec4f particle_scale_alpha_scale;
};

GlobalUniformData make_global_uniform_data(const Camera& camera,
                                           const Vec2f& particle_scale,
                                           float alpha_scale) {
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  GlobalUniformData result;
  result.projection = proj;
  result.view = camera.get_view();
  result.particle_scale_alpha_scale = Vec4f{
    particle_scale.x, particle_scale.y, alpha_scale, 0.0f
  };
  return result;
}

float velocity_to_xy_rotation(const Mat4f& view, const Vec3f& vel) {
  auto vel_cam = view * Vec4f{vel, 0.0f};
  Vec2f vel_cam_xy = Vec2f{-vel_cam.y, vel_cam.x};
  if (vel_cam_xy.length() == 0.0f) {
    return 0.0f;
  } else {
    return float(std::atan2(vel_cam_xy.y, vel_cam_xy.x));
  }
}

std::array<VertexBufferDescriptor, 2> vertex_buffer_descriptors() {
  std::array<VertexBufferDescriptor, 2> buffer_descriptors;
  buffer_descriptors[0].add_attribute(AttributeDescriptor::float2(0));    //  position
  buffer_descriptors[1].add_attribute(AttributeDescriptor::float4(1, 1)); //  translation, alpha
  buffer_descriptors[1].add_attribute(AttributeDescriptor::float4(2, 1)); //  rand01, rot, unused ...
  return buffer_descriptors;
}

VertexBufferDescriptor instance_buffer_dst_descriptor(InstanceVertexBufferIndices* dst_inds) {
  VertexBufferDescriptor result;
  result.add_attribute(AttributeDescriptor::float3(0)); //  translation
  result.add_attribute(AttributeDescriptor::float1(1)); //  alpha
  result.add_attribute(AttributeDescriptor::float1(2)); //  rand01
  result.add_attribute(AttributeDescriptor::float1(3)); //  rot
  result.add_attribute(AttributeDescriptor::float2(4)); //  <unused>
  dst_inds->translation = 0;
  dst_inds->alpha = 1;
  dst_inds->rand01 = 2;
  dst_inds->rotation = 3;
  return result;
}

Optional<glsl::VertFragProgramSource> create_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "particle/rain.vert";
  params.frag_file = "particle/rain.frag";
  params.reflect.to_vk_descriptor_type = vk::refl::always_dynamic_uniform_buffer_descriptor_type;
  return glsl::make_vert_frag_program_source(params);
}

Result<Pipeline> create_pipeline(VkDevice device,
                                 const glsl::VertFragProgramSource& source,
                                 const PipelineRenderPassInfo& pass_info,
                                 VkPipelineLayout layout) {
  auto descs = vertex_buffer_descriptors();
  SimpleVertFragGraphicsPipelineCreateInfo create_info{};
  create_info.pipeline_layout = layout;
  create_info.pipeline_render_pass_info = &pass_info;
  create_info.configure_params = [](DefaultConfigureGraphicsPipelineStateParams& params) {
    params.num_color_attachments = 1;
    params.blend_enabled[0] = true;
  };
  create_info.configure_pipeline_state = [](GraphicsPipelineStateCreateInfo& state) {
    state.depth_stencil.depthWriteEnable = VK_FALSE;
  };
  create_info.vertex_buffer_descriptors = descs.data();
  create_info.num_vertex_buffer_descriptors = uint32_t(descs.size());
  create_info.vert_bytecode = &source.vert_bytecode;
  create_info.frag_bytecode = &source.frag_bytecode;
  return create_vert_frag_graphics_pipeline(device, &create_info);
}

} //  anon

bool RainParticleRenderer::is_valid() const {
  return initialized;
}

bool RainParticleRenderer::initialize(const InitInfo& info) {
  auto prog_source = create_program_source();
  if (!prog_source) {
    return false;
  }
  if (!info.pipeline_system.require_layouts(
    info.core.device.handle,
    make_view(prog_source.value().push_constant_ranges),
    make_view(prog_source.value().descriptor_set_layout_bindings),
    &pipeline_layout,
    &desc_set_layouts)) {
    return false;
  }
  auto pipe_res = create_pipeline(
    info.core.device.handle,
    prog_source.value(),
    info.pass_info,
    pipeline_layout);
  if (!pipe_res) {
    return false;
  } else {
    pipeline = info.pipeline_system.emplace(std::move(pipe_res.value));
  }

  {
    DescriptorPoolAllocator::PoolSizes pool_sizes;
    auto get_size = [](ShaderResourceType) { return 4; };
    push_pool_sizes_from_layout_bindings(
      pool_sizes, make_view(prog_source.value().descriptor_set_layout_bindings), get_size);
    desc_pool_alloc = info.desc_system.create_pool_allocator(make_view(pool_sizes), 4);
    desc_set0_alloc = info.desc_system.create_set_allocator(desc_pool_alloc.get());
  }

  {
    size_t buff_size;
    auto un_buff = create_dynamic_uniform_buffer<GlobalUniformData>(
      info.allocator,
      &info.core.physical_device.info.properties,
      info.frame_queue_depth,
      &global_uniform_buffer_stride,
      &buff_size);
    if (un_buff) {
      global_uniform_buffer = info.buffer_system.emplace(std::move(un_buff.value));
    } else {
      return false;
    }
  }

  {
    auto geom = geometry::quad_positions(false);
    size_t geom_size = sizeof(float) * geom.size();
    auto inds = geometry::quad_indices();
    size_t inds_size = sizeof(uint16_t) * inds.size();
    auto geom_buff = create_device_local_vertex_buffer(info.allocator, geom_size, true);
    auto ind_buff = create_device_local_index_buffer(info.allocator, inds_size, true);
    if (!geom_buff || !ind_buff) {
      return false;
    }
    const ManagedBuffer* dst_buffs[2] = {&geom_buff.value, &ind_buff.value};
    const void* src_datas[2] = {geom.data(), inds.data()};
    auto upload_context = make_upload_from_staging_buffer_context(
      &info.core, info.allocator, &info.staging_buffer_system, &info.command_processor);
    bool success = upload_from_staging_buffer_sync(
      src_datas, dst_buffs, nullptr, 2, upload_context);
    if (!success) {
      return false;
    } else {
      vertex_geometry_buffer = info.buffer_system.emplace(std::move(geom_buff.value));
      vertex_index_buffer = info.buffer_system.emplace(std::move(ind_buff.value));
      num_vertex_indices = uint32_t(inds.size());
    }
  }

  initialized = true;
  return true;
}

void RainParticleRenderer::begin_frame(const BeginFrameInfo& info) {
  {
    auto un_data = make_global_uniform_data(
      info.camera,
      render_params.global_particle_scale,
      render_params.global_alpha_scale);
    auto un_off = info.frame_index * global_uniform_buffer_stride;
    global_uniform_buffer.get().write(&un_data, sizeof(GlobalUniformData), un_off);
  }

  for (auto& [_, drawable] : drawables) {
    if (drawable.instance_buffer_needs_update[info.frame_index]) {
      auto size = sizeof(InstanceData) * drawable.num_instances;
      auto off = size * info.frame_index;
      drawable.instance_buffer.get().write(drawable.cpu_instance_data.get(), size, off);
      drawable.instance_buffer_needs_update[info.frame_index] = false;
    }
  }
}

void RainParticleRenderer::render(const RenderInfo& info) {
  if (drawables.empty()) {
    return;
  }

  DescriptorPoolAllocator* pool_alloc;
  DescriptorSetAllocator* set0_alloc;
  if (!info.desc_system.get(desc_pool_alloc.get(), &pool_alloc) ||
      !info.desc_system.get(desc_set0_alloc.get(), &set0_alloc)) {
    return;
  }

  DescriptorSetScaffold set0_scaffold;
  set0_scaffold.set = 0;
  uint32_t set0_bind{};
  push_dynamic_uniform_buffer(
    set0_scaffold, set0_bind++, global_uniform_buffer.get(), sizeof(GlobalUniformData));

  VkDescriptorSet desc_set0;
  if (auto err = set0_alloc->require_updated_descriptor_set(
    info.device, *desc_set_layouts.find(0), *pool_alloc, set0_scaffold, &desc_set0)) {
    return;
  }

  const uint32_t set0_dyn_offs[1] = {
    uint32_t(info.frame_index * global_uniform_buffer_stride)
  };
  const uint32_t num_set0_dyn_offs = 1;

  cmd::bind_graphics_pipeline(info.cmd, pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  cmd::bind_graphics_descriptor_sets(
    info.cmd, pipeline_layout, 0, 1, &desc_set0, num_set0_dyn_offs, set0_dyn_offs);

  for (auto& [_, drawable] : drawables) {
    const VkBuffer vert_buffs[2] = {
      vertex_geometry_buffer.get().contents().buffer.handle,
      drawable.instance_buffer.get().contents().buffer.handle
    };
    const VkDeviceSize vb_offs[2] = {
      0,
      sizeof(InstanceData) * drawable.num_instances * info.frame_index
    };
    vkCmdBindVertexBuffers(info.cmd, 0, 2, vert_buffs, vb_offs);
    auto ind_buff = vertex_index_buffer.get().contents().buffer.handle;
    vkCmdBindIndexBuffer(info.cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);

    DrawIndexedDescriptor draw_desc{};
    draw_desc.num_indices = num_vertex_indices;
    draw_desc.num_instances = drawable.num_instances;
    cmd::draw_indexed(info.cmd, &draw_desc);
  }
}

void RainParticleRenderer::set_data(DrawableHandle handle,
                                    const void* src,
                                    const VertexBufferDescriptor& src_desc,
                                    const InstanceVertexBufferIndices& src_inds,
                                    uint32_t num_instances) {
  auto drawable_it = drawables.find(handle.id);
  if (drawable_it == drawables.end()) {
    GROVE_ASSERT(false);
    return;
  }
  auto& drawable = drawable_it->second;
  GROVE_ASSERT(drawable.num_instances == num_instances);

  InstanceVertexBufferIndices dst_inds{};
  auto dst_desc = instance_buffer_dst_descriptor(&dst_inds);
  void* dst = drawable.cpu_instance_data.get();

  const int* src_ind_p = &src_inds.translation;
  const int* dst_ind_p = &dst_inds.translation;

  bool copy_success = copy_buffer(
    src, src_desc, src_ind_p, dst, dst_desc, dst_ind_p, 4, num_instances);
  if (!copy_success) {
    GROVE_ASSERT(false);
  }
  for (uint32_t i = 0; i < drawable.frame_queue_depth; i++) {
    drawable.instance_buffer_needs_update[i] = true;
  }
}

void RainParticleRenderer::set_data(DrawableHandle handle,
                                    const Particles& particles,
                                    const Mat4f& view) {
  auto drawable_it = drawables.find(handle.id);
  if (drawable_it == drawables.end()) {
    GROVE_ASSERT(false);
    return;
  }
  auto& drawable = drawable_it->second;
  GROVE_ASSERT(drawable.num_instances == uint32_t(particles.size()));
  fill_scratch_instance_data(particles, view);
  memcpy(
    drawable.cpu_instance_data.get(),
    scratch_instances.data(),
    sizeof(InstanceData) * drawable.num_instances);
  for (uint32_t i = 0; i < drawable.frame_queue_depth; i++) {
    drawable.instance_buffer_needs_update[i] = true;
  }
}

void RainParticleRenderer::fill_scratch_instance_data(const Particles& particles,
                                                      const Mat4f& view) {
  auto num_particles = int(particles.size());
  if (int(scratch_instances.size()) < num_particles) {
    scratch_instances.resize(num_particles);
    scratch_depths.resize(num_particles);
    scratch_depth_inds.resize(num_particles);
  }

  int ct{};
  for (auto& particle : particles) {
    scratch_depths[ct] = (view * Vec4f{particle.position, 1.0f}).z;
    scratch_depth_inds[ct] = ct;
    ct++;
  }

  std::sort(scratch_depth_inds.begin(), scratch_depth_inds.begin() + ct, [&](int a, int b) {
    return scratch_depths[a] > scratch_depths[b];
  });

  for (int i = 0; i < ct; i++) {
    auto& particle = particles[scratch_depth_inds[i]];
    auto theta_xy = velocity_to_xy_rotation(view, particle.velocity);
    InstanceData instance{};
    instance.translation_alpha = Vec4f{particle.position, particle.alpha};
    instance.rand01_rotation = Vec4f{particle.rand01, theta_xy, 0.0f, 0.0f};
    scratch_instances[i] = instance;
  }
}

Optional<DrawableHandle> RainParticleRenderer::create_drawable(const AddResourceContext& context,
                                                               uint32_t num_instances) {
  auto inst_size = sizeof(InstanceData) * num_instances;
  auto gpu_buff_size = inst_size * context.frame_queue_depth;
  auto gpu_buff = create_host_visible_vertex_buffer(context.allocator, gpu_buff_size);
  if (!gpu_buff) {
    return NullOpt{};
  }

  Drawable drawable{};
  drawable.num_instances = num_instances;
  drawable.cpu_instance_data = std::make_unique<unsigned char[]>(inst_size);
  drawable.instance_buffer = context.buffer_system.emplace(std::move(gpu_buff.value));
  drawable.frame_queue_depth = context.frame_queue_depth;

  DrawableHandle handle{next_drawable_id++};
  drawables[handle.id] = std::move(drawable);
  return Optional<DrawableHandle>(handle);
}

RainParticleRenderer::AddResourceContext
RainParticleRenderer::make_add_resource_context(vk::GraphicsContext& graphics_context) {
  return RainParticleRenderer::AddResourceContext{
    graphics_context.core,
    &graphics_context.allocator,
    graphics_context.buffer_system,
    graphics_context.frame_queue_depth
  };
}

GROVE_NAMESPACE_END
