#include "ProceduralFlowerStemRenderer.hpp"
#include "./graphics_context.hpp"
#include "../procedural_tree/render.hpp"
#include "../procedural_tree/utility.hpp"
#include "csm.hpp"
#include "grove/common/common.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/math/Bounds3.hpp"

#define ALLOW_NON_FINITE_BOUNDS (1)

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

using DrawableHandle = ProceduralFlowerStemRenderer::DrawableHandle;
using Drawable = ProceduralFlowerStemRenderer::Drawable;
using DrawableParams = ProceduralFlowerStemRenderer::DrawableParams;
using AddResourceContext = ProceduralFlowerStemRenderer::AddResourceContext;

struct Config {
  static constexpr float leaf_tip_radius = 0.005f;
};

struct DynamicInstanceData {
  Vec3f position;
  Vec3f child_position;
  Vec2f radii;
};

struct StaticInstanceData {
  Vec4f instance_directions{};
  Vec3f aabb_p0{};
  float y_fraction{};
  Vec3f aabb_p1{};
  float child_y_fraction{};
};

struct GlobalUniformData {
  Mat4f view;
  Mat4f projection;
  Mat4f sun_light_view_projection0;
  Vec4f num_points_xz_t; //  x, z, t, unused
  Vec4f wind_world_bound_xz;
  Vec4f camera_position;
  Vec4f sun_color;
};

struct PushConstantData {
  Vec4f color_wind_influence_enabled;
};

float node_y_fraction(const tree::Internode& node, const Bounds3f& node_aabb) {
  float frac = node_aabb.to_fraction(node.position).y;
#if ALLOW_NON_FINITE_BOUNDS == 0
  assert(frac >= 0.0f && frac <= 1.0f);
#endif
  return frac;
}

void make_dynamic_data(const tree::Internodes& inodes, std::unique_ptr<unsigned char[]>& out,
                       bool allow_lateral) {
  const auto tip_radius = Config::leaf_tip_radius;

  int inode_ind{};
  for (auto& node : inodes) {
    DynamicInstanceData data{};
    auto child_data = tree::get_child_render_data(node, inodes.data(), allow_lateral, tip_radius);

    data.position = node.render_position;
    data.child_position = child_data.position;
    data.radii = Vec2f{node.radius(), child_data.radius};

    const size_t off = sizeof(DynamicInstanceData) * inode_ind;
    memcpy(out.get() + off, &data, sizeof(DynamicInstanceData));
    inode_ind++;
  }
}

std::vector<StaticInstanceData> make_static_data(const tree::Internodes& inodes,
                                                 bool allow_lateral) {
  std::vector<StaticInstanceData> result;
  result.reserve(inodes.size());

  const auto tip_radius = Config::leaf_tip_radius;
  const auto stem_aabb = tree::internode_aabb(inodes);
  for (auto& node : inodes) {
    StaticInstanceData data{};
    auto child_data = tree::get_child_render_data(node, inodes.data(), allow_lateral, tip_radius);

    auto self_dir = node.spherical_direction();
    auto child_dir = child_data.direction;

    data.instance_directions.x = self_dir.x;
    data.instance_directions.y = self_dir.y;
    data.instance_directions.z = child_dir.x;
    data.instance_directions.w = child_dir.y;

    data.aabb_p0 = stem_aabb.min;
    data.y_fraction = node_y_fraction(node, stem_aabb);
    data.aabb_p1 = stem_aabb.max;
    data.child_y_fraction = node_y_fraction(*child_data.child, stem_aabb);

    result.push_back(data);
  }

  return result;
}

GlobalUniformData make_global_uniform_data(const Camera& camera,
                                           const GridGeometryParams& geom_params,
                                           const Vec4f& wind_world_bound_xz,
                                           float t,
                                           const csm::CSMDescriptor& csm_desc,
                                           const Vec3f& sun_color) {
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  GlobalUniformData data;
  data.view = camera.get_view();
  data.projection = proj;
  data.sun_light_view_projection0 = csm_desc.light_shadow_sample_view;
  data.num_points_xz_t = Vec4f{
    float(geom_params.num_pts_x), float(geom_params.num_pts_z), t, 0.0f
  };
  data.wind_world_bound_xz = wind_world_bound_xz;
  data.camera_position = Vec4f{camera.get_position(), 0.0f};
  data.sun_color = Vec4f{sun_color, 0.0f};
  return data;
}

