#include "render_particles_gpu.hpp"
#include "./graphics.hpp"
#include "../vk/vk.hpp"
#include "grove/common/common.hpp"
#include "grove/common/pack.hpp"
#include "grove/math/util.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/visual/geometry.hpp"
#include <vector>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace particle;

using RenderInfo = RenderParticlesRenderInfo;
using BeginFrameInfo = RenderParticlesBeginFrameInfo;

struct SegmentedQuadVertex {
  Vec4<uint32_t> position_and_color;
};

struct SegmentedQuadSampleDepthVertex {
  Vec4<uint32_t> position_and_color;
  Vec4f min_depth_weight_unused;
};

struct CircleQuadInstance {
  Vec4f translation_scale;
  Vec4f color_opacity;
};

struct SegmentedQuadPushConstantData {
  Mat4f projection_view;
};

struct CircleQuadPushConstantData {
  Mat4f projection_view;
  Mat4f inv_view;
};

struct DynamicArrayBuffer {
  gfx::BufferHandle buff;
  uint32_t size{};
  uint32_t capacity{};
};

struct GeometryBuffer {
  bool is_valid() const {
    return vertices.is_valid();
  }

  gfx::BufferHandle vertices;
  gfx::BufferHandle indices;
  uint32_t num_indices{};
};

struct GPUContext {
  std::vector<SegmentedQuadVertex> segmented_quad_vertices_cpu;
  DynamicArrayBuffer segmented_quad_vertices_gpu;
  gfx::PipelineHandle segmented_quad_pipeline;

  std::vector<SegmentedQuadSampleDepthVertex> segmented_quad_sample_depth_vertices_cpu;
  DynamicArrayBuffer segmented_quad_sample_depth_vertices_gpu;
  gfx::PipelineHandle segmented_quad_sample_depth_pipeline;

  std::vector<CircleQuadInstance> circle_quad_sample_depth_instances_cpu;
  DynamicArrayBuffer circle_quad_sample_depth_instances_gpu;
  gfx::PipelineHandle circle_quad_sample_depth_pipeline;

  GeometryBuffer quad_buffer;

  Optional<VkDescriptorSet> sample_depth_desc_set0;
  bool need_remake_pipelines{};
};

Optional<GeometryBuffer> create_quad_geometry(gfx::Context* context) {
  GeometryBuffer result{};
  auto verts = geometry::quad_positions(false);
  auto verts_buff = gfx::create_device_local_vertex_buffer_sync(
    context, verts.size() * sizeof(float), verts.data());
  if (!verts_buff) {
    return NullOpt{};
  }

  auto inds = geometry::quad_indices();
  auto inds_buff = gfx::create_device_local_index_buffer_sync(
    context, inds.size() * sizeof(uint16_t), inds.data());
  if (!inds_buff) {
    return NullOpt{};
  }

  result.vertices = std::move(verts_buff.value());
  result.indices = std::move(inds_buff.value());
  result.num_indices = uint32_t(inds.size());
  return Optional<GeometryBuffer>(std::move(result));
}

template <typename T>
void set_common_segmented_quad_vertices(T& v, const SegmentedQuadVertexDescriptor& src) {
  memcpy(&v.position_and_color.x, &src.position.x, 3 * sizeof(float));
  auto col = clamp_each(src.color, Vec3f{}, Vec3f{1.0f}) * 255.0f;
  auto opac = uint8_t((1.0f - clamp01(src.translucency)) * 255.0f);
  v.position_and_color.w = pack::pack_4u8_1u32(uint8_t(col.x), uint8_t(col.y), uint8_t(col.z), opac);
}

void push_segmented_quad_vertices(
  SegmentedQuadVertex* dst_verts, const SegmentedQuadVertexDescriptor* src_descs, uint32_t n) {
  //
  for (uint32_t i = 0; i < n; i++) {
    assert(src_descs[i].min_depth_weight == 0.0f);
    set_common_segmented_quad_vertices(dst_verts[i], src_descs[i]);
  }
}

void push_segmented_quad_sample_depth_vertices(
  SegmentedQuadSampleDepthVertex* dst_verts,
  const SegmentedQuadVertexDescriptor* src_descs, uint32_t n) {
  //
  for (uint32_t i = 0; i < n; i++) {
    set_common_segmented_quad_vertices(dst_verts[i], src_descs[i]);
    dst_verts[i].min_depth_weight_unused.x = src_descs[i].min_depth_weight;
  }
}

