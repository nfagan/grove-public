#include "CloudRenderer.hpp"
#include "utility.hpp"
#include "graphics_context.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/common/common.hpp"

#define SAMPLE_SCENE_COLOR_IMAGE (0)

GROVE_NAMESPACE_BEGIN

namespace {

using namespace vk;

using VolumeDrawableHandle = CloudRenderer::VolumeDrawableHandle;
using BillboardDrawableHandle = CloudRenderer::BillboardDrawableHandle;
using BillboardDrawableParams = CloudRenderer::BillboardDrawableParams;
using VolumeDrawable = CloudRenderer::VolumeDrawable;

struct BillboardPushConstantData {
  Vec4f uvw_offset;
  Vec4f scale_depth_test_enable;
  Vec4f translation_opacity_scale;
  Vec4f camera_right_front;
  Mat4f projection_view;
};

struct VolumePostProcessPushConstantData {
  Mat4f projection;
  Mat4f view;
};

struct GlobalUniformData {
  Mat4f inv_view_proj;
  Vec4f camera_position4;
  Vec4f cloud_color;
};

struct VolumeInstanceUniformData {
  Vec4f uvw_offset_density_scale;
  Vec4f uvw_scale_depth_test_enable;
  Vec4f volume_aabb_min;
  Vec4f volume_aabb_max;
};

BillboardPushConstantData make_billboard_push_constant_data(const Camera& camera,
                                                            const BillboardDrawableParams& params) {
  auto proj = camera.get_projection();
  proj[1] = -proj[1];

  auto cam_right = camera.get_right();
  auto cam_front = camera.get_front();
  cam_right.y = 0.0f;
  cam_front.y = 0.0f;
  cam_right = normalize(cam_right);
  cam_front = normalize(cam_front);

  BillboardPushConstantData result{};
  result.uvw_offset = Vec4f{params.uvw_offset, 0.0f};
  result.scale_depth_test_enable = Vec4f{
    params.scale, float(params.depth_test_enabled)};
  result.translation_opacity_scale = Vec4f{params.translation, params.opacity_scale};
  result.camera_right_front = Vec4f{
    cam_right.x, cam_right.z, cam_front.x, cam_front.z
  };
  result.projection_view = proj * camera.get_view();
  return result;
}

GlobalUniformData make_global_uniform_data(const Camera& camera, const Vec3f& cloud_color) {
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  GlobalUniformData result;
  result.inv_view_proj = inverse(proj * camera.get_view());
  result.camera_position4 = Vec4f{camera.get_position(), 0.0f};
  result.cloud_color = Vec4f{cloud_color, 0.0f};
  return result;
}

VolumeInstanceUniformData make_volume_instance_uniform_data(const VolumeDrawable& drawable) {
  auto& params = drawable.params;
  VolumeInstanceUniformData result;
  result.uvw_offset_density_scale = Vec4f{params.uvw_offset, params.density_scale};
  result.uvw_scale_depth_test_enable = Vec4f{params.uvw_scale, float(params.depth_test_enable)};
  result.volume_aabb_min = Vec4f{params.translation - params.scale, 0.0f};
  result.volume_aabb_max = Vec4f{params.translation + params.scale, 0.0f};
  return result;
}

VolumePostProcessPushConstantData make_volume_post_process_push_constant_data(const Camera& camera) {
  VolumePostProcessPushConstantData result;
  auto proj = camera.get_projection();
  proj[1] = -proj[1];
  result.projection = proj;
  result.view = camera.get_view();
  return result;
}

std::array<VertexBufferDescriptor, 1> vertex_buffer_descriptors() {
  std::array<VertexBufferDescriptor, 1> result;
  result[0].add_attribute(AttributeDescriptor::float3(0));
  return result;
}

Optional<glsl::VertFragProgramSource> create_post_process_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "cloud/pass-through.vert";
  params.frag_file = "cloud/debug-clouds-post-process.frag";
#if !SAMPLE_SCENE_COLOR_IMAGE
  params.compile.frag_defines.push_back({"NO_COLOR_IMAGE", "", false});
#endif
  params.reflect.to_vk_descriptor_type = vk::refl::always_dynamic_uniform_buffer_descriptor_type;
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_billboard_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "cloud/billboard.vert";
  params.frag_file = "cloud/billboard.frag";
  params.reflect.to_vk_descriptor_type = vk::refl::always_dynamic_uniform_buffer_descriptor_type;
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_forward_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "cloud/pass-through.vert";
  params.frag_file = "cloud/debug-clouds-forward.frag";
  params.reflect.to_vk_descriptor_type = vk::refl::always_dynamic_uniform_buffer_descriptor_type;
  return glsl::make_vert_frag_program_source(params);
}

