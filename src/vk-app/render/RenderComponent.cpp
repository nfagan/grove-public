#include "RenderComponent.hpp"
#include "frustum_cull_data.hpp"
#include "frustum_cull_gpu.hpp"
#include "frustum_cull_types.hpp"
#include "render_tree_leaves_types.hpp"
#include "render_tree_leaves.hpp"
#include "render_tree_leaves_gpu.hpp"
#include "render_vines.hpp"
#include "render_branch_nodes_gpu.hpp"
#include "render_branch_nodes.hpp"
#include "render_ornamental_foliage_gpu.hpp"
#include "render_ornamental_foliage_data.hpp"
#include "render_gui_gpu.hpp"
#include "render_gui_data.hpp"
#include "font.hpp"
#include "render_particles_gpu.hpp"
#include "gen_depth_pyramid_gpu.hpp"
#include "occlusion_cull_gpu.hpp"
#include "../imgui/GraphicsGUI.hpp"
#include "grove/common/common.hpp"

#define GROVE_RENDER_RAIN_IN_FORWARD_PASS (1)
#define GROVE_RENDER_UI_PLANE_IN_FORWARD_PASS (0)

GROVE_NAMESPACE_BEGIN

void RenderComponent::initialize(const InitInfo& init_info) {
  font::initialize_fonts();

  noise_images.initialize({
    init_info.image_manager,
  });

  static_model_renderer.initialize({
    init_info.core,
    init_info.allocator,
    init_info.forward_pass_info,
    init_info.shadow_pass_info,
    init_info.frame_queue_depth,
    init_info.pipeline_system,
    init_info.desc_system
  });

  grass_renderer.initialize({
    *init_info.graphics_context,
    init_info.core,
    init_info.allocator,
    init_info.frame_queue_depth,
    init_info.forward_pass_info,
    init_info.pipeline_system,
    init_info.desc_system,
    init_info.buffer_system,
  });

  terrain_renderer.initialize({
    init_info.core,
    init_info.allocator,
    init_info.pipeline_system,
    init_info.desc_system,
    init_info.buffer_system,
    init_info.uploader,
    init_info.forward_pass_info,
    init_info.shadow_pass_info,
    init_info.frame_queue_depth
  });

  sky_renderer.initialize({
    init_info.allocator,
    init_info.core,
    init_info.buffer_system,
    init_info.pipeline_system,
    init_info.desc_system,
    init_info.uploader,
    init_info.frame_queue_depth,
    init_info.forward_pass_info,
  });

  procedural_tree_roots_renderer.initialize({
    init_info.allocator,
    init_info.core,
    init_info.buffer_system,
    init_info.staging_buffer_system,
    init_info.pipeline_system,
    init_info.desc_system,
    init_info.uploader,
    init_info.frame_queue_depth,
    init_info.forward_pass_info,
    init_info.shadow_pass_info
  });

  procedural_flower_stem_renderer.initialize({
    init_info.allocator,
    init_info.core,
    init_info.buffer_system,
    init_info.staging_buffer_system,
    init_info.pipeline_system,
    init_info.desc_system,
    init_info.uploader,
    init_info.frame_queue_depth,
    init_info.forward_pass_info
  });

  wind_particle_renderer.initialize({
    init_info.core,
    init_info.allocator,
    init_info.pipeline_system,
    init_info.desc_system,
    init_info.buffer_system,
    init_info.uploader,
    init_info.forward_pass_info,
    init_info.frame_queue_depth
  });

  simple_shape_renderer.initialize({
    init_info.graphics_context,
    init_info.core,
    init_info.allocator,
    init_info.pipeline_system,
    init_info.desc_system,
    init_info.buffer_system,
    init_info.uploader,
    init_info.forward_pass_info,
    init_info.frame_queue_depth
  });

  pollen_particle_renderer.initialize({
    init_info.core,
    init_info.allocator,
    init_info.pipeline_system,
    init_info.desc_system,
    init_info.buffer_system,
    init_info.staging_buffer_system,
    init_info.uploader,
    init_info.forward_pass_info,
    init_info.frame_queue_depth
  });

  point_buffer_renderer.initialize({
    init_info.core,
    init_info.allocator,
    init_info.pipeline_system,
    init_info.buffer_system,
    init_info.forward_pass_info,
    init_info.frame_queue_depth
  });

  cloud_renderer.initialize({
    init_info.allocator,
    init_info.core,
    init_info.buffer_system,
    init_info.staging_buffer_system,
    init_info.pipeline_system,
    init_info.desc_system,
    init_info.uploader,
    init_info.frame_queue_depth,
    init_info.post_process_pass_info,
    init_info.forward_pass_info
  });

  arch_renderer.initialize({
    init_info.core,
    init_info.allocator,
    init_info.pipeline_system,
    init_info.buffer_system,
    init_info.desc_system,
    init_info.forward_pass_info,
    init_info.shadow_pass_info,
    init_info.frame_queue_depth
  });

  {
#if GROVE_RENDER_RAIN_IN_FORWARD_PASS
    const vk::PipelineRenderPassInfo& pass_info = init_info.forward_pass_info;
#else
    const vk::PipelineRenderPassInfo& pass_info = init_info.post_process_pass_info ?
      init_info.post_process_pass_info.value() : init_info.forward_pass_info;
#endif
    rain_particle_renderer.initialize({
      init_info.core,
      init_info.allocator,
      init_info.buffer_system,
      init_info.staging_buffer_system,
      init_info.pipeline_system,
      init_info.desc_system,
      init_info.uploader,
      pass_info,
      init_info.frame_queue_depth
    });
  }

  post_process_blitter.initialize({
    *init_info.graphics_context,
  });

  if (noise_images.bayer8) {
    sky_renderer.set_bayer_image(noise_images.bayer8.value());
  }
}

