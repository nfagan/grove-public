#include "bud_fate.hpp"
#include "utility.hpp"
#include "grove/common/common.hpp"
#include "grove/common/vector_util.hpp"
#include "grove/math/util.hpp"
#include "grove/math/random.hpp"
#include "grove/math/matrix_transform.hpp"

//#define GROVE_DETERMINISTIC_BUD_FATE

GROVE_NAMESPACE_BEGIN

using namespace tree;

namespace {

using InternodeBudIndices = decltype(Internode().bud_indices);

template <typename T>
InternodeBudIndices keep_except_at(const InternodeBudIndices& src, int num_src, const T& remove_at) {
  InternodeBudIndices result{};
  int sz{};
  for (int i = 0; i < num_src; i++) {
    if (std::find(remove_at.begin(), remove_at.end(), i) == remove_at.end()) {
      assert(sz < int(src.size()));
      result[sz++] = src[i];
    }
  }
  return result;
}

Vec3f shoot_direction(const Internode& parent, const Bud& bud, const Vec3f& env_dir,
                      const SpawnInternodeParams& params) {
#if 1
  //  @NOTE: Modified 02/03/22
  return params.shoot_direction_func(parent, bud, env_dir, params);
#else
  auto grav_order = float(parent.gravelius_order);
  auto bud_dir_weight = params.bud_direction_weight * std::exp(-grav_order * 0.25f);
  auto env_dir_weight = params.environment_direction_weight;
  auto trop_dir_weight = params.bud_tropism_direction_weight_func(parent);
  return normalize_or_default(
    env_dir * env_dir_weight +
    parent.direction * bud_dir_weight +
    Vec3f{0.0f, trop_dir_weight, 0.0f}, Vec3f{0.0f, 1.0f, 0.0f});
#endif
}

#ifndef GROVE_DETERMINISTIC_BUD_FATE
[[maybe_unused]] float y_rotation(const SpawnInternodeParams& params) {
  auto max_y_rot = params.max_new_bud_y_rotation;
  auto min_y_rot = params.min_new_bud_y_rotation;
  auto y_rot_span = max_y_rot - min_y_rot;
  auto y_rot = (y_rot_span * urandf() + min_y_rot) * (urand() > 0.5 ? -1.0f : 1.0f);
  return y_rot;
}

Vec3f lateral_direction(const Internode& parent, const Vec3f& shoot_dir,
                        const SpawnInternodeParams& params) {
#if 1
  //  @NOTE: Modified 02/03/22
  return params.lateral_bud_direction_func(parent, shoot_dir);
#else
  if (params.make_lateral_buds_horizontal_func(parent)) {
    auto theta = 2.0f * grove::pif() * urandf();
    return normalize(Vec3f{std::cos(theta), 0.0f, std::sin(theta)});
  } else {
    auto y_rot = y_rotation(params);
    return normalize(to_vec3(make_y_rotation(y_rot) * Vec4f{shoot_dir, 1.0f}));
  }
#endif
}
#else
Vec3f lateral_direction(const Internode&, const Vec3f&, const SpawnInternodeParams&) {
//  auto theta = 2.0f * grove::pif() * grove::rand();
  auto theta = 1.5f;
  return normalize(Vec3f{std::cos(theta), 0.0f, std::sin(theta)});
}
#endif

void grow_shoot(TreeNodeIndex parent_inode_ind, TreeNodeIndex inode_bud_ind,
                Internodes& internodes, std::vector<Bud>& buds,
                const Vec3f& shoot_dir, int num_metamers, float inode_len,
                const SpawnInternodeParams& params) {
  auto pa = params.bud_perception_angle;
  auto pd = params.bud_perception_distance;
  auto ozr = params.bud_occupancy_zone_radius;

  for (int i = 0; i < num_metamers; i++) {
    auto next_inode_ind = TreeNodeIndex(internodes.size());

    auto& parent_inode = internodes[parent_inode_ind];
    //  @Note: read from buds array rather than passing a reference to the bud; we may
    //  invalidate pointers after pushing new buds to the buds array below.
    auto& source_bud = buds[inode_bud_ind];
    const auto parent_is_lateral_bud = i == 0 && !source_bud.is_terminal;
    auto grav_order = uint16_t(
      parent_is_lateral_bud ? parent_inode.gravelius_order + 1 : parent_inode.gravelius_order);

    auto inode_p = source_bud.position + shoot_dir * inode_len * float(i);
    //  Create a new internode.
    auto new_inode = make_internode(parent_inode_ind, inode_p, shoot_dir, inode_len, grav_order);

    auto bud_p = source_bud.position + shoot_dir * inode_len * float(i+1);

    if (bud_p.y >= params.min_lateral_branch_y) {
      auto lat_bud_d = lateral_direction(parent_inode, shoot_dir, params);
      auto lat_bud = make_lateral_bud(next_inode_ind, bud_p, lat_bud_d, pa, pd, ozr);
      auto lat_ind = TreeNodeIndex(buds.size());
      new_inode.bud_indices[new_inode.num_buds++] = lat_ind;
      buds.push_back(lat_bud);
    }

    if (i == num_metamers-1) {
      //  Last internode also contains a terminal bud, facing the shoot direction.
      auto term_bud = make_terminal_bud(next_inode_ind, bud_p, shoot_dir, pa, pd, ozr);
      auto term_ind = TreeNodeIndex(buds.size());
      new_inode.bud_indices[new_inode.num_buds++] = term_ind;
      buds.push_back(term_bud);
    }

    if (parent_is_lateral_bud) {
      //  Spawning from a lateral bud is a lateral shoot.
      assert(!internodes[parent_inode_ind].has_lateral_child());
      internodes[parent_inode_ind].lateral_child = next_inode_ind;
    } else {
      //  Spawning from a terminal bud (or parent shoot) is a medial shoot.
      assert(!internodes[parent_inode_ind].has_medial_child());
      internodes[parent_inode_ind].medial_child = next_inode_ind;
    }

    internodes.push_back(new_inode);
    //  Make the next internode a child of the current one.
    parent_inode_ind = next_inode_ind;
  }
}

inline TreeNodeIndex
ith_bud_index(const Internodes& internodes, TreeNodeIndex inode_ind, int bud_index) {
  return internodes[inode_ind].bud_indices[bud_index];
}

inline float leaf_diameter(const SpawnInternodeParams& params) {
  return std::pow(params.leaf_diameter, params.diameter_power);
}

float assign_diameter(Internode* internodes, TreeNodeIndex inode_ind,
                      const SpawnInternodeParams& params) {
  auto& node = internodes[inode_ind];
  auto md = node.has_medial_child() ?
    assign_diameter(internodes, node.medial_child, params) : leaf_diameter(params);
  auto ld = node.has_lateral_child() ?
    assign_diameter(internodes, node.lateral_child, params) : leaf_diameter(params);

  auto d = md + ld;
  node.diameter = std::max(params.leaf_diameter, float(std::pow(d, 1.0/params.diameter_power)));
  if (params.attenuate_diameter_by_length_scale) {
    node.diameter *= node.length_scale;
  }

  assert(std::isfinite(node.diameter) && node.diameter >= 0.0f);
  return d;
}

} //  anon

