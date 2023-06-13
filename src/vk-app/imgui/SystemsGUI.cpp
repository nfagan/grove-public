#include "SystemsGUI.hpp"
#include "../bounds/BoundsComponent.hpp"
#include "../bounds/debug.hpp"
#include "../procedural_tree/tree_system.hpp"
#include "../procedural_tree/render_tree_system.hpp"
#include "../procedural_tree/projected_nodes.hpp"
#include "../procedural_tree/roots_system.hpp"
#include "../procedural_tree/vine_system.hpp"
#include "../procedural_tree/resource_flow_along_nodes.hpp"
#include "grove/common/common.hpp"
#include "grove/common/stats.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

namespace {

struct GlobalData {
  std::vector<uint32_t> num_contents_per_node;
} global_data;

void gather_num_contents_per_node(const bounds::Accel* accel, int num_nodes) {
  global_data.num_contents_per_node.resize(num_nodes);
  accel->num_contents_per_node(global_data.num_contents_per_node.data());
}

uint32_t compute_max_num_contents_per_node() {
  if (global_data.num_contents_per_node.empty()) {
    return 0;
  } else {
    return *std::max_element(
      global_data.num_contents_per_node.begin(),
      global_data.num_contents_per_node.end());
  }
}

float compute_mean_num_contents_per_node() {
  return float(stats::mean_double(
    global_data.num_contents_per_node.data(),
    global_data.num_contents_per_node.size()));
}

bool default_input_float(const char* label, float* v) {
  return ImGui::InputFloat(label, v, 0.0f, 0.0f, "%0.3f", ImGuiInputTextFlags_EnterReturnsTrue);
}

bool default_input_float3(const char* label, float* v) {
  return ImGui::InputFloat3(label, v, "%0.3f", ImGuiInputTextFlags_EnterReturnsTrue);
}

void render_bounds_system(const SystemsGUI& gui, const SystemsGUIRenderInfo& info,
                          SystemsGUIUpdateResult& result) {
  for (int i = 0; i < info.num_accel_instances; i++) {
    bounds::AccelInstanceHandle inst = info.accel_instances[i];
    auto* accel = bounds::request_read(info.bounds_system, inst, gui.bounds_accessor);

    int num_accel_nodes{};
    float mean_num_contents_per_node{};
    uint32_t max_num_contents_per_node{};
    uint32_t num_inactive{};
    uint32_t num_elements{};
    if (accel) {
      num_accel_nodes = int(accel->num_nodes());
      gather_num_contents_per_node(accel, num_accel_nodes);
      mean_num_contents_per_node = compute_mean_num_contents_per_node();
      max_num_contents_per_node = compute_max_num_contents_per_node();
      num_inactive = uint32_t(accel->num_inactive());
      num_elements = uint32_t(accel->num_elements());
      bounds::release_read(info.bounds_system, inst, gui.bounds_accessor);
    }

    ImGui::Text("Instance: %u; Nodes: %d", inst.id, num_accel_nodes);
    ImGui::Text("ContentsPerNode: %0.3f mean; %u max",
                mean_num_contents_per_node, max_num_contents_per_node);
    ImGui::Text("Inactive: %u (%0.3f%%)",
                num_inactive, 100.0f * float(num_inactive) / float(num_accel_nodes));
    ImGui::Text("Total: %u", num_elements);
    if (ImGui::SmallButton("Rebuild")) {
      result.need_rebuild = inst;
    }

    SystemsGUIUpdateResult::ModifyDebugInstance mod;
    mod.target = inst;
    mod.intersect_drawing_enabled = bounds::debug::intersection_drawing_enabled(inst);
    mod.intersect_bounds_scale = bounds::debug::get_intersection_drawing_bounds_scale(inst);
    if (ImGui::Checkbox("IntersectDrawingEnabled", &mod.intersect_drawing_enabled)) {
      result.modify_debug_instance = mod;
    }
    if (default_input_float3("IntersectBoundsScale", &mod.intersect_bounds_scale.x)) {
      result.modify_debug_instance = mod;
    }
  }

  auto inst_params = info.bounds_component.create_accel_instance_params;
  if (default_input_float("InitialSpanSize", &inst_params.initial_span_size)) {
    result.default_build_params = inst_params;
  }
  if (default_input_float("MaxSpanSizeSplit", &inst_params.max_span_size_split)) {
    result.default_build_params = inst_params;
  }
}

void render_tree_system(SystemsGUI& gui, const SystemsGUIRenderInfo& info,
                        SystemsGUIUpdateResult&) {
  {
    auto stats = tree::get_stats(&info.tree_system);
    ImGui::Text("Instances: %d", stats.num_instances);
    ImGui::Text("AxisDeathContexts: %d", stats.num_axis_death_contexts);
    ImGui::Text("AxisGrowthContexts: %d", stats.num_axis_growth_contexts);
    ImGui::Text("PendingDeletion: %d", stats.num_pending_deletion);
    ImGui::Text("InsertedAttractionPoints: %d", stats.num_inserted_attraction_points);
    ImGui::Text("MaxNumGeneratedStructureOneFrame: %d",
                stats.max_num_instances_generated_node_structure_in_one_frame);
    ImGui::Text("MaxTimeSpentStateGrowing: %0.3fms",
                float(stats.max_time_spent_state_growing_s * 1e3));
    ImGui::Text("MaxTimeSpentGeneratingNodeStructure: %0.3fms",
                float(stats.max_time_spent_generating_node_structure_s * 1e3));
    ImGui::Text("MaxTimeSpentPruningAgainstRadiusLimiter: %0.3fms",
                float(stats.max_time_spent_pruning_against_radius_limiter_s * 1e3));
  }
  { //  render
    auto stats = tree::get_stats(&info.render_tree_system);
    ImGui::Text("MaxMSDeletingBranches: %0.3f", float(stats.max_ms_spent_deleting_branches));
    ImGui::Text("MaxMSDeletingFoliage: %0.3f", float(stats.max_ms_spent_deleting_foliage));
    ImGui::Text("MaxNumDrawablesDestroyed: %d", int(stats.max_num_drawables_destroyed_in_one_frame));

    ImGui::InputInt("SelectedInstanceIndex", &gui.debug_ith_render_tree_instance);
    auto inst = tree::debug::get_ith_instance(
      &info.render_tree_system, gui.debug_ith_render_tree_instance);
    float* scl = &gui.debug_render_tree_instance_global_leaf_scale;
    if (inst) {
      if (ImGui::SliderFloat("LeafScale", scl, 0.0f, 1.0f)) {
        tree::set_leaf_global_scale_fraction(&info.render_tree_system, inst.value(), *scl);
      }
    }
  }
}

void render_projected_nodes_system(SystemsGUI&, const SystemsGUIRenderInfo& info,
                                   SystemsGUIUpdateResult&) {
  auto stats = tree::get_stats(&info.projected_nodes_system);
  ImGui::Text("Instances: %d", stats.num_instances);
  ImGui::Text("AxisDeathContexts: %d", stats.num_axis_death_contexts);
  ImGui::Text("AxisGrowthContexts: %d", stats.num_axis_growth_contexts);
}

void render_roots_system(SystemsGUI&, const SystemsGUIRenderInfo& info,
                         SystemsGUIUpdateResult&) {
  auto stats = tree::get_stats(&info.roots_system);
  ImGui::Text("Instances: %d", stats.num_instances);
  ImGui::Text("GrowingInstances: %d", stats.num_growing_instances);
  ImGui::Text("MaxNumNewBranchInfos: %d", stats.max_num_new_branch_infos);
}

void render_vine_system(SystemsGUI&, const SystemsGUIRenderInfo& info,
                        SystemsGUIUpdateResult&) {
  auto stats = tree::get_stats(&info.vine_system);
  ImGui::Text("Instances: %d", stats.num_instances);
  ImGui::Text("Nodes: %d", stats.num_nodes);
  ImGui::Text("Segments: %d", stats.num_segments);
}

void render_resource_flow_along_nodes(SystemsGUI&, const SystemsGUIRenderInfo&,
                                      SystemsGUIUpdateResult&) {
  auto* sys = tree::get_global_resource_spiral_around_nodes_system();
  auto stats = tree::get_stats(sys);
  ImGui::Text("Instances: %d", stats.num_instances);
  ImGui::Text("FreeInstances: %d", stats.num_free_instances);
  ImGui::Text("GlobalVel0: %0.3f", stats.current_global_vel0);
  ImGui::Text("GlobalTheta0: %0.3f", stats.current_global_theta0);
  ImGui::Text("GlobalVel1: %0.3f", stats.current_global_vel1);
  ImGui::Text("GlobalTheta1: %0.3f", stats.current_global_theta1);
}

} //  anon

SystemsGUIUpdateResult SystemsGUI::render(const SystemsGUIRenderInfo& info) {
  SystemsGUIUpdateResult result{};
  ImGui::Begin("SystemsGUI");

  if (ImGui::TreeNode("Bounds")) {
    render_bounds_system(*this, info, result);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Tree")) {
    render_tree_system(*this, info, result);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("ProjectedTree")) {
    render_projected_nodes_system(*this, info, result);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Roots")) {
    render_roots_system(*this, info, result);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Vines")) {
    render_vine_system(*this, info, result);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("ResourceFlowAlongNodes")) {
    render_resource_flow_along_nodes(*this, info, result);
    ImGui::TreePop();
  }

  if (ImGui::Button("Close")) {
    result.close = true;
  }

  ImGui::End();
  return result;
}

GROVE_NAMESPACE_END
