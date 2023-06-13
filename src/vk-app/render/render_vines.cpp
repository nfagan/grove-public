#include "render_vines.hpp"
#include "graphics.hpp"
#include "../vk/vk.hpp"
#include "DynamicSampledImageManager.hpp"
#include "../procedural_tree/render_vine_system.hpp"
#include "./debug_label.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/common/common.hpp"
#include "grove/math/triangle.hpp"
#include <bitset>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

using BeginFrameInfo = RenderVinesBeginFrameInfo;
using RenderForwardInfo = RenderVinesForwardRenderInfo;

struct ForwardPushConstantData {
  Mat4f projection_view;
  Vec4f vine_color_t;
  Vec4f wind_world_bound_xz;
  Vec4f wind_displacement_limits_wind_strength_limits;
};

struct GeometryBuffer {
  gfx::BufferHandle geom;
  gfx::BufferHandle index;
  uint32_t num_indices{};
  bool is_valid{};
};

struct RenderVinesData {
  gfx::BufferHandle instance_buffer;
  bool instance_buffer_valid{};

  gfx::BufferHandle aggregate_buffer;
  bool aggregate_buffer_valid{};

  gfx::PipelineHandle alt_forward_pipeline;
  Optional<VkDescriptorSet> alt_forward_desc_set0;

  Optional<vk::DynamicSampledImageManager::Handle> wind_image;

  GeometryBuffer geometry_buffer;
  bool tried_initialize{};
  bool need_remake_programs{};

//  Vec3f vine_color{0.192f, 0.218f, 0.087f}; //  orig
  Vec3f vine_color{0.07f, 0.056f, 0.0f}; //  darker
  Vec4f wind_world_bound_xz{};
  Vec2f wind_displacement_limits{};
  Vec2f wind_strength_limits{};
  float elapsed_time{};

  uint32_t num_instances_reserved{};
  uint32_t num_instances_active{};
  uint32_t num_aggregates_reserved{};
  uint32_t num_aggregates_active{};
  std::bitset<32> instance_data_modified{};
};

ForwardPushConstantData make_forward_push_constant_data(const Camera& camera, const Vec3f& color,
                                                        float elapsed_time,
                                                        const Vec4f& wind_world_bound_xz,
                                                        const Vec2f& wind_displacement_limits,
                                                        const Vec2f& wind_strength_limits) {
  ForwardPushConstantData result{};
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  result.projection_view = proj * camera.get_view();
  result.vine_color_t = Vec4f{color, elapsed_time};
  result.wind_world_bound_xz = wind_world_bound_xz;
  result.wind_displacement_limits_wind_strength_limits = Vec4f{
    wind_displacement_limits.x, wind_displacement_limits.y,
    wind_strength_limits.x, wind_strength_limits.y
  };
  return result;
}

Optional<GeometryBuffer> create_geometry(gfx::Context* context) {
  const Vec3f ps[8]{
    /*0*/ Vec3f{-0.5f, -0.5f, 0.0f},
    /*1*/ Vec3f{0.5f, -0.5f, 0.0f},
    /*2*/ Vec3f{0.5f, 0.5f, 0.0f},
    /*3*/ Vec3f{-0.5f, 0.5f, 0.0f},
    /*4*/ Vec3f{-0.5f, -0.5f, 1.0f},
    /*5*/ Vec3f{0.5f, -0.5f, 1.0f},
    /*6*/ Vec3f{0.5f, 0.5f, 1.0f},
    /*7*/ Vec3f{-0.5f, 0.5f, 1.0f},
  };

#if 1
  uint16_t tris[24]{
    1, 5, 2,  //  r
    2, 5, 6,
    4, 0, 7,  //  l
    7, 0, 3,
    3, 2, 6,  //  t
    6, 7, 3,
    0, 4, 1,  //  b
    1, 4, 5,
  };
  for (int i = 0; i < 8; i++) {
    std::swap(tris[i * 3 + 1], tris[i * 3 + 2]);
    assert(tri::is_ccw(ps[tris[i * 3 + 0]], ps[tris[i * 3 + 1]], ps[tris[i * 3 + 2]]));
  }
#else
  const uint16_t tris[24] {
    1, 5, 2,
    5, 6, 2,
    4, 0, 7,
    7, 0, 3,
    2, 6, 3,
    6, 7, 3,
    5, 1, 0,
    4, 5, 0,
  };
#endif

  const auto num_inds = uint32_t(sizeof(tris) / sizeof(uint16_t));
  auto ind_res = gfx::create_device_local_index_buffer_sync(
    context, num_inds * sizeof(uint16_t), tris);
  if (!ind_res) {
    return NullOpt{};
  }

  auto geom_res = gfx::create_device_local_vertex_buffer_sync(
    context, 8 * 3 * sizeof(float), &ps[0].x);
  if (!geom_res) {
    return NullOpt{};
  }

  GeometryBuffer result{};
  result.index = std::move(ind_res.value());
  result.geom = std::move(geom_res.value());
  result.num_indices = num_inds;
  result.is_valid = true;
  return Optional<GeometryBuffer>(std::move(result));
}

