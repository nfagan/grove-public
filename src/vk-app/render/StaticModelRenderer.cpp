#include "StaticModelRenderer.hpp"
#include "../vk/vk.hpp"
#include "./graphics_context.hpp"
#include "shadow.hpp"
#include "memory.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/scope.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/common/fs.hpp"
#include <functional>
#include <iostream>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace vk;

/*
 * vertex buffer requirements
 * material requirements
 * check inverted projection y
 * check front face
 */

using GeometryHandle = StaticModelRenderer::GeometryHandle;
using MaterialHandle = StaticModelRenderer::MaterialHandle;
using DrawableHandle = StaticModelRenderer::DrawableHandle;
using AddResourceContext = StaticModelRenderer::AddResourceContext;

[[maybe_unused]] constexpr const char* logging_id() {
  return "StaticModelRenderer";
}

struct Config {
  static constexpr int num_model_uniforms_per_buffer = 16;
  static constexpr int model_uniform_pool_size = 16;
};

struct UniformModelData {
  Mat4f projection;
  Mat4f view;
  Mat4f model;
  Mat4f sun_light_view_projection0;
  Vec4f camera_position;
  Vec4f sun_color;
  Vec4f sun_position;
};

struct ShadowUniformModelData {
  Mat4f transform;
};

static_assert(alignof(UniformModelData) == 4);
static_assert(alignof(ShadowUniformModelData) == 4);

struct Vertex {
  static VertexBufferDescriptor buffer_descriptor() {
    VertexBufferDescriptor result;
    result.add_attribute(AttributeDescriptor::float3(0));
    result.add_attribute(AttributeDescriptor::float3(1));
    result.add_attribute(AttributeDescriptor::float2(2));
    return result;
  }

  Vec3f position;
  Vec3f normal;
  Vec2f uv;
};

Mat4f negate_y(Mat4f proj) {
  proj[1] = -proj[1];
  return proj;
}

UniformModelData make_uniform_model_data(const Camera& camera,
                                         const Mat4f& sun_light_view_projection0,
                                         const Mat4f& model,
                                         const Vec3f& sun_pos,
                                         const Vec3f& sun_color) {
  UniformModelData result{};
  result.projection = negate_y(camera.get_projection());
  result.view = camera.get_view();
  result.model = model;
  result.sun_light_view_projection0 = sun_light_view_projection0;
  result.camera_position = Vec4f{camera.get_position(), 0.0f};
  result.sun_color = Vec4f{sun_color, 0.0f};
  result.sun_position = Vec4f{sun_pos, 0.0f};
  return result;
}

ShadowUniformModelData make_shadow_uniform_model_data(const Mat4f& view_proj, const Mat4f& model) {
  ShadowUniformModelData result{};
  result.transform = view_proj * model;
  return result;
}

Optional<glsl::VertFragProgramSource> create_shadow_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "static-model/simple-model-shadow.vert";
  params.frag_file = "static-model/simple-model-shadow.frag";
  params.reflect.to_vk_descriptor_type = [](const glsl::refl::DescriptorInfo& info) {
    return info.is_uniform_buffer() ?
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : vk::refl::identity_descriptor_type(info);
  };
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_forward_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "static-model/simple-model.vert";
  params.frag_file = "static-model/simple-model.frag";
  params.compile.frag_defines.push_back(csm::make_num_sun_shadow_cascades_preprocessor_definition());
  params.reflect.to_vk_descriptor_type = [](const glsl::refl::DescriptorInfo& info) {
    if (info.is_uniform_buffer() && info.set == 1 && info.binding == 0) {
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    } else {
      return vk::refl::to_vk_descriptor_type(info.type);
    }
  };
  return glsl::make_vert_frag_program_source(params);
}

Result<Pipeline> create_common_pipeline(const vk::Device& device,
                                        const glsl::VertFragProgramSource& source,
                                        const PipelineRenderPassInfo& pass_info,
                                        VkPipelineLayout layout,
                                        uint32_t num_color_attachments) {
  auto buff_descr = Vertex::buffer_descriptor();
  VertexInputDescriptors input_descrs{};
  to_vk_vertex_input_descriptors(1, &buff_descr, &input_descrs);
  DefaultConfigureGraphicsPipelineStateParams params{input_descrs};
  params.num_color_attachments = num_color_attachments;
  params.raster_samples = pass_info.raster_samples;
  GraphicsPipelineStateCreateInfo state{};
  default_configure(&state, params);
  return create_vert_frag_graphics_pipeline(
    device.handle, source.vert_bytecode, source.frag_bytecode,
    &state, layout, pass_info.render_pass, pass_info.subpass);
}

