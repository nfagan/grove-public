#include "render_branch_nodes_gpu.hpp"
#include "render_branch_nodes_types.hpp"
#include "graphics.hpp"
#include "occlusion_cull_gpu.hpp"
#include "frustum_cull_types.hpp"
#include "../vk/vk.hpp"
#include "DynamicSampledImageManager.hpp"
#include "shadow.hpp"
#include "debug_label.hpp"
#include "../procedural_flower/geometry.hpp"
#include "csm.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/common/common.hpp"
#include <bitset>
#include <numeric>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace vk;
using namespace tree;

using BeginFrameInfo = RenderBranchNodesBeginFrameInfo;
using EarlyComputeInfo = RenderBranchNodesEarlyGraphicsComputeInfo;
using RenderForwardInfo = RenderBranchNodesRenderForwardInfo;
using RenderShadowInfo = RenderBranchNodesRenderShadowInfo;
using IndirectDrawCommand = VkDrawIndexedIndirectCommand;

struct ForwardPushConstantData {
  Mat4f projection_view;
};

struct ShadowPushConstantData {
  Mat4f projection_view;
};

struct UniformBufferData {
  Vec4f num_points_xz_t;
  Vec4f wind_displacement_info;  //  vec2 wind_displacement_limits, vec2 wind_strength_limits
  Vec4f wind_world_bound_xz;
  //  Shadow info.
  Mat4f view;
  Mat4f sun_light_view_projection0;
  Vec4f shadow_info; //  min_radius_shadow, max_radius_scale_shadow, unused, unused
  //  Frag info.
  Vec4f sun_position;
  Vec4f sun_color;
  Vec4f camera_position;
  Vec4f color;
  csm::SunCSMSampleData sun_csm_sample_data;
};

struct LODOutputData {
  uint32_t lod_index;
  uint32_t unused_reserved1;
  uint32_t unused_reserved2;
  uint32_t unused_reserved3;
};

struct DynamicArrayBuffer {
  gfx::BufferHandle buffer;
  uint32_t num_reserved{};
  uint32_t num_active{};
};

struct GeometryBuffer {
  gfx::BufferHandle geom;
  gfx::BufferHandle index;
  uint32_t num_indices{};
  bool is_valid{};
};

struct InstanceBuffers {
  void require(uint32_t frame_queue_depth) {
    aggregate_buffers.resize(int64_t(frame_queue_depth));
    dynamic_buffers.resize(int64_t(frame_queue_depth));
    static_buffers.resize(int64_t(frame_queue_depth));
    lod_data_buffers.resize(int64_t(frame_queue_depth));
    instance_indices.resize(int64_t(frame_queue_depth));
  }

  DynamicArray<DynamicArrayBuffer, 3> aggregate_buffers;
  DynamicArray<DynamicArrayBuffer, 3> dynamic_buffers;
  DynamicArray<DynamicArrayBuffer, 3> static_buffers;
  DynamicArray<DynamicArrayBuffer, 3> lod_data_buffers;
  DynamicArray<DynamicArrayBuffer, 3> instance_indices;
  std::vector<uint32_t> tmp_cpu_indices;

  std::bitset<32> dynamic_modified{};
  std::bitset<32> static_modified{};
  std::bitset<32> aggregates_modified{};
  std::bitset<32> indices_modified{};
  std::bitset<32> lod_data_modified{};
  bool buffers_valid{};
};

struct LODDeviceComputeBuffers {
  DynamicArrayBuffer lod_output_data;
  DynamicArrayBuffer draw_indices;
  DynamicArray<gfx::BufferHandle, 3> draw_commands;
  bool is_valid{};
};

struct GPUContext {
  GridGeometryParams lod0_geom_params{7, 2};
  GridGeometryParams lod1_geom_params{5, 2};

  GeometryBuffer lod0_geometry_buffer;
  GeometryBuffer lod1_geometry_buffer;
  GeometryBuffer quad_geometry_buffer;
  bool use_lod1_geometry{};

  gfx::DynamicUniformBuffer uniform_buffer;
  InstanceBuffers base_instance_buffers;
  InstanceBuffers wind_instance_buffers;
  LODDeviceComputeBuffers base_lod_compute_buffers;
  LODDeviceComputeBuffers wind_lod_compute_buffers;

  gfx::PipelineHandle forward_base_pipeline;
  gfx::PipelineHandle forward_wind_pipeline;
  gfx::PipelineHandle shadow_base_pipeline;
  gfx::PipelineHandle shadow_wind_pipeline;

  gfx::PipelineHandle quad_forward_pipeline;
  gfx::PipelineHandle quad_shadow_pipeline;

  gfx::PipelineHandle gen_lod_indices_occlusion_cull_pipeline;
  gfx::PipelineHandle gen_lod_indices_frustum_cull_pipeline;
  gfx::PipelineHandle gen_draw_list_pipeline;

  Optional<VkDescriptorSet> base_forward_desc_set0;
  Optional<VkDescriptorSet> wind_forward_desc_set0;
  Optional<VkDescriptorSet> base_shadow_desc_set0;
  Optional<VkDescriptorSet> wind_shadow_desc_set0;
  Optional<VkDescriptorSet> quad_wind_desc_set0;
  Optional<VkDescriptorSet> quad_wind_shadow_desc_set0;
  Optional<VkDescriptorSet> quad_base_desc_set0;
  Optional<VkDescriptorSet> quad_base_shadow_desc_set0;

  RenderBranchNodesRenderParams render_params{};
  Optional<DynamicSampledImageManager::Handle> wind_image;

  IndirectDrawCommand prev_base_indirect_draw_command{};
  IndirectDrawCommand prev_wind_indirect_draw_command{};

  uint32_t max_shadow_cascade_index{0};
  int compute_local_size_x{32};

  Optional<bool> set_use_lod1_geometry;

  bool prefer_indirect_pipeline{true};
  bool base_lod_data_potentially_invalidated{};
  bool wind_lod_data_potentially_invalidated{};
  bool generated_base_indirect_draw_list{};
  bool generated_wind_indirect_draw_list{};
  bool generated_base_indirect_draw_list_with_occlusion_culling{};
  bool generated_wind_indirect_draw_list_with_occlusion_culling{};
  bool rendered_base_forward_with_occlusion_culling{};
  bool rendered_wind_forward_with_occlusion_culling{};

  bool disable_wind_drawables{};
  bool disable_base_drawables{};
  bool disable_wind_shadow{};
  bool disable_base_shadow{};

  Optional<bool> set_render_base_as_quads;
  Optional<bool> set_render_wind_as_quads;

  bool render_base_as_quads{};
  bool render_wind_as_quads{};

  bool tried_initialize{};
  bool pipelines_valid{};
  bool disabled{};
  bool began_frame{};

  bool gui_feedback_did_render_base_with_occlusion_culling{};
  bool gui_feedback_did_render_wind_with_occlusion_culling{};
};

constexpr Vec3f default_branch_color() {
  return Vec3f{0.47f, 0.26f, 0.02f};
}

constexpr float min_radius_shadow() {
  return 0.1f;
}

constexpr float max_radius_scale_shadow() {
  //  return 8.0f;
  return 1.0f;
}

Vec2f num_grid_points_xz(const GridGeometryParams& params) {
  return {float(params.num_pts_x), float(params.num_pts_z)};
}

