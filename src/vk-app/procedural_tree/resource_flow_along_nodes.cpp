#include "resource_flow_along_nodes.hpp"
#include "growth_on_nodes.hpp"
#include "tree_system.hpp"
#include "roots_system.hpp"
#include "render.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/common.hpp"
#include "grove/common/profile.hpp"
#include "grove/common/BuddyAllocator.hpp"
#include "grove/math/ease.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/util.hpp"
#include "grove/math/random.hpp"
#include "grove/math/frame.hpp"
#include <bitset>
#include <vector>

GROVE_NAMESPACE_BEGIN

namespace {

using CylinderNode = tree::ResourceSpiralCylinderNode;
using CylinderNodeAlloc = BuddyAllocator<sizeof(CylinderNode), 9>;
using CylinderNodeBlock = CylinderNodeAlloc::Block;

constexpr auto max_num_cylinder_nodes_per_instance = CylinderNodeAlloc::num_slots;
constexpr uint8_t num_param_sets = 3;

struct SpiralAroundNodes2Params {
  float vel{0.0f};
  Vec3f color{1.0f};
  float theta{pif() * 0.25f};
  float n_off{0.1f};
  float taper_frac{1.0f};
  float vel_expo_frac{};
  bool draw_frames{};
  int max_num_medial_lateral_intersect_bounds{};
  bool disable_intersect_check{true};
  float target_segment_length{4.0f};
  int num_points_per_segment{16};
  int num_quad_segments{8};
  float compute_time_ms{};
  float last_adjust_time_ms{};
  float lod_distance{64.0f};
  bool enable_lod{true};
  bool disabled{};
};

enum class SpiralAroundNodesState {
  PendingInitialization,
  TraversingAxes,
  BurrowingIntoTarget0,
  BurrowingIntoTarget1,
  FadingOut,
  Expired,
};

struct InstanceFlags {
  void set_non_fixed_parent_origin() {
    flags |= 2u;
  }
  bool non_fixed_parent_origin() const {
    return flags & 2u;
  }
  void set_burrows_into_target() {
    flags |= 1u;
  }
  bool burrows_into_target() const {
    return flags & 1u;
  }

  uint8_t flags;
};

struct ResourceSpiralInstance {
  bool active{};
  uint8_t param_set_index{};
  tree::TreeInstanceHandle associated_tree{};
  tree::RootsInstanceHandle associated_roots{};
  uint16_t num_cylinder_nodes{};
  CylinderNodeBlock cylinder_nodes{};
  SpiralAroundNodesState state{};
  InstanceFlags flags{};
  bool need_compute_next_segment{};
  bool need_destroy{};
  float theta_offset{};
  Vec3f parent_origin{};
  Stopwatch timer;
};

} //  anon

struct tree::ResourceSpiralAroundNodesSystem {
  std::vector<ResourceSpiralInstance> instances;
  std::vector<SpiralAroundNodesUpdateContext> contexts;
  std::vector<int> free_instances;
  SpiralAroundNodes2Params spiral_param_sets[num_param_sets]{};
  CylinderNodeAlloc cyl_node_alloc;
};

