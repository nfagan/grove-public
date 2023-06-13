#include "projected_nodes.hpp"
#include "render.hpp"
#include "utility.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/util.hpp"
#include "grove/math/random.hpp"
#include "grove/math/frame.hpp"
#include "grove/math/ease.hpp"
#include "grove/common/common.hpp"
#include <numeric>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

using UpdateInfo = ProjectedNodesSystem::UpdateInfo;
using GrowthState = ProjectedNodesSystem::GrowthState;
using GrowthPhase = ProjectedNodesSystem::GrowthPhase;

struct Config {
  static constexpr double reference_dt = 1.0 / 60.0;
};

struct ProjectTreeNodesResult {
  tree::PostProcessProjectedNodesResult post_process_res;
  std::vector<ProjectRayResultEntry> project_ray_results;
};

tree::SpawnInternodeParams make_default_spawn_params(float diam_power) {
  tree::SpawnInternodeParams spawn_params{};
  spawn_params.leaf_diameter *= 2.0f;
  spawn_params.diameter_power = diam_power;
  return spawn_params;
}

template <typename T>
std::unique_ptr<T> require_context(std::vector<std::unique_ptr<T>>& contexts) {
  if (!contexts.empty()) {
    auto res = std::move(contexts.back());
    contexts.pop_back();
    return res;
  } else {
    return std::make_unique<T>();
  }
}

std::unique_ptr<RenderAxisGrowthContext> require_growth_context(ProjectedNodesSystem* system) {
  return require_context<RenderAxisGrowthContext>(system->render_growth_contexts);
}

std::unique_ptr<RenderAxisDeathContext> require_death_context(ProjectedNodesSystem* system) {
  return require_context<RenderAxisDeathContext>(system->render_death_contexts);
}

void return_growth_context(ProjectedNodesSystem* system,
                           std::unique_ptr<RenderAxisGrowthContext> context) {
  system->render_growth_contexts.push_back(std::move(context));
}

void return_death_context(ProjectedNodesSystem* system,
                          std::unique_ptr<RenderAxisDeathContext> context) {
  system->render_death_contexts.push_back(std::move(context));
}

template <typename Instance, typename System>
Instance* find_instance_impl(System* system, ProjectedTreeInstanceHandle handle) {
  auto it = std::find_if(
    system->instances.begin(), system->instances.end(),
    [handle](const auto& inst) {
      return inst.id == handle.id;
    });
  return it == system->instances.end() ? nullptr : &*it;
}

ProjectedNodesSystem::Instance* find_instance(ProjectedNodesSystem* system,
                                              ProjectedTreeInstanceHandle handle) {
  using Inst = ProjectedNodesSystem::Instance;
  using Sys = ProjectedNodesSystem;
  return find_instance_impl<Inst, Sys>(system, handle);
}

const ProjectedNodesSystem::Instance* find_instance(const ProjectedNodesSystem* system,
                                                    ProjectedTreeInstanceHandle handle) {
  using Inst = const ProjectedNodesSystem::Instance;
  using Sys = const ProjectedNodesSystem;
  return find_instance_impl<Inst, Sys>(system, handle);
}

Vec3<double> edge_uv_to_world_point(const uint32_t* tris, uint32_t ti,
                                    const Vec3f* ps, const Vec2f& uv = Vec2f{0.5f}) {
  return edge_uv_to_world_point(
    ps[tris[ti * 3 + 0]], ps[tris[ti * 3 + 1]], ps[tris[ti * 3 + 2]], uv);
}

double compute_initial_ray_direction(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2) {
  auto v = transform_vector_to_projected_triangle_space(p0, p1, p2, Vec3f{0.0f, 1.0f, 0.0f});
  auto init_theta = atan2(v.y, v.x);
  return init_theta >= 0 ? init_theta : float(two_pi()) + init_theta;
}

double compute_initial_ray_direction(const uint32_t* tris, uint32_t ti, const Vec3f* ps) {
  return compute_initial_ray_direction(
    ps[tris[ti * 3 + 0]], ps[tris[ti * 3 + 1]], ps[tris[ti * 3 + 2]]);
}

