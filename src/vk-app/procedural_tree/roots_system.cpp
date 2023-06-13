#include "roots_system.hpp"
#include "roots_growth.hpp"
#include "roots_utility.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include <unordered_map>
#include <memory>

#define ENABLE_BRANCH_INFOS (1)

GROVE_NAMESPACE_BEGIN

namespace tree {

struct Config {
  static constexpr int max_num_nodes_per_roots = 512;
};

struct AddRootsParams {
  Vec3f origin;
  Vec3f init_direction;
  float node_length;
  float leaf_diameter;
  float diameter_power;
  int max_num_nodes;
};

struct PruningContext {
  std::vector<int> pruned_dst_to_src;
  std::vector<TreeRootNodeIndices> pruned_node_indices;
  std::unordered_set<int> skip_receding;
};

struct GrowthEvaluationOrder {
  void add_instance(RootsInstanceHandle handle) {
    order.push_back(handle);
  }

  void remove_instance(RootsInstanceHandle handle) {
    auto it = std::find(order.begin(), order.end(), handle);
    assert(it != order.end());
    const int ind = int(it - order.begin());
    order.erase(it);
    if (next_instance > ind) {
      next_instance--;
    }
    if (next_instance >= int(order.size())) {
      next_instance = 0;
    }
    assert((order.empty() && next_instance == 0) || next_instance < int(order.size()));
  }

  std::vector<RootsInstanceHandle> order;
  int next_instance{};
};

struct RootsInstance {
  struct PendingStateChanges {
    bool any() const {
      return need_start_dying || need_destroy || need_start_pruning;
    }

    bool need_start_dying{};
    bool need_destroy{};
    bool need_start_pruning{};
  };

  TreeRoots roots;
  std::vector<bounds::RadiusLimiterElementHandle> radius_limiter_elements;
  std::unique_ptr<PruningContext> pruning_context;
  TreeRootsGrowthContext growth_context;
  TreeRootsRecedeContext recede_context;
  bool need_init_recede_context{};
  TreeRootsState state{};
  PendingStateChanges pending_state_changes{};

  float min_axis_length_spawn_lateral{16.0f};
  double p_spawn_lateral{0.1};
  Vec3f attractor_point_position{};
  float attractor_point_scale{};

  CreateRootsInstanceParams create_params{};

  RootsEvents events{};
};

struct RootsSystem {
  std::unordered_map<uint32_t, RootsInstance> instances;
  uint32_t next_instance_id{1};
  GrowthEvaluationOrder growth_evaluation_order;

  bounds::RadiusLimiterElementTag roots_element_tag{};

  float growth_rate_scale{1.0f};
  Vec3f global_attractor_point{};
  float global_attractor_point_scale{};
  bool prefer_global_attractor_point{true};
  float spectral_fraction{};
  bool attenuate_growth_rate_by_spectral_fraction{true};

  double global_p_spawn_lateral{0.1};
  bool prefer_global_p_spawn_lateral{};

  std::vector<RootsNewBranchInfo> new_branch_infos;
  int max_num_new_branch_infos{};
};

} //  tree