namespace {

using namespace tree;

using UpdateInfo = ResourceSpiralAroundNodesUpdateInfo;

const SpiralAroundNodes2Params& get_params(const ResourceSpiralAroundNodesSystem* sys,
                                           const ResourceSpiralInstance* inst) {
  assert(inst->param_set_index < num_param_sets);
  return sys->spiral_param_sets[inst->param_set_index];
}

float clamp_scale(float s) {
  return std::max(s, 1e-4f);
}

Vec3f decompose_cylinder_nodes(
  const ResourceSpiralInstance* inst, OBB3f* bounds, int* med, int* lat, int* par) {
  //
  if (inst->num_cylinder_nodes == 0) {
    return {};

  } else if (inst->num_cylinder_nodes == 1) {
    CylinderNode n0{};
    memcpy(&n0, inst->cylinder_nodes.data, sizeof(CylinderNode));
    med[0] = -1;
    lat[0] = -1;
    par[0] = -1;
    //  Arbitrary bounds.
    bounds[0] = OBB3f::axis_aligned(n0.position, Vec3f{n0.radius, 1.0f, n0.radius});
    return n0.position;
  }

  Vec3f ori{};
  for (int i = 0; i < int(inst->num_cylinder_nodes) - 1; i++) {
    CylinderNode n0;
    memcpy(&n0, inst->cylinder_nodes.data + (i + 0) * sizeof(CylinderNode), sizeof(CylinderNode));

    CylinderNode n1;
    memcpy(&n1, inst->cylinder_nodes.data + (i + 1) * sizeof(CylinderNode), sizeof(CylinderNode));

    Vec3f v = n1.position - n0.position;
    const float l = v.length();
    v = l > 0.0f ? v / l : ConstVec3f::positive_y;
    OBB3f cyl_bounds;
    make_coordinate_system_y(v, &cyl_bounds.i, &cyl_bounds.j, &cyl_bounds.k);
    cyl_bounds.position = n0.position + (n1.position - n0.position) * 0.5f;
    cyl_bounds.half_size = Vec3f{n0.radius, l * 0.5f, n0.radius};

    bounds[i] = cyl_bounds;
    med[i] = i + 1 < int(inst->num_cylinder_nodes) - 1 ? i + 1 : -1;
    lat[i] = -1;
    par[i] = i == 0 ? -1 : i - 1;

    if (i == 0) {
      ori = n0.position;
    }
  }

  return ori;
}

void decompose_internodes(const Internode* nodes, int num_nodes, OBB3f* bounds,
                          int* medial_children, int* lateral_children, int* parents) {
  tree::internode_obbs(nodes, num_nodes, bounds);
  for (int i = 0; i < num_nodes; i++) {
    medial_children[i] = nodes[i].medial_child;
    lateral_children[i] = nodes[i].lateral_child;
    parents[i] = nodes[i].parent;
  }
}

void decompose_root_nodes(const TreeRootNode* nodes, int num_nodes, OBB3f* bounds,
                          int* medial_children, int* lateral_children, int* parents) {
  for (int i = 0; i < num_nodes; i++) {
    bounds[i] = tree::make_tree_root_node_obb(nodes[i]);
    medial_children[i] = nodes[i].medial_child;
    lateral_children[i] = nodes[i].lateral_child;
    parents[i] = nodes[i].parent;
  }
}

void extract_spiral_around_nodes_quad_vertex_transforms(
  const tree::SpiralAroundNodesEntry* dst_entries, int num_entries,
  SpiralAroundNodesQuadVertexTransform* tforms) {
  //
  if (num_entries == 0) {
    return;

  } else if (num_entries == 1) {
    tforms[0] = {dst_entries[0].p, ConstVec3f::positive_x};

  } else {
    for (int i = 0; i < num_entries - 1; i++) {
      auto up = normalize(dst_entries[i + 1].p - dst_entries[i].p);
      Vec3f zs = dst_entries[i].n;
      float weight{1.0f};
      if (i > 0) {
        zs += dst_entries[i - 1].n * 0.25f;
        weight += 0.25f;
      }
      if (i + 1 < num_entries) {
        zs += dst_entries[i + 1].n * 0.25f;
        weight += 0.25f;
      }
      auto z = zs / weight;
      auto x = normalize(cross(up, z));
      z = normalize(cross(x, up));
      tforms[i] = {dst_entries[i].p, x};
    }
    tforms[num_entries - 1] = {dst_entries[num_entries - 1].p, tforms[num_entries - 2].frame_x};
  }
}

int initial_node_index(const int* med, int num_nodes, float th) {
  if (th >= 0.0f) {
    return 0;
  } else {
    int candidate{};
    for (int i = 0; i < num_nodes; i++) {
      if (med[i] == -1) {
        candidate = i;
        if (urand() < 0.25) {
          break;
        }
      }
    }
    return candidate;
  }
}

bool initialize_spiral_around_nodes_update_context(
  SpiralAroundNodesUpdateContext* context,
  const int* med, const int* lat, const int* par, const OBB3f* bounds,
  int num_internodes, const SpiralAroundNodes2Params& spiral_params, float theta_off) {
  //
  const Vec3<uint8_t> color = context->color;
  uint8_t render_pipe_index = context->render_pipeline_index;
  const float scale = context->scale;
  assert(scale > 0.0f);

  *context = {};

  constexpr int max_num_points = SpiralAroundNodesUpdateContext::max_num_points_per_segment;
  const int num_points = std::min(spiral_params.num_points_per_segment, max_num_points);
  const float target_step_size = spiral_params.target_segment_length / float(num_points);

  context->num_points_per_segment = num_points;
  context->scale = scale;
  context->color = color;
  context->render_pipeline_index = render_pipe_index;

  bool failed_init{};
  for (int s = 0; s < 2; s++) {
    const float theta = spiral_params.theta + theta_off;

    tree::SpiralAroundNodesParams params{};
    params.init_p = context->next_p;
    params.use_manual_init_p = s == 1;
    params.init_ni = s == 1 ? context->next_ni : initial_node_index(med, num_internodes, theta);
    params.n_off = spiral_params.n_off;
    params.theta = theta;
    params.step_size = target_step_size;
    params.max_num_medial_lateral_intersect_bounds = spiral_params.max_num_medial_lateral_intersect_bounds;

    tree::SpiralAroundNodesEntry dst_entries[max_num_points];
    auto res = tree::spiral_around_nodes2(
      bounds, med, lat, par, num_internodes, params, num_points, dst_entries);

    if (res.num_entries < 2) {
      failed_init = true;
      break;
    }

    SpiralAroundNodesQuadVertexTransform tforms[max_num_points];
    extract_spiral_around_nodes_quad_vertex_transforms(dst_entries, res.num_entries, tforms);
    for (int i = 0; i < res.num_entries; i++) {
      assert(context->point_segment1_end < num_points * 2);
      context->points[context->point_segment1_end++] = tforms[i];
    }

    if (s == 0) {
      context->point_segment0_end = context->point_segment1_end;
    }

    context->next_p = res.next_p;
    context->next_ni = res.next_ni;
  }

  return !failed_init;
}

bool tick_context_t(SpiralAroundNodesUpdateContext* context, double dt,
                    const SpiralAroundNodes2Params& spiral_params) {
  assert(context->velocity_scale >= -1.0f);
  double vel = 0.25 + spiral_params.vel_expo_frac * (ease::in_out_expo(context->t) * 0.5);
  context->t += (1.0f + context->velocity_scale) * spiral_params.vel * float(dt * vel);
  bool need_compute_next_segment = context->t >= 1.0f;
  while (context->t >= 1.0f) {
    context->t -= 1.0f;
  }
  return need_compute_next_segment;
}

bool compute_next_spiral_around_nodes_segment(
  SpiralAroundNodesUpdateContext* context,
  const int* med, const int* lat, const int* par, const OBB3f* bounds, int num_internodes,
  const SpiralAroundNodes2Params& spiral_params, float theta_off) {

  std::rotate(
    context->points,
    context->points + context->point_segment0_end,
    context->points + context->point_segment1_end);
  context->point_segment0_end = context->point_segment1_end - context->point_segment0_end;
  context->point_segment1_end = context->point_segment0_end;

  constexpr int max_num_points = SpiralAroundNodesUpdateContext::max_num_points_per_segment;
  const int num_points = context->num_points_per_segment;
  assert(num_points > 0 && num_points <= max_num_points);
  const float target_step_size = spiral_params.target_segment_length / float(num_points);

  tree::SpiralAroundNodesParams params{};
  params.init_p = context->next_p;
  params.use_manual_init_p = true;
  params.init_ni = context->next_ni;
  params.n_off = spiral_params.n_off;
  params.theta = spiral_params.theta + theta_off;
  params.step_size = target_step_size;
  params.max_num_medial_lateral_intersect_bounds = spiral_params.max_num_medial_lateral_intersect_bounds;
  tree::SpiralAroundNodesEntry dst_entries[max_num_points];
  auto res = tree::spiral_around_nodes2(
    bounds, med, lat, par, num_internodes, params, num_points, dst_entries);

  SpiralAroundNodesQuadVertexTransform tforms[max_num_points];
  extract_spiral_around_nodes_quad_vertex_transforms(dst_entries, res.num_entries, tforms);
  for (int i = 0; i < res.num_entries; i++) {
    assert(context->point_segment1_end < context->num_points_per_segment * 2);
    context->points[context->point_segment1_end++] = tforms[i];
  }

  context->next_ni = res.next_ni;
  context->next_p = res.next_p;

  if (res.reached_axis_end) {
    context->t = 0.0f;
    return true;
  } else {
    return false;
  }
}

void state_pending_initialization(ResourceSpiralAroundNodesSystem* sys, ResourceSpiralInstance* inst,
                                  SpiralAroundNodesUpdateContext* context, const UpdateInfo& info) {
  Temporary<int, 1024> store_med_children;
  Temporary<int, 1024> store_lat_children;
  Temporary<int, 1024> store_parents;
  Temporary<OBB3f, 1024> store_bounds;

  int* med{};
  int* lat{};
  int* par{};
  OBB3f* bounds{};
  Vec3f parent_origin{};

  int num_nodes{};
  if (inst->associated_tree.is_valid()) {
    auto tree_inst = tree::read_tree(info.tree_sys, inst->associated_tree);
    if (!tree_inst.nodes) {
      return;
    }

    parent_origin = tree_inst.nodes->origin();

    num_nodes = int(tree_inst.nodes->internodes.size());
    med = store_med_children.require(num_nodes);
    lat = store_lat_children.require(num_nodes);
    par = store_parents.require(num_nodes);
    bounds = store_bounds.require(num_nodes);
    decompose_internodes(tree_inst.nodes->internodes.data(), num_nodes, bounds, med, lat, par);

  } else if (inst->associated_roots.is_valid()) {
    auto roots_inst = tree::read_roots_instance(info.roots_sys, inst->associated_roots);
    if (!roots_inst.roots || roots_inst.state != TreeRootsState::Alive) {
      return;
    }

    parent_origin = roots_inst.roots->origin;

    num_nodes = roots_inst.roots->curr_num_nodes;
    med = store_med_children.require(num_nodes);
    lat = store_lat_children.require(num_nodes);
    par = store_parents.require(num_nodes);
    bounds = store_bounds.require(num_nodes);
    decompose_root_nodes(roots_inst.roots->nodes.data(), num_nodes, bounds, med, lat, par);

  } else if (inst->num_cylinder_nodes > 1) {
    //  @NOTE: Cylinder nodes are points between which lines are implicitly drawn, but the spiral
    //  around nodes procedure operates on internodes.
    const int num_cyl_nodes = int(inst->num_cylinder_nodes);
    med = store_med_children.require(num_cyl_nodes);
    lat = store_lat_children.require(num_cyl_nodes);
    par = store_parents.require(num_cyl_nodes);
    bounds = store_bounds.require(num_cyl_nodes);
    parent_origin = decompose_cylinder_nodes(inst, bounds, med, lat, par);
    num_nodes = num_cyl_nodes - 1;
  }

  if (num_nodes > 0) {
    const bool success = initialize_spiral_around_nodes_update_context(
      context, med, lat, par, bounds, num_nodes, get_params(sys, inst), inst->theta_offset);
    if (success) {
      inst->state = SpiralAroundNodesState::TraversingAxes;
      context->active = true;
    }

    inst->timer.reset();
    inst->parent_origin = parent_origin;
  }
}

void begin_fadeout(ResourceSpiralInstance* inst, SpiralAroundNodesUpdateContext* context) {
  inst->state = SpiralAroundNodesState::FadingOut;
  inst->timer.reset();
  context->fadeout = true;
  context->fade_frac = 0.0f;
}

void end_fadeout(ResourceSpiralInstance* inst, SpiralAroundNodesUpdateContext*) {
  inst->state = inst->need_destroy ?
    SpiralAroundNodesState::Expired : SpiralAroundNodesState::PendingInitialization;
}

void state_traversing_axes(ResourceSpiralAroundNodesSystem* sys, ResourceSpiralInstance* inst,
                           SpiralAroundNodesUpdateContext* context, const UpdateInfo& info) {
  context->fade_frac = float(clamp01(inst->timer.delta().count() / 1.0));

  const float prev_context_t = context->t;
  if (!inst->need_compute_next_segment) {
    if (tick_context_t(context, info.real_dt, get_params(sys, inst))) {
      inst->need_compute_next_segment = true;
    } else {
      return;
    }
  }

  Temporary<int, 1024> store_med_children;
  Temporary<int, 1024> store_lat_children;
  Temporary<int, 1024> store_parents;
  Temporary<OBB3f, 1024> store_bounds;

  int* med{};
  int* lat{};
  int* par{};
  OBB3f* bounds{};

  int num_nodes{};
  if (inst->associated_tree.is_valid()) {
    auto tree_inst = tree::read_tree(info.tree_sys, inst->associated_tree);
    if (!tree_inst.nodes) {
      return;
    }

    num_nodes = int(tree_inst.nodes->internodes.size());
    med = store_med_children.require(num_nodes);
    lat = store_lat_children.require(num_nodes);
    par = store_parents.require(num_nodes);
    bounds = store_bounds.require(num_nodes);
    decompose_internodes(tree_inst.nodes->internodes.data(), num_nodes, bounds, med, lat, par);

  } else if (inst->associated_roots.is_valid()) {
    auto roots_inst = tree::read_roots_instance(info.roots_sys, inst->associated_roots);
    if (!roots_inst.roots || roots_inst.state != TreeRootsState::Alive) {
      context->t = prev_context_t;
      inst->need_compute_next_segment = false;
      if (inst->flags.burrows_into_target()) {
        inst->state = SpiralAroundNodesState::BurrowingIntoTarget0;
      } else {
        begin_fadeout(inst, context);
      }
      return;
    }

    num_nodes = roots_inst.roots->curr_num_nodes;
    med = store_med_children.require(num_nodes);
    lat = store_lat_children.require(num_nodes);
    par = store_parents.require(num_nodes);
    bounds = store_bounds.require(num_nodes);
    decompose_root_nodes(roots_inst.roots->nodes.data(), num_nodes, bounds, med, lat, par);

  } else if (inst->num_cylinder_nodes > 1) {
    const auto num_cyl_nodes = int(inst->num_cylinder_nodes);
    num_nodes = num_cyl_nodes - 1;

    med = store_med_children.require(num_cyl_nodes);
    lat = store_lat_children.require(num_cyl_nodes);
    par = store_parents.require(num_cyl_nodes);
    bounds = store_bounds.require(num_cyl_nodes);
    decompose_cylinder_nodes(inst, bounds, med, lat, par);
  }

  if (context->next_ni >= num_nodes) {
    //  Traveling along non-existent node, possibly because branch was pruned.
    inst->need_compute_next_segment = false;
    begin_fadeout(inst, context);
    return;
  }

  if (num_nodes > 0) {
    const float t = context->t;
    bool reached_end = compute_next_spiral_around_nodes_segment(
      context, med, lat, par, bounds, num_nodes, get_params(sys, inst), inst->theta_offset);
    if (reached_end) {
      if (inst->flags.burrows_into_target()) {
        context->t = t;
        inst->state = SpiralAroundNodesState::BurrowingIntoTarget0;
      } else {
        begin_fadeout(inst, context);
      }
    }

    inst->need_compute_next_segment = false;
  }
}

void state_burrowing_into_target0(ResourceSpiralAroundNodesSystem* sys, ResourceSpiralInstance* inst,
                                  SpiralAroundNodesUpdateContext* context, const UpdateInfo& info) {
  if (tick_context_t(context, info.real_dt, get_params(sys, inst))) {
    inst->state = SpiralAroundNodesState::BurrowingIntoTarget1;
    context->burrowing = true;
  }
}

void state_burrowing_into_target1(ResourceSpiralAroundNodesSystem* sys, ResourceSpiralInstance* inst,
                                  SpiralAroundNodesUpdateContext* context, const UpdateInfo& info) {
  if (tick_context_t(context, info.real_dt, get_params(sys, inst))) {
    context->fadeout = true;
    context->fade_frac = 1.0f;
    end_fadeout(inst, context);
  }
}

void update_instance(ResourceSpiralAroundNodesSystem* sys, ResourceSpiralInstance* inst,
                     SpiralAroundNodesUpdateContext* context, const UpdateInfo& info) {
  if (inst->state == SpiralAroundNodesState::PendingInitialization) {
    state_pending_initialization(sys, inst, context, info);

  } else if (inst->state == SpiralAroundNodesState::TraversingAxes) {
    state_traversing_axes(sys, inst, context, info);

  } else if (inst->state == SpiralAroundNodesState::BurrowingIntoTarget0) {
    state_burrowing_into_target0(sys, inst, context, info);

  } else if (inst->state == SpiralAroundNodesState::BurrowingIntoTarget1) {
    state_burrowing_into_target1(sys, inst, context, info);

  } else if (inst->state == SpiralAroundNodesState::FadingOut) {
    const double fade_t = 2.0;
    float t = float(clamp(inst->timer.delta().count(), 0.0, fade_t) / fade_t);
    context->fade_frac = t;
    if (t == 1.0f) {
      end_fadeout(inst, context);
    }
  }
#if 1
  if (inst->flags.non_fixed_parent_origin() && context->point_segment0_end > 0) {
    inst->parent_origin = context->points[0].p;
  }
#endif
}

int acquire_instance(ResourceSpiralAroundNodesSystem* sys, const CreateResourceSpiralParams& params) {
  assert(params.scale >= 0.0f);
  assert(params.global_param_set_index < num_param_sets);

  int inst_index;
  if (sys->free_instances.empty()) {
    inst_index = int(sys->instances.size());
    sys->instances.emplace_back();
    sys->contexts.emplace_back();
  } else {
    inst_index = sys->free_instances.back();
    sys->free_instances.pop_back();
  }

  auto& inst = sys->instances[inst_index];
  inst = {};
  inst.active = true;
  inst.theta_offset = params.theta_offset;
  if (params.burrows_into_target) {
    inst.flags.set_burrows_into_target();
  }
  if (params.non_fixed_parent_origin) {
    inst.flags.set_non_fixed_parent_origin();
  }
  inst.param_set_index = params.global_param_set_index;

  auto& ctx = sys->contexts[inst_index];
  ctx = {};
  ctx.color = params.linear_color;
  ctx.render_pipeline_index = params.render_pipeline_index;
  ctx.scale = clamp_scale(params.scale);

  return inst_index;
}

void destroy_instance(ResourceSpiralAroundNodesSystem* sys, int i) {
#ifdef GROVE_DEBUG
  auto free_it = std::find(sys->free_instances.begin(), sys->free_instances.end(), i);
  assert(free_it == sys->free_instances.end());
#endif

  auto& inst = sys->instances[i];
  auto& ctx = sys->contexts[i];

  if (inst.num_cylinder_nodes > 0) {
    sys->cyl_node_alloc.free(inst.cylinder_nodes);
  }

  inst = {};
  ctx = {};
  sys->free_instances.push_back(i);
}

void get_instance_and_context(
  ResourceSpiralAroundNodesSystem* sys, ResourceSpiralAroundNodesHandle handle,
  ResourceSpiralInstance** inst, SpiralAroundNodesUpdateContext** context) {
  //
  assert(handle.is_valid());
  int index = handle.index - 1; //  @NOTE: 1-based index
  assert(index < int(sys->contexts.size()));
  assert(sys->instances[index].active);
  *inst = &sys->instances[index];
  *context = &sys->contexts[index];
}

[[maybe_unused]]
SpiralAroundNodesUpdateContext* get_context(
  ResourceSpiralAroundNodesSystem* sys, ResourceSpiralAroundNodesHandle handle) {
  //
  SpiralAroundNodesUpdateContext* result{};
  ResourceSpiralInstance* ignore{};
  get_instance_and_context(sys, handle, &ignore, &result);
  return result;
}

struct {
  ResourceSpiralAroundNodesSystem system;
} globals;

} //  anon