Result<Pipeline> create_forward_pipeline(VkDevice device,
                                         const glsl::VertFragProgramSource& source,
                                         const PipelineRenderPassInfo& pass_info,
                                         VkPipelineLayout layout) {
  auto buff_descs = vertex_buffer_descriptors();
  SimpleVertFragGraphicsPipelineCreateInfo create_info{};
  create_info.pipeline_layout = layout;
  create_info.pipeline_render_pass_info = &pass_info;
  create_info.configure_pipeline_state = [](auto&) {};
  create_info.configure_params = [](auto& params) {
    params.num_color_attachments = 1;
    params.blend_enabled[0] = true;
    params.cull_mode = VK_CULL_MODE_FRONT_BIT;
  };
  create_info.vertex_buffer_descriptors = buff_descs.data();
  create_info.num_vertex_buffer_descriptors = uint32_t(buff_descs.size());
  create_info.vert_bytecode = &source.vert_bytecode;
  create_info.frag_bytecode = &source.frag_bytecode;
  return create_vert_frag_graphics_pipeline(device, &create_info);
}

Result<Pipeline> create_post_process_pipeline(VkDevice device,
                                              const glsl::VertFragProgramSource& source,
                                              const PipelineRenderPassInfo& pass_info,
                                              VkPipelineLayout layout) {
  auto buff_descs = vertex_buffer_descriptors();
  SimpleVertFragGraphicsPipelineCreateInfo create_info{};
  create_info.pipeline_layout = layout;
  create_info.pipeline_render_pass_info = &pass_info;
  create_info.configure_pipeline_state = [](GraphicsPipelineStateCreateInfo& state) {
    state.depth_stencil.depthTestEnable = VK_FALSE;
  };
  create_info.configure_params = [](DefaultConfigureGraphicsPipelineStateParams& params) {
    params.num_color_attachments = 1;
#if SAMPLE_SCENE_COLOR_IMAGE
    params.blend_enabled[0] = false;
#else
    params.blend_enabled[0] = true;
#endif
    params.cull_mode = VK_CULL_MODE_FRONT_BIT;
  };
  create_info.vertex_buffer_descriptors = buff_descs.data();
  create_info.num_vertex_buffer_descriptors = uint32_t(buff_descs.size());
  create_info.vert_bytecode = &source.vert_bytecode;
  create_info.frag_bytecode = &source.frag_bytecode;
  return create_vert_frag_graphics_pipeline(device, &create_info);
}

Result<Pipeline> create_billboard_pipeline(VkDevice device,
                                           const glsl::VertFragProgramSource& source,
                                           const PipelineRenderPassInfo& pass_info,
                                           VkPipelineLayout layout) {
  auto buff_descs = vertex_buffer_descriptors();
  SimpleVertFragGraphicsPipelineCreateInfo create_info{};
  create_info.pipeline_layout = layout;
  create_info.pipeline_render_pass_info = &pass_info;
  create_info.configure_pipeline_state = [](GraphicsPipelineStateCreateInfo& state) {
    state.depth_stencil.depthTestEnable = VK_FALSE;
  };
  create_info.configure_params = [](DefaultConfigureGraphicsPipelineStateParams& params) {
    params.num_color_attachments = 1;
    params.blend_enabled[0] = true;
    params.cull_mode = VK_CULL_MODE_NONE;
  };
  create_info.vertex_buffer_descriptors = buff_descs.data();
  create_info.num_vertex_buffer_descriptors = uint32_t(buff_descs.size());
  create_info.vert_bytecode = &source.vert_bytecode;
  create_info.frag_bytecode = &source.frag_bytecode;
  return create_vert_frag_graphics_pipeline(device, &create_info);
}

