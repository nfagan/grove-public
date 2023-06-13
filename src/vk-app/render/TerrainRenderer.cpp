#include "TerrainRenderer.hpp"
#include "GrassRenderer.hpp"
#include "shadow.hpp"
#include "utility.hpp"
#include "graphics_context.hpp"
#include "graphics.hpp"
#include "debug_label.hpp"
#include "csm.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/common/common.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/Frustum.hpp"
#include "grove/math/intersect.hpp"

#define USE_PUSH_DESCRIPTORS (0)
#define PREFER_SIMPLE_CUBE_MARCH (1)

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

using InitInfo = TerrainRenderer::InitInfo;
using RenderParams = TerrainRenderer::RenderParams;
using CubeMarchVertex = TerrainRenderer::CubeMarchVertex;

struct Set0UniformBufferData {
  csm::SunCSMSampleData shadow_data;
  Mat4f light_view_projection0;
  Mat4f view;
  Vec4f camera_position;
  Vec4f sun_pos_color_r;
  Vec4f sun_color_gb_time;
  Vec4f wind_world_bound_xz;
  Vec4f min_shadow_global_color_scale;
};

Set0UniformBufferData make_set0_uniform_buffer_data(const csm::SunCSMSampleData& shadow_data,
                                                    const csm::CSMDescriptor& csm_desc,
                                                    const Camera& camera,
                                                    const RenderParams& params,
                                                    float elapsed_time) {
  Set0UniformBufferData result;
  result.shadow_data = shadow_data;
  result.light_view_projection0 = csm_desc.light_shadow_sample_view;
  result.view = camera.get_view();
  result.camera_position = Vec4f{camera.get_position(), 0.0f};
  result.sun_pos_color_r = Vec4f{params.sun_position, params.sun_color.x};
  result.sun_color_gb_time = Vec4f{params.sun_color.y, params.sun_color.z, elapsed_time, 0.0f};
  result.wind_world_bound_xz = params.wind_world_bound_xz;
  result.min_shadow_global_color_scale = Vec4f{
    params.min_shadow, params.global_color_scale, 0.0f, 0.0f};
  return result;
}

struct CubeMarchPushConstantData {
  Mat4f projection_view;
};

std::array<VertexBufferDescriptor, 1> cube_march_buffer_descriptors() {
  std::array<VertexBufferDescriptor, 1> result;
  result[0].add_attribute(AttributeDescriptor::float4(0));
  result[0].add_attribute(AttributeDescriptor::float4(1));
  return result;
}

CubeMarchPushConstantData make_cube_march_push_constant_data(const Camera& camera) {
  CubeMarchPushConstantData result{};
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  result.projection_view = proj * camera.get_view();
  return result;
}

struct CubeMarchShadowPushConstantData {
  Mat4f projection_view;
};

CubeMarchShadowPushConstantData make_cube_march_shadow_push_constant_data(const Mat4f& proj_view) {
  CubeMarchShadowPushConstantData result{};
  result.projection_view = proj_view;
  return result;
}

struct TerrainGrassPushConstantData {
  Mat4f projection_view;
};

TerrainGrassPushConstantData make_terrain_grass_push_constant_data(const Camera& camera) {
  TerrainGrassPushConstantData result;
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  result.projection_view = proj * camera.get_view();
  return result;
}

std::array<VertexBufferDescriptor, 2> terrain_grass_buffer_descriptors() {
  std::array<VertexBufferDescriptor, 2> result;
  result[0].add_attribute(AttributeDescriptor::float2(0));
  result[1].add_attribute(AttributeDescriptor::float4(1, 1));
  result[1].add_attribute(AttributeDescriptor::float4(2, 1));
  return result;
}

struct Vertex {
  static VertexBufferDescriptor buffer_descriptor() {
    VertexBufferDescriptor result;
    result.add_attribute(AttributeDescriptor::float3(0));
    return result;
  }

  Vec3f position;
};

struct UniformData {
  Mat4f model;
  Mat4f view;
  Mat4f projection;
  Mat4f sun_light_view_projection0;
  Vec4f camera_position;
  Vec4f min_shadow_global_color_scale;
};

UniformData make_uniform_data(const Camera& camera,
                              const Mat4f& model,
                              const csm::CSMDescriptor& csm_desc,
                              float min_shadow,
                              float global_color_scale) {
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  UniformData result{};
  result.model = model;
  result.view = camera.get_view();
  result.projection = proj;
  result.sun_light_view_projection0 = csm_desc.light_shadow_sample_view;
  result.camera_position = Vec4f{camera.get_position(), 0.0f};
  result.min_shadow_global_color_scale = Vec4f{
    min_shadow, global_color_scale, 0.0f, 0.0f
  };
  return result;
}

Optional<glsl::VertFragProgramSource> create_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "terrain/terrain.vert";
  params.frag_file = "terrain/terrain.frag";
  params.compile.frag_defines.push_back(csm::make_num_sun_shadow_cascades_preprocessor_definition());
  params.reflect.to_vk_descriptor_type = [](const glsl::refl::DescriptorInfo& info) {
    if (info.is_storage_buffer()) {
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    } else if (info.is_uniform_buffer() && info.set == 0 && info.binding == 8) {
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    } else {
      return vk::refl::identity_descriptor_type(info);
    }
  };
  return glsl::make_vert_frag_program_source(params);
}

Result<Pipeline> create_pipeline(VkDevice device,
                                 const glsl::VertFragProgramSource& source,
                                 const vk::PipelineRenderPassInfo& pass_info,
                                 VkPipelineLayout layout) {
  auto buff_descr = Vertex::buffer_descriptor();
  VertexInputDescriptors input_descrs{};
  to_vk_vertex_input_descriptors(1, &buff_descr, &input_descrs);
  DefaultConfigureGraphicsPipelineStateParams params{input_descrs};
  params.num_color_attachments = 1;
  params.raster_samples = pass_info.raster_samples;
  params.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

  GraphicsPipelineStateCreateInfo state{};
  default_configure(&state, params);
  return create_vert_frag_graphics_pipeline(
    device, source.vert_bytecode, source.frag_bytecode,
    &state, layout, pass_info.render_pass, pass_info.subpass);
}