ResourceSpiralAroundNodesHandle tree::create_resource_spiral_around_tree(
  ResourceSpiralAroundNodesSystem* sys, const TreeInstanceHandle& tree,
  const CreateResourceSpiralParams& params) {
  //
  assert(tree.is_valid());
  int inst_index = acquire_instance(sys, params);
  auto& inst = sys->instances[inst_index];
  inst.associated_tree = tree;
  //  @NOTE: 1-based index
  return ResourceSpiralAroundNodesHandle{inst_index + 1};
}

ResourceSpiralAroundNodesHandle tree::create_resource_spiral_around_roots(
  ResourceSpiralAroundNodesSystem* sys, const RootsInstanceHandle& roots,
  const CreateResourceSpiralParams& params) {
  //
  assert(roots.is_valid());
  int inst_index = acquire_instance(sys, params);
  auto& inst = sys->instances[inst_index];
  inst.associated_roots = roots;
  //  @NOTE: 1-based index
  return ResourceSpiralAroundNodesHandle{inst_index + 1};
}

ResourceSpiralAroundNodesHandle tree::create_resource_spiral_around_line_of_cylinders(
  ResourceSpiralAroundNodesSystem* sys, const ResourceSpiralCylinderNode* nodes, int num_nodes,
  const CreateResourceSpiralParams& params) {
  //
  assert(num_nodes > 1 && num_nodes <= int(max_num_cylinder_nodes_per_instance));
  (void) max_num_cylinder_nodes_per_instance;

  CylinderNodeBlock block = sys->cyl_node_alloc.allocate(sizeof(CylinderNode) * num_nodes);
  memcpy(block.data, nodes, num_nodes * sizeof(CylinderNode));

  int inst_index = acquire_instance(sys, params);
  auto& inst = sys->instances[inst_index];
  inst.cylinder_nodes = block;
  inst.num_cylinder_nodes = uint16_t(num_nodes);
  //  @NOTE: 1-based index
  return ResourceSpiralAroundNodesHandle{inst_index + 1};
}