bool require_layouts(CloudRenderer::PipelineData* pd, const glsl::VertFragProgramSource& source,
                     const CloudRenderer::InitInfo& info) {
  return info.pipeline_system.require_layouts(
    info.core.device.handle,
    make_view(source.push_constant_ranges),
    make_view(source.descriptor_set_layout_bindings),
    &pd->layout,
    &pd->desc_set_layouts);
}

} //  anon

bool CloudRenderer::is_valid() const {
  return initialized && initialized_forward_program;
}

bool CloudRenderer::initialize(const InitInfo& info) {
  glsl::VertFragProgramSource forward_prog_source;
  if (!initialize_forward_program(info, &forward_prog_source)) {
    return false;
  } else {
    initialized_forward_program = true;
  }

  Optional<glsl::VertFragProgramSource> post_process_prog_source;
  Optional<glsl::VertFragProgramSource> billboard_prog_source;

  {
    glsl::VertFragProgramSource volume_source;
    if (!initialize_post_process_program(info, &volume_source)) {
      return false;
    } else {
      initialized_post_process_program = true;
      post_process_prog_source = std::move(volume_source);
    }

    glsl::VertFragProgramSource billboard_source;
    if (!initialize_billboard_program(info, &billboard_source)) {
      return false;
    } else {
      initialized_billboard_program = true;
      billboard_prog_source = std::move(billboard_source);
    }
  }

  {
    auto get_size = [](ShaderResourceType) { return 4; };
    DescriptorPoolAllocator::PoolSizes pool_sizes;
    push_pool_sizes_from_layout_bindings(
      pool_sizes,
      make_view(forward_prog_source.descriptor_set_layout_bindings),
      get_size);
    if (post_process_prog_source) {
      push_pool_sizes_from_layout_bindings(
        pool_sizes,
        make_view(post_process_prog_source.value().descriptor_set_layout_bindings),
        get_size);
    }
    if (billboard_prog_source) {
      push_pool_sizes_from_layout_bindings(
        pool_sizes,
        make_view(billboard_prog_source.value().descriptor_set_layout_bindings),
        get_size);
    }
    desc_pool_alloc = info.desc_system.create_pool_allocator(make_view(pool_sizes), 4);
    forward_desc_set0_alloc = info.desc_system.create_set_allocator(desc_pool_alloc.get());
    forward_desc_set1_alloc = info.desc_system.create_set_allocator(desc_pool_alloc.get());
    if (post_process_prog_source) {
      post_process_desc_set0_alloc = info.desc_system.create_set_allocator(desc_pool_alloc.get());
      post_process_desc_set1_alloc = info.desc_system.create_set_allocator(desc_pool_alloc.get());
    }
    if (billboard_prog_source) {
      billboard_desc_set0_alloc = info.desc_system.create_set_allocator(desc_pool_alloc.get());
    }
  }

  {
    size_t buff_size{};
    auto un_buff_res = create_dynamic_uniform_buffer<GlobalUniformData>(
      info.allocator,
      &info.core.physical_device.info.properties,
      info.frame_queue_depth,
      &global_uniform_buffer_stride,
      &buff_size);
    if (!un_buff_res) {
      return false;
    } else {
      global_uniform_buffer = info.buffer_system.emplace(std::move(un_buff_res.value));
    }
  }

  {
    auto geom = geometry::quad_positions(/*is_3d=*/true, 1.0f);
    auto geom_inds = geometry::quad_indices();

    size_t geom_size = geom.size() * sizeof(float);
    size_t ind_size = geom_inds.size() * sizeof(uint16_t);
    auto geom_buff = create_device_local_vertex_buffer(info.allocator, geom_size, true);
    auto geom_ind_buff = create_device_local_index_buffer(info.allocator, ind_size, true);
    if (!geom_buff || !geom_ind_buff) {
      return false;
    }

    const ManagedBuffer* dst_buffs[2] = {&geom_buff.value, &geom_ind_buff.value};
    const void* src_data[2] = {geom.data(), geom_inds.data()};
    auto upload_context = make_upload_from_staging_buffer_context(
      &info.core,
      info.allocator,
      &info.staging_buffer_system,
      &info.uploader);
    if (!upload_from_staging_buffer_sync(src_data, dst_buffs, nullptr, 2, upload_context)) {
      return false;
    } else {
      vertex_geometry = info.buffer_system.emplace(std::move(geom_buff.value));
      vertex_indices = info.buffer_system.emplace(std::move(geom_ind_buff.value));
      aabb_draw_desc.num_indices = uint32_t(geom_inds.size());
    }
  }

  initialized = true;
  return true;
}

