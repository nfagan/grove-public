#include "render.hpp"
#include "utility.hpp"
#include "bud_fate.hpp"
#include "grove/math/frame.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/pack.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

OBB3f make_obb(const tree::Internode& internode, float diameter) {
  auto half_size_xz = diameter * 0.5f;
  auto half_size_y = internode.length * 0.5f;
  auto position = internode.position + internode.direction * half_size_y;

  OBB3f res{};
  make_coordinate_system_y(internode.direction, &res.i, &res.j, &res.k);
  res.position = position;
  res.half_size = Vec3f{half_size_xz, half_size_y, half_size_xz};
  return res;
}

void set_render_position_new_method(tree::Internodes& internodes,
                                    tree::TreeNodeIndex axis_root_index) {
  assert(axis_root_index < int(internodes.size()));
  auto& root = internodes[axis_root_index];
  root.render_position = root.position;

  std::vector<tree::TreeNodeIndex> parent_info;
  parent_info.push_back(axis_root_index);

  while (!parent_info.empty()) {
    const tree::TreeNodeIndex parent_index = parent_info.back();
    parent_info.pop_back();

    auto& parent = internodes[parent_index];
    if (parent.has_lateral_child()) {
      auto& child = internodes[parent.lateral_child];
      child.render_position = lerp(parent.length_scale, parent.position, child.position);
      parent_info.push_back(parent.lateral_child);
    }
    if (parent.has_medial_child()) {
      auto& child = internodes[parent.medial_child];
      child.render_position = lerp(parent.length_scale, parent.position, child.position);
      parent_info.push_back(parent.medial_child);
    }
  }
}

void apply_render_growth_change(tree::Internodes& internodes,
                                const tree::SpawnInternodeParams& spawn_params,
                                tree::TreeNodeIndex root_axis_index) {
  //  @TODO: We need to use the new method eventually. The old method allows render
  //   positions of fully grown internodes to differ from their canonical positions.
#if 1
  tree::set_render_position(internodes, root_axis_index);
#else
  set_render_position_new_method(internodes, root_axis_index);
#endif
  auto spawn_p = spawn_params;
  spawn_p.attenuate_diameter_by_length_scale = true;
  tree::set_diameter(internodes, spawn_p, root_axis_index);
}

void apply_render_growth_change_new_method(tree::Internodes& internodes,
                                           tree::TreeNodeIndex root_axis_index) {
  set_render_position_new_method(internodes, root_axis_index);
}

bool tick_render_axis_growth_depth_first(Internodes& internodes, RenderAxisGrowthContext& context,
                                         float growth_incr, bool* new_axis) {
  *new_axis = false;
  if (!context.depth_first_growing) {
    return false;
  }

  auto& node = internodes[context.depth_first_growing.value()];
  node.length_scale += growth_incr;

  if (node.length_scale >= 1.0f) {
    node.length_scale = 1.0f;
    //  @NOTE: Check for the lateral child first, so that if the axis has no medial child but
    //  does have a lateral child, the lateral child will become the new growing axis.
    if (node.has_lateral_child()) {
      context.growing.push_back(node.lateral_child);
    }
    if (node.has_medial_child()) {
      context.depth_first_growing = node.medial_child;

    } else if (!context.growing.empty()) {
      context.depth_first_growing = context.growing.front();
      context.growing.erase(context.growing.begin());
      *new_axis = true;

    } else {
      context.depth_first_growing = NullOpt{};
    }
  }

  return true;
}

} //  anon

void tree::copy_diameter_to_lateral_q(Internodes& inodes) {
  for (auto& node : inodes) {
    node.lateral_q = node.diameter;
  }
}

void tree::copy_position_to_render_position(Internodes& inodes) {
  for (auto& node : inodes) {
    node.render_position = node.position;
  }
}

