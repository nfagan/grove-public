#include "ArchRenderer.hpp"
#include "memory.hpp"
#include "graphics_context.hpp"
#include "csm.hpp"
#include "grove/common/common.hpp"
#include "grove/visual/Camera.hpp"
#include <array>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace vk;

using DrawableHandle = ArchRenderer::DrawableHandle;
using GeometryHandle = ArchRenderer::GeometryHandle;
using DrawableParams = ArchRenderer::DrawableParams;
using Geometry = ArchRenderer::Geometry;
using InitInfo = ArchRenderer::InitInfo;
using GetGeometryData = ArchRenderer::GetGeometryData;
using ReserveGeometryData = ArchRenderer::ReserveGeometryData;

struct Vertex {
  Vec3f position;
  Vec3f normal;
};

struct ShadowPushConstantData {
  Mat4f proj_view;
  Vec4f translation_scale;
};

struct ForwardPushConstantData {
  Vec4f translation_scale;
  Vec4f color;
};

struct ForwardUniformData {
  Mat4f view;
  Mat4f projection;
  Mat4f sun_light_view_projection0;
  Vec4f camera_position_randomized_color;
  Vec4f sun_position;
  Vec4f sun_color;
};

bool is_dynamic(ArchRenderer::DrawType type) {
  return type == ArchRenderer::DrawType::Dynamic;
}

ForwardUniformData make_forward_uniform_data(const Camera& camera,
                                             const Mat4f& shadow_proj0,
                                             bool randomized_color,
                                             const Vec3f& sun_pos,
                                             const Vec3f& sun_color) {
  ForwardUniformData result;
  result.view = camera.get_view();
  result.projection = camera.get_projection();
  result.projection[1] = -result.projection[1];
  result.sun_light_view_projection0 = shadow_proj0;
  result.camera_position_randomized_color = Vec4f{camera.get_position(), float(randomized_color)};
  result.sun_position = Vec4f{sun_pos, 1.0f};
  result.sun_color = Vec4f{sun_color, 1.0f};
  return result;
}

ForwardPushConstantData make_forward_push_constant_data(const Vec3f& trans,
                                                        float scale,
                                                        const Vec3f& color) {
  ForwardPushConstantData result;
  result.translation_scale = Vec4f{trans, scale};
  result.color = Vec4f{color, 0.0f};
  return result;
}

ShadowPushConstantData make_shadow_push_constant_data(const Mat4f& proj_view,
                                                      const Vec3f& trans,
                                                      float scale) {
  ShadowPushConstantData result;
  result.proj_view = proj_view;
  result.translation_scale = Vec4f{trans, scale};
  return result;
}

std::array<VertexBufferDescriptor, 1> vertex_buffer_descriptors() {
  std::array<VertexBufferDescriptor, 1> result;
  result[0].add_attribute(AttributeDescriptor::float3(0));
  result[0].add_attribute(AttributeDescriptor::float3(1));
  return result;
}

Optional<glsl::VertFragProgramSource> create_shadow_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "arch/experiment-shadow.vert";
  params.frag_file = "shadow/empty.frag";
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_forward_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "arch/experiment.vert";
  params.frag_file = "arch/experiment.frag";
  params.compile.frag_defines.push_back(
    csm::make_num_sun_shadow_cascades_preprocessor_definition());
  params.compile.frag_defines.push_back(
    csm::make_default_num_sun_shadow_samples_preprocessor_definition());
  params.reflect.to_vk_descriptor_type = refl::always_dynamic_uniform_buffer_descriptor_type;
  return glsl::make_vert_frag_program_source(params);
}

Result<Pipeline> create_pipeline(VkDevice device,
                                 const glsl::VertFragProgramSource& source,
                                 const vk::PipelineRenderPassInfo& pass_info,
                                 VkPipelineLayout layout,
                                 uint32_t num_color_attach,
                                 VkCullModeFlags cull_mode) {
  auto buff_descs = vertex_buffer_descriptors();
  VertexInputDescriptors input_descrs{};
  to_vk_vertex_input_descriptors(uint32_t(buff_descs.size()), buff_descs.data(), &input_descrs);
  DefaultConfigureGraphicsPipelineStateParams params{input_descrs};
  params.num_color_attachments = num_color_attach;
  params.raster_samples = pass_info.raster_samples;
  params.cull_mode = cull_mode;
  GraphicsPipelineStateCreateInfo state{};
  default_configure(&state, params);
  return create_vert_frag_graphics_pipeline(
    device, source.vert_bytecode, source.frag_bytecode,
    &state, layout, pass_info.render_pass, pass_info.subpass);
}