PushConstantData make_push_constant_data(const DrawableParams& params) {
  PushConstantData result;
  result.color_wind_influence_enabled = Vec4f{
    params.color, float(params.wind_influence_enabled)};
  return result;
}

GridGeometryParams stem_geometry_params() {
  GridGeometryParams params{};
  params.num_pts_x = 7;
  params.num_pts_z = 2;
  return params;
}

std::array<VertexBufferDescriptor, 3> vertex_buffer_descriptors() {
  std::array<VertexBufferDescriptor, 3> result;
  int off{};
  //  geom
  result[0].add_attribute(AttributeDescriptor::float2(off++));      //  vertices
  //  static
  result[1].add_attribute(AttributeDescriptor::float4(off++, 1));   //  directions
  result[1].add_attribute(AttributeDescriptor::float4(off++, 1));   //  aabb0
  result[1].add_attribute(AttributeDescriptor::float4(off++, 1));   //  aabb1
  //  dynamic
  result[2].add_attribute(AttributeDescriptor::float3(off++, 1));   //  instance pos
  result[2].add_attribute(AttributeDescriptor::float3(off++, 1));   //  child instance pos
  result[2].add_attribute(AttributeDescriptor::float2(off++, 1));   //  instance radii
  return result;
}

Optional<glsl::VertFragProgramSource> create_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "proc-flower/stem.vert";
  params.frag_file = "proc-flower/stem.frag";
  params.compile.frag_defines = csm::make_default_sample_shadow_preprocessor_definitions();
  params.reflect.to_vk_descriptor_type = [](const auto& info) {
    if (info.is_uniform_buffer()) {
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    } else {
      return vk::refl::identity_descriptor_type(info);
    }
  };
  return glsl::make_vert_frag_program_source(params);
}

Result<Pipeline> create_pipeline(VkDevice device,
                                 const glsl::VertFragProgramSource& source,
                                 const PipelineRenderPassInfo& pass_info,
                                 VkPipelineLayout layout) {
  auto buff_descs = vertex_buffer_descriptors();
  VertexInputDescriptors input_descrs{};
  to_vk_vertex_input_descriptors(uint32_t(buff_descs.size()), buff_descs.data(), &input_descrs);
  DefaultConfigureGraphicsPipelineStateParams params{input_descrs};
  params.num_color_attachments = 1;
  params.raster_samples = pass_info.raster_samples;
  GraphicsPipelineStateCreateInfo state{};
  default_configure(&state, params);
  return create_vert_frag_graphics_pipeline(
    device, source.vert_bytecode, source.frag_bytecode,
    &state, layout, pass_info.render_pass, pass_info.subpass);
}

Optional<Drawable> create_drawable(const AddResourceContext& context,
                                   const tree::Internodes& internodes,
                                   const DrawableParams& params) {
  const auto num_internodes = uint32_t(internodes.size());
  auto static_inst_data = make_static_data(internodes, params.allow_lateral_branch);

  const size_t cpu_dyn_data_size = num_internodes * sizeof(DynamicInstanceData);
  auto store_dyn_data = std::make_unique<unsigned char[]>(cpu_dyn_data_size);
  make_dynamic_data(internodes, store_dyn_data, params.allow_lateral_branch);

  const size_t static_size = num_internodes * sizeof(StaticInstanceData);
  const size_t tot_dynamic_size = cpu_dyn_data_size * context.frame_queue_depth;

  auto static_inst_buff = create_host_visible_vertex_buffer(context.allocator, static_size);
  auto dyn_inst_buff = create_host_visible_vertex_buffer(context.allocator, tot_dynamic_size);
  if (!dyn_inst_buff || !static_inst_buff) {
    return NullOpt{};
  }

  static_inst_buff.value.write(static_inst_data.data(), static_size);
  for (uint32_t i = 0; i < context.frame_queue_depth; i++) {
    dyn_inst_buff.value.write(store_dyn_data.get(), cpu_dyn_data_size, cpu_dyn_data_size * i);
  }

  Drawable drawable{};
  drawable.num_instances = num_internodes;
  drawable.frame_queue_depth = context.frame_queue_depth;
  drawable.dynamic_instance_buffer = context.buffer_system.emplace(
    std::move(dyn_inst_buff.value));
  drawable.static_instance_buffer = context.buffer_system.emplace(
    std::move(static_inst_buff.value));
  drawable.cpu_dynamic_instance_data = std::move(store_dyn_data);
  drawable.params = params;
  return Optional<Drawable>(std::move(drawable));
}

} //  anon