void tree::mul_lateral_q_diameter_by_length_scale(Internodes& inodes) {
  for (auto& node : inodes) {
    node.diameter = node.lateral_q * node.length_scale;
#ifdef GROVE_DEBUG
    if (node.length_scale == 1.0f) {
      assert(node.diameter == node.lateral_q && "First copy diameter to lateral q.");
    }
#endif
  }
}

tree::RemappedAxisRoots tree::remap_axis_roots(const tree::Internodes& internodes) {
  RemappedAxisRoots result;

  for (int i = 0; i < int(internodes.size()); i++) {
    auto& node = internodes[i];
    if (!node.has_parent()) {
      //  Root node is also an axis root.
      RemappedAxisRoot axis_info{};
      axis_info.position = node.position;
//      axis_info.gravelius_order = node.gravelius_order;
      result.root_info[node.id] = axis_info;

    } else if (internodes[node.parent].lateral_child == i) {
      //  This node is the lateral child of its parent, hence it's an axis root.
      auto& parent = internodes[node.parent];
      auto obb_parent = make_obb(parent, parent.diameter);

      auto self_ind = i;
      RemappedAxisRoot axis_info{};
//      axis_info.gravelius_order = node.gravelius_order;
//      axis_info.found_non_intersecting_child = true;
      DynamicArray<tree::TreeInternodeID, 16> maybe_remap;

      while (self_ind >= 0) {
        auto& self_node = internodes[self_ind];
        axis_info.position = self_node.position;
        auto obb_self = make_obb(self_node, self_node.diameter);

        if (obb_obb_intersect(obb_self, obb_parent)) {
          maybe_remap.push_back(self_node.id);
//          axis_info.root_offset++;
          self_ind = internodes[self_ind].medial_child;
        } else {
          break;
        }
      }

      if (self_ind < 0) {
        //  All nodes along the child axis intersect with the parent, so do nothing.
//        axis_info.root_offset = 0;
//        axis_info.found_non_intersecting_child = false;
      } else {
        //  For each node along the child axis that intersects with the parent, pretend that it
        //  belongs to the parent axis.
        for (auto& remap : maybe_remap) {
          assert(result.evaluate_at.count(remap) == 0);
          result.evaluate_at[remap] = parent.id;
        }
      }

      result.root_info[node.id] = axis_info;
    }
  }

  //  Any remaining nodes map to themselves.
  for (auto& node : internodes) {
    if (result.evaluate_at.count(node.id) == 0) {
      result.evaluate_at[node.id] = node.id;
    }
  }

  return result;
}

tree::PackedWindAxisRootInfo tree::to_packed_wind_info(const WindAxisRootInfo& parent,
                                                       const WindAxisRootInfo& child) {
  assert(parent.info.size() == child.info.size());
  DynamicArray<Vec4<uint32_t>, 3> result;

  for (int i = 0; i < parent.info.size(); i++) {
    Vec4<uint32_t> packed{};
    for (int j = 0; j < 4; j++) {
      packed[j] = pack::pack_2fn_1u32(parent.info[i][j], child.info[i][j]);
    }
    result.push_back(packed);
  }

  return result;
}

WindAxisRootInfo tree::WindAxisRootInfo::missing(int n) {
  WindAxisRootInfo result{};
  for (int i = 0; i < n; i++) {
    result.info.push_back(Vec4f{});
  }
  return result;
}