ProjectTreeNodesResult project_tree_nodes(const Internodes& src_inodes,
                                          const ProjectNodesOntoMeshParams* proj_params,
                                          const SpawnInternodeParams* spawn_params,
                                          const PostProcessProjectedNodesParams* post_process_params) {
  const uint32_t ti = proj_params->ti;
  const uint32_t* tris = proj_params->tris;
  const uint32_t num_tris = proj_params->num_tris;
  const Vec3f* ps = proj_params->ps;

  auto proj_res = tree::project_internodes_onto_mesh(
    tris, num_tris,
    ps, ti,
    edge_uv_to_world_point(tris, ti, ps),
    src_inodes,
    proj_params->initial_ray_theta_offset + compute_initial_ray_direction(tris, ti, ps),
    proj_params->ray_length,
    proj_params->edge_indices,
    proj_params->non_adjacent_connections);

  auto post_process_res = tree::post_process_projected_internodes(
    proj_res.internodes,
    *spawn_params,
    proj_params->ns,
    proj_res.project_ray_results.data(),
    uint32_t(proj_res.project_ray_results.size()),
    *post_process_params);

  ProjectTreeNodesResult result;
  result.post_process_res = std::move(post_process_res);
  result.project_ray_results = std::move(proj_res.project_ray_results);
  return result;
}

PostProcessProjectedNodesParams make_default_post_process_params() {
  PostProcessProjectedNodesParams result{};
  result.max_diameter = 0.1f;
  return result;
}

ProjectedNodesSystem::Instance make_instance(uint32_t id,
                                             const CreateProjectedTreeInstanceParams& params) {
  assert(params.diameter_power > 0.0f);
  ProjectedNodesSystem::Instance result{};
  result.id = id;
  result.diameter_power = params.diameter_power;
  result.post_process_params = make_default_post_process_params();
  result.axis_growth_incr = params.axis_growth_incr;
  result.ornament_growth_incr = params.ornament_growth_incr;
  return result;
}

void apply_render_growth(Internodes& inodes) {
  for (auto& inode : inodes) {
    inode.diameter = lerp(inode.length_scale, 0.0f, inode.lateral_q);
  }
}

void start_render_growth(Internodes& inodes, RenderAxisGrowthContext* context) {
  tree::initialize_depth_first_axis_render_growth_context(context, inodes, 0);
  for (auto& inode : inodes) {
    assert(inode.render_position == inode.position);
    inode.lateral_q = inode.diameter;
    inode.diameter = 0.0f;
    inode.length_scale = 0.0f;
  }
}

void start_render_growth(ProjectedNodesSystem::Instance* inst) {
  start_render_growth(*inst->get_internodes(), inst->growth_context.get());
  inst->growth_state = GrowthState::PreparingToGrow;
  inst->growing_axis_root = 0;
}

void set_growing_ornament_indices(ProjectedNodesSystem::Instance& inst,
                                  const std::vector<int>& indices) {
  inst.growing_ornament_indices.resize(indices.size());
  std::copy(indices.begin(), indices.end(), inst.growing_ornament_indices.begin());
}

void set_growing_ornament_indices_range(ProjectedNodesSystem::Instance& inst, int size) {
  auto& inds = inst.growing_ornament_indices;
  inds.resize(size);
  std::iota(inds.begin(), inds.end(), 0);
}

float ornament_growth_incr(const ProjectedNodesSystem::Instance& inst, double real_dt) {
  return inst.ornament_growth_incr * float(real_dt / Config::reference_dt);
}

bool apply_ornament_growth_incr(ProjectedNodesSystem::Instance& inst, float incr) {
  inst.ornament_growth_frac += incr;
  if (incr > 0.0f) {
    if (inst.ornament_growth_frac >= 1.0f) {
      inst.ornament_growth_frac = 1.0f;
      return true;
    }
  } else if (incr < 0.0f) {
    if (inst.ornament_growth_frac <= 0.0f) {
      inst.ornament_growth_frac = 0.0f;
      return true;
    }
  }
  return false;
}

void phase_ornaments_growing(ProjectedNodesSystem*, ProjectedNodesSystem::Instance& inst,
                             const UpdateInfo& info) {
  inst.events.ornaments_modified = true;

  if (apply_ornament_growth_incr(inst, ornament_growth_incr(inst, info.real_dt))) {
    inst.growth_phase = GrowthPhase::BranchesGrowing;
  }
}