void push_segmented_quad_particle_vertices(
  GPUContext* context, const SegmentedQuadVertexDescriptor* descs, uint32_t num_verts) {
  //
  assert((num_verts % 3) == 0);
  auto& cpu = context->segmented_quad_vertices_cpu;
  const auto dst = uint32_t(cpu.size());
  cpu.resize(dst + num_verts);
  push_segmented_quad_vertices(cpu.data() + dst, descs, num_verts);
}

void push_segmented_quad_sample_depth_particle_vertices(
  GPUContext* context, const SegmentedQuadVertexDescriptor* descs, uint32_t num_verts) {
  //
  assert((num_verts % 3) == 0);
  auto& cpu = context->segmented_quad_sample_depth_vertices_cpu;
  const auto dst = uint32_t(cpu.size());
  cpu.resize(dst + num_verts);
  push_segmented_quad_sample_depth_vertices(cpu.data() + dst, descs, num_verts);
}

void push_circle_quad_sample_depth_instances(
  GPUContext* context, const CircleQuadInstanceDescriptor* descs, uint32_t num_insts) {
  //
  auto& cpu = context->circle_quad_sample_depth_instances_cpu;
  const auto off = uint32_t(cpu.size());
  cpu.resize(off + num_insts);
  for (uint32_t i = 0; i < num_insts; i++) {
    auto& src = descs[i];
    auto& dst = cpu[off + i];
    dst.translation_scale = Vec4f{src.position, src.scale};
    dst.color_opacity = Vec4f{src.color, 1.0f - src.translucency};
  }
}

bool reserve(
  gfx::Context* graphics_context, DynamicArrayBuffer& buff, uint32_t count, size_t element_size) {
  //
  buff.size = 0;

  uint32_t num_res = buff.capacity;
  while (num_res < count) {
    num_res = num_res == 0 ? 64 : num_res * 2;
  }

  if (num_res != buff.capacity) {
    auto fq_depth = gfx::get_frame_queue_depth(graphics_context);
    auto buff_handle = gfx::create_host_visible_vertex_buffer(
      graphics_context, num_res * element_size * fq_depth);
    if (!buff_handle) {
      return false;
    } else {
      buff.buff = std::move(buff_handle.value());
      buff.capacity = num_res;
    }
  }

  buff.size = count;
  return true;
}

bool reserve_segmented_quad_vertices(GPUContext* context, gfx::Context* graphics_context) {
  const auto num_cpu = uint32_t(context->segmented_quad_vertices_cpu.size());
  auto& buff = context->segmented_quad_vertices_gpu;
  return reserve(graphics_context, buff, num_cpu, sizeof(SegmentedQuadVertex));
}

bool reserve_segmented_quad_sample_depth_vertices(GPUContext* context, gfx::Context* graphics_context) {
  const auto num_cpu = uint32_t(context->segmented_quad_sample_depth_vertices_cpu.size());
  auto& buff = context->segmented_quad_sample_depth_vertices_gpu;
  return reserve(graphics_context, buff, num_cpu, sizeof(SegmentedQuadSampleDepthVertex));
}

bool reserve_circle_quad_sample_depth_instances(GPUContext* context, gfx::Context* graphics_context) {
  const auto num_cpu = uint32_t(context->circle_quad_sample_depth_instances_cpu.size());
  auto& buff = context->circle_quad_sample_depth_instances_gpu;
  return reserve(graphics_context, buff, num_cpu, sizeof(CircleQuadInstance));
}

void require_circle_quad_sample_depth_buffer_pipeline(
  GPUContext* context, gfx::Context* graphics_context, bool remake) {
  //
  if (context->circle_quad_sample_depth_pipeline.is_valid() && !remake) {
    return;
  }

  auto create_source = []() {
    glsl::LoadVertFragProgramSourceParams params{};
    params.vert_file = "particle/circle-quad-sample-depth.glsl";
    params.compile.vert_defines.push_back(glsl::make_define("IS_VERTEX"));
    params.frag_file = "particle/circle-quad-sample-depth.glsl";
    return glsl::make_vert_frag_program_source(params);
  };

  auto pass_info = gfx::get_post_process_pass_handle(graphics_context);
  auto src = create_source();
  if (!src || !pass_info) {
    return;
  }

  VertexBufferDescriptor buff_descs[2];
  buff_descs[0].add_attribute(AttributeDescriptor::float2(0));
  buff_descs[1].add_attribute(AttributeDescriptor::float4(1, 1));
  buff_descs[1].add_attribute(AttributeDescriptor::float4(2, 1));

  gfx::GraphicsPipelineCreateInfo create_info{};
  create_info.disable_cull_face = true;
  create_info.num_color_attachments = 1;
  create_info.vertex_buffer_descriptors = buff_descs;
  create_info.num_vertex_buffer_descriptors = 2;
  create_info.enable_blend[0] = true;

  auto pipe_res = gfx::create_pipeline(
    graphics_context, std::move(src.value()), create_info, pass_info.value());
  if (pipe_res) {
    context->circle_quad_sample_depth_pipeline = std::move(pipe_res.value());
  }
}