UniformBufferData make_uniform_buffer_data(const GPUContext& context, const Camera& camera,
                                           const csm::CSMDescriptor& csm_desc,
                                           const GridGeometryParams& geom_params) {
  auto& rp = context.render_params;
  auto np_xz = num_grid_points_xz(geom_params);
  UniformBufferData result{};
  result.num_points_xz_t = Vec4f{np_xz.x, np_xz.y, rp.elapsed_time, 0.0f};
  result.wind_displacement_info = Vec4f{
    rp.wind_displacement_limits.x, rp.wind_displacement_limits.y,
    rp.wind_strength_limits.x, rp.wind_strength_limits.y
  };
  result.wind_world_bound_xz = rp.wind_world_bound_xz;
  result.view = camera.get_view();
  result.sun_light_view_projection0 = csm_desc.light_shadow_sample_view;
  result.shadow_info = Vec4f{min_radius_shadow(), max_radius_scale_shadow(), 0.0f, 0.0f};
  result.sun_position = Vec4f{rp.sun_position, 0.0f};
  result.sun_color = Vec4f{rp.sun_color, 0.0f};
  result.camera_position = Vec4f{camera.get_position(), 0.0f};
  result.color = Vec4f{default_branch_color(), 0.0f};
  result.sun_csm_sample_data = csm::make_sun_csm_sample_data(csm_desc);
  return result;
}

ForwardPushConstantData make_forward_push_constant_data(const GPUContext&, const Camera& camera) {
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  ForwardPushConstantData result{};
  result.projection_view = proj * camera.get_view();
  return result;
}

ShadowPushConstantData make_shadow_push_constant_data(const Mat4f& proj_view) {
  ShadowPushConstantData result{};
  result.projection_view = proj_view;
  return result;
}

Optional<gfx::DynamicUniformBuffer> create_uniform_buffer(const BeginFrameInfo& info) {
  return gfx::create_dynamic_uniform_buffer<UniformBufferData>(
    info.graphics_context, info.frame_queue_depth);
}

Optional<GeometryBuffer> create_quad_geometry_buffer(const BeginFrameInfo& info) {
  const auto pos = geometry::quad_positions(false);
  const auto inds = geometry::quad_indices();

  auto geom_buffer = gfx::create_device_local_vertex_buffer_sync(
    info.graphics_context, pos.size() * sizeof(float), pos.data());
  if (!geom_buffer) {
    return NullOpt{};
  }
  auto ind_buffer = gfx::create_device_local_index_buffer_sync(
    info.graphics_context, inds.size() * sizeof(uint16_t), inds.data());
  if (!ind_buffer) {
    return NullOpt{};
  }

  GeometryBuffer result{};
  result.geom = std::move(geom_buffer.value());
  result.index = std::move(ind_buffer.value());
  result.num_indices = uint32_t(inds.size());
  result.is_valid = true;
  return Optional<GeometryBuffer>(std::move(result));
}

Optional<GeometryBuffer> create_geometry_buffer(const GridGeometryParams& geometry_params,
                                                const BeginFrameInfo& info) {
  const auto pos = make_reflected_grid_indices(
    geometry_params.num_pts_x, geometry_params.num_pts_z);
  const auto inds = triangulate_reflected_grid(
    geometry_params.num_pts_x, geometry_params.num_pts_z);

  auto geom_buffer = gfx::create_device_local_vertex_buffer_sync(
    info.graphics_context, pos.size() * sizeof(float), pos.data());
  if (!geom_buffer) {
    return NullOpt{};
  }
  auto ind_buffer = gfx::create_device_local_index_buffer_sync(
    info.graphics_context, inds.size() * sizeof(uint16_t), inds.data());
  if (!ind_buffer) {
    return NullOpt{};
  }

  GeometryBuffer result{};
  result.geom = std::move(geom_buffer.value());
  result.index = std::move(ind_buffer.value());
  result.num_indices = uint32_t(inds.size());
  result.is_valid = true;
  return Optional<GeometryBuffer>(std::move(result));
}

Optional<glsl::VertFragProgramSource> create_quad_program_source(bool is_shadow) {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "proc-tree/quad-branch-node.vert";

  if (is_shadow) {
    params.frag_file = "shadow/empty.frag";
    params.compile.vert_defines.push_back(glsl::make_define("IS_SHADOW"));
  } else {
    params.frag_file = "proc-tree/quad-branch-node.frag";
  }

  auto shadow_defs = csm::make_default_sample_shadow_preprocessor_definitions();
  params.compile.vert_defines.insert(
    params.compile.vert_defines.end(), shadow_defs.begin(), shadow_defs.end());
  params.compile.frag_defines.insert(
    params.compile.frag_defines.end(), shadow_defs.begin(), shadow_defs.end());

  params.reflect.to_vk_descriptor_type = refl::always_dynamic_uniform_buffer_descriptor_type;
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_program_source(bool is_wind, bool is_shadow) {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "proc-tree/branch-node.vert";

  if (is_shadow) {
    params.frag_file = "shadow/empty.frag";
  } else {
    params.frag_file = "proc-tree/branch-node.frag";
  }

  if (is_wind) {
    params.compile.vert_defines.push_back(glsl::make_define("IS_WIND"));
    params.compile.frag_defines.push_back(glsl::make_define("IS_WIND"));
  }

  if (is_shadow) {
    params.compile.vert_defines.push_back(glsl::make_define("IS_SHADOW"));
  }

  params.reflect.to_vk_descriptor_type = refl::always_dynamic_uniform_buffer_descriptor_type;

  auto shadow_defs = csm::make_default_sample_shadow_preprocessor_definitions();
  params.compile.vert_defines.insert(
    params.compile.vert_defines.end(), shadow_defs.begin(), shadow_defs.end());
  params.compile.frag_defines.insert(
    params.compile.frag_defines.end(), shadow_defs.begin(), shadow_defs.end());

  return glsl::make_vert_frag_program_source(params);
}

Optional<gfx::PipelineHandle> create_quad_pipeline(gfx::Context* graphics_context, bool is_shadow) {
  auto source = create_quad_program_source(is_shadow);
  if (!source) {
    return NullOpt{};
  }

  int loc{};
  VertexBufferDescriptor buff_descs[2];
  buff_descs[0].add_attribute(AttributeDescriptor::float2(loc++));
  buff_descs[1].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(loc++, 1, 1));

  auto pass = is_shadow ?
    gfx::get_shadow_render_pass_handle(graphics_context) :
    gfx::get_forward_write_back_render_pass_handle(graphics_context);
  if (!pass) {
    return NullOpt{};
  }

  gfx::GraphicsPipelineCreateInfo create_info{};
  create_info.num_vertex_buffer_descriptors = 2;
  create_info.vertex_buffer_descriptors = buff_descs;
  create_info.num_color_attachments = is_shadow ? 0 : 1;
  create_info.disable_cull_face = true;
  return gfx::create_pipeline(graphics_context, std::move(source.value()), create_info, pass.value());
}

Optional<gfx::PipelineHandle>
create_pipeline(gfx::Context* graphics_context, bool is_wind, bool is_shadow) {
  auto source = create_program_source(is_wind, is_shadow);
  if (!source) {
    return NullOpt{};
  }

  int loc{};
  VertexBufferDescriptor buff_descs[2];
  buff_descs[0].add_attribute(AttributeDescriptor::float2(loc++));
  buff_descs[1].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(loc++, 1, 1));

  auto pass = is_shadow ?
    gfx::get_shadow_render_pass_handle(graphics_context) :
    gfx::get_forward_write_back_render_pass_handle(graphics_context);
  if (!pass) {
    return NullOpt{};
  }

  gfx::GraphicsPipelineCreateInfo create_info{};
  create_info.num_vertex_buffer_descriptors = 2;
  create_info.vertex_buffer_descriptors = buff_descs;
  create_info.num_color_attachments = is_shadow ? 0 : 1;
  return gfx::create_pipeline(graphics_context, std::move(source.value()), create_info, pass.value());
}

