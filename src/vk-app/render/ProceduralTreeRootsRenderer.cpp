#include "ProceduralTreeRootsRenderer.hpp"
#include "utility.hpp"
#include "debug_label.hpp"
#include "graphics_context.hpp"
#include "../procedural_flower/geometry.hpp"
#include "grove/common/common.hpp"
#include "grove/common/pack.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/visual/image_process.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace vk;

using InitInfo = ProceduralTreeRootsRenderer::InitInfo;
using DrawableHandle = ProceduralTreeRootsRenderer::DrawableHandle;
using InstanceData = ProceduralTreeRootsRenderer::Instance;
using GeometryBuffer = ProceduralTreeRootsRenderer::GeometryBuffer;

struct PushConstantData {
  Mat4f projection_view;
  Vec4f num_points_xz_sun_position_xy;
  Vec4f sun_position_z_sun_color_xyz;
};

struct WindPushConstantData {
  Mat4f projection_view;
  Vec4<uint32_t> num_points_xz_color_sun_position_xy;
  Vec4f sun_position_z_sun_color_xyz;
  Vec4f aabb_p0_t;
  Vec4f aabb_p1_wind_strength;
};

Mat4f get_camera_projection_view(const Camera& camera) {
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  return proj * camera.get_view();
}

PushConstantData make_push_constant_data(const Mat4f& proj_view,
                                         const GridGeometryParams& geom_params,
                                         const Vec3f& sun_pos, const Vec3f& sun_color) {
  PushConstantData result{};
  result.projection_view = proj_view;
  result.num_points_xz_sun_position_xy = Vec4f{
    float(geom_params.num_pts_x), float(geom_params.num_pts_z), sun_pos.x, sun_pos.y
  };
  result.sun_position_z_sun_color_xyz = Vec4f{sun_pos.z, sun_color.x, sun_color.y, sun_color.z};
  return result;
}

WindPushConstantData make_wind_push_constant_data(const Camera& camera,
                                                  const GridGeometryParams& geom_params,
                                                  const Vec4<uint8_t>& lin_color,
                                                  const Vec3f& sun_pos, const Vec3f& sun_color,
                                                  const Bounds3f& bounds, float elapsed_time,
                                                  float wind_strength) {
  const uint32_t num_ps = uint32_t(geom_params.num_pts_x) | (uint32_t(geom_params.num_pts_z) << 16u);
  uint32_t sun_pos_x;
  memcpy(&sun_pos_x, &sun_pos.x, sizeof(float));

  uint32_t sun_pos_y;
  memcpy(&sun_pos_y, &sun_pos.y, sizeof(float));

  const uint32_t color = pack::pack_4u8_1u32(lin_color.x, lin_color.y, lin_color.z, lin_color.w);

  WindPushConstantData result{};
  result.projection_view = get_camera_projection_view(camera);
  result.num_points_xz_color_sun_position_xy = Vec4<uint32_t>{
    num_ps, color, sun_pos_x, sun_pos_y
  };
  result.sun_position_z_sun_color_xyz = Vec4f{sun_pos.z, sun_color.x, sun_color.y, sun_color.z};
  result.aabb_p0_t = Vec4f{bounds.min, elapsed_time};
  result.aabb_p1_wind_strength = Vec4f{bounds.max, wind_strength};
  return result;
}

Vec4<uint8_t> default_roots_color() {
  auto res = image::srgb_to_linear(Vec3f(0.47f, 0.26f, 0.02f)) * 255.0f;
  return Vec4<uint8_t>{
    uint8_t(res.x),
    uint8_t(res.y),
    uint8_t(res.z),
    255
  };
}

std::array<VertexBufferDescriptor, 2> vertex_buffer_descriptors() {
  std::array<VertexBufferDescriptor, 2> descs;
  //  Position
  descs[0].add_attribute(AttributeDescriptor::float2(0));
  // Dynamic instance data
  descs[1].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(1, 4, 1)); //  directions0
  descs[1].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(2, 4, 1)); //  directions1
  descs[1].add_attribute(AttributeDescriptor::float4(3, 1)); //  pos, radius
  descs[1].add_attribute(AttributeDescriptor::float4(4, 1)); //  child pos, child radius
  return descs;
}