namespace {

using namespace tree;

using UpdateInfo = RootsSystemUpdateInfo;

RootsInstance* find_instance(RootsSystem* sys, RootsInstanceHandle inst) {
  auto it = sys->instances.find(inst.id);
  return it == sys->instances.end() ? nullptr : &it->second;
}

const RootsInstance* find_instance(const RootsSystem* sys, RootsInstanceHandle inst) {
  auto it = sys->instances.find(inst.id);
  return it == sys->instances.end() ? nullptr : &it->second;
}

GrowRootsParams to_grow_roots_params(const RootsSystem& sys, const RootsInstance& inst,
                                     double real_dt, bool disable_node_creation) {
  float gr = sys.growth_rate_scale;
  if (sys.attenuate_growth_rate_by_spectral_fraction) {
    gr *= sys.spectral_fraction;
  }

  GrowRootsParams grow_params{};
  grow_params.real_dt = real_dt;
  grow_params.growth_rate = gr;

  if (sys.prefer_global_attractor_point) {
    grow_params.attractor_point_scale = sys.global_attractor_point_scale;
    grow_params.attractor_point = sys.global_attractor_point;
  } else {
    grow_params.attractor_point_scale = inst.attractor_point_scale;
    grow_params.attractor_point = inst.attractor_point_position;
  }

  if (sys.prefer_global_p_spawn_lateral) {
    grow_params.p_spawn_lateral = sys.global_p_spawn_lateral;
  } else {
    grow_params.p_spawn_lateral = inst.p_spawn_lateral;
  }

  grow_params.node_length_scale = inst.roots.node_length_scale;
  grow_params.min_axis_length_spawn_lateral = inst.min_axis_length_spawn_lateral;
  grow_params.disable_node_creation = disable_node_creation;
  return grow_params;
}

AssignRootsDiameterParams to_assign_diameter_params(const TreeRoots& roots) {
  AssignRootsDiameterParams diam_params{};
  diam_params.leaf_diameter = roots.leaf_diameter;
  diam_params.diameter_power = roots.diameter_power;
  return diam_params;
}

RootsInstance make_instance(const CreateRootsInstanceParams& params) {
  RootsInstance result{};
  result.state = TreeRootsState::PendingInit;
  result.create_params = params;
  return result;
}

AddRootsParams to_add_roots_params(const RootsSystem&, const RootsInstance& inst) {
  assert(std::abs(inst.create_params.init_direction.length() - 1.0f) < 1e-3f);
  AddRootsParams result{};
  result.origin = inst.create_params.origin;
  result.init_direction = inst.create_params.init_direction;
  result.node_length = 1.0f;      //  @TODO
  result.leaf_diameter = 0.075f;  //  @TODO
  result.diameter_power = 1.8f;   //  @TODO
  result.max_num_nodes = Config::max_num_nodes_per_roots;
  return result;
}

[[maybe_unused]] bool all_radius_limiter_elements_invalid(const RootsInstance& inst) {
  assert(int(inst.radius_limiter_elements.size()) >= inst.roots.curr_num_nodes);
  for (int i = 0; i < inst.roots.curr_num_nodes; i++) {
    if (inst.radius_limiter_elements[i] != bounds::RadiusLimiterElementHandle::invalid()) {
      return false;
    }
  }
  return true;
}

void init_instance(RootsInstance& inst, const AddRootsParams& params,
                   bounds::RadiusLimiter* radius_limiter,
                   bounds::RadiusLimiterElementTag roots_tag) {
  assert(inst.roots.nodes.empty() && inst.radius_limiter_elements.empty() &&
         inst.growth_context.growing.empty());

  const auto roots_id = bounds::RadiusLimiterAggregateID::create();

  inst.roots = make_tree_roots(
    roots_id, params.max_num_nodes, params.origin, params.init_direction,
    params.node_length, params.leaf_diameter, params.leaf_diameter, params.diameter_power);

  auto& rad_lims = inst.radius_limiter_elements;
  rad_lims.resize(params.max_num_nodes);
  std::fill(rad_lims.begin(), rad_lims.end(), bounds::RadiusLimiterElementHandle::invalid());

  auto root_el = make_tree_root_node_radius_limiter_element(
    make_tree_root_node_obb(inst.roots.nodes[0]), roots_id, roots_tag);
  inst.radius_limiter_elements[0] = bounds::insert(radius_limiter, root_el);

  inst.growth_context.growing.push_back(make_growing_tree_root_node(0));
}

auto grow_instance(RootsSystem* sys, RootsInstance& inst, bool disable_node_creation,
                   const UpdateInfo& info) {
  struct Result {
    bool finished_growing;
    int num_new_nodes;
    int num_new_branches;
    int next_growing_ni_begin;
  };

  auto grow_params = to_grow_roots_params(*sys, inst, info.real_dt, disable_node_creation);
  auto diam_params = to_assign_diameter_params(inst.roots);

  const auto grow_res = grow_roots(
    &inst.roots, info.radius_limiter, inst.radius_limiter_elements.data(),
    sys->roots_element_tag, inst.growth_context, grow_params, diam_params);

  Result result{};
  result.finished_growing = grow_res.finished;
  result.num_new_nodes = grow_res.num_nodes_added;
  result.num_new_branches = grow_res.num_new_branches;
  result.next_growing_ni_begin = grow_res.next_growing_ni_begin;
  return result;
}

auto recede_instance(RootsSystem* sys, RootsInstance& inst, const UpdateInfo& info) {
  struct Result {
    bool finished_receding;
  };

  auto& roots = inst.roots;
  auto& recede_ctx = inst.recede_context;
  auto& lim_els = inst.radius_limiter_elements;

  if (inst.need_init_recede_context) {
    init_roots_recede_context(&recede_ctx, roots.nodes.data(), roots.curr_num_nodes);
    inst.need_init_recede_context = false;
  }

  auto grow_params = to_grow_roots_params(*sys, inst, info.real_dt, false);
  const auto recede_res = recede_roots(
    &roots, info.radius_limiter, lim_els.data(), recede_ctx, grow_params);

  Result result{};
  result.finished_receding = recede_res.finished;
  return result;
}

auto prune_instance(RootsSystem* sys, RootsInstance& inst, const UpdateInfo& info) {
  struct Result {
    bool finished_pruning;
  };

  auto& roots = inst.roots;
  auto& recede_ctx = inst.recede_context;
  auto& pc = inst.pruning_context;
  auto& lim_els = inst.radius_limiter_elements;
  assert(pc);

  if (inst.need_init_recede_context) {
    init_roots_recede_context(
      &recede_ctx, roots.nodes.data(), roots.curr_num_nodes, &pc->skip_receding);
    inst.need_init_recede_context = false;
  }

  auto grow_params = to_grow_roots_params(*sys, inst, info.real_dt, false);
  auto prune_res = prune_roots(
    &roots, info.radius_limiter, lim_els.data(), recede_ctx, grow_params);

  Result result{};
  result.finished_pruning = prune_res.finished;
  return result;
}

void move_from_pruning_context_to_pruned_nodes(RootsInstance& inst) {
  auto& pc = inst.pruning_context;
  assert(pc);

#ifdef GROVE_DEBUG
  for (int i = 0; i < inst.roots.curr_num_nodes; i++) {
    if (pc->skip_receding.count(i) == 0) {
      assert(inst.radius_limiter_elements[i] == bounds::RadiusLimiterElementHandle::invalid());
    } else {
      assert(inst.radius_limiter_elements[i] != bounds::RadiusLimiterElementHandle::invalid());
    }
  }
#endif

  auto new_nodes = inst.roots.nodes;
  const auto num_new_nodes = int(pc->pruned_dst_to_src.size());
  tree::copy_nodes_applying_node_indices(
    inst.roots.nodes.data(),
    pc->pruned_dst_to_src.data(),
    pc->pruned_node_indices.data(), num_new_nodes, new_nodes.data());

  auto new_rad_lims = inst.radius_limiter_elements;
  std::fill(new_rad_lims.begin(), new_rad_lims.end(), bounds::RadiusLimiterElementHandle::invalid());
  for (int i = 0; i < num_new_nodes; i++) {
    new_rad_lims[i] = inst.radius_limiter_elements[pc->pruned_dst_to_src[i]];
  }

  inst.roots.curr_num_nodes = num_new_nodes;
  inst.roots.nodes = std::move(new_nodes);
  inst.radius_limiter_elements = std::move(new_rad_lims);

  //  Delete the context
  inst.pruning_context = nullptr;
}

[[maybe_unused]]
void push_new_branch_infos(RootsSystem* sys, const RootsInstance& inst, int next_gi) {
  const auto& growing = inst.growth_context.growing;
  const auto num_growing = int(growing.size());
  assert(next_gi >= 0 && next_gi <= num_growing);

  auto& dst_infos = sys->new_branch_infos;
  const auto* nodes = inst.roots.nodes.data();

  for (int gi = next_gi; gi < num_growing; gi++) {
    const int ni = growing[gi].index;
    assert(ni >= 0 && ni < inst.roots.curr_num_nodes);

    auto& node = nodes[ni];
    if (node.is_axis_root(ni, nodes)) {
      auto& branch_info = dst_infos.emplace_back();
      branch_info.position = node.position;
    }
  }
}

auto update_growing(RootsSystem* sys, const UpdateInfo& info) {
  struct Result {
    int num_new_branches;
  };

  Result result{};

  const int max_num_add{64};  //  @TODO
//  const int max_num_add = std::numeric_limits<int>::max();

  bool disable_node_creation{};
  bool growth_terminated_early{};
  int total_num_added{};

  auto& eval_order = sys->growth_evaluation_order;
  const int num_insts = int(eval_order.order.size());

  if (num_insts == 0) {
    return result;
  }

  int ith_processed{};
  int num_permitted_new_nodes{};
  for (; ith_processed < num_insts; ith_processed++) {
    const int eval_index = (eval_order.next_instance + ith_processed) % num_insts;
    auto& inst = sys->instances.at(eval_order.order[eval_index].id);
    if (inst.state != TreeRootsState::Growing) {
      num_permitted_new_nodes++;
      continue;
    }

    inst.events.grew = true;
    auto grow_res = grow_instance(sys, inst, disable_node_creation, info);
    if (grow_res.finished_growing) {
      inst.state = TreeRootsState::Alive;
    }

#if ENABLE_BRANCH_INFOS
    push_new_branch_infos(sys, inst, grow_res.next_growing_ni_begin);
#endif

    total_num_added += grow_res.num_new_nodes;
    num_permitted_new_nodes += int(!disable_node_creation);
    result.num_new_branches += grow_res.num_new_branches;

    if (!growth_terminated_early && total_num_added >= max_num_add) {
      growth_terminated_early = true;
      disable_node_creation = true;
    }
  }

  eval_order.next_instance = (eval_order.next_instance + num_permitted_new_nodes) % num_insts;
  return result;
}

void update_dying(RootsSystem* sys, const UpdateInfo& info) {
  for (auto& [_, inst] : sys->instances) {
    if (inst.state != TreeRootsState::Dying) {
      continue;
    }

    inst.events.receded = true;
    auto recede_res = recede_instance(sys, inst, info);
    if (recede_res.finished_receding) {
      inst.state = TreeRootsState::Dead;
    }
  }
}

void update_pruning(RootsSystem* sys, const UpdateInfo& info) {
  for (auto& [_, inst] : sys->instances) {
    if (inst.state != TreeRootsState::Pruning) {
      continue;
    }

    inst.events.pruned = true;
    auto prune_res = prune_instance(sys, inst, info);
    if (prune_res.finished_pruning) {
      move_from_pruning_context_to_pruned_nodes(inst);
      inst.events.just_finished_pruning = true;
      inst.state = TreeRootsState::Alive;
    }
  }
}

bool can_read_roots(const RootsInstance& inst) {
  return inst.state != tree::TreeRootsState::PendingInit;
}

} //  anon