Result<Pipeline> create_shadow_pipeline(const vk::Device& device,
                                        const glsl::VertFragProgramSource& source,
                                        const PipelineRenderPassInfo& pass_info,
                                        VkPipelineLayout layout) {
  return create_common_pipeline(device, source, pass_info, layout, 0);
}

Result<Pipeline> create_forward_pipeline(const vk::Device& device,
                                         const glsl::VertFragProgramSource& source,
                                         const PipelineRenderPassInfo& pass_info,
                                         VkPipelineLayout layout) {
  return create_common_pipeline(device, source, pass_info, layout, 1);
}

Optional<StaticModelRenderer::Geometry> create_geometry(const AddResourceContext& context,
                                                        const void* data,
                                                        const VertexBufferDescriptor& src_desc,
                                                        std::size_t size,
                                                        int pos_ind,
                                                        int norm_ind,
                                                        int uv_ind) {
  const auto num_verts = uint32_t(src_desc.num_vertices(size));
  std::vector<Vertex> vertices(num_verts);
  auto dst_desc = Vertex::buffer_descriptor();
  const int src_inds[3] = {pos_ind, norm_ind, uv_ind};
  if (!copy_buffer(data, src_desc, src_inds, vertices.data(), dst_desc, 3, num_verts)) {
    GROVE_LOG_ERROR_CAPTURE_META("Invalid vertex format.", logging_id());
    return NullOpt{};
  }

  const auto buff_size = sizeof(Vertex) * num_verts;
  auto buff_res = create_device_local_vertex_buffer_sync(
    context.allocator, buff_size, vertices.data(), &context.core, &context.uploader);
  if (!buff_res) {
      GROVE_ASSERT(false);
    return NullOpt{};
  }
  auto buff = std::move(buff_res.value);

  StaticModelRenderer::Geometry geom{};
  geom.buffer = context.buffer_system.emplace(std::move(buff));
  geom.draw_descriptor.num_vertices = uint32_t(num_verts);
  geom.draw_descriptor.num_instances = 1;
  return Optional<StaticModelRenderer::Geometry>(std::move(geom));
}

} //  anon

bool StaticModelRenderer::is_valid() const {
  return initialized && initialized_programs;
}

bool StaticModelRenderer::initialize(const InitInfo& init_info) {
  glsl::VertFragProgramSource forward_pipe_source;
  if (!initialize_forward_pipeline(init_info, &forward_pipe_source)) {
    return false;
  }

  glsl::VertFragProgramSource shadow_pipe_source;
  if (!initialize_shadow_pipeline(init_info, &shadow_pipe_source)) {
    return false;
  }

  {
    auto& desc_system = init_info.desc_system;
    DescriptorPoolAllocator::PoolSizes pool_sizes;
    auto get_pool_size = [](ShaderResourceType) { return 4; };
    push_pool_sizes_from_layout_bindings(
      pool_sizes, make_view(forward_pipe_source.descriptor_set_layout_bindings), get_pool_size);
    push_pool_sizes_from_layout_bindings(
      pool_sizes, make_view(shadow_pipe_source.descriptor_set_layout_bindings), get_pool_size);
    desc_pool_allocator = desc_system.create_pool_allocator(make_view(pool_sizes), 8);
    forward_set0_allocator = desc_system.create_set_allocator(desc_pool_allocator.get());
    forward_set1_allocator = desc_system.create_set_allocator(desc_pool_allocator.get());
    shadow_set0_allocator = desc_system.create_set_allocator(desc_pool_allocator.get());
  }

  {
    const auto& ctx = init_info.core;
    auto min_align = ctx.physical_device.info.min_uniform_buffer_offset_alignment();
    auto forward_stride = aligned_element_size_check_zero(sizeof(UniformModelData), min_align);
    auto forward_size = forward_stride * Config::num_model_uniforms_per_buffer;
    auto shadow_stride = aligned_element_size_check_zero(sizeof(ShadowUniformModelData), min_align);
    auto shadow_size_per_cascade = shadow_stride * Config::num_model_uniforms_per_buffer;
    auto shadow_size = shadow_size_per_cascade * GROVE_NUM_SUN_SHADOW_CASCADES;

    uniform_buffer_info.forward_stride = forward_stride;
    uniform_buffer_info.forward_size = forward_size;
    uniform_buffer_info.shadow_stride = shadow_stride;
    uniform_buffer_info.shadow_size = shadow_size;
    uniform_buffer_info.shadow_size_per_cascade = shadow_size_per_cascade;
    uniform_buffer_info.align = min_align;
  }

  for (uint32_t i = 0; i < init_info.frame_queue_depth; i++) {
    drawable_uniform_buffers.emplace_back();
    if (auto buff = create_uniform_buffer(init_info.allocator, sizeof(csm::SunCSMSampleData))) {
      forward_shadow_data_uniform_buffers.push_back(std::move(buff.value));
    } else {
      GROVE_LOG_ERROR_CAPTURE_META("Failed to create shadow uniform buffer", logging_id());
      return false;
    }
  }

  initialized = true;
  initialized_programs = true;
  return true;
}