Optional<glsl::VertFragProgramSource> create_cube_march_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "terrain/cube-march.vert";
#if PREFER_SIMPLE_CUBE_MARCH
  params.frag_file = "terrain/cube-march-simple.frag";
#else
  params.frag_file = "terrain/cube-march.frag";
#endif
#if !USE_PUSH_DESCRIPTORS
  params.reflect.to_vk_descriptor_type = vk::refl::always_dynamic_uniform_buffer_descriptor_type;
#endif
  params.compile.frag_defines = csm::make_default_sample_shadow_preprocessor_definitions();
  params.compile.vert_defines = params.compile.frag_defines;
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_cube_march_shadow_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "terrain/cube-march-shadow.vert";
  params.frag_file = "shadow/empty.frag";
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_terrain_grass_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "terrain/terrain-grass.vert";
  params.frag_file = "terrain/terrain-grass.frag";
#if !USE_PUSH_DESCRIPTORS
  params.reflect.to_vk_descriptor_type = vk::refl::always_dynamic_uniform_buffer_descriptor_type;
#endif
  params.compile.frag_defines = csm::make_default_sample_shadow_preprocessor_definitions();
  params.compile.vert_defines = params.compile.frag_defines;
  return glsl::make_vert_frag_program_source(params);
}

template <typename GetSource, typename ConfigParams>
auto create_forward_pipeline_data(const InitInfo& info,
                                  const ArrayView<const VertexBufferDescriptor>& view_descs,
                                  const GetSource& get_source, ConfigParams&& config_params,
                                  glsl::VertFragProgramSource* dst_source,
                                  const PipelineSystem::RequireLayoutParams& layout_params) {
  auto create_pd = [&](VkDevice device, const glsl::VertFragProgramSource& source, VkPipelineLayout layout) {
    SimpleVertFragGraphicsPipelineCreateInfo create_info{};
    configure_pipeline_create_info(
      &create_info, view_descs, source, info.forward_pass_info, layout, config_params, {});
    return create_vert_frag_graphics_pipeline(device, &create_info);
  };
  return info.pipeline_system.create_pipeline_data(
    info.core.device.handle, get_source, create_pd, dst_source, layout_params);
}

auto create_cube_march_pipeline(const InitInfo& info, glsl::VertFragProgramSource* dst_source) {
  auto buff_descs = cube_march_buffer_descriptors();
  auto view_descs = make_data_array_view<const VertexBufferDescriptor>(buff_descs);
  auto get_source = []() { return create_cube_march_program_source(); };
  auto config = [](DefaultConfigureGraphicsPipelineStateParams& params) {
    params.num_color_attachments = 1;
  };
  PipelineSystem::RequireLayoutParams layout_params{};
#if USE_PUSH_DESCRIPTORS
  layout_params.enable_push_descriptors_in_descriptor_sets = true;
#endif
  return create_forward_pipeline_data(
    info, view_descs, get_source, std::move(config), dst_source, layout_params);
}

auto create_cube_march_shadow_pipeline(const InitInfo& info, glsl::VertFragProgramSource* dst_source) {
  auto get_source = []() { return create_cube_march_shadow_program_source(); };
  auto create_pd = [&info](VkDevice device, const glsl::VertFragProgramSource& source, VkPipelineLayout layout) {
    auto buff_descs = cube_march_buffer_descriptors();
    auto view_descs = make_data_array_view<const VertexBufferDescriptor>(buff_descs);
    SimpleVertFragGraphicsPipelineCreateInfo create_info{};
    auto config_params = [](DefaultConfigureGraphicsPipelineStateParams& params) {
      params.num_color_attachments = 0;
    };
    configure_pipeline_create_info(
      &create_info, view_descs, source, info.shadow_pass_info, layout, config_params, {});
    return create_vert_frag_graphics_pipeline(device, &create_info);
  };
  return info.pipeline_system.create_pipeline_data(
    info.core.device.handle, get_source, create_pd, dst_source);
}

auto create_terrain_grass_pipeline(const InitInfo& info, glsl::VertFragProgramSource* dst_source) {
  auto buff_descs = terrain_grass_buffer_descriptors();
  auto view_descs = make_data_array_view<const VertexBufferDescriptor>(buff_descs);
  auto get_source = []() { return create_terrain_grass_program_source(); };
  auto config = [](DefaultConfigureGraphicsPipelineStateParams& params) {
    params.num_color_attachments = 1;
    params.cull_mode = VK_CULL_MODE_NONE;
  };
  PipelineSystem::RequireLayoutParams layout_params{};
#if USE_PUSH_DESCRIPTORS
  layout_params.enable_push_descriptors_in_descriptor_sets = true;
#endif
  return create_forward_pipeline_data(
    info, view_descs, get_source, std::move(config), dst_source, layout_params);
}