void tree::destroy_resource_spiral(
  ResourceSpiralAroundNodesSystem* sys, ResourceSpiralAroundNodesHandle handle) {
  //
  assert(handle.is_valid());
  int index = handle.index - 1; //  @NOTE: 1-based index
  assert(index < int(sys->instances.size()));
#ifdef GROVE_DEBUG
  auto free_it = std::find(sys->free_instances.begin(), sys->free_instances.end(), index);
  assert(free_it == sys->free_instances.end());
#endif
  auto& inst = sys->instances[index];
  auto& ctx = sys->contexts[index];
  assert(!inst.need_destroy && inst.active);
  ctx.fadeout = true;
  ctx.fade_frac = 0.0f;
  inst.need_destroy = true;
  inst.timer.reset();
  if (inst.state != SpiralAroundNodesState::Expired) {
    inst.state = SpiralAroundNodesState::FadingOut;
  }
}

void tree::set_resource_spiral_scale(
  ResourceSpiralAroundNodesSystem* sys, ResourceSpiralAroundNodesHandle handle, float s) {
  //
  assert(s >= 0.0f);
  ResourceSpiralInstance* inst;
  SpiralAroundNodesUpdateContext* ctx;
  get_instance_and_context(sys, handle, &inst, &ctx);
  (void) inst;
  ctx->scale = clamp_scale(s);
}