RootsInstanceHandle tree::create_roots_instance(RootsSystem* sys,
                                                const CreateRootsInstanceParams& params) {
  RootsInstanceHandle result{sys->next_instance_id++};
  sys->instances[result.id] = make_instance(params);
  sys->growth_evaluation_order.add_instance(result);
  return result;
}

ReadRootsInstance tree::read_roots_instance(const RootsSystem* sys, RootsInstanceHandle handle) {
  if (auto* inst = find_instance(sys, handle)) {
    ReadRootsInstance result{};
    result.events = inst->events;
    result.state = inst->state;
    if (can_read_roots(*inst)) {
      result.roots = &inst->roots;
    }
    return result;

  } else {
    assert(false);
    return {};
  }
}

int tree::collect_roots_instance_handles(const RootsSystem* sys, RootsInstanceHandle* dst,
                                         int max_dst) {
  int n{};
  for (auto& [id, _] : sys->instances) {
    if (n >= max_dst) {
      break;
    }
    dst[n++] = RootsInstanceHandle{id};
  }
  return n;
}

Optional<RootsInstanceHandle>
tree::lookup_roots_instance_by_radius_limiter_aggregate_id(const RootsSystem* sys,
                                                           bounds::RadiusLimiterAggregateID id) {
  for (auto& [handle_id, inst] : sys->instances) {
    if (inst.roots.id == id) {
      return Optional<RootsInstanceHandle>(RootsInstanceHandle{handle_id});
    }
  }
  return NullOpt{};
}