Result<Pipeline> create_forward_pipeline(VkDevice device,
                                         const glsl::VertFragProgramSource& source,
                                         const vk::PipelineRenderPassInfo& pass_info,
                                         VkPipelineLayout layout) {
//  auto cull_mode = VK_CULL_MODE_BACK_BIT;
  auto cull_mode = VK_CULL_MODE_NONE;
  return create_pipeline(device, source, pass_info, layout, 1, cull_mode);
}

Result<Pipeline> create_shadow_pipeline(VkDevice device,
                                        const glsl::VertFragProgramSource& source,
                                        const vk::PipelineRenderPassInfo& pass_info,
                                        VkPipelineLayout layout) {
//  auto cull_mode = VK_CULL_MODE_BACK_BIT;
  auto cull_mode = VK_CULL_MODE_NONE;
  return create_pipeline(device, source, pass_info, layout, 0, cull_mode);
}

void render_drawable(VkCommandBuffer cmd, const Geometry& geom, uint32_t frame_index) {
  const VkBuffer vert_buffs[1] = {
    geom.geometry_buffer.get().contents().buffer.handle
  };
  VkDeviceSize vb_offs[1] = {};
  VkDeviceSize ind_off{};
  if (is_dynamic(geom.draw_type)) {
    vb_offs[0] = geom.num_vertices * sizeof(Vertex) * frame_index;
    ind_off = geom.num_indices_allocated * sizeof(uint16_t) * frame_index;
  }

  vkCmdBindVertexBuffers(cmd, 0, 1, vert_buffs, vb_offs);
  VkBuffer ind_buff = geom.index_buffer.get().contents().buffer.handle;
  vkCmdBindIndexBuffer(cmd, ind_buff, ind_off, VK_INDEX_TYPE_UINT16);

  vk::DrawIndexedDescriptor draw_desc;
  draw_desc.num_indices = geom.num_indices_active;
  draw_desc.num_instances = 1;
  cmd::draw_indexed(cmd, &draw_desc);
}

bool make_shadow_program(ArchRenderer& renderer, const InitInfo& info,
                         glsl::VertFragProgramSource* out) {
  auto prog_source = create_shadow_program_source();
  if (!prog_source) {
    return false;
  }
  if (!info.pipeline_system.require_layouts(
    info.core.device.handle,
    make_view(prog_source.value().push_constant_ranges),
    make_view(prog_source.value().descriptor_set_layout_bindings),
    &renderer.shadow_pipeline.pipeline_layout,
    &renderer.shadow_pipeline.desc_set_layouts)) {
    return false;
  }
  auto pipe_res = create_shadow_pipeline(
    info.core.device.handle,
    prog_source.value(),
    info.shadow_pass_info,
    renderer.shadow_pipeline.pipeline_layout);
  if (!pipe_res) {
    return false;
  } else {
    renderer.shadow_pipeline.pipeline = info.pipeline_system.emplace(std::move(pipe_res.value));
  }
  if (out) {
    *out = std::move(prog_source.value());
  }
  return true;
}

bool make_forward_program(ArchRenderer& renderer, const InitInfo& info,
                          glsl::VertFragProgramSource* out) {
  auto prog_source = create_forward_program_source();
  if (!prog_source) {
    return false;
  }
  if (!info.pipeline_system.require_layouts(
    info.core.device.handle,
    make_view(prog_source.value().push_constant_ranges),
    make_view(prog_source.value().descriptor_set_layout_bindings),
    &renderer.forward_pipeline.pipeline_layout,
    &renderer.forward_pipeline.desc_set_layouts)) {
    return false;
  }
  auto pipe_res = create_forward_pipeline(
    info.core.device.handle,
    prog_source.value(),
    info.forward_pass_info,
    renderer.forward_pipeline.pipeline_layout);
  if (!pipe_res) {
    return false;
  } else {
    renderer.forward_pipeline.pipeline = info.pipeline_system.emplace(std::move(pipe_res.value));
  }
  if (out) {
    *out = std::move(prog_source.value());
  }
  return true;
}