bool ProceduralFlowerStemRenderer::is_valid() const {
  return initialized && initialized_program;
}

bool ProceduralFlowerStemRenderer::initialize(const InitInfo& info) {
  glsl::VertFragProgramSource prog_source;
  if (!make_pipeline(info.core, info.pipeline_system, info.forward_pass_info, &prog_source)) {
    return false;
  }

  {
    vk::DescriptorPoolAllocator::PoolSizes pool_sizes;
    auto get_size = [](ShaderResourceType) { return 32; };
    push_pool_sizes_from_layout_bindings(
      pool_sizes, make_view(prog_source.descriptor_set_layout_bindings), get_size);
    desc_pool_alloc = info.desc_system.create_pool_allocator(make_view(pool_sizes), 32);
    desc_set0_alloc = info.desc_system.create_set_allocator(desc_pool_alloc.get());
  }

  {
    size_t glob_size{};
    auto un_buff = create_dynamic_uniform_buffer<GlobalUniformData>(
      info.allocator,
      &info.core.physical_device.info.properties,
      info.frame_queue_depth,
      &global_uniform_buffer.stride,
      &glob_size);
    if (un_buff) {
      global_uniform_buffer.handle = info.buffer_system.emplace(std::move(un_buff.value));
    } else {
      return false;
    }
  }
  {
    size_t shadow_size{};
    auto un_buff = create_dynamic_uniform_buffer<csm::SunCSMSampleData>(
      info.allocator,
      &info.core.physical_device.info.properties,
      info.frame_queue_depth,
      &sample_shadow_uniform_buffer.stride,
      &shadow_size);
    if (un_buff) {
      sample_shadow_uniform_buffer.handle = info.buffer_system.emplace(std::move(un_buff.value));
    } else {
      return false;
    }
  }

  {
    geom_params = stem_geometry_params();
    std::vector<float> geom = make_reflected_grid_indices(geom_params);
    std::vector<uint16_t> inds = triangulate_reflected_grid(geom_params);
    size_t geom_size = geom.size() * sizeof(float);
    size_t inds_size = inds.size() * sizeof(uint16_t);
    auto geom_buff = create_device_local_vertex_buffer(info.allocator, geom_size, true);
    auto ind_buff = create_device_local_index_buffer(info.allocator, inds_size, true);
    if (!geom_buff || !ind_buff) {
      return false;
    }

    const ManagedBuffer* dst_buffs[2] = {&geom_buff.value, &ind_buff.value};
    const void* src_datas[2] = {geom.data(), inds.data()};
    auto upload_context = make_upload_from_staging_buffer_context(
      &info.core,
      info.allocator,
      &info.staging_buffer_system,
      &info.uploader);
    bool success = upload_from_staging_buffer_sync(
      src_datas, dst_buffs, nullptr, 2, upload_context);
    if (!success) {
      return false;
    } else {
      geom_buffer = info.buffer_system.emplace(std::move(geom_buff.value));
      index_buffer = info.buffer_system.emplace(std::move(ind_buff.value));
      num_geom_indices = uint32_t(inds.size());
    }
  }

  initialized = true;
  return true;
}