tree::WindAxisRootInfo tree::make_wind_axis_root_info(const tree::Internode& internode,
                                                      const tree::Internodes& store,
                                                      const tree::AxisRootInfo& axis_root_info,
                                                      const RemappedAxisRoots& remapped_roots,
                                                      const Bounds3f& tree_aabb) {
  assert(axis_root_info.size() == store.size());
  WindAxisRootInfo result{};
  //  axis root0
  auto id = internode.id;
  Vec4f info_l0{};
  Vec4f info_l1{};
  Vec4f info_l2{};

  while (true) {
    auto eval_id = id;
    while (true) {
      auto next_id = remapped_roots.evaluate_at.at(eval_id);
      if (next_id == eval_id) {
        break;
      } else {
        eval_id = next_id;
      }
    }

    auto& root_info = axis_root_info.at(eval_id);
    auto axis_root_ind = root_info.axis_root_index;
    auto* axis_root_node = &store[axis_root_ind];
    const auto& axis_info = remapped_roots.root_info.at(axis_root_node->id);

    info_l2 = info_l1;
    info_l1 = info_l0;

    info_l0.w = 1.0f; //  active
    auto pos01 = tree_aabb.to_fraction(axis_info.position);
    for (int i = 0; i < 3; i++) {
      assert(std::isfinite(pos01[i]) && pos01[i] >= 0.0f && pos01[i] <= 1.0f);
      info_l0[i] = pos01[i];
    }

    if (axis_root_node->has_parent()) {
      id = store[axis_root_node->parent].id;
    } else {
      break;
    }
  }

  result.info.push_back(info_l0);
  result.info.push_back(info_l1);
  result.info.push_back(info_l2);
  return result;
}

Bounds3f tree::internode_aabb(const tree::Internode* nodes, uint32_t num_nodes) {
  Bounds3f res;
  for (uint32_t ni = 0; ni < num_nodes; ni++) {
    auto& node = nodes[ni];
    for (int i = 0; i < 3; i++) {
      if (node.position[i] > res.max[i]) {
        res.max[i] = node.position[i];
      }
      if (node.position[i] < res.min[i]) {
        res.min[i] = node.position[i];
      }
    }
  }
  return res;
}

Bounds3f tree::internode_aabb(const tree::Internodes& nodes) {
  return internode_aabb(nodes.data(), uint32_t(nodes.size()));
}

OBB3f tree::internode_obb(const tree::Internode& node) {
  return make_obb(node, node.diameter);
}

void tree::internode_obbs(const tree::Internode* nodes, int num_nodes, OBB3f* dst) {
  for (int i = 0; i < num_nodes; i++) {
    dst[i] = internode_obb(nodes[i]);
  }
}

OBB3f tree::internode_obb_custom_diameter(const tree::Internode& node, float diameter) {
  return make_obb(node, diameter);
}

OBB3f tree::internode_relative_obb(const Internode& inode, const Vec3f& scale, const Vec3f& off) {
  OBB3f node_obb;
  orient_to_internode_direction(&node_obb, inode);
  node_obb.half_size = scale;
  node_obb.position = orient(node_obb, off) + inode.position;
  return node_obb;
}

void tree::orient_to_internode_direction(OBB3f* dst, const tree::Internode& inode) {
  make_coordinate_system_y(inode.direction, &dst->i, &dst->j, &dst->k);
}

tree::ChildRenderData tree::get_child_render_data(const tree::Internode& internode,
                                                  const tree::Internode* store,
                                                  bool allow_branch_to_lateral_child,
                                                  float leaf_tip_radius) {
  //  Get the components of the larger (by radius) of the two children. If the node
  //  is a leaf node with no children, just return the parent's info.
  auto r0 = internode.radius();
  auto dir0 = internode.spherical_direction();
  auto p0 = internode.render_position;

  float r_lat{-1.0f};
  Vec2f dir_lat{};
  Vec3f p_lat{};
  const tree::Internode* i_lat{nullptr};

  float r_med{-1.0f};
  Vec2f dir_med{};
  Vec3f p_med{};
  const tree::Internode* i_med{nullptr};

  if (internode.has_lateral_child()) {
    auto& lat = store[internode.lateral_child];
    r_lat = lat.radius();
    dir_lat = lat.spherical_direction();
    p_lat = lat.render_position;
    i_lat = &lat;
  }
  if (internode.has_medial_child()) {
    auto& med = store[internode.medial_child];
    r_med = med.radius();
    dir_med = med.spherical_direction();
    p_med = med.render_position;
    i_med = &med;
  }

  float r1 = r_med;
  auto dir1 = dir_med;
  auto p1 = p_med;
  auto i1 = i_med;

  if (allow_branch_to_lateral_child && r_lat > r1) {
    r1 = r_lat;
    dir1 = dir_lat;
    p1 = p_lat;
    i1 = i_lat;
  }

  if (r1 < 0.0f) {
    r1 = leaf_tip_radius;
    dir1 = dir0;
    p1 = p0 + internode.direction * internode.length * internode.length_scale;
    i1 = &internode;
  }

//  assert(r1 <= r0);
  (void) r0;
  return {i1, p1, dir1, r1};
}

void tree::constrain_lateral_child_diameter(tree::Internodes& inodes) {
  for (auto& node : inodes) {
    if (node.has_lateral_child()) {
      auto& child = inodes[node.lateral_child];
      child.diameter = std::min(child.diameter, node.diameter);
    }
  }
}

void tree::prefer_larger_axes(Internode* nodes, int num_nodes) {
  for (int i = 0; i < num_nodes; i++) {
    auto& node = nodes[i];
    if (node.has_lateral_child() && !node.has_medial_child()) {
      std::swap(node.lateral_child, node.medial_child);

    } else if (node.has_lateral_child() && node.has_medial_child()) {
      if (nodes[node.medial_child].diameter < nodes[node.lateral_child].diameter) {
        std::swap(node.lateral_child, node.medial_child);
      }
    }
  }
}

void tree::set_render_length_scale(Internodes& internodes, TreeNodeIndex root_index, float scl) {
  auto axis_func = [&internodes, scl](auto ind) {
    internodes[ind].length_scale = scl;
  };
  tree::map_axis(axis_func, internodes, root_index);
  tree::set_render_position(internodes, root_index);
}

void tree::set_render_position(Internodes& internodes, TreeNodeIndex axis_root_index) {
  struct ParentInfo {
    TreeNodeIndex parent_index;
    Vec3f position;
    float length;
  };

  if (internodes.empty()) {
    return;
  }

#if 0 //  12/31/22
  assert(axis_root_index < int(internodes.size()));
  auto& root = internodes[axis_root_index];
  root.render_position = root.position;

  Temporary<ParentInfo, 2048> store_parent_info;
  ParentInfo* parent_info = store_parent_info.require(int(internodes.size()));

  int si{};
  parent_info[si++] = {axis_root_index, root.position, root.length * root.length_scale};

  while (si > 0) {
    auto info = parent_info[--si];
    auto& parent = internodes[info.parent_index];
    auto child_pos = info.position + parent.direction * info.length;

    if (parent.has_lateral_child()) {
      auto& child = internodes[parent.lateral_child];
      child.render_position = child_pos;
      auto len = child.length * child.length_scale;
      parent_info[si++] = {parent.lateral_child, child_pos, len};
    }

    if (parent.has_medial_child()) {
      auto& child = internodes[parent.medial_child];
      child.render_position = child_pos;
      auto len = child.length * child.length_scale;
      parent_info[si++] = {parent.medial_child, child_pos, len};
    }
  }
#else
  assert(axis_root_index < int(internodes.size()));
  auto& root = internodes[axis_root_index];
  root.render_position = root.position;

  std::vector<ParentInfo> parent_info;
  parent_info.push_back({
    axis_root_index, root.position, root.length * root.length_scale
  });

  const auto push_child_info = [&parent_info, &internodes](
    TreeNodeIndex child_ind, const Vec3f& child_pos) {
    //
    auto& child = internodes[child_ind];
    child.render_position = child_pos;
    auto len = child.length * child.length_scale;
    parent_info.push_back({child_ind, child_pos, len});
  };

  while (!parent_info.empty()) {
    auto info = parent_info.back();
    parent_info.pop_back();

    auto& parent = internodes[info.parent_index];
    auto child_pos = info.position + parent.direction * info.length;

    if (parent.has_lateral_child()) {
      push_child_info(parent.lateral_child, child_pos);
    }
    if (parent.has_medial_child()) {
      push_child_info(parent.medial_child, child_pos);
    }
  }
#endif
}