void RenderComponent::terminate(const vk::Core& core) {
  static_model_renderer.destroy(core.device);
  simple_shape_renderer.terminate();
  post_process_blitter.terminate();
  grass_renderer.terminate();
  terrain_renderer.terminate();
  cull::terminate_frustum_cull_gpu_context();
  cull::terminate_occlusion_cull_against_depth_pyramid();
  gpu::terminate_gen_depth_pyramid();
  foliage::terminate_tree_leaves_renderer();
  foliage::terminate_ornamental_foliage_rendering();
  tree::terminate_vine_renderer();
  tree::terminate_branch_node_renderer();
  particle::terminate_particle_renderer();
  gui::terminate_render_gui();
  font::terminate_fonts();
}

void RenderComponent::begin_update() {
  pollen_particle_renderer.begin_update();
}

void RenderComponent::set_foliage_occlusion_system_modified(bool structure_modified,
                                                            bool clusters_modified) {
  if (structure_modified || clusters_modified) {
    foliage::tree_leaves_renderer_set_cpu_occlusion_data_modified();
  }
}

void RenderComponent::set_tree_leaves_renderer_enabled(bool enable) {
  foliage::set_tree_leaves_renderer_enabled(enable);
}

void RenderComponent::set_wind_displacement_image(vk::DynamicSampledImageManager::Handle handle) {
  grass_renderer.set_wind_displacement_image(handle);
  terrain_renderer.set_wind_displacement_image(handle);
  procedural_flower_stem_renderer.set_wind_displacement_image(handle);
  foliage::set_tree_leaves_renderer_wind_displacement_image(handle.id);
  tree::set_render_vines_wind_displacement_image(handle.id);
  tree::set_render_branch_nodes_wind_displacement_image(handle.id);
  foliage::set_render_ornamental_foliage_wind_displacement_image(handle.id);
}

void RenderComponent::end_frame() {
  foliage::tree_leaves_renderer_end_frame();
  tree::render_branch_nodes_end_frame();
}