std::array<VertexBufferDescriptor, 3> wind_vertex_buffer_descriptors() {
  std::array<VertexBufferDescriptor, 3> descs;
  //  Position
  descs[0].add_attribute(AttributeDescriptor::float2(0));
  // Dynamic instance data
  descs[1].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(1, 4, 1)); //  directions0
  descs[1].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(2, 4, 1)); //  directions1
  descs[1].add_attribute(AttributeDescriptor::float4(3, 1)); //  pos, radius
  descs[1].add_attribute(AttributeDescriptor::float4(4, 1)); //  child pos, child radius
  //  Wind
  descs[2].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(5, 4, 1)); //  packed_axis_root_info0
  descs[2].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(6, 4, 1)); //  packed_axis_root_info1
  descs[2].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(7, 4, 1)); //  packed_axis_root_info2
  return descs;
}

Optional<glsl::VertFragProgramSource> create_forward_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "proc-tree/roots-pack.vert";
  params.frag_file = "proc-tree/roots.frag";
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_shadow_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "proc-tree/roots-pack.vert";
  params.frag_file = "shadow/empty.frag";
  params.compile.vert_defines.push_back(glsl::make_define("IS_SHADOW"));
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_wind_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "proc-tree/roots-wind.vert";
  params.frag_file = "proc-tree/roots-wind.frag";
  return glsl::make_vert_frag_program_source(params);
}

template <typename BuffDescs>
Result<Pipeline> create_pipeline(VkDevice device, const glsl::VertFragProgramSource& source,
                                 const BuffDescs& buff_descs,
                                 const PipelineRenderPassInfo& pass_info, VkPipelineLayout layout,
                                 int num_color_attach) {
  auto view_descs = make_data_array_view<const VertexBufferDescriptor>(buff_descs);
  auto config_params = [num_color_attach](DefaultConfigureGraphicsPipelineStateParams& params) {
    params.num_color_attachments = num_color_attach;
  };
  SimpleVertFragGraphicsPipelineCreateInfo create_info{};
  configure_pipeline_create_info(
    &create_info, view_descs, source, pass_info, layout, config_params, {});
  return create_vert_frag_graphics_pipeline(device, &create_info);
}

template <typename BuffDescs>
Optional<vk::PipelineSystem::PipelineData>
create_pipeline_data(const InitInfo& info, const glsl::VertFragProgramSource& src,
                     const BuffDescs& buff_descs, int num_color_attachments,
                     const PipelineRenderPassInfo& pass_info) {
  vk::PipelineSystem::PipelineData result{};

  VkDevice device = info.core.device.handle;
  if (!info.pipeline_system.require_layouts(device, src, &result)) {
    return NullOpt{};
  }

  auto pipe_res = create_pipeline(
    device, src, buff_descs, pass_info, result.layout, num_color_attachments);
  if (!pipe_res) {
    return NullOpt{};
  } else {
    result.pipeline = info.pipeline_system.emplace(std::move(pipe_res.value));
  }

  return Optional<vk::PipelineSystem::PipelineData>(std::move(result));
}

Optional<vk::PipelineSystem::PipelineData> create_forward_pipeline_data(const InitInfo& info) {
  auto src = create_forward_program_source();
  if (!src) {
    return NullOpt{};
  } else {
    auto& pass_info = info.forward_pass_info;
    return create_pipeline_data(info, src.value(), vertex_buffer_descriptors(), 1, pass_info);
  }
}

Optional<vk::PipelineSystem::PipelineData> create_shadow_pipeline_data(const InitInfo& info) {
  auto src = create_shadow_program_source();
  if (!src) {
    return NullOpt{};
  } else {
    auto& pass_info = info.shadow_pass_info;
    return create_pipeline_data(info, src.value(), vertex_buffer_descriptors(), 0, pass_info);
  }
}

Optional<vk::PipelineSystem::PipelineData> create_forward_wind_pipeline_data(const InitInfo& info) {
  auto src = create_wind_program_source();
  if (!src) {
    return NullOpt{};
  } else {
    auto& pass_info = info.forward_pass_info;
    return create_pipeline_data(info, src.value(), wind_vertex_buffer_descriptors(), 1, pass_info);
  }
}