Optional<gfx::PipelineHandle>
create_gen_lod_indices_pipeline(gfx::Context* graphics_context, int local_size_x,
                                bool use_frustum_culling) {
  glsl::LoadComputeProgramSourceParams params{};
  params.file = "branch-node-lod/gen-lod-indices.comp";
  params.compile.defines.push_back(glsl::make_integer_define("LOCAL_SIZE_X", local_size_x));
  cull::push_read_occlusion_cull_preprocessor_defines(params.compile.defines);
  if (use_frustum_culling) {
    params.compile.defines.push_back(glsl::make_define("USE_FRUSTUM_CULL"));
  }
  auto src = glsl::make_compute_program_source(params);
  if (src) {
    return gfx::create_compute_pipeline(graphics_context, std::move(src.value()));
  } else {
    return NullOpt{};
  }
}

Optional<gfx::PipelineHandle>
create_gen_draw_list_pipeline(gfx::Context* graphics_context, int local_size_x) {
  glsl::LoadComputeProgramSourceParams params{};
  params.file = "branch-node-lod/gen-draw-list.comp";
  params.compile.defines.push_back(glsl::make_integer_define("LOCAL_SIZE_X", local_size_x));
  auto src = glsl::make_compute_program_source(params);
  if (src) {
    return gfx::create_compute_pipeline(graphics_context, std::move(src.value()));
  } else {
    return NullOpt{};
  }
}

void set_modified(std::bitset<32>& bs, uint32_t n) {
  for (uint32_t i = 0; i < n; i++) {
    bs[i] = true;
  }
}

bool reserve(DynamicArrayBuffer& dyn_buff, size_t element_size, gfx::Context* graphics_context,
             uint32_t count, gfx::BufferUsage usage, gfx::MemoryType mem_type) {
  uint32_t num_reserve = dyn_buff.num_reserved;
  while (num_reserve < count) {
    num_reserve = num_reserve == 0 ? 64 : num_reserve * 2;
  }
  if (num_reserve == dyn_buff.num_reserved) {
    return true;
  }

  auto buff = gfx::create_buffer(graphics_context, usage, mem_type, num_reserve * element_size);
  if (!buff) {
    return false;
  } else {
    dyn_buff.buffer = std::move(buff.value());
    dyn_buff.num_reserved = num_reserve;
    return true;
  }
}

bool reserve(DynamicArrayBuffer& dyn_buff, size_t element_size, gfx::Context* graphics_context,
             uint32_t count, bool is_vert_buff = false, bool is_device_local = false) {
  gfx::BufferUsage use = is_vert_buff ?
    gfx::BufferUsage{gfx::BufferUsageFlagBits::Vertex} :
    gfx::BufferUsage{gfx::BufferUsageFlagBits::Storage};
  gfx::MemoryType mem_type = is_device_local ?
    gfx::MemoryType{gfx::MemoryTypeFlagBits::DeviceLocal} :
    gfx::MemoryType{gfx::MemoryTypeFlagBits::HostVisible};
  return reserve(dyn_buff, element_size, graphics_context, count, use, mem_type);
}

template <typename T>
bool update_modified(T& src, InstanceBuffers& dst, const BeginFrameInfo& info) {
  if (src.aggregates_modified) {
    set_modified(dst.aggregates_modified, info.frame_queue_depth);
    src.aggregates_modified = false;
  }

  bool inst_modified{};
  if (src.static_instances_modified) {
    set_modified(dst.static_modified, info.frame_queue_depth);
    src.static_instances_modified = false;
    inst_modified = true;
  }
  if (src.dynamic_instances_modified) {
    set_modified(dst.dynamic_modified, info.frame_queue_depth);
    src.dynamic_instances_modified = false;
    inst_modified = true;
  }
  if (src.lod_data_modified) {
    set_modified(dst.lod_data_modified, info.frame_queue_depth);
    src.lod_data_modified = false;
    inst_modified = true;
  }

  if (inst_modified) {
    set_modified(dst.indices_modified, info.frame_queue_depth);
  }

  return inst_modified;
}

template <typename InstanceSet>
void fill_modified(const InstanceSet& src, InstanceBuffers& dst, size_t static_size, size_t dyn_size,
                   const BeginFrameInfo& info) {
  const uint32_t fi = info.frame_index;

  auto& agg_buff = dst.aggregate_buffers[fi];
  if (dst.aggregates_modified[fi]) {
    const uint32_t num_agg = src.num_aggregates();
    bool res = reserve(
      agg_buff, sizeof(RenderBranchNodeAggregate), info.graphics_context, num_agg);
    if (!res) {
      dst.buffers_valid = false;
      return;
    }

    assert(num_agg <= agg_buff.num_reserved);
    agg_buff.buffer.write(src.aggregates.data(), num_agg * sizeof(RenderBranchNodeAggregate));
    agg_buff.num_active = num_agg;
    dst.aggregates_modified[fi] = false;
  }

  auto& static_buff = dst.static_buffers[fi];
  if (dst.static_modified[fi]) {
    const uint32_t num_static = src.num_instances();
    bool res = reserve(static_buff, static_size, info.graphics_context, num_static);
    if (!res) {
      dst.buffers_valid = false;
      return;
    }

    assert(num_static <= static_buff.num_reserved);
    static_buff.buffer.write(src.static_instances.data(), num_static * static_size);
    static_buff.num_active = num_static;
    dst.static_modified[fi] = false;
  }

  auto& dyn_buff = dst.dynamic_buffers[fi];
  if (dst.dynamic_modified[fi]) {
    const uint32_t num_dyn = src.num_instances();
    bool res = reserve(dyn_buff, dyn_size, info.graphics_context, num_dyn);
    if (!res) {
      dst.buffers_valid = false;
      return;
    }

    assert(num_dyn <= dyn_buff.num_reserved);
    dyn_buff.buffer.write(src.dynamic_instances.data(), num_dyn * dyn_size);
    dyn_buff.num_active = num_dyn;
    dst.dynamic_modified[fi] = false;
  }

  auto& lod_buff = dst.lod_data_buffers[fi];
  if (dst.lod_data_modified[fi]) {
    const uint32_t num_insts = src.num_instances();
    if (!reserve(lod_buff, sizeof(RenderBranchNodeLODData), info.graphics_context, num_insts)) {
      dst.buffers_valid = false;
      return;
    }

    assert(num_insts <= lod_buff.num_reserved);
    lod_buff.buffer.write(src.lod_data.data(), num_insts * sizeof(RenderBranchNodeLODData));
    lod_buff.num_active = num_insts;
    dst.lod_data_modified[fi] = false;
  }

  auto& inds_buff = dst.instance_indices[fi];
  if (dst.indices_modified[fi]) {
    const uint32_t num_inds = src.num_instances();
    if (dst.tmp_cpu_indices.size() < num_inds) {
      dst.tmp_cpu_indices.resize(num_inds);
      std::iota(dst.tmp_cpu_indices.begin(), dst.tmp_cpu_indices.end(), 0);
    }

    bool res = reserve(inds_buff, sizeof(uint32_t), info.graphics_context, num_inds, true);
    if (!res) {
      dst.buffers_valid = false;
      return;
    }

    inds_buff.buffer.write(dst.tmp_cpu_indices.data(), num_inds * sizeof(uint32_t));
    inds_buff.num_active = num_inds;
    dst.indices_modified[fi] = false;
  }

  dst.buffers_valid = true;
}

void update_base_modified(GPUContext& context, const BeginFrameInfo& info) {
  auto& set = info.cpu_data->base_set;
  if (set.lod_data_potentially_invalidated) {
    assert(set.lod_data_modified);
    context.base_lod_data_potentially_invalidated = true;
    set.lod_data_potentially_invalidated = false;
  }

  update_modified(info.cpu_data->base_set, context.base_instance_buffers, info);
}

void update_wind_modified(GPUContext& context, const BeginFrameInfo& info) {
  auto& set = info.cpu_data->wind_set;
  if (set.lod_data_potentially_invalidated) {
    assert(set.lod_data_modified);
    context.wind_lod_data_potentially_invalidated = true;
    set.lod_data_potentially_invalidated = false;
  }

  update_modified(info.cpu_data->wind_set, context.wind_instance_buffers, info);
}

