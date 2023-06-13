#include "PointBufferRenderer.hpp"
#include "memory.hpp"
#include "./graphics_context.hpp"
#include "grove/common/common.hpp"
#include "grove/visual/Camera.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

using DrawableHandle = PointBufferRenderer::DrawableHandle;
using DrawableParams = PointBufferRenderer::DrawableParams;

struct PointVertex {
  Vec3f position;
};

struct LineVertex {
  Vec3f position;
  Vec3f color;
};

struct PointPushConstantData {
  Mat4f projection_view;
  Vec4f color_point_size;
};

struct LinePushConstantData {
  Mat4f projection_view;
};

bool is_line_type(PointBufferRenderer::DrawableType type) {
  return type == PointBufferRenderer::DrawableType::Lines;
}

void set_drawable_params(PointPushConstantData& data, const DrawableParams& params) {
  data.color_point_size = Vec4f{params.color, params.point_size};
}

Mat4f get_projection_view(const Camera& camera) {
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  return proj * camera.get_view();
}

PointPushConstantData make_point_push_constant_data(const Camera& camera,
                                                    const DrawableParams& drawable_params) {
  PointPushConstantData result;
  result.projection_view = get_projection_view(camera);
  set_drawable_params(result, drawable_params);
  return result;
}

LinePushConstantData make_line_push_constant_data(const Camera& camera,
                                                  const DrawableParams&) {
  LinePushConstantData result;
  result.projection_view = get_projection_view(camera);
  return result;
}

VertexBufferDescriptor point_vertex_buffer_descriptor() {
  VertexBufferDescriptor result;
  result.add_attribute(AttributeDescriptor::float3(0));
  return result;
}

VertexBufferDescriptor line_vertex_buffer_descriptor() {
  VertexBufferDescriptor result;
  result.add_attribute(AttributeDescriptor::float3(0));
  result.add_attribute(AttributeDescriptor::float3(1));
  return result;
}

Optional<glsl::VertFragProgramSource> create_point_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "debug/points.vert";
  params.frag_file = "debug/points.frag";
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_line_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "debug/lines.vert";
  params.frag_file = "debug/lines.frag";
  return glsl::make_vert_frag_program_source(params);
}

Result<Pipeline> create_pipeline(VkDevice device,
                                 const VertexBufferDescriptor& buff_desc,
                                 const glsl::VertFragProgramSource& source,
                                 const vk::PipelineRenderPassInfo& pass_info,
                                 VkPipelineLayout layout,
                                 VkPrimitiveTopology prim_topology) {
  VertexInputDescriptors input_descrs{};
  to_vk_vertex_input_descriptors(1u, &buff_desc, &input_descrs);
  DefaultConfigureGraphicsPipelineStateParams params{input_descrs};
  params.num_color_attachments = 1;
  params.raster_samples = pass_info.raster_samples;
  params.topology = prim_topology;
  GraphicsPipelineStateCreateInfo state{};
  default_configure(&state, params);
  return create_vert_frag_graphics_pipeline(
    device, source.vert_bytecode, source.frag_bytecode,
    &state, layout, pass_info.render_pass, pass_info.subpass);
}