bool StaticModelRenderer::remake_programs(const InitInfo& info) {
  initialized_programs = false;
  if (!initialize_forward_pipeline(info, nullptr)) {
    return false;
  }
  if (!initialize_shadow_pipeline(info, nullptr)) {
    return false;
  }
  initialized_programs = true;
  return true;
}

bool StaticModelRenderer::initialize_forward_pipeline(const InitInfo& info,
                                                      glsl::VertFragProgramSource* prog_source) {
  auto forward_pipe_source = create_forward_program_source();
  if (!forward_pipe_source) {
    return false;
  }
  if (!info.pipeline_system.require_layouts(
    info.core.device.handle,
    make_view(forward_pipe_source.value().push_constant_ranges),
    make_view(forward_pipe_source.value().descriptor_set_layout_bindings),
    &forward_pipeline_layout,
    &forward_layouts)) {
    return false;
  }

  auto pipe_res = create_forward_pipeline(
    info.core.device,
    forward_pipe_source.value(),
    info.forward_pass_info,
    forward_pipeline_layout);
  if (!pipe_res) {
    return false;
  } else {
    forward_pipeline = info.pipeline_system.emplace(std::move(pipe_res.value));
  }
  if (prog_source) {
    *prog_source = std::move(forward_pipe_source.value());
  }
  return true;
}

bool StaticModelRenderer::initialize_shadow_pipeline(const InitInfo& info,
                                                     glsl::VertFragProgramSource* prog_source) {
  auto shadow_pipe_source = create_shadow_program_source();
  if (!shadow_pipe_source) {
    return false;
  }
  if (!info.pipeline_system.require_layouts(
    info.core.device.handle,
    make_view(shadow_pipe_source.value().push_constant_ranges),
    make_view(shadow_pipe_source.value().descriptor_set_layout_bindings),
    &shadow_pipeline_layout,
    &shadow_layouts)) {
    return false;
  }
  auto pipe_res = create_shadow_pipeline(
    info.core.device,
    shadow_pipe_source.value(),
    info.shadow_pass_info,
    shadow_pipeline_layout);
  if (!pipe_res) {
    return false;
  } else {
    shadow_pipeline = info.pipeline_system.emplace(std::move(pipe_res.value));
  }
  if (prog_source) {
    *prog_source = std::move(shadow_pipe_source.value());
  }
  return true;
}

void StaticModelRenderer::destroy(const vk::Device&) {
  geometries.clear();
  materials.clear();
  drawable_uniform_buffers.clear();
  drawable_uniform_buffer_free_list.clear();
  forward_shadow_data_uniform_buffers.clear();
}

void StaticModelRenderer::set_params(DrawableHandle handle, const DrawableParams& params) {
  if (auto it = drawables.find(handle); it != drawables.end()) {
    it->second.params = params;
  }
}

void StaticModelRenderer::begin_frame(const BeginFrameInfo& info) {
  update_forward_buffers(info);
}