void phase_branches_growing(ProjectedNodesSystem* system, ProjectedNodesSystem::Instance& inst,
                            const UpdateInfo& info) {
  inst.events.branches_modified = true;

  bool new_axis{};
  bool still_growing = tree::update_render_growth_depth_first(
    *inst.get_internodes(),
    *inst.growth_context,
    inst.axis_growth_incr * float(info.real_dt / Config::reference_dt),
    &new_axis);

  if (still_growing) {
    apply_render_growth(*inst.get_internodes());
    if (new_axis) {
      //  @TODO: We also need to grow ornaments after the last axis finishes growing.
      assert(inst.growing_axis_root);
      auto root_inds = tree::collect_medial_indices(
        inst.get_internodes()->data(),
        int(inst.get_internodes()->size()),
        inst.growing_axis_root.value());
      set_growing_ornament_indices(inst, root_inds);
      inst.growth_phase = GrowthPhase::OrnamentsGrowing;
      inst.growing_axis_root = inst.growth_context->depth_first_growing;
      inst.ornament_growth_frac = 0.0f;
    }
  } else {
#ifdef GROVE_DEBUG
    for (auto& node : *inst.get_internodes()) {
      assert(node.position == node.render_position);
    }
#endif
    return_growth_context(system, std::move(inst.growth_context));
    inst.growth_state = GrowthState::Idle;
    inst.growth_phase = GrowthPhase::Idle;
    inst.growing_axis_root = NullOpt{};
  }
}

void phase_ornaments_receding(ProjectedNodesSystem*, ProjectedNodesSystem::Instance& inst,
                              const UpdateInfo& info) {
  inst.events.ornaments_modified = true;

  if (apply_ornament_growth_incr(inst, -ornament_growth_incr(inst, info.real_dt))) {
    inst.growth_phase = GrowthPhase::BranchesReceding;
  }
}

void phase_branches_receding(ProjectedNodesSystem* system, ProjectedNodesSystem::Instance& inst,
                             const UpdateInfo& info) {
  inst.events.branches_modified = true;

  bool still_receding = tree::update_render_death_new_method(
    *inst.get_internodes(),
    *inst.death_context,
    inst.axis_growth_incr * float(info.real_dt / Config::reference_dt));

  if (still_receding) {
    apply_render_growth(*inst.get_internodes());
  } else {
    return_death_context(system, std::move(inst.death_context));
    inst.growth_phase = GrowthPhase::FinishedReceding;
  }
}

void state_growing(ProjectedNodesSystem* system, ProjectedNodesSystem::Instance& inst,
                   const UpdateInfo& info) {
  switch (inst.growth_phase) {
    case GrowthPhase::BranchesGrowing: {
      phase_branches_growing(system, inst, info);
      break;
    }
    case GrowthPhase::OrnamentsGrowing: {
      phase_ornaments_growing(system, inst, info);
      break;
    }
    default: {
      assert(false);
    }
  }
}

void state_receding(ProjectedNodesSystem* system, ProjectedNodesSystem::Instance& inst,
                    const UpdateInfo& info) {
  switch (inst.growth_phase) {
    case GrowthPhase::PreparingToRecede: {
      assert(!inst.death_context);
      inst.death_context = require_death_context(system);
      *inst.death_context = tree::make_default_render_axis_death_context(*inst.get_internodes());
      set_growing_ornament_indices_range(inst, int(inst.get_internodes()->size()));
      inst.growth_phase = GrowthPhase::OrnamentsReceding;
      break;
    }
    case GrowthPhase::OrnamentsReceding: {
      phase_ornaments_receding(system, inst, info);
      break;
    }
    case GrowthPhase::BranchesReceding: {
      phase_branches_receding(system, inst, info);
      break;
    }
    case GrowthPhase::FinishedReceding: {
      break;
    }
    default: {
      assert(false);
    }
  }
}

} //  anon

ProjectedTreeInstanceHandle tree::create_instance(ProjectedNodesSystem* system,
                                                  const CreateProjectedTreeInstanceParams& params) {
  const uint32_t id = system->next_instance_id++;
  system->instances.emplace_back() = make_instance(id, params);
  ProjectedTreeInstanceHandle handle{id};
  return handle;
}

void tree::destroy_instance(ProjectedNodesSystem* system, ProjectedTreeInstanceHandle handle) {
  if (auto inst = find_instance(system, handle)) {
    auto off = inst - system->instances.data();
    system->instances.erase(system->instances.begin() + off);
  } else {
    assert(false);
  }
}