bool tree::can_start_dying(const RootsSystem* sys, RootsInstanceHandle handle) {
  if (auto* inst = find_instance(sys, handle)) {
    return inst->state == TreeRootsState::Alive && !inst->pending_state_changes.any();
  } else {
    assert(false);
    return false;
  }
}

void tree::start_dying(RootsSystem* sys, RootsInstanceHandle handle) {
  assert(can_start_dying(sys, handle));
  if (auto* inst = find_instance(sys, handle)) {
    inst->pending_state_changes.need_start_dying = true;
  } else {
    assert(false);
  }
}

bool tree::can_start_pruning(const RootsSystem* sys, RootsInstanceHandle handle) {
  if (auto* inst = find_instance(sys, handle)) {
    return inst->state == TreeRootsState::Alive && !inst->pending_state_changes.any();
  } else {
    assert(false);
    return false;
  }
}

void tree::start_pruning_roots(RootsSystem* sys, RootsInstanceHandle handle,
                               std::vector<int>&& pruned_dst_to_src,
                               std::vector<tree::TreeRootNodeIndices>&& pruned_node_indices) {
  assert(can_start_pruning(sys, handle));
  auto* inst = find_instance(sys, handle);
  assert(inst);
  assert(pruned_dst_to_src.size() == pruned_node_indices.size());
  assert(int(pruned_dst_to_src.size()) <= inst->roots.curr_num_nodes);
#ifdef GROVE_DEBUG
  for (const int ind : pruned_dst_to_src) {
    assert(ind < inst->roots.curr_num_nodes);
  }
#endif
  assert(!inst->pruning_context);
  inst->pruning_context = std::make_unique<PruningContext>();
  inst->pruning_context->skip_receding = std::unordered_set<int>{
    pruned_dst_to_src.begin(), pruned_dst_to_src.end()};
  inst->pruning_context->pruned_dst_to_src = std::move(pruned_dst_to_src);
  inst->pruning_context->pruned_node_indices = std::move(pruned_node_indices);
  inst->pending_state_changes.need_start_pruning = true;
}