void require_segmented_quad_sample_depth_buffer_pipeline(
  GPUContext* context, gfx::Context* graphics_context, bool remake) {
  //
  if (context->segmented_quad_sample_depth_pipeline.is_valid() && !remake) {
    return;
  }

  auto create_source = []() {
    glsl::LoadVertFragProgramSourceParams params{};
    params.vert_file = "particle/segmented-quad-sample-depth.glsl";
    params.compile.vert_defines.push_back(glsl::make_define("IS_VERTEX"));
    params.frag_file = "particle/segmented-quad-sample-depth.glsl";
    return glsl::make_vert_frag_program_source(params);
  };

  auto pass_info = gfx::get_post_process_pass_handle(graphics_context);
  auto src = create_source();
  if (!src || !pass_info) {
    return;
  }

  VertexBufferDescriptor buff_desc;
  buff_desc.add_attribute(AttributeDescriptor::unconverted_unsigned_intn(0, 4));
  buff_desc.add_attribute(AttributeDescriptor::float4(1));
  gfx::GraphicsPipelineCreateInfo create_info{};
  create_info.disable_cull_face = true;
  create_info.num_color_attachments = 1;
  create_info.vertex_buffer_descriptors = &buff_desc;
  create_info.num_vertex_buffer_descriptors = 1;
  create_info.enable_blend[0] = true;
  auto pipe_res = gfx::create_pipeline(
    graphics_context, std::move(src.value()), create_info, pass_info.value());
  if (pipe_res) {
    context->segmented_quad_sample_depth_pipeline = std::move(pipe_res.value());
  }
}

void require_segmented_quad_pipeline(GPUContext* context, gfx::Context* graphics_context) {
  auto create_source = []() {
    glsl::LoadVertFragProgramSourceParams params{};
    params.vert_file = "particle/segmented-quad.glsl";
    params.compile.vert_defines.push_back(glsl::make_define("IS_VERTEX"));
    params.frag_file = "particle/segmented-quad.glsl";
    return glsl::make_vert_frag_program_source(params);
  };

  if (!context->segmented_quad_pipeline.is_valid()) {
    auto src = create_source();
    if (src) {
      auto pass_info = gfx::get_forward_write_back_render_pass_handle(graphics_context);
      if (pass_info) {
        VertexBufferDescriptor buff_desc;
        buff_desc.add_attribute(AttributeDescriptor::unconverted_unsigned_intn(0, 4));
        gfx::GraphicsPipelineCreateInfo create_info{};
        create_info.disable_cull_face = true;
        create_info.num_color_attachments = 1;
        create_info.vertex_buffer_descriptors = &buff_desc;
        create_info.num_vertex_buffer_descriptors = 1;
        create_info.enable_blend[0] = true;
        auto pipe_res = gfx::create_pipeline(
          graphics_context, std::move(src.value()), create_info, pass_info.value());
        if (pipe_res) {
          context->segmented_quad_pipeline = std::move(pipe_res.value());
        }
      }
    }
  }
}

template <typename T>
void fill(DynamicArrayBuffer& dst, const std::vector<T>& src, uint32_t frame_index) {
  const auto num_src = uint32_t(src.size());
  if (num_src > 0) {
    assert(num_src == dst.size);
    auto off = dst.capacity * frame_index * sizeof(T);
    auto sz = num_src * sizeof(T);
    dst.buff.write(src.data(), sz, off);
  }
}

void fill_segmented_quad_vertices(GPUContext* context, uint32_t frame_index) {
  fill(context->segmented_quad_vertices_gpu, context->segmented_quad_vertices_cpu, frame_index);
}

void fill_segmented_quad_sample_depth_vertices(GPUContext* context, uint32_t frame_index) {
  auto& gpu = context->segmented_quad_sample_depth_vertices_gpu;
  auto& cpu = context->segmented_quad_sample_depth_vertices_cpu;
  fill(gpu, cpu, frame_index);
}

void fill_circle_quad_sample_depth_instances(GPUContext* context, uint32_t frame_index) {
  auto& gpu = context->circle_quad_sample_depth_instances_gpu;
  auto& cpu = context->circle_quad_sample_depth_instances_cpu;
  fill(gpu, cpu, frame_index);
}