void fill_base_instance_buffers(GPUContext& context, const BeginFrameInfo& info) {
  fill_modified(
    info.cpu_data->base_set,
    context.base_instance_buffers,
    sizeof(RenderBranchNodeStaticData), sizeof(RenderBranchNodeDynamicData), info);
}

void fill_wind_instance_buffers(GPUContext& context, const BeginFrameInfo& info) {
  fill_modified(
    info.cpu_data->wind_set,
    context.wind_instance_buffers,
    sizeof(RenderWindBranchNodeStaticData), sizeof(RenderBranchNodeDynamicData), info);
}

Optional<IndirectDrawCommand>
require_lod_compute_buffers(gfx::Context* context, LODDeviceComputeBuffers& buffs,
                            uint32_t num_instances, uint32_t num_vertex_indices,
                            const BeginFrameInfo& info) {
  buffs.is_valid = false;

  if (!reserve(buffs.lod_output_data, sizeof(LODOutputData), context, num_instances, false, true)) {
    return NullOpt{};
  }

  {
    auto use = gfx::BufferUsage{gfx::BufferUsageFlagBits::Vertex | gfx::BufferUsageFlagBits::Storage};
    auto mem_type = gfx::MemoryType{gfx::MemoryTypeFlagBits::DeviceLocal};
    if (!reserve(buffs.draw_indices, sizeof(uint32_t), context, num_instances, use, mem_type)) {
      return NullOpt{};
    }
  }

  while (info.frame_index >= uint32_t(buffs.draw_commands.size())) {
    buffs.draw_commands.emplace_back();
  }

  auto& dc_buff = buffs.draw_commands[info.frame_index];

  IndirectDrawCommand prev_command{};
  if (!dc_buff.is_valid()) {
    auto use = gfx::BufferUsage{gfx::BufferUsageFlagBits::Storage | gfx::BufferUsageFlagBits::Indirect};
    auto mem_type = gfx::MemoryType{gfx::MemoryTypeFlagBits::HostVisible};

    auto buff = gfx::create_buffer(context, use, mem_type, sizeof(IndirectDrawCommand));
    if (buff) {
      IndirectDrawCommand command{};
      command.indexCount = num_vertex_indices;
      buff.value().write(&command, sizeof(IndirectDrawCommand));
      dc_buff = std::move(buff.value());
    } else {
      return NullOpt{};
    }
  } else {
    dc_buff.read(&prev_command, sizeof(IndirectDrawCommand));
    IndirectDrawCommand new_cmd{};
    new_cmd.indexCount = num_vertex_indices;
    dc_buff.write(&new_cmd, sizeof(IndirectDrawCommand));
  }

  buffs.is_valid = true;
  return Optional<IndirectDrawCommand>(prev_command);
}

void create_pipeline_data(GPUContext& context, const BeginFrameInfo& info) {
  context.pipelines_valid = false;

  if (auto src = create_pipeline(info.graphics_context, false, false)) {
    context.forward_base_pipeline = std::move(src.value());
  } else {
    return;
  }

  if (auto src = create_pipeline(info.graphics_context, true, false)) {
    context.forward_wind_pipeline = std::move(src.value());
  } else {
    return;
  }

  if (auto src = create_pipeline(info.graphics_context, false, true)) {
    context.shadow_base_pipeline = std::move(src.value());
  } else {
    return;
  }

  if (auto src = create_pipeline(info.graphics_context, true, true)) {
    context.shadow_wind_pipeline = std::move(src.value());
  } else {
    return;
  }

  if (auto src = create_quad_pipeline(info.graphics_context, false)) {
    context.quad_forward_pipeline = std::move(src.value());
  } else {
    return;
  }

  if (auto src = create_quad_pipeline(info.graphics_context, true)) {
    context.quad_shadow_pipeline = std::move(src.value());
  } else {
    return;
  }

  if (auto pd = create_gen_lod_indices_pipeline(info.graphics_context, context.compute_local_size_x, false)) {
    context.gen_lod_indices_occlusion_cull_pipeline = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_gen_lod_indices_pipeline(info.graphics_context, context.compute_local_size_x, true)) {
    context.gen_lod_indices_frustum_cull_pipeline = std::move(pd.value());
  } else {
    return;
  }

  if (auto pd = create_gen_draw_list_pipeline(info.graphics_context, context.compute_local_size_x)) {
    context.gen_draw_list_pipeline = std::move(pd.value());
  } else {
    return;
  }

  context.pipelines_valid = true;
}

void require_quad_base_desc_set0s(GPUContext& context, const BeginFrameInfo& info) {
  context.quad_base_desc_set0 = NullOpt{};
  context.quad_base_shadow_desc_set0 = NullOpt{};

  auto& inst_buff = context.base_instance_buffers;
  auto& un_buff = context.uniform_buffer;
  if (!inst_buff.buffers_valid || !un_buff.buffer.is_valid()) {
    return;
  }

  const uint32_t fi = info.frame_index;
  const auto& forward_pd = context.quad_forward_pipeline;
  const auto& shadow_pd = context.quad_shadow_pipeline;
  if (inst_buff.dynamic_buffers[fi].num_active == 0 ||
      !forward_pd.is_valid() || !shadow_pd.is_valid()) {
    return;
  }

  DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};

  push_dynamic_uniform_buffer(
    scaffold, bind++,
    un_buff.buffer.get(), sizeof(UniformBufferData));
  push_storage_buffer(
    scaffold, bind++,
    inst_buff.dynamic_buffers[fi].buffer.get(),
    inst_buff.dynamic_buffers[fi].num_active * sizeof(RenderBranchNodeDynamicData));

  context.quad_base_desc_set0 = gfx::require_updated_descriptor_set(
    info.graphics_context, scaffold, forward_pd);
  context.quad_base_shadow_desc_set0 = gfx::require_updated_descriptor_set(
    info.graphics_context, scaffold, shadow_pd, true);
}

void require_quad_wind_desc_set0s(GPUContext& context, const BeginFrameInfo& info) {
  context.quad_wind_desc_set0 = NullOpt{};
  context.quad_wind_shadow_desc_set0 = NullOpt{};

  auto& inst_buff = context.wind_instance_buffers;
  auto& un_buff = context.uniform_buffer;
  if (!inst_buff.buffers_valid || !un_buff.buffer.is_valid()) {
    return;
  }

  const uint32_t fi = info.frame_index;
  const auto& forward_pd = context.quad_forward_pipeline;
  const auto& shadow_pd = context.quad_shadow_pipeline;
  if (inst_buff.dynamic_buffers[fi].num_active == 0 ||
      !forward_pd.is_valid() || !shadow_pd.is_valid()) {
    return;
  }

  DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};

  push_dynamic_uniform_buffer(
    scaffold, bind++,
    un_buff.buffer.get(), sizeof(UniformBufferData));
  push_storage_buffer(
    scaffold, bind++,
    inst_buff.dynamic_buffers[fi].buffer.get(),
    inst_buff.dynamic_buffers[fi].num_active * sizeof(RenderBranchNodeDynamicData));

  context.quad_wind_desc_set0 = gfx::require_updated_descriptor_set(
    info.graphics_context, scaffold, forward_pd);
  context.quad_wind_shadow_desc_set0 = gfx::require_updated_descriptor_set(
    info.graphics_context, scaffold, shadow_pd, true);
}

