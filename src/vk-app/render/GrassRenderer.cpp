#include "GrassRenderer.hpp"
#include "../vk/vk.hpp"
#include "debug_label.hpp"
#include "shadow.hpp"
#include "graphics.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/scope.hpp"
#include "grove/math/util.hpp"
#include <iostream>

#define PREFER_ALT_SUN (1)

GROVE_NAMESPACE_BEGIN

namespace {

using namespace vk;

using SetDataContext = GrassRenderer::SetDataContext;

struct HighLODGrassUniformData {
  Mat4f view;
  Mat4f projection;
  Mat4f sun_light_view_projection0;
  Vec4f camera_position;

  Vec4f camera_front_xz;
  Vec4f blade_scale_taper_power;
  Vec4f next_blade_scale;

  Vec4f frustum_grid_dims; //  vec2 cell size, float offset, float extent
  Vec4f extent_info; // vec2 z-extent, float scale0, float scale1

  Vec4f sun_position;
  Vec4f sun_color;

  Vec4f wind_world_bound_xz;
  Vec4f time_info;
  Vec4f terrain_grid_scale_max_diffuse_max_spec;
  Vec4f min_shadow_global_color_scale_discard_at_edge;
};

struct LowLODGrassUniformData {
  Mat4f view;
  Mat4f projection;
  Mat4f sun_light_view_projection0;

  Vec4f camera_position;
  Vec4f frustum_grid_cell_size_terrain_grid_scale;
  Vec4f wind_world_bound_xz;

  Vec4f near_scale_info; //  extent xy, scale zw
  Vec4f far_scale_info;  //  extent xy, scale zw

  Vec4f time_max_diffuse_max_specular; //  t, max diffuse, max specular, unused