bool ProceduralFlowerStemRenderer::make_pipeline(const vk::Core& core,
                                                 vk::PipelineSystem& pipe_sys,
                                                 const vk::PipelineRenderPassInfo& forward_pass_info,
                                                 glsl::VertFragProgramSource* source) {
  initialized_program = false;
  auto prog_source = create_program_source();
  if (!prog_source) {
    return false;
  }

  auto& layout_bindings = prog_source.value().descriptor_set_layout_bindings;
  if (!pipe_sys.require_layouts(
    core.device.handle,
    make_view(prog_source.value().push_constant_ranges),
    make_view(layout_bindings),
    &pipeline_layout,
    &desc_set_layouts)) {
    return false;
  }

  auto pipeline_res = create_pipeline(
    core.device.handle,
    prog_source.value(),
    forward_pass_info,
    pipeline_layout);
  if (!pipeline_res) {
    return false;
  } else {
    pipeline = pipe_sys.emplace(std::move(pipeline_res.value));
  }

  *source = std::move(prog_source.value());
  initialized_program = true;
  return true;
}

void ProceduralFlowerStemRenderer::update_buffers(const BeginFrameInfo& info) {
  { //  uniform
    const auto global_dyn_off = uint32_t(global_uniform_buffer.stride * info.frame_index);
    const auto global_uniform_data = make_global_uniform_data(
      info.camera,
      geom_params,
      render_params.wind_world_bound_xz,
      render_params.elapsed_time,
      info.csm_desc,
      render_params.sun_color);
    global_uniform_buffer.handle.get().write(
      &global_uniform_data, sizeof(GlobalUniformData), global_dyn_off);
  }

  { //  shadow
    const auto off = uint32_t(sample_shadow_uniform_buffer.stride * info.frame_index);
    const auto data = csm::make_sun_csm_sample_data(info.csm_desc);
    sample_shadow_uniform_buffer.handle.get().write(&data, sizeof(data), off);
  }

  for (auto& [_, drawable] : drawables) {
    if (drawable.dynamic_instance_buffer_needs_update[info.frame_index]) {
      size_t inst_size = sizeof(DynamicInstanceData) * drawable.num_instances;
      size_t inst_off = inst_size * info.frame_index;
      drawable.dynamic_instance_buffer.get().write(
        drawable.cpu_dynamic_instance_data.get(), inst_size, inst_off);
      drawable.dynamic_instance_buffer_needs_update[info.frame_index] = false;
    }
  }
}

void ProceduralFlowerStemRenderer::begin_frame(const BeginFrameInfo& info) {
  update_buffers(info);
}