using CubeMarchGeometries = std::unordered_map<uint32_t, TerrainRenderer::CubeMarchGeometry>;
auto draw_cube_march_geometries(const CubeMarchGeometries& geometries, uint32_t frame_index,
                                VkCommandBuffer cmd, const Optional<Frustum>& cull_against) {
  struct Result {
    uint32_t num_chunks_drawn;
    uint32_t num_vertices_drawn;
  };

  Result result{};
  for (auto& [_, geom] : geometries) {
    if (geom.num_vertices_active == 0) {
      continue;
    }

    if (cull_against && !frustum_aabb_intersect(cull_against.value(), geom.world_bound)) {
      continue;
    }

    VkBuffer vb = geom.geometry.get().contents().buffer.handle;
    const VkDeviceSize vb_off =
      frame_index * sizeof(CubeMarchVertex) * geom.num_vertices_reserved;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &vb_off);

    vk::DrawDescriptor cube_draw_desc;
    cube_draw_desc.num_instances = 1;
    cube_draw_desc.num_vertices = geom.num_vertices_active;
    cmd::draw(cmd, &cube_draw_desc);

    result.num_chunks_drawn++;
    result.num_vertices_drawn += geom.num_vertices_active;
  }

  return result;
}

template <typename T>
void draw_grass_instances(const T& instances, const TerrainRenderer::TerrainGrassGeometry& geom,
                          uint32_t frame_index, VkCommandBuffer cmd) {
  for (auto& [_, inst] : instances) {
    if (inst.num_instances == 0) {
      continue;
    }

    const VkBuffer vbs[2] = {
      geom.vertex.get().contents().buffer.handle,
      inst.buffer.get().contents().buffer.handle,
    };
    const VkDeviceSize vb_offs[2] = {
      0,
      frame_index * sizeof(TerrainRenderer::TerrainGrassInstance) * inst.num_instances_reserved
    };
    vkCmdBindVertexBuffers(cmd, 0, 2, vbs, vb_offs);

    auto ind_buff = geom.index.get().contents().buffer.handle;
    vkCmdBindIndexBuffer(cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);

    DrawIndexedDescriptor draw_desc;
    draw_desc.num_instances = inst.num_instances;
    draw_desc.num_indices = geom.num_indices;
    cmd::draw_indexed(cmd, &draw_desc);
  }
}

Optional<TerrainRenderer::TerrainGrassGeometry> create_terrain_grass_geometry(const InitInfo& info) {
  TerrainRenderer::TerrainGrassGeometry result;

  auto geom = geometry::quad_positions(false);
  auto inds = geometry::quad_indices();
  auto geom_size = geom.size() * sizeof(float);
  auto ind_size = inds.size() * sizeof(uint16_t);

  if (auto vert_buff = create_device_local_vertex_buffer_sync(
    info.allocator, geom_size, geom.data(), &info.core, &info.uploader)) {
    result.vertex = info.buffer_system.emplace(std::move(vert_buff.value));
  } else {
    return NullOpt{};
  }

  if (auto ind_buff = create_device_local_index_buffer_sync(
    info.allocator, ind_size, inds.data(), &info.core, &info.uploader)) {
    result.index = info.buffer_system.emplace(std::move(ind_buff.value));
    result.num_indices = uint32_t(inds.size());
  } else {
    return NullOpt{};
  }

  return Optional<TerrainRenderer::TerrainGrassGeometry>(std::move(result));
}

Optional<TerrainRenderer::Set0UniformBuffer> create_set0_uniform_buffer(const InitInfo& info) {
  TerrainRenderer::Set0UniformBuffer result{};
  size_t ignore_size{};
  auto buff = create_dynamic_uniform_buffer<Set0UniformBufferData>(
    info.allocator,
    &info.core.physical_device.info.properties,
    info.frame_queue_depth, &result.stride, &ignore_size);
  if (buff) {
    result.buffer = info.buffer_system.emplace(std::move(buff.value));
    return Optional<TerrainRenderer::Set0UniformBuffer>(std::move(result));
  } else {
    return NullOpt{};
  }
}

struct {
  gfx::PipelineHandle new_material_pipeline;
  gfx::PipelineHandle new_material_inverted_winding_pipeline;
  gfx::BufferHandle new_material_uniform_buffers[3];
} globals;

} //  anon

bool TerrainRenderer::is_valid() const {
  return pipeline_handle.get().is_valid();
}

void TerrainRenderer::require_desc_set_allocators(vk::DescriptorSystem& desc_system,
                                                  const glsl::VertFragProgramSource* sources,
                                                  int num_sources) {
  DescriptorPoolAllocator::PoolSizes pool_sizes;
  auto get_size = [](ShaderResourceType) { return 8; };
  for (int i = 0; i < num_sources; i++) {
    auto& src_binds = sources[i].descriptor_set_layout_bindings;
    push_pool_sizes_from_layout_bindings(pool_sizes, make_view(src_binds), get_size);
  }
  desc_pool_allocator = desc_system.create_pool_allocator(make_view(pool_sizes), 2);
  desc_set0_allocator = desc_system.create_set_allocator(desc_pool_allocator.get());
  cube_march_set0_allocator = desc_system.create_set_allocator(desc_pool_allocator.get());
  grass_set0_allocator = desc_system.create_set_allocator(desc_pool_allocator.get());
}

bool TerrainRenderer::make_program(const InitInfo& info, glsl::VertFragProgramSource* source) {
  auto source_res = create_program_source();
  if (!source_res) {
    return false;
  }

  VkDevice device_handle = info.core.device.handle;
  if (!info.pipeline_system.require_layouts(
    device_handle,
    make_view(source_res.value().push_constant_ranges),
    make_view(source_res.value().descriptor_set_layout_bindings),
    &pipeline_layout,
    &desc_set_layouts)) {
    return false;
  }

  auto pipe_res = create_pipeline(
    device_handle, source_res.value(), info.forward_pass_info, pipeline_layout);
  if (!pipe_res) {
    return false;
  } else {
    pipeline_handle = info.pipeline_system.emplace(std::move(pipe_res.value));
  }

  *source = std::move(source_res.value());
  return true;
}