void tree::set_resource_spiral_velocity_scale(
  ResourceSpiralAroundNodesSystem* sys, ResourceSpiralAroundNodesHandle handle, float s) {
  //
  assert(s >= -1.0f);
  ResourceSpiralInstance* inst;
  SpiralAroundNodesUpdateContext* ctx;
  get_instance_and_context(sys, handle, &inst, &ctx);
  (void) inst;
  ctx->velocity_scale = std::max(-1.0f, s);
}

ResourceSpiralAroundNodesSystem* tree::get_global_resource_spiral_around_nodes_system() {
  return &globals.system;
}

void tree::terminate_resource_spiral_around_nodes_system(ResourceSpiralAroundNodesSystem* sys) {
  for (int i = 0; i < int(sys->instances.size()); i++) {
    if (sys->instances[i].active) {
      destroy_instance(sys, i);
    }
  }
}

const SpiralAroundNodesUpdateContext*
tree::read_contexts(const ResourceSpiralAroundNodesSystem* sys, int* num_contexts) {
  *num_contexts = int(sys->contexts.size());
  return sys->contexts.data();
}

ResourceSpiralAroundNodesSystemStats
tree::get_stats(const ResourceSpiralAroundNodesSystem* sys) {
  ResourceSpiralAroundNodesSystemStats stats{};
  stats.num_instances = int(sys->instances.size());
  stats.num_free_instances = int(sys->free_instances.size());
  stats.current_global_theta0 = sys->spiral_param_sets[0].theta;
  stats.current_global_vel0 = sys->spiral_param_sets[0].vel;
  stats.current_global_theta1 = sys->spiral_param_sets[1].theta;
  stats.current_global_vel1 = sys->spiral_param_sets[1].vel;
  return stats;
}

