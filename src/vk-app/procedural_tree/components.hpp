#pragma once

#include "PointOctree.hpp"
#include "grove/common/identifier.hpp"
#include <unordered_map>
#include <unordered_set>

namespace grove::tree {

using TreeNodeIndex = int32_t;

constexpr TreeNodeIndex null_tree_node_index() {
  return -1;
}

struct TreeBudID {
  GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, TreeBudID, id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(TreeBudID, id)
  GROVE_INTEGER_IDENTIFIER_INEQUALITIES(TreeBudID, id)
  static TreeBudID create();

  uint32_t id{};
};

struct TreeInternodeID {
  GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, TreeInternodeID, id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(TreeInternodeID, id)
  GROVE_INTEGER_IDENTIFIER_INEQUALITIES(TreeInternodeID, id)
  static TreeInternodeID create();

  uint32_t id{};
};

struct TreeID {
  GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, TreeID, id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(TreeID, id)
  GROVE_INTEGER_IDENTIFIER_INEQUALITIES(TreeID, id)
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  static TreeID create();

  uint32_t id{};
};

struct SpawnInternodeParams;

struct AttractionPoint {
public:
  static constexpr uint32_t active_mask = 1u << 30u;
  static constexpr uint32_t consumed_mask = 1u << 31u;

public:
  void set_state(bool v, uint32_t mask) {
    if (v) {
      state_id |= mask;
    } else {
      state_id &= ~mask;
    }
  }
  void set_consumed(bool v) {
    set_state(v, consumed_mask);
  }
  void set_active(bool v) {
    set_state(v, active_mask);
  }
  void set_id(uint32_t id) {
    assert(id < (1u << 30u) && "ID is too large.");
    uint32_t m = active_mask | consumed_mask;
    uint32_t state = state_id & m;
    id &= ~m;
    id |= state;
    state_id = id;
  }
  bool is_active() const {
    return state_id & active_mask;
  }
  bool is_consumed() const {
    return state_id & consumed_mask;
  }
  uint32_t id() const {
    return state_id & ~(active_mask | consumed_mask);
  }

public:
  Vec3f position{};
  uint32_t state_id{};
};

inline AttractionPoint make_attraction_point(const Vec3f& pos, uint32_t id) {
  assert(id < (1u << 30u) && "ID is too large.");
  AttractionPoint result;
  result.position = pos;
  result.set_active(true);
  result.set_consumed(false);
  result.set_id(id);
  return result;
}

struct AttractionPointOctreeTraits {
  static inline auto position(const AttractionPoint& data) {
    return data.position;
  }

  static inline bool empty(const AttractionPoint& data) {
    return !data.is_active();
  }

  static inline void clear(AttractionPoint& data) {
    data.set_active(false);
  }

  static inline void fill(AttractionPoint& data) {
    data.set_active(true);
  }
};

using AttractionPoints = PointOctree<AttractionPoint, AttractionPointOctreeTraits>;

struct Bud {
  void translate(const Vec3f& p) {
    position += p;
  }

  TreeBudID id{};
  TreeNodeIndex parent{};
  Vec3f position{};
  Vec3f direction{};
  float perception_angle{};
  float perception_distance{};
  float occupancy_zone_radius{};
  float q{};
  float v{};
  bool is_terminal{};
};

struct Internode {
  static bool equal(const Internode& a, const Internode& b);

  bool has_parent() const {
    return parent >= 0;
  }
  bool has_lateral_child() const {
    return lateral_child >= 0;
  }
  bool has_medial_child() const {
    return medial_child >= 0;
  }
  bool is_leaf() const {
    return lateral_child < 0 && medial_child < 0;
  }
  bool is_axis_root(const std::vector<Internode>& internodes) const;
  Vec3f tip_position() const {
    return position + direction * length;
  }
  Vec3f render_tip_position() const {
    return render_position + direction * length;
  }
  float radius() const {
    return diameter * 0.5f;
  }
  Vec2f spherical_direction() const;
  void translate(const Vec3f& p) {
    position += p;
    render_position += p;
  }
  void offset_valid_node_indices(TreeNodeIndex off);