bool tree::tick_render_axis_growth(Internodes& internodes, RenderAxisGrowthContext& context,
                                   float growth_incr) {
  bool any_grew = !context.growing.empty();
  std::vector<TreeNodeIndex> still_growing;

  for (auto& growing : context.growing) {
    auto& node = internodes[growing];
    node.length_scale += growth_incr;

    if (node.length_scale >= 1.0f) {
      node.length_scale = 1.0f;
      if (node.has_lateral_child()) {
        still_growing.push_back(node.lateral_child);
      }
      if (node.has_medial_child()) {
        still_growing.push_back(node.medial_child);
      }
    } else {
      still_growing.push_back(growing);
    }
  }

  context.growing = std::move(still_growing);
  return any_grew;
}

bool tree::tick_render_axis_death(Internodes& internodes, RenderAxisDeathContext& context,
                                  float growth_incr) {
  assert(growth_incr >= 0.0f);
  bool any_changed = !context.dying.empty();
  std::vector<TreeNodeIndex> still_dying;

  for (auto& dying : context.dying) {
    auto& node = internodes[dying];
    const bool preserved = context.preserve.count(dying) > 0;
    if (!preserved) {
      node.length_scale -= growth_incr;
      if (node.length_scale > 0.0f) {
        still_dying.push_back(dying);
        continue;
      }
      //  Finished dying.
      node.length_scale = 0.0f;
    }
    if (node.is_axis_root(internodes)) {
      context.num_pending_axis_roots--;
      if (context.num_pending_axis_roots == 0 && node.has_parent()) {
        //  Add tip of axis.
        assert(node.gravelius_order > 0);
        const uint16_t next_order = node.gravelius_order - 1;

        TreeNodeIndex inode_ind{};
        for (auto& inode : internodes) {
          if (inode.is_axis_root(internodes) && inode.gravelius_order == next_order) {
            still_dying.push_back(axis_tip_index(internodes, inode_ind));
            context.num_pending_axis_roots++;
          }
          inode_ind++;
        }
      }
    } else {
      //  Not an axis root, so add the (medial) parent along the axis.
      assert(node.has_parent());
      still_dying.push_back(node.parent);
    }
  }

  context.dying = std::move(still_dying);
  return any_changed;
}

tree::RenderAxisDeathContext
tree::make_default_render_axis_death_context(const Internodes& internodes) {
  const int max_grav_order = tree::max_gravelius_order(internodes);
  assert(internodes.empty() || max_grav_order >= 0);

  std::vector<tree::TreeNodeIndex> leaf_indices;
  tree::TreeNodeIndex leaf_ind{};
  for (auto& node : internodes) {
    if (node.is_axis_root(internodes) && node.gravelius_order == max_grav_order) {
      leaf_indices.push_back(tree::axis_tip_index(internodes, leaf_ind));
    }
    leaf_ind++;
  }

  tree::RenderAxisDeathContext context;
  context.num_pending_axis_roots = int(leaf_indices.size());
  context.dying = std::move(leaf_indices);
  return context;
}

void tree::initialize_axis_pruning(tree::RenderAxisDeathContext* context,
                                   const Internodes& internodes,
                                   std::unordered_set<int>&& preserve) {
  *context = make_default_render_axis_death_context(internodes);
  context->preserve = std::move(preserve);
}

void tree::initialize_depth_first_axis_render_growth_context(RenderAxisGrowthContext* context,
                                                             const Internodes& internodes,
                                                             TreeNodeIndex root_index) {
  context->clear();
  context->root_axis_index = root_index;
  if (!internodes.empty()) {
    context->depth_first_growing = root_index;
  }
}