void require_base_forward_desc_set0(GPUContext& context, const BeginFrameInfo& info) {
  context.base_forward_desc_set0 = NullOpt{};

  auto& inst_buff = context.base_instance_buffers;
  auto& un_buff = context.uniform_buffer;
  if (!inst_buff.buffers_valid || !un_buff.buffer.is_valid()) {
    return;
  }

  const uint32_t fi = info.frame_index;
  const auto& pd = context.forward_base_pipeline;
  if (inst_buff.dynamic_buffers[fi].num_active == 0 || !pd.is_valid()) {
    return;
  }

  auto sampler_edge_clamp = gfx::get_image_sampler_linear_edge_clamp(info.graphics_context);

  DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};

  push_dynamic_uniform_buffer(
    scaffold, bind++,
    un_buff.buffer.get(), sizeof(UniformBufferData));
  push_storage_buffer(
    scaffold, bind++,
    inst_buff.dynamic_buffers[fi].buffer.get(),
    inst_buff.dynamic_buffers[fi].num_active * sizeof(RenderBranchNodeDynamicData));
  push_storage_buffer(
    scaffold, bind++,
    inst_buff.static_buffers[fi].buffer.get(),
    inst_buff.static_buffers[fi].num_active * sizeof(RenderBranchNodeStaticData));
  push_storage_buffer(
    scaffold, bind++,
    inst_buff.aggregate_buffers[fi].buffer.get(),
    inst_buff.aggregate_buffers[fi].num_active * sizeof(RenderBranchNodeAggregate));
  push_combined_image_sampler(
    scaffold, bind++, info.shadow_image, sampler_edge_clamp);

  context.base_forward_desc_set0 = gfx::require_updated_descriptor_set(
    info.graphics_context, scaffold, pd);
}

void require_wind_forward_desc_set0(GPUContext& context, const BeginFrameInfo& info) {
  context.wind_forward_desc_set0 = NullOpt{};

  auto& inst_buff = context.wind_instance_buffers;
  auto& un_buff = context.uniform_buffer;
  if (!inst_buff.buffers_valid || !un_buff.buffer.is_valid()) {
    return;
  }

  const uint32_t fi = info.frame_index;
  const auto& pd = context.forward_wind_pipeline;

  if (inst_buff.dynamic_buffers[fi].num_active == 0 || !pd.is_valid()) {
    return;
  }

  Optional<SampleImageView> wind_im;
  if (context.wind_image) {
    if (auto im = info.dynamic_sampled_image_manager->get(context.wind_image.value())) {
      if (im.value().is_2d() && im.value().vertex_shader_sample_ok()) {
        wind_im = im.value().to_sample_image_view();
      }
    }
  }
  if (!wind_im) {
    return;
  }

  auto sampler_repeat = gfx::get_image_sampler_linear_repeat(info.graphics_context);
  auto sampler_edge_clamp = gfx::get_image_sampler_linear_edge_clamp(info.graphics_context);

  DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};

  push_dynamic_uniform_buffer(
    scaffold, bind++,
    un_buff.buffer.get(), sizeof(UniformBufferData));
  push_storage_buffer(
    scaffold, bind++,
    inst_buff.dynamic_buffers[fi].buffer.get(),
    inst_buff.dynamic_buffers[fi].num_active * sizeof(RenderBranchNodeDynamicData));
  push_storage_buffer(
    scaffold, bind++,
    inst_buff.static_buffers[fi].buffer.get(),
    inst_buff.static_buffers[fi].num_active * sizeof(RenderWindBranchNodeStaticData));
  push_storage_buffer(
    scaffold, bind++,
    inst_buff.aggregate_buffers[fi].buffer.get(),
    inst_buff.aggregate_buffers[fi].num_active * sizeof(RenderBranchNodeAggregate));
  push_combined_image_sampler(
    scaffold, bind++, wind_im.value(), sampler_repeat);
  push_combined_image_sampler(
    scaffold, bind++, info.shadow_image, sampler_edge_clamp);

  context.wind_forward_desc_set0 = gfx::require_updated_descriptor_set(
    info.graphics_context, scaffold, pd);
}

Optional<VkDescriptorSet>
require_shadow_desc_set0(const GPUContext& context, const InstanceBuffers& inst_buff,
                         size_t static_size, size_t dyn_size,
                         const gfx::PipelineHandle& pd, const BeginFrameInfo& info) {
  auto& un_buff = context.uniform_buffer;
  if (!inst_buff.buffers_valid || !un_buff.buffer.is_valid() || !pd.is_valid()) {
    return NullOpt{};
  }

  const uint32_t fi = info.frame_index;
  if (inst_buff.dynamic_buffers[fi].num_active == 0) {
    return NullOpt{};
  }

  DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};

  push_dynamic_uniform_buffer(
    scaffold, bind++,
    un_buff.buffer.get(), sizeof(UniformBufferData));
  push_storage_buffer(
    scaffold, bind++,
    inst_buff.dynamic_buffers[fi].buffer.get(),
    inst_buff.dynamic_buffers[fi].num_active * dyn_size);
  push_storage_buffer(
    scaffold, bind++,
    inst_buff.static_buffers[fi].buffer.get(),
    inst_buff.static_buffers[fi].num_active * static_size);
  push_storage_buffer(
    scaffold, bind++,
    inst_buff.aggregate_buffers[fi].buffer.get(),
    inst_buff.aggregate_buffers[fi].num_active * sizeof(RenderBranchNodeAggregate));

  return gfx::require_updated_descriptor_set(info.graphics_context, scaffold, pd);
}

void require_shadow_desc_set0(GPUContext& context, const BeginFrameInfo& info) {
  {
    auto& inst_buff = context.base_instance_buffers;
    auto& pd = context.shadow_base_pipeline;
    context.base_shadow_desc_set0 = require_shadow_desc_set0(
      context, inst_buff,
      sizeof(RenderBranchNodeStaticData),
      sizeof(RenderBranchNodeDynamicData), pd, info);
  }
  {
    auto& inst_buff = context.wind_instance_buffers;
    auto& pd = context.shadow_wind_pipeline;
    context.wind_shadow_desc_set0 = require_shadow_desc_set0(
      context, inst_buff,
      sizeof(RenderWindBranchNodeStaticData),
      sizeof(RenderBranchNodeDynamicData), pd, info);
  }
}

void require_desc_sets(GPUContext& context, const BeginFrameInfo& info) {
  require_base_forward_desc_set0(context, info);
  require_wind_forward_desc_set0(context, info);
  require_shadow_desc_set0(context, info);
  require_quad_base_desc_set0s(context, info);
  require_quad_wind_desc_set0s(context, info);
}

void try_initialize(GPUContext& context, const BeginFrameInfo& info) {
  create_pipeline_data(context, info);
  if (auto geom = create_geometry_buffer(context.lod0_geom_params, info)) {
    context.lod0_geometry_buffer = std::move(geom.value());
  }
  if (auto geom = create_geometry_buffer(context.lod1_geom_params, info)) {
    context.lod1_geometry_buffer = std::move(geom.value());
  }
  if (auto geom = create_quad_geometry_buffer(info)) {
    context.quad_geometry_buffer = std::move(geom.value());
  }
  if (auto un_buff = create_uniform_buffer(info)) {
    context.uniform_buffer = std::move(un_buff.value());
  }
}

void update_uniform_buffer(GPUContext& context, const BeginFrameInfo& info) {
  auto& un_buff = context.uniform_buffer;
  if (!un_buff.buffer.is_valid()) {
    return;
  }

  const auto& geom_params = context.use_lod1_geometry ?
    context.lod1_geom_params : context.lod0_geom_params;

  const size_t off = un_buff.element_stride * info.frame_index;
  auto un_data = make_uniform_buffer_data(context, info.camera, info.csm_desc, geom_params);
  un_buff.buffer.write(&un_data, sizeof(un_data), off);
}

void apply_pending_modifications(GPUContext& context) {
  if (context.set_use_lod1_geometry) {
    context.use_lod1_geometry = context.set_use_lod1_geometry.value();
    context.set_use_lod1_geometry = NullOpt{};
  }

  if (context.set_render_base_as_quads) {
    context.render_base_as_quads = context.set_render_base_as_quads.value();
    context.set_render_base_as_quads = NullOpt{};
  }

  if (context.set_render_wind_as_quads) {
    context.render_wind_as_quads = context.set_render_wind_as_quads.value();
    context.set_render_wind_as_quads = NullOpt{};
  }
}