void RenderComponent::begin_frame(const BeginFrameInfo& info) {
  const uint32_t frame_index = info.frame_info.current_frame_index;
  grass_renderer.begin_frame({
    *info.graphics_context,
    info.camera,
    info.csm_desc,
    frame_index
  });
  procedural_tree_roots_renderer.begin_frame({
    frame_index
  });
  procedural_flower_stem_renderer.begin_frame({
    info.camera,
    frame_index,
    info.csm_desc
  });
  point_buffer_renderer.begin_frame(frame_index);
  pollen_particle_renderer.begin_frame({
    info.allocator,
    info.buffer_system,
    info.frame_info
  });
  simple_shape_renderer.begin_frame(info.graphics_context, frame_index);
  static_model_renderer.begin_frame({
    info.camera,
    info.csm_desc,
    frame_index
  });
  terrain_renderer.begin_frame({
    *info.graphics_context,
    info.camera,
    info.csm_desc,
    frame_index,
    grass_renderer.get_new_material_data()
  });
  if (cloud_renderer.is_valid()) {
    cloud_renderer.begin_frame({
      info.camera,
      frame_index
    });
  }
  if (rain_particle_renderer.is_valid()) {
    rain_particle_renderer.begin_frame({
      frame_index,
      info.camera
    });
  }
  if (arch_renderer.is_valid()) {
    arch_renderer.begin_frame({
      info.allocator,
      info.core,
      info.frame_info.frame_queue_depth,
      info.buffer_system,
      info.staging_buffer_system,
      info.command_processor,
      info.csm_desc,
      info.camera,
      frame_index
    });
  }

  {
    cull::FrustumCullInputs cull_input_sets[2];
    uint32_t num_cull_input_sets{2};

    //  tree leaves
    cull_input_sets[0].cpu_cull_data = cull::get_global_tree_leaves_frustum_cull_data();
    cull_input_sets[0].arg_frustums[0] = info.camera.make_world_space_frustum(
      cull::get_frustum_cull_far_plane_distance());
    cull_input_sets[0].num_frustums = 1;

    //  branch nodes
    cull_input_sets[1].cpu_cull_data = cull::get_global_branch_nodes_frustum_cull_data();
    cull_input_sets[1].arg_frustums[0] = info.camera.make_world_space_frustum(
      cull::get_frustum_cull_far_plane_distance());
    cull_input_sets[1].num_frustums = 1;

    auto cull_begin_res = cull::frustum_cull_gpu_context_begin_frame({
      cull_input_sets,
      num_cull_input_sets,
      *info.graphics_context,
      info.frame_info.current_frame_index,
      info.frame_info.frame_queue_depth,
      info.core,
      info.allocator,
      info.buffer_system,
    });

    cull::occlusion_cull_against_depth_pyramid_begin_frame(
      cull_begin_res.dependent_instances_potentially_invalidated, num_cull_input_sets);
  }

  if (auto read_res = cull::frustum_cull_gpu_context_read_results(0, 0)) {
    {
      auto* rp = foliage::get_tree_leaves_render_params();
      rp->sun_color = common_render_params.sun_color;
      rp->sun_position = common_render_params.sun_position;
      rp->wind_world_bound_xz = common_render_params.wind_world_bound_xz;
      rp->wind_displacement_limits = common_render_params.wind_displacement_limits;
      rp->wind_strength_limits = common_render_params.branch_wind_strength_limits;
    }

    tree::set_render_vines_wind_info(
      common_render_params.wind_world_bound_xz,
      common_render_params.wind_displacement_limits,
      common_render_params.branch_wind_strength_limits);
    tree::set_render_vines_elapsed_time(common_render_params.elapsed_time);

    Optional<foliage::TreeLeavesRendererGPUOcclusionCullResult> opt_prev_foliage_cull_result;
    if (auto res = cull::get_previous_occlusion_cull_against_depth_pyramid_result(0)) {
      foliage::TreeLeavesRendererGPUOcclusionCullResult prev_cull_result{};
      prev_cull_result.num_elements = res.value().num_elements;
      prev_cull_result.result_buffer = res.value().result_buffer;
      opt_prev_foliage_cull_result = prev_cull_result;
    }

    foliage::tree_leaves_renderer_begin_frame({
      info.graphics_context,
      foliage::get_global_tree_leaves_render_data(),
      nullptr,
      info.frame_info.current_frame_index,
      info.frame_info.frame_queue_depth,
      info.allocator,
      info.core,
      info.buffer_system,
      info.pipeline_system,
      info.descriptor_system,
      info.sampler_system,
      info.command_processor,
      info.sampled_image_manager,
      info.dynamic_sampled_image_manager,
      *read_res.value().results,
      read_res.value().num_results,
      *read_res.value().group_offsets,
      read_res.value().num_group_offsets,
      info.camera,
      info.csm_desc,
      info.forward_pass_info,
      info.shadow_pass_info,
      common_render_params.elapsed_time,
      info.sample_shadow_image,
      opt_prev_foliage_cull_result
    });
  }

  tree::render_vines_begin_frame({
    info.graphics_context,
    &info.dynamic_sampled_image_manager,
    info.forward_pass_info,
    info.render_vine_system,
    frame_index,
    info.frame_info.frame_queue_depth
  });

  {
    auto* rp = tree::get_render_branch_nodes_render_params();
    rp->elapsed_time = common_render_params.elapsed_time;
    rp->wind_world_bound_xz = common_render_params.wind_world_bound_xz;
    rp->wind_displacement_limits = common_render_params.wind_displacement_limits;
    rp->wind_strength_limits = common_render_params.branch_wind_strength_limits;
    rp->sun_position = common_render_params.sun_position;
    rp->sun_color = common_render_params.sun_color;
  }

  tree::render_branch_nodes_begin_frame({
    info.graphics_context,
    tree::get_global_branch_nodes_data(),
    info.frame_info.frame_queue_depth,
    frame_index,
    &info.dynamic_sampled_image_manager,
    info.camera,
    info.csm_desc,
    info.sample_shadow_image,
  });

  {
    const auto* src_rp = tree::get_render_branch_nodes_render_params();
    auto* dst_rp = foliage::get_render_ornamental_foliage_render_params();

    dst_rp->sun_position = src_rp->sun_position;
    dst_rp->sun_color = src_rp->sun_color;
    dst_rp->wind_world_bound_xz = src_rp->wind_world_bound_xz;
    dst_rp->wind_displacement_limits = src_rp->wind_displacement_limits;
    dst_rp->wind_strength_limits = src_rp->wind_strength_limits;
    dst_rp->elapsed_time = src_rp->elapsed_time;
    dst_rp->branch_elapsed_time = src_rp->elapsed_time;

    foliage::render_ornamental_foliage_begin_frame({
      info.graphics_context,
      frame_index,
      info.frame_info.frame_queue_depth,
      foliage::get_global_ornamental_foliage_data(),
      &info.sampled_image_manager,
      &info.dynamic_sampled_image_manager,
      info.csm_desc,
      info.sample_shadow_image,
      info.camera,
    });
  }

  particle::render_particles_begin_frame({
    info.graphics_context,
    frame_index,
    info.sample_scene_depth_image
  });

  gui::render_gui_begin_frame({
    frame_index,
    info.graphics_context,
    gui::get_global_gui_render_data(),
    info.sampled_image_manager
  });
}

