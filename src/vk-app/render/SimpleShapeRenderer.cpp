#include "SimpleShapeRenderer.hpp"
#include "./graphics.hpp"
#include "./graphics_context.hpp"
#include "memory.hpp"
#include "debug_label.hpp"
#include "grove/env.hpp"
#include "grove/common/common.hpp"
#include "grove/common/pack.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/load/obj.hpp"
#include "../model/mesh.hpp"

GROVE_NAMESPACE_BEGIN

struct SimpleShapeRendererNewGraphicsContextImpl {
  gfx::BufferHandle two_side_vertices_buffer;
  gfx::PipelineHandle two_sided_triangle_pipeline;
};

using namespace vk;

namespace {

using GeometryHandle = SimpleShapeRenderer::GeometryHandle;
using DrawableHandle = SimpleShapeRenderer::DrawableHandle;
using InstanceData = SimpleShapeRenderer::InstanceData;

struct PushConstantData {
  Mat4f projection_view;
};

struct Vertex {
  static std::array<VertexBufferDescriptor, 2> buffer_descriptors() {
    std::array<VertexBufferDescriptor, 2> result;
    result[0].add_attribute(AttributeDescriptor::float3(0));    //  position
    result[1].add_attribute(AttributeDescriptor::float4(1, 1)); //  color
    result[1].add_attribute(AttributeDescriptor::float4(2, 1)); //  scale, unused
    result[1].add_attribute(AttributeDescriptor::float4(3, 1)); //  translation, unused
    return result;
  }

  Vec3f position;
};

PushConstantData make_push_constant_data(const Camera& camera) {
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  PushConstantData result;
  result.projection_view = proj * camera.get_view();
  return result;
}

Optional<glsl::VertFragProgramSource> create_two_sided_triangle_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "two-sided-triangle.glsl";
  params.compile.vert_defines.push_back(glsl::make_define("IS_VERTEX"));
  params.frag_file = "two-sided-triangle.glsl";
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_program_source(bool oriented) {
  glsl::LoadVertFragProgramSourceParams params{};
  if (oriented) {
    params.vert_file = "static-model/oriented-simple-shape.vert";
  } else {
    params.vert_file = "static-model/simple-shape.vert";
  }
  params.frag_file = "static-model/simple-shape.frag";
  return glsl::make_vert_frag_program_source(params);
}

Result<Pipeline> create_pipeline(VkDevice device,
                                 const glsl::VertFragProgramSource& source,
                                 const vk::PipelineRenderPassInfo& pass_info,
                                 VkPipelineLayout layout, bool oriented) {
  auto buff_descrs = Vertex::buffer_descriptors();
  VertexInputDescriptors input_descrs{};
  to_vk_vertex_input_descriptors(uint32_t(buff_descrs.size()), buff_descrs.data(), &input_descrs);
  DefaultConfigureGraphicsPipelineStateParams params{input_descrs};
  params.num_color_attachments = 1;
  params.raster_samples = pass_info.raster_samples;
  if (oriented) {
    params.cull_mode = VK_CULL_MODE_NONE; //  @TODO
  }
  GraphicsPipelineStateCreateInfo state{};
  default_configure(&state, params);
  return create_vert_frag_graphics_pipeline(
    device, source.vert_bytecode, source.frag_bytecode,
    &state, layout, pass_info.render_pass, pass_info.subpass);
}

} //  anon

SimpleShapeRenderer::SimpleShapeRenderer() :
  graphics_context_impl{new SimpleShapeRendererNewGraphicsContextImpl()} {
  //
}

SimpleShapeRenderer::~SimpleShapeRenderer() {
  delete graphics_context_impl;
}

bool SimpleShapeRenderer::is_valid() const {
  return initialized;
}

void SimpleShapeRenderer::push_two_sided_triangles(const Vec3f* p, uint32_t num_ps,
                                                   const Vec3f& color) {
  auto dst = uint32_t(two_sided_vertices.size());
  two_sided_vertices.resize(dst + num_ps);
  auto col = clamp_each(color, Vec3f{}, Vec3f{1.0f}) * 255.0f;
  for (uint32_t i = 0; i < num_ps; i++) {
    auto& v = two_sided_vertices[i + dst];
    memcpy(&v.data.x, &p[i].x, 3 * sizeof(float));
    v.data.w = pack::pack_4u8_1u32(uint8_t(col.x), uint8_t(col.y), uint8_t(col.z), 0);
  }
}