void set_instance_data_modified(RenderVinesData* data, uint32_t frame_queue_depth) {
  for (uint32_t i = 0; i < frame_queue_depth; i++) {
    data->instance_data_modified[i] = true;
  }
}

Optional<glsl::VertFragProgramSource> create_render_forward_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "proc-tree/vine.vert";
  params.frag_file = "proc-tree/vine.frag";
  params.reflect.to_vk_descriptor_type = vk::refl::always_dynamic_storage_buffer_descriptor_type;
  return glsl::make_vert_frag_program_source(params);
}

void set_vertex_attribute_descriptors(VertexBufferDescriptor* buff_descs) {
  int loc{};
  buff_descs[0].add_attribute(AttributeDescriptor::float3(loc++));
  buff_descs[1].add_attribute(AttributeDescriptor::float4(loc++, 1));
  buff_descs[1].add_attribute(AttributeDescriptor::float4(loc++, 1));
  buff_descs[1].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(loc++, 4, 1));
  buff_descs[1].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(loc++, 4, 1));
  buff_descs[1].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(loc++, 4, 1));
  buff_descs[1].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(loc++, 4, 1));
  buff_descs[1].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(loc++, 4, 1));
  buff_descs[1].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(loc++, 4, 1));
}

bool create_alt_forward_pipeline(RenderVinesData* data, const BeginFrameInfo& info) {
  auto pass = gfx::get_forward_write_back_render_pass_handle(info.graphics_context);
  if (!pass) {
    return false;
  }

  auto source = create_render_forward_program_source();
  if (!source) {
    return false;
  }

  VertexBufferDescriptor buff_descs[2];
  set_vertex_attribute_descriptors(buff_descs);

  gfx::GraphicsPipelineCreateInfo create_info{};
  create_info.vertex_buffer_descriptors = buff_descs;
  create_info.num_vertex_buffer_descriptors = 2;
  create_info.num_color_attachments = 1;
  auto pipe_res = gfx::create_pipeline(
    info.graphics_context, std::move(source.value()), create_info, pass.value());

  if (!pipe_res) {
    return false;
  }

  data->alt_forward_pipeline = std::move(pipe_res.value());
  return true;
}

void create_geometry_buffer(RenderVinesData* data, const BeginFrameInfo& info) {
  auto geom = create_geometry(info.graphics_context);
  if (geom) {
    data->geometry_buffer = std::move(geom.value());
  }
}

void reserve_instance_buffer(RenderVinesData* data, uint32_t num_nodes, const BeginFrameInfo& info) {
  uint32_t num_reserve = data->num_instances_reserved;
  while (num_reserve < num_nodes) {
    num_reserve = num_reserve == 0 ? 64 : num_reserve * 2;
  }

  if (num_reserve != data->num_instances_reserved) {
    const size_t buff_size = num_reserve * sizeof(VineRenderNode) * info.frame_queue_depth;
    auto buff = gfx::create_host_visible_vertex_buffer(info.graphics_context, buff_size);
    if (!buff) {
      data->instance_buffer_valid = false;
      return;
    } else {
      data->instance_buffer = std::move(buff.value());
      data->num_instances_reserved = num_reserve;
      data->instance_buffer_valid = true;
    }

    set_instance_data_modified(data, info.frame_queue_depth);
  }
}