bool CloudRenderer::remake_programs(const InitInfo& info) {
  initialized_forward_program = false;
  initialized_post_process_program = false;
  initialized_billboard_program = false;
  if (!initialize_forward_program(info, nullptr)) {
    return false;
  }
  if (!initialize_post_process_program(info, nullptr)) {
    return false;
  }
  if (!initialize_billboard_program(info, nullptr)) {
    return false;
  }
  initialized_forward_program = true;
  initialized_post_process_program = true;
  initialized_billboard_program = true;
  return true;
}

bool CloudRenderer::initialize_post_process_program(const InitInfo& info,
                                                    glsl::VertFragProgramSource* out_source) {
  auto prog_source = create_post_process_program_source();
  if (!prog_source) {
    return false;
  }
  if (!require_layouts(&post_process_pipeline_data, prog_source.value(), info)) {
    return false;
  }
  auto pipeline = create_post_process_pipeline(
    info.core.device.handle,
    prog_source.value(),
    info.post_process_pass_info,
    post_process_pipeline_data.layout);
  if (!pipeline) {
    return false;
  } else {
    post_process_pipeline_data.pipeline = info.pipeline_system.emplace(std::move(pipeline.value));
  }
  if (out_source) {
    *out_source = std::move(prog_source.value());
  }
  return true;
}

bool CloudRenderer::initialize_forward_program(const InitInfo& info,
                                               glsl::VertFragProgramSource* out_source) {
  auto prog_source = create_forward_program_source();
  if (!prog_source) {
    return false;
  }
  if (!require_layouts(&forward_pipeline_data, prog_source.value(), info)) {
    return false;
  }
  auto pipeline = create_forward_pipeline(
    info.core.device.handle,
    prog_source.value(),
    info.forward_pass_info,
    forward_pipeline_data.layout);
  if (!pipeline) {
    return false;
  } else {
    forward_pipeline_data.pipeline = info.pipeline_system.emplace(std::move(pipeline.value));
  }
  if (out_source) {
    *out_source = std::move(prog_source.value());
  }
  return true;
}

bool CloudRenderer::initialize_billboard_program(const InitInfo& info,
                                                 glsl::VertFragProgramSource* source) {
  auto prog_source = create_billboard_program_source();
  if (!prog_source) {
    return false;
  }
  if (!require_layouts(&billboard_pipeline_data, prog_source.value(), info)) {
    return false;
  }
  auto pipeline = create_billboard_pipeline(
    info.core.device.handle,
    prog_source.value(),
    info.post_process_pass_info,
    billboard_pipeline_data.layout);
  if (!pipeline) {
    return false;
  } else {
    billboard_pipeline_data.pipeline = info.pipeline_system.emplace(std::move(pipeline.value));
  }
  if (source) {
    *source = std::move(prog_source.value());
  }
  return true;
}

void CloudRenderer::begin_frame(const BeginFrameInfo& info) {
  if (enabled) {
    update_buffers(info.camera, info.frame_index);
  }
}

int CloudRenderer::num_active_volume_drawables() const {
  int ct{};
  for (auto& [_, drawable] : volume_drawables) {
    if (!drawable.inactive) {
      ct++;
    }
  }
  return ct;
}