GridGeometryParams make_geometry_params() {
  GridGeometryParams geometry_params{};
  geometry_params.num_pts_x = 7;
  geometry_params.num_pts_z = 2;
  return geometry_params;
}

Optional<GeometryBuffer> create_geometry_buffer(const InitInfo& info,
                                                const GridGeometryParams& geom_params) {
  GeometryBuffer result{};

  const auto pos = make_reflected_grid_indices(geom_params.num_pts_x, geom_params.num_pts_z);
  const auto inds = triangulate_reflected_grid(geom_params.num_pts_x, geom_params.num_pts_z);

  auto geom_buffer = create_device_local_vertex_buffer(
    info.allocator, pos.size() * sizeof(float), true);
  if (!geom_buffer) {
    return NullOpt{};
  }
  auto ind_buffer = create_device_local_index_buffer(
    info.allocator, inds.size() * sizeof(uint16_t), true);
  if (!ind_buffer) {
    return NullOpt{};
  }

  auto upload_ctx = make_upload_from_staging_buffer_context(
    &info.core,
    info.allocator,
    &info.staging_buffer_system,
    &info.command_processor);

  const ManagedBuffer* dst_buffs[2] = {&geom_buffer.value, &ind_buffer.value};
  const void* src_datas[2] = {pos.data(), inds.data()};
  auto res = upload_from_staging_buffer_sync(src_datas, dst_buffs, nullptr, 2, upload_ctx);
  if (!res) {
    return NullOpt{};
  } else {
    result.geom_buff = info.buffer_system.emplace(std::move(geom_buffer.value));
    result.index_buff = info.buffer_system.emplace(std::move(ind_buffer.value));
  }

  result.num_indices = uint32_t(inds.size());
  return Optional<GeometryBuffer>(std::move(result));
}

bool any_active_drawables(const ProceduralTreeRootsRenderer& renderer,
                          ProceduralTreeRootsRenderer::DrawableType type) {
  for (auto& [_, drawable] : renderer.drawables) {
    if (drawable.type == type && drawable.num_instances_active > 0 && !drawable.hidden) {
      return true;
    }
  }
  return false;
}

bool any_active_drawables(const ProceduralTreeRootsRenderer& renderer) {
  for (auto& [_, drawable] : renderer.drawables) {
    if (drawable.num_instances_active > 0) {
      return true;
    }
  }
  return false;
}

auto* find_drawable(ProceduralTreeRootsRenderer& renderer, DrawableHandle handle) {
  if (auto it = renderer.drawables.find(handle.id); it != renderer.drawables.end()) {
    return &it->second;
  } else {
    decltype(&it->second) ptr{nullptr};
    return ptr;
  }
}

} //  anon

bool ProceduralTreeRootsRenderer::is_valid() const {
  return initialized;
}

bool ProceduralTreeRootsRenderer::initialize(const InitInfo& info) {
  if (auto pd = create_forward_pipeline_data(info)) {
    pipeline_data = std::move(pd.value());
  } else {
    return false;
  }

  if (auto pd = create_shadow_pipeline_data(info)) {
    shadow_pipeline_data = std::move(pd.value());
  } else {
    return false;
  }

  if (auto pd = create_forward_wind_pipeline_data(info)) {
    wind_pipeline_data = std::move(pd.value());
  } else {
    return false;
  }

  auto geom_buff = create_geometry_buffer(info, make_geometry_params());
  if (geom_buff) {
    geometry_buffer = std::move(geom_buff.value());
  } else {
    return false;
  }

  initialized = true;
  return true;
}

void ProceduralTreeRootsRenderer::remake_programs(const InitInfo& info) {
  const bool orig_init = initialized;
  initialized = false;

  if (auto pd = create_forward_pipeline_data(info)) {
    pipeline_data = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_shadow_pipeline_data(info)) {
    shadow_pipeline_data = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_forward_wind_pipeline_data(info)) {
    wind_pipeline_data = std::move(pd.value());
  } else {
    return;
  }

  initialized = orig_init;
}