void SimpleShapeRenderer::terminate() {
  delete graphics_context_impl;
  graphics_context_impl = nullptr;
}

bool SimpleShapeRenderer::initialize(const InitInfo& info) {
  frame_queue_depth = info.frame_queue_depth;

  {
    const bool is_oriented = false;
    auto get_source = [is_oriented]() {
      (void) is_oriented;
      return create_program_source(is_oriented);
    };
    auto make_pipeline = [&](VkDevice device, const glsl::VertFragProgramSource& source,
                             VkPipelineLayout layout) {
      return create_pipeline(device, source, info.forward_pass_info, layout, is_oriented);
    };

    auto pd = info.pipeline_system.create_pipeline_data(
      info.core.device.handle, get_source, make_pipeline, nullptr);
    if (!pd) {
      return false;
    } else {
      non_oriented_pipeline_data = std::move(pd.value());
    }
  }
  {
    const bool is_oriented = true;
    auto get_source = [is_oriented]() {
      (void) is_oriented;
      return create_program_source(is_oriented);
    };
    auto make_pipeline = [&](VkDevice device, const glsl::VertFragProgramSource& source,
                             VkPipelineLayout layout) {
      return create_pipeline(device, source, info.forward_pass_info, layout, is_oriented);
    };

    auto pd = info.pipeline_system.create_pipeline_data(
      info.core.device.handle, get_source, make_pipeline, nullptr);
    if (!pd) {
      return false;
    } else {
      oriented_pipeline_data = std::move(pd.value());
    }
  }
  {
    if (auto src = create_two_sided_triangle_program_source()) {
      auto pass_info = gfx::get_forward_write_back_render_pass_handle(info.graphics_context);
      if (pass_info) {
        VertexBufferDescriptor buff_desc;
        buff_desc.add_attribute(AttributeDescriptor::unconverted_unsigned_intn(0, 4));
        gfx::GraphicsPipelineCreateInfo create_info{};
        create_info.disable_cull_face = true;
        create_info.num_color_attachments = 1;
        create_info.vertex_buffer_descriptors = &buff_desc;
        create_info.num_vertex_buffer_descriptors = 1;
        auto pipe_res = gfx::create_pipeline(
          info.graphics_context, std::move(src.value()), create_info, pass_info.value());
        if (pipe_res) {
          graphics_context_impl->two_sided_triangle_pipeline = std::move(pipe_res.value());
        }
      }
    }
  }

  initialized = true;
  return true;
}

Optional<DrawableHandle> SimpleShapeRenderer::add_instances(const AddResourceContext& context,
                                                            GeometryHandle geometry,
                                                            int num_instances,
                                                            PipelineType pipeline_type) {
  auto buff_size = sizeof(InstanceData) * num_instances * context.frame_queue_depth;
  auto inst_buff_res = create_host_visible_vertex_buffer(context.allocator, buff_size);
  if (!inst_buff_res) {
    return NullOpt{};
  }

  auto cpu_data = std::make_unique<InstanceData[]>(num_instances);
  for (uint32_t i = 0; i < context.frame_queue_depth; i++) {
    size_t size = num_instances * sizeof(InstanceData);
    size_t off = size * i;
    inst_buff_res.value.write(cpu_data.get(), size, off);
  }

  Drawable drawable{};
  drawable.cpu_instance_data = std::move(cpu_data);
  drawable.geometry_handle = geometry;
  drawable.num_instances = uint32_t(num_instances);
  drawable.num_active_instances = uint32_t(num_instances);
  drawable.instance_buffer = context.buffer_system.emplace(std::move(inst_buff_res.value));
  drawable.pipeline_type = pipeline_type;
  GROVE_ASSERT(frame_queue_depth == context.frame_queue_depth);

  DrawableHandle handle{next_drawable_id++};
  drawables[handle] = std::move(drawable);
  return Optional<DrawableHandle>(handle);
}

