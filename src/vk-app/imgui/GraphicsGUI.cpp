#include "GraphicsGUI.hpp"
#include "../render/graphics.hpp"
#include "../render/RenderComponent.hpp"
#include "../render/ShadowComponent.hpp"
#include "../render/graphics_context.hpp"
#include "../render/frustum_cull_gpu.hpp"
#include "../render/render_tree_leaves_gpu.hpp"
#include "../render/render_tree_leaves.hpp"
#include "../render/render_tree_leaves_types.hpp"
#include "../render/frustum_cull_data.hpp"
#include "../render/frustum_cull_types.hpp"
#include "../render/render_branch_nodes_gpu.hpp"
#include "../render/render_branch_nodes_types.hpp"
#include "../render/render_branch_nodes.hpp"
#include "../render/render_ornamental_foliage_gpu.hpp"
#include "../render/gen_depth_pyramid_gpu.hpp"
#include "../render/occlusion_cull_gpu.hpp"
#include "../render/render_particles_gpu.hpp"
#include "../render/render_gui_gpu.hpp"
#include "../procedural_tree/render_tree_system.hpp"
#include "grove/common/common.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

namespace {

bool default_input_float(const char* name, float* p) {
  return ImGui::InputFloat(name, p, 0, 0, nullptr, ImGuiInputTextFlags_EnterReturnsTrue);
}

[[maybe_unused]]
void render_proc_tree_roots_params(const RenderComponent&,
                                   GraphicsGUIUpdateResult& result) {
  if (ImGui::Button("RemakePrograms")) {
    result.proc_tree_roots_params.remake_programs = true;
  }
}

void render_ornamental_foliage_params(const RenderComponent&, GraphicsGUIUpdateResult& result) {
  bool disabled = foliage::get_render_ornamental_foliage_disabled();
  if (ImGui::Checkbox("Disabled", &disabled)) {
    result.ornamental_foliage_params.disable = disabled;
  }

  if (ImGui::Button("ToggleAlphaTest")) {
    if (disabled) {
      result.ornamental_foliage_params.disable = false;
    } else {
      result.ornamental_foliage_params.disable = true;
    }
  }

  const auto stats = foliage::get_render_ornamental_foliage_stats();
  ImGui::Text("NumFlatPlaneSmall: %d", int(stats.num_flat_plane_small_instances));
  ImGui::Text("NumFlatPlaneLarge: %d", int(stats.num_flat_plane_large_instances));
  ImGui::Text("NumSmallCurvedPlane: %d", int(stats.num_curved_plane_small_instances));
  ImGui::Text("NumLargeCurvedPlane: %d", int(stats.num_curved_plane_large_instances));
  ImGui::Text("WroteToIndices: %d", int(stats.wrote_to_indices_buffers));
  ImGui::Text("WroteToInstances: %d", int(stats.wrote_to_instance_buffers));
}

void render_branch_node_params(const RenderComponent&) {
  bool disabled = tree::get_render_branch_nodes_disabled();
  if (ImGui::Checkbox("Disabled", &disabled)) {
    tree::set_render_branch_nodes_disabled(disabled);
  }

  bool base_disabled = tree::get_set_render_branch_nodes_disable_base_drawables(nullptr);
  if (ImGui::Checkbox("BaseDisabled", &base_disabled)) {
    tree::get_set_render_branch_nodes_disable_base_drawables(&base_disabled);
  }

  bool wind_disabled = tree::get_set_render_branch_nodes_disable_wind_drawables(nullptr);
  if (ImGui::Checkbox("WindDisabled", &wind_disabled)) {
    tree::get_set_render_branch_nodes_disable_wind_drawables(&wind_disabled);
  }

  bool base_as_quads = tree::get_set_render_branch_nodes_render_base_drawables_as_quads(nullptr);
  if (ImGui::Checkbox("RenderBaseAsQuads", &base_as_quads)) {
    tree::get_set_render_branch_nodes_render_base_drawables_as_quads(&base_as_quads);
  }

  bool wind_as_quads = tree::get_set_render_branch_nodes_render_wind_drawables_as_quads(nullptr);
  if (ImGui::Checkbox("RenderWindAsQuads", &wind_as_quads)) {
    tree::get_set_render_branch_nodes_render_wind_drawables_as_quads(&wind_as_quads);
  }

  bool base_shadow_disabled = tree::get_set_render_branch_nodes_base_shadow_disabled(nullptr);
  if (ImGui::Checkbox("BaseShadowDisabled", &base_shadow_disabled)) {
    tree::get_set_render_branch_nodes_base_shadow_disabled(&base_shadow_disabled);
  }

  bool wind_shadow_disabled = tree::get_set_render_branch_nodes_wind_shadow_disabled(nullptr);
  if (ImGui::Checkbox("WindShadowDisabled", &wind_shadow_disabled)) {
    tree::get_set_render_branch_nodes_wind_shadow_disabled(&wind_shadow_disabled);
  }

  auto max_cascade_ind = int(tree::get_set_render_branch_nodes_max_cascade_index(nullptr));
  if (ImGui::InputInt("MaxCascadeIndex", &max_cascade_ind) && max_cascade_ind >= 0) {
    auto res = uint32_t(max_cascade_ind);
    tree::get_set_render_branch_nodes_max_cascade_index(&res);
  }

  auto stats = tree::get_render_branch_nodes_stats();
  ImGui::Text("PrevNumBaseForward: %d", int(stats.prev_num_base_forward_instances));
  ImGui::Text("PrevNumWindForward: %d", int(stats.prev_num_wind_forward_instances));
  ImGui::Text("UsedOcclusionCullingForBase: %d", int(stats.rendered_base_forward_with_occlusion_culling));
  ImGui::Text("UsedOcclusionCullingForWind: %d", int(stats.rendered_wind_forward_with_occlusion_culling));

  const auto* rd = tree::get_global_branch_nodes_data();
  ImGui::Text("NumWindInstances: %d", int(rd->wind_set.num_instances()));
  ImGui::Text("NumWindAggregates: %d", int(rd->wind_set.num_aggregates()));
  ImGui::Text("NumBaseInstances: %d", int(rd->base_set.num_instances()));
  ImGui::Text("NumBaseAggregates: %d", int(rd->base_set.num_aggregates()));

  bool cull_enabled = tree::get_set_render_branch_nodes_prefer_cull_enabled(nullptr);
  if (ImGui::Checkbox("EnableCull", &cull_enabled)) {
    tree::get_set_render_branch_nodes_prefer_cull_enabled(&cull_enabled);
  }

  auto* rp = tree::get_render_branch_nodes_render_params();
  ImGui::Checkbox("LimitToMaxNumInstances", &rp->limit_to_max_num_instances);

  bool use_low_lod = tree::get_set_render_branch_nodes_prefer_low_lod_geometry(nullptr);
  if (ImGui::Checkbox("UseLowLODGeometry", &use_low_lod)) {
    tree::get_set_render_branch_nodes_prefer_low_lod_geometry(&use_low_lod);
  }

  int num_insts = int(rp->max_num_instances);
  if (ImGui::InputInt("MaxNumInstances", &num_insts) && num_insts >= 0) {
    rp->max_num_instances = uint32_t(num_insts);
  }
}

void render_gpu_driven_foliage_params(
  GraphicsGUI& gui, const RenderComponent&, tree::RenderTreeSystem& render_tree_sys,
  GraphicsGUIUpdateResult& result) {
  //
  bool enabled = foliage::get_tree_leaves_renderer_enabled();
  if (ImGui::Checkbox("Enabled", &enabled)) {
    result.foliage_params.enable_gpu_driven = enabled;
  }

  bool forward_enabled = foliage::get_tree_leaves_renderer_forward_rendering_enabled();
  if (ImGui::Checkbox("EnableForwardRendering", &forward_enabled)) {
    result.foliage_params.enable_gpu_driven_foliage_rendering = forward_enabled;
  }

  bool shadow_disabled = foliage::get_set_tree_leaves_renderer_shadow_rendering_disabled(nullptr);
  if (ImGui::Checkbox("DisableShadowRendering", &shadow_disabled)) {
    foliage::get_set_tree_leaves_renderer_shadow_rendering_disabled(&shadow_disabled);
  }

  bool use_tiny_array_ims = foliage::get_tree_leaves_renderer_use_tiny_array_images();
  if (ImGui::Checkbox("UseTinyArrayImages", &use_tiny_array_ims)) {
    result.foliage_params.gpu_driven_use_tiny_array_images = use_tiny_array_ims;
  }

  bool use_alpha_to_coverage = foliage::get_tree_leaves_renderer_use_alpha_to_coverage();
  if (ImGui::Checkbox("UseAlphaToCoverage", &use_alpha_to_coverage)) {
    result.foliage_params.gpu_driven_use_alpha_to_coverage = use_alpha_to_coverage;
  }

  bool use_mip_maps = foliage::get_set_tree_leaves_renderer_use_mip_mapped_images(nullptr);
  if (ImGui::Checkbox("UseMipMaps", &use_mip_maps)) {
    foliage::get_set_tree_leaves_renderer_use_mip_mapped_images(&use_mip_maps);
  }

  bool one_alpha_chan = foliage::get_set_tree_leaves_renderer_use_single_channel_alpha_images(nullptr);
  if (ImGui::Checkbox("OneAlphaChannel", &one_alpha_chan)) {
    foliage::get_set_tree_leaves_renderer_use_single_channel_alpha_images(&one_alpha_chan);
  }

  bool use_image_mix = foliage::get_set_tree_leaves_renderer_prefer_color_image_mix_pipeline(nullptr);
  if (ImGui::Checkbox("UseColorImageMix", &use_image_mix)) {
    foliage::get_set_tree_leaves_renderer_prefer_color_image_mix_pipeline(&use_image_mix);
  }

  bool pcf_disabled = foliage::get_set_tree_leaves_renderer_pcf_disabled(nullptr);
  if (ImGui::Checkbox("PCFDisabled", &pcf_disabled)) {
    foliage::get_set_tree_leaves_renderer_pcf_disabled(&pcf_disabled);
  }

  bool high_lod_disabled = foliage::get_set_tree_leaves_renderer_disable_high_lod(nullptr);
  if (ImGui::Checkbox("HighLODDisabled", &high_lod_disabled)) {
    foliage::get_set_tree_leaves_renderer_disable_high_lod(&high_lod_disabled);
  }

  bool color_mix_disabled = foliage::get_set_tree_leaves_renderer_color_mix_disabled(nullptr);
  if (ImGui::Checkbox("ColorMixDisabled", &color_mix_disabled)) {
    foliage::get_set_tree_leaves_renderer_color_mix_disabled(&color_mix_disabled);
  }

  auto* rp = foliage::get_tree_leaves_render_params();
  if (ImGui::SliderFloat("ColorImageMix", &rp->global_color_image_mix, 0.0f, 1.0f)) {
    foliage::set_tree_leaves_color_image_mix_fraction_all_groups(rp->global_color_image_mix);
  }

  bool use_cpu_occlusion = foliage::get_tree_leaves_renderer_cpu_occlusion_enabled();
  if (ImGui::Checkbox("CPUOcclusionEnabled", &use_cpu_occlusion)) {
    result.foliage_params.gpu_driven_cpu_occlusion_enabled = use_cpu_occlusion;
  }

  bool pref_gpu_occlusion = foliage::get_set_tree_leaves_renderer_prefer_gpu_occlusion(nullptr);
  if (ImGui::Checkbox("PreferGPUOcclusion", &pref_gpu_occlusion)) {
    foliage::get_set_tree_leaves_renderer_prefer_gpu_occlusion(&pref_gpu_occlusion);
  }

  bool post_comp_disabled =
    foliage::get_set_tree_leaves_renderer_post_forward_graphics_compute_disabled(nullptr);
  if (ImGui::Checkbox("PostComputeDisabled", &post_comp_disabled)) {
    foliage::get_set_tree_leaves_renderer_post_forward_graphics_compute_disabled(&post_comp_disabled);
  }

  auto max_cascade_ind = int(foliage::get_tree_leaves_renderer_max_shadow_cascade_index());
  if (ImGui::InputInt("MaxShadowCascadeIndex", &max_cascade_ind)) {
    result.foliage_params.gpu_driven_max_shadow_cascade_index = max_cascade_ind;
  }

  int lod = tree::get_preferred_foliage_lod(&render_tree_sys);
  if (ImGui::InputInt("TreeLeavesLOD", &lod)) {
    tree::maybe_set_preferred_foliage_lod(&render_tree_sys, lod);
  }

  ImGui::Checkbox("PreferFixedTime", &rp->prefer_fixed_time);
  ImGui::SliderFloat("FixedTime", &rp->fixed_time, 0.0f, 20.0f);

  if (ImGui::Button("RecreatePipelines")) {
    foliage::recreate_tree_leaves_renderer_pipelines();
  }

  int comp_local_size = foliage::get_set_tree_leaves_renderer_compute_local_size_x(nullptr);
  if (ImGui::InputInt("ComputeLocalSize", &comp_local_size, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) {
    foliage::get_set_tree_leaves_renderer_compute_local_size_x(&comp_local_size);
  }

  auto stats = foliage::get_tree_leaves_renderer_stats();
  ImGui::Text("NumForwardInstances: %d", int(stats.prev_total_num_forward_instances));
  ImGui::Text("NumShadowInstances: %d", int(stats.num_shadow_instances));
  ImGui::Text("NumLOD0ForwardInstances: %d", int(stats.prev_num_lod0_forward_instances));
  ImGui::Text("NumLOD1ForwardInstances: %d", int(stats.prev_num_lod1_forward_instances));
  ImGui::Text("NumVertices: %d", int(stats.prev_num_forward_vertices_drawn));

  int sum_instances = int(stats.prev_total_num_forward_instances);
  sum_instances += int(stats.prev_total_num_post_forward_instances);
  ImGui::Text("NumPostForwardInstances: %d", int(stats.prev_total_num_post_forward_instances));
  ImGui::Text("NumLOD0PostForwardInstances: %d", int(stats.prev_num_lod0_post_forward_instances));
  ImGui::Text("NumLOD1PostForwardInstances: %d", int(stats.prev_num_lod1_post_forward_instances));
  ImGui::Text("PostNumVertices: %d", int(stats.prev_num_post_forward_vertices_drawn));

  const auto* rd = foliage::get_global_tree_leaves_render_data();
  ImGui::Text("NumCPUInstances: %d", int(rd->num_instances()));
  ImGui::Text("NumCPUInstanceGroups: %d", int(rd->num_instance_groups()));

  ImGui::Checkbox("ShowStats", &gui.show_foliage_stats);
  if (gui.show_foliage_stats) {
    if (ImGui::InputInt("QueryPoolSize", &gui.foliage_query_pool_size)) {
      gui.foliage_query_pool_size = std::max(1, gui.foliage_query_pool_size);
    }

    auto rd_stats = foliage::get_tree_leaves_render_data_stats(rd, gui.foliage_query_pool_size);
    ImGui::Text("NumActive: %d", int(rd_stats.num_active_instances));
    ImGui::Text("NumInactive: %d", int(rd_stats.num_inactive_instances));
    ImGui::Text("MaxPerGroup: %d", int(rd_stats.max_num_instances_in_group));
    ImGui::Text("MinPerGroup: %d", int(rd_stats.min_num_instances_in_group));
    ImGui::Text("MaxPerGroup: %d", int(rd_stats.max_num_instances_in_group));
    ImGui::Text("MeanPerGroup: %0.3f", float(rd_stats.mean_num_instances_per_group));
    ImGui::Text("NumWouldOverdraw: %d", int(rd_stats.num_would_overdraw_with_query_pool_size));
    ImGui::Text("WouldOverdraw: %0.3f%%", 1e2f * float(rd_stats.frac_would_overdraw_with_query_pool_size));
  }

  float prop_drawn = rd->num_instances() == 0 ? 0.0f :
    float(sum_instances) / float(rd->num_instances());
  ImGui::Text("TotalNumInstancesForwardRendered: %d", sum_instances);
  ImGui::Text("PropInstancesForwardRendered: %0.2f", prop_drawn);
  ImGui::Text("RenderedWithGPUOcclusion: %d", int(stats.did_render_with_gpu_occlusion));

  bool clear_via_copy = foliage::get_set_tree_leaves_renderer_do_clear_indirect_commands_via_explicit_buffer_copy(nullptr);
  if (ImGui::Checkbox("ClearIndirectBuffsViaExplicitCopy", &clear_via_copy)) {
    foliage::get_set_tree_leaves_renderer_do_clear_indirect_commands_via_explicit_buffer_copy(&clear_via_copy);
  }
}

void render_shadow_component_params(const ShadowComponent& component,
                                    GraphicsGUIUpdateResult& result) {
  auto& sun_csm_desc = component.get_sun_csm_descriptor();
  float proj_sign_y = sun_csm_desc.sign_y;
  if (default_input_float("ProjSignY", &proj_sign_y)) {
    result.shadow_component_params.projection_sign_y = proj_sign_y;
  }
}

void render_cloud_params(RenderComponent& component, GraphicsGUIUpdateResult& result) {
  if (ImGui::Button("RemakeProgram")) {
    result.cloud_params.remake_programs = true;
  }
  bool render_enabled = component.cloud_renderer.is_enabled();
  if (ImGui::Checkbox("RenderingEnabled", &render_enabled)) {
    result.cloud_params.render_enabled = render_enabled;
  }

  bool volume_enabled = component.cloud_renderer.is_volume_enabled();
  if (ImGui::Checkbox("VolumeEnabled", &volume_enabled)) {
    component.cloud_renderer.set_volume_enabled(volume_enabled);
  }
}

void render_static_model_params(const RenderComponent& component,
                                GraphicsGUIUpdateResult& result) {
  if (ImGui::Button("RemakePrograms")) {
    result.static_model_params.remake_programs = true;
  }

  {
    bool disabled = component.simple_shape_renderer.is_disabled();
    if (ImGui::Checkbox("SimpleShapeRendererDisabled", &disabled)) {
      result.static_model_params.disable_simple_shape_renderer = disabled;
    }
  }
}

void render_arch_params(const RenderComponent& component, GraphicsGUIUpdateResult& result) {
  auto* params = component.arch_renderer.get_render_params();
  bool rand_color = params->randomized_color;
  if (ImGui::Checkbox("RandomizedColor", &rand_color)) {
    result.arch_params.randomized_color = rand_color;
  }
  bool hidden = component.arch_renderer.is_hidden();
  if (ImGui::Checkbox("Hidden", &hidden)) {
    result.arch_params.hidden = hidden;
  }
  if (ImGui::Button("RemakePrograms")) {
    result.arch_params.remake_programs = true;
  }
}

void render_grass_params(RenderComponent& component, GraphicsGUIUpdateResult& result) {
  auto& grass_renderer = component.grass_renderer;

  ImGui::Text("Drew: %d", int(grass_renderer.get_latest_total_num_vertices_drawn()));

  bool high_lod_enabled = grass_renderer.is_high_lod_enabled();
  if (ImGui::Checkbox("HighLODEnabled", &high_lod_enabled)) {
    result.grass_params.render_high_lod = high_lod_enabled;
  }

  bool low_lod_enabled = grass_renderer.is_low_lod_enabled();
  if (ImGui::Checkbox("LowLODEnabled", &low_lod_enabled)) {
    result.grass_params.render_low_lod = low_lod_enabled;
  }

  bool high_lod_post_enabled = grass_renderer.is_high_lod_post_pass_enabled();
  if (ImGui::Checkbox("HighLODPostPassEnabled", &high_lod_post_enabled)) {
    result.grass_params.render_high_lod_post_pass = high_lod_post_enabled;
  }

  ImGui::Checkbox("NewMaterialPipeline", &grass_renderer.prefer_new_material_pipeline);
  if (ImGui::Button("RecreateNewMaterialPipelines")) {
    grass_renderer.need_recreate_new_pipelines = true;
    component.terrain_renderer.need_create_new_material_pipeline = true;
  }
  if (ImGui::Button("ToggleOriginal")) {
    grass_renderer.toggle_new_material_pipeline();
    component.terrain_renderer.toggle_new_material_pipeline();
  }

  if (ImGui::TreeNode("NewMaterialParams")) {
    auto& rp = grass_renderer.get_render_params();
    ImGui::Checkbox("PreferSeasonControlled", &rp.prefer_season_controlled_new_material_params);
    ImGui::Checkbox("PreferRevisedParams", &rp.prefer_revised_new_material_params);

    if (ImGui::SmallButton("OtherGreen")) {
      rp.new_material_params = {};
      rp.new_material_params.base_color0 = Vec3f{0.15f, 0.606f, 0.067f};
      rp.new_material_params.base_color1 = Vec3f{0.22f, 0.659f, 0.112f};
      rp.new_material_params.tip_color = Vec3f{1.0f};
      rp.new_material_params.spec_scale = 0.4f;
      rp.new_material_params.spec_power = 1.0f;
      rp.new_material_params.min_overall_scale = 0.85f;
      rp.new_material_params.max_overall_scale = 1.45f;
      rp.new_material_params.min_color_variation = 0.0f;
      rp.new_material_params.max_color_variation = 1.0f;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("OtherGreen2")) {
      rp.new_material_params = {};
      rp.new_material_params.base_color0 = Vec3f{0.15f, 0.606f, 0.067f};
      rp.new_material_params.base_color1 = Vec3f{0.275f, 0.9f, 0.112f};
      rp.new_material_params.tip_color = Vec3f{1.0f};
      rp.new_material_params.spec_scale = 0.4f;
      rp.new_material_params.spec_power = 1.0f;
      rp.new_material_params.min_overall_scale = 0.85f;
      rp.new_material_params.max_overall_scale = 1.45f;
      rp.new_material_params.min_color_variation = 0.0f;
      rp.new_material_params.max_color_variation = 1.0f;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Fall")) {
      rp.new_material_params = {};
      rp.new_material_params.base_color0 = Vec3f{0.286f, 0.45f, 0.173f};
      rp.new_material_params.base_color1 = Vec3f{0.375f, 1.0f, 0.222f};
      rp.new_material_params.tip_color = Vec3f{0.8f, 1.0f, 0.901f};
      rp.new_material_params.spec_scale = 0.4f;
      rp.new_material_params.spec_power = 1.558f;
      rp.new_material_params.min_overall_scale = 0.85f;
      rp.new_material_params.max_overall_scale = 1.25f;
      rp.new_material_params.min_color_variation = 0.25f;
      rp.new_material_params.max_color_variation = 0.755f;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("OneColor")) {
      rp.new_material_params = {};
      rp.new_material_params.base_color0 = Vec3f{0.443f, 1.0f, 0.281f};
      rp.new_material_params.base_color1 = Vec3f{0.443f, 1.0f, 0.281f};
      rp.new_material_params.spec_scale = 0.4f;
      rp.new_material_params.spec_power = 4.0f;
      rp.new_material_params.min_overall_scale = 0.85f;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("MatchOrig")) {
      rp.new_material_params = {};
      rp.new_material_params.spec_scale = 0.4f;
      rp.new_material_params.spec_power = 1.0f;
      rp.new_material_params.base_color1.y = 1.0f;
      rp.new_material_params.min_overall_scale = 0.85f;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("MostlyGreen")) {
      rp.new_material_params = {};
      rp.new_material_params.base_color0 = Vec3f{0.179f, 1.0f, 0.175f};
      rp.new_material_params.base_color1 = Vec3f{0.179f, 1.0f, 0.175f};
      rp.new_material_params.spec_scale = 0.478f;
      rp.new_material_params.spec_power = 1.7f;
      rp.new_material_params.min_overall_scale = 0.85f;
    }
    if (ImGui::SmallButton("MoreGreen")) {
      rp.new_material_params.base_color0.x = 0.265f;
    }
    if (ImGui::SmallButton("Default")) {
      rp.new_material_params = {};
    }
    ImGui::SliderFloat3("BaseColor0", &rp.new_material_params.base_color0.x, 0.0f, 2.0f);
    ImGui::SliderFloat3("BaseColor1", &rp.new_material_params.base_color1.x, 0.0f, 2.0f);
    ImGui::SliderFloat3("TipColor", &rp.new_material_params.tip_color.x, 0.0f, 1.0f);
    ImGui::SliderFloat("SpecScale", &rp.new_material_params.spec_scale, 0.0f, 2.0f);
    ImGui::SliderFloat("SpecPower", &rp.new_material_params.spec_power, 0.25f, 16.0f);
    ImGui::SliderFloat("MinOverallScale", &rp.new_material_params.min_overall_scale, 0.0f, 4.0f);
    ImGui::SliderFloat("MaxOverallScale", &rp.new_material_params.max_overall_scale, 0.0f, 4.0f);
    ImGui::SliderFloat("MinColorVariation", &rp.new_material_params.min_color_variation, 0.0f, 1.0f);
    ImGui::SliderFloat("MaxColorVariation", &rp.new_material_params.max_color_variation, 0.0f, 1.0f);
    ImGui::TreePop();
  }

  bool pcf_enabled = grass_renderer.is_pcf_enabled();
  if (ImGui::Checkbox("PCFEnabled", &pcf_enabled)) {
    result.grass_params.pcf_enabled = pcf_enabled;
  }

  bool prefer_alt_color = grass_renderer.prefer_alt_color_image;
  if (ImGui::Checkbox("PreferAltColorImage", &prefer_alt_color)) {
    result.grass_params.prefer_alt_color_image = prefer_alt_color;
  }

  const auto& render_params = grass_renderer.get_render_params();
  float max_diff = render_params.max_diffuse;
  if (ImGui::SliderFloat("MaxDiffuse", &max_diff, 0.0f, 1.0f)) {
    result.grass_params.max_diffuse = max_diff;
  }
  float max_spec = render_params.max_specular;
  if (ImGui::SliderFloat("MaxSpecular", &max_spec, 0.0f, 1.0f)) {
    result.grass_params.max_specular = max_spec;
  }

  if (ImGui::Button("LessDiffuse")) {
    result.grass_params.max_diffuse = 0.45f;
  }

  if (ImGui::Button("RemakePrograms")) {
    result.grass_params.remake_programs = true;
  }
}

void render_terrain_params(RenderComponent& component, GraphicsGUIUpdateResult& result) {
  bool pcf_enabled = component.terrain_renderer.pcf_enabled;
  if (ImGui::Checkbox("PCFEnabled", &pcf_enabled)) {
    component.terrain_renderer.set_pcf_enabled = pcf_enabled;
  }

  ImGui::Checkbox(
    "PreferInvertedWinding",
    &component.terrain_renderer.prefer_inverted_winding_new_material_pipeline);

  ImGui::Checkbox("Disabled", &component.terrain_renderer.disabled);
  ImGui::Text("NumCubeMarchChunksDrawn: %d",
              int(component.terrain_renderer.latest_num_cube_march_chunks_drawn));
  ImGui::Text("NumCubeMarchVerticesDrawn: %d",
              int(component.terrain_renderer.latest_num_cube_march_vertices_drawn));
  if (ImGui::Button("RemakePrograms")) {
    result.terrain_params.remake_programs = true;
  }
}

void render_graphics_context(GraphicsGUI& gui, vk::GraphicsContext& context,
                             const gfx::Context& opaque_graphics_context,
                             GraphicsGUIUpdateResult&) {
  const auto& pipe_sys = context.pipeline_system;
  const auto& buffer_sys = context.buffer_system;
  const auto& staging_buffer_sys = context.staging_buffer_system;
  const auto& desc_sys = context.descriptor_system;
  const auto& simple_desc_sys = context.simple_descriptor_system;
  const auto& sampler_sys = context.sampler_system;
  const auto& sampled_image_manager = context.sampled_image_manager;
  const auto& dynamic_sampled_image_manager = context.dynamic_sampled_image_manager;
  const auto to_mb = [](size_t bytes) {
    return double(bytes) / 1024.0 / 1024.0;
  };

  bool present_pass_enabled = vk::get_present_pass_enabled(&context);
  if (ImGui::Checkbox("PresentPassEnabled", &present_pass_enabled)) {
    vk::set_present_pass_enabled(&context, present_pass_enabled);
  }

  {
    auto internal_res = vk::get_internal_forward_resolution(&context);
    Vec2<int> res{int(internal_res.width), int(internal_res.height)};
    if (ImGui::InputInt2("InternalResolution", &res.x, ImGuiInputTextFlags_EnterReturnsTrue)) {
      VkExtent2D extent{uint32_t(res.x), uint32_t(res.y)};
      vk::set_internal_forward_resolution(&context, extent);
    }
  }

  ImGui::Checkbox("ShowStats", &gui.show_context_stats);
  if (gui.show_context_stats) {
    auto gfx_stats = gfx::get_stats(&opaque_graphics_context);
    ImGui::Text("GraphicsContextPipelines: %u", gfx_stats.num_pipelines);
    ImGui::Text("GraphicsContextBuffers: %u", gfx_stats.num_buffers);
    ImGui::Text("GraphicsContextBufferMB: %0.3f", to_mb(gfx_stats.buffer_mb));
    ImGui::Text("PipelineSystemPipelines: %u", (uint32_t) pipe_sys.num_pipelines());
    ImGui::Text("DescSetLayouts: %u", (uint32_t) pipe_sys.num_descriptor_set_layouts());
    ImGui::Text("PipelineLayouts: %u", (uint32_t) pipe_sys.num_pipeline_layouts());
    ImGui::Text("BufferSystemBuffers: %u", (uint32_t) buffer_sys.num_buffers());
    ImGui::Text("BufferSystemBufferMB: %0.3f", to_mb(buffer_sys.approx_num_bytes_used()));
    ImGui::Text("StagingBuffers: %u", (uint32_t) staging_buffer_sys.num_buffers());
    ImGui::Text("StagingBufferMB: %0.3f", to_mb(staging_buffer_sys.approx_num_bytes_used()));
    ImGui::Text("MaxSimpleDescSetUpdateTime: %0.3f",
                simple_desc_sys.max_ms_spent_requiring_descriptor_sets);
    ImGui::Text("LatestSimpleDescSetUpdateTime: %0.3f",
                simple_desc_sys.latest_ms_spent_requiring_descriptor_sets);
    ImGui::Text("SimpleDescPools: %u", simple_desc_sys.total_num_descriptor_pools());
    ImGui::Text("SimpleDescSets: %u", simple_desc_sys.total_num_descriptor_sets());
    ImGui::Text("DescPoolAllocators: %u", (uint32_t) desc_sys.num_descriptor_pool_allocators());
    ImGui::Text("DescSetAllocators: %u", (uint32_t) desc_sys.num_descriptor_set_allocators());
    ImGui::Text("DescSets: %u", (uint32_t) desc_sys.num_descriptor_sets());
    ImGui::Text("DescPools: %u", (uint32_t) desc_sys.num_descriptor_pools());
    ImGui::Text("Samplers: %u", (uint32_t) sampler_sys.num_samplers());
    ImGui::Text("SampledImages: %u", (uint32_t) sampled_image_manager.num_instances());
    ImGui::Text("SampledImageMB: %0.3f", to_mb(sampled_image_manager.approx_image_memory_usage()));
    ImGui::Text("DynamicSampledImages: %u",
                (uint32_t) dynamic_sampled_image_manager.num_instances());
    ImGui::Text("DynamicSampledImageMB: %0.3f", to_mb(
      dynamic_sampled_image_manager.approx_image_memory_usage()));
    ImGui::Text("ForwardWriteBackPassImageMB: %0.3f", to_mb(
      context.forward_write_back_pass.approx_image_memory_usage()));
    ImGui::Text("ShadowPassImageMB: %0.3f", to_mb(context.shadow_pass.approx_image_memory_usage()));
    ImGui::Text("PostProcessPassImageMB: %0.3f", to_mb(
      context.post_process_pass.approx_image_memory_usage()));
  }
}

void render_cull_data(const cull::FrustumCullData* cull_data, uint32_t dsi) {
  ImGui::Text("NumGroups: %d", int(cull_data->num_group_offsets()));
  ImGui::Text("NumInstances: %d", int(cull_data->num_instances()));

  auto pyr_stats = cull::get_occlusion_cull_against_depth_pyramid_stats(dsi);
  float p_occluded{};
  if (pyr_stats.prev_num_total > 0) {
    p_occluded = float(pyr_stats.prev_num_purely_occlusion_culled) / float(pyr_stats.prev_num_total);
  }

  ImGui::Text("NumOcclusionCullOccluded: %d", int(pyr_stats.prev_num_occluded));
  ImGui::Text("NumOcclusionCullVisible: %d", int(pyr_stats.prev_num_visible));
  ImGui::Text("NumOcclusionCullTotal: %d", int(pyr_stats.prev_num_total));
  ImGui::Text("NumFrustumCulled: %d", int(pyr_stats.prev_num_frustum_culled));
  ImGui::Text("P Additionally culled: %0.3f", p_occluded);
}

void render_cull_params(GraphicsGUIUpdateResult& result) {
  float cam_far = cull::get_frustum_cull_far_plane_distance();
  if (ImGui::SliderFloat("CameraFar", &cam_far, 2.0f, 1024.0f)) {
    result.cull_params.far_plane_distance = cam_far;
  }

  bool debug_draw = cull::get_frustum_cull_debug_draw_enabled();
  if (ImGui::Checkbox("DebugDrawEnabled", &debug_draw)) {
    result.cull_params.debug_draw = debug_draw;
  }

  if (ImGui::TreeNode("Leaves")) {
    render_cull_data(cull::get_global_tree_leaves_frustum_cull_data(), 0);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Branches")) {
    render_cull_data(cull::get_global_branch_nodes_frustum_cull_data(), 1);
    ImGui::TreePop();
  }
}

void render_depth_pyramid_params() {
  bool enabled = gpu::get_set_gen_depth_pyramid_enabled(nullptr);
  if (ImGui::Checkbox("Enabled", &enabled)) {
    gpu::get_set_gen_depth_pyramid_enabled(&enabled);
  }
}

void render_ui_params(GraphicsGUIUpdateResult&, RenderComponent& render_component) {
  auto stats = gui::get_render_gui_stats();
  ImGui::Text("NumQuadVerts: %d", int(stats.num_quad_vertices));
  ImGui::Text("NumGlyphQuadVerts: %d", int(stats.num_glyph_quad_vertices));
  ImGui::Checkbox("RenderAtNativeRes", &render_component.prefer_to_render_ui_at_native_resolution);
  if (ImGui::Button("RemakePipelines")) {
    gui::render_gui_remake_pipelines();
  }
}

void render_particle_params() {
  auto stats = particle::get_render_particles_stats();
  ImGui::Text("NumSegQuadVertices: %d", int(stats.last_num_segmented_quad_vertices));
  ImGui::Text("NumSegQuadSampleDepthVertices: %d",
              int(stats.last_num_segmented_quad_sample_depth_vertices));
  ImGui::Text("NumCircleQuadSampleDepthInstances: %d",
              int(stats.last_num_circle_quad_sample_depth_instances));
  if (ImGui::Button("RemakePipelines")) {
    particle::set_render_particles_need_remake_pipelines();
  }
}

} //  anon

GraphicsGUIUpdateResult GraphicsGUI::render(vk::GraphicsContext& graphics_context,
                                            const gfx::Context& opaque_graphics_context,
                                            RenderComponent& render_component,
                                            const ShadowComponent& shadow_component,
                                            tree::RenderTreeSystem& render_tree_system) {
  GraphicsGUIUpdateResult result{};
  ImGui::Begin("GraphicsGUI");
  if (ImGui::TreeNode("Context")) {
    render_graphics_context(*this, graphics_context, opaque_graphics_context, result);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Shadow")) {
    render_shadow_component_params(shadow_component, result);
    ImGui::TreePop();
  }
#if 0
  if (ImGui::TreeNode("ProceduralTreeRenderer")) {
    render_proc_tree_params(render_component, result);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("ProceduralTreeRootsRenderer")) {
    render_proc_tree_roots_params(render_component, result);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("ProceduralTreeLeavesRenderer")) {
    render_proc_tree_leaves_params(render_component, result);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("ProceduralFlowerOrnamentRenderer")) {
    render_proc_ornament_params(render_component, result);
    ImGui::TreePop();
  }
#endif
  if (ImGui::TreeNode("OrnamentalFoliage")) {
    render_ornamental_foliage_params(render_component, result);
    ImGui::TreePop();
  }
#if 0
  if (ImGui::TreeNode("FoliageRenderer")) {
    render_foliage_params(render_component, result);
    ImGui::TreePop();
  }
#endif
  if (ImGui::TreeNode("GPUDrivenFoliage")) {
    render_gpu_driven_foliage_params(*this, render_component, render_tree_system, result);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("BranchNodes")) {
    render_branch_node_params(render_component);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("CloudRenderer")) {
    render_cloud_params(render_component, result);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("StaticModelRenderer")) {
    render_static_model_params(render_component, result);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("ArchRenderer")) {
    render_arch_params(render_component, result);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("GrassRenderer")) {
    render_grass_params(render_component, result);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("TerrainRenderer")) {
    render_terrain_params(render_component, result);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Cull")) {
    render_cull_params(result);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("DepthPyramid")) {
    render_depth_pyramid_params();
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("UI")) {
    render_ui_params(result, render_component);
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Particle")) {
    render_particle_params();
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("General")) {
    ImGui::Checkbox("RenderGrassLate", &render_component.render_grass_late);

    if (ImGui::Button("RemakeToonLightDependent")) {
      result.proc_tree_params.remake_programs = true;
      result.static_model_params.remake_programs = true;
    }
    ImGui::TreePop();
  }
  if (ImGui::Button("Close")) {
    result.close = true;
  }
  ImGui::End();
  return result;
}

GROVE_NAMESPACE_END