void RenderComponent::early_graphics_compute(const EarlyGraphicsComputeInfo& info) {
  cull::frustum_cull_gpu_context_early_graphics_compute({
    info.cmd,
    info.frame_index
  });

  foliage::tree_leaves_renderer_early_graphics_compute({
    info.cmd, info.frame_index
  });

  {
    Optional<tree::RenderBranchNodesCullResults> opt_frust_cull_res;
    Optional<tree::RenderBranchNodesCullResults> opt_occlusion_cull_res;

    if (auto frust_res = cull::frustum_cull_gpu_context_read_results(1, 0)) {
      {
        tree::RenderBranchNodesCullResults cull_res{};
        cull_res.num_results = frust_res.value().num_results;
        cull_res.results_buffer = frust_res.value().results->contents().buffer.handle;
        cull_res.num_group_offsets = frust_res.value().num_group_offsets;
        cull_res.group_offsets_buffer = frust_res.value().group_offsets->contents().buffer.handle;
        opt_frust_cull_res = cull_res;
      }

      if (auto occ_res = cull::get_previous_occlusion_cull_against_depth_pyramid_result(1)) {
        tree::RenderBranchNodesCullResults cull_res{};
        cull_res.num_results = occ_res.value().num_elements;
        cull_res.results_buffer = occ_res.value().result_buffer;
        cull_res.num_group_offsets = frust_res.value().num_group_offsets;
        cull_res.group_offsets_buffer = frust_res.value().group_offsets->contents().buffer.handle;
        opt_occlusion_cull_res = cull_res;
      }
    }

    tree::render_branch_nodes_early_graphics_compute({
      &info.context,
      info.frame_index,
      info.cmd,
      opt_frust_cull_res,
      opt_occlusion_cull_res
    });
  }
}