Optional<GeometryHandle> SimpleShapeRenderer::require_cube(const AddResourceContext& context) {
  if (cube_geometry) {
    return cube_geometry;
  }
  std::vector<float> geom = geometry::cube_positions();
  std::vector<uint16_t> inds = geometry::cube_indices();
  VertexBufferDescriptor geom_desc;
  geom_desc.add_attribute(AttributeDescriptor::float3(0));
  auto handle_res = add_geometry(
    context, geom.data(), geom.size() * sizeof(float),
    geom_desc, 0, inds.data(), uint32_t(inds.size()));
  if (handle_res) {
    cube_geometry = handle_res.value();
  }
  return handle_res;
}

Optional<GeometryHandle> SimpleShapeRenderer::require_plane(const AddResourceContext& context) {
  if (plane_geometry) {
    return plane_geometry;
  }
  auto plane_geom = geometry::quad_positions(true, 0.0f);
  auto inds = geometry::quad_indices();
  VertexBufferDescriptor geom_desc;
  geom_desc.add_attribute(AttributeDescriptor::float3(0));
  auto handle_res = add_geometry(
    context, plane_geom.data(), plane_geom.size() * sizeof(float),
    geom_desc, 0, inds.data(), uint32_t(inds.size()));
  if (handle_res) {
    plane_geometry = handle_res.value();
  }
  return handle_res;
}

Optional<GeometryHandle> SimpleShapeRenderer::require_sphere(const AddResourceContext& context) {
  if (sphere_geometry) {
    return sphere_geometry;
  }
  auto model_p = std::string{GROVE_ASSET_DIR} + "/models/sphere";
  auto model_file = model_p + "/sphere.obj";
  bool success{};
  auto obj_data = obj::load_simple(model_file.c_str(), model_p.c_str(), &success);
  if (!success) {
    return NullOpt{};
  }
  int pos_ind{};
  if (auto attr_ind = obj_data.find_attribute(obj::AttributeType::Position)) {
    pos_ind = attr_ind.value();
  } else {
    return NullOpt{};
  }
  auto desc = vertex_buffer_descriptor_from_obj_data(obj_data);
  auto handle_res = add_geometry(
    context, obj_data.packed_data.data(), obj_data.packed_data.size() * sizeof(float),
    desc, pos_ind, nullptr, 0);
  if (handle_res) {
    sphere_geometry = handle_res.value();
  }
  return handle_res;
}

void SimpleShapeRenderer::destroy_instances(DrawableHandle handle) {
  if (auto it = drawables.find(handle); it != drawables.end()) {
    drawables.erase(it);
  } else {
    GROVE_ASSERT(false);
  }
  remove_active_drawable(handle);
}

Optional<GeometryHandle> SimpleShapeRenderer::add_geometry(const AddResourceContext& context,
                                                           const void* data,
                                                           size_t size,
                                                           const VertexBufferDescriptor& desc,
                                                           int pos_attr_index,
                                                           const uint16_t* indices,
                                                           uint32_t num_indices) {
  const auto num_verts = int(desc.num_vertices(size));
  auto buff_descs = Vertex::buffer_descriptors();
  const auto& dst_desc = buff_descs[0];
  std::vector<Vertex> vertices(num_verts);
  if (!copy_buffer(data, desc, &pos_attr_index, vertices.data(), dst_desc, 1, num_verts)) {
    return NullOpt{};
  }

  const size_t buff_size = vertices.size() * sizeof(Vertex);
  auto buff_res = create_device_local_vertex_buffer(context.allocator, buff_size, true);
  if (!buff_res) {
    return NullOpt{};
  }

  ManagedBuffer maybe_index_buffer;
  const size_t inds_size = sizeof(uint16_t) * num_indices;
  const bool has_indices = indices != nullptr;
  if (has_indices) {
    if (auto buff = create_device_local_index_buffer(context.allocator, inds_size, true)) {
      maybe_index_buffer = std::move(buff.value);
    } else {
      return NullOpt{};
    }
  }

  const ManagedBuffer* dst_buffs[] = {&buff_res.value, &maybe_index_buffer};
  const void* src_datas[] = {vertices.data(), indices};
  {
    auto upload_context = make_upload_from_staging_buffer_context(
      &context.core,
      context.allocator,
      &context.staging_buffer_system,
      &context.command_processor);

    const uint32_t num_buffs = has_indices ? 2 : 1;
    bool success = upload_from_staging_buffer_sync(
      src_datas, dst_buffs, nullptr, num_buffs, upload_context);
    if (!success) {
      return NullOpt{};
    }
  }

  Geometry geom{};
  geom.geometry_buffer = context.buffer_system.emplace(std::move(buff_res.value));
  if (has_indices) {
    geom.index_buffer = context.buffer_system.emplace(std::move(maybe_index_buffer));
  }
  geom.num_vertices = uint32_t(num_verts);
  geom.num_indices = num_indices;

  GeometryHandle handle{next_geometry_id++};
  geometries[handle] = std::move(geom);
  return Optional<GeometryHandle>(handle);
}

