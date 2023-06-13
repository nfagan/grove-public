#include "render_gui_gpu.hpp"
#include "render_gui_data.hpp"
#include "graphics.hpp"
#include "font.hpp"
#include "SampledImageManager.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using BeginFrameInfo = gui::RenderGUIBeginFrameInfo;
using RenderInfo = gui::RenderGUIRenderInfo;

struct Config {
  static constexpr int max_num_gui_layers = 2;
};

struct QuadPushConstantData {
  Vec4f framebuffer_dimensions;
};

struct DynamicArrayBuffer {
  gfx::BufferHandle buffer;
  uint32_t num_reserved{};
  uint32_t num_active{};
};

struct GPUContext {
  gfx::PipelineHandle quad_pipeline;
  DynamicArrayBuffer quad_vertices[Config::max_num_gui_layers];
  DynamicArrayBuffer quad_indices[Config::max_num_gui_layers];
  bool quad_buffers_valid[Config::max_num_gui_layers]{};

  gfx::PipelineHandle glyph_pipeline;
  DynamicArrayBuffer glyph_vertices[Config::max_num_gui_layers];
  DynamicArrayBuffer glyph_indices[Config::max_num_gui_layers];
  bool glyph_buffers_valid[Config::max_num_gui_layers]{};

  Optional<uint32_t> glyph_image;
  Optional<VkDescriptorSet> glyph_desc_set0;

  bool need_remake_pipelines{true};
};

Optional<uint32_t> create_font_image(
  vk::SampledImageManager& image_manager, const unsigned char** image_data,
  int num_images, int image_dim) {
  //
  auto packed_data = std::make_unique<unsigned char[]>(image_dim * image_dim * num_images);
  for (int i = 0; i < num_images; i++) {
    auto i0 = image_dim * image_dim * i;
    auto s = image_dim * image_dim;
    memcpy(packed_data.get() + i0, image_data[i], s);
  }

  vk::SampledImageManager::ImageCreateInfo create_info{};
  create_info.data = packed_data.get();
  create_info.descriptor = image::Descriptor{
    image::Shape::make_3d(image_dim, image_dim, num_images),
    image::Channels::make_uint8n(1)
  };
  create_info.format = VK_FORMAT_R8_UNORM;
  create_info.sample_in_stages = {vk::PipelineStage::FragmentShader};
  create_info.image_type = vk::SampledImageManager::ImageType::Image2DArray;

  auto create_res = image_manager.create_sync(create_info);
  if (create_res) {
    return Optional<uint32_t>(create_res.value().id);
  } else {
    return NullOpt{};
  }
}

bool reserve(gfx::Context* graphics_context, DynamicArrayBuffer& buff,
             uint32_t count, uint32_t frame_queue_depth, size_t element_size, bool is_index) {
  buff.num_active = 0;

  auto num_reserve = buff.num_reserved;
  while (num_reserve < count) {
    num_reserve = num_reserve == 0 ? 128 : num_reserve * 2;
  }

  if (num_reserve != buff.num_reserved) {
    gfx::BufferHandle dst_buff;
    auto sz = num_reserve * frame_queue_depth * element_size;
    if (is_index) {
      if (auto gpu_buff = gfx::create_host_visible_index_buffer(graphics_context, sz)) {
        dst_buff = std::move(gpu_buff.value());
      }
    } else {
      if (auto gpu_buff = gfx::create_host_visible_vertex_buffer(graphics_context, sz)) {
        dst_buff = std::move(gpu_buff.value());
      }
    }
    if (!dst_buff.is_valid()) {
      return false;
    } else {
      buff.num_reserved = num_reserve;
      buff.buffer = std::move(dst_buff);
    }
  }

  buff.num_active = count;
  return true;
}

Optional<gfx::PipelineHandle> create_glyph_pipeline(gfx::Context* graphics_context) {
  auto get_source = []() {
    glsl::LoadVertFragProgramSourceParams params{};
    params.vert_file = "ui/glyph.glsl";
    params.compile.vert_defines.push_back(glsl::make_define("IS_VERTEX"));
    params.frag_file = "ui/glyph.glsl";
    return glsl::make_vert_frag_program_source(params);
  };

  auto source = get_source();
  if (!source) {
    return NullOpt{};
  }

  int loc{};
  VertexBufferDescriptor buff_descs[1];
  buff_descs[0].add_attribute(AttributeDescriptor::float4(loc++));
  buff_descs[0].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(loc++, 4));

  auto pass = gfx::get_post_process_pass_handle(graphics_context);
  if (!pass) {
    return NullOpt{};
  }

  gfx::GraphicsPipelineCreateInfo create_info{};
  create_info.num_vertex_buffer_descriptors = 1;
  create_info.vertex_buffer_descriptors = buff_descs;
  create_info.num_color_attachments = 1;
  create_info.enable_blend[0] = true;
  create_info.depth_compare_op = gfx::DepthCompareOp::LessOrEqual;
  create_info.cull_mode = gfx::CullMode::Front;
  return gfx::create_pipeline(graphics_context, std::move(source.value()), create_info, pass.value());
}