void TerrainRenderer::remake_program(const InitInfo& info) {
  glsl::VertFragProgramSource sources[3];
  if (!make_program(info, &sources[0])) {
    return;
  }

  if (auto cube_pipe = create_cube_march_pipeline(info, &sources[1])) {
    cube_march_pipeline_data = std::move(cube_pipe.value());
  } else {
    return;
  }

  if (auto grass_pipe = create_terrain_grass_pipeline(info, &sources[2])) {
    grass_pipeline_data = std::move(grass_pipe.value());
  } else {
    return;
  }

  require_desc_set_allocators(info.desc_system, sources, 3);
}

bool TerrainRenderer::initialize(const InitInfo& info) {
  glsl::VertFragProgramSource sources[4];
  int num_sources{};
  if (!make_program(info, &sources[num_sources++])) {
    return false;
  }

  if (auto cube_pipe = create_cube_march_pipeline(info, &sources[num_sources++])) {
    cube_march_pipeline_data = std::move(cube_pipe.value());
  } else {
    return false;
  }

  if (auto cube_pipe = create_cube_march_shadow_pipeline(info, &sources[num_sources++])) {
    cube_march_shadow_pipeline_data = std::move(cube_pipe.value());
  } else {
    return false;
  }

  if (auto grass_pipe = create_terrain_grass_pipeline(info, &sources[num_sources++])) {
    grass_pipeline_data = std::move(grass_pipe.value());
  } else {
    return false;
  }

  if (auto buff = create_set0_uniform_buffer(info)) {
    set0_uniform_buffer = std::move(buff.value());
  } else {
    return false;
  }

  require_desc_set_allocators(info.desc_system, sources, num_sources);

  for (uint32_t i = 0; i < info.frame_queue_depth; i++) {
    auto& fd = frame_data.emplace_back();
    if (auto un_buff = create_uniform_buffer(info.allocator, sizeof(UniformData))) {
      fd.uniform_buffer = info.buffer_system.emplace(std::move(un_buff.value));
    } else {
      return false;
    }
    if (auto un_buff = create_uniform_buffer(info.allocator, sizeof(csm::SunCSMSampleData))) {
      fd.shadow_uniform_buffer = info.buffer_system.emplace(std::move(un_buff.value));
    } else {
      return false;
    }
  }

  {
    const int vertex_dim = 128;
    auto geom = geometry::triangle_strip_quad_positions(vertex_dim);
    auto inds = geometry::triangle_strip_indices(vertex_dim);
    auto geom_size = geom.size() * sizeof(float);
    auto ind_size = inds.size() * sizeof(uint16_t);

    draw_desc.num_indices = uint32_t(inds.size());
    draw_desc.num_instances = 1;

    if (auto vert_buff = create_device_local_vertex_buffer_sync(
      info.allocator,
      geom_size,
      geom.data(),
      &info.core,
      &info.uploader)) {
      vertex_buffer = info.buffer_system.emplace(std::move(vert_buff.value));
    } else {
      return false;
    }

    if (auto ind_buff = create_device_local_index_buffer_sync(
      info.allocator,
      ind_size,
      inds.data(),
      &info.core,
      &info.uploader)) {
      index_buffer = info.buffer_system.emplace(std::move(ind_buff.value));
    } else {
      return false;
    }
  }

  if (auto grass_geom = create_terrain_grass_geometry(info)) {
    grass_geometry = std::move(grass_geom.value());
  } else {
    return false;
  }

#if 1
  toggle_new_material_pipeline();
#endif

  return true;
}

void TerrainRenderer::terminate() {
  globals.new_material_pipeline = {};
  globals.new_material_inverted_winding_pipeline = {};
  for (auto& buff : globals.new_material_uniform_buffers) {
    buff = {};
  }
}