void SimpleShapeRenderer::add_active_drawable(DrawableHandle handle) {
#ifdef GROVE_DEBUG
  auto it = std::find(active_drawables.begin(), active_drawables.end(), handle);
  GROVE_ASSERT(it == active_drawables.end());
#endif
  active_drawables.push_back(handle);
}

void SimpleShapeRenderer::remove_active_drawable(DrawableHandle handle) {
  auto it = std::find(active_drawables.begin(), active_drawables.end(), handle);
  if (it != active_drawables.end()) {
    active_drawables.erase(it);
  }
}

void SimpleShapeRenderer::prepare_two_sided(gfx::Context* graphics_context, uint32_t frame_index) {
  num_two_sided_vertices_active = 0;

  if (two_sided_vertices.empty()) {
    return;
  }

  uint32_t num_reserve = num_two_sided_vertices_reserved;
  while (num_reserve < two_sided_vertices.size()) {
    num_reserve = num_reserve == 0 ? 64 : num_reserve * 2;
  }

  auto& two_side_vertices_buffer = graphics_context_impl->two_side_vertices_buffer;
  if (num_reserve != num_two_sided_vertices_reserved) {
    auto fq_depth = gfx::get_frame_queue_depth(graphics_context);
    auto buff = gfx::create_host_visible_vertex_buffer(
      graphics_context, fq_depth * sizeof(TwoSidedTriangleVertex) * num_reserve);
    if (!buff) {
      return;
    } else {
      two_side_vertices_buffer = std::move(buff.value());
      num_two_sided_vertices_reserved = num_reserve;
    }
  }

  auto off = num_two_sided_vertices_reserved * sizeof(TwoSidedTriangleVertex) * frame_index;
  two_side_vertices_buffer.write(
    two_sided_vertices.data(), two_sided_vertices.size() * sizeof(TwoSidedTriangleVertex), off);
  num_two_sided_vertices_active = uint32_t(two_sided_vertices.size());
  two_sided_vertices.clear();
}

void SimpleShapeRenderer::begin_frame(gfx::Context* graphics_context, uint32_t frame_index) {
  for (auto& handle : active_drawables) {
    if (auto it = drawables.find(handle); it != drawables.end()) {
      auto& drawable = it->second;
      if (drawable.instance_buffer_needs_update[frame_index]) {
        size_t size = sizeof(InstanceData) * drawable.num_instances;
        size_t off = size * frame_index;
        drawable.instance_buffer.get().write(drawable.cpu_instance_data.get(), size, off);
        drawable.instance_buffer_needs_update[frame_index] = false;
      }
    } else {
      GROVE_ASSERT(false);
    }
  }

  prepare_two_sided(graphics_context, frame_index);
}

void SimpleShapeRenderer::render_pipeline_type(const RenderInfo& info, PipelineType type) {
  for (auto& handle : active_drawables) {
    auto it = drawables.find(handle);
    if (it == drawables.end()) {
      GROVE_ASSERT(false);
      continue;
    }

    auto& drawable = it->second;
    if (drawable.num_active_instances == 0 || drawable.pipeline_type != type) {
      continue;
    }

    auto& geom = geometries.at(drawable.geometry_handle);
    const size_t inst_size = sizeof(InstanceData) * drawable.num_instances;
    const size_t inst_off = inst_size * info.frame_index;

    const VkBuffer vertex_buffs[2] = {
      geom.geometry_buffer.get().contents().buffer.handle,
      drawable.instance_buffer.get().contents().buffer.handle
    };
    const VkDeviceSize vb_offs[2] = {0, inst_off};

    vkCmdBindVertexBuffers(info.cmd, 0, 2, vertex_buffs, vb_offs);
    if (geom.index_buffer.is_valid()) {
      vkCmdBindIndexBuffer(
        info.cmd, geom.index_buffer.get().contents().buffer.handle, 0, VK_INDEX_TYPE_UINT16);
      DrawIndexedDescriptor draw_desc{};
      draw_desc.num_indices = geom.num_indices;
      draw_desc.num_instances = drawable.num_active_instances;
      cmd::draw_indexed(info.cmd, &draw_desc);
    } else {
      DrawDescriptor draw_desc{};
      draw_desc.num_vertices = geom.num_vertices;
      draw_desc.num_instances = drawable.num_active_instances;
      cmd::draw(info.cmd, &draw_desc);
    }
  }
}