Result<Pipeline> create_point_pipeline(VkDevice device,
                                       const glsl::VertFragProgramSource& source,
                                       const vk::PipelineRenderPassInfo& pass_info,
                                       VkPipelineLayout layout) {
  auto buff_desc = point_vertex_buffer_descriptor();
  return create_pipeline(
    device, buff_desc, source, pass_info, layout, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
}

Result<Pipeline> create_line_pipeline(VkDevice device,
                                      const glsl::VertFragProgramSource& source,
                                      const vk::PipelineRenderPassInfo& pass_info,
                                      VkPipelineLayout layout) {
  auto buff_desc = line_vertex_buffer_descriptor();
  return create_pipeline(
    device, buff_desc, source, pass_info, layout, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
}

void do_copy_buffer(PointBufferRenderer::DrawableType type,
                    const void* data,
                    const VertexBufferDescriptor& desc,
                    int pos_attr,
                    int color_attr,
                    void* dst,
                    uint32_t num_verts) {
  int src_attrs[2] = {0, 0};
  int dst_attrs[2] = {0, 0};

  int num_attrs{};
  if (pos_attr >= 0) {
    src_attrs[num_attrs] = pos_attr;
    dst_attrs[num_attrs] = 0;
    num_attrs++;
  }
  if (color_attr >= 0) {
    assert(is_line_type(type));
    src_attrs[num_attrs] = color_attr;
    dst_attrs[num_attrs] = 1;
    num_attrs++;
  }

  const auto dst_desc = is_line_type(type) ?
    line_vertex_buffer_descriptor() :
    point_vertex_buffer_descriptor();
  if (!copy_buffer(data, desc, src_attrs, dst, dst_desc, dst_attrs, num_attrs, num_verts)) {
    GROVE_ASSERT(false);
    return;
  }
}

} //  anon

bool vk::PointBufferRenderer::is_valid() const {
  return initialized;
}

bool vk::PointBufferRenderer::initialize(const InitInfo& info) {
  if (!initialize_point_program(info)) {
    return false;
  }
  if (!initialize_line_program(info)) {
    return false;
  }
  initialized = true;
  return true;
}

bool vk::PointBufferRenderer::initialize_line_program(const InitInfo& info) {
  auto prog_source = create_line_program_source();
  if (!prog_source) {
    return false;
  }
  if (!info.pipeline_system.require_layouts(
    info.core.device.handle,
    make_view(prog_source.value().push_constant_ranges),
    make_view(prog_source.value().descriptor_set_layout_bindings),
    &line_pipeline.pipeline_layout,
    &line_pipeline.desc_set_layouts)) {
    return false;
  }
  auto pipe_res = create_line_pipeline(
    info.core.device.handle,
    prog_source.value(),
    info.forward_pass_info,
    line_pipeline.pipeline_layout);
  if (!pipe_res) {
    return false;
  } else {
    line_pipeline.pipeline = info.pipeline_system.emplace(std::move(pipe_res.value));
  }
  return true;
}

bool vk::PointBufferRenderer::initialize_point_program(const InitInfo& info) {
  auto prog_source = create_point_program_source();
  if (!prog_source) {
    return false;
  }
  if (!info.pipeline_system.require_layouts(
    info.core.device.handle,
    make_view(prog_source.value().push_constant_ranges),
    make_view(prog_source.value().descriptor_set_layout_bindings),
    &point_pipeline.pipeline_layout,
    &point_pipeline.desc_set_layouts)) {
    return false;
  }
  auto pipe_res = create_point_pipeline(
    info.core.device.handle,
    prog_source.value(),
    info.forward_pass_info,
    point_pipeline.pipeline_layout);
  if (!pipe_res) {
    return false;
  } else {
    point_pipeline.pipeline = info.pipeline_system.emplace(std::move(pipe_res.value));
  }
  return true;
}

void vk::PointBufferRenderer::begin_frame(uint32_t frame_index) {
  update_buffers(frame_index);
}

void vk::PointBufferRenderer::render(const RenderInfo& info) {
  if (!active_drawables.empty()) {
    render_points(info);
    render_lines(info);
  }
}

void vk::PointBufferRenderer::render_points(const RenderInfo& info) {
  auto pc_data = make_point_push_constant_data(info.camera, {});
  cmd::bind_graphics_pipeline(info.cmd, point_pipeline.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);

  for (auto& handle : active_drawables) {
    auto& drawable = drawables.at(handle.id);
    if (drawable.type != DrawableType::Points || drawable.num_vertices_active == 0) {
      continue;
    }
    const VkBuffer vert_buffs[1] = {
      drawable.vertex_buffer.get().contents().buffer.handle
    };
    const VkDeviceSize vb_offs[1] = {
      sizeof(PointVertex) * info.frame_index * drawable.num_vertices_reserved  //  @note
    };

    set_drawable_params(pc_data, drawable.params);
    cmd::push_constants(
      info.cmd, point_pipeline.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, &pc_data);
    vkCmdBindVertexBuffers(info.cmd, 0, 1, vert_buffs, vb_offs);
    DrawDescriptor draw_desc;
    draw_desc.num_instances = 1;
    draw_desc.num_vertices = drawable.num_vertices_active;
    cmd::draw(info.cmd, &draw_desc);
  }
}

void vk::PointBufferRenderer::render_lines(const RenderInfo& info) {
  auto pc_data = make_line_push_constant_data(info.camera, {});
  cmd::bind_graphics_pipeline(info.cmd, line_pipeline.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);

  for (auto& handle : active_drawables) {
    auto& drawable = drawables.at(handle.id);
    if (!is_line_type(drawable.type) || drawable.num_vertices_active == 0) {
      continue;
    }
    const VkBuffer vert_buffs[1] = {
      drawable.vertex_buffer.get().contents().buffer.handle
    };
    const VkDeviceSize vb_offs[1] = {
      sizeof(LineVertex) * info.frame_index * drawable.num_vertices_reserved  //  @note
    };

    cmd::push_constants(
      info.cmd, line_pipeline.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, &pc_data);
    vkCmdBindVertexBuffers(info.cmd, 0, 1, vert_buffs, vb_offs);
    DrawDescriptor draw_desc;
    draw_desc.num_instances = 1;
    draw_desc.num_vertices = drawable.num_vertices_active;
    cmd::draw(info.cmd, &draw_desc);
  }
}

DrawableHandle vk::PointBufferRenderer::create_drawable(DrawableType type,
                                                        const DrawableParams& params) {
  auto handle = DrawableHandle{next_drawable_id++};
  Drawable drawable{};
  drawable.params = params;
  drawable.type = type;
  drawable.vertex_size_bytes = is_line_type(type) ? sizeof(LineVertex) : sizeof(PointVertex);
  drawables[handle.id] = std::move(drawable);
  return handle;
}

void vk::PointBufferRenderer::destroy_drawable(DrawableHandle handle) {
  remove_active_drawable(handle);
  drawables.erase(handle.id);
}

void PointBufferRenderer::add_active_drawable(DrawableHandle handle) {
#ifdef GROVE_DEBUG
  auto it = std::find(active_drawables.begin(), active_drawables.end(), handle);
  GROVE_ASSERT(it == active_drawables.end());
#endif
  active_drawables.push_back(handle);
}

void PointBufferRenderer::require_active_drawable(DrawableHandle handle) {
  auto it = std::find(active_drawables.begin(), active_drawables.end(), handle);
  if (it == active_drawables.end()) {
    active_drawables.push_back(handle);
  }
}

void PointBufferRenderer::remove_active_drawable(DrawableHandle handle) {
  auto it = std::find(active_drawables.begin(), active_drawables.end(), handle);
  if (it != active_drawables.end()) {
    active_drawables.erase(it);
  } else {
    GROVE_ASSERT(false);
  }
}

void PointBufferRenderer::toggle_active_drawable(DrawableHandle handle) {
  if (auto it = std::find(active_drawables.begin(), active_drawables.end(), handle);
      it != active_drawables.end()) {
    remove_active_drawable(handle);
  } else {
    add_active_drawable(handle);
  }
}

void vk::PointBufferRenderer::update_buffers(uint32_t frame_index) {
  for (auto& handle : active_drawables) {
    auto& drawable = drawables.at(handle.id);
    if (drawable.vertex_buffer_needs_update[frame_index]) {
      const size_t off = drawable.vertex_size_bytes * drawable.num_vertices_reserved * frame_index;
      const size_t size = drawable.vertex_size_bytes * drawable.num_vertices_active;
      if (size > 0) {
        drawable.vertex_buffer.get().write(drawable.cpu_vertex_data.get(), size, off);
      }
      drawable.vertex_buffer_needs_update[frame_index] = false;
    }
  }
}

void vk::PointBufferRenderer::reserve_instances(const AddResourceContext& context,
                                                DrawableHandle handle,
                                                uint32_t num_verts) {
  auto it = drawables.find(handle.id);
  if (it == drawables.end()) {
    GROVE_ASSERT(false);
    return;
  }

  auto& drawable = it->second;
  if (num_verts != drawable.num_vertices_reserved) {
    const size_t cpu_buff_size = drawable.vertex_size_bytes * num_verts;
    if (cpu_buff_size > 0) {
      const size_t buff_size = cpu_buff_size * context.frame_queue_depth;
      auto buff = create_host_visible_vertex_buffer(context.allocator, buff_size);
      if (!buff) {
        return;
      } else {
        drawable.vertex_buffer = context.buffer_system.emplace(std::move(buff.value));
        drawable.cpu_vertex_data = std::make_unique<unsigned char[]>(cpu_buff_size);
      }
    } else {
      drawable.vertex_buffer = {};
      drawable.cpu_vertex_data = {};
    }
    drawable.num_vertices_reserved = num_verts;
    drawable.num_vertices_active = 0;
  }
}

void vk::PointBufferRenderer::update_instances(const AddResourceContext& context,
                                               DrawableHandle handle,
                                               const void* data,
                                               size_t size,
                                               const VertexBufferDescriptor& desc,
                                               int pos_attr,
                                               int color_attr) {
  auto it = drawables.find(handle.id);
  if (it == drawables.end()) {
    GROVE_ASSERT(false);
    return;
  }

  auto& drawable = it->second;
  const auto num_verts = uint32_t(desc.num_vertices(size));
  reserve_instances(context, handle, num_verts);
  drawable.num_vertices_active = num_verts;

  do_copy_buffer(
    drawable.type,
    data,
    desc,
    pos_attr,
    color_attr,
    drawable.cpu_vertex_data.get(),
    num_verts);

  for (uint32_t i = 0; i < context.frame_queue_depth; i++) {
    drawable.vertex_buffer_needs_update[i] = true;
  }
}

void vk::PointBufferRenderer::update_instances(const AddResourceContext& context,
                                               DrawableHandle handle,
                                               const Vec3f* positions,
                                               int num_points) {
  VertexBufferDescriptor desc;
  desc.add_attribute(AttributeDescriptor::float3(0));
  const size_t size = sizeof(float) * 3 * num_points;
  update_instances(context, handle, positions, size, desc, 0, -1);
}

void vk::PointBufferRenderer::clear_active_instances(DrawableHandle handle) {
  if (auto it = drawables.find(handle.id); it != drawables.end()) {
    it->second.num_vertices_active = 0;
  } else {
    GROVE_ASSERT(false);
  }
}

void vk::PointBufferRenderer::set_instances(const AddResourceContext& context,
                                            DrawableHandle handle,
                                            const void* data,
                                            size_t size,
                                            const VertexBufferDescriptor& desc,
                                            int pos_attr,
                                            int color_attr,
                                            int ith_element_offset) {
  auto it = drawables.find(handle.id);
  if (it == drawables.end()) {
    GROVE_ASSERT(false);
    return;
  }

  auto& drawable = it->second;
  auto num_verts = desc.num_vertices(size);
  if (num_verts + ith_element_offset > drawable.num_vertices_reserved) {
    GROVE_ASSERT(false);
    return;
  }

  auto* dst = drawable.cpu_vertex_data.get() + drawable.vertex_size_bytes * ith_element_offset;
  do_copy_buffer(
    drawable.type,
    data,
    desc,
    pos_attr,
    color_attr,
    dst,
    uint32_t(num_verts));

  for (uint32_t i = 0; i < context.frame_queue_depth; i++) {
    drawable.vertex_buffer_needs_update[i] = true;
  }

  drawable.num_vertices_active = std::max(
    drawable.num_vertices_active,
    uint32_t(ith_element_offset + num_verts));
}

void vk::PointBufferRenderer::set_instances(const AddResourceContext& context,
                                            DrawableHandle handle,
                                            const Vec3f* positions,
                                            int num_points,
                                            int offset) {
  VertexBufferDescriptor desc;
  desc.add_attribute(AttributeDescriptor::float3(0));
  const size_t size = sizeof(float) * 3 * num_points;
  set_instances(context, handle, positions, size, desc, 0, -1, offset);
}

void vk::PointBufferRenderer::set_instance_color_range(const AddResourceContext& context,
                                                       DrawableHandle handle,
                                                       const Vec3f* colors,
                                                       int num_colors,
                                                       int offset) {
  VertexBufferDescriptor desc;
  desc.add_attribute(AttributeDescriptor::float3(1));
  const size_t size = sizeof(float) * 3 * num_colors;
  set_instances(context, handle, colors, size, desc, -1, 0, offset);
}

void vk::PointBufferRenderer::set_point_color(DrawableHandle handle, const Vec3f& color) {
  if (auto it = drawables.find(handle.id); it != drawables.end()) {
    assert(!is_line_type(it->second.type));
    it->second.params.color = color;
  }
}

vk::PointBufferRenderer::AddResourceContext
vk::PointBufferRenderer::make_add_resource_context(vk::GraphicsContext& graphics_context) {
  return vk::PointBufferRenderer::AddResourceContext{
    graphics_context.core,
    &graphics_context.allocator,
    graphics_context.buffer_system,
    graphics_context.frame_queue_depth
  };
}

GROVE_NAMESPACE_END