Optional<gfx::PipelineHandle> create_quad_pipeline(gfx::Context* graphics_context) {
  auto get_source = []() {
    glsl::LoadVertFragProgramSourceParams params{};
    params.vert_file = "ui/quad.vert";
    params.frag_file = "ui/quad.frag";
    return glsl::make_vert_frag_program_source(params);
  };

  auto source = get_source();
  if (!source) {
    return NullOpt{};
  }

  int loc{};
  VertexBufferDescriptor buff_descs[1];
  buff_descs[0].add_attribute(AttributeDescriptor::float4(loc++));
  buff_descs[0].add_attribute(AttributeDescriptor::float4(loc++));
  buff_descs[0].add_attribute(AttributeDescriptor::float4(loc++));
  buff_descs[0].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(loc++, 4));

  auto pass = gfx::get_post_process_pass_handle(graphics_context);
  if (!pass) {
    return NullOpt{};
  }

  gfx::GraphicsPipelineCreateInfo create_info{};
  create_info.num_vertex_buffer_descriptors = 1;
  create_info.vertex_buffer_descriptors = buff_descs;
  create_info.num_color_attachments = 1;
  create_info.enable_blend[0] = true;
  create_info.depth_compare_op = gfx::DepthCompareOp::LessOrEqual;
  create_info.cull_mode = gfx::CullMode::Front;
  return gfx::create_pipeline(graphics_context, std::move(source.value()), create_info, pass.value());
}

template <typename T>
bool update_draw_buffer(DynamicArrayBuffer& buff, const std::vector<T>& src,
                        const BeginFrameInfo& info, bool is_index) {
  const uint32_t frame_queue_depth = gfx::get_frame_queue_depth(info.context);

  if (reserve(info.context, buff, uint32_t(src.size()), frame_queue_depth, sizeof(T), is_index) &&
      !src.empty()) {
    size_t off = info.frame_index * sizeof(T) * buff.num_reserved;
    buff.buffer.write(src.data(), uint32_t(src.size()) * sizeof(T), off);
    return true;
  } else {
    return false;
  }
}

void update_quad_draw_buffers(GPUContext* context, const BeginFrameInfo& info) {
  const int num_update = std::min(Config::max_num_gui_layers, gui::RenderData::max_num_gui_layers);
  auto& rd = *info.render_data;
  for (int i = 0; i < num_update; i++) {
    bool qb_valid{true};
    if (!update_draw_buffer(context->quad_vertices[i], rd.quad_vertices[i], info, false)) {
      qb_valid = false;
    }
    if (!update_draw_buffer(context->quad_indices[i], rd.quad_vertex_indices[i], info, true)) {
      qb_valid = false;
    }
    context->quad_buffers_valid[i] = qb_valid;
  }
}

void update_glyph_draw_buffers(GPUContext* context, const BeginFrameInfo& info) {
  auto& rd = *info.render_data;
  const int num_update = std::min(Config::max_num_gui_layers, gui::RenderData::max_num_gui_layers);
  for (int i = 0; i < num_update; i++) {
    bool gb_valid{true};
    if (!update_draw_buffer(context->glyph_vertices[i], rd.glyph_vertices[i], info, false)) {
      gb_valid = false;
    }
    if (!update_draw_buffer(context->glyph_indices[i], rd.glyph_vertex_indices[i], info, true)) {
      gb_valid = false;
    }
    context->glyph_buffers_valid[i] = gb_valid;
  }
}

void update_draw_buffers(GPUContext* context, const BeginFrameInfo& info) {
  update_quad_draw_buffers(context, info);
  update_glyph_draw_buffers(context, info);
}

void acquire_glyph_desc_set0(GPUContext* context, const BeginFrameInfo& info) {
  if (!context->glyph_image || !context->glyph_pipeline.is_valid()) {
    return;
  }

  vk::SampledImageManager::Handle im_handle{context->glyph_image.value()};
  auto im = info.sampled_image_manager.get(im_handle);
  if (!im || !im.value().is_2d_array() || !im.value().fragment_shader_sample_ok()) {
    return;
  }

  auto& rd = *info.render_data;
  if (int(rd.max_glyph_image_index) >= im.value().descriptor.shape.depth) {
    assert(false);
    return;
  }

  auto sampler_linear = gfx::get_image_sampler_linear_edge_clamp(info.context);
  uint32_t bind{};
  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  vk::push_combined_image_sampler(scaffold, bind++, im.value().to_sample_image_view(), sampler_linear);

  context->glyph_desc_set0 = gfx::require_updated_descriptor_set(
    info.context, scaffold, context->glyph_pipeline);
}