void SimpleShapeRenderer::render_two_sided(const RenderInfo& info) {
  VkBuffer buffs[1] = {
    graphics_context_impl->two_side_vertices_buffer.get()
  };
  VkDeviceSize vb_offs[1] = {
    info.frame_index * sizeof(TwoSidedTriangleVertex) * num_two_sided_vertices_reserved
  };
  vkCmdBindVertexBuffers(info.cmd, 0, 1, buffs, vb_offs);

  vk::DrawDescriptor draw_desc{};
  draw_desc.num_vertices = num_two_sided_vertices_active;
  draw_desc.num_instances = 1;
  cmd::draw(info.cmd, &draw_desc);
}

void SimpleShapeRenderer::render(const RenderInfo& info) {
  if (active_drawables.empty() || disabled) {
    return;
  }

  auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "SimpleShapeRenderer");
  (void) profiler;

  const PushConstantData pc_data = make_push_constant_data(info.camera);
  const auto pc_stage = VK_SHADER_STAGE_VERTEX_BIT;

  cmd::bind_graphics_pipeline(info.cmd, non_oriented_pipeline_data.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  cmd::push_constants(info.cmd, non_oriented_pipeline_data.layout, pc_stage, &pc_data);
  render_pipeline_type(info, PipelineType::NonOriented);

  cmd::bind_graphics_pipeline(info.cmd, oriented_pipeline_data.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  cmd::push_constants(info.cmd, oriented_pipeline_data.layout, pc_stage, &pc_data);
  render_pipeline_type(info, PipelineType::Oriented);

  auto& two_sided_triangle_pipeline = graphics_context_impl->two_sided_triangle_pipeline;
  if (num_two_sided_vertices_active > 0 && two_sided_triangle_pipeline.is_valid()) {
    cmd::bind_graphics_pipeline(info.cmd, two_sided_triangle_pipeline.get());
    cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
    cmd::push_constants(info.cmd, two_sided_triangle_pipeline.get_layout(), pc_stage, &pc_data);
    render_two_sided(info);
  }
}

void SimpleShapeRenderer::set_instance_params(DrawableHandle handle, uint32_t instance,
                                              const Vec3f& color, const Vec3f& scale,
                                              const Vec3f& trans) {
  if (auto it = drawables.find(handle); it != drawables.end()) {
    auto& drawable = it->second;
    GROVE_ASSERT(instance < drawable.num_instances);
    GROVE_ASSERT(drawable.pipeline_type == PipelineType::NonOriented);
    InstanceData data;
    data.color = Vec4f{color, 1.0f};
    data.scale_active = Vec4f{scale, 1.0f}; //  1.0f -> active
    data.translation = Vec4f{trans, 0.0f};
    drawable.cpu_instance_data[instance] = data;
    drawable.num_active_instances = std::max(instance + 1, drawable.num_active_instances);
    for (uint32_t i = 0; i < frame_queue_depth; i++) {
      drawable.instance_buffer_needs_update[i] = true;
    }
  } else {
    GROVE_ASSERT(false);
  }
}

void SimpleShapeRenderer::set_oriented_instance_params(DrawableHandle handle, uint32_t instance,
                                                       const Vec3f& color, const Vec3f& scale,
                                                       const Vec3f& translation,
                                                       const Vec3f& right, const Vec3f& up) {
  if (auto it = drawables.find(handle); it != drawables.end()) {
    auto& drawable = it->second;
    GROVE_ASSERT(instance < drawable.num_instances);
    GROVE_ASSERT(drawable.pipeline_type == PipelineType::Oriented);

    auto col = clamp_each(color, Vec3f{}, Vec3f{1.0f}) * 255.0f;
    uint32_t packed_col = pack::pack_4u8_1u32(uint8_t(col.x), uint8_t(col.y), uint8_t(col.z), 255);

    auto xc = clamp_each(right, Vec3f{-1.0f}, Vec3f{1.0f}) * 0.5f + 0.5f;
    auto yc = clamp_each(up, Vec3f{-1.0f}, Vec3f{1.0f}) * 0.5f + 0.5f;
    uint32_t xc_xy = pack::pack_2fn_1u32(xc.x, xc.y);
    uint32_t xc_z_yc_x = pack::pack_2fn_1u32(xc.z, yc.x);
    uint32_t yc_yz = pack::pack_2fn_1u32(yc.y, yc.z);
    auto color_ori = Vec4<uint32_t>{packed_col, xc_xy, xc_z_yc_x, yc_yz};

    InstanceData data;
    static_assert(sizeof(float) == sizeof(uint32_t));
    memcpy(&data.color.x, &color_ori.x, 4 * sizeof(float));
    data.scale_active = Vec4f{scale, 1.0f}; //  1.0f -> active
    data.translation = Vec4f{translation, 0.0f};
    drawable.cpu_instance_data[instance] = data;
    drawable.num_active_instances = std::max(instance + 1, drawable.num_active_instances);
    for (uint32_t i = 0; i < frame_queue_depth; i++) {
      drawable.instance_buffer_needs_update[i] = true;
    }
  } else {
    GROVE_ASSERT(false);
  }
}

void SimpleShapeRenderer::set_active_instance(DrawableHandle handle, uint32_t instance,
                                              bool active) {
  if (auto it = drawables.find(handle); it != drawables.end()) {
    auto& drawable = it->second;
    GROVE_ASSERT(instance < drawable.num_instances);
    auto& data = drawable.cpu_instance_data[instance];
    data.scale_active.w = float(active);
    if (active) {
      drawable.num_active_instances = std::max(instance + 1, drawable.num_active_instances);
    } else if (instance + 1 == drawable.num_active_instances) {
      drawable.num_active_instances--;
    }
    for (uint32_t i = 0; i < frame_queue_depth; i++) {
      drawable.instance_buffer_needs_update[i] = true;
    }
  } else {
    GROVE_ASSERT(false);
  }
}

void SimpleShapeRenderer::clear_active_instances(DrawableHandle handle) {
  if (auto it = drawables.find(handle); it != drawables.end()) {
    auto& drawable = it->second;
    drawable.num_active_instances = 0;
    for (uint32_t i = 0; i < drawable.num_instances; i++) {
      drawable.cpu_instance_data[i].scale_active.w = 0.0f;  //  deactivate
    }
    for (uint32_t i = 0; i < frame_queue_depth; i++) {
      drawable.instance_buffer_needs_update[i] = true;
    }
  } else {
    GROVE_ASSERT(false);
  }
}

void SimpleShapeRenderer::attenuate_active_instance_scales(DrawableHandle handle, float s) {
  if (auto it = drawables.find(handle); it != drawables.end()) {
    auto& drawable = it->second;
    for (uint32_t i = 0; i < drawable.num_active_instances; i++) {
      auto scale = to_vec3(drawable.cpu_instance_data[i].scale_active) * s;
      drawable.cpu_instance_data[i].scale_active = Vec4f{
        scale, drawable.cpu_instance_data[i].scale_active.w};
    }
    for (uint32_t i = 0; i < frame_queue_depth; i++) {
      drawable.instance_buffer_needs_update[i] = true;
    }
  } else {
    GROVE_ASSERT(false);
  }
}

SimpleShapeRenderer::AddResourceContext
SimpleShapeRenderer::make_add_resource_context(vk::GraphicsContext& graphics_context) {
  return SimpleShapeRenderer::AddResourceContext{
    graphics_context.core,
    &graphics_context.allocator,
    graphics_context.command_processor,
    graphics_context.buffer_system,
    graphics_context.staging_buffer_system,
    graphics_context.frame_queue_depth
  };
}

GROVE_NAMESPACE_END
