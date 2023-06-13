#include "DebugImageRenderer.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/visual/geometry.hpp"
#include "utility.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace vk;

[[maybe_unused]] constexpr const char* logging_id() {
  return "DebugImageRenderer";
}

using Drawable = DebugImageRenderer::Drawable;

struct PushConstantData {
  Vec4f translation_scale;  //  vec2 trans, vec2 scale
  Vec4f viewport_dims_image_dims;  //  vec2 viewport, vec2 image;
  Vec4f min_alpha;  //  float, unused ...
};

PushConstantData make_push_constant_data(const Drawable& drawable,
                                         VkViewport viewport,
                                         const image::Descriptor& image_desc) {
  PushConstantData result;
  result.translation_scale = Vec4f{
    drawable.params.translation.x, drawable.params.translation.y,
    drawable.params.scale.x, drawable.params.scale.y
  };
  result.viewport_dims_image_dims = Vec4f{
    float(viewport.width), float(viewport.height),
    float(image_desc.shape.width), float(image_desc.shape.height)
  };
  result.min_alpha = Vec4f{drawable.params.min_alpha, 0.0f, 0.0f, 0.0f};
  return result;
}

Optional<glsl::VertFragProgramSource> create_program_source(int num_image_components) {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "debug/image.vert";
  params.frag_file = "debug/image.frag";
  params.compile.frag_defines.push_back(
    {"NUM_IMAGE_COMPONENTS", std::to_string(num_image_components), true});
  return glsl::make_vert_frag_program_source(params);
}

Result<Pipeline> create_pipeline(VkDevice device,
                                 const glsl::VertFragProgramSource& source,
                                 const PipelineRenderPassInfo& pass_info,
                                 VkPipelineLayout layout) {
  VertexBufferDescriptor buff_desc{};
  buff_desc.add_attribute(AttributeDescriptor::float2(0));  //  position
  SimpleVertFragGraphicsPipelineCreateInfo create_info{};
  create_info.pipeline_layout = layout;
  create_info.pipeline_render_pass_info = &pass_info;
  create_info.configure_pipeline_state = [](GraphicsPipelineStateCreateInfo& state) {
    state.depth_stencil.depthTestEnable = VK_FALSE;
    state.depth_stencil.depthWriteEnable = VK_FALSE;
  };
  create_info.configure_params = [](DefaultConfigureGraphicsPipelineStateParams& params) {
    params.num_color_attachments = 1;
    params.cull_mode = VK_CULL_MODE_NONE;
  };
  create_info.vertex_buffer_descriptors = &buff_desc;
  create_info.num_vertex_buffer_descriptors = 1;
  create_info.vert_bytecode = &source.vert_bytecode;
  create_info.frag_bytecode = &source.frag_bytecode;
  return create_vert_frag_graphics_pipeline(device, &create_info);
}

} //  anon

void DebugImageRenderer::push_drawable(vk::SampledImageManager::Handle image,
                                       const DrawableParams& params) {
  Drawable drawable;
  drawable.params = params;
  drawable.static_image = image;
  pending_drawables.push_back(drawable);
}

void DebugImageRenderer::push_drawable(vk::DynamicSampledImageManager::Handle image,
                                       const DrawableParams& params) {
  Drawable drawable;
  drawable.params = params;
  drawable.dynamic_image = image;
  pending_drawables.push_back(drawable);
}

bool DebugImageRenderer::require_geometry_buffers(const RenderInfo& info) {
  auto geom = geometry::quad_positions(false);
  size_t geom_size = geom.size() * sizeof(float);
  auto inds = geometry::quad_indices();
  size_t inds_size = inds.size() * sizeof(uint16_t);
  auto geom_buff = create_device_local_vertex_buffer(info.allocator, geom_size, true);
  if (!geom_buff) {
    return false;
  }
  auto ind_buff = create_device_local_index_buffer(info.allocator, inds_size, true);
  if (!ind_buff) {
    return false;
  }
  const vk::ManagedBuffer* dst_buffs[2] = {&geom_buff.value, &ind_buff.value};
  const void* src_data[2] = {geom.data(), inds.data()};
  auto upload_ctx = make_upload_from_staging_buffer_context(
    &info.core, info.allocator, &info.staging_buffer_system, &info.command_processor);
  if (!upload_from_staging_buffer_sync(src_data, dst_buffs, nullptr, 2, upload_ctx)) {
    return false;
  } else {
    vertex_geometry_buffer = info.buffer_system.emplace(std::move(geom_buff.value));
    vertex_index_buffer = info.buffer_system.emplace(std::move(ind_buff.value));
    draw_desc.num_instances = 1;
    draw_desc.num_indices = uint32_t(inds.size());
    return true;
  }
}

