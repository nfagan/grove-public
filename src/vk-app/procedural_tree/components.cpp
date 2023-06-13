#include "components.hpp"
#include "grove/common/common.hpp"
#include "grove/math/frame.hpp"
#include "grove/math/random.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/math/constants.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

uint32_t next_tree_bud_id_value{1};
uint32_t next_tree_internode_id_value{1};
uint32_t next_tree_id_value{1};

tree::Bud make_bud(tree::TreeNodeIndex parent, const Vec3f& p, const Vec3f& d,
                   float pa, float pd, float ozr, bool is_terminal) {
  tree::Bud result;
  result.id = tree::TreeBudID::create();
  result.parent = parent;
  result.position = p;
  result.direction = d;
  result.perception_angle = pa;
  result.perception_distance = pd;
  result.occupancy_zone_radius = ozr;
  result.is_terminal = is_terminal;
  return result;
}

Vec3f pine_shoot_direction(const Internode& parent, const Bud& bud, const Vec3f& env_dir,
                           const SpawnInternodeParams& params) {
  auto bud_dir_weight = params.bud_direction_weight;
  auto env_dir_weight = params.environment_direction_weight;
  auto trop_dir_weight = params.bud_tropism_direction_weight_func(parent);
  auto bud_dir = bud.direction;
  if (parent.gravelius_order > 0) {
    bud_dir.y = 0.0f;
    bud_dir = normalize_or_default(bud_dir, Vec3f{1.0f, 0.0f, 0.0f});
  }
  return normalize_or_default(
    env_dir * env_dir_weight +
    bud_dir * bud_dir_weight +
    Vec3f{0.0f, trop_dir_weight, 0.0f}, Vec3f{0.0f, 1.0f, 0.0f});
}

Vec3f original_shoot_direction(const Internode& parent, const Bud&, const Vec3f& env_dir,
                               const SpawnInternodeParams& params) {
  auto grav_order = float(parent.gravelius_order);
  auto bud_dir_weight = params.bud_direction_weight * std::exp(-grav_order * 0.25f);
  auto env_dir_weight = params.environment_direction_weight;
  auto trop_dir_weight = params.bud_tropism_direction_weight_func(parent);
  return normalize_or_default(
    env_dir * env_dir_weight +
    parent.direction * bud_dir_weight +
    Vec3f{0.0f, trop_dir_weight, 0.0f}, Vec3f{0.0f, 1.0f, 0.0f});
}

} //  anon

tree::TreeBudID tree::TreeBudID::create() {
  return TreeBudID{next_tree_bud_id_value++};
}

tree::TreeInternodeID tree::TreeInternodeID::create() {
  return TreeInternodeID{next_tree_internode_id_value++};
}

tree::TreeID tree::TreeID::create() {
  return TreeID{next_tree_id_value++};
}

void tree::TreeNodeStore::translate(const Vec3f& p) {
  for (auto& node : internodes) {
    node.translate(p);
  }
  for (auto& bud : buds) {
    bud.translate(p);
  }
}

tree::Bud tree::make_lateral_bud(TreeNodeIndex parent, const Vec3f& p, const Vec3f& d,
                                 float pa, float pd, float ozr) {
  return make_bud(parent, p, d, pa, pd, ozr, false);
}

tree::Bud tree::make_terminal_bud(TreeNodeIndex parent, const Vec3f& p, const Vec3f& d,
                                  float pa, float pd, float ozr) {
  return make_bud(parent, p, d, pa, pd, ozr, true);
}

Vec2f tree::Internode::spherical_direction() const {
  return cartesian_to_spherical(direction);
}

void tree::Internode::offset_valid_node_indices(TreeNodeIndex off) {
  if (parent != -1) {
    parent += off;
  }
  if (medial_child != -1) {
    medial_child += off;
  }
  if (lateral_child != -1) {
    lateral_child += off;
  }
}

tree::Internode tree::make_internode(TreeNodeIndex parent, const Vec3f& p, const Vec3f& d,
                                     float len, uint16_t grav_order) {
  Internode result;
  result.id = TreeInternodeID::create();
  result.parent = parent;
  result.position = p;
  result.direction = d;
  result.length = len;
  result.gravelius_order = grav_order;
  return result;
}

bool tree::Internode::equal(const Internode& a, const Internode& b) {
  return a.id == b.id &&
         a.parent == b.parent &&
         a.medial_child == b.medial_child &&
         a.lateral_child == b.lateral_child &&
         a.position == b.position &&
         a.render_position == b.render_position &&
         a.direction == b.direction &&
         a.length == b.length &&
         a.length_scale == b.length_scale &&
         a.diameter == b.diameter &&
         a.lateral_q == b.lateral_q &&
         a.bud_indices[0] == b.bud_indices[0] &&
         a.bud_indices[1] == b.bud_indices[1] &&
         a.num_buds == b.num_buds &&
         a.gravelius_order == b.gravelius_order;
}