  Vec4f min_shadow_global_color_scale; //  min, max, unused, unused
  Vec4f sun_position;
  Vec4f sun_color;
};

static_assert(alignof(HighLODGrassUniformData) == 4);
static_assert(alignof(LowLODGrassUniformData) == 4);

[[maybe_unused]] constexpr const char* logging_id() {
  return "GrassRenderer";
}

std::array<VertexBufferDescriptor, 2> high_lod_vertex_buffer_descriptors() {
  std::array<VertexBufferDescriptor, 2> result{};
  result[0].add_attribute(AttributeDescriptor::float2(0));    //  geometry
  result[1].add_attribute(AttributeDescriptor::float2(1, 1));
  result[1].add_attribute(AttributeDescriptor::float1(2, 1));
  result[1].add_attribute(AttributeDescriptor::float1(3, 1));
  return result;
}

std::array<VertexBufferDescriptor, 2> low_lod_vertex_buffer_descriptors() {
  std::array<VertexBufferDescriptor, 2> result{};
  result[0].add_attribute(AttributeDescriptor::float2(0));    //  geometry
  result[1].add_attribute(AttributeDescriptor::float4(1, 1));
  return result;
}

glsl::PreprocessorDefinition pcf_disabled_def() {
  return glsl::make_define("NO_PCF");
}

void configure_defines(glsl::LoadVertFragProgramSourceParams& params, bool pcf_disabled) {
  params.compile.frag_defines.push_back(
    csm::make_num_sun_shadow_cascades_preprocessor_definition());
  if (pcf_disabled) {
    params.compile.frag_defines.push_back(pcf_disabled_def());
  }
}

Optional<glsl::VertFragProgramSource> create_high_lod_program_source(bool pcf_disabled) {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "grass/grass.vert";
  params.frag_file = "grass/grass.frag";
  configure_defines(params, pcf_disabled);
  params.reflect.to_vk_descriptor_type = [](const glsl::refl::DescriptorInfo& info) {
    if (info.is_storage_buffer()) {
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    } else if (info.is_uniform_buffer() &&
               ((info.set == 0 && info.binding == 0) ||
               (info.set == 0 && info.binding == 10))) {
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    } else {
      return vk::refl::to_vk_descriptor_type(info.type);
    }
  };
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_low_lod_program_source(bool pcf_disabled) {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "grass/alt-grass.vert";
  params.frag_file = "grass/alt-grass.frag";
  configure_defines(params, pcf_disabled);
  params.reflect.to_vk_descriptor_type = [](const glsl::refl::DescriptorInfo& info) {
    if (info.is_storage_buffer()) {
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    } else if (info.is_uniform_buffer() && info.set == 0 && info.binding == 10) {
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    } else {
      return vk::refl::to_vk_descriptor_type(info.type);
    }
  };
  return glsl::make_vert_frag_program_source(params);
}

Result<Pipeline> create_high_lod_pipeline(const vk::Device& device,
                                          const glsl::VertFragProgramSource& source,
                                          VkPipelineLayout layout,
                                          const PipelineRenderPassInfo& pass_info) {
  auto buff_descrs = high_lod_vertex_buffer_descriptors();
  VertexInputDescriptors in_descrs{};
  to_vk_vertex_input_descriptors(uint32_t(buff_descrs.size()), buff_descrs.data(), &in_descrs);
  DefaultConfigureGraphicsPipelineStateParams params{in_descrs};
  params.raster_samples = pass_info.raster_samples;
  params.cull_mode = VK_CULL_MODE_NONE;
  params.num_color_attachments = 1;
  params.blend_enabled[0] = true;
  GraphicsPipelineStateCreateInfo state{};
  default_configure(&state, params);
  return create_vert_frag_graphics_pipeline(
    device.handle, source.vert_bytecode, source.frag_bytecode,
    &state, layout, pass_info.render_pass, pass_info.subpass);
}

Result<Pipeline> create_low_lod_pipeline(const vk::Device& device,
                                         const glsl::VertFragProgramSource& source,
                                         VkPipelineLayout layout,
                                         const PipelineRenderPassInfo& pass_info) {
  auto buff_descrs = low_lod_vertex_buffer_descriptors();
  VertexInputDescriptors in_descrs{};
  to_vk_vertex_input_descriptors(uint32_t(buff_descrs.size()), buff_descrs.data(), &in_descrs);
  DefaultConfigureGraphicsPipelineStateParams params{in_descrs};
  params.raster_samples = pass_info.raster_samples;
  params.cull_mode = VK_CULL_MODE_NONE;
  params.num_color_attachments = 1;
  params.blend_enabled[0] = true;
  GraphicsPipelineStateCreateInfo state{};
  default_configure(&state, params);
  return create_vert_frag_graphics_pipeline(
    device.handle, source.vert_bytecode, source.frag_bytecode,
    &state, layout, pass_info.render_pass, pass_info.subpass);
}

HighLODGrassUniformData
make_high_lod_grass_uniform_data(const Camera& camera, const GrassVisualParams& visual_params,
                                 const Vec2f& grid_cell_size, float grid_z_extent,
                                 float grid_z_offset,
                                 const Vec3f& sun_pos, const Vec3f& sun_color,
                                 const Mat4f& sun_light_view_projection0,
                                 const Vec4f& wind_world_bound_xz,
                                 float time, float terrain_grid_scale,
                                 float min_shadow, float global_color_scale, bool discard_at_edge,
                                 float max_diffuse, float max_specular) {
  const Vec3f front_xz = camera.get_front_xz();
  Vec4f use_front_xz{-front_xz.x, -front_xz.z, 0.0f, 0.0f};
  const Vec4f grid_info{grid_cell_size.x, grid_cell_size.y, grid_z_offset, grid_z_extent};

  auto proj = camera.get_projection();
  proj[1] = -proj[1];

  HighLODGrassUniformData result{};
  result.view = camera.get_view();
  result.projection = proj;
  result.sun_light_view_projection0 = sun_light_view_projection0;
  result.camera_position = Vec4f{camera.get_position(), 0.0f};
  result.camera_front_xz = use_front_xz;
  result.blade_scale_taper_power = Vec4f{visual_params.blade_scale, visual_params.taper_power};
  result.next_blade_scale = Vec4f{visual_params.next_blade_scale, 0.0f};
  result.frustum_grid_dims = grid_info;
  result.extent_info = Vec4f{
    visual_params.far_z_extents.x,
    visual_params.far_z_extents.y,
    visual_params.far_scale_factors.x,
    visual_params.far_scale_factors.y
  };
  result.sun_position = Vec4f{sun_pos, 1.0f};
  result.sun_color = Vec4f{sun_color, 1.0f};
  result.wind_world_bound_xz = wind_world_bound_xz;
  result.time_info = Vec4f{time, 0.0f, 0.0f, 0.0f};
  result.terrain_grid_scale_max_diffuse_max_spec = Vec4f{
    terrain_grid_scale, max_diffuse, max_specular, 0.0f};
  result.min_shadow_global_color_scale_discard_at_edge = Vec4f{
    min_shadow, global_color_scale, float(discard_at_edge), 0.0f
  };
  return result;
}

void set_discard_at_edge(HighLODGrassUniformData& data, bool value) {
  data.min_shadow_global_color_scale_discard_at_edge.z = float(value);
}

LowLODGrassUniformData make_low_lod_grass_uniform_data(const Camera& camera,
                                                       const GrassVisualParams& visual_params,
                                                       const Vec2f& grid_cell_size,
                                                       const Vec3f& sun_pos,
                                                       const Vec3f& sun_color,
                                                       const Mat4f& sun_light_view_projection0,
                                                       const Vec4f& wind_world_bound_xz,
                                                       float time,
                                                       float terrain_grid_scale,
                                                       float min_shadow,
                                                       float global_color_scale,
                                                       float max_diffuse, float max_specular) {
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  LowLODGrassUniformData res{};
  res.view = camera.get_view();
  res.projection = proj;
  res.sun_light_view_projection0 = sun_light_view_projection0;
  res.camera_position = Vec4f{camera.get_position(), 0.0f};
  res.frustum_grid_cell_size_terrain_grid_scale = Vec4f{
    grid_cell_size.x, grid_cell_size.y, terrain_grid_scale, 0.0f
  };
  res.wind_world_bound_xz = wind_world_bound_xz;
  res.near_scale_info = Vec4f{
    visual_params.near_z_extents.x, visual_params.near_z_extents.y,
    visual_params.near_scale_factors.x, visual_params.near_scale_factors.y
  };
  res.far_scale_info = Vec4f{
    visual_params.far_z_extents.x, visual_params.far_z_extents.y,
    visual_params.far_scale_factors.x, visual_params.far_scale_factors.y
  };
  res.time_max_diffuse_max_specular = Vec4f{time, max_diffuse, max_specular, 0.0f};
  res.min_shadow_global_color_scale = Vec4f{min_shadow, global_color_scale, 0.0f, 0.0f};
  res.sun_position = Vec4f{sun_pos, 0.0f};
  res.sun_color = Vec4f{sun_color, 0.0f};
  return res;
}

vk::BufferSystem::BufferHandle create_instance_buffer(const SetDataContext& context,
                                                      const std::vector<float>& data,
                                                      size_t* size) {
  auto instance_buffer_size = data.size() * sizeof(float);
  *size = instance_buffer_size;
  if (auto res = create_host_visible_vertex_buffer(context.allocator, instance_buffer_size)) {
    auto instance = context.buffer_system.emplace(std::move(res.value));
    instance.get().write(data.data(), instance_buffer_size);
    return instance;
  } else {
    GROVE_ASSERT(false);
    return {};
  }
}

vk::BufferSystem::BufferHandle create_frustum_grid_buffer(const SetDataContext& context,
                                                          const std::vector<float>& data,
                                                          size_t* size) {
  auto grid_data_size = data.size() * sizeof(float);
  *size = grid_data_size;
  auto grid_buffer_size = grid_data_size * context.frame_info.frame_queue_depth;
  if (auto res = create_storage_buffer(context.allocator, grid_buffer_size)) {
    auto grid = context.buffer_system.emplace(std::move(res.value));
    return grid;
  } else {
    GROVE_ASSERT(false);
    return {};
  }
}

struct {
  gfx::PipelineHandle new_high_lod_pipeline;
  gfx::PipelineHandle new_low_lod_pipeline;
  gfx::BufferHandle new_material_uniform_buffers[3];
} globals;

void create_new_material_pipelines(const GrassRenderer::BeginFrameInfo& info) {
  auto pass = gfx::get_forward_write_back_render_pass_handle(&info.context);
  if (!pass) {
    return;
  }

  bool pcf_disabled = true;
  {
    auto source = [pcf_disabled]() {
      glsl::LoadVertFragProgramSourceParams params{};
      params.vert_file = "grass/new-high-lod.vert";
      params.frag_file = "grass/new-high-lod.frag";
      configure_defines(params, pcf_disabled);
      params.reflect.to_vk_descriptor_type = [](const auto& info) {
        if (info.is_storage_buffer()) {
          return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        } else if (info.is_uniform_buffer() && info.binding == 0) {
          return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        } else {
          return refl::identity_descriptor_type(info);
        }
      };
      return glsl::make_vert_frag_program_source(params);
    }();
    if (source) {
      auto buff_descs = high_lod_vertex_buffer_descriptors();
      gfx::GraphicsPipelineCreateInfo create_info{};
      create_info.num_vertex_buffer_descriptors = 2;
      create_info.vertex_buffer_descriptors = buff_descs.data();
      create_info.num_color_attachments = 1;
      create_info.enable_blend[0] = true;
      create_info.disable_cull_face = true;
      auto pipe = gfx::create_pipeline(
        &info.context, std::move(source.value()), create_info, pass.value());
      if (pipe) {
        globals.new_high_lod_pipeline = std::move(pipe.value());
      }
    }
  }
  {
    auto source = [pcf_disabled]() {
      glsl::LoadVertFragProgramSourceParams params{};
      params.vert_file = "grass/new-low-lod.vert";
      params.frag_file = "grass/new-low-lod.frag";
      configure_defines(params, pcf_disabled);
      params.reflect.to_vk_descriptor_type = [](const auto& info) {
        if (info.is_storage_buffer()) {
          return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        } else {
          return vk::refl::to_vk_descriptor_type(info.type);
        }
      };
      return glsl::make_vert_frag_program_source(params);
    }();
    if (source) {
      auto buff_descs = low_lod_vertex_buffer_descriptors();
      gfx::GraphicsPipelineCreateInfo create_info{};
      create_info.num_vertex_buffer_descriptors = 2;
      create_info.vertex_buffer_descriptors = buff_descs.data();
      create_info.num_color_attachments = 1;
      create_info.enable_blend[0] = true;
      create_info.disable_cull_face = true;
      auto pipe = gfx::create_pipeline(
        &info.context, std::move(source.value()), create_info, pass.value());
      if (pipe) {
        globals.new_low_lod_pipeline = std::move(pipe.value());
      }
    }
  }
}

} //  anon

bool GrassRenderer::is_valid() const {
  return high_lod_program_components.pipeline_handle.get().is_valid() &&
         high_lod_info.has_data &&
         low_lod_program_components.pipeline_handle.get().is_valid() &&
         low_lod_info.has_data;
}

void GrassRenderer::set_high_lod_params(const GrassVisualParams& params) {
  high_lod_info.visual_params = params;
}

void GrassRenderer::set_low_lod_params(const GrassVisualParams& params) {
  low_lod_info.visual_params = params;
}

NewGrassRendererMaterialData GrassRenderer::get_new_material_data() const {
  auto& rp = render_params;
  assert(rp.frac_global_color_scale >= 0.0f && rp.frac_global_color_scale <= 1.0f);
  const auto& mat_params = rp.prefer_season_controlled_new_material_params ?
    rp.season_controlled_new_material_params : rp.new_material_params;
  float overall_scale = lerp(
    rp.frac_global_color_scale,
    mat_params.min_overall_scale, mat_params.max_overall_scale);
  float color_variation = lerp(
    rp.frac_global_color_scale,
    mat_params.min_color_variation, mat_params.max_color_variation);
  NewGrassRendererMaterialData material_data{};
  material_data.base_color0_spec_scale = Vec4f{mat_params.base_color0, mat_params.spec_scale};
  material_data.base_color1_spec_power = Vec4f{mat_params.base_color1, mat_params.spec_power};
  material_data.tip_color_overall_scale = Vec4f{mat_params.tip_color, overall_scale};
  material_data.color_variation_unused = Vec4f{color_variation, 0.0f, 0.0f, 0.0f};
  return material_data;
}

void GrassRenderer::begin_frame_set_high_lod_grid_data(const SetDataContext& context,
                                                       const FrustumGrid& grid) {
  auto& grid_data = grid.get_data();
  auto grid_data_size = grid_data.size() * sizeof(float);
  GROVE_ASSERT(grid_data_size == high_lod_buffers.current_grid_data_size);
  auto off = context.frame_info.current_frame_index * grid_data_size;
  high_lod_buffers.grid.get().write(grid_data.data(), grid_data_size, off);

  high_lod_info.grid_cell_size = grid.get_cell_size();
  high_lod_info.grid_z_extent = grid.get_z_extent();
  high_lod_info.grid_z_offset = grid.get_z_offset();
}

void GrassRenderer::begin_frame_set_low_lod_grid_data(const SetDataContext& context,
                                                      const FrustumGrid& grid) {
  auto& grid_data = grid.get_data();
  auto grid_data_size = grid_data.size() * sizeof(float);
  GROVE_ASSERT(grid_data_size == low_lod_buffers.current_grid_data_size);
  auto off = context.frame_info.current_frame_index * grid_data_size;
  low_lod_buffers.grid.get().write(grid_data.data(), grid_data_size, off);

  low_lod_info.grid_cell_size = grid.get_cell_size();
  low_lod_info.grid_z_extent = grid.get_z_extent();
  low_lod_info.grid_z_offset = grid.get_z_offset();
}

void GrassRenderer::set_high_lod_data(const SetDataContext& context,
                                      const FrustumGridInstanceData& instance_data,
                                      const std::vector<float>& grid_data) {
  high_lod_info.has_data = false;

  auto* allocator = context.allocator;
  auto& buffer_system = context.buffer_system;

  const std::vector<float> geom_data = geometry::segmented_quad_positions(
    high_lod_info.visual_params.num_blade_segments, false);
  const auto geom_size = geom_data.size() * sizeof(float);

  if (auto res = create_host_visible_vertex_buffer(allocator, geom_size)) {
    high_lod_buffers.geometry = buffer_system.emplace(std::move(res.value));
    high_lod_buffers.geometry.get().write(geom_data.data(), geom_size);
  } else {
    GROVE_ASSERT(false);
    return;
  }

  size_t inst_buff_size{};
  high_lod_buffers.instance = create_instance_buffer(context, instance_data.data, &inst_buff_size);
  high_lod_buffers.grid = create_frustum_grid_buffer(
    context, grid_data, &high_lod_buffers.current_grid_data_size);
  high_lod_info.draw_desc = {};
  high_lod_info.draw_desc.num_vertices = uint32_t(geom_data.size()) / 2;
  high_lod_info.draw_desc.num_instances = uint32_t(instance_data.num_instances);
  high_lod_info.has_data = true;
}

void GrassRenderer::set_low_lod_data(const SetDataContext& context,
                                     const FrustumGridInstanceData& instance_data,
                                     const std::vector<float>& grid_data) {
  low_lod_info.has_data = false;

  const std::vector<float> positions = geometry::quad_positions(false);
  const std::vector<uint16_t> indices = geometry::quad_indices();
  const size_t pos_size = positions.size() * sizeof(float);
  const size_t inds_size = indices.size() * sizeof(uint16_t);

  {
    auto res = create_device_local_vertex_buffer_sync(
      context.allocator,
      pos_size,
      positions.data(),
      &context.core,
      &context.uploader);
    if (res) {
      low_lod_buffers.geometry = context.buffer_system.emplace(std::move(res.value));
    } else {
      GROVE_ASSERT(false);
      return;
    }
  }
  {
    auto res = create_device_local_index_buffer_sync(
      context.allocator,
      inds_size,
      indices.data(),
      &context.core,
      &context.uploader);
    if (res) {
      low_lod_buffers.index = context.buffer_system.emplace(std::move(res.value));
    } else {
        GROVE_ASSERT(false);
      return;
    }
  }

  size_t inst_buff_size{};
  low_lod_buffers.instance = create_instance_buffer(context, instance_data.data, &inst_buff_size);
  low_lod_buffers.grid = create_frustum_grid_buffer(
    context, grid_data, &low_lod_buffers.current_grid_data_size);
  low_lod_info.draw_indexed_desc = {};
  low_lod_info.draw_indexed_desc.num_indices = uint32_t(indices.size());
  low_lod_info.draw_indexed_desc.num_instances = uint32_t(instance_data.num_instances);
  low_lod_info.has_data = true;
}

void GrassRenderer::terminate() {
  globals.new_high_lod_pipeline = {};
  globals.new_low_lod_pipeline = {};
  for (auto& buff : globals.new_material_uniform_buffers) {
    buff = {};
  }
}

void GrassRenderer::initialize(const InitInfo& init_info) {
  glsl::VertFragProgramSource high_lod_program_source;
  if (!make_high_lod_program(init_info, &high_lod_program_source)) {
    return;
  }

  glsl::VertFragProgramSource low_lod_program_source;
  if (!make_low_lod_program(init_info, &low_lod_program_source)) {
    return;
  }

  make_desc_set_allocators(
    init_info.descriptor_system, high_lod_program_source, low_lod_program_source);

  for (uint32_t i = 0; i < init_info.frame_queue_depth; i++) {
    if (auto res = create_uniform_buffer(init_info.allocator, sizeof(csm::SunCSMSampleData))) {
      shadow_uniform_buffers.push_back(
        init_info.buffer_system.emplace(std::move(res.value)));
    } else {
      GROVE_ASSERT(false);
      return;
    }
    {
      //  High lod uniform buffer, dynamic.
      size_t buff_size{};
      auto res = create_dynamic_uniform_buffer<HighLODGrassUniformData>(
        init_info.allocator,
        &init_info.core.physical_device.info.properties,
        2,  //  render twice per frame, toggle discard_at_edge enabled / disabled
        &high_lod_buffers.uniform_stride,
        &buff_size);
      if (!res) {
        GROVE_ASSERT(false);
        return;
      }
      high_lod_buffers.uniform.push_back(
        init_info.buffer_system.emplace(std::move(res.value)));
    }
    if (auto res = create_uniform_buffer(init_info.allocator, sizeof(LowLODGrassUniformData))) {
      low_lod_buffers.uniform.push_back(
        init_info.buffer_system.emplace(std::move(res.value)));
    } else {
      GROVE_ASSERT(false);
      return;
    }
  }

#if PREFER_ALT_SUN
  render_params.max_diffuse = 0.45f;
#endif
#if 1
  toggle_new_material_pipeline();
#endif
}

Optional<vk::SampledImageManager::ReadInstance>
GrassRenderer::get_terrain_color_image(const vk::SampledImageManager& manager) {
  auto maybe_im_handle = (prefer_alt_color_image && alt_terrain_color_image) ?
    alt_terrain_color_image : terrain_color_image;

  if (maybe_im_handle) {
    if (auto terrain_color_im = manager.get(maybe_im_handle.value())) {
      if (terrain_color_im.value().is_2d() &&
          terrain_color_im.value().fragment_shader_sample_ok()) {
        return terrain_color_im;
      }
    }
  }

  return NullOpt{};
}

Optional<vk::DynamicSampledImageManager::ReadInstance>
GrassRenderer::get_wind_displacement_image(const vk::DynamicSampledImageManager& manager) {
  if (wind_displacement_image) {
    if (auto wind_im = manager.get(wind_displacement_image.value())) {
      if (wind_im.value().is_2d() && wind_im.value().vertex_shader_sample_ok()) {
        return wind_im;
      }
    }
  }
  return NullOpt{};
}

Optional<vk::DynamicSampledImageManager::ReadInstance>
GrassRenderer::get_height_map_image(const vk::DynamicSampledImageManager& manager) {
  if (height_map_image) {
    if (auto height_im = manager.get(height_map_image.value())) {
      if (height_im.value().is_2d() && height_im.value().vertex_shader_sample_ok()) {
        return height_im;
      }
    }
  }
  return NullOpt{};
}

void GrassRenderer::begin_frame(const BeginFrameInfo& info) {
  const auto elapsed_t = float((stopwatch.delta() + std::chrono::seconds(30)).count());
  latest_total_num_vertices_drawn = 0;

  {
    //  Shadow
    auto& un_buff = shadow_uniform_buffers[info.frame_index];
    csm::SunCSMSampleData un_data = csm::make_sun_csm_sample_data(info.csm_desc);
    un_buff.get().write(&un_data, sizeof(csm::SunCSMSampleData));
  }

  {
    //  High lod
    auto un_data_discard = make_high_lod_grass_uniform_data(
      info.camera,
      high_lod_info.visual_params,
      high_lod_info.grid_cell_size,
      high_lod_info.grid_z_extent,
      high_lod_info.grid_z_offset,
      render_params.sun_position,
      render_params.sun_color,
      info.csm_desc.light_shadow_sample_view,
      render_params.wind_world_bound_xz,
      elapsed_t,
      render_params.terrain_grid_scale,
      render_params.min_shadow,
      render_params.global_color_scale,
      false,
      render_params.max_diffuse,
      render_params.max_specular);

    auto un_data_no_discard = un_data_discard;
    set_discard_at_edge(un_data_discard, true);
    set_discard_at_edge(un_data_no_discard, false);

    size_t stride = high_lod_buffers.uniform_stride;
    auto& un_buff = high_lod_buffers.uniform[info.frame_index];
    un_buff.get().write(&un_data_discard, sizeof(un_data_discard));
    un_buff.get().write(&un_data_no_discard, sizeof(un_data_no_discard), stride);
  }
  {
    //  Low lod
    auto& un_buff = low_lod_buffers.uniform[info.frame_index];
    auto un_data = make_low_lod_grass_uniform_data(
      info.camera,
      low_lod_info.visual_params,
      low_lod_info.grid_cell_size,
      render_params.sun_position,
      render_params.sun_color,
      info.csm_desc.light_shadow_sample_view,
      render_params.wind_world_bound_xz,
      elapsed_t,
      render_params.terrain_grid_scale,
      render_params.min_shadow,
      render_params.global_color_scale,
      render_params.max_diffuse,
      render_params.max_specular);
    un_buff.get().write(&un_data, sizeof(un_data));
  }

  if (info.frame_index < 3) {
    auto& un_buff = globals.new_material_uniform_buffers[info.frame_index];
    if (!un_buff.is_valid()) {
      auto buff = gfx::create_uniform_buffer(&info.context, sizeof(NewGrassRendererMaterialData));
      if (buff) {
        globals.new_material_uniform_buffers[info.frame_index] = std::move(buff.value());
      }
    }
    if (un_buff.is_valid()) {
      NewGrassRendererMaterialData material_data = get_new_material_data();
      un_buff.write(&material_data, sizeof(NewGrassRendererMaterialData));
    }
  }

  if (need_recreate_new_pipelines) {
    create_new_material_pipelines(info);
    need_recreate_new_pipelines = false;
  }
}

bool GrassRenderer::make_low_lod_program(const InitInfo& info,
                                         glsl::VertFragProgramSource* source) {
  auto low_lod_prog_source = create_low_lod_program_source(pcf_disabled);
  if (!low_lod_prog_source) {
    return false;
  }

  const auto& low_lod_layout_bindings =
    low_lod_prog_source.value().descriptor_set_layout_bindings;

  if (!info.pipeline_system.require_layouts(
    info.core.device.handle,
    make_view(low_lod_prog_source.value().push_constant_ranges),
    make_view(low_lod_layout_bindings),
    &low_lod_program_components.pipeline_layout,
    &low_lod_program_components.set_layouts)) {
    return false;
  }

  auto pipe_res = create_low_lod_pipeline(
    info.core.device,
    low_lod_prog_source.value(),
    low_lod_program_components.pipeline_layout,
    info.forward_pass_info);
  if (!pipe_res) {
    return false;
  } else {
    low_lod_program_components.pipeline_handle =
      info.pipeline_system.emplace(std::move(pipe_res.value));
  }

  if (source) {
    *source = std::move(low_lod_prog_source.value());
  }
  return true;
}

void GrassRenderer::remake_programs(const InitInfo& info, Optional<bool> pcf_enabled) {
  if (pcf_enabled) {
    pcf_disabled = !pcf_enabled.value();
  }

  glsl::VertFragProgramSource high_lod;
  glsl::VertFragProgramSource low_lod;
  if (!make_high_lod_program(info, &high_lod)) {
    return;
  }
  if (!make_low_lod_program(info, &low_lod)) {
    return;
  }

  make_desc_set_allocators(info.descriptor_system, high_lod, low_lod);
}

void GrassRenderer::make_desc_set_allocators(vk::DescriptorSystem& desc_system,
                                             const glsl::VertFragProgramSource& high_lod,
                                             const glsl::VertFragProgramSource& low_lod) {
  const auto& high_lod_layout_bindings = high_lod.descriptor_set_layout_bindings;
  const auto& low_lod_layout_bindings = low_lod.descriptor_set_layout_bindings;

  auto get_size = [](ShaderResourceType) { return 8; };
  DescriptorPoolAllocator::PoolSizes pool_sizes;
  push_pool_sizes_from_layout_bindings(pool_sizes, make_view(high_lod_layout_bindings), get_size);
  push_pool_sizes_from_layout_bindings(pool_sizes, make_view(low_lod_layout_bindings), get_size);
  desc_pool_allocator = desc_system.create_pool_allocator(make_view(pool_sizes), 8);
  high_lod_program_components.desc_set0_allocator =
    desc_system.create_set_allocator(desc_pool_allocator.get());
  low_lod_program_components.desc_set0_allocator =
    desc_system.create_set_allocator(desc_pool_allocator.get());
}

bool GrassRenderer::make_high_lod_program(const InitInfo& info,
                                          glsl::VertFragProgramSource* source) {
  auto high_lod_prog_source = create_high_lod_program_source(pcf_disabled);
  if (!high_lod_prog_source) {
    return false;
  }

  const auto& high_lod_layout_bindings =
    high_lod_prog_source.value().descriptor_set_layout_bindings;

  if (!info.pipeline_system.require_layouts(
    info.core.device.handle,
    make_view(high_lod_prog_source.value().push_constant_ranges),
    make_view(high_lod_layout_bindings),
    &high_lod_program_components.pipeline_layout,
    &high_lod_program_components.set_layouts)) {
    return false;
  }

  auto pipe_res = create_high_lod_pipeline(
    info.core.device,
    high_lod_prog_source.value(),
    high_lod_program_components.pipeline_layout,
    info.forward_pass_info);
  if (!pipe_res) {
    return false;
  } else {
    high_lod_program_components.pipeline_handle =
      info.pipeline_system.emplace(std::move(pipe_res.value));
  }

  if (source) {
    *source = std::move(high_lod_prog_source.value());
  }
  return true;
}

void GrassRenderer::render(const RenderInfo& render_info) {
  auto db_label = GROVE_VK_SCOPED_DEBUG_LABEL(render_info.cmd, "render_grass");
  (void) db_label;

  if (prefer_new_material_pipeline) {
    if (!high_lod_info.disabled) {
      render_new_material_high_lod(render_info, 0);
    }
    if (!low_lod_info.disabled) {
      render_new_material_low_lod(render_info);
    }
    if (!high_lod_info.disabled && !high_lod_info.post_pass_disabled) {
      render_new_material_high_lod(render_info, high_lod_buffers.uniform_stride);
    }
  } else {
    if (!high_lod_info.disabled) {
      render_high_lod(render_info, 0);
    }
    if (!low_lod_info.disabled) {
      render_low_lod(render_info);
    }
    if (!high_lod_info.disabled && !high_lod_info.post_pass_disabled) {
      render_high_lod(render_info, high_lod_buffers.uniform_stride);
    }
  }
}

void GrassRenderer::render_new_material_low_lod(const RenderInfo& info) {
  auto& pipe = globals.new_low_lod_pipeline;
  if (!pipe.is_valid() || info.frame_index >= 3 ||
      !globals.new_material_uniform_buffers[info.frame_index].is_valid()) {
    return;
  }

  auto color_im = get_terrain_color_image(info.sampled_image_manager);
  auto wind_im = get_wind_displacement_image(info.dynamic_sampled_image_manager);
  auto height_im = get_height_map_image(info.dynamic_sampled_image_manager);
  if (!color_im || !wind_im || !height_im) {
    return;
  }

  uint32_t dynamic_offsets[16];
  uint32_t num_dynamic_offsets{};

  const auto& mat_un_buff = globals.new_material_uniform_buffers[info.frame_index];

  dynamic_offsets[num_dynamic_offsets++] = uint32_t(
    low_lod_buffers.current_grid_data_size * info.frame_index);

  auto sampler = info.sampler_system.require_linear_edge_clamp(info.device);

  DescriptorSetScaffold scaffold{};
  scaffold.set = 0;
  uint32_t bind{};
  push_uniform_buffer(  //  uniform
    scaffold, bind++, low_lod_buffers.uniform[info.frame_index].get());
  push_dynamic_storage_buffer(  //  frustum grid
    scaffold, bind++, low_lod_buffers.grid.get(), low_lod_buffers.current_grid_data_size);
  push_combined_image_sampler(  //  height map
    scaffold, bind++, height_im.value().view, sampler, height_im.value().layout);
  push_combined_image_sampler(  //  wind displacement
    scaffold, bind++, wind_im.value().view, sampler, wind_im.value().layout);
  push_uniform_buffer(  //  shadow uniform buffer
    scaffold, bind++, shadow_uniform_buffers[info.frame_index].get());
  push_combined_image_sampler(  //  shadow image
    scaffold, bind++, info.shadow_image, sampler);
  push_uniform_buffer(  //  new material data
    scaffold, bind++, mat_un_buff.get(), sizeof(NewGrassRendererMaterialData));
  push_combined_image_sampler(  //  splotch
    scaffold, bind++, color_im.value().view, sampler, color_im.value().layout);

  auto desc_set = gfx::require_updated_descriptor_set(&info.context, scaffold, pipe);
  if (!desc_set) {
    return;
  }

  cmd::bind_graphics_pipeline(info.cmd, pipe.get());
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  //
  const VkDeviceSize vb_offsets[2] = {0, 0};
  const VkBuffer vb_buffers[2] = {
    low_lod_buffers.geometry.get().contents().buffer.handle,
    low_lod_buffers.instance.get().contents().buffer.handle
  };
  VkBuffer ind_buff = low_lod_buffers.index.get().contents().buffer.handle;

  vkCmdBindVertexBuffers(info.cmd, 0, 2, vb_buffers, vb_offsets);
  vkCmdBindIndexBuffer(info.cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);

  cmd::bind_graphics_descriptor_sets(
    info.cmd, pipe.get_layout(), 0, 1, &desc_set.value(), num_dynamic_offsets, dynamic_offsets);
  cmd::draw_indexed(info.cmd, &low_lod_info.draw_indexed_desc);

  const auto& count_desc = low_lod_info.draw_indexed_desc;
  uint32_t num_drawn = count_desc.num_instances * count_desc.num_indices;
  latest_total_num_vertices_drawn += num_drawn;
}

void GrassRenderer::render_new_material_high_lod(const RenderInfo& info, size_t un_buff_dyn_off) {
  auto& pipe = globals.new_high_lod_pipeline;
  if (!pipe.is_valid() || info.frame_index >= 3 ||
      !globals.new_material_uniform_buffers[info.frame_index].is_valid()) {
    return;
  }

  auto color_im = get_terrain_color_image(info.sampled_image_manager);
  auto wind_im = get_wind_displacement_image(info.dynamic_sampled_image_manager);
  auto height_im = get_height_map_image(info.dynamic_sampled_image_manager);
  if (!color_im || !wind_im || !height_im) {
    return;
  }

  uint32_t dynamic_offsets[16];
  uint32_t num_dynamic_offsets{};

  dynamic_offsets[num_dynamic_offsets++] =
    uint32_t(un_buff_dyn_off);
  dynamic_offsets[num_dynamic_offsets++] =
    uint32_t(high_lod_buffers.current_grid_data_size * info.frame_index);

  auto sampler = info.sampler_system.require_linear_edge_clamp(info.device);
  const auto& dyn_un_buff = high_lod_buffers.uniform[info.frame_index].get();
  const auto& mat_un_buff = globals.new_material_uniform_buffers[info.frame_index];

  DescriptorSetScaffold scaffold{};
  scaffold.set = 0;
  uint32_t bind{};
  push_dynamic_uniform_buffer(  //  uniform buffer
    scaffold, bind++, dyn_un_buff, sizeof(HighLODGrassUniformData));
  push_dynamic_storage_buffer(  //  frustum grid buffer
    scaffold, bind++, high_lod_buffers.grid.get(), high_lod_buffers.current_grid_data_size);
  push_uniform_buffer(  //  shadow uniform
    scaffold, bind++, shadow_uniform_buffers[info.frame_index].get());
  push_combined_image_sampler(  //  shadow image
    scaffold, bind++, info.shadow_image, sampler);
  push_combined_image_sampler(  //  wind displacement
    scaffold, bind++, wind_im.value().view, sampler, wind_im.value().layout);
  push_uniform_buffer(  //  new material data
    scaffold, bind++, mat_un_buff.get(), sizeof(NewGrassRendererMaterialData));
  push_combined_image_sampler(  //  height displacement
    scaffold, bind++, height_im.value().view, sampler, height_im.value().layout);
  push_combined_image_sampler(  //  splotch
    scaffold, bind++, color_im.value().view, sampler, color_im.value().layout);

  auto desc_set = gfx::require_updated_descriptor_set(&info.context, scaffold, pipe);
  if (!desc_set) {
    return;
  }

  cmd::bind_graphics_pipeline(info.cmd, pipe.get());
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  //
  const VkDeviceSize vb_offsets[2] = {0, 0};
  const VkBuffer vb_buffers[2] = {
    high_lod_buffers.geometry.get().contents().buffer.handle,
    high_lod_buffers.instance.get().contents().buffer.handle
  };
  vkCmdBindVertexBuffers(info.cmd, 0, 2, vb_buffers, vb_offsets);
  cmd::bind_graphics_descriptor_sets(
    info.cmd, pipe.get_layout(), 0, 1, &desc_set.value(), num_dynamic_offsets, dynamic_offsets);
  cmd::draw(info.cmd, &high_lod_info.draw_desc);

  const auto& count_desc = high_lod_info.draw_desc;
  uint32_t num_drawn = count_desc.num_instances * count_desc.num_vertices;
  latest_total_num_vertices_drawn += num_drawn;
}

void GrassRenderer::render_high_lod(const RenderInfo& info, size_t un_buff_dyn_off) {
  auto terrain_color_im = get_terrain_color_image(info.sampled_image_manager);
  auto wind_im = get_wind_displacement_image(info.dynamic_sampled_image_manager);
  auto height_im = get_height_map_image(info.dynamic_sampled_image_manager);
  if (!terrain_color_im || !wind_im || !height_im) {
    return;
  }

  auto& desc_system = info.descriptor_system;
  DescriptorPoolAllocator* pool_alloc{};
  DescriptorSetAllocator* set0_alloc{};
  if (!desc_system.get(desc_pool_allocator.get(), &pool_alloc) ||
      !desc_system.get(high_lod_program_components.desc_set0_allocator.get(), &set0_alloc)) {
    return;
  }

  uint32_t dynamic_offsets[16];
  uint32_t num_dynamic_offsets{};

  dynamic_offsets[num_dynamic_offsets++] =
    uint32_t(un_buff_dyn_off);
  dynamic_offsets[num_dynamic_offsets++] =
    uint32_t(high_lod_buffers.current_grid_data_size * info.frame_index);

  VkDescriptorSet descr_set;
  {
    auto& terrain_im = terrain_color_im.value();
    auto sampler = info.sampler_system.require_linear_edge_clamp(info.device);

    const auto& dyn_un_buff = high_lod_buffers.uniform[info.frame_index].get();

    DescriptorSetScaffold scaffold{};
    scaffold.set = 0;
    uint32_t bind{};
    push_dynamic_uniform_buffer(  //  uniform buffer
      scaffold, bind++, dyn_un_buff, sizeof(HighLODGrassUniformData));
    push_dynamic_storage_buffer(  //  frustum grid buffer
      scaffold, bind++, high_lod_buffers.grid.get(), high_lod_buffers.current_grid_data_size);
    push_uniform_buffer(  //  shadow uniform
      scaffold, bind++, shadow_uniform_buffers[info.frame_index].get());
    push_combined_image_sampler(  //  shadow image
      scaffold, bind++, info.shadow_image, sampler);
    push_combined_image_sampler(  //  wind displacement
      scaffold, bind++, wind_im.value().view, sampler, wind_im.value().layout);
    push_combined_image_sampler(  //  ground color
      scaffold, bind++, terrain_im.view, sampler, terrain_im.layout);
    push_combined_image_sampler(  //  height displacement
      scaffold, bind++, height_im.value().view, sampler, height_im.value().layout);

    auto descr_set_res = set0_alloc->require_updated_descriptor_set(
      info.device, *high_lod_program_components.set_layouts.find(0), *pool_alloc, scaffold);
    if (!descr_set_res) {
      assert(false);
      return;
    } else {
      descr_set = descr_set_res.value;
    }
  }

  VkPipeline pipeline = high_lod_program_components.pipeline_handle.get().handle;
  cmd::bind_graphics_pipeline(info.cmd, pipeline);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  //
  const VkDeviceSize vb_offsets[2] = {0, 0};
  const VkBuffer vb_buffers[2] = {
    high_lod_buffers.geometry.get().contents().buffer.handle,
    high_lod_buffers.instance.get().contents().buffer.handle
  };
  vkCmdBindVertexBuffers(info.cmd, 0, 2, vb_buffers, vb_offsets);
  cmd::bind_graphics_descriptor_sets(
    info.cmd, high_lod_program_components.pipeline_layout, 0, 1, &descr_set,
    num_dynamic_offsets, dynamic_offsets);
  cmd::draw(info.cmd, &high_lod_info.draw_desc);

  const auto& count_desc = high_lod_info.draw_desc;
  uint32_t num_drawn = count_desc.num_instances * count_desc.num_vertices;
  latest_total_num_vertices_drawn += num_drawn;
}

void GrassRenderer::render_low_lod(const RenderInfo& render_info) {
  auto terrain_color_im = get_terrain_color_image(render_info.sampled_image_manager);
  auto wind_im = get_wind_displacement_image(render_info.dynamic_sampled_image_manager);
  auto height_im = get_height_map_image(render_info.dynamic_sampled_image_manager);
  if (!terrain_color_im || !wind_im || !height_im) {
    return;
  }

  auto& desc_system = render_info.descriptor_system;
  DescriptorPoolAllocator* pool_alloc{};
  DescriptorSetAllocator* set0_alloc{};
  if (!desc_system.get(desc_pool_allocator.get(), &pool_alloc) ||
      !desc_system.get(low_lod_program_components.desc_set0_allocator.get(), &set0_alloc)) {
    return;
  }

  uint32_t dynamic_offsets[16];
  uint32_t num_dynamic_offsets{};

  dynamic_offsets[num_dynamic_offsets++] = uint32_t(
    low_lod_buffers.current_grid_data_size * render_info.frame_index);

  VkDescriptorSet descr_set;
  {
    auto& terrain_im = terrain_color_im.value();
    auto sampler = render_info.sampler_system.require_linear_edge_clamp(render_info.device);

    DescriptorSetScaffold scaffold{};
    scaffold.set = 0;
    uint32_t bind{};
    push_uniform_buffer(  //  uniform
      scaffold, bind++, low_lod_buffers.uniform[render_info.frame_index].get());
    push_dynamic_storage_buffer(  //  frustum grid
      scaffold, bind++, low_lod_buffers.grid.get(), low_lod_buffers.current_grid_data_size);
    push_combined_image_sampler(  //  height map
      scaffold, bind++, height_im.value().view, sampler, height_im.value().layout);
    push_combined_image_sampler(  //  wind displacement
      scaffold, bind++, wind_im.value().view, sampler, wind_im.value().layout);
    push_uniform_buffer(  //  shadow uniform buffer
      scaffold, bind++, shadow_uniform_buffers[render_info.frame_index].get());
    push_combined_image_sampler(  //  shadow image
      scaffold, bind++, render_info.shadow_image, sampler);
    push_combined_image_sampler(  //  ground color
      scaffold, bind++, terrain_im.view, sampler, terrain_im.layout);

    auto descr_set_res = set0_alloc->require_updated_descriptor_set(
      render_info.device, *low_lod_program_components.set_layouts.find(0), *pool_alloc, scaffold);
    if (!descr_set_res) {
      assert(false);
      return;
    } else {
      descr_set = descr_set_res.value;
    }
  }

  const VkCommandBuffer cmd = render_info.cmd;

  VkPipeline pipeline = low_lod_program_components.pipeline_handle.get().handle;
  cmd::bind_graphics_pipeline(cmd, pipeline);
  cmd::set_viewport_and_scissor(cmd, &render_info.viewport, &render_info.scissor_rect);
  //
  const VkDeviceSize vb_offsets[2] = {0, 0};
  const VkBuffer vb_buffers[2] = {
    low_lod_buffers.geometry.get().contents().buffer.handle,
    low_lod_buffers.instance.get().contents().buffer.handle
  };
  VkBuffer ind_buff = low_lod_buffers.index.get().contents().buffer.handle;

  vkCmdBindVertexBuffers(cmd, 0, 2, vb_buffers, vb_offsets);
  vkCmdBindIndexBuffer(cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);

  cmd::bind_graphics_descriptor_sets(
    cmd, low_lod_program_components.pipeline_layout, 0, 1,
    &descr_set, num_dynamic_offsets, dynamic_offsets);
  cmd::draw_indexed(cmd, &low_lod_info.draw_indexed_desc);

  const auto& count_desc = low_lod_info.draw_indexed_desc;
  uint32_t num_drawn = count_desc.num_instances * count_desc.num_indices;
  latest_total_num_vertices_drawn += num_drawn;
}

void GrassRenderer::toggle_new_material_pipeline() {
  if (prefer_new_material_pipeline) {
    prefer_new_material_pipeline = false;
    prefer_alt_color_image = false;
  } else {
    prefer_new_material_pipeline = true;
    prefer_alt_color_image = true;
    if (!globals.new_low_lod_pipeline.is_valid()) {
      need_recreate_new_pipelines = true;
    }
  }
}

GrassRenderer::NewMaterialParams
GrassRenderer::NewMaterialParams::from_frac_fall(float f, bool pref_new) {
  auto dflt = pref_new ? config_default_new() : config_default();
  auto fall = pref_new ? config_default_new() : config_fall();
  GrassRenderer::NewMaterialParams result{};
  result.base_color0 = lerp(f, dflt.base_color0, fall.base_color0);
  result.base_color1 = lerp(f, dflt.base_color1, fall.base_color1);
  result.tip_color = lerp(f, dflt.tip_color, fall.tip_color);
  result.spec_scale = lerp(f, dflt.spec_scale, fall.spec_scale);
  result.spec_power = lerp(f, dflt.spec_power, fall.spec_power);
  result.min_overall_scale = lerp(f, dflt.min_overall_scale, fall.min_overall_scale);
  result.max_overall_scale = lerp(f, dflt.max_overall_scale, fall.max_overall_scale);
  result.min_color_variation = lerp(f, dflt.min_color_variation, fall.min_color_variation);
  result.max_color_variation = lerp(f, dflt.max_color_variation, fall.max_color_variation);
  return result;
}

GrassRenderer::NewMaterialParams GrassRenderer::NewMaterialParams::config_default() {
  return {};
}

GrassRenderer::NewMaterialParams GrassRenderer::NewMaterialParams::config_default_new() {
  GrassRenderer::NewMaterialParams result{};
  result.base_color0 = Vec3f{0.15f, 0.606f, 0.067f};
//  result.base_color1 = Vec3f{0.22f, 0.659f, 0.112f};
  result.base_color1 = Vec3f{0.275f, 0.9f, 0.112f};
  result.tip_color = Vec3f{1.0f};
  result.spec_scale = 0.4f;
  result.spec_power = 1.0f;
  result.min_overall_scale = 0.85f;
  result.max_overall_scale = 1.45f;
  result.min_color_variation = 0.0f;
  result.max_color_variation = 1.0f;
  return result;
}

GrassRenderer::NewMaterialParams GrassRenderer::NewMaterialParams::config_fall() {
  GrassRenderer::NewMaterialParams result{};
  result.base_color0 = Vec3f{0.286f, 0.45f, 0.173f};
  result.base_color1 = Vec3f{0.375f, 1.0f, 0.222f};
  result.tip_color = Vec3f{0.8f, 1.0f, 0.901f};
  result.spec_scale = 0.4f;
  result.spec_power = 1.558f;
  result.min_overall_scale = 0.85f;
  result.max_overall_scale = 1.25f;
  result.min_color_variation = 0.25f;
  result.max_color_variation = 0.755f;
  return result;
}

GROVE_NAMESPACE_END