void reserve_aggregate_buffer(RenderVinesData* data, uint32_t num_aggregates,
                              const BeginFrameInfo& info) {
  uint32_t num_reserve = data->num_aggregates_reserved;
  while (num_reserve < num_aggregates) {
    num_reserve = num_reserve == 0 ? 64 : num_reserve * 2;
  }

  if (num_reserve != data->num_aggregates_reserved) {
    const size_t el_size = sizeof(VineAttachedToAggregateRenderData);
    const size_t buff_size = num_reserve * el_size * info.frame_queue_depth;
    auto buff = gfx::create_storage_buffer(info.graphics_context, buff_size);
    if (!buff) {
      data->aggregate_buffer_valid = false;
      return;
    } else {
      data->aggregate_buffer = std::move(buff.value());
      data->num_aggregates_reserved = num_reserve;
      data->aggregate_buffer_valid = true;
    }

    set_instance_data_modified(data, info.frame_queue_depth);
  }
}

void require_forward_desc_set0(RenderVinesData* data, const BeginFrameInfo& info) {
  data->alt_forward_desc_set0 = NullOpt{};

  if (!data->wind_image || !data->alt_forward_pipeline.is_valid() || !data->aggregate_buffer_valid) {
    return;
  }

  auto& dyn_image_manager = *info.dynamic_sampled_image_manager;
  Optional<vk::SampleImageView> wind_image;
  if (auto inst = dyn_image_manager.get(data->wind_image.value())) {
    if (inst.value().vertex_shader_sample_ok() && inst.value().is_2d()) {
      wind_image = inst.value().to_sample_image_view();
    }
  }
  if (!wind_image) {
    return;
  }

  auto wind_sampler = gfx::get_image_sampler_linear_repeat(info.graphics_context);

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};

  vk::push_dynamic_storage_buffer(
    scaffold, bind++, data->aggregate_buffer.get(),
    data->num_aggregates_active * sizeof(VineAttachedToAggregateRenderData));
  vk::push_combined_image_sampler(scaffold, bind++, wind_image.value(), wind_sampler);

  auto maybe_set = gfx::require_updated_descriptor_set(
    info.graphics_context, scaffold, data->alt_forward_pipeline);
  if (maybe_set) {
    data->alt_forward_desc_set0 = maybe_set.value();
  }
}

void begin_frame(RenderVinesData* data, const BeginFrameInfo& info) {
  if (!data->tried_initialize) {
    create_alt_forward_pipeline(data, info);
    create_geometry_buffer(data, info);
    data->tried_initialize = true;
  }

  if (data->need_remake_programs) {
    create_alt_forward_pipeline(data, info);
    data->need_remake_programs = false;
  }

  if (test_clear_render_nodes_modified(info.render_vine_system)) {
    set_instance_data_modified(data, info.frame_queue_depth);
  }

  const auto view_nodes = read_vine_render_nodes(info.render_vine_system);
  const auto view_aggregates = read_vine_attached_to_aggregate_render_data(info.render_vine_system);

  reserve_instance_buffer(data, uint32_t(view_nodes.size()), info);
  reserve_aggregate_buffer(data, uint32_t(view_aggregates.size()), info);

  if (data->instance_data_modified[info.frame_index] && data->instance_buffer_valid &&
      data->aggregate_buffer_valid) {
    {
      const auto num_active = uint32_t(view_nodes.size());
      data->num_instances_active = num_active;
      const size_t off = info.frame_index * data->num_instances_reserved * sizeof(VineRenderNode);
      data->instance_buffer.write(view_nodes.data(), num_active * sizeof(VineRenderNode), off);
    }
    {
      const auto num_active = uint32_t(view_aggregates.size());
      data->num_aggregates_active = num_active;
      const size_t el_size = sizeof(VineAttachedToAggregateRenderData);
      const size_t off = info.frame_index * data->num_aggregates_reserved * el_size;
      data->aggregate_buffer.write(view_aggregates.data(), num_active * el_size, off);
    }
    data->instance_data_modified[info.frame_index] = false;
  }

  require_forward_desc_set0(data, info);
}