void RenderComponent::post_forward_compute(const PostForwardComputeInfo& info) {
  auto pyr_res = gpu::gen_depth_pyramid({
    info.context,
    info.vk_context,
    info.sample_scene_depth_image,
    info.scene_depth_image_extent,
    info.cmd,
    info.frame_index
  });

  Optional<cull::OcclusionCullDepthPyramidInfo> opt_pyr_info;
  if (pyr_res.sample_depth_pyramid) {
    cull::OcclusionCullDepthPyramidInfo pyr_info{};
    pyr_info.depth_pyramid_image_extent = pyr_res.depth_pyramid_image_extent;
    pyr_info.depth_pyramid_image_max_mip = pyr_res.depth_pyramid_image_num_mips - 1;
    pyr_info.depth_pyramid_image = pyr_res.sample_depth_pyramid.value();
    opt_pyr_info = pyr_info;
  }

  Optional<cull::OcclusionCullFrustumCullInfo> opt_frust_infos[2];
  uint32_t num_opt_frust_infos{2};
  if (auto frust_cull_res = cull::frustum_cull_gpu_context_read_results(0, 0)) {
    cull::OcclusionCullFrustumCullInfo frust_info{};
    frust_info.cull_results = frust_cull_res.value().results;
    frust_info.instances = frust_cull_res.value().instances;
    frust_info.num_instances = frust_cull_res.value().num_results;
    opt_frust_infos[0] = frust_info;
  }
  if (auto frust_cull_res = cull::frustum_cull_gpu_context_read_results(1, 0)) {
    cull::OcclusionCullFrustumCullInfo frust_info{};
    frust_info.cull_results = frust_cull_res.value().results;
    frust_info.instances = frust_cull_res.value().instances;
    frust_info.num_instances = frust_cull_res.value().num_results;
    opt_frust_infos[1] = frust_info;
  }

  cull::occlusion_cull_against_depth_pyramid({
    info.context,
    opt_pyr_info,
    opt_frust_infos,
    num_opt_frust_infos,
    info.cmd,
    info.frame_index,
    info.camera
  });

  {
    const vk::ManagedBuffer* frustum_cull_group_offsets{};
    uint32_t num_frustum_cull_group_offsets{};
    if (auto res = cull::frustum_cull_gpu_context_read_results(0, 0)) {
      frustum_cull_group_offsets = res.value().group_offsets;
      num_frustum_cull_group_offsets = res.value().num_group_offsets;
    }

    Optional<foliage::TreeLeavesRendererGPUOcclusionCullResult> opt_leaves_cull_result;
    if (auto cull_res = cull::get_previous_occlusion_cull_against_depth_pyramid_result(0)) {
      foliage::TreeLeavesRendererGPUOcclusionCullResult leaves_cull_result{};
      leaves_cull_result.result_buffer = cull_res.value().result_buffer;
      leaves_cull_result.num_elements = cull_res.value().num_elements;
      opt_leaves_cull_result = leaves_cull_result;
    }

    foliage::tree_leaves_renderer_post_forward_graphics_compute({
      &info.context,
      info.cmd,
      info.frame_index,
      opt_leaves_cull_result,
      frustum_cull_group_offsets,
      num_frustum_cull_group_offsets
    });
  }
}

void RenderComponent::render_post_forward(const PostForwardRenderInfo& info) {
  foliage::tree_leaves_renderer_render_post_process({
    info.cmd,
    info.frame_index,
    info.viewport,
    info.scissor_rect,
  });
}

void RenderComponent::render_post_process_pass(const PostProcessPassRenderInfo& info) {
  if (info.scene_color_image) {
    post_process_blitter.render_post_process_pass({
      *info.graphics_context,
      info.core.device.handle,
      info.desc_system,
      info.sampler_system,
      info.cmd,
      info.viewport,
      info.scissor_rect,
      info.scene_color_image.value(),
    });
  }
#if !GROVE_RENDER_RAIN_IN_FORWARD_PASS
  if (info.post_processing_enabled) {
    rain_particle_renderer.render({
      info.core.device.handle,
      info.desc_system,
      info.cmd,
      info.viewport,
      info.scissor_rect,
      info.frame_index
    });
  }
#endif
#if 0
  if (auto buffs = experimental_foliage_renderer.read_geometry_buffers()) {
    foliage::tree_leaves_renderer_render_post_process({
      info.cmd,
      info.frame_index,
      info.viewport,
      info.scissor_rect,
      *buffs.value().lod0_geometry,
      *buffs.value().lod0_indices,
      *buffs.value().lod1_geometry,
      *buffs.value().lod1_indices,
    });
  }
#endif

  particle::render_particles_render_post_process({
    info.frame_index,
    info.cmd,
    info.viewport,
    info.scissor_rect,
    info.graphics_context,
    info.camera
  });

  if (cloud_renderer.is_valid()) {
    cloud_renderer.render_post_process({
      info.core.device.handle,
      info.allocator,
      info.sampler_system,
      info.desc_system,
      info.dynamic_sampled_image_manager,
      info.scene_color_image,
      info.scene_depth_image,
      info.post_processing_enabled,
      info.frame_index,
      info.cmd,
      info.viewport,
      info.scissor_rect,
      info.camera
    });
  }

  if (!info.present_pass_enabled || !prefer_to_render_ui_at_native_resolution) {
    gui::render_gui_render({
      info.cmd,
      info.viewport,
      info.scissor_rect,
      info.frame_index
    });
  }
}

void RenderComponent::render_present_pass(const PresentPassRenderInfo& info) {
  post_process_blitter.render_present_pass({
    *info.graphics_context,
    info.core.device.handle,
    info.descriptor_system,
    info.sampler_system,
    info.cmd,
    info.viewport,
    info.scissor_rect,
    info.scene_color_image
  });
  if (prefer_to_render_ui_at_native_resolution) {
    gui::render_gui_render({
      info.cmd,
      info.viewport,
      info.scissor_rect,
      info.frame_index
    });
  }
}