bool tree::can_destroy_roots_instance(const RootsSystem* sys, RootsInstanceHandle handle) {
  if (auto* inst = find_instance(sys, handle)) {
    return inst->state == TreeRootsState::Dead && !inst->pending_state_changes.any();
  } else {
    assert(false);
    return false;
  }
}

void tree::destroy_roots_instance(RootsSystem* sys, RootsInstanceHandle handle) {
  assert(can_destroy_roots_instance(sys, handle));
  if (auto* inst = find_instance(sys, handle)) {
    inst->pending_state_changes.need_destroy = true;
  } else {
    assert(false);
  }
}

RootsSystem* tree::create_roots_system(bounds::RadiusLimiterElementTag roots_element_tag) {
  auto* res = new RootsSystem();
  res->roots_element_tag = roots_element_tag;
  return res;
}

tree::RootsSystemUpdateResult tree::update_roots_system(
  RootsSystem* sys, const RootsSystemUpdateInfo& info) {
  //
  RootsSystemUpdateResult result{};

  for (auto& [_, inst] : sys->instances) {
    inst.events = {};
  }

  sys->new_branch_infos.clear();

  for (auto& [_, inst] : sys->instances) {
    if (inst.state == TreeRootsState::PendingInit) {
      auto add_roots_params = to_add_roots_params(*sys, inst);
      init_instance(inst, add_roots_params, info.radius_limiter, sys->roots_element_tag);
      inst.state = TreeRootsState::Growing;

    } else if (inst.pending_state_changes.need_start_dying) {
      assert(inst.state == TreeRootsState::Alive);
      inst.pending_state_changes.need_start_dying = false;
      inst.need_init_recede_context = true;
      inst.state = TreeRootsState::Dying;

    } else if (inst.pending_state_changes.need_destroy) {
      assert(inst.state == TreeRootsState::Dead);
      inst.pending_state_changes.need_destroy = false;
      inst.state = TreeRootsState::WillDestroy;

    } else if (inst.pending_state_changes.need_start_pruning) {
      assert(inst.state == TreeRootsState::Alive);
      inst.pending_state_changes.need_start_pruning = false;
      inst.need_init_recede_context = true;
      inst.state = TreeRootsState::Pruning;
    }
  }

  auto grow_res = update_growing(sys, info);
  update_dying(sys, info);
  update_pruning(sys, info);

  result.num_new_branches = grow_res.num_new_branches;
  result.new_branch_infos = sys->new_branch_infos.data();
  result.num_new_branch_infos = int(sys->new_branch_infos.size());

#if ENABLE_BRANCH_INFOS
  assert(result.num_new_branch_infos == result.num_new_branches);
  sys->max_num_new_branch_infos = std::max(
    sys->max_num_new_branch_infos, result.num_new_branch_infos);
#endif

  return result;
}