int CloudRenderer::num_active_billboard_drawables() const {
  int ct{};
  for (auto& [_, drawable] : billboard_drawables) {
    if (!drawable.inactive) {
      ct++;
    }
  }
  return ct;
}

void CloudRenderer::update_buffers(const Camera& camera, uint32_t frame_index) {
  {
    size_t off = global_uniform_buffer_stride * frame_index;
    auto un_data = make_global_uniform_data(camera, render_params.cloud_color);
    global_uniform_buffer.get().write(&un_data, sizeof(GlobalUniformData), off);
  }
  for (auto& [_, drawable] : volume_drawables) {
    if (!drawable.inactive) {
      size_t off = drawable.uniform_buffer_stride * frame_index;
      auto un_data = make_volume_instance_uniform_data(drawable);
      drawable.uniform_buffer.get().write(&un_data, sizeof(VolumeInstanceUniformData), off);
    }
  }
}

void CloudRenderer::render_post_process(const RenderInfo& info) {
  if (!enabled) {
    return;
  }
  if (info.post_processing_enabled && info.scene_depth_image && info.scene_color_image) {
    if (initialized_post_process_program && num_active_volume_drawables() > 0 &&
        !volume_disabled) {
      render_volume_post_process(info);
    }
    if (initialized_billboard_program && num_active_billboard_drawables() > 0) {
      render_billboard_post_process(info);
    }
  }
}

void CloudRenderer::render_billboard_post_process(const RenderInfo& info) {
  vk::DescriptorPoolAllocator* pool_alloc;
  vk::DescriptorSetAllocator* set0_alloc;
  if (!info.descriptor_system.get(desc_pool_alloc.get(), &pool_alloc) ||
      !info.descriptor_system.get(billboard_desc_set0_alloc.get(), &set0_alloc)) {
    return;
  }

  auto& pd = billboard_pipeline_data;
  cmd::bind_graphics_pipeline(info.cmd, pd.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);

  for (auto& [_, drawable] : billboard_drawables) {
    if (drawable.inactive) {
      continue;
    }
    auto opacity_image = info.dynamic_sampled_image_manager.get(drawable.image_handle);
    if (!opacity_image ||
#if 1
        !opacity_image.value().is_3d() ||
#else
        !opacity_image.value().is_2d() ||
#endif
        !opacity_image.value().fragment_shader_sample_ok()) {
      continue;
    }

    VkDescriptorSet desc_set0;
    {
      DescriptorSetScaffold scaffold;
      scaffold.set = 0;
      uint32_t bind{};
      auto linear_edge_clamp = info.sampler_system.require_linear_edge_clamp(info.device);
      auto linear_repeat = info.sampler_system.require_linear_repeat(info.device);

      push_combined_image_sampler(  //  scene depth image
        scaffold, bind++, info.scene_depth_image.value(), linear_edge_clamp);
      push_combined_image_sampler(  //  opacity image
        scaffold, bind++, opacity_image.value().to_sample_image_view(), linear_repeat);

      if (auto err = set0_alloc->require_updated_descriptor_set(
        info.device, *pd.desc_set_layouts.find(0), *pool_alloc, scaffold, &desc_set0)) {
        return;
      }
    }

    auto pc_data = make_billboard_push_constant_data(info.camera, drawable.params);
    auto pc_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    cmd::bind_graphics_descriptor_sets(info.cmd, pd.layout, 0, 1, &desc_set0);
    cmd::push_constants(info.cmd, pd.layout, pc_stages, &pc_data);

    const VkBuffer vert_buffs[1] = {
      vertex_geometry.get().contents().buffer.handle
    };
    const VkDeviceSize vb_offs[1] = {};
    vkCmdBindVertexBuffers(info.cmd, 0, 1, vert_buffs, vb_offs);

    VkBuffer index_buff = vertex_indices.get().contents().buffer.handle;
    vkCmdBindIndexBuffer(info.cmd, index_buff, 0, VK_INDEX_TYPE_UINT16);

    auto draw_desc = aabb_draw_desc;
    draw_desc.num_instances = 1;
    cmd::draw_indexed(info.cmd, &draw_desc);
  }
}