DynamicArray<int, 4> tree::internode_bud_fate(TreeNodeIndex internode_ind,
                                              TreeNodeStore& tree_nodes,
                                              const EnvironmentInputs& inputs,
                                              const SpawnInternodeParams& params) {
  DynamicArray<int, 4> remove_from_inode;
  DynamicArray<int, 4> remove_from_buds;

  auto& internodes = tree_nodes.internodes;
  auto& buds = tree_nodes.buds;

  auto num_buds = internodes[internode_ind].num_buds;
  int8_t num_removed_buds{};

  for (int i = 0; i < num_buds; i++) {
    auto bud_ind = ith_bud_index(internodes, internode_ind, i);
    auto& bud = buds[bud_ind];
    auto reported_num_metamers = int(std::floor(bud.v));
    auto num_metamers = std::min(
      params.max_num_metamers_per_growth_cycle, reported_num_metamers);

    if (params.max_num_internodes >= 0) {
      int num_remain = std::max(0, params.max_num_internodes - int(internodes.size()));
      num_metamers = std::min(num_metamers, num_remain);
    }

    bool too_low = !bud.is_terminal && bud.position.y < params.min_lateral_branch_y;
    if (num_metamers == 0 || bud.q == 0 || too_low) {
      continue;
    }

    auto environment_dir = inputs.at(bud.id).direction;
    auto shoot_dir = shoot_direction(internodes[internode_ind], bud, environment_dir, params);

    if (params.allow_spawn_func &&
        !params.allow_spawn_func(tree_nodes.internodes, bud, shoot_dir)) {
      continue;
    }

    assert(inputs.count(bud.id) > 0);
    auto inode_len = bud.v / float(reported_num_metamers) * params.internode_length_scale;
    inode_len = clamp(inode_len, params.min_internode_length, params.max_internode_length);

    grow_shoot(
      internode_ind, bud_ind, internodes, buds, shoot_dir, num_metamers, inode_len, params);

    remove_from_inode.push_back(i);
    remove_from_buds.push_back(bud_ind);
    num_removed_buds++;
  }

  //  Remove buds that grew into new internodes.
  auto& inode = internodes[internode_ind];
  inode.bud_indices = keep_except_at(inode.bud_indices, inode.num_buds, remove_from_inode);
  inode.num_buds = inode.num_buds - num_removed_buds;

  return remove_from_buds;
}