Optional<int> DebugImageRenderer::require_pipeline(const RenderInfo& info,
                                                   int num_image_components) {
  for (int i = 0; i < int(pipelines.size()); i++) {
    if (pipelines[i].num_image_components == num_image_components) {
      return Optional<int>(i);
    }
  }

  auto source = create_program_source(num_image_components);
  if (!source) {
    return NullOpt{};
  }

  vk::BorrowedDescriptorSetLayouts ignore_layouts;
  vk::BorrowedDescriptorSetLayouts* dst_layouts;
  if (acquired_desc_set_layouts) {
    dst_layouts = &ignore_layouts;
    GROVE_ASSERT(desc_set_allocator.has_value() && desc_pool_allocator.has_value());
  } else {
    GROVE_ASSERT(pipelines.empty());
    dst_layouts = &desc_set_layouts;
  }

  PipelineData pipeline_data{};
  pipeline_data.num_image_components = num_image_components;
  if (!info.pipeline_system.require_layouts(
    info.core.device.handle,
    make_view(source.value().push_constant_ranges),
    make_view(source.value().descriptor_set_layout_bindings),
    &pipeline_data.layout,
    dst_layouts)) {
    return NullOpt{};
  }

  if (!acquired_desc_set_layouts) {
    vk::DescriptorPoolAllocator::PoolSizes pool_sizes;
    auto get_size = [](ShaderResourceType) { return 4; };
    push_pool_sizes_from_layout_bindings(
      pool_sizes, make_view(source.value().descriptor_set_layout_bindings), get_size);
    desc_pool_allocator = info.desc_system.create_pool_allocator(make_view(pool_sizes), 4);
    desc_set_allocator = info.desc_system.create_set_allocator(desc_pool_allocator.get());
    acquired_desc_set_layouts = true;
  }

  auto pipe_res = create_pipeline(
    info.core.device.handle,
    source.value(),
    info.pass_info,
    pipeline_data.layout);
  if (!pipe_res) {
    return NullOpt{};
  } else {
    pipeline_data.pipeline = info.pipeline_system.emplace(std::move(pipe_res.value));
  }

  auto ind = int(pipelines.size());
  pipelines.push_back(std::move(pipeline_data));
  return Optional<int>(ind);
}