const ProjectedNodesSystem::Instance* tree::read_instance(const ProjectedNodesSystem* system,
                                                          ProjectedTreeInstanceHandle handle) {
  return find_instance(system, handle);
}

void tree::set_axis_growth_increment(ProjectedNodesSystem* system,
                                     ProjectedTreeInstanceHandle handle, float incr) {
  assert(incr >= 0.0f);
  if (auto* inst = find_instance(system, handle)) {
    inst->axis_growth_incr = incr;
  } else {
    assert(false);
  }
}

void tree::set_need_start_receding(ProjectedNodesSystem* system,
                                   ProjectedTreeInstanceHandle handle) {
  if (auto* inst = find_instance(system, handle)) {
    inst->pending_state.start_receding = true;
  } else {
    assert(false);
  }
}

bool tree::is_finished_receding(const ProjectedNodesSystem* system,
                                ProjectedTreeInstanceHandle handle) {
  if (auto* inst = find_instance(system, handle)) {
    return inst->growth_state == GrowthState::Receding &&
           inst->growth_phase == GrowthPhase::FinishedReceding;
  } else {
    assert(false);
    return false;
  }
}

void tree::emplace_projected_nodes(ProjectedNodesSystem* sys,
                                   ProjectedTreeInstanceHandle handle,
                                   PostProcessProjectedNodesResult&& proj_res) {
  auto* inst = find_instance(sys, handle);
  if (!inst) {
    assert(false);
    return;
  }

  inst->project_result = std::move(proj_res);
  inst->growth_context = require_growth_context(sys);
  inst->events.nodes_created = true;
  start_render_growth(inst);
}

void tree::project_nodes_onto_mesh(ProjectedNodesSystem* system,
                                   ProjectedTreeInstanceHandle handle,
                                   const tree::Internodes& src_inodes,
                                   const ProjectNodesOntoMeshParams& params) {
  auto* inst = find_instance(system, handle);
  if (!inst) {
    assert(false);
    return;
  }

  auto spawn_params = make_default_spawn_params(inst->diameter_power);
  auto proj_res = project_tree_nodes(
    src_inodes,
    &params,
    &spawn_params,
    &inst->post_process_params);

  inst->project_result = std::move(proj_res.post_process_res);
  inst->growth_context = require_growth_context(system);
  inst->events.nodes_created = true;
  start_render_growth(inst);
}

void tree::begin_update(ProjectedNodesSystem* system) {
  for (auto& inst : system->instances) {
    inst.events = {};
  }
}

void tree::update(ProjectedNodesSystem* system, const UpdateInfo& info) {
  for (auto& inst : system->instances) {
    if (inst.growth_state == GrowthState::Idle) {
      assert(inst.growth_phase == GrowthPhase::Idle);
      if (inst.pending_state.start_receding) {
        inst.growth_state = GrowthState::Receding;
        inst.growth_phase = GrowthPhase::PreparingToRecede;
      }
    }

    if (inst.growth_state == GrowthState::PreparingToGrow) {
      inst.growth_state = GrowthState::Growing;
      inst.growth_phase = GrowthPhase::BranchesGrowing;

    } else if (inst.growth_state == GrowthState::Growing) {
      state_growing(system, inst, info);

    } else if (inst.growth_state == GrowthState::Receding) {
      state_receding(system, inst, info);
    }
  }
}

ProjectedNodesSystem::Stats tree::get_stats(const ProjectedNodesSystem* system) {
  ProjectedNodesSystem::Stats result{};
  result.num_instances = int(system->instances.size());
  result.num_axis_growth_contexts = int(system->render_growth_contexts.size());
  result.num_axis_death_contexts = int(system->render_death_contexts.size());
  return result;
}

DefaultProjectNodesOntoMeshResult
tree::default_project_nodes_onto_mesh(const Internodes& src_inodes,
                                      const ProjectNodesOntoMeshParams* proj_params,
                                      float diam_power) {
  auto spawn_params = make_default_spawn_params(diam_power);
  auto process_params = make_default_post_process_params();
  auto proj_res = project_tree_nodes(src_inodes, proj_params, &spawn_params, &process_params);
  DefaultProjectNodesOntoMeshResult result{};
  result.post_process_res = std::move(proj_res.post_process_res);
  result.project_ray_results = std::move(proj_res.project_ray_results);
  return result;
}

GROVE_NAMESPACE_END