void TerrainRenderer::begin_frame(const BeginFrameInfo& info) {
  const auto shadow_un_data = csm::make_sun_csm_sample_data(info.csm_desc);
  {
    const float terrain_dim = render_params.terrain_dim;
    auto model = make_scale(Vec3f(terrain_dim * 0.5f, 1.0f, terrain_dim * 0.5f));

    auto& fd = frame_data[info.frame_index];
    auto un_data = make_uniform_data(
      info.camera,
      model,
      info.csm_desc,
      render_params.min_shadow,
      render_params.global_color_scale);
    fd.uniform_buffer.get().write(&un_data, sizeof(un_data));
    fd.shadow_uniform_buffer.get().write(&shadow_un_data, sizeof(shadow_un_data));
  }

  for (auto& [_, geom] : cube_march_geometries) {
    if (geom.modified[info.frame_index]) {
      size_t sz{};
      const void* data{};
      geom.get_geometry_data(&data, &sz);

      const size_t vert_size = sizeof(CubeMarchVertex);
      assert(sz % vert_size == 0 && sz / vert_size <= geom.num_vertices_reserved);
      const size_t off = geom.num_vertices_reserved * vert_size * info.frame_index;

      geom.num_vertices_active = uint32_t(sz / vert_size);
      if (geom.num_vertices_active > 0) {
        geom.geometry.get().write(data, sz, off);
      }
      geom.modified[info.frame_index] = false;
    }
  }

  for (auto& [_, inst] : grass_instance_buffers) {
    if (inst.modified[info.frame_index]) {
      const size_t inst_size = sizeof(TerrainGrassInstance);
      const size_t off = inst.num_instances_reserved * inst_size * info.frame_index;
      const size_t size = inst_size * inst.num_instances;
      if (size > 0) {
        inst.buffer.get().write(inst.cpu_data.data(), size, off);
      }
      inst.modified[info.frame_index] = false;
    }
  }

  {
    auto elapsed_time = float(stopwatch.delta().count());
    auto set0_data = make_set0_uniform_buffer_data(
      shadow_un_data, info.csm_desc, info.camera, render_params, elapsed_time);
    const auto off = set0_uniform_buffer.stride * info.frame_index;
    set0_uniform_buffer.buffer.get().write(&set0_data, sizeof(set0_data), off);
  }

  if (set_pcf_enabled) {
    pcf_enabled = set_pcf_enabled.value();
    need_create_new_material_pipeline = true;
    set_pcf_enabled = NullOpt{};
  }

  if (prefer_new_material_pipeline && need_create_new_material_pipeline) {
    auto source = [this]() {
      glsl::LoadVertFragProgramSourceParams params{};
      params.vert_file = "terrain/terrain-new-material.vert";
      params.frag_file = "terrain/terrain-new-material.frag";
      params.compile.frag_defines.push_back(csm::make_num_sun_shadow_cascades_preprocessor_definition());
      if (!pcf_enabled) {
        params.compile.frag_defines.push_back(glsl::make_define("NO_PCF"));
      }
      return glsl::make_vert_frag_program_source(params);
    }();
    auto pass = gfx::get_forward_write_back_render_pass_handle(&info.context);
    if (pass && source) {
      auto copy_src = source.value();

      auto buff_desc = Vertex::buffer_descriptor();
      gfx::GraphicsPipelineCreateInfo create_info{};
      create_info.num_vertex_buffer_descriptors = 1;
      create_info.vertex_buffer_descriptors = &buff_desc;
      create_info.num_color_attachments = 1;
      create_info.primitive_topology = gfx::PrimitiveTopology::TriangleStrip;
      auto pipe = gfx::create_pipeline(
        &info.context, std::move(source.value()), create_info, pass.value());
      if (pipe) {
        globals.new_material_pipeline = std::move(pipe.value());
      }

      create_info.cull_mode = gfx::CullMode::Front;
      auto pipe_cull_front = gfx::create_pipeline(
        &info.context, std::move(copy_src), create_info, pass.value());
      if (pipe_cull_front) {
        globals.new_material_inverted_winding_pipeline = std::move(pipe_cull_front.value());
      }
    }
    need_create_new_material_pipeline = false;
  }

  if (info.frame_index < 3) {
    auto& un_buff = globals.new_material_uniform_buffers[info.frame_index];
    if (!un_buff.is_valid()) {
      auto maybe_un_buff = gfx::create_uniform_buffer(
        &info.context, sizeof(NewGrassRendererMaterialData));
      if (maybe_un_buff) {
        un_buff = std::move(maybe_un_buff.value());
      }
    }
    if (un_buff.is_valid()) {
      un_buff.write(&info.grass_material_data, sizeof(NewGrassRendererMaterialData));
    }
  }
}

namespace {

Optional<SampledImageManager::ReadInstance>
get_2d_image(const SampledImageManager& manager,
             const Optional<SampledImageManager::Handle>& handle) {
  if (handle) {
    if (auto color_im = manager.get(handle.value())) {
      if (color_im.value().is_2d() && color_im.value().fragment_shader_sample_ok()) {
        return color_im;
      } else {
        GROVE_ASSERT(false);
      }
    }
  }
  return NullOpt{};
}

} //  anon

Optional<vk::SampledImageManager::ReadInstance>
TerrainRenderer::get_color_image(const vk::SampledImageManager& manager) {
  return get_2d_image(manager, color_image_handle);
}

Optional<vk::SampledImageManager::ReadInstance>
TerrainRenderer::get_new_material_image(const vk::SampledImageManager& manager) {
  return get_2d_image(manager, new_material_image_handle);
}

Optional<vk::SampledImageManager::ReadInstance>
TerrainRenderer::get_alt_color_image(const vk::SampledImageManager& manager) {
  return get_2d_image(manager, alt_color_image_handle);
}

Optional<vk::SampledImageManager::ReadInstance>
TerrainRenderer::get_splotch_image(const vk::SampledImageManager& manager) {
  return get_2d_image(manager, splotch_image_handle);
}

Optional<vk::DynamicSampledImageManager::ReadInstance>
TerrainRenderer::get_height_image(const vk::DynamicSampledImageManager& manager) {
  if (height_map_image_handle) {
    if (auto height_im = manager.get(height_map_image_handle.value())) {
      if (height_im.value().is_2d() && height_im.value().vertex_shader_sample_ok()) {
        return height_im;
      } else {
        GROVE_ASSERT(false);
      }
    }
  }
  return NullOpt{};
}

Optional<vk::DynamicSampledImageManager::ReadInstance>
TerrainRenderer::get_wind_displacement_image(const vk::DynamicSampledImageManager& manager) {
  if (wind_displacement_image_handle) {
    if (auto wind_im = manager.get(wind_displacement_image_handle.value())) {
      if (wind_im.value().is_2d() && wind_im.value().vertex_shader_sample_ok()) {
        return wind_im;
      } else {
        GROVE_ASSERT(false);
      }
    }
  }
  return NullOpt{};
}