void DebugImageRenderer::render(const RenderInfo& info) {
  if (pending_drawables.empty()) {
    return;
  }

  if (!vertex_geometry_buffer.is_valid()) {
    if (!require_geometry_buffers(info)) {
      return;
    }
  }

  if (drawable_pipeline_indices.size() < pending_drawables.size()) {
    drawable_pipeline_indices.resize(pending_drawables.size());
  }
  if (drawable_indices.size() < pending_drawables.size()) {
    drawable_indices.resize(pending_drawables.size());
  }

  int pend_drawable_index{};
  auto pend_it = pending_drawables.begin();
  while (pend_it != pending_drawables.end()) {
    auto& pend = *pend_it;
    int num_components{-1};
    if (pend.static_image.is_valid()) {
      if (auto inst = info.image_manager.get(pend.static_image)) {
        if (inst.value().is_2d() &&
            inst.value().fragment_shader_sample_ok() &&
            inst.value().descriptor.channels.num_channels <= 4) {
          num_components = inst.value().descriptor.channels.num_channels;
        }
      }
    } else if (pend.dynamic_image.is_valid()) {
      if (auto inst = info.dynamic_image_manager.get(pend.dynamic_image)) {
        if (inst.value().is_2d() &&
            inst.value().fragment_shader_sample_ok() &&
            inst.value().descriptor.channels.num_channels <= 4) {
          num_components = inst.value().descriptor.channels.num_channels;
        }
      }
    }
    if (num_components <= 0) {
      GROVE_LOG_ERROR_CAPTURE_META("Invalid or unsupported image.", logging_id());
      pend_it = pending_drawables.erase(pend_it);
    } else {
      //  Check whether we need to create a program for this number of image components.
      auto pipeline_index = require_pipeline(info, num_components);
      if (!pipeline_index) {
        return;
      }
      drawable_pipeline_indices[pend_drawable_index] = pipeline_index.value();
      drawable_indices[pend_drawable_index] = pend_drawable_index;
      pend_drawable_index++;
      ++pend_it;
    }
  }

  std::sort(drawable_indices.begin(), drawable_indices.end(), [&](int a, int b) {
    return drawable_pipeline_indices[a] < drawable_pipeline_indices[b];
  });

  vk::DescriptorPoolAllocator* pool_alloc;
  vk::DescriptorSetAllocator* set0_alloc;
  if (!info.desc_system.get(desc_pool_allocator.get(), &pool_alloc) ||
      !info.desc_system.get(desc_set_allocator.get(), &set0_alloc)) {
    GROVE_ASSERT(false);
    return;
  }

  auto image_sampler = info.sampler_system.require_linear_edge_clamp(info.core.device.handle);

  int last_num_components{-1};
  for (int drawable_ind : drawable_indices) {
    auto& drawable = pending_drawables[drawable_ind];
    auto& pipeline = pipelines[drawable_pipeline_indices[drawable_ind]];
    if (pipeline.num_image_components != last_num_components) {
      last_num_components = pipeline.num_image_components;
      cmd::bind_graphics_pipeline(info.cmd, pipeline.pipeline.get().handle);
      cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);
    }

    VkImageView image_view{};
    VkImageLayout image_layout{};
    image::Descriptor image_desc;
    if (drawable.static_image.is_valid()) {
      auto inst = info.image_manager.get(drawable.static_image);
      GROVE_ASSERT(inst);
      image_view = inst.value().view;
      image_layout = inst.value().layout;
      image_desc = inst.value().descriptor;
    } else if (drawable.dynamic_image.is_valid()) {
      auto inst = info.dynamic_image_manager.get(drawable.dynamic_image);
      GROVE_ASSERT(inst);
      image_view = inst.value().view;
      image_layout = inst.value().layout;
      image_desc = inst.value().descriptor;
    } else {
      GROVE_ASSERT(false);
    }

    {
      VkDescriptorSet desc_set0;
      vk::DescriptorSetScaffold set_scaffold;
      set_scaffold.set = 0;
      uint32_t bind{};
      push_combined_image_sampler(
        set_scaffold, bind++, image_view, image_sampler, image_layout);
      if (auto err = set0_alloc->require_updated_descriptor_set(
        info.core.device.handle,
        *desc_set_layouts.find(0),
        *pool_alloc,
        set_scaffold,
        &desc_set0)) {
        GROVE_ASSERT(false);
        return;
      }

      cmd::bind_graphics_descriptor_sets(info.cmd, pipeline.layout, 0, 1, &desc_set0);
    }

    const auto pc_data = make_push_constant_data(drawable, info.viewport, image_desc);
    vkCmdPushConstants(
      info.cmd,
      pipeline.layout,
      VK_SHADER_STAGE_VERTEX_BIT,
      0,
      sizeof(PushConstantData),
      &pc_data);

    const VkBuffer vert_buffs[1] = {vertex_geometry_buffer.get().contents().buffer.handle};
    const VkDeviceSize vb_offs[1] = {0};
    vkCmdBindVertexBuffers(info.cmd, 0, 1, vert_buffs, vb_offs);
    VkBuffer ind_buff = vertex_index_buffer.get().contents().buffer.handle;
    vkCmdBindIndexBuffer(info.cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);
    cmd::draw_indexed(info.cmd, &draw_desc);
  }

  pending_drawables.clear();
}

GROVE_NAMESPACE_END