void StaticModelRenderer::update_forward_buffers(const BeginFrameInfo& info) {
  {
    //  Shadow sample
    auto& forward_shadow_un_buff = forward_shadow_data_uniform_buffers[info.frame_index];
    auto un_data = csm::make_sun_csm_sample_data(info.csm_desc);
    forward_shadow_un_buff.write(&un_data, sizeof(csm::SunCSMSampleData));
  }

  auto& forward_un_buffs = drawable_uniform_buffers[info.frame_index];
  for (auto& [handle, drawable] : drawables) {
    auto& buff_cpu = uniform_cpu_data[drawable.buffer_index].forward_cpu_data;
    auto forward_off = uniform_buffer_info.forward_stride * drawable.buffer_element;
    auto un_data = make_uniform_model_data(
      info.camera,
      info.csm_desc.light_shadow_sample_view,
      drawable.params.transform,
      render_params.sun_position,
      render_params.sun_color);
    auto cpu_ptr = static_cast<unsigned char*>(buff_cpu.data) + forward_off;
    memcpy(cpu_ptr, &un_data, sizeof(un_data));
  }

  int buff_ind{};
  for (auto& buff : forward_un_buffs.buffers) {
    auto& buff_gpu = buff.forward_gpu_data;
    auto& buff_cpu = uniform_cpu_data[buff_ind++].forward_cpu_data;
    int count = buff.count;
    if (count > 0) {
      buff_gpu.write(buff_cpu.data, count * uniform_buffer_info.forward_stride);
    }
  }
}

void StaticModelRenderer::render(const RenderInfo& info) {
  const auto& forward_un_buffs = drawable_uniform_buffers[info.frame_index];
  const auto& forward_shadow_un_buff = forward_shadow_data_uniform_buffers[info.frame_index];

  const auto& device = info.core.device;
  auto& desc_system = info.desc_system;
  auto sampler = info.sampler_system.require_linear_edge_clamp(device.handle);

  DescriptorPoolAllocator* pool_alloc{};
  DescriptorSetAllocator* set0_alloc{};
  DescriptorSetAllocator* set1_alloc{};
  if (!desc_system.get(desc_pool_allocator.get(), &pool_alloc) ||
      !desc_system.get(forward_set0_allocator.get(), &set0_alloc) ||
      !desc_system.get(forward_set1_allocator.get(), &set1_alloc)) {
    return;
  }

  VkDescriptorSet descr_set0;
  {
    DescriptorSetScaffold scaffold{};
    scaffold.set = 0;
    push_uniform_buffer(scaffold, 0, forward_shadow_un_buff);
    push_combined_image_sampler(scaffold, 1, info.shadow_image, sampler);

    auto descr_res = set0_alloc->require_updated_descriptor_set(
      device.handle, *forward_layouts.find(0), *pool_alloc, scaffold);
    if (!descr_res) {
      return;
    } else {
      descr_set0 = descr_res.value;
    }
  }

  auto& cmd = info.cmd_buffer;
  cmd::bind_graphics_pipeline(cmd, forward_pipeline.get().handle);
  cmd::set_viewport_and_scissor(cmd, &info.viewport, &info.scissor_rect);
  cmd::bind_graphics_descriptor_sets(cmd, forward_pipeline_layout, 0, 1, &descr_set0);

  for (auto& [handle, drawable] : drawables) {
    auto& geom = geometries.at(drawable.geometry);
    auto& mat = materials.at(drawable.material);
    auto& un_buff = forward_un_buffs.buffers[drawable.buffer_index].forward_gpu_data;

    VkImageView texture_view;
    VkImageLayout texture_layout;
    if (auto im = info.sampled_image_manager.get(mat.image_handle)) {
      texture_view = im.value().view;
      texture_layout = im.value().layout;
    } else {
      GROVE_ASSERT(false);
      continue;
    }

    vk::DescriptorSetScaffold scaffold{};
    scaffold.set = 1;
    push_dynamic_uniform_buffer(scaffold, 0, un_buff, sizeof(UniformModelData));
    push_combined_image_sampler(scaffold, 1, texture_view, sampler, texture_layout);

    auto descr_set1 = set1_alloc->require_updated_descriptor_set(
      device.handle, *forward_layouts.find(1), *pool_alloc, scaffold);
    if (!descr_set1) {
      break;
    }

    auto forward_dyn_offset = uint32_t(uniform_buffer_info.forward_stride * drawable.buffer_element);
    uint32_t forward_dyn_offset_count = 1;

    VkDeviceSize vb_offset{};
    VkBuffer vb_buffer = geom.buffer.get().contents().buffer.handle;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb_buffer, &vb_offset);
    cmd::bind_graphics_descriptor_sets(
      cmd, forward_pipeline_layout, 1, 1, &descr_set1.value,
      forward_dyn_offset_count, &forward_dyn_offset);
    cmd::draw(cmd, &geom.draw_descriptor);
  }
}