void RenderComponent::render_shadow(const ShadowRenderInfo& render_info) {
  if (static_model_renderer.is_valid()) {
    static_model_renderer.render_shadow({
      render_info.device,
      render_info.desc_system,
      render_info.cmd_buffer,
      render_info.frame_index,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.cascade_index,
      render_info.view_proj,
    });
  }

  tree::render_branch_nodes_shadow({
    render_info.frame_index,
    render_info.cmd_buffer,
    render_info.viewport,
    render_info.scissor_rect,
    render_info.cascade_index,
    render_info.view_proj
  });

  foliage::tree_leaves_renderer_render_shadow({
    render_info.cmd_buffer,
    render_info.frame_index,
    render_info.cascade_index,
    render_info.viewport,
    render_info.scissor_rect,
    render_info.view_proj
  });

  if (procedural_tree_roots_renderer.is_valid()) {
    procedural_tree_roots_renderer.render_shadow({
      render_info.device.handle,
      render_info.frame_index,
      render_info.cmd_buffer,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.view_proj,
      render_info.cascade_index
    });
  }

  if (arch_renderer.is_valid()) {
    arch_renderer.render_shadow({
      render_info.device,
      render_info.desc_system,
      render_info.cmd_buffer,
      render_info.frame_index,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.cascade_index,
      render_info.view_proj
    });
  }

  if (terrain_renderer.is_valid()) {
    terrain_renderer.render_shadow({
      render_info.frame_index,
      render_info.cmd_buffer,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.view_proj
    });
  }
}

void RenderComponent::render_grass(const RenderInfo& render_info) {
  if (grass_renderer.is_valid()) {
    grass_renderer.render({
      *render_info.graphics_context,
      render_info.core.device.handle,
      render_info.sampler_system,
      render_info.desc_system,
      render_info.sampled_image_manager,
      render_info.dynamic_sampled_image_manager,
      render_info.frame_index,
      render_info.cmd,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.csm_descriptor,
      render_info.shadow_image,
      render_info.camera
    });
  }
}