  TreeInternodeID id{};
  TreeNodeIndex parent{-1};
  TreeNodeIndex medial_child{-1};
  TreeNodeIndex lateral_child{-1};
  Vec3f position{};
  Vec3f render_position{};
  Vec3f direction{};
  float length{};
  float length_scale{1.0f};
  float diameter{};
  float lateral_q{};
  std::array<TreeNodeIndex, 2> bud_indices{};
  int8_t num_buds{};
  uint16_t gravelius_order{};
};

inline bool Internode::is_axis_root(const std::vector<Internode>& internodes) const {
  if (!has_parent()) {
    return true;
  } else {
    auto& par = internodes[parent];
    if (par.has_lateral_child() && internodes[par.lateral_child].id == id) {
      return true;
    } else {
      return false;
    }
  }
}

struct InternodeAxisRootInfo {
  TreeNodeIndex axis_root_index{};
  int nth_along_axis{};
  int axis_size{};
};

using Internodes = std::vector<Internode>;
using AxisRootInfo =
  std::unordered_map<TreeInternodeID, InternodeAxisRootInfo, TreeInternodeID::Hash>;

struct TreeNodeStore {
  Vec3f origin() const {
    return internodes.empty() ? Vec3f{} : internodes[0].position;
  }
  void translate(const Vec3f& p);

  TreeID id{};
  std::vector<Internode> internodes;
  std::vector<Bud> buds;
};

Bud make_lateral_bud(TreeNodeIndex parent, const Vec3f& p, const Vec3f& d,
                     float pa, float pd, float ozr);
Bud make_terminal_bud(TreeNodeIndex parent, const Vec3f& p, const Vec3f& d,
                      float pa, float pd, float ozr);
Internode make_internode(TreeNodeIndex parent, const Vec3f& p, const Vec3f& d,
                         float len, uint16_t grav_order);
TreeNodeStore make_tree_node_store(const Vec3f& origin,
                                   float initial_inode_length,
                                   float bud_pa, float bud_pd, float bud_ozr);
TreeNodeStore make_tree_node_store(const Vec3f& origin, const SpawnInternodeParams& spawn_params);

struct EnvironmentInput {
  Vec3f direction{};
  float q{};
};

using EnvironmentInputs = std::unordered_map<TreeBudID, EnvironmentInput, TreeBudID::Hash>;

struct RenderAxisGrowthContext {
  void clear() {
    growing.clear();
    depth_first_growing = NullOpt{};
  }

  TreeNodeIndex root_axis_index{};
  std::vector<tree::TreeNodeIndex> growing;
  Optional<tree::TreeNodeIndex> depth_first_growing;
};

struct RenderAxisDeathContext {
  int num_pending_axis_roots{};
  std::vector<tree::TreeNodeIndex> dying;
  std::unordered_set<tree::TreeNodeIndex> preserve;
};

using ClosestPointsToBuds = std::unordered_map<const AttractionPoints::Node*, Bud>;
struct SenseContext {
  void clear() {
    closest_points_to_buds.clear();
  }

  ClosestPointsToBuds closest_points_to_buds;
};

struct DistributeBudQParams {
  static DistributeBudQParams make_debug();

  float resource_scalar{2.0f};
  float k{1.0f};
  float w_min{0.005f};
  float w_max{1.0f};
};

struct SpawnInternodeParams {
  using LateralBudDirection = std::function<Vec3f(const Internode& parent, const Vec3f& shoot_dir)>;
  using ShootDirection = std::function<Vec3f(const Internode& parent, const Bud& bud,
                                             const Vec3f& env_dir, const SpawnInternodeParams& params)>;
  using AllowSpawn = std::function<bool(const Internodes&, const Bud&, const Vec3f&)>;

  static SpawnInternodeParams make_pine(float scale);
  static SpawnInternodeParams make_debug(float scale);
  static SpawnInternodeParams make_debug_thicker(float scale);

  int max_num_metamers_per_growth_cycle{2};
  int max_num_internodes{-1};
  float min_lateral_branch_y{1.0f};
  float internode_length_scale{0.1f};
  float min_internode_length{0.005f};
  float max_internode_length{3.0f};
  float bud_direction_weight{1.0f};
  float environment_direction_weight{4.0f};
  std::function<float(const Internode&)> bud_tropism_direction_weight_func{nullptr};
  AllowSpawn allow_spawn_func{nullptr};
  float min_new_bud_y_rotation{};
  float max_new_bud_y_rotation{};
  float bud_perception_angle{0.0f};
  float bud_perception_distance{0.6f};
  float bud_occupancy_zone_radius{0.2f};
  LateralBudDirection lateral_bud_direction_func{nullptr};
  ShootDirection shoot_direction_func{nullptr};
  float leaf_diameter{0.0025f};
  float diameter_power{1.5f};
  bool attenuate_diameter_by_length_scale{};
};

}