void StaticModelRenderer::render_shadow(const ShadowRenderInfo& render_info) {
  auto& un_buffs = drawable_uniform_buffers[render_info.frame_index];
  const auto cascade_offset = uint32_t(
    render_info.cascade_index * uniform_buffer_info.shadow_size_per_cascade);

  for (auto& [handle, drawable] : drawables) {
    auto& buff_cpu = uniform_cpu_data[drawable.buffer_index].shadow_cpu_data;
    auto un_data = make_shadow_uniform_model_data(
      render_info.view_proj, drawable.params.transform);
    auto off = cascade_offset + uniform_buffer_info.shadow_stride * drawable.buffer_element;
    auto cpu_ptr = static_cast<unsigned char*>(buff_cpu.data) + off;
    memcpy(cpu_ptr, &un_data, sizeof(un_data));
  }

  int buff_ind{};
  for (auto& buff : un_buffs.buffers) {
    auto& buff_gpu = buff.shadow_gpu_data;
    auto& buff_cpu = uniform_cpu_data[buff_ind++].shadow_cpu_data;
    int count = buff.count;
    if (count > 0) {
      auto* cpu_read = static_cast<unsigned char*>(buff_cpu.data) + cascade_offset;
      buff_gpu.write(cpu_read, count * uniform_buffer_info.shadow_stride, cascade_offset);
    }
  }

  DescriptorPoolAllocator* pool_alloc{};
  DescriptorSetAllocator* set0_alloc{};
  if (!render_info.desc_system.get(desc_pool_allocator.get(), &pool_alloc) ||
      !render_info.desc_system.get(shadow_set0_allocator.get(), &set0_alloc)) {
    return;
  }

  auto& cmd = render_info.cmd_buffer;
  cmd::bind_graphics_pipeline(cmd, shadow_pipeline.get().handle);
  cmd::set_viewport_and_scissor(cmd, &render_info.viewport, &render_info.scissor_rect);

  for (auto& [handle, drawable] : drawables) {
    auto& geom = geometries.at(drawable.geometry);
    auto& shadow_buffer = un_buffs.buffers[drawable.buffer_index];
    auto& shadow_un_buff = shadow_buffer.shadow_gpu_data;

    vk::DescriptorSetScaffold scaffold;
    scaffold.set = 0;
    push_dynamic_uniform_buffer(scaffold, 0, shadow_un_buff, sizeof(ShadowUniformModelData));

    auto descr_set = set0_alloc->require_updated_descriptor_set(
      render_info.device.handle, *shadow_layouts.find(0), *pool_alloc, scaffold);
    if (!descr_set) {
      break;
    }

    VkDeviceSize vb_offset{};
    VkBuffer vb_buffer = geom.buffer.get().contents().buffer.handle;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb_buffer, &vb_offset);

    auto dyn_off = cascade_offset +
      uint32_t(uniform_buffer_info.shadow_stride * drawable.buffer_element);
    uint32_t dyn_off_count = 1;

    cmd::bind_graphics_descriptor_sets(
      cmd,
      shadow_pipeline_layout,
      0,
      1,
      &descr_set.value,
      dyn_off_count,
      &dyn_off);
    cmd::draw(cmd, &geom.draw_descriptor);
  }
}

Optional<GeometryHandle> StaticModelRenderer::add_geometry(const AddResourceContext& context,
                                                           const void* data,
                                                           const VertexBufferDescriptor& src_desc,
                                                           std::size_t size,
                                                           int pos_ind,
                                                           int norm_ind,
                                                           int uv_ind) {
  if (auto geom = create_geometry(context, data, src_desc, size, pos_ind, norm_ind, uv_ind)) {
    GeometryHandle handle{next_geometry_id++};
    geometries[handle] = std::move(geom.value());
    return Optional<GeometryHandle>(handle);
  } else {
    return NullOpt{};
  }
}