void TerrainRenderer::render_new_material(const RenderInfo& info) {
  auto& pipe = prefer_inverted_winding_new_material_pipeline ?
    globals.new_material_inverted_winding_pipeline : globals.new_material_pipeline;

  if (!pipe.is_valid()) {
    return;
  }

  if (info.frame_index >= 3 || !globals.new_material_uniform_buffers[info.frame_index].is_valid()) {
    return;
  }

  auto color_im = get_new_material_image(info.sampled_image_manager);
  auto height_im = get_height_image(info.dynamic_sampled_image_manager);
  if (!color_im || !height_im) {
    return;
  }

  const auto& fd = frame_data[info.frame_index];
  auto& mat_buff = globals.new_material_uniform_buffers[info.frame_index];

  auto height_sampler = info.sampler_system.require_linear_edge_clamp(info.core.device.handle);
  auto color_sampler = height_sampler;
  auto shadow_sampler = height_sampler;

  DescriptorSetScaffold scaffold;
  uint32_t binding{};
  scaffold.set = 0;
  push_combined_image_sampler(  //  height map
    scaffold, binding++, height_im.value().view, height_sampler, height_im.value().layout);
  push_uniform_buffer(  //  main uniform buffer
    scaffold, binding++, fd.uniform_buffer.get());
  push_combined_image_sampler(  //  splotch texture
    scaffold, binding++, color_im.value().view, color_sampler, color_im.value().layout);
  push_uniform_buffer(  //  shadow data
    scaffold, binding++, fd.shadow_uniform_buffer.get());
  push_combined_image_sampler(  //  shadow texture
    scaffold, binding++, info.shadow_image, shadow_sampler);
  push_uniform_buffer(  //  new material data
    scaffold, binding++, mat_buff.get(), sizeof(NewGrassRendererMaterialData));

  auto desc_set = gfx::require_updated_descriptor_set(&info.context, scaffold, pipe);
  if (!desc_set) {
    return;
  }

  VkBuffer vb = vertex_buffer.get().contents().buffer.handle;
  const VkDeviceSize vb_off{};

  cmd::bind_graphics_pipeline(info.cmd, pipe.get());
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  cmd::bind_graphics_descriptor_sets(
    info.cmd, pipe.get_layout(), 0, 1, &desc_set.value());
  vkCmdBindIndexBuffer(
    info.cmd, index_buffer.get().contents().buffer.handle, 0, VK_INDEX_TYPE_UINT16);
  vkCmdBindVertexBuffers(info.cmd, 0, 1, &vb, &vb_off);
  cmd::draw_indexed(info.cmd, &draw_desc);
}

void TerrainRenderer::render(const RenderInfo& info) {
  latest_num_cube_march_vertices_drawn = 0;
  latest_num_cube_march_chunks_drawn = 0;

  if (disabled) {
    return;
  }

  auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "render_terrain");
  (void) profiler;

  if (prefer_new_material_pipeline) {
    render_new_material(info);
  } else {
    render_original(info);
  }

  if (!hide_cube_map_geometries) {
    render_cube_march(info);
  }
  render_grass(info);
}

bool TerrainRenderer::any_cube_march_active() const {
  for (auto& [_, geom] : cube_march_geometries) {
    if (geom.num_vertices_active > 0) {
      return true;
    }
  }
  return false;
}

bool TerrainRenderer::any_grass_active() const {
  for (auto& [_, inst] : grass_instance_buffers) {
    if (inst.num_instances > 0) {
      return true;
    }
  }
  return false;
}

void TerrainRenderer::render_shadow(const ShadowRenderInfo& info) {
  if (hide_cube_map_geometries) {
    return;
  }

  const auto& pd = cube_march_shadow_pipeline_data;
  if (!pd.pipeline.is_valid() || !any_cube_march_active()) {
    return;
  }

  cmd::bind_graphics_pipeline(info.cmd, pd.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);

  auto pc = make_cube_march_shadow_push_constant_data(info.light_view_proj);
  auto stages = VK_SHADER_STAGE_VERTEX_BIT;
  cmd::push_constants(info.cmd, pd.layout, stages, &pc);
  draw_cube_march_geometries(cube_march_geometries, info.frame_index, info.cmd, NullOpt{});
}

void TerrainRenderer::render_cube_march(const RenderInfo& info) {
  const auto& pd = cube_march_pipeline_data;
  if (!pd.pipeline.is_valid() || !any_cube_march_active()) {
    return;
  }

#if !PREFER_SIMPLE_CUBE_MARCH
  auto color_im = get_alt_color_image(info.sampled_image_manager);
  if (!color_im) {
    return;
  }

  auto splotch_im = get_splotch_image(info.sampled_image_manager);
  if (!splotch_im) {
    return;
  }
#endif

#if !USE_PUSH_DESCRIPTORS
  vk::DescriptorSetAllocator* desc_set0_alloc{};
  vk::DescriptorPoolAllocator* desc_pool_alloc{};
  if (!info.desc_system.get(desc_pool_allocator.get(), &desc_pool_alloc) ||
      !info.desc_system.get(cube_march_set0_allocator.get(), &desc_set0_alloc)) {
    return;
  }
#endif

  cmd::bind_graphics_pipeline(info.cmd, pd.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);

  VkDevice device_handle = info.core.device.handle;
#if USE_PUSH_DESCRIPTORS
  {
    const auto un0_off = uint32_t(info.frame_index * set0_uniform_buffer.stride);

    auto linear = info.sampler_system.require_linear_edge_clamp(device_handle);
    auto repeat = info.sampler_system.require_linear_repeat(device_handle);

    vk::DescriptorSetScaffold scaffold{};
    uint32_t bind{};
    push_uniform_buffer(
      scaffold, bind++, set0_uniform_buffer.buffer.get(), sizeof(Set0UniformBufferData), un0_off);
    push_combined_image_sampler(
      scaffold, bind++, info.shadow_image, linear);
    push_combined_image_sampler(
      scaffold, bind++, splotch_im.value().to_sample_image_view(), repeat);
    push_combined_image_sampler(
      scaffold, bind++, color_im.value().to_sample_image_view(), repeat);

    DescriptorWrites<32> writes{};
    make_descriptor_writes(writes, VK_NULL_HANDLE, scaffold);
    cmd::push_graphics_descriptor_set(info.core, info.cmd, pd.layout, 0, writes);
  }
#else
  VkDescriptorSet desc_set0{};
  {
    auto linear = info.sampler_system.require_linear_edge_clamp(device_handle);

    vk::DescriptorSetScaffold scaffold{};
    uint32_t bind{};
    push_dynamic_uniform_buffer(
      scaffold, bind++, set0_uniform_buffer.buffer.get(), sizeof(Set0UniformBufferData));
    push_combined_image_sampler(
      scaffold, bind++, info.shadow_image, linear);
#if !PREFER_SIMPLE_CUBE_MARCH
    auto repeat = info.sampler_system.require_linear_repeat(device_handle);
    push_combined_image_sampler(
      scaffold, bind++, splotch_im.value().to_sample_image_view(), repeat);
    push_combined_image_sampler(
      scaffold, bind++, color_im.value().to_sample_image_view(), repeat);
#endif

    if (auto desc_res = desc_set0_alloc->require_updated_descriptor_set(
      device_handle, *pd.descriptor_set_layouts.find(0), *desc_pool_alloc, scaffold)) {
      desc_set0 = desc_res.value;
    } else {
      assert(false);
      return;
    }
  }

  const uint32_t dyn_offs[1] = {
    uint32_t(info.frame_index * set0_uniform_buffer.stride)
  };

  cmd::bind_graphics_descriptor_sets(info.cmd, pd.layout, 0, 1, &desc_set0, 1, dyn_offs);
#endif

  const auto cull_frust = info.camera.make_world_space_frustum(512.0f);

  auto pc = make_cube_march_push_constant_data(info.camera);
  auto stages = VK_SHADER_STAGE_VERTEX_BIT;
  cmd::push_constants(info.cmd, pd.layout, stages, &pc);
  auto stats = draw_cube_march_geometries(
    cube_march_geometries, info.frame_index, info.cmd, Optional<Frustum>(cull_frust));

  latest_num_cube_march_vertices_drawn = stats.num_vertices_drawn;
  latest_num_cube_march_chunks_drawn = stats.num_chunks_drawn;
}