void require_base_lod_compute_buffers(GPUContext& context, const BeginFrameInfo& info) {
  uint32_t num_base_geom_vertex_indices{};
  if (context.render_base_as_quads) {
    num_base_geom_vertex_indices = context.quad_geometry_buffer.num_indices;
  } else {
    num_base_geom_vertex_indices = context.use_lod1_geometry ?
      context.lod1_geometry_buffer.num_indices : context.lod0_geometry_buffer.num_indices;
  }

  auto prev_base_draw_cmd = require_lod_compute_buffers(
    info.graphics_context, context.base_lod_compute_buffers,
    info.cpu_data->base_set.num_instances(), num_base_geom_vertex_indices, info);
  if (prev_base_draw_cmd) {
    context.prev_base_indirect_draw_command = prev_base_draw_cmd.value();
  } else {
    context.prev_base_indirect_draw_command = {};
  }
}

void require_wind_lod_compute_buffers(GPUContext& context, const BeginFrameInfo& info) {
  uint32_t num_wind_geom_vertex_indices{};
  if (context.render_wind_as_quads) {
    num_wind_geom_vertex_indices = context.quad_geometry_buffer.num_indices;
  } else {
    num_wind_geom_vertex_indices = context.use_lod1_geometry ?
      context.lod1_geometry_buffer.num_indices : context.lod0_geometry_buffer.num_indices;
  }

  auto prev_wind_draw_cmd = require_lod_compute_buffers(
    info.graphics_context, context.wind_lod_compute_buffers,
    info.cpu_data->wind_set.num_instances(), num_wind_geom_vertex_indices, info);
  if (prev_wind_draw_cmd) {
    context.prev_wind_indirect_draw_command = prev_wind_draw_cmd.value();
  } else {
    context.prev_wind_indirect_draw_command = {};
  }
}

void clear_flags(GPUContext& context) {
  context.generated_base_indirect_draw_list = false;
  context.generated_wind_indirect_draw_list = false;
  context.generated_base_indirect_draw_list_with_occlusion_culling = false;
  context.generated_wind_indirect_draw_list_with_occlusion_culling = false;
  context.rendered_base_forward_with_occlusion_culling = false;
  context.rendered_wind_forward_with_occlusion_culling = false;
  context.base_lod_data_potentially_invalidated = false;
  context.wind_lod_data_potentially_invalidated = false;
}

void begin_frame(GPUContext& context, const BeginFrameInfo& info) {
  clear_flags(context);

  if (context.disabled) {
    return;
  }

  if (!context.tried_initialize) {
    try_initialize(context, info);
    context.tried_initialize = true;
  }

  apply_pending_modifications(context);

  context.base_instance_buffers.require(info.frame_queue_depth);
  context.wind_instance_buffers.require(info.frame_queue_depth);

  update_base_modified(context, info);
  update_wind_modified(context, info);

  fill_base_instance_buffers(context, info);
  fill_wind_instance_buffers(context, info);

  require_base_lod_compute_buffers(context, info);
  require_wind_lod_compute_buffers(context, info);

  update_uniform_buffer(context, info);
  require_desc_sets(context, info);
  context.began_frame = true;
}

void draw_instances_indirect(VkCommandBuffer cmd, const GeometryBuffer& geom,
                             const LODDeviceComputeBuffers& buffs, uint32_t fi) {
  VkBuffer vert_buffs[2]{
    geom.geom.get(),
    buffs.draw_indices.buffer.get()
  };
  VkDeviceSize vb_offs[2]{0, 0};
  VkBuffer ind_buff = geom.index.get();
  vkCmdBindVertexBuffers(cmd, 0, 2, vert_buffs, vb_offs);
  vkCmdBindIndexBuffer(cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);
  vkCmdDrawIndexedIndirect(cmd, buffs.draw_commands[fi].get(), 0, 1, 0);
}

void draw_instances(VkCommandBuffer cmd, const DynamicArrayBuffer& ib,
                    const GeometryBuffer& geom, const Optional<uint32_t>& max_num_insts) {
  VkBuffer vert_buffs[2]{
    geom.geom.get(),
    ib.buffer.get()
  };
  VkDeviceSize vb_offs[2]{0, 0};

  VkBuffer ind_buff = geom.index.get();
  vkCmdBindVertexBuffers(cmd, 0, 2, vert_buffs, vb_offs);
  vkCmdBindIndexBuffer(cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);

  DrawIndexedDescriptor draw_desc{};
  draw_desc.num_indices = geom.num_indices;
  draw_desc.num_instances = ib.num_active;

  if (max_num_insts) {
    draw_desc.num_instances = std::min(draw_desc.num_instances, max_num_insts.value());
  }

  cmd::draw_indexed(cmd, &draw_desc);
}

void render_forward(GPUContext& context, const RenderForwardInfo& info) {
  if (!context.began_frame) {
    return;
  }

  if (!context.pipelines_valid ||
      !context.lod0_geometry_buffer.is_valid ||
      !context.lod1_geometry_buffer.is_valid ||
      !context.uniform_buffer.buffer.is_valid()) {
    return;
  }

  auto& un_buff = context.uniform_buffer;
  const uint32_t dyn_offs[1] = {
    uint32_t(un_buff.element_stride * info.frame_index)
  };

  Optional<uint32_t> inst_limit;
  if (context.render_params.limit_to_max_num_instances) {
    inst_limit = context.render_params.max_num_instances;
  }

  if (!context.disable_base_drawables) {
    const auto* ib = &context.base_instance_buffers.instance_indices[info.frame_index];
    if (ib->num_active > 0) {
      auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "render_base_branch_nodes");
      (void) profiler;

      const auto pc = make_forward_push_constant_data(context, info.camera);
      const gfx::PipelineHandle* ph;
      const Optional<VkDescriptorSet>* desc_set0;
      const GeometryBuffer* geom;

      if (context.render_base_as_quads) {
        ph = &context.quad_forward_pipeline;
        desc_set0 = &context.quad_base_desc_set0;
        geom = &context.quad_geometry_buffer;
      } else {
        ph = &context.forward_base_pipeline;
        desc_set0 = &context.base_forward_desc_set0;
        geom = context.use_lod1_geometry ?
          &context.lod1_geometry_buffer : &context.lod0_geometry_buffer;
      }

      if (ph->is_valid() && desc_set0->has_value()) {
        cmd::bind_graphics_pipeline(info.cmd, ph->get());
        cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor);
        cmd::bind_graphics_descriptor_sets(
          info.cmd, ph->get_layout(), 0, 1, &desc_set0->value(), 1, dyn_offs);
        cmd::push_constants(info.cmd, ph->get_layout(), VK_SHADER_STAGE_VERTEX_BIT, &pc);

        if (context.prefer_indirect_pipeline) {
          if (context.generated_base_indirect_draw_list) {
            draw_instances_indirect(info.cmd, *geom, context.base_lod_compute_buffers, info.frame_index);
            context.rendered_base_forward_with_occlusion_culling =
              context.generated_base_indirect_draw_list_with_occlusion_culling;
          }
        } else {
          draw_instances(info.cmd, *ib, *geom, inst_limit);
        }
      }
    }
  }

  if (!context.disable_wind_drawables) {
    auto& ib = context.wind_instance_buffers.instance_indices[info.frame_index];
    if (ib.num_active > 0) {
      auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "render_wind_branch_nodes");
      (void) profiler;

      auto pc = make_forward_push_constant_data(context, info.camera);
      const gfx::PipelineHandle* ph;
      const Optional<VkDescriptorSet>* desc_set0;
      const GeometryBuffer* geom;

      if (context.render_wind_as_quads) {
        ph = &context.quad_forward_pipeline;
        desc_set0 = &context.quad_wind_desc_set0;
        geom = &context.quad_geometry_buffer;
      } else {
        ph = &context.forward_wind_pipeline;
        desc_set0 = &context.wind_forward_desc_set0;
        geom = context.use_lod1_geometry ?
          &context.lod1_geometry_buffer : &context.lod0_geometry_buffer;
      }

      cmd::bind_graphics_pipeline(info.cmd, ph->get());
      cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor);
      cmd::bind_graphics_descriptor_sets(
        info.cmd, ph->get_layout(), 0, 1, &desc_set0->value(), 1, dyn_offs);
      cmd::push_constants(info.cmd, ph->get_layout(), VK_SHADER_STAGE_VERTEX_BIT, &pc);

      if (context.prefer_indirect_pipeline) {
        if (context.generated_wind_indirect_draw_list) {
          draw_instances_indirect(info.cmd, *geom, context.wind_lod_compute_buffers, info.frame_index);
          context.rendered_wind_forward_with_occlusion_culling =
            context.generated_wind_indirect_draw_list_with_occlusion_culling;
        }
      } else {
        draw_instances(info.cmd, ib, *geom, inst_limit);
      }
    }
  }
}