ForwardPushConstantData make_forward_push_constant_data(RenderVinesData* data,
                                                        const RenderForwardInfo& info) {
  return make_forward_push_constant_data(
    info.camera,
    data->vine_color,
    data->elapsed_time,
    data->wind_world_bound_xz,
    data->wind_displacement_limits,
    data->wind_strength_limits);
}

void render_forward(RenderVinesData* data, const RenderForwardInfo& info) {
  if (!data->instance_buffer_valid || !data->alt_forward_pipeline.is_valid() ||
      !data->geometry_buffer.is_valid || !data->alt_forward_desc_set0) {
    return;
  }

  auto db_label = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "render_vines_forward");
  (void) db_label;

  VkPipeline pipeline = data->alt_forward_pipeline.get();
  VkPipelineLayout layout = data->alt_forward_pipeline.get_layout();

  auto pc_data = make_forward_push_constant_data(data, info);
  vk::cmd::bind_graphics_pipeline(info.cmd, pipeline);
  vk::cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor);
  vk::cmd::push_constants(info.cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, &pc_data);

  using Aggregate = VineAttachedToAggregateRenderData;
  const uint32_t dyn_offs[1] = {
    uint32_t(data->num_aggregates_reserved * sizeof(Aggregate) * info.frame_index)
  };

  vk::cmd::bind_graphics_descriptor_sets(
    info.cmd, layout, 0, 1, &data->alt_forward_desc_set0.value(), 1, dyn_offs);

  VkBuffer vert_buffs[2]{
    data->geometry_buffer.geom.get(),
    data->instance_buffer.get()
  };
  VkDeviceSize vb_offs[2]{
    0,
    sizeof(VineRenderNode) * data->num_instances_reserved * info.frame_index
  };

  VkBuffer ind_buff = data->geometry_buffer.index.get();
  vkCmdBindVertexBuffers(info.cmd, 0, 2, vert_buffs, vb_offs);
  vkCmdBindIndexBuffer(info.cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);

  vk::DrawIndexedDescriptor draw_desc{};
  draw_desc.num_instances = data->num_instances_active;
  draw_desc.num_indices = data->geometry_buffer.num_indices;
  vk::cmd::draw_indexed(info.cmd, &draw_desc);
}

struct {
  RenderVinesData render_vines_data;
} globals;

} //  anon

void tree::render_vines_begin_frame(const RenderVinesBeginFrameInfo& info) {
  begin_frame(&globals.render_vines_data, info);
}

void tree::terminate_vine_renderer() {
  globals.render_vines_data = {};
}

void tree::render_vines_forward(const RenderVinesForwardRenderInfo& info) {
  grove::render_forward(&globals.render_vines_data, info);
}

void tree::set_render_vines_wind_displacement_image(uint32_t handle_id) {
  globals.render_vines_data.wind_image = vk::DynamicSampledImageManager::Handle{handle_id};
}

void tree::set_render_vines_need_remake_programs() {
  globals.render_vines_data.need_remake_programs = true;
}

void tree::set_render_vines_elapsed_time(float t) {
  globals.render_vines_data.elapsed_time = t;
}

Vec3f tree::get_render_vines_color() {
  return globals.render_vines_data.vine_color;
}

void tree::set_render_vines_color(const Vec3f& c) {
  globals.render_vines_data.vine_color = c;
}

void tree::set_render_vines_wind_info(const Vec4f& wind_world_bound_xz,
                                      const Vec2f& wind_displacement_limits,
                                      const Vec2f& wind_strength_limits) {
  globals.render_vines_data.wind_world_bound_xz = wind_world_bound_xz;
  globals.render_vines_data.wind_displacement_limits = wind_displacement_limits;
  globals.render_vines_data.wind_strength_limits = wind_strength_limits;
}

GROVE_NAMESPACE_END