GeometryHandle create_geometry(ArchRenderer& renderer, ArchRenderer::DrawType type,
                               GetGeometryData&& get_data, ReserveGeometryData&& reserve_data = nullptr) {
  GeometryHandle result{renderer.next_geometry_id++};
  renderer.geometries[result.id] = {};
  auto& res = renderer.geometries.at(result.id);
  res.draw_type = type;
  res.get_data = std::move(get_data);
  res.reserve_data = std::move(reserve_data);
  return result;
}

int num_active_drawables(const ArchRenderer& renderer) {
  int ct{};
  for (auto& [_, drawable] : renderer.drawables) {
    if (!drawable.inactive) {
      ct++;
    }
  }
  return ct;
}

ArchRenderer::AddResourceContext to_add_resource_context(const ArchRenderer::BeginFrameInfo& info) {
  return ArchRenderer::AddResourceContext{
    info.allocator,
    info.core,
    info.frame_queue_depth,
    info.buffer_system,
    info.staging_buffer_system,
    info.command_processor,
  };
}

} //  anon

void ArchRenderer::begin_frame(const BeginFrameInfo& info) {
  {
    auto forward_data = make_forward_uniform_data(
      info.camera,
      info.csm_descriptor.light_shadow_sample_view,
      render_params.randomized_color,
      render_params.sun_position,
      render_params.sun_color);
    auto off = forward_uniform_buffer_stride * info.frame_index;
    forward_uniform_buffer.get().write(&forward_data, sizeof(forward_data), off);
  }
  {
    auto sun_data = csm::make_sun_csm_sample_data(info.csm_descriptor);
    auto off = forward_shadow_uniform_buffer_stride * info.frame_index;
    forward_shadow_uniform_buffer.get().write(&sun_data, sizeof(sun_data), off);
  }

  for (auto& [id, geom] : geometries) {
    if (is_dynamic(geom.draw_type) && geom.modified) {
      for (uint32_t i = 0; i < info.frame_queue_depth; i++) {
        geom.buffers_need_update[i] = true;
      }

      if (geom.reserve_data) {
        size_t num_verts{};
        size_t num_inds{};
        geom.reserve_data(&num_verts, &num_inds);

        if (geom.num_indices_allocated < num_inds || geom.num_vertices < num_verts) {
          auto res_ctx = to_add_resource_context(info);
          VertexBufferDescriptor desc;
          desc.add_attribute(AttributeDescriptor::float3(0));
          desc.add_attribute(AttributeDescriptor::float3(0));
          (void) update_geometry(
            res_ctx, GeometryHandle{id},
            nullptr, num_verts * sizeof(Vertex),
            desc, 0, Optional<int>(1), nullptr, uint32_t(num_inds));
        }
      }

      geom.modified = false;
    }
  }

  for (auto& [_, geom] : geometries) {
    if (is_dynamic(geom.draw_type) &&
        geom.get_data &&
        geom.buffers_need_update[info.frame_index]) {
      size_t expect_geom_size = geom.num_vertices * sizeof(Vertex);
      size_t max_num_indices = geom.num_indices_allocated;
      size_t max_index_size = max_num_indices * sizeof(uint16_t);

      const void* geom_src{};
      size_t geom_size{};
      const void* ind_src{};
      size_t ind_size{};
      geom.get_data(&geom_src, &geom_size, &ind_src, &ind_size);

      size_t num_inds = ind_size / sizeof(uint16_t);
      assert(geom_size <= expect_geom_size &&
             ind_size <= max_index_size &&
             num_inds <= max_num_indices &&
             num_inds * sizeof(uint16_t) == ind_size);

      auto geom_offset = expect_geom_size * info.frame_index;
      geom.geometry_buffer.get().write(geom_src, geom_size, geom_offset);

      auto ind_offset = max_index_size * info.frame_index;
      geom.index_buffer.get().write(ind_src, ind_size, ind_offset);
      geom.num_indices_active = uint32_t(num_inds);

      geom.buffers_need_update[info.frame_index] = false;
    }
  }
}