void tree::update_resource_spiral_around_nodes(ResourceSpiralAroundNodesSystem* sys,
                                               const ResourceSpiralAroundNodesUpdateInfo& info) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("update_resource_spiral_around_nodes");
  (void) profiler;

  for (int i = 0; i < int(sys->instances.size()); i++) {
    if (sys->instances[i].active) {
      update_instance(sys, &sys->instances[i], &sys->contexts[i], info);
    }
  }

  for (int i = 0; i < int(sys->instances.size()); i++) {
    auto& po = sys->instances[i].parent_origin;
    sys->contexts[i].distance_to_camera = (po - info.camera_position).length();
  }

  for (int i = 0; i < int(sys->instances.size()); i++) {
    auto& inst = sys->instances[i];
    if (inst.active && inst.need_destroy && inst.state == SpiralAroundNodesState::Expired) {
      destroy_instance(sys, i);
    }
  }
}

void tree::set_global_velocity_scale(ResourceSpiralAroundNodesSystem* sys, uint8_t set, float v) {
  assert(set < num_param_sets);
  assert(v >= 0.0f);
  sys->spiral_param_sets[set].vel = v;
}

void tree::set_global_theta(ResourceSpiralAroundNodesSystem* sys, uint8_t set, float th) {
  assert(set < num_param_sets);
  sys->spiral_param_sets[set].theta = th;
}

GROVE_NAMESPACE_END