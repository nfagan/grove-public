#include "roots_growth.hpp"
#include "grove/math/random.hpp"
#include "grove/math/util.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/common.hpp"
#include "grove/common/DynamicArray.hpp"

#define CONSTANT_INITIAL_RADIUS (1)

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

constexpr float initial_radius_limiter_diameter() {
  return 0.25f * 2.0f;
}

float initial_radius_limiter_diameter(const TreeRootNode& node) {
#if CONSTANT_INITIAL_RADIUS
  (void) node;
  return initial_radius_limiter_diameter();
#else
  return node.target_diameter;
#endif
}

float axis_length(int node_index, const TreeRootNode* nodes) {
  auto* node = nodes + node_index;
  float res{node->target_length};
  while (node->has_parent()) {
    auto* parent = nodes + node->parent;
    if (parent->medial_child == node_index) {
      res += parent->target_length;
      node_index = node->parent;
      node = parent;
    } else {
      break;
    }
  }
  return res;
}

Vec3f randomly_offset(const Vec3f& curr, float rand_strength) {
  return normalize(curr + Vec3f{urand_11f(), urand_11f(), urand_11f()} * rand_strength);
}

Vec3f opposite_dense_direction(const bounds::RadiusLimiter* lim,
                               bounds::RadiusLimiterAggregateID aggregate,
                               const Vec3f& new_p, const Vec3f& new_dir) {
  int freqs[512];
  float filt[512];
  float tmp_filt[512];

  memset(freqs, 0, 512 * sizeof(int));
  auto hist_cell_counts = Vec3<int16_t>{8};
  const int16_t pow2_cell_size{};
  const float cell_size = std::pow(2.0f, float(pow2_cell_size));

  auto c_off = float(hist_cell_counts.x) * 0.5f * cell_size;
  auto orif = floor(new_p / cell_size) - c_off;
  auto ori = Vec3<int16_t>{int16_t(orif.x), int16_t(orif.y), int16_t(orif.z)};

  Vec3<int16_t> cell_size3{pow2_cell_size};
#if 1
  (void) aggregate;
  bounds::histogram(lim, ori, cell_size3, hist_cell_counts, 0, freqs);
#else
  bounds::histogram(lim, ori, cell_size3, hist_cell_counts, aggregate, axis, freqs);
#endif
  bounds::filter_histogram(freqs, hist_cell_counts, tmp_filt, filt);
  auto mean_dir = bounds::mean_gradient(filt, hist_cell_counts);

  auto dir_len = mean_dir.length();
  if (dir_len > 1e-2f) {
    mean_dir /= dir_len;
    return normalize(new_dir - mean_dir * 0.1f);
  } else {
    return new_dir;
  }
}

Vec3f towards_attraction_point(const Vec3f& dir, const Vec3f& node_p,
                               const Vec3f& attract_p, float attract_scale) {
  if (attract_scale == 0.0f) {
    return dir;
  } else {
    auto to_p = attract_p - node_p;
    auto to_p_len = to_p.length();
    if (to_p_len > 1e-2f) {
      return normalize((to_p / to_p_len) * attract_scale + dir);
    } else {
      return dir;
    }
  }
}

[[maybe_unused]] bool reached_targets(const TreeRootNode& node) {
  return node.length == node.target_length && node.diameter == node.target_diameter;
}

[[maybe_unused]] bool reached_targets(const TreeRootNode* nodes, int num_nodes) {
  for (int i = 0; i < num_nodes; i++) {
    auto* node = nodes + i;
    if (!reached_targets(*node)) {
      return false;
    }
  }
  return true;
}

void grow_length(TreeRootNode& node, float incr) {
  if (node.target_length < node.length) {
    node.length -= incr;
    node.length = std::max(node.length, node.target_length);

  } else if (node.target_length > node.length){
    node.length += incr;
    node.length = std::min(node.length, node.target_length);
  }
}

auto partition_finished(std::vector<GrowingTreeRootNode>& nodes) {
  auto not_finished = [](const GrowingTreeRootNode& node) { return !node.finished; };
  return std::partition(nodes.begin(), nodes.end(), not_finished);
}

int remove_finished(std::vector<GrowingTreeRootNode>& nodes) {
  nodes.erase(partition_finished(nodes), nodes.end());
  return int(nodes.size());
}

void push_next(std::vector<GrowingTreeRootNode>& nodes, const GrowingTreeRootNode* beg,
               const GrowingTreeRootNode* end) {
  nodes.insert(nodes.end(), beg, end);
}

auto grow(TreeRootsGrowthContext* ctx, bounds::RadiusLimiter* lim, TreeRootNode* nodes,
          bounds::RadiusLimiterAggregateID roots_id, bounds::RadiusLimiterElementTag roots_tag,
          bounds::RadiusLimiterElementHandle* radius_limiter_handles, int curr_num_nodes,
          int max_num_nodes, const GrowRootsParams& params) {
  struct Result {
    int new_num_nodes;
    int num_new_branches;
    int next_growing_ni_begin;
  };

  const auto incr = float(params.real_dt * params.growth_rate);
  assert(incr >= 0.0f);
  const float target_length = params.node_length_scale;

  Temporary<GrowingTreeRootNode, 1024> tmp_next_growing;
  auto next_growing = tmp_next_growing.view_stack();

  const auto& attractor_point = params.attractor_point;
  float attractor_point_scale = params.attractor_point_scale;

  int num_new_branches{};
  for (auto& growing : ctx->growing) {
    assert(!growing.finished);
    auto& node = nodes[growing.index];
    grow_length(node, incr);

    if (params.disable_node_creation) {
      continue;
    }

    const bool finished_growing = node.length == node.target_length;
    if (!finished_growing) {
      continue;
    } else {
      growing.finished = true;
    }

    if (curr_num_nodes < max_num_nodes) {
      assert(node.medial_child == -1);
      auto new_dir = randomly_offset(node.direction, 0.1f);
#if 0
      float up_scale{0.1f};
      if (axis_root_index(growing.index, nodes) == 0) {
        up_scale = 0.25f;
      }
      new_dir = normalize(new_dir + Vec3f{0.0f, 1.0f, 0.0f} * up_scale);
#endif
      auto new_p = node.position + node.direction * node.target_length;

      new_dir = opposite_dense_direction(lim, roots_id, new_p, new_dir);
      new_dir = towards_attraction_point(new_dir, new_p, attractor_point, attractor_point_scale);

      const float diam = initial_radius_limiter_diameter(node);
      auto query_obb = make_tree_root_node_obb(new_p, new_dir, node.target_length, diam);
      auto query_el = make_tree_root_node_radius_limiter_element(query_obb, roots_id, roots_tag);
      const bool reject = bounds::intersects_other(lim, query_el);

      if (!reject) {
        auto new_ind = curr_num_nodes++;
        node.medial_child = new_ind;
        nodes[new_ind] = copy_make_tree_root_node(
          node, growing.index, new_dir, new_p, target_length);
        *next_growing.push(1) = make_growing_tree_root_node(new_ind);

        assert(radius_limiter_handles[new_ind] == bounds::RadiusLimiterElementHandle::invalid());
        radius_limiter_handles[new_ind] = bounds::insert(lim, query_el);
      }
    }

    if (curr_num_nodes < max_num_nodes && urand() < params.p_spawn_lateral &&
        axis_length(growing.index, nodes) > params.min_axis_length_spawn_lateral) {
      assert(node.lateral_child == -1);
      const auto& p = node.position;

      auto new_dir = randomly_offset(node.direction, 0.5f);
//      new_dir.y = 0.0f;
      new_dir = opposite_dense_direction(lim, roots_id, p, new_dir);
      new_dir = towards_attraction_point(new_dir, p, attractor_point, attractor_point_scale);

      const float diam = initial_radius_limiter_diameter(node);
      auto query_obb = make_tree_root_node_obb(p, new_dir, node.target_length, diam);
      auto query_el = make_tree_root_node_radius_limiter_element(query_obb, roots_id, roots_tag);
      const bool reject = bounds::intersects_other(lim, query_el);

      if (!reject) {
        auto new_ind = curr_num_nodes++;
        node.lateral_child = new_ind;
        nodes[new_ind] = copy_make_tree_root_node(
          node, growing.index, new_dir, node.position, target_length);
        *next_growing.push(1) = make_growing_tree_root_node(new_ind);

        assert(radius_limiter_handles[new_ind] == bounds::RadiusLimiterElementHandle::invalid());
        radius_limiter_handles[new_ind] = bounds::insert(lim, query_el);
        num_new_branches++;
      }
    }
  }

#ifdef GROVE_DEBUG
  for (int i = 0; i < curr_num_nodes; i++) {
    auto* el = bounds::read_element(lim, radius_limiter_handles[i]);
    assert(el->aggregate_id == roots_id);
  }
#endif

  assert(!next_growing.heap && "Alloc required.");
  const int next_beg = remove_finished(ctx->growing);
  push_next(ctx->growing, next_growing.begin(), next_growing.end());

  Result result;
  result.new_num_nodes = curr_num_nodes;
  result.num_new_branches = num_new_branches;
  result.next_growing_ni_begin = next_beg;
  return result;
}

bool recede(TreeRootsRecedeContext* ctx, TreeRootNode* nodes, int num_nodes,
            TemporaryViewStack<int>& finished_receding, const GrowRootsParams& params) {
  const auto incr = float(params.real_dt * params.growth_rate);
  assert(incr >= 0.0f);

  Temporary<GrowingTreeRootNode, 1024> tmp_next_receding;
  auto next_receding = tmp_next_receding.view_stack();

  for (auto& receding : ctx->receding) {
    assert(!receding.finished);
    auto& node = nodes[receding.index];
    const bool skip_current = ctx->skip && ctx->skip->count(receding.index) > 0;

    if (!skip_current) {
      grow_length(node, incr);
      if (node.length != node.target_length) {
        continue;
      }
    }

    receding.finished = true;
    if (node.is_axis_root(receding.index, nodes)) {
      assert(ctx->num_pending_axis_roots > 0);
      ctx->num_pending_axis_roots--;
      if (ctx->num_pending_axis_roots == 0 && node.has_parent()) {
        assert(ctx->node_orders[receding.index] > 0);
        const auto next_order = ctx->node_orders[receding.index] - 1;
        for (int i = 0; i < num_nodes; i++) {
          if (nodes[i].is_axis_tip() && ctx->node_orders[i] == next_order) {
            *next_receding.push(1) = make_growing_tree_root_node(i);
            const bool skip_next = ctx->skip && ctx->skip->count(i) > 0;
            if (!skip_next) {
              nodes[i].target_length = 0.0f;
            }
            ctx->num_pending_axis_roots++;
          }
        }
      }
    } else {
      //  Not an axis root, so add the (medial) parent along the axis.
      assert(node.has_parent());
      *next_receding.push(1) = make_growing_tree_root_node(node.parent);
      const bool skip_next = ctx->skip && ctx->skip->count(node.parent) > 0;
      if (!skip_next) {
        nodes[node.parent].target_length = 0.0f;
      }
    }
  }

  assert(!next_receding.heap && "Alloc required.");
  auto rem_it = partition_finished(ctx->receding);
  auto num_finished = int(ctx->receding.end() - rem_it);
  auto* finished = finished_receding.push(num_finished);

  for (int i = 0; i < num_finished; i++) {
    finished[i] = (rem_it + i)->index;
  }

  ctx->receding.erase(rem_it, ctx->receding.end());
  push_next(ctx->receding, next_receding.begin(), next_receding.end());
  return !ctx->receding.empty();
}

float assign_diameter(TreeRootNode* nodes, TreeRootNode* node,
                      const AssignRootsDiameterParams& params) {
  auto leaf_diameter = [](const AssignRootsDiameterParams& params) -> float {
    return std::pow(params.leaf_diameter, params.diameter_power);
  };

  float md = leaf_diameter(params);
  float ld = md;
  if (node->has_medial_child()) {
    md = assign_diameter(nodes, nodes + node->medial_child, params);
  }
  if (node->has_lateral_child()) {
    ld = assign_diameter(nodes, nodes + node->lateral_child, params);
  }

  auto d = md + ld;
  const auto min_diam = float(std::pow(d, 1.0 / params.diameter_power));
  node->target_diameter = std::max(params.leaf_diameter, min_diam);
  assert(std::isfinite(node->target_diameter) && node->target_diameter >= 0.0f);
  return d;
}

template <typename Get, typename Set>
void smooth_float_property(TreeRootNode* nodes, int num_nodes, int adjacent_count,
                           const Get& get_value, const Set& set_value) {
  if (num_nodes == 0) {
    return;
  }

  constexpr int max_adjacent_count = 32;
  constexpr int max_count = max_adjacent_count * 2 + 1;
  adjacent_count = std::max(1, std::min(adjacent_count, max_adjacent_count));

  DynamicArray<int, 1024> pend_lat;
  pend_lat.push_back(0);
  while (!pend_lat.empty()) {
    int med_ind = pend_lat.back();
    pend_lat.pop_back();
    while (med_ind != -1) {
      auto* med_node = nodes + med_ind;
      float values[max_count];
      float weights[max_count];
      int value_count{};
      //  Traverse parents.
      int prev_count{};
      int parent_ind = med_node->parent;
      while (parent_ind != -1 && prev_count < adjacent_count) {
        values[value_count] = get_value(nodes + parent_ind);
        weights[value_count] = 1.0f - (float(prev_count) + 0.5f) / float(adjacent_count);
        prev_count++;
        value_count++;
        parent_ind = nodes[parent_ind].parent;
      }
      //  Add the value at the current node.
      values[value_count] = get_value(nodes + med_ind);
      weights[value_count] = 1.0f;
      value_count++;
      //  Traverse medial children.
      int next_count{};
      int next_ind = med_node->medial_child;
      while (next_ind != -1 && next_count < adjacent_count) {
        values[value_count] = get_value(nodes + next_ind);
        weights[value_count] = 1.0f - (float(next_count) + 0.5f) / float(adjacent_count);
        next_count++;
        value_count++;
        next_ind = nodes[next_ind].medial_child;
      }
      assert(value_count > 0 && value_count <= max_count);
      //  average
      float s{};
      float ws{};
      for (int i = 0; i < value_count; i++) {
        s += values[i] * weights[i];
        ws += weights[i];
      }
      s /= ws;
      set_value(nodes + med_ind, s);
      //  next medial child
      med_ind = med_node->medial_child;
      if (med_node->has_lateral_child()) {
        pend_lat.push_back(med_node->lateral_child);
      }
    }
  }
}

void constrain_lateral_child_diameter(TreeRootNode* nodes, int num_nodes) {
  for (int i = 0; i < num_nodes; i++) {
    auto& node = nodes[i];
    if (node.has_lateral_child()) {
      float max_diam = node.target_diameter;
      if (node.has_medial_child()) {
        auto& med = nodes[node.medial_child];
        max_diam = std::min(max_diam, med.target_diameter);
      }
      auto& lat = nodes[node.lateral_child];
      lat.target_diameter = std::min(lat.target_diameter, max_diam);
    }
  }
}

void smooth_diameter(TreeRootNode* nodes, int num_nodes) {
  auto get_value = [](TreeRootNode* node) { return node->target_diameter; };
  auto set_value = [](TreeRootNode* node, float val) { node->target_diameter = val; };
  smooth_float_property(nodes, num_nodes, 5, get_value, set_value);
}

bool grow_diameter(TreeRootNode* nodes, int curr_num_nodes, double real_dt) {
  const double lerp_t = 1.0 - std::pow(0.5, real_dt);
  const float eps = 1e-3f;

  bool any_modified{};
  for (int i = 0; i < curr_num_nodes; i++) {
    auto& node = nodes[i];
    if (node.diameter != node.target_diameter) {
      node.diameter = lerp(float(lerp_t), node.diameter, node.target_diameter);
      if (std::abs(node.target_diameter - node.diameter) < eps) {
        node.diameter = node.target_diameter;
      }
      any_modified = true;
    }
  }

  return any_modified;
}

void expand_diameter(bounds::RadiusLimiter* lim, TreeRootNode* nodes,
                     const bounds::RadiusLimiterElementHandle* elements, int num_nodes) {
  for (int i = 0; i < num_nodes; i++) {
    auto& node = nodes[i];
    node.target_diameter = 2.0f * bounds::expand(lim, elements[i], node.target_radius());
  }
}

[[maybe_unused]] uint16_t gravelius_order(int ni, const TreeRootNode* nodes) {
  uint16_t res{};
  while (nodes[ni].has_parent()) {
    res += uint16_t(nodes[ni].is_axis_root(ni, nodes));
    ni = nodes[ni].parent;
  }
  return res;
}

std::vector<uint16_t> gravelius_order(const TreeRootNode* nodes, int num_nodes, uint16_t* max_ord) {
  struct Entry {
    int node_index;
    uint16_t order;
  };

  if (num_nodes == 0) {
    return {};
  }

  std::vector<uint16_t> result(num_nodes);

  std::vector<Entry> pend_entries;
  pend_entries.push_back({0, 0});
  uint16_t max_order{};

  while (!pend_entries.empty()) {
    auto entry = pend_entries.back();
    pend_entries.pop_back();
    int ni = entry.node_index;
    while (ni != -1) {
      assert(result[ni] == 0);
      result[ni] = entry.order;
      auto& node = nodes[ni];
      if (node.has_lateral_child()) {
        const auto next_order = uint16_t(entry.order + 1);
        pend_entries.push_back({node.lateral_child, next_order});
        max_order = std::max(max_order, next_order);
      }
      ni = node.medial_child;
    }
  }

  *max_ord = max_order;
  return result;
}

} //  anon