bool ArchRenderer::is_valid() const {
  return initialized && initialized_programs;
}

void ArchRenderer::remake_programs(const InitInfo& info) {
  initialized_programs = false;
  glsl::VertFragProgramSource forward_prog_source;
  if (!make_forward_program(*this, info, nullptr)) {
    return;
  }
  if (!make_shadow_program(*this, info, nullptr)) {
    return;
  }
  initialized_programs = true;
}

bool ArchRenderer::initialize(const InitInfo& info) {
  glsl::VertFragProgramSource forward_prog_source;
  if (!make_forward_program(*this, info, &forward_prog_source)) {
    return false;
  }
  glsl::VertFragProgramSource shadow_prog_source;
  if (!make_shadow_program(*this, info, &shadow_prog_source)) {
    return false;
  }
  {
    auto get_size = [](ShaderResourceType) { return 4; };
    vk::DescriptorPoolAllocator::PoolSizes pool_sizes;
    push_pool_sizes_from_layout_bindings(
      pool_sizes,
      make_view(forward_prog_source.descriptor_set_layout_bindings),
      get_size);
    push_pool_sizes_from_layout_bindings(
      pool_sizes,
      make_view(shadow_prog_source.descriptor_set_layout_bindings),
      get_size);
    desc_pool_alloc = info.desc_system.create_pool_allocator(make_view(pool_sizes), 4);
    desc_set0_alloc = info.desc_system.create_set_allocator(desc_pool_alloc.get());
  }

  {
    std::size_t act_forward_buff_size{};
    auto forward_un_buff_res = create_dynamic_uniform_buffer<ForwardUniformData>(
      info.allocator,
      &info.core.physical_device.info.properties,
      info.frame_queue_depth,
      &forward_uniform_buffer_stride,
      &act_forward_buff_size);
    if (!forward_un_buff_res) {
      return false;
    } else {
      forward_uniform_buffer = info.buffer_system.emplace(std::move(forward_un_buff_res.value));
    }
    std::size_t act_forward_shadow_buff_size{};
    auto forward_shadow_un_buff_res = create_dynamic_uniform_buffer<csm::SunCSMSampleData>(
      info.allocator,
      &info.core.physical_device.info.properties,
      info.frame_queue_depth,
      &forward_shadow_uniform_buffer_stride,
      &act_forward_shadow_buff_size);
    if (!forward_shadow_un_buff_res) {
      return false;
    } else {
      forward_shadow_uniform_buffer = info.buffer_system.emplace(
        std::move(forward_shadow_un_buff_res.value));
    }
  }

  initialized = true;
  initialized_programs = true;
  return true;
}

void ArchRenderer::render(const RenderInfo& info) {
  if (num_active_drawables(*this) == 0 || hidden) {
    return;
  }

  vk::DescriptorPoolAllocator* pool_alloc;
  vk::DescriptorSetAllocator* set0_alloc;
  if (!info.desc_system.get(desc_pool_alloc.get(), &pool_alloc) ||
      !info.desc_system.get(desc_set0_alloc.get(), &set0_alloc)) {
    GROVE_ASSERT(false);
    return;
  }

  cmd::bind_graphics_pipeline(info.cmd, forward_pipeline.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);

  VkDescriptorSet desc_set0;
  {
    auto shadow_sampler = info.sampler_system.require_linear_edge_clamp(info.core.device.handle);
    vk::DescriptorSetScaffold scaffold;
    scaffold.set = 0;
    uint32_t bind{};
    push_dynamic_uniform_buffer(
      scaffold, bind++, forward_uniform_buffer.get(), sizeof(ForwardUniformData));
    push_dynamic_uniform_buffer(
      scaffold, bind++, forward_shadow_uniform_buffer.get(), sizeof(csm::SunCSMSampleData));
    push_combined_image_sampler(
      scaffold, bind++, info.shadow_image, shadow_sampler);
    if (auto err = set0_alloc->require_updated_descriptor_set(
      info.core.device.handle,
      *forward_pipeline.desc_set_layouts.find(0),
      *pool_alloc,
      scaffold,
      &desc_set0)) {
      //
      GROVE_ASSERT(false);
      return;
    }
  }

  const uint32_t dyn_offs[2] = {
    uint32_t(forward_uniform_buffer_stride * info.frame_index),
    uint32_t(forward_shadow_uniform_buffer_stride * info.frame_index)
  };
  cmd::bind_graphics_descriptor_sets(
    info.cmd, forward_pipeline.pipeline_layout, 0, 1, &desc_set0, 2, dyn_offs);

  for (auto& [_, drawable] : drawables) {
    if (drawable.inactive) {
      continue;
    }
    auto& geom = geometries.at(drawable.geometry.id);
    if (geom.is_valid && geom.num_indices_active > 0) {
      auto pc_data = make_forward_push_constant_data(
        drawable.params.translation, drawable.params.scale, drawable.params.color);
      cmd::push_constants(
        info.cmd, forward_pipeline.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, &pc_data);
      render_drawable(info.cmd, geom, info.frame_index);
    }
  }
}