void TerrainRenderer::render_grass(const RenderInfo& info) {
  const auto& pd = grass_pipeline_data;
  if (!pd.pipeline.is_valid() || !any_grass_active()) {
    return;
  }

  auto wind_im = get_wind_displacement_image(info.dynamic_sampled_image_manager);
  if (!wind_im) {
    return;
  }

#if !USE_PUSH_DESCRIPTORS
  vk::DescriptorSetAllocator* desc_set0_alloc{};
  vk::DescriptorPoolAllocator* desc_pool_alloc{};
  if (!info.desc_system.get(desc_pool_allocator.get(), &desc_pool_alloc) ||
      !info.desc_system.get(grass_set0_allocator.get(), &desc_set0_alloc)) {
    return;
  }
#endif

  cmd::bind_graphics_pipeline(info.cmd, pd.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);

  VkDevice device_handle = info.core.device.handle;
#if USE_PUSH_DESCRIPTORS
  {
    const auto set0_un_off = uint32_t(info.frame_index * set0_uniform_buffer.stride);
    auto sampler_linear = info.sampler_system.require_linear_edge_clamp(device_handle);
    vk::DescriptorSetScaffold scaffold{};
    uint32_t bind{};
    push_uniform_buffer(
      scaffold, bind++, set0_uniform_buffer.buffer.get(), sizeof(Set0UniformBufferData), set0_un_off);
    push_combined_image_sampler(
      scaffold, bind++, info.shadow_image, sampler_linear);
    push_combined_image_sampler(
      scaffold, bind++, wind_im.value().to_sample_image_view(), sampler_linear);

    DescriptorWrites<32> writes;
    make_descriptor_writes(writes, VK_NULL_HANDLE, scaffold);
    cmd::push_graphics_descriptor_set(info.core, info.cmd, pd.layout, 0, writes);
  }
#else
  VkDescriptorSet desc_set0{};
  {
    auto sampler_linear = info.sampler_system.require_linear_edge_clamp(device_handle);
    vk::DescriptorSetScaffold scaffold{};
    uint32_t bind{};
    push_dynamic_uniform_buffer(
      scaffold, bind++, set0_uniform_buffer.buffer.get(), sizeof(Set0UniformBufferData));
    push_combined_image_sampler(
      scaffold, bind++, info.shadow_image, sampler_linear);
    push_combined_image_sampler(
      scaffold, bind++, wind_im.value().to_sample_image_view(), sampler_linear);

    if (auto desc_res = desc_set0_alloc->require_updated_descriptor_set(
      device_handle, *pd.descriptor_set_layouts.find(0), *desc_pool_alloc, scaffold)) {
      desc_set0 = desc_res.value;
    } else {
      assert(false);
      return;
    }
  }

  const uint32_t dyn_offs[1] = {
    uint32_t(info.frame_index * set0_uniform_buffer.stride)
  };

  cmd::bind_graphics_descriptor_sets(info.cmd, pd.layout, 0, 1, &desc_set0, 1, dyn_offs);
#endif

  auto pc = make_terrain_grass_push_constant_data(info.camera);
  auto stages = VK_SHADER_STAGE_VERTEX_BIT;
  cmd::push_constants(info.cmd, pd.layout, stages, &pc);
  draw_grass_instances(grass_instance_buffers, grass_geometry, info.frame_index, info.cmd);
}