GrowRootsResult tree::grow_roots(TreeRoots* roots, bounds::RadiusLimiter* lim,
                                 bounds::RadiusLimiterElementHandle* elements,
                                 bounds::RadiusLimiterElementTag roots_tag,
                                 TreeRootsGrowthContext& growth_context,
                                 const GrowRootsParams& grow_params,
                                 const AssignRootsDiameterParams& diameter_params) {
  const int curr_num_nodes = roots->curr_num_nodes;

  auto grow_res = grow(
    &growth_context, lim, roots->nodes.data(), roots->id,
    roots_tag, elements, roots->curr_num_nodes, roots->max_num_nodes, grow_params);
  roots->curr_num_nodes = grow_res.new_num_nodes;

  int num_nodes_added{};
  if (roots->curr_num_nodes > curr_num_nodes) {
    grove::assign_diameter(roots->nodes.data(), roots->nodes.data(), diameter_params);
    expand_diameter(lim, roots->nodes.data(), elements, roots->curr_num_nodes);
    smooth_diameter(roots->nodes.data(), roots->curr_num_nodes);
    constrain_lateral_child_diameter(roots->nodes.data(), roots->curr_num_nodes);
    num_nodes_added = roots->curr_num_nodes - curr_num_nodes;
  }

  bool any_changed = grow_diameter(roots->nodes.data(), roots->curr_num_nodes, grow_params.real_dt);
  const bool finished = growth_context.growing.empty() && !any_changed &&
    !grow_params.disable_node_creation;
#ifdef GROVE_DEBUG
  if (finished) {
    assert(reached_targets(roots->nodes.data(), roots->curr_num_nodes));
  }
#endif

  GrowRootsResult result{};
  result.finished = finished;
  result.num_nodes_added = num_nodes_added;
  result.num_new_branches = grow_res.num_new_branches;
  result.next_growing_ni_begin = grow_res.next_growing_ni_begin;
  return result;
}

RecedeRootsResult tree::recede_roots(TreeRoots* roots, bounds::RadiusLimiter* lim,
                                     bounds::RadiusLimiterElementHandle* bounds_elements,
                                     TreeRootsRecedeContext& recede_context,
                                     const GrowRootsParams& params) {
  Temporary<int, 1024> finished_receding;
  auto finished_view = finished_receding.view_stack();
  bool any_receded = recede(
    &recede_context, roots->nodes.data(), roots->curr_num_nodes, finished_view, params);
  const bool all_finished = !any_receded;
#ifdef GROVE_DEBUG
  if (all_finished) {
    assert(reached_targets(roots->nodes.data(), roots->curr_num_nodes));
  }
#endif

  for (int finished_ind : finished_view) {
    bounds::remove(lim, bounds_elements[finished_ind]);
    bounds_elements[finished_ind] = bounds::RadiusLimiterElementHandle::invalid();
  }

  RecedeRootsResult result{};
  result.finished = all_finished;
  return result;
}

PruneRootsResult tree::prune_roots(TreeRoots* roots, bounds::RadiusLimiter* lim,
                                   bounds::RadiusLimiterElementHandle* bounds_elements,
                                   TreeRootsRecedeContext& recede_context,
                                   const GrowRootsParams& params) {
  assert(recede_context.skip);
  Temporary<int, 1024> finished_receding;
  auto finished_view = finished_receding.view_stack();
  bool any_receded = recede(
    &recede_context, roots->nodes.data(), roots->curr_num_nodes, finished_view, params);
  const bool all_finished = !any_receded;
#ifdef GROVE_DEBUG
  if (all_finished) {
    assert(reached_targets(roots->nodes.data(), roots->curr_num_nodes));
  }
#endif

  assert(recede_context.skip);
  for (int finished_ind : finished_view) {
    if (recede_context.skip->count(finished_ind) == 0) {
      bounds::remove(lim, bounds_elements[finished_ind]);
      bounds_elements[finished_ind] = bounds::RadiusLimiterElementHandle::invalid();
    }
  }

  PruneRootsResult result{};
  result.finished = all_finished;
  return result;
}

void tree::assign_diameter(TreeRootNode* root, const AssignRootsDiameterParams& params) {
  (void) grove::assign_diameter(root, root, params);
}

void tree::init_roots_recede_context(TreeRootsRecedeContext* context,
                                     TreeRootNode* nodes, int num_nodes,
                                     const TreeRootsSkipReceding* skip) {
  *context = {};
  context->skip = skip;

  if (num_nodes == 0) {
    return;
  }

  uint16_t max_order{};
  context->node_orders = gravelius_order(nodes, num_nodes, &max_order);

  for (int i = 0; i < num_nodes; i++) {
    if (nodes[i].is_axis_tip() && context->node_orders[i] == max_order) {
      if (!skip || !skip->count(i)) {
        nodes[i].target_length = 0.0f;
      }
      context->receding.push_back(make_growing_tree_root_node(i));
      context->num_pending_axis_roots++;
    }
  }
}

GROVE_NAMESPACE_END