void ArchRenderer::render_shadow(const ShadowRenderInfo& info) {
  if (num_active_drawables(*this) == 0 || hidden) {
    return;
  }

  auto& cmd = info.cmd_buffer;
  cmd::bind_graphics_pipeline(cmd, shadow_pipeline.pipeline.get().handle);
  cmd::set_viewport_and_scissor(cmd, &info.viewport, &info.scissor_rect);

  for (auto& [_, drawable] : drawables) {
    if (drawable.inactive) {
      continue;
    }
    auto& geom = geometries.at(drawable.geometry.id);
    if (geom.is_valid && geom.num_indices_active > 0) {
      auto pc_data = make_shadow_push_constant_data(
        info.view_proj, drawable.params.translation, drawable.params.scale);
      cmd::push_constants(
        cmd, shadow_pipeline.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, &pc_data);
      render_drawable(info.cmd_buffer, geom, info.frame_index);
    }
  }
}

void ArchRenderer::set_modified(GeometryHandle handle) {
  if (auto it = geometries.find(handle.id); it != geometries.end()) {
    auto& geom = it->second;
    assert(is_dynamic(geom.draw_type));
    geom.modified = true;
  } else {
    assert(false);
  }
}

bool ArchRenderer::update_geometry(const AddResourceContext& context,
                                   GeometryHandle handle, const void* data, size_t size,
                                   const VertexBufferDescriptor& desc,
                                   int pos_attr, const Optional<int>& norm_attr,
                                   const uint16_t* indices, uint32_t num_indices) {
  Geometry* target;
  if (auto it = geometries.find(handle.id); it != geometries.end()) {
    target = &it->second;
  } else {
    assert(false);
    return false;
  }
  target->geometry_buffer = {};
  target->index_buffer = {};
  target->is_valid = false;

  const uint32_t frame_queue_mult = is_dynamic(target->draw_type) ? context.frame_queue_depth : 1;
  const auto num_verts = uint32_t(desc.num_vertices(size));
  const size_t dst_size = sizeof(Vertex) * num_verts;

  ManagedBuffer geom_buffer;
  if (dst_size > 0) {
    auto full_size = dst_size * frame_queue_mult;
    auto geom_res = create_host_visible_vertex_buffer(context.allocator, full_size);
    if (!geom_res) {
      return false;
    } else {
      geom_buffer = std::move(geom_res.value);
    }
  }

  ManagedBuffer index_buffer;
  if (num_indices > 0) {
    if (is_dynamic(target->draw_type)) {
      auto inds_size = num_indices * sizeof(uint16_t);
      auto ind_res = create_host_visible_index_buffer(
        context.allocator, inds_size * frame_queue_mult);
      if (!ind_res) {
        return false;
      }
      if (indices) {
        for (uint32_t i = 0; i < frame_queue_mult; i++) {
          ind_res.value.write(indices, inds_size, inds_size * i);
        }
      }
      index_buffer = std::move(ind_res.value);

    } else {
      auto ind_res = create_device_local_index_buffer(
        context.allocator, num_indices * sizeof(uint16_t), true);
      if (!ind_res) {
        return false;
      }
      if (indices) {
        auto upload_ctx = make_upload_from_staging_buffer_context(
          &context.core,
          context.allocator,
          &context.staging_buffer_system,
          &context.command_processor);
        const void* src_data[1] = {indices};
        const ManagedBuffer* dst_buff[1] = {&ind_res.value};
        if (!upload_from_staging_buffer_sync(src_data, dst_buff, nullptr, 1, upload_ctx)) {
          return false;
        }
      }
      index_buffer = std::move(ind_res.value);
    }
  }

  if (data && dst_size > 0) {
    if (desc.count_attributes() == 2 &&
        pos_attr == 0 &&
        norm_attr &&
        norm_attr.value() == 1 &&
        size == dst_size) {
      for (uint32_t i = 0; i < frame_queue_mult; i++) {
        geom_buffer.write(data, dst_size, dst_size * i);
      }
    } else {
      auto tmp_data = std::make_unique<Vertex[]>(num_verts);
      auto* dst = tmp_data.get();
      auto dst_descs = vertex_buffer_descriptors();
      auto& dst_desc = dst_descs[0];
      int src_attrs[2] = {};
      int dst_attrs[2] = {};

      int ct{};
      src_attrs[ct] = pos_attr;
      dst_attrs[ct] = 0;
      ct++;
      if (norm_attr) {
        src_attrs[ct] = norm_attr.value();
        dst_attrs[ct] = 1;
        ct++;
      }

      if (!copy_buffer(data, desc, src_attrs, dst, dst_desc, dst_attrs, ct, num_verts)) {
        GROVE_ASSERT(false);
        return false;
      }
      for (uint32_t i = 0; i < frame_queue_mult; i++) {
        geom_buffer.write(dst, dst_size, dst_size * i);
      }
    }
  }

  target->index_buffer = context.buffer_system.emplace(std::move(index_buffer));
  target->geometry_buffer = context.buffer_system.emplace(std::move(geom_buffer));
  target->is_valid = true;
  target->num_indices_allocated = num_indices;
  target->num_indices_active = num_indices;
  target->num_vertices = num_verts;
  return true;
}