Optional<VkDescriptorSet> require_sample_depth_desc_set0(
  const gfx::PipelineHandle& pipe, const BeginFrameInfo& info) {
  //
  if (!info.scene_depth_image) {
    return NullOpt{};
  }

  auto sampler = gfx::get_image_sampler_linear_edge_clamp(info.context);

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};
  vk::push_combined_image_sampler(scaffold, bind++, info.scene_depth_image.value(), sampler);

  return gfx::require_updated_descriptor_set(info.context, scaffold, pipe, true);
}

void try_fill_buffers(GPUContext* context, gfx::Context* graphics_context, uint32_t frame_index) {
  if (!context->quad_buffer.is_valid()) {
    if (auto qb = create_quad_geometry(graphics_context)) {
      context->quad_buffer = std::move(qb.value());
    }
  }

  if (reserve_segmented_quad_vertices(context, graphics_context)) {
    fill_segmented_quad_vertices(context, frame_index);
  }

  if (reserve_segmented_quad_sample_depth_vertices(context, graphics_context)) {
    fill_segmented_quad_sample_depth_vertices(context, frame_index);
  }

  if (reserve_circle_quad_sample_depth_instances(context, graphics_context)) {
    fill_circle_quad_sample_depth_instances(context, frame_index);
  }
}

void try_require_pipelines(GPUContext* context, gfx::Context* graphics_context, bool remake) {
  require_segmented_quad_pipeline(context, graphics_context);
  require_segmented_quad_sample_depth_buffer_pipeline(context, graphics_context, remake);
  require_circle_quad_sample_depth_buffer_pipeline(context, graphics_context, remake);
}

void clear_cpu_data(GPUContext* context) {
  context->segmented_quad_vertices_cpu.clear();
  context->segmented_quad_sample_depth_vertices_cpu.clear();
  context->circle_quad_sample_depth_instances_cpu.clear();
}

void begin_frame(GPUContext* context, const BeginFrameInfo& info) {
  gfx::Context* graphics_context = info.context;
  const uint32_t frame_index = info.frame_index;

  const bool remake = context->need_remake_pipelines;
  context->need_remake_pipelines = false;

  try_require_pipelines(context, graphics_context, remake);
  try_fill_buffers(context, graphics_context, frame_index);

  if (context->segmented_quad_sample_depth_pipeline.is_valid()) {
    context->sample_depth_desc_set0 = require_sample_depth_desc_set0(
      context->segmented_quad_sample_depth_pipeline, info);
  }

  clear_cpu_data(context);
}

Mat4f get_projection_view(const Camera& camera) {
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  return proj * camera.get_view();
}

void render_segmented_quad(GPUContext* context, const RenderInfo& info) {
  auto& gpu_buff = context->segmented_quad_vertices_gpu;
  if (gpu_buff.size == 0 || !context->segmented_quad_pipeline.is_valid()) {
    return;
  }

  SegmentedQuadPushConstantData pc_data{};
  pc_data.projection_view = get_projection_view(info.camera);

  auto& pipe = context->segmented_quad_pipeline;
  vk::cmd::bind_graphics_pipeline(info.cmd, pipe.get());
  vk::cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor);
  vk::cmd::push_constants(info.cmd, pipe.get_layout(), VK_SHADER_STAGE_VERTEX_BIT, &pc_data);

  VkBuffer buffs[1] = {gpu_buff.buff.get()};
  VkDeviceSize buff_offs[1] = {info.frame_index * gpu_buff.capacity * sizeof(SegmentedQuadVertex)};
  vkCmdBindVertexBuffers(info.cmd, 0, 1, buffs, buff_offs);

  vk::DrawDescriptor draw_desc{};
  draw_desc.num_instances = 1;
  draw_desc.num_vertices = gpu_buff.size;
  vk::cmd::draw(info.cmd, &draw_desc);
}

void render_segmented_quad_sample_depth(GPUContext* context, const RenderInfo& info) {
  const auto& gpu_buff = context->segmented_quad_sample_depth_vertices_gpu;
  const auto& pipe = context->segmented_quad_sample_depth_pipeline;
  const auto& desc_set0 = context->sample_depth_desc_set0;

  if (gpu_buff.size == 0 || !pipe.is_valid() || !desc_set0) {
    return;
  }

  SegmentedQuadPushConstantData pc_data{};
  pc_data.projection_view = get_projection_view(info.camera);

  vk::cmd::bind_graphics_pipeline(info.cmd, pipe.get());
  vk::cmd::bind_graphics_descriptor_sets(info.cmd, pipe.get_layout(), 0, 1, &desc_set0.value());
  vk::cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor);
  vk::cmd::push_constants(info.cmd, pipe.get_layout(), VK_SHADER_STAGE_VERTEX_BIT, &pc_data);

  VkBuffer buffs[1] = {gpu_buff.buff.get()};
  VkDeviceSize buff_offs[1] = {
    info.frame_index * gpu_buff.capacity * sizeof(SegmentedQuadSampleDepthVertex)};
  vkCmdBindVertexBuffers(info.cmd, 0, 1, buffs, buff_offs);

  vk::DrawDescriptor draw_desc{};
  draw_desc.num_instances = 1;
  draw_desc.num_vertices = gpu_buff.size;
  vk::cmd::draw(info.cmd, &draw_desc);
}