void tree::initialize_axis_render_growth_context(RenderAxisGrowthContext* context,
                                                 const Internodes& internodes,
                                                 TreeNodeIndex root_index) {
  context->clear();
  context->root_axis_index = root_index;
  if (!internodes.empty()) {
    context->growing.push_back(root_index);
  }
}

bool tree::update_render_growth(Internodes& internodes,
                                const SpawnInternodeParams& spawn_params,
                                RenderAxisGrowthContext& growth_context, float incr) {
  if (tick_render_axis_growth(internodes, growth_context, incr)) {
    apply_render_growth_change(internodes, spawn_params, growth_context.root_axis_index);
    return true;
  } else {
    return false;
  }
}

bool tree::update_render_growth_depth_first(Internodes& internodes,
                                            RenderAxisGrowthContext& growth_context, float incr,
                                            bool* new_axis) {
  if (tick_render_axis_growth_depth_first(internodes, growth_context, incr, new_axis)) {
    apply_render_growth_change_new_method(internodes, growth_context.root_axis_index);
    return true;
  } else {
    return false;
  }
}

bool tree::update_render_growth_new_method(Internodes& internodes,
                                           RenderAxisGrowthContext& growth_context,
                                           float incr) {
  if (tick_render_axis_growth(internodes, growth_context, incr)) {
    apply_render_growth_change_new_method(internodes, growth_context.root_axis_index);
    return true;
  } else {
    return false;
  }
}

bool tree::update_render_death(Internodes& internodes,
                               const tree::SpawnInternodeParams& spawn_params,
                               RenderAxisDeathContext& death_context, float incr) {
  if (tick_render_axis_death(internodes, death_context, incr)) {
    apply_render_growth_change(internodes, spawn_params, 0);
    return true;
  } else {
    return false;
  }
}

bool tree::update_render_growth_src_diameter_in_lateral_q(
  Internodes& internodes, RenderAxisGrowthContext& context,
  const SpawnInternodeParams& spawn_params, float incr) {
  //
  assert(incr >= 0.0f);
  (void) spawn_params;

  Temporary<int, 1024> store_growing;
  auto next_growing = store_growing.view_stack();

  const auto num_growing = int(context.growing.size());
  bool any_grew = num_growing > 0;

  for (int i = 0; i < num_growing; i++) {
    auto& ni = context.growing[i];
    auto& node = internodes[ni];

    const float len_scale = node.length_scale + incr;
    const bool node_finished = len_scale >= 1.0f;

    node.length_scale = clamp01(len_scale);
    node.diameter = node.length_scale * node.lateral_q;

    if (node.has_medial_child()) {
      auto& child = internodes[node.medial_child];
      child.render_position = lerp(node.length_scale, node.position, child.position);
    }

    if (node_finished) {
      ni = -1;
      if (node.has_lateral_child()) {
        *next_growing.push(1) = node.lateral_child;
      }
      if (node.has_medial_child()) {
        *next_growing.push(1) = node.medial_child;
      }
    }
  }

  const auto not_done = [](int ind) { return ind >= 0; };
  auto grow_end = std::partition(context.growing.begin(), context.growing.end(), not_done);
  context.growing.erase(grow_end, context.growing.end());

  const auto new_end = int(context.growing.size());
  context.growing.resize(new_end + next_growing.size);
  if (next_growing.size > 0) {
    std::copy(next_growing.begin(), next_growing.end(), context.growing.begin() + new_end);
  }

  return any_grew;
}