void ProceduralTreeRootsRenderer::begin_frame(const BeginFrameInfo& info) {
  for (auto& [_, drawable] : drawables) {
    if (drawable.needs_update[info.frame_index]) {
      {
        auto* src = drawable.cpu_data.data();
        size_t off = sizeof(InstanceData) * drawable.num_instances_reserved * info.frame_index;
        size_t sz = sizeof(InstanceData) * drawable.num_instances_active;
        drawable.instance_buffer.get().write(src, sz, off);
      }

      if (drawable.type == DrawableType::Wind) {
        auto* src = drawable.wind_cpu_data.data();
        size_t off = sizeof(WindInstance) * drawable.num_instances_reserved * info.frame_index;
        size_t sz = sizeof(WindInstance) * drawable.num_instances_active;
        drawable.wind_instance_buffer.get().write(src, sz, off);
      }

      drawable.needs_update[info.frame_index] = false;
    }
  }

  render_params.elapsed_time = float(stopwatch.delta().count());
}

void ProceduralTreeRootsRenderer::draw_non_wind(VkCommandBuffer cmd, uint32_t frame_index,
                                                bool enforce_drawable_type) {
  const auto ind_buff = geometry_buffer.index_buff.get().contents().buffer.handle;
  vkCmdBindIndexBuffer(cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);

  for (auto& [_, drawable] : drawables) {
    if (drawable.num_instances_active == 0 ||
        drawable.hidden ||
        (enforce_drawable_type && drawable.type != DrawableType::NoWind)) {
      continue;
    }

    VkBuffer buffs[2] = {
      geometry_buffer.geom_buff.get().contents().buffer.handle,
      drawable.instance_buffer.get().contents().buffer.handle
    };

    const VkDeviceSize buff_offs[2] = {
      0,
      drawable.num_instances_reserved * sizeof(InstanceData) * frame_index
    };

    vkCmdBindVertexBuffers(cmd, 0, 2, buffs, buff_offs);

    vk::DrawIndexedDescriptor draw_desc{};
    draw_desc.num_indices = geometry_buffer.num_indices;
    draw_desc.num_instances = drawable.num_instances_active;
    cmd::draw_indexed(cmd, &draw_desc);
  }
}

void ProceduralTreeRootsRenderer::render(const RenderInfo& info) {
  auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "render_tree_roots");
  (void) profiler;

  render_non_wind(info);
  render_wind(info);
}

void ProceduralTreeRootsRenderer::render_non_wind(const RenderInfo& info) {
  if (!any_active_drawables(*this, DrawableType::NoWind)) {
    return;
  }

  auto& pd = pipeline_data;

  cmd::bind_graphics_pipeline(info.cmd, pd.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);

  auto pc_data = make_push_constant_data(
    get_camera_projection_view(info.camera),
    make_geometry_params(), render_params.sun_position, render_params.sun_color);
  auto pc_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  cmd::push_constants(info.cmd, pd.layout, pc_stages, &pc_data);

  draw_non_wind(info.cmd, info.frame_index, true);
}

void ProceduralTreeRootsRenderer::render_wind(const RenderInfo& info) {
  if (!any_active_drawables(*this, DrawableType::Wind)) {
    return;
  }

  auto& pd = wind_pipeline_data;

  cmd::bind_graphics_pipeline(info.cmd, pd.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);

  auto geom_params = make_geometry_params();
  const auto ind_buff = geometry_buffer.index_buff.get().contents().buffer.handle;
  vkCmdBindIndexBuffer(info.cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);

  for (auto& [_, drawable] : drawables) {
    if (drawable.num_instances_active == 0 || drawable.hidden ||
        drawable.type != DrawableType::Wind) {
      continue;
    }

    const float wind_strength = drawable.wind_disabled ? 0.0f : drawable.wind_strength;
    auto pc_data = make_wind_push_constant_data(
      info.camera, geom_params, drawable.color, render_params.sun_position, render_params.sun_color,
      drawable.aabb, render_params.elapsed_time, wind_strength);

    auto pc_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    cmd::push_constants(info.cmd, pd.layout, pc_stages, &pc_data);

    VkBuffer buffs[3] = {
      geometry_buffer.geom_buff.get().contents().buffer.handle,
      drawable.instance_buffer.get().contents().buffer.handle,
      drawable.wind_instance_buffer.get().contents().buffer.handle
    };

    const VkDeviceSize buff_offs[3] = {
      0,
      drawable.num_instances_reserved * sizeof(InstanceData) * info.frame_index,
      drawable.num_instances_reserved * sizeof(WindInstance) * info.frame_index
    };

    vkCmdBindVertexBuffers(info.cmd, 0, 3, buffs, buff_offs);

    vk::DrawIndexedDescriptor draw_desc{};
    draw_desc.num_indices = geometry_buffer.num_indices;
    draw_desc.num_instances = drawable.num_instances_active;
    cmd::draw_indexed(info.cmd, &draw_desc);
  }
}