void tree::remove_grown_buds(TreeNodeStore& tree_nodes, const ArrayView<int>& remove_at) {
#ifdef GROVE_DEBUG
  for (auto& rem : remove_at) {
    assert(rem >= 0 && rem < int(tree_nodes.buds.size()));
  }
#endif

  for (auto& node : tree_nodes.internodes) {
    for (int i = 0; i < node.num_buds; i++) {
      auto& ind = node.bud_indices[i];
      auto it = std::upper_bound(remove_at.begin(), remove_at.end(), ind);
      auto num_adjust = int(it - remove_at.begin());
      assert(ind - num_adjust >= 0);
      ind -= num_adjust;
    }
  }

  erase_set(tree_nodes.buds, remove_at);
}

void tree::bud_fate(TreeNodeStore& tree_nodes,
                    const EnvironmentInputs& inputs,
                    const SpawnInternodeParams& params) {
  std::vector<int> remove_bud_inds;
  auto num_nodes = int(tree_nodes.internodes.size());

  for (int i = 0; i < num_nodes; i++) {
    auto to_remove = internode_bud_fate(i, tree_nodes, inputs, params);
    for (auto& rem : to_remove) {
      remove_bud_inds.push_back(rem);
    }
  }

  std::sort(remove_bud_inds.begin(), remove_bud_inds.end());
  remove_grown_buds(tree_nodes, make_data_array_view<int>(remove_bud_inds));

  set_diameter(tree_nodes.internodes, params);
#ifdef GROVE_DEBUG
  tree::validate_internode_relationships(tree_nodes.internodes);
#endif
}

void tree::set_diameter(Internodes& internodes, const SpawnInternodeParams& params,
                        TreeNodeIndex root_index) {
  assert(params.diameter_power > 0.0f);
  if (!internodes.empty()) {
    assign_diameter(internodes.data(), root_index, params);
  }
}

void tree::set_diameter(Internode* internodes, int num_internodes,
                        const SpawnInternodeParams& params, TreeNodeIndex root_index) {
  assert(params.diameter_power > 0.0f);
  if (num_internodes > 0) {
    assign_diameter(internodes, root_index, params);
  }
}

GROVE_NAMESPACE_END