void render_circle_quad_sample_depth(GPUContext* context, const RenderInfo& info) {
  const auto& inst_buff = context->circle_quad_sample_depth_instances_gpu;
  const auto& geom_buff = context->quad_buffer;
  const auto& pipe = context->circle_quad_sample_depth_pipeline;
  const auto& desc_set0 = context->sample_depth_desc_set0;

  if (inst_buff.size == 0 || !pipe.is_valid() || !desc_set0 || !geom_buff.is_valid()) {
    return;
  }

  auto inv_view = transpose(info.camera.get_view());
  CircleQuadPushConstantData pc_data{};
  pc_data.projection_view = get_projection_view(info.camera);
  pc_data.inv_view = inv_view;

  vk::cmd::bind_graphics_pipeline(info.cmd, pipe.get());
  vk::cmd::bind_graphics_descriptor_sets(info.cmd, pipe.get_layout(), 0, 1, &desc_set0.value());
  vk::cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor);
  vk::cmd::push_constants(info.cmd, pipe.get_layout(), VK_SHADER_STAGE_VERTEX_BIT, &pc_data);

  VkBuffer buffs[2] = {geom_buff.vertices.get(), inst_buff.buff.get()};
  VkDeviceSize buff_offs[2] = {0, info.frame_index * inst_buff.capacity * sizeof(CircleQuadInstance)};
  vkCmdBindVertexBuffers(info.cmd, 0, 2, buffs, buff_offs);
  vkCmdBindIndexBuffer(info.cmd, geom_buff.indices.get(), 0, VK_INDEX_TYPE_UINT16);

  vk::DrawIndexedDescriptor draw_desc{};
  draw_desc.num_instances = inst_buff.size;
  draw_desc.num_indices = geom_buff.num_indices;
  vk::cmd::draw_indexed(info.cmd, &draw_desc);
}

void render_forward(GPUContext* context, const RenderInfo& info) {
  render_segmented_quad(context, info);
}

void render_post_process(GPUContext* context, const RenderInfo& info) {
  render_segmented_quad_sample_depth(context, info);
  render_circle_quad_sample_depth(context, info);
}

struct {
  GPUContext context;
} globals;

} //  anon

void particle::push_segmented_quad_particle_vertices(
  const SegmentedQuadVertexDescriptor* descs, uint32_t num_verts) {
  //
  push_segmented_quad_particle_vertices(&globals.context, descs, num_verts);
}

void particle::push_segmented_quad_sample_depth_image_particle_vertices(
  const SegmentedQuadVertexDescriptor* descs, uint32_t num_verts) {
  //
  push_segmented_quad_sample_depth_particle_vertices(&globals.context, descs, num_verts);
}

void particle::push_circle_quad_sample_depth_instances(
  const CircleQuadInstanceDescriptor* descs, uint32_t num_insts) {
  //
  push_circle_quad_sample_depth_instances(&globals.context, descs, num_insts);
}

void particle::render_particles_begin_frame(const BeginFrameInfo& info) {
  begin_frame(&globals.context, info);
}

void particle::render_particles_render_forward(const RenderInfo& info) {
  render_forward(&globals.context, info);
}

void particle::render_particles_render_post_process(const RenderInfo& info) {
  render_post_process(&globals.context, info);
}

void particle::set_render_particles_need_remake_pipelines() {
  globals.context.need_remake_pipelines = true;
}

Stats particle::get_render_particles_stats() {
  const auto& ctx = globals.context;
  Stats result{};
  result.last_num_segmented_quad_vertices = int(ctx.segmented_quad_vertices_gpu.size);
  result.last_num_segmented_quad_sample_depth_vertices =
    int(ctx.segmented_quad_sample_depth_vertices_gpu.size);
  result.last_num_circle_quad_sample_depth_instances =
    int(ctx.circle_quad_sample_depth_instances_gpu.size);
  return result;
}

void particle::terminate_particle_renderer() {
  globals.context = {};
}

GROVE_NAMESPACE_END