void begin_frame(GPUContext* context, const BeginFrameInfo& info) {
  if (context->need_remake_pipelines) {
    if (auto pipe = create_quad_pipeline(info.context)) {
      context->quad_pipeline = std::move(pipe.value());
    }
    if (auto pipe = create_glyph_pipeline(info.context)) {
      context->glyph_pipeline = std::move(pipe.value());
    }
    context->need_remake_pipelines = false;
  }

  if (!context->glyph_image) {
    if (auto read_ims = font::read_font_images()) {
      auto& ims = read_ims.value();
      context->glyph_image = create_font_image(
        info.sampled_image_manager, ims.images, ims.num_images, ims.image_dim);
    }
  }

  acquire_glyph_desc_set0(context, info);
  update_draw_buffers(context, info);
}

void render_glyphs(GPUContext* context, int layer, const RenderInfo& info) {
  auto& pipe = context->glyph_pipeline;
  vk::cmd::bind_graphics_pipeline(info.cmd, pipe.get());
  vk::cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor);

  vk::cmd::bind_graphics_descriptor_sets(
    info.cmd, pipe.get_layout(), 0, 1, &context->glyph_desc_set0.unwrap());

  QuadPushConstantData pc_data{};
  pc_data.framebuffer_dimensions = Vec4f{
    float(info.viewport.width), float(info.viewport.height), 0.0f, 0.0f
  };
  vk::cmd::push_constants(info.cmd, pipe.get_layout(), VK_SHADER_STAGE_VERTEX_BIT, &pc_data);

  VkBuffer vbs[1] = {context->glyph_vertices[layer].buffer.get()};
  const VkDeviceSize vb_offs[1] = {
    sizeof(gui::GlyphQuadVertex) * context->glyph_vertices[layer].num_reserved * info.frame_index
  };

  vkCmdBindVertexBuffers(info.cmd, 0, 1, vbs, vb_offs);

  VkBuffer ind_buff = context->glyph_indices[layer].buffer.get();
  const size_t off = sizeof(uint16_t) * context->glyph_indices[layer].num_reserved * info.frame_index;
  vkCmdBindIndexBuffer(info.cmd, ind_buff, off, VK_INDEX_TYPE_UINT16);

  vk::DrawIndexedDescriptor draw_desc{};
  draw_desc.num_instances = 1;
  draw_desc.num_indices = context->glyph_indices[layer].num_active;
  vk::cmd::draw_indexed(info.cmd, &draw_desc);
}

void render_quads(GPUContext* context, int layer, const RenderInfo& info) {
  auto& pipe = context->quad_pipeline;
  vk::cmd::bind_graphics_pipeline(info.cmd, pipe.get());
  vk::cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor);

  QuadPushConstantData pc_data{};
  pc_data.framebuffer_dimensions = Vec4f{
    float(info.viewport.width), float(info.viewport.height), 0.0f, 0.0f
  };
  vk::cmd::push_constants(info.cmd, pipe.get_layout(), VK_SHADER_STAGE_VERTEX_BIT, &pc_data);

  VkBuffer vbs[1] = {context->quad_vertices[layer].buffer.get()};
  const VkDeviceSize vb_offs[1] = {
    sizeof(gui::QuadVertex) * context->quad_vertices[layer].num_reserved * info.frame_index
  };

  vkCmdBindVertexBuffers(info.cmd, 0, 1, vbs, vb_offs);

  VkBuffer ind_buff = context->quad_indices[layer].buffer.get();
  const size_t off = sizeof(uint16_t) * context->quad_indices[layer].num_reserved * info.frame_index;
  vkCmdBindIndexBuffer(info.cmd, ind_buff, off, VK_INDEX_TYPE_UINT16);

  vk::DrawIndexedDescriptor draw_desc{};
  draw_desc.num_instances = 1;
  draw_desc.num_indices = context->quad_indices[layer].num_active;
  vk::cmd::draw_indexed(info.cmd, &draw_desc);
}

void render(GPUContext* context, const RenderInfo& info) {
  for (int i = 0; i < Config::max_num_gui_layers; i++) {
    if (context->quad_pipeline.is_valid()) {
      if (context->quad_buffers_valid[i] && context->quad_vertices[i].num_active > 0) {
        render_quads(context, i, info);
      }
    }
    if (context->glyph_pipeline.is_valid() && context->glyph_desc_set0) {
      if (context->glyph_buffers_valid[i] && context->glyph_vertices[i].num_active > 0) {
        render_glyphs(context, i, info);
      }
    }
  }
}

struct {
  GPUContext context;
} globals;

} //  anon

void gui::render_gui_render(const RenderGUIRenderInfo& info) {
  render(&globals.context, info);
}

void gui::render_gui_begin_frame(const RenderGUIBeginFrameInfo& info) {
  begin_frame(&globals.context, info);
}

void gui::terminate_render_gui() {
  globals.context = {};
}

void gui::render_gui_remake_pipelines() {
  globals.context.need_remake_pipelines = true;
}

gui::RenderGUIStats gui::get_render_gui_stats() {
  gui::RenderGUIStats stats{};
  for (auto& set : globals.context.glyph_vertices) {
    stats.num_glyph_quad_vertices += set.num_active;
  }
  for (auto& set : globals.context.quad_vertices) {
    stats.num_quad_vertices += set.num_active;
  }
  return stats;
}

GROVE_NAMESPACE_END