tree::TreeNodeStore tree::make_tree_node_store(const Vec3f& origin, float initial_inode_length,
                                               float bud_pa, float bud_pd, float bud_ozr) {
  tree::TreeNodeStore result{};
  result.id = TreeID::create();

  auto init_bud_d = Vec3f{0.0f, 1.0f, 0.0f};
  auto init_bud_p = origin + init_bud_d * initial_inode_length;
  auto first_bud = make_terminal_bud(0, init_bud_p, init_bud_d, bud_pa, bud_pd, bud_ozr);

  auto first_inode = make_internode(-1, origin, init_bud_d, initial_inode_length, 0);
  first_inode.bud_indices[first_inode.num_buds++] = 0;

  result.buds.push_back(first_bud);
  result.internodes.push_back(first_inode);
  return result;
}

tree::TreeNodeStore tree::make_tree_node_store(const Vec3f& origin,
                                               const SpawnInternodeParams& spawn_p) {
  auto len = spawn_p.internode_length_scale;
  auto pa = spawn_p.bud_perception_angle;
  auto pd = spawn_p.bud_perception_distance;
  auto ozr = spawn_p.bud_occupancy_zone_radius;
  return make_tree_node_store(origin, len, pa, pd, ozr);
}

tree::DistributeBudQParams tree::DistributeBudQParams::make_debug() {
  DistributeBudQParams result{};
  result.resource_scalar = 2.0f;
  result.k = 1.0f;
  result.w_min = 0.005f;
  result.w_max = 1.0f;
  return result;
}

tree::SpawnInternodeParams tree::SpawnInternodeParams::make_debug(float scale) {
  SpawnInternodeParams result{};
  result.max_num_metamers_per_growth_cycle = 2;
  result.min_lateral_branch_y = 0.05f * scale;
  result.internode_length_scale = 0.1f * scale;
  result.min_internode_length = 0.005f * scale;
  result.max_internode_length = 3.0f * scale;
  result.bud_direction_weight = 1.0f;
  result.environment_direction_weight = 4.0f;
  result.bud_tropism_direction_weight_func = [](auto& internode) {
    return internode.gravelius_order <= 2 ? 2.0f : 0.25f;
  };
  result.min_new_bud_y_rotation = grove::pif() / 8.0f;
  result.max_new_bud_y_rotation = grove::pif() / 4.0f;
  result.bud_perception_angle = grove::pif() * 0.5f;
  result.bud_perception_distance = 0.6f * scale;
  result.bud_occupancy_zone_radius = 0.2f * scale;
  result.lateral_bud_direction_func = [](const Internode&, const Vec3f&) {
    auto theta = 2.0f * grove::pif() * urandf();
    return normalize(Vec3f{std::cos(theta), 0.0f, std::sin(theta)});
  };
  result.shoot_direction_func = original_shoot_direction;
  result.leaf_diameter = 0.0025f * scale;
  return result;
}

tree::SpawnInternodeParams tree::SpawnInternodeParams::make_debug_thicker(float scale) {
  auto res = make_debug(scale);
//  res.leaf_diameter *= 8.0f;
  res.leaf_diameter *= 4.0f;
  res.diameter_power = 2.0f;
  return res;
}

tree::SpawnInternodeParams tree::SpawnInternodeParams::make_pine(float scale) {
  auto result = make_debug(scale);
  result.bud_direction_weight = 8.0f;
  result.environment_direction_weight = 4.0f;
  result.bud_tropism_direction_weight_func = [](auto& internode) {
    return internode.gravelius_order == 0 ? 16.0f : 0.25f;
  };
  result.lateral_bud_direction_func = [](const Internode&, const Vec3f& shoot_dir) {
    auto curr_dir_xz = normalize_or_default(
      Vec3f{shoot_dir.x, 0.0f, shoot_dir.z}, Vec3f{1.0f, 0.0f, 0.0f});
    auto new_rot = make_rotation(urand_11f() * pif() * 0.5f);
    auto new_dir = new_rot * Vec2f{curr_dir_xz.x, curr_dir_xz.z};
    return normalize(Vec3f{new_dir.x, 0.0f, new_dir.y});
  };
  result.shoot_direction_func = pine_shoot_direction;
  result.leaf_diameter = 0.004f * scale;
  result.diameter_power = 1.8f;
  return result;
}

GROVE_NAMESPACE_END