void CloudRenderer::render_volume_post_process(const RenderInfo& info) {
  vk::DescriptorPoolAllocator* pool_alloc;
  vk::DescriptorSetAllocator* set0_alloc;
  vk::DescriptorSetAllocator* set1_alloc;
  if (!info.descriptor_system.get(desc_pool_alloc.get(), &pool_alloc) ||
      !info.descriptor_system.get(post_process_desc_set0_alloc.get(), &set0_alloc) ||
      !info.descriptor_system.get(post_process_desc_set1_alloc.get(), &set1_alloc)) {
    return;
  }

  cmd::bind_graphics_pipeline(info.cmd, post_process_pipeline_data.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);

  VkDescriptorSet desc_set0;
  {
    DescriptorSetScaffold scaffold;
    scaffold.set = 0;
    uint32_t bind{};
#if SAMPLE_SCENE_COLOR_IMAGE
    auto color_sampler = info.sampler_system.require_simple(
      info.device,
      VK_FILTER_NEAREST,
      VK_FILTER_NEAREST,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
#endif
    auto depth_sampler = info.sampler_system.require_linear_edge_clamp(info.device);
    push_dynamic_uniform_buffer(  //  global uniform data
      scaffold, bind++, global_uniform_buffer.get(), sizeof(GlobalUniformData));
    push_combined_image_sampler(  //  scene depth image
      scaffold, bind++, info.scene_depth_image.value(), depth_sampler);
#if SAMPLE_SCENE_COLOR_IMAGE
    push_combined_image_sampler(  //  scene color image
      scaffold, bind++, info.scene_color_image.value(), color_sampler);
#endif
    if (auto err = set0_alloc->require_updated_descriptor_set(
      info.device,
      *post_process_pipeline_data.desc_set_layouts.find(0),
      *pool_alloc,
      scaffold,
      &desc_set0)) {
      return;
    }
  }

  const uint32_t set0_dyn_offs[] = {
    uint32_t(global_uniform_buffer_stride * info.frame_index)
  };
  const uint32_t num_set0_dyn_offs = 1;
  cmd::bind_graphics_descriptor_sets(
    info.cmd, post_process_pipeline_data.layout, 0, 1, &desc_set0, num_set0_dyn_offs, set0_dyn_offs);

  //  Push projection-view first
  auto pc_data = make_volume_post_process_push_constant_data(info.camera);
  vkCmdPushConstants(
    info.cmd,
    post_process_pipeline_data.layout,
    VK_SHADER_STAGE_FRAGMENT_BIT,
    0,
    sizeof(VolumePostProcessPushConstantData),
    &pc_data);

  for (auto& [_, drawable] : volume_drawables) {
    if (drawable.inactive) {
      continue;
    }
    VkDescriptorSet desc_set1;

    Optional<DynamicSampledImageManager::ReadInstance> volume_im;
    if (auto inst = info.dynamic_sampled_image_manager.get(drawable.image_handle)) {
      if (inst.value().fragment_shader_sample_ok() && inst.value().is_3d()) {
        volume_im = inst.value();
      }
    }
    if (!volume_im) {
      continue;
    }

    {
      auto cloud_sampler = info.sampler_system.require_linear_repeat(info.device);
      DescriptorSetScaffold scaffold;
      scaffold.set = 1;
      uint32_t bind{};
      push_dynamic_uniform_buffer(  //  instance uniform data
        scaffold, bind++, drawable.uniform_buffer.get(), sizeof(VolumeInstanceUniformData));
      push_combined_image_sampler(  //  cloud image
        scaffold, bind++, volume_im.value().view, cloud_sampler, volume_im.value().layout);
      if (auto err = set1_alloc->require_updated_descriptor_set(
        info.device,
        *post_process_pipeline_data.desc_set_layouts.find(1),
        *pool_alloc,
        scaffold,
        &desc_set1)) {
        continue;
      }
    }

    const uint32_t set1_dyn_offs[] = {
      uint32_t(drawable.uniform_buffer_stride * info.frame_index)
    };
    const uint32_t num_set1_dyn_offs = 1;
    cmd::bind_graphics_descriptor_sets(
      info.cmd,
      post_process_pipeline_data.layout,
      1, 1, &desc_set1, num_set1_dyn_offs, set1_dyn_offs);

    const VkBuffer vert_buffs[1] = {
      vertex_geometry.get().contents().buffer.handle
    };
    const VkDeviceSize vb_offs[1] = {};
    vkCmdBindVertexBuffers(info.cmd, 0, 1, vert_buffs, vb_offs);

    VkBuffer index_buff = vertex_indices.get().contents().buffer.handle;
    vkCmdBindIndexBuffer(info.cmd, index_buff, 0, VK_INDEX_TYPE_UINT16);

    auto draw_desc = aabb_draw_desc;
    draw_desc.num_instances = 1;
    cmd::draw_indexed(info.cmd, &draw_desc);
  }
}

void CloudRenderer::render_forward(const RenderInfo& info) {
  if (num_active_volume_drawables() == 0 || !enabled || info.post_processing_enabled) {
    return;
  }

  vk::DescriptorPoolAllocator* pool_alloc;
  vk::DescriptorSetAllocator* set0_alloc;
  vk::DescriptorSetAllocator* set1_alloc;
  if (!info.descriptor_system.get(desc_pool_alloc.get(), &pool_alloc) ||
      !info.descriptor_system.get(forward_desc_set0_alloc.get(), &set0_alloc) ||
      !info.descriptor_system.get(forward_desc_set1_alloc.get(), &set1_alloc)) {
    return;
  }

  const auto& pd = forward_pipeline_data;
  cmd::bind_graphics_pipeline(info.cmd, pd.pipeline.get().handle);
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor_rect);

  VkDescriptorSet desc_set0;
  {
    DescriptorSetScaffold scaffold;
    scaffold.set = 0;
    uint32_t bind{};
    push_dynamic_uniform_buffer(  //  global uniform data
      scaffold, bind++, global_uniform_buffer.get(), sizeof(GlobalUniformData));
    if (auto err = set0_alloc->require_updated_descriptor_set(
      info.device, *pd.desc_set_layouts.find(0), *pool_alloc, scaffold, &desc_set0)) {
      return;
    }
  }

  const uint32_t set0_dyn_offs[] = {
    uint32_t(global_uniform_buffer_stride * info.frame_index)
  };
  const uint32_t num_set0_dyn_offs = 1;
  cmd::bind_graphics_descriptor_sets(
    info.cmd, pd.layout, 0, 1, &desc_set0, num_set0_dyn_offs, set0_dyn_offs);

  for (auto& [_, drawable] : volume_drawables) {
    if (drawable.inactive) {
      continue;
    }
    VkDescriptorSet desc_set1;

    Optional<DynamicSampledImageManager::ReadInstance> volume_im;
    if (auto inst = info.dynamic_sampled_image_manager.get(drawable.image_handle)) {
      if (inst.value().fragment_shader_sample_ok() && inst.value().is_3d()) {
        volume_im = inst.value();
      }
    }
    if (!volume_im) {
      continue;
    }

    {
      auto cloud_sampler = info.sampler_system.require_linear_repeat(info.device);
      DescriptorSetScaffold scaffold;
      scaffold.set = 1;
      uint32_t bind{};
      push_dynamic_uniform_buffer(  //  instance uniform data
        scaffold, bind++, drawable.uniform_buffer.get(), sizeof(VolumeInstanceUniformData));
      push_combined_image_sampler(  //  cloud image
        scaffold, bind++, volume_im.value().view, cloud_sampler, volume_im.value().layout);
      if (auto err = set1_alloc->require_updated_descriptor_set(
        info.device, *pd.desc_set_layouts.find(1), *pool_alloc, scaffold, &desc_set1)) {
        continue;
      }
    }

    const uint32_t set1_dyn_offs[] = {
      uint32_t(drawable.uniform_buffer_stride * info.frame_index)
    };
    const uint32_t num_set1_dyn_offs = 1;
    cmd::bind_graphics_descriptor_sets(
      info.cmd, pd.layout, 1, 1, &desc_set1, num_set1_dyn_offs, set1_dyn_offs);

    const VkBuffer vert_buffs[1] = {
      vertex_geometry.get().contents().buffer.handle
    };
    const VkDeviceSize vb_offs[1] = {};
    vkCmdBindVertexBuffers(info.cmd, 0, 1, vert_buffs, vb_offs);

    VkBuffer index_buff = vertex_indices.get().contents().buffer.handle;
    vkCmdBindIndexBuffer(info.cmd, index_buff, 0, VK_INDEX_TYPE_UINT16);

    auto draw_desc = aabb_draw_desc;
    draw_desc.num_instances = 1;
    cmd::draw_indexed(info.cmd, &draw_desc);
  }
}