void TerrainRenderer::render_original(const RenderInfo& info) {
  auto color_im = get_color_image(info.sampled_image_manager);
  auto height_im = get_height_image(info.dynamic_sampled_image_manager);
  if (!color_im || !height_im) {
    return;
  }

  DescriptorPoolAllocator* pool_alloc{};
  DescriptorSetAllocator* set0_alloc{};
  if (!info.desc_system.get(desc_pool_allocator.get(), &pool_alloc) ||
      !info.desc_system.get(desc_set0_allocator.get(), &set0_alloc)) {
    return;
  }

  uint32_t set0_dyn_offs[16];
  uint32_t num_dyn_offs{};

  const auto& fd = frame_data[info.frame_index];
  auto device_handle = info.core.device.handle;
  VkDescriptorSet desc_set;
  {
    auto height_sampler = info.sampler_system.require_linear_edge_clamp(device_handle);
    auto color_sampler = height_sampler;
    auto shadow_sampler = height_sampler;

    DescriptorSetScaffold scaffold;
    uint32_t binding{};
    scaffold.set = 0;
    push_combined_image_sampler(  //  height map
      scaffold, binding++, height_im.value().view, height_sampler, height_im.value().layout);
    push_uniform_buffer(  //  main uniform buffer
      scaffold, binding++, fd.uniform_buffer.get());
    push_combined_image_sampler(  //  color texture
      scaffold, binding++, color_im.value().view, color_sampler, color_im.value().layout);
    push_uniform_buffer(  //  shadow data
      scaffold, binding++, fd.shadow_uniform_buffer.get());
    push_combined_image_sampler(  //  shadow texture
      scaffold, binding++, info.shadow_image, shadow_sampler);

    if (auto desc_res = set0_alloc->require_updated_descriptor_set(
      device_handle, *desc_set_layouts.find(0), *pool_alloc, scaffold)) {
      desc_set = desc_res.value;
    } else {
      assert(false);
      return;
    }
  }

  VkBuffer vb = vertex_buffer.get().contents().buffer.handle;
  const VkDeviceSize vb_off{};

  cmd::bind_graphics_pipeline(info.cmd, pipeline_handle.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
  cmd::bind_graphics_descriptor_sets(
    info.cmd, pipeline_layout, 0, 1, &desc_set, num_dyn_offs, set0_dyn_offs);
  vkCmdBindIndexBuffer(
    info.cmd, index_buffer.get().contents().buffer.handle, 0, VK_INDEX_TYPE_UINT16);
  vkCmdBindVertexBuffers(info.cmd, 0, 1, &vb, &vb_off);
  cmd::draw_indexed(info.cmd, &draw_desc);
}

void TerrainRenderer::destroy_chunk(CubeMarchChunkHandle handle) {
  cube_march_geometries.erase(handle.id);
}

bool TerrainRenderer::require_chunk(const AddResourceContext& context,
                                    CubeMarchChunkHandle* handle, uint32_t num_reserve,
                                    GetGeometryData&& get_data, const Bounds3f& world_bound) {
  if (!handle->is_valid()) {
    *handle = CubeMarchChunkHandle{next_cube_march_chunk_id++};
    cube_march_geometries[handle->id] = {};
  }

  auto& geom = cube_march_geometries.at(handle->id);
  geom.world_bound = world_bound;
  geom.get_geometry_data = std::move(get_data);
  if (geom.num_vertices_reserved >= num_reserve) {
    return true;
  }

  auto size = sizeof(CubeMarchVertex) * num_reserve * context.frame_queue_depth;
  auto buff = create_host_visible_vertex_buffer(context.allocator, size);
  if (!buff) {
    return false;
  } else {
    geom.geometry = context.buffer_system.emplace(std::move(buff.value));
    geom.num_vertices_reserved = num_reserve;
  }

  for (uint32_t i = 0; i < context.frame_queue_depth; i++) {
    geom.modified[i] = true;
  }

  return true;
}

void TerrainRenderer::set_chunk_modified(const AddResourceContext& context,
                                         CubeMarchChunkHandle chunk) {
  assert(cube_march_geometries.count(chunk.id));
  auto& geom = cube_march_geometries.at(chunk.id);
  for (uint32_t i = 0; i < context.frame_queue_depth; i++) {
    geom.modified[i] = true;
  }
}

bool TerrainRenderer::reserve(const AddResourceContext& context, TerrainGrassDrawableHandle* handle,
                              uint32_t num_instances) {
  if (!handle->is_valid()) {
    *handle = TerrainGrassDrawableHandle{next_grass_instance_buffer_id++};
    grass_instance_buffers[handle->id] = {};
  }

  auto& inst_buff = grass_instance_buffers.at(handle->id);
  if (inst_buff.num_instances_reserved >= num_instances) {
    return true;
  }

  auto size = sizeof(TerrainGrassInstance) * num_instances * context.frame_queue_depth;
  auto buff = create_host_visible_vertex_buffer(context.allocator, size);
  if (!buff) {
    return false;
  } else {
    inst_buff.buffer = context.buffer_system.emplace(std::move(buff.value));
    inst_buff.num_instances_reserved = num_instances;
    inst_buff.cpu_data.resize(sizeof(TerrainGrassInstance) * num_instances);
  }

  for (uint32_t i = 0; i < context.frame_queue_depth; i++) {
    inst_buff.modified[i] = true;
  }

  return true;
}

void TerrainRenderer::set_instances(const AddResourceContext& context,
                                    TerrainGrassDrawableHandle handle,
                                    const TerrainGrassInstance* instances, uint32_t num_instances) {
  auto& inst_buff = grass_instance_buffers.at(handle.id);
  assert(inst_buff.num_instances_reserved >= num_instances);
  memcpy(inst_buff.cpu_data.data(), instances, sizeof(TerrainGrassInstance) * num_instances);
  inst_buff.num_instances = num_instances;

  for (uint32_t i = 0; i < context.frame_queue_depth; i++) {
    inst_buff.modified[i] = true;
  }
}

TerrainRenderer::AddResourceContext
TerrainRenderer::make_add_resource_context(vk::GraphicsContext& graphics_context) {
  return TerrainRenderer::AddResourceContext{
    graphics_context.frame_queue_depth,
    graphics_context.core,
    &graphics_context.allocator,
    graphics_context.buffer_system
  };
}

void TerrainRenderer::toggle_new_material_pipeline() {
  if (prefer_new_material_pipeline) {
    prefer_new_material_pipeline = false;
  } else {
    prefer_new_material_pipeline = true;
    if (!globals.new_material_pipeline.is_valid()) {
      need_create_new_material_pipeline = true;
    }
  }
}

GROVE_NAMESPACE_END