void render_shadow(GPUContext& context, const RenderShadowInfo& info) {
  if (!context.began_frame) {
    return;
  }

  if (!context.pipelines_valid ||
      !context.lod0_geometry_buffer.is_valid ||
      !context.lod1_geometry_buffer.is_valid ||
      !context.uniform_buffer.buffer.is_valid()) {
    return;
  }

  if (info.cascade_index > context.max_shadow_cascade_index) {
    return;
  }

  auto& un_buff = context.uniform_buffer;
  const uint32_t dyn_offs[1] = {
    uint32_t(un_buff.element_stride * info.frame_index)
  };

  Optional<uint32_t> inst_limit;
  if (context.render_params.limit_to_max_num_instances) {
    inst_limit = context.render_params.max_num_instances;
  }

  if (!context.disable_base_drawables && !context.disable_base_shadow) {
    auto& ib = context.base_instance_buffers.instance_indices[info.frame_index];
    if (ib.num_active > 0) {
      auto db_label = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "render_base_branch_nodes_shadow");
      (void) db_label;

      auto pc = make_shadow_push_constant_data(info.proj_view);
      const gfx::PipelineHandle* ph;
      const Optional<VkDescriptorSet>* desc_set0;
      const GeometryBuffer* geom;

      if (context.render_base_as_quads) {
        ph = &context.quad_shadow_pipeline;
        desc_set0 = &context.quad_base_shadow_desc_set0;
        geom = &context.quad_geometry_buffer;
      } else {
        ph = &context.shadow_base_pipeline;
        desc_set0 = &context.base_shadow_desc_set0;
        geom = context.use_lod1_geometry ?
          &context.lod1_geometry_buffer : &context.lod0_geometry_buffer;
      }

      cmd::bind_graphics_pipeline(info.cmd, ph->get());
      cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor);
      cmd::bind_graphics_descriptor_sets(
        info.cmd, ph->get_layout(), 0, 1, &desc_set0->value(), 1, dyn_offs);
      cmd::push_constants(info.cmd, ph->get_layout(), VK_SHADER_STAGE_VERTEX_BIT, &pc);
      draw_instances(info.cmd, ib, *geom, inst_limit);
    }
  }

  if (!context.disable_wind_drawables && !context.disable_wind_shadow) {
    auto& ib = context.wind_instance_buffers.instance_indices[info.frame_index];
    if (ib.num_active > 0) {
      auto db_label = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "render_wind_branch_nodes_shadow");
      (void) db_label;

      auto pc = make_shadow_push_constant_data(info.proj_view);
      const gfx::PipelineHandle* ph;
      const Optional<VkDescriptorSet>* desc_set0;
      const GeometryBuffer* geom;

      if (context.render_wind_as_quads) {
        ph = &context.quad_shadow_pipeline;
        desc_set0 = &context.quad_wind_shadow_desc_set0;
        geom = &context.quad_geometry_buffer;
      } else {
        ph = &context.shadow_wind_pipeline;
        desc_set0 = &context.wind_shadow_desc_set0;
        geom = context.use_lod1_geometry ?
          &context.lod1_geometry_buffer : &context.lod0_geometry_buffer;
      }

      cmd::bind_graphics_pipeline(info.cmd, ph->get());
      cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor);
      cmd::bind_graphics_descriptor_sets(
        info.cmd, ph->get_layout(), 0, 1, &desc_set0->value(), 1, dyn_offs);
      cmd::push_constants(info.cmd, ph->get_layout(), VK_SHADER_STAGE_VERTEX_BIT, &pc);
      draw_instances(info.cmd, ib, *geom, inst_limit);
    }
  }
}