Optional<BillboardDrawableHandle>
CloudRenderer::create_billboard_drawable(const AddResourceContext&,
                                         vk::DynamicSampledImageManager::Handle image,
                                         const BillboardDrawableParams& params) {
  BillboardDrawable drawable{};
  drawable.image_handle = image;
  drawable.params = params;
  BillboardDrawableHandle handle{next_drawable_id++};
  billboard_drawables[handle.id] = std::move(drawable);
  return Optional<BillboardDrawableHandle>(handle);
}

Optional<VolumeDrawableHandle>
CloudRenderer::create_volume_drawable(const AddResourceContext& context,
                                      vk::DynamicSampledImageManager::Handle image_handle,
                                      const VolumeDrawableParams& params) {
  VolumeDrawable drawable{};
  drawable.image_handle = image_handle;
  drawable.params = params;
  {
    size_t buff_size;
    auto un_buff_res = create_dynamic_uniform_buffer<VolumeInstanceUniformData>(
      context.allocator,
      &context.core->physical_device.info.properties,
      context.frame_queue_depth,
      &drawable.uniform_buffer_stride,
      &buff_size);
    if (!un_buff_res) {
      return NullOpt{};
    } else {
      drawable.uniform_buffer = context.buffer_system->emplace(std::move(un_buff_res.value));
    }
  }
  VolumeDrawableHandle handle{next_drawable_id++};
  volume_drawables[handle.id] = std::move(drawable);
  return Optional<VolumeDrawableHandle>(handle);
}