void RenderComponent::render_forward(const RenderInfo& render_info) {
  if (terrain_renderer.is_valid()) {
    terrain_renderer.render({
      *render_info.graphics_context,
      render_info.core,
      render_info.sampler_system,
      render_info.desc_system,
      render_info.sampled_image_manager,
      render_info.dynamic_sampled_image_manager,
      render_info.frame_index,
      render_info.cmd,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.camera,
      render_info.shadow_image,
      render_info.csm_descriptor
    });
  }

  if (sky_renderer.is_valid()) {
    sky_renderer.render({
      render_info.core,
      render_info.sampled_image_manager,
      render_info.dynamic_sampled_image_manager,
      render_info.desc_system,
      render_info.sampler_system,
      render_info.frame_index,
      render_info.camera,
      render_info.cmd,
      render_info.viewport,
      render_info.scissor_rect
    });
  }

  if (arch_renderer.is_valid()) {
    arch_renderer.render({
      render_info.core,
      render_info.desc_system,
      render_info.sampler_system,
      render_info.frame_index,
      render_info.cmd,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.shadow_image
    });
  }

  if (!render_grass_late) {
    render_grass(render_info);
  }

  if (static_model_renderer.is_valid()) {
    static_model_renderer.render({
      render_info.core,
      render_info.sampler_system,
      render_info.desc_system,
      render_info.sampled_image_manager,
      render_info.cmd,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.frame_index,
      render_info.camera,
      render_info.shadow_image,
      render_info.csm_descriptor
    });
  }

  if (procedural_tree_roots_renderer.is_valid()) {
    procedural_tree_roots_renderer.render({
      render_info.core.device.handle,
      render_info.allocator,
      render_info.buffer_system,
      render_info.sampler_system,
      render_info.desc_system,
      render_info.frame_index,
      render_info.cmd,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.camera,
    });
  }

  tree::render_vines_forward({
    render_info.graphics_context,
    render_info.cmd,
    render_info.scissor_rect,
    render_info.viewport,
    render_info.camera,
    render_info.frame_index
  });

  tree::render_branch_nodes_forward({
    render_info.frame_index,
    render_info.cmd,
    render_info.viewport,
    render_info.scissor_rect,
    render_info.camera
  });

  foliage::render_ornamental_foliage_render_forward({
    render_info.cmd,
    render_info.viewport,
    render_info.scissor_rect,
    render_info.frame_index,
    render_info.camera
  });

  foliage::tree_leaves_renderer_render_forward({
    render_info.cmd,
    render_info.frame_index,
    render_info.viewport,
    render_info.scissor_rect,
  });

  if (procedural_flower_stem_renderer.is_valid()) {
    procedural_flower_stem_renderer.render({
      render_info.core.device.handle,
      render_info.allocator,
      render_info.buffer_system,
      render_info.sampler_system,
      render_info.desc_system,
      render_info.dynamic_sampled_image_manager,
      render_info.frame_index,
      render_info.cmd,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.camera,
      render_info.shadow_image
    });
  }

  if (simple_shape_renderer.is_valid()) {
    simple_shape_renderer.render({
      render_info.core,
      render_info.frame_index,
      render_info.cmd,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.camera
    });
  }

  if (render_grass_late) {
    render_grass(render_info);
  }

  if (pollen_particle_renderer.is_valid()) {
    pollen_particle_renderer.render({
      render_info.core,
      render_info.allocator,
      render_info.buffer_system,
      render_info.desc_system,
      render_info.frame_index,
      render_info.frame_queue_depth,
      render_info.cmd,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.camera
    });
  }

#if GROVE_RENDER_UI_PLANE_IN_FORWARD_PASS
  if (ui_plane_renderer.is_valid()) {
    ui_plane_renderer.render({
      render_info.core,
      render_info.sampler_system,
      render_info.desc_system,
      render_info.dynamic_sampled_image_manager,
      render_info.frame_index,
      render_info.cmd,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.camera
    });
  }
#endif

  if (wind_particle_renderer.is_valid()) {
    wind_particle_renderer.render({
      render_info.core,
      render_info.frame_index,
      render_info.cmd,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.camera
    });
  }

  if (point_buffer_renderer.is_valid()) {
    point_buffer_renderer.render({
      render_info.core,
      render_info.frame_index,
      render_info.cmd,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.camera
    });
  }

#if GROVE_RENDER_RAIN_IN_FORWARD_PASS
  if (rain_particle_renderer.is_valid()) {
#else
  if (!render_info.post_processing_enabled && rain_particle_renderer.is_valid()) {
#endif
    rain_particle_renderer.render({
      render_info.core.device.handle,
      render_info.desc_system,
      render_info.cmd,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.frame_index
    });
  }

  if (cloud_renderer.is_valid()) {
    cloud_renderer.render_forward({
      render_info.core.device.handle,
      render_info.allocator,
      render_info.sampler_system,
      render_info.desc_system,
      render_info.dynamic_sampled_image_manager,
      NullOpt{},  //  no color image
      NullOpt{},  //  no depth image
      render_info.post_processing_enabled,
      render_info.frame_index,
      render_info.cmd,
      render_info.viewport,
      render_info.scissor_rect,
      render_info.camera
    });
  }

  particle::render_particles_render_forward({
    render_info.frame_index,
    render_info.cmd,
    render_info.viewport,
    render_info.scissor_rect,
    render_info.graphics_context,
    render_info.camera
  });

  debug_image_renderer.render({
    render_info.core,
    render_info.allocator,
    render_info.command_processor,
    render_info.buffer_system,
    render_info.staging_buffer_system,
    render_info.pipeline_system,
    render_info.desc_system,
    render_info.forward_pass_info,
    render_info.sampled_image_manager,
    render_info.dynamic_sampled_image_manager,
    render_info.sampler_system,
    render_info.cmd,
    render_info.viewport,
    render_info.scissor_rect
  });
}

void RenderComponent::on_gui_update(const InitInfo& info, const GraphicsGUIUpdateResult& res) {
  //  Roots
  if (res.proc_tree_roots_params.remake_programs) {
    procedural_tree_roots_renderer.remake_programs({
      info.allocator,
      info.core,
      info.buffer_system,
      info.staging_buffer_system,
      info.pipeline_system,
      info.desc_system,
      info.uploader,
      info.frame_queue_depth,
      info.forward_pass_info,
      info.shadow_pass_info
    });
  }
  //  Ornamental foliage
  if (res.ornamental_foliage_params.disable) {
    foliage::set_render_ornamental_foliage_disabled(res.ornamental_foliage_params.disable.value());
  }
  if (res.ornamental_foliage_params.disable_stem) {
    procedural_flower_stem_renderer.set_disabled(
      res.ornamental_foliage_params.disable_stem.value());
  }
  if (res.foliage_params.enable_gpu_driven_foliage_rendering) {
    foliage::set_tree_leaves_renderer_forward_rendering_enabled(
      res.foliage_params.enable_gpu_driven_foliage_rendering.value());
  }
  if (res.foliage_params.enable_gpu_driven) {
    foliage::set_tree_leaves_renderer_enabled(res.foliage_params.enable_gpu_driven.value());
  }
  if (res.foliage_params.gpu_driven_use_tiny_array_images) {
    foliage::set_tree_leaves_renderer_use_tiny_array_images(
      res.foliage_params.gpu_driven_use_tiny_array_images.value());
  }
  if (res.foliage_params.gpu_driven_use_alpha_to_coverage) {
    foliage::set_tree_leaves_renderer_use_alpha_to_coverage(
      res.foliage_params.gpu_driven_use_alpha_to_coverage.value());
  }
  if (res.foliage_params.gpu_driven_cpu_occlusion_enabled) {
    foliage::set_tree_leaves_renderer_cpu_occlusion_enabled(
      res.foliage_params.gpu_driven_cpu_occlusion_enabled.value());
  }
  if (res.foliage_params.gpu_driven_max_shadow_cascade_index) {
    int ind = res.foliage_params.gpu_driven_max_shadow_cascade_index.value();
    if (ind >= 0) {
      foliage::set_tree_leaves_renderer_max_shadow_cascade_index(uint32_t(ind));
    }
  }
  //  cloud
  if (res.cloud_params.remake_programs) {
    cloud_renderer.remake_programs({
      info.allocator,
      info.core,
      info.buffer_system,
      info.staging_buffer_system,
      info.pipeline_system,
      info.desc_system,
      info.uploader,
      info.frame_queue_depth,
      info.post_process_pass_info,
      info.forward_pass_info
    });
  }
  if (res.cloud_params.render_enabled) {
    cloud_renderer.set_enabled(res.cloud_params.render_enabled.value());
  }
  if (res.static_model_params.remake_programs) {
    static_model_renderer.remake_programs({
      info.core,
      info.allocator,
      info.forward_pass_info,
      info.shadow_pass_info,
      info.frame_queue_depth,
      info.pipeline_system,
      info.desc_system
    });
  }
  if (res.static_model_params.disable_simple_shape_renderer) {
    simple_shape_renderer.set_disabled(res.static_model_params.disable_simple_shape_renderer.value());
  }
  //  arch
  if (res.arch_params.randomized_color) {
    arch_renderer.get_render_params()->randomized_color = res.arch_params.randomized_color.value();
  }
  if (res.arch_params.hidden) {
    arch_renderer.set_hidden(res.arch_params.hidden.value());
  }
  if (res.arch_params.remake_programs) {
    arch_renderer.remake_programs({
      info.core,
      info.allocator,
      info.pipeline_system,
      info.buffer_system,
      info.desc_system,
      info.forward_pass_info,
      info.shadow_pass_info,
      info.frame_queue_depth
    });
  }
  //  grass / terrain
  if (res.grass_params.render_low_lod) {
    grass_renderer.set_low_lod_enabled(res.grass_params.render_low_lod.value());
  }
  if (res.grass_params.render_high_lod) {
    grass_renderer.set_high_lod_enabled(res.grass_params.render_high_lod.value());
  }
  if (res.grass_params.render_high_lod_post_pass) {
    grass_renderer.set_high_lod_post_pass_enabled(
      res.grass_params.render_high_lod_post_pass.value());
  }
  if (res.grass_params.remake_programs || res.grass_params.pcf_enabled) {
    grass_renderer.remake_programs({
      *info.graphics_context,
      info.core,
      info.allocator,
      info.frame_queue_depth,
      info.forward_pass_info,
      info.pipeline_system,
      info.desc_system,
      info.buffer_system
    }, res.grass_params.pcf_enabled);
  }
  if (res.terrain_params.remake_programs) {
    terrain_renderer.remake_program({
      info.core,
      info.allocator,
      info.pipeline_system,
      info.desc_system,
      info.buffer_system,
      info.uploader,
      info.forward_pass_info,
      info.shadow_pass_info,
      info.frame_queue_depth
    });
  }
  if (res.grass_params.max_specular) {
    grass_renderer.get_render_params().max_specular = res.grass_params.max_specular.value();
  }
  if (res.grass_params.max_diffuse) {
    grass_renderer.get_render_params().max_diffuse = res.grass_params.max_diffuse.value();
  }
  if (res.grass_params.prefer_alt_color_image) {
    bool pref = res.grass_params.prefer_alt_color_image.value();
    grass_renderer.prefer_alt_color_image = pref;
    terrain_renderer.prefer_new_material_pipeline = pref;
  }
  //  cull
  if (res.cull_params.far_plane_distance) {
    cull::set_frustum_cull_far_plane_distance(res.cull_params.far_plane_distance.value());
  }
  if (res.cull_params.debug_draw) {
    cull::set_frustum_cull_debug_draw_enabled(res.cull_params.debug_draw.value());
  }
}

int RenderComponent::get_num_foliage_material1_alpha_texture_layers() {
  return foliage::get_render_ornamental_foliage_num_material1_texture_layers();
}

GROVE_NAMESPACE_END