DrawableParams* ArchRenderer::get_params(DrawableHandle handle) {
  if (auto it = drawables.find(handle.id); it != drawables.end()) {
    return &it->second.params;
  } else {
    return nullptr;
  }
}

void ArchRenderer::toggle_active(DrawableHandle handle) {
  set_active(handle, !is_active(handle));
}

void ArchRenderer::set_active(DrawableHandle handle, bool active) {
  if (auto it = drawables.find(handle.id); it != drawables.end()) {
    it->second.inactive = !active;
  } else {
    assert(false);
  }
}

bool ArchRenderer::is_active(DrawableHandle handle) const {
  if (auto it = drawables.find(handle.id); it != drawables.end()) {
    return !it->second.inactive;
  } else {
    assert(false);
    return false;
  }
}

DrawableHandle ArchRenderer::create_drawable(GeometryHandle geom, const DrawableParams& params) {
  DrawableHandle result{next_drawable_id++};
  drawables[result.id] = {};
  auto& res = drawables.at(result.id);
  res.params = params;
  res.geometry = geom;
  return result;
}

GeometryHandle ArchRenderer::create_static_geometry() {
  return create_geometry(*this, DrawType::Static, nullptr);
}

GeometryHandle ArchRenderer::create_dynamic_geometry(GetGeometryData&& get_data,
                                                     ReserveGeometryData&& reserve_data) {
  return create_geometry(*this, DrawType::Dynamic, std::move(get_data), std::move(reserve_data));
}

void ArchRenderer::destroy_drawable(DrawableHandle handle) {
  drawables.erase(handle.id);
}

ArchRenderer::AddResourceContext
ArchRenderer::make_add_resource_context(vk::GraphicsContext& graphics_context) {
  return ArchRenderer::AddResourceContext{
    &graphics_context.allocator,
    graphics_context.core,
    graphics_context.frame_queue_depth,
    graphics_context.buffer_system,
    graphics_context.staging_buffer_system,
    graphics_context.command_processor
  };
}

GROVE_NAMESPACE_END