void tree::end_update_roots_system(RootsSystem* sys) {
  auto it = sys->instances.begin();
  while (it != sys->instances.end()) {
    auto& inst = it->second;
    if (inst.state == TreeRootsState::WillDestroy) {
      //  Delete the instance
      assert(all_radius_limiter_elements_invalid(inst));
      sys->growth_evaluation_order.remove_instance(RootsInstanceHandle{it->first});
      it = sys->instances.erase(it);
    } else {
      ++it;
    }
  }
}

void tree::destroy_roots_system(RootsSystem** sys) {
  delete *sys;
  *sys = nullptr;
}

void tree::set_global_growth_rate_scale(RootsSystem* sys, float s) {
  sys->growth_rate_scale = std::max(0.0f, s);
}

void tree::set_global_attractor_point(RootsSystem* sys, const Vec3f& p) {
  sys->global_attractor_point = p;
}

void tree::set_global_attractor_point_scale(RootsSystem* sys, float s) {
  //  @NOTE can be negative
  sys->global_attractor_point_scale = s;
}

void tree::set_attenuate_growth_rate_by_spectral_fraction(RootsSystem* sys, bool atten) {
  sys->attenuate_growth_rate_by_spectral_fraction = atten;
}

void tree::set_spectral_fraction(RootsSystem* sys, float s) {
  sys->spectral_fraction = s;
}

void tree::set_global_p_spawn_lateral_branch(RootsSystem* sys, double p) {
  assert(p >= 0.0 && p <= 1.0);
  sys->global_p_spawn_lateral = p;
}

void tree::set_prefer_global_p_spawn_lateral_branch(RootsSystem* sys, bool pref) {
  sys->prefer_global_p_spawn_lateral = pref;
}

bounds::RadiusLimiterElementTag tree::get_roots_radius_limiter_element_tag(const RootsSystem* sys) {
  assert(sys->roots_element_tag.tag > 0);
  return sys->roots_element_tag;
}

RootsSystemStats tree::get_stats(const RootsSystem* sys) {
  RootsSystemStats result{};
  result.num_instances = int(sys->instances.size());
  for (auto& [_, inst] : sys->instances) {
    if (inst.state == TreeRootsState::Growing) {
      result.num_growing_instances++;
    }
  }
  result.max_num_new_branch_infos = sys->max_num_new_branch_infos;
  return result;
}

GROVE_NAMESPACE_END