void ProceduralTreeRootsRenderer::render_shadow(const ShadowRenderInfo& info) {
  if (!any_active_drawables(*this)) {
    return;
  }

  auto& pd = shadow_pipeline_data;

  cmd::bind_graphics_pipeline(info.cmd, pd.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);

  auto geom_params = make_geometry_params();
  auto pc_data = make_push_constant_data(
    info.shadow_view_proj, geom_params, render_params.sun_position, render_params.sun_color);
  auto pc_stages = VK_SHADER_STAGE_VERTEX_BIT;
  cmd::push_constants(info.cmd, pd.layout, pc_stages, &pc_data);

  draw_non_wind(info.cmd, info.frame_index, false); //  false -> draw both wind and non wind
}

DrawableHandle ProceduralTreeRootsRenderer::create(DrawableType type) {
  DrawableHandle result{next_drawable_id++, type};
  Drawable drawable{};
  drawable.type = type;
  drawable.color = default_roots_color();
  drawables[result.id] = std::move(drawable);
  return result;
}

void ProceduralTreeRootsRenderer::fill_activate(const AddResourceContext& context,
                                                DrawableHandle handle, const Instance* instances,
                                                uint32_t num_instances) {
  fill_activate(context, handle, instances, nullptr, num_instances);
}

void ProceduralTreeRootsRenderer::fill_activate(const AddResourceContext& context,
                                                DrawableHandle handle, const Instance* instances,
                                                const WindInstance* wind_instances,
                                                uint32_t num_instances) {
  if (reserve(context, handle, num_instances)) {
    set(context, handle, instances, wind_instances, num_instances, 0);
    activate(handle, num_instances);
  }
}

void ProceduralTreeRootsRenderer::set_hidden(DrawableHandle handle, bool v) {
  if (auto* drawable = find_drawable(*this, handle)) {
    drawable->hidden = v;
  } else {
    assert(false);
  }
}

void ProceduralTreeRootsRenderer::set_aabb(DrawableHandle handle, const Bounds3f& aabb) {
  if (auto* drawable = find_drawable(*this, handle)) {
    drawable->aabb = aabb;
  } else {
    assert(false);
  }
}

void ProceduralTreeRootsRenderer::set_wind_strength(DrawableHandle handle, float v) {
  if (auto* drawable = find_drawable(*this, handle)) {
    assert(drawable->type == DrawableType::Wind);
    drawable->wind_strength = v;
  } else {
    assert(false);
  }
}

void ProceduralTreeRootsRenderer::set_wind_disabled(DrawableHandle handle, bool disable) {
  if (auto* drawable = find_drawable(*this, handle)) {
    assert(drawable->type == DrawableType::Wind);
    drawable->wind_disabled = disable;
  } else {
    assert(false);
  }
}

void ProceduralTreeRootsRenderer::set_linear_color(DrawableHandle handle, const Vec3<uint8_t>& color) {
  if (auto* drawable = find_drawable(*this, handle)) {
    drawable->color = Vec4<uint8_t>{color.x, color.y, color.z, 255};
  } else {
    assert(false);
  }
}

void ProceduralTreeRootsRenderer::set(const AddResourceContext& context, DrawableHandle handle,
                                      const Instance* instances, const WindInstance* wind_instances,
                                      uint32_t num_instances, uint32_t instance_offset) {
  auto* drawable = find_drawable(*this, handle);
  if (!drawable) {
    assert(false);
    return;
  }

  assert(drawable->num_instances_reserved >= instance_offset + num_instances);

  if (instances) {
    auto* cpu_dst = drawable->cpu_data.data() + sizeof(InstanceData) * instance_offset;
    memcpy(cpu_dst, instances, sizeof(InstanceData) * num_instances);
  }

  if (wind_instances) {
    assert(drawable->type == DrawableType::Wind && handle.type == DrawableType::Wind);
    auto* cpu_dst = drawable->wind_cpu_data.data() + sizeof(WindInstance) * instance_offset;
    memcpy(cpu_dst, wind_instances, sizeof(WindInstance) * num_instances);
  }

  for (uint32_t i = 0; i < context.frame_queue_depth; i++) {
    drawable->needs_update[i] = true;
  }
}