void CloudRenderer::set_drawable_params(VolumeDrawableHandle handle,
                                        const VolumeDrawableParams& params) {
  if (auto it = volume_drawables.find(handle.id); it != volume_drawables.end()) {
    it->second.params = params;
  } else {
    GROVE_ASSERT(false);
  }
}

void CloudRenderer::set_active(VolumeDrawableHandle handle, bool active) {
  if (auto it = volume_drawables.find(handle.id); it != volume_drawables.end()) {
    it->second.inactive = !active;
  }
}

void CloudRenderer::set_drawable_params(BillboardDrawableHandle handle,
                                        const BillboardDrawableParams& params) {
  if (auto it = billboard_drawables.find(handle.id); it != billboard_drawables.end()) {
    it->second.params = params;
  } else {
    GROVE_ASSERT(false);
  }
}

void CloudRenderer::set_active(BillboardDrawableHandle handle, bool active) {
  if (auto it = billboard_drawables.find(handle.id); it != billboard_drawables.end()) {
    it->second.inactive = !active;
  }
}

CloudRenderer::AddResourceContext
CloudRenderer::make_add_resource_context(vk::GraphicsContext& graphics_context) {
  return CloudRenderer::AddResourceContext {
    &graphics_context.core,
    &graphics_context.allocator,
    &graphics_context.command_processor,
    &graphics_context.buffer_system,
    &graphics_context.staging_buffer_system,
    graphics_context.frame_queue_depth
  };
}

GROVE_NAMESPACE_END