void ProceduralFlowerStemRenderer::render(const RenderInfo& info) {
  if (drawables.empty()) {
    return;
  }

  Optional<vk::DynamicSampledImageManager::ReadInstance> wind_im;
  if (wind_displacement_image) {
    if (auto im = info.dynamic_sampled_image_manager.get(wind_displacement_image.value())) {
      if (im.value().vertex_shader_sample_ok() && im.value().is_2d()) {
        wind_im = im.value();
      }
    }
  }
  if (!wind_im) {
    return;
  }

  vk::DescriptorPoolAllocator* pool_alloc;
  vk::DescriptorSetAllocator* set0_alloc;
  if (!info.desc_system.get(desc_pool_alloc.get(), &pool_alloc) ||
      !info.desc_system.get(desc_set0_alloc.get(), &set0_alloc)) {
    return;
  }

  VkDescriptorSet desc_set0;
  {
    auto wind_sampler = info.sampler_system.require_linear_edge_clamp(info.device);
    auto shadow_sampler = wind_sampler;

    vk::DescriptorSetScaffold scaffold;
    scaffold.set = 0;
    uint32_t bind{};
    push_dynamic_uniform_buffer(  //  global uniform buffer
      scaffold, bind++, global_uniform_buffer.handle.get(), sizeof(GlobalUniformData));
    push_combined_image_sampler(  //  wind image
      scaffold, bind++, wind_im.value().view, wind_sampler, wind_im.value().layout);
    push_dynamic_uniform_buffer(  //  shadow uniform buffer
      scaffold, bind++, sample_shadow_uniform_buffer.handle.get(), sizeof(csm::SunCSMSampleData));
    push_combined_image_sampler(  //  shadow image
      scaffold, bind++, info.shadow_image, shadow_sampler);

    if (auto err = set0_alloc->require_updated_descriptor_set(
      info.device, *desc_set_layouts.find(0), *pool_alloc, scaffold, &desc_set0)) {
      return;
    }
  }

  const uint32_t dyn_offsets[2] = {
    uint32_t(global_uniform_buffer.stride * info.frame_index),
    uint32_t(sample_shadow_uniform_buffer.stride * info.frame_index)
  };

  cmd::bind_graphics_pipeline(info.cmd, pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  cmd::bind_graphics_descriptor_sets(info.cmd, pipeline_layout, 0, 1, &desc_set0, 2, dyn_offsets);

  for (auto& [_, drawable] : drawables) {
    if (drawable.inactive) {
      continue;
    }
    const VkBuffer vert_buffs[3] = {
      geom_buffer.get().contents().buffer.handle,
      drawable.static_instance_buffer.get().contents().buffer.handle,
      drawable.dynamic_instance_buffer.get().contents().buffer.handle
    };
    const VkDeviceSize vb_offs[3] = {
      0,
      0,
      sizeof(DynamicInstanceData) * drawable.num_instances * info.frame_index
    };

    const auto pc_data = make_push_constant_data(drawable.params);
    DrawIndexedDescriptor draw_desc{};
    draw_desc.num_instances = drawable.num_instances;
    draw_desc.num_indices = num_geom_indices;

    VkBuffer index_buff = index_buffer.get().contents().buffer.handle;
    vkCmdBindIndexBuffer(info.cmd, index_buff, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindVertexBuffers(info.cmd, 0, 3, vert_buffs, vb_offs);
    vkCmdPushConstants(
      info.cmd,
      pipeline_layout,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      0,
      sizeof(PushConstantData),
      &pc_data);
    cmd::draw_indexed(info.cmd, &draw_desc);
  }
}

void ProceduralFlowerStemRenderer::set_dynamic_data(DrawableHandle handle,
                                                    const tree::Internodes& internodes) {
  if (auto it = drawables.find(handle.id); it != drawables.end()) {
    auto& drawable = it->second;
    GROVE_ASSERT(drawable.num_instances == uint32_t(internodes.size()));
    make_dynamic_data(
      internodes,
      drawable.cpu_dynamic_instance_data,
      drawable.params.allow_lateral_branch);
    for (uint32_t i = 0; i < drawable.frame_queue_depth; i++) {
      drawable.dynamic_instance_buffer_needs_update[i] = true;
    }
  } else {
    GROVE_ASSERT(false);
  }
}

AddResourceContext
ProceduralFlowerStemRenderer::make_add_resource_context(vk::GraphicsContext& graphics_context) {
  return ProceduralFlowerStemRenderer::AddResourceContext{
    graphics_context.core,
    &graphics_context.allocator,
    graphics_context.buffer_system,
    graphics_context.command_processor,
    graphics_context.frame_queue_depth
  };
}

Optional<DrawableHandle>
ProceduralFlowerStemRenderer::create_drawable(const AddResourceContext& context,
                                              const tree::Internodes& internodes,
                                              const DrawableParams& params) {
  if (auto drawable = grove::create_drawable(context, internodes, params)) {
    DrawableHandle handle{next_drawable_id++};
    drawables[handle.id] = std::move(drawable.value());
    return Optional<DrawableHandle>(handle);
  } else {
    return NullOpt{};
  }
}

bool ProceduralFlowerStemRenderer::update_drawable(const AddResourceContext& context,
                                                   DrawableHandle handle,
                                                   const tree::Internodes& internodes,
                                                   const Vec3f& color) {
  if (auto it = drawables.find(handle.id); it != drawables.end()) {
    auto params = it->second.params;
    params.color = color;
    if (auto drawable = grove::create_drawable(context, internodes, params)) {
      it->second = std::move(drawable.value());
      return true;
    }
  }
  return false;
}

void ProceduralFlowerStemRenderer::set_active(DrawableHandle handle, bool active) {
  if (auto it = drawables.find(handle.id); it != drawables.end()) {
    it->second.inactive = !active;
  } else {
    assert(false);
  }
}

GROVE_NAMESPACE_END