bool StaticModelRenderer::require_geometry(const AddResourceContext& context, const void* data,
                                           const VertexBufferDescriptor& desc,
                                           std::size_t size, int pos_ind, int norm_ind,
                                           int uv_ind, GeometryHandle* handle) {
  if (handle->is_valid()) {
    if (auto it = geometries.find(*handle); it != geometries.end()) {
      if (auto geom = create_geometry(context, data, desc, size, pos_ind, norm_ind, uv_ind)) {
        it->second = std::move(geom.value());
        return true;
      }
    }
  } else {
    if (auto dst_handle = add_geometry(context, data, desc, size, pos_ind, norm_ind, uv_ind)) {
      *handle = dst_handle.value();
      return true;
    }
  }
  return false;
}

Optional<DrawableHandle>
StaticModelRenderer::add_drawable(const AddResourceContext& context,
                                  GeometryHandle geometry,
                                  MaterialHandle material,
                                  const DrawableParams& params) {
  if (drawable_uniform_buffer_free_list.empty()) {
    if (drawable_uniform_buffers.empty()) {
      return NullOpt{};
    }
    for (int i = 0; i < Config::model_uniform_pool_size; i++) {
      int free_idx{};
      const auto& buff_info = uniform_buffer_info;
      UniformCPUData cpu_data{};
      cpu_data.forward_cpu_data = make_aligned_if_non_zero(buff_info.forward_size, buff_info.align);
      cpu_data.shadow_cpu_data = make_aligned_if_non_zero(buff_info.shadow_size, buff_info.align);
      uniform_cpu_data.push_back(std::move(cpu_data));

      for (auto& buffs : drawable_uniform_buffers) {
        auto forward_buff = create_uniform_buffer(context.allocator, buff_info.forward_size);
        auto shadow_buff = create_uniform_buffer(context.allocator, buff_info.shadow_size);
        if (!forward_buff || !shadow_buff) {
          return NullOpt{};
        }

        free_idx = int(buffs.buffers.size());
        PerDrawableUniformBuffers buffers{};
        buffers.forward_gpu_data = std::move(forward_buff.value);
        buffers.shadow_gpu_data = std::move(shadow_buff.value);
        buffs.buffers.push_back(std::move(buffers));
      }
      drawable_uniform_buffer_free_list.push_back(free_idx);
    }
  }

  auto buffer_index = drawable_uniform_buffer_free_list.back();
  int buffer_element{};
  bool need_pop_free{};
  for (auto& buff : drawable_uniform_buffers) {
    auto& ct = buff.buffers[buffer_index].count;
    buffer_element = ct++;
    if (ct == Config::num_model_uniforms_per_buffer) {
      need_pop_free = true;
    }
  }
  if (need_pop_free) {
    drawable_uniform_buffer_free_list.pop_back();
  }

  Drawable drawable{};
  drawable.geometry = geometry;
  drawable.material = material;
  drawable.buffer_index = buffer_index;
  drawable.buffer_element = buffer_element;
  drawable.params = params;

  DrawableHandle handle{next_drawable_id++};
  drawables[handle] = drawable;
  return Optional<DrawableHandle>(handle);
}

Optional<MaterialHandle>
StaticModelRenderer::add_texture_material(const AddResourceContext& context,
                                          vk::SampledImageManager::Handle handle) {
  if (auto maybe_im = context.sampled_image_manager.get(handle)) {
    auto& im = maybe_im.value();
    if (im.fragment_shader_sample_ok() && im.is_2d()) {
      Material material{};
      material.image_handle = handle;

      MaterialHandle mat_handle{next_material_id++};
      materials[mat_handle] = std::move(material);
      return Optional<MaterialHandle>(mat_handle);
    } else {
      GROVE_ASSERT(false);
    }
  }
  return NullOpt{};
}

StaticModelRenderer::AddResourceContext
StaticModelRenderer::make_add_resource_context(vk::GraphicsContext& graphics_context) {
  return StaticModelRenderer::AddResourceContext{
    &graphics_context.allocator,
    graphics_context.core,
    graphics_context.command_processor,
    graphics_context.sampled_image_manager,
    graphics_context.buffer_system
  };
}

GROVE_NAMESPACE_END