bool tree::update_render_death_src_diameter_in_lateral_q(
  Internodes& internodes, RenderAxisDeathContext& context, float incr) {
  //
  assert(incr >= 0.0f);
  bool any_changed = !context.dying.empty();

  Temporary<TreeNodeIndex, 2048> store_still_dying;
  TreeNodeIndex* still_dying = store_still_dying.require(int(internodes.size()));
  int num_still_dying{};

  for (auto& dying : context.dying) {
    auto& node = internodes[dying];
    const bool preserved = context.preserve.count(dying) > 0;
    if (!preserved) {
      node.length_scale -= incr;
      node.diameter = node.lateral_q * node.length_scale; //  @NOTE
      if (node.length_scale > 0.0f) {
        still_dying[num_still_dying++] = dying;
        continue;
      }
      //  Finished dying.
      node.length_scale = 0.0f;
      node.diameter = 0.0f;
    }
    if (node.is_axis_root(internodes)) {
      context.num_pending_axis_roots--;
      if (context.num_pending_axis_roots == 0 && node.has_parent()) {
        //  Add tip of axis.
        assert(node.gravelius_order > 0);
        const uint16_t next_order = node.gravelius_order - 1;

        TreeNodeIndex inode_ind{};
        for (auto& inode : internodes) {
          if (inode.is_axis_root(internodes) && inode.gravelius_order == next_order) {
            still_dying[num_still_dying++] = axis_tip_index(internodes, inode_ind);
            context.num_pending_axis_roots++;
          }
          inode_ind++;
        }
      }
    } else {
      //  Not an axis root, so add the (medial) parent along the axis.
      assert(node.has_parent());
      still_dying[num_still_dying++] = node.parent;
    }
  }

  context.dying.resize(num_still_dying);
  std::copy(still_dying, still_dying + num_still_dying, context.dying.begin());
  return any_changed;
}

bool tree::update_render_death_new_method(Internodes& tree_nodes,
                                          RenderAxisDeathContext& death_context, float incr) {
  if (tick_render_axis_death(tree_nodes, death_context, incr)) {
    apply_render_growth_change_new_method(tree_nodes, 0);
    return true;
  } else {
    return false;
  }
}

bool tree::update_render_prune(Internodes& internodes,
                               RenderAxisDeathContext& death_context, float incr) {
  if (tree::tick_render_axis_death(internodes, death_context, incr)) {
    tree::set_render_position(internodes, 0);
    tree::mul_lateral_q_diameter_by_length_scale(internodes);
    return true;
  } else {
    return false;
  }
}

void tree::compute_internode_frames(const tree::Internode* nodes, int num_nodes, Mat3f* dst) {
  if (num_nodes == 0) {
    return;
  }

  Mat3f root{};
  make_coordinate_system_y(nodes[0].direction, &root[0], &root[1], &root[2]);
  dst[0] = root;

  for (int i = 0; i < num_nodes; i++) {
    auto& self_frame = dst[i];
    auto& self_node = nodes[i];

    const Internode* child_node{};
    if (self_node.has_medial_child()) {
      child_node = nodes + self_node.medial_child;
      if (self_node.has_lateral_child()) {
        //  Start of new axis.
        auto& root_frame = dst[self_node.lateral_child];
        root_frame[1] = nodes[self_node.lateral_child].direction;
        make_coordinate_system_y(root_frame[1], &root_frame[0], &root_frame[1], &root_frame[2]);
      }
    } else if (self_node.has_lateral_child()) {
      child_node = nodes + self_node.lateral_child;
    } else {
      continue;
    }

    auto& child_frame = dst[int(child_node - nodes)];
    child_frame[1] = child_node->direction;
    if (std::abs(dot(child_frame[1], self_frame[2])) > 0.99f) {
      make_coordinate_system_y(child_frame[1], &child_frame[0], &child_frame[1], &child_frame[2]);
    } else {
      child_frame[0] = normalize(cross(child_frame[1], self_frame[2]));
      if (dot(child_frame[0], self_frame[0]) < 0.0f)  {
        child_frame[0] = -child_frame[0];
      }
      child_frame[2] = cross(child_frame[0], child_frame[1]);
      if (dot(child_frame[2], self_frame[2]) < 0.0f) {
        child_frame[2] = -child_frame[2];
      }
    }
  }
}

GROVE_NAMESPACE_END