void ProceduralTreeRootsRenderer::activate(DrawableHandle handle, uint32_t num_instances) {
  auto* drawable = find_drawable(*this, handle);
  if (!drawable) {
    assert(false);
    return;
  }

  assert(drawable->num_instances_reserved >= num_instances);
  drawable->num_instances_active = num_instances;
}

bool ProceduralTreeRootsRenderer::reserve(const AddResourceContext& context,
                                          DrawableHandle handle, uint32_t num_instances) {
  auto* drawable = find_drawable(*this, handle);
  if (!drawable) {
    assert(false);
    return false;
  } else if (drawable->num_instances_reserved >= num_instances) {
    return true;
  }

  auto num_reserve = !drawable->num_instances_reserved ? 8 : drawable->num_instances_reserved * 2;
  while (num_reserve < num_instances) {
    num_reserve *= 2;
  }

  const auto inst_buff_sz = num_reserve * sizeof(InstanceData) * context.frame_queue_depth;
  auto inst_buff = create_host_visible_vertex_buffer(context.allocator, inst_buff_sz);
  if (!inst_buff) {
    return false;
  }

  vk::BufferSystem::BufferHandle wind_buff;
  if (drawable->type == DrawableType::Wind) {
    const auto wind_buff_sz = num_reserve * sizeof(WindInstance) * context.frame_queue_depth;
    auto wind_inst_buff = create_host_visible_vertex_buffer(context.allocator, wind_buff_sz);
    if (!wind_inst_buff) {
      return false;
    } else {
      wind_buff = context.buffer_system.emplace(std::move(wind_inst_buff.value));
      drawable->wind_cpu_data.resize(num_reserve * sizeof(WindInstance));
    }
  }

  drawable->cpu_data.resize(num_reserve * sizeof(InstanceData));
  drawable->instance_buffer = context.buffer_system.emplace(std::move(inst_buff.value));
  drawable->wind_instance_buffer = std::move(wind_buff);
  drawable->num_instances_reserved = num_reserve;
  return true;
}

namespace {

uint16_t float_to_u16(float v) {
  v = clamp(v, -1.0f, 1.0f);
  return uint16_t(clamp((v * 0.5f + 0.5f) * float(0xffff), 0.0f, float(0xffff)));
}

uint32_t to_u32(float c, float s) {
  return (uint32_t(float_to_u16(c)) << 16u) | uint32_t(float_to_u16(s));
}

} //  anon

void ProceduralTreeRootsRenderer::encode_directions(const Vec3f& self_right, const Vec3f& self_up,
                                                    const Vec3f& child_right, const Vec3f& child_up,
                                                    Vec4<uint32_t>* directions0,
                                                    Vec4<uint32_t>* directions1) {
  Vec4<uint32_t> dir0{};
  for (int i = 0; i < 3; i++) {
    dir0[i] = to_u32(child_right[i], self_right[i]);
  }
  dir0[3] = to_u32(child_up[0], self_up[0]);

  Vec4<uint32_t> dir1{};
  for (int i = 0; i < 2; i++) {
    dir1[i] = to_u32(child_up[i+1], self_up[i+1]);
  }

  *directions0 = dir0;
  *directions1 = dir1;
}

ProceduralTreeRootsRenderer::AddResourceContext
ProceduralTreeRootsRenderer::make_add_resource_context(vk::GraphicsContext& graphics_context) {
  return ProceduralTreeRootsRenderer::AddResourceContext{
    graphics_context.core,
    &graphics_context.allocator,
    graphics_context.command_processor,
    graphics_context.buffer_system,
    graphics_context.staging_buffer_system,
    graphics_context.frame_queue_depth
  };
}

GROVE_NAMESPACE_END