void early_graphics_compute(GPUContext& context, const EarlyComputeInfo& info) {
  struct PushConstants {
    Vec4<uint32_t> num_instances_unused;
  };

  if (!context.prefer_indirect_pipeline || !context.began_frame) {
    return;
  }

  auto& gen_lod_pipe_occlusion_cull = context.gen_lod_indices_occlusion_cull_pipeline;
  auto& gen_lod_pipe_frustum_cull = context.gen_lod_indices_frustum_cull_pipeline;
  auto& gen_draw_list_pipe = context.gen_draw_list_pipeline;

  if (!gen_lod_pipe_occlusion_cull.is_valid() ||
      !gen_lod_pipe_frustum_cull.is_valid() ||
      !gen_draw_list_pipe.is_valid()) {
    return;
  }

  auto& occlusion_cull_res = info.occlusion_cull_results;
  auto& frust_cull_res = info.frustum_cull_results;

  //  @TODO: Can still render the other set with occlusion culling if only one set is invalidated.
  const bool lod_data_invalidated =
    context.base_lod_data_potentially_invalidated ||
    context.wind_lod_data_potentially_invalidated;

  bool prefer_frustum_culling = lod_data_invalidated || !occlusion_cull_res;
  if (prefer_frustum_culling && !frust_cull_res) {
    return;
  }

  auto db_label = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "gen_branch_nodes_draw_indices");
  (void) db_label;

  const auto& gen_lod_pipe = prefer_frustum_culling ?
    gen_lod_pipe_frustum_cull : gen_lod_pipe_occlusion_cull;

  const uint32_t fi = info.frame_index;

  const InstanceBuffers* inst_buffs[2] = {
    &context.base_instance_buffers,
    &context.wind_instance_buffers
  };

  const LODDeviceComputeBuffers* comp_buffs[2] = {
    &context.base_lod_compute_buffers,
    &context.wind_lod_compute_buffers
  };

  //  generate LOD indices
  for (uint32_t ds = 0; ds < 2; ds++) {
    const uint32_t num_insts = inst_buffs[ds]->lod_data_buffers[fi].num_active;

    if (num_insts == 0) {
      continue;
    }

    vk::DescriptorSetScaffold scaffold;
    scaffold.set = 0;
    uint32_t bind{};
    push_storage_buffer(
      scaffold, bind++,
      inst_buffs[ds]->lod_data_buffers[fi].buffer.get(), num_insts * sizeof(RenderBranchNodeLODData));

    if (prefer_frustum_culling) {
      push_storage_buffer(
        scaffold, bind++,
        frust_cull_res.value().group_offsets_buffer,
        frust_cull_res.value().num_group_offsets * sizeof(cull::FrustumCullGroupOffset));
      push_storage_buffer(
        scaffold, bind++,
        frust_cull_res.value().results_buffer,
        frust_cull_res.value().num_results * sizeof(cull::FrustumCullResult));
    } else {
      push_storage_buffer(
        scaffold, bind++,
        occlusion_cull_res.value().group_offsets_buffer,
        occlusion_cull_res.value().num_group_offsets * sizeof(cull::FrustumCullGroupOffset));
      push_storage_buffer(
        scaffold, bind++,
        occlusion_cull_res.value().results_buffer,
        occlusion_cull_res.value().num_results * sizeof(cull::OcclusionCullAgainstDepthPyramidElementResult));
    }

    push_storage_buffer(
      scaffold, bind++,
      comp_buffs[ds]->lod_output_data.buffer.get(), num_insts * sizeof(LODOutputData));

    auto desc_set = gfx::require_updated_descriptor_set(info.context, scaffold, gen_lod_pipe);
    if (!desc_set) {
      return;
    }

    PushConstants pcs{};
    pcs.num_instances_unused.x = num_insts;

    vk::cmd::bind_compute_pipeline(info.cmd, gen_lod_pipe.get());
    vk::cmd::bind_compute_descriptor_sets(info.cmd, gen_lod_pipe.get_layout(), 0, 1, &desc_set.value());
    vk::cmd::push_constants(info.cmd, gen_lod_pipe.get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, &pcs);

    auto num_dispatch = uint32_t(std::ceil(double(num_insts) / context.compute_local_size_x));
    vkCmdDispatch(info.cmd, num_dispatch, 1, 1);
  }

  {
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(
      info.cmd,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      0, 1, &barrier, 0, nullptr, 0, nullptr);
  }

  //  generate draw commands and indices
  for (uint32_t ds = 0; ds < 2; ds++) {
    const uint32_t num_insts = inst_buffs[ds]->lod_data_buffers[fi].num_active;

    if (num_insts == 0) {
      continue;
    }

    auto& dc_buff = comp_buffs[ds]->draw_commands[info.frame_index];

    vk::DescriptorSetScaffold scaffold;
    scaffold.set = 0;
    uint32_t bind{};
    push_storage_buffer(
      scaffold, bind++,
      comp_buffs[ds]->lod_output_data.buffer.get(), num_insts * sizeof(LODOutputData));
    push_storage_buffer(
      scaffold, bind++,
      dc_buff.get(), sizeof(IndirectDrawCommand));
    push_storage_buffer(
      scaffold, bind++,
      comp_buffs[ds]->draw_indices.buffer.get(), num_insts * sizeof(uint32_t));

    auto desc_set = gfx::require_updated_descriptor_set(info.context, scaffold, gen_draw_list_pipe);
    if (!desc_set) {
      return;
    }

    PushConstants pcs{};
    pcs.num_instances_unused.x = num_insts;

    vk::cmd::bind_compute_pipeline(info.cmd, gen_draw_list_pipe.get());
    vk::cmd::bind_compute_descriptor_sets(info.cmd, gen_draw_list_pipe.get_layout(), 0, 1, &desc_set.value());
    vk::cmd::push_constants(info.cmd, gen_draw_list_pipe.get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, &pcs);

    auto num_dispatch = uint32_t(std::ceil(double(num_insts) / context.compute_local_size_x));
    vkCmdDispatch(info.cmd, num_dispatch, 1, 1);
  }

  {
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vkCmdPipelineBarrier(
      info.cmd,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
      0, 1, &barrier, 0, nullptr, 0, nullptr);
  }

  context.generated_base_indirect_draw_list = true;
  context.generated_wind_indirect_draw_list = true;
  context.generated_base_indirect_draw_list_with_occlusion_culling = !prefer_frustum_culling;
  context.generated_wind_indirect_draw_list_with_occlusion_culling = !prefer_frustum_culling;
}

struct {
  GPUContext context;
} globals;

} //  anon

void tree::render_branch_nodes_begin_frame(const RenderBranchNodesBeginFrameInfo& info) {
  begin_frame(globals.context, info);
}

void tree::render_branch_nodes_early_graphics_compute(const EarlyComputeInfo& info) {
  early_graphics_compute(globals.context, info);
}

void tree::render_branch_nodes_end_frame() {
  globals.context.began_frame = false;
  globals.context.gui_feedback_did_render_base_with_occlusion_culling =
    globals.context.rendered_base_forward_with_occlusion_culling;
  globals.context.gui_feedback_did_render_wind_with_occlusion_culling =
    globals.context.rendered_wind_forward_with_occlusion_culling;
}

void tree::render_branch_nodes_forward(const RenderBranchNodesRenderForwardInfo& info) {
  render_forward(globals.context, info);
}

void tree::render_branch_nodes_shadow(const RenderBranchNodesRenderShadowInfo& info) {
  render_shadow(globals.context, info);
}

void tree::set_render_branch_nodes_disabled(bool disable) {
  globals.context.disabled = disable;
}

bool tree::get_render_branch_nodes_disabled() {
  return globals.context.disabled;
}

bool tree::get_set_render_branch_nodes_base_shadow_disabled(const bool* disable) {
  if (disable) {
    globals.context.disable_base_shadow = *disable;
  }
  return globals.context.disable_base_shadow;
}

bool tree::get_set_render_branch_nodes_wind_shadow_disabled(const bool* disable) {
  if (disable) {
    globals.context.disable_wind_shadow = *disable;
  }
  return globals.context.disable_wind_shadow;
}

bool tree::get_set_render_branch_nodes_prefer_cull_enabled(const bool* enable) {
  if (enable) {
    globals.context.prefer_indirect_pipeline = *enable;
  }
  return globals.context.prefer_indirect_pipeline;
}

bool tree::get_set_render_branch_nodes_disable_wind_drawables(const bool* disable) {
  if (disable) {
    globals.context.disable_wind_drawables = *disable;
  }
  return globals.context.disable_wind_drawables;
}

bool tree::get_set_render_branch_nodes_disable_base_drawables(const bool* disable) {
  if (disable) {
    globals.context.disable_base_drawables = *disable;
  }
  return globals.context.disable_base_drawables;
}

bool tree::get_set_render_branch_nodes_prefer_low_lod_geometry(const bool* pref) {
  if (pref) {
    globals.context.set_use_lod1_geometry = *pref;
  }
  return globals.context.use_lod1_geometry;
}

bool tree::get_set_render_branch_nodes_render_base_drawables_as_quads(const bool* pref) {
  if (pref) {
    globals.context.set_render_base_as_quads = *pref;
  }
  return globals.context.render_base_as_quads;
}

bool tree::get_set_render_branch_nodes_render_wind_drawables_as_quads(const bool* pref) {
  if (pref) {
    globals.context.set_render_wind_as_quads = *pref;
  }
  return globals.context.render_wind_as_quads;
}

uint32_t tree::get_set_render_branch_nodes_max_cascade_index(const uint32_t* ind) {
  if (ind) {
    globals.context.max_shadow_cascade_index = *ind;
  }
  return globals.context.max_shadow_cascade_index;
}

void tree::terminate_branch_node_renderer() {
  globals.context = {};
}

void tree::set_render_branch_nodes_wind_displacement_image(uint32_t id) {
  globals.context.wind_image = DynamicSampledImageManager::Handle{id};
}

RenderBranchNodesRenderParams* tree::get_render_branch_nodes_render_params() {
  return &globals.context.render_params;
}

RenderBranchNodesStats tree::get_render_branch_nodes_stats() {
  RenderBranchNodesStats stats{};
  auto& base_cmd = globals.context.prev_base_indirect_draw_command;
  auto& wind_cmd = globals.context.prev_wind_indirect_draw_command;
  stats.prev_num_base_forward_instances = base_cmd.instanceCount;
  stats.prev_num_wind_forward_instances = wind_cmd.instanceCount;
  stats.rendered_base_forward_with_occlusion_culling =
    globals.context.gui_feedback_did_render_base_with_occlusion_culling;
  stats.rendered_wind_forward_with_occlusion_culling =
    globals.context.gui_feedback_did_render_wind_with_occlusion_culling;
  return stats;
}

GROVE_NAMESPACE_END
