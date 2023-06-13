#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/matrix.hpp"
#include "grove/math/bounds.hpp"
#include "grove/common/SlotLists.hpp"
#include "grove/common/ContiguousElementGroupAllocator.hpp"

namespace grove::foliage_occlusion {

inline uint32_t pack_2fn_1u32(float a, float b) {
  assert(a >= 0.0f && a <= 1.0f);
  assert(b >= 0.0f && b <= 1.0f);
  auto a32 = uint32_t(a * float(0xffff));
  auto b32 = uint32_t(b * float(0xffff));
  a32 <<= 16u;
  a32 |= b32;
  return a32;
}

inline Vec2f unpack_1u32_2fn(uint32_t v) {
  auto mask = uint32_t(0xffff);
  auto b16 = uint16_t(v & mask);
  auto a16 = uint16_t((v & ~mask) >> 16u);
  return Vec2f{float(a16) / float(0xffff), float(b16) / float(0xffff)};
}

inline Vec4f unpack_normal(float really_uint320, float really_uint321) {
  uint32_t c0;
  memcpy(&c0, &really_uint320, sizeof(uint32_t));
  uint32_t c1;
  memcpy(&c1, &really_uint321, sizeof(uint32_t));
  auto xy = unpack_1u32_2fn(c0);
  auto zw = unpack_1u32_2fn(c1);
  return Vec4f{xy.x, xy.y, zw.x, zw.y} * 2.0f - 1.0f;
}

struct Config {
  static constexpr uint32_t max_num_instances_per_cluster = 5;
  static constexpr uint32_t max_num_grid_cells = 64 * 16 * 64;
  static constexpr uint32_t default_max_num_occlude_steps = 8;
  static constexpr uint32_t max_num_debug_occlude_steps = 16;
};

struct ClusterPendingProcessIndices {
  uint32_t cluster;
  uint32_t instance;
};

//  Expect size = 2 * uint, even though instance index could be smaller.
static_assert(sizeof(ClusterPendingProcessIndices) == 8);

enum class CullingState : uint32_t {
  Idle,
  FadingOut,
  FullyFadedOut,
  PendingFadeIn,
  FadingIn,
};

struct ClusterInstance {
  bool is_sentinel() const {
    return data0.y != 0;
  }
  void set_sentinel() {
    data0.y = 1;
  }
  void set_culling_state(CullingState state) {
    data0.x = uint32_t(state);
  }
  CullingState get_culling_state() const {
    assert(data0.x >= 0 && data0.x <= 4);
    return CullingState{data0.x};
  }
  bool is_idle_state() const {
    return get_culling_state() == CullingState::Idle;
  }
  void set_position(const Vec3f& p) {
    position_right_xy.x = p.x;
    position_right_xy.y = p.y;
    position_right_xy.z = p.z;
  }
  Vec3f get_position() const {
    return to_vec3(position_right_xy);
  }
  Vec3f get_right() const {
    return to_vec3(unpack_normal(position_right_xy.w, right_z_normal_xyz_scale_xy.x));
  }
  void set_right_normal(Vec3f r, Vec3f n) {
    r = clamp_each(r, Vec3f{-1.0f}, Vec3f{1.0f}) * 0.5f + 0.5f;
    n = clamp_each(n, Vec3f{-1.0f}, Vec3f{1.0f}) * 0.5f + 0.5f;
    uint32_t right_xy = pack_2fn_1u32(r.x, r.y);
    uint32_t right_z_normal_x = pack_2fn_1u32(r.z, n.x);
    uint32_t normal_yz = pack_2fn_1u32(n.y, n.z);
    memcpy(&position_right_xy.w, &right_xy, sizeof(float));
    memcpy(&right_z_normal_xyz_scale_xy.x, &right_z_normal_x, sizeof(float));
    memcpy(&right_z_normal_xyz_scale_xy.y, &normal_yz, sizeof(float));
  }
  Vec3f get_normal() const {
    auto v = unpack_normal(right_z_normal_xyz_scale_xy.x, right_z_normal_xyz_scale_xy.y);
    return Vec3f{v.y, v.z, v.w};  //  @NOTE
  }
  Vec3f get_up() const {
    return cross(get_normal(), get_right());
  }
  void set_scale(const Vec2f& s) {
    right_z_normal_xyz_scale_xy.z = s.x;
    right_z_normal_xyz_scale_xy.w = s.y;
  }
  Vec2f get_scale() const {
    return Vec2f{right_z_normal_xyz_scale_xy.z, right_z_normal_xyz_scale_xy.w};
  }
  void set_culled_on_frame_id(uint32_t i) {
    data0.z = i;
  }
  uint32_t get_culled_on_frame_id() const {
    return data0.z;
  }
  void set_transition_fraction(float v) {
    assert(v >= 0.0f && v <= 1.0f);
    memcpy(&data0.w, &v, sizeof(float));
  }
  float get_transition_fraction() const {
    float v;
    memcpy(&v, &data0.w, sizeof(float));
    assert(v >= 0.0f && v <= 1.0f);
    return v;
  }

  Vec4f position_right_xy;
  Vec4f right_z_normal_xyz_scale_xy;
  Vec4<uint32_t> data0; //  culling (true / false)
};

struct Cluster {
  uint32_t iteratively_count_num_instances() const {
    uint32_t s{};
    for (auto& inst : instances) {
      if (inst.is_sentinel()) {
        break;
      } else {
        s++;
      }
    }
    return s;
  }

  Vec3f get_canonical_position() const {
    return to_vec3(canonical_position);
  }
  Bounds3f get_aabb() const {
    return Bounds3f{to_vec3(aabb_p0), to_vec3(aabb_p1)};
  }

  Vec4f aabb_p0;
  Vec4f aabb_p1;
  Vec4f canonical_position;
  ClusterInstance instances[Config::max_num_instances_per_cluster];
};

struct ClusterMeta {
  OBB3f src_bounds;
};

struct GridCellClusterListNodeData {
  Vec3f get_position() const {
    return Vec3f{
      inv_frame_x_position_x.w,
      inv_frame_y_position_y.w,
      inv_frame_z_position_z.w
    };
  }
  Mat3f get_inv_frame() const {
    return Mat3f{
      to_vec3(inv_frame_x_position_x),
      to_vec3(inv_frame_y_position_y),
      to_vec3(inv_frame_z_position_z)
    };
  }
  Vec3f get_half_size() const {
    return to_vec3(half_size);
  }

  Vec4f inv_frame_x_position_x;
  Vec4f inv_frame_y_position_y;
  Vec4f inv_frame_z_position_z;
  Vec4f half_size;
  uint32_t cluster_group_index;
  uint32_t cluster_offset;
};

using GridCellClusterListNode = SlotListsPaddedNode<GridCellClusterListNodeData, 3>;
using GridCellClusterLists = SlotLists<GridCellClusterListNodeData, GridCellClusterListNode>;
using GridCellClusterList = GridCellClusterLists::List;

struct Grid {
public:
  static constexpr uint32_t max_num_cells = Config::max_num_grid_cells;

public:
  size_t size_of_active_cells_bytes() const {
    return size_t(prod(num_cells)) * sizeof(uint32_t);
  }
  uint32_t num_active_cells() const {
    return uint32_t(prod(num_cells));
  }

public:
  uint32_t cells[max_num_cells];

  Vec3f origin;
  Vec3f cell_size;
  Vec3<int> num_cells;
};

struct OcclusionCheckDebugContext {
  Vec3f ro;
  Vec3f rd;
  Vec3<int> steps[Config::max_num_debug_occlude_steps];
  uint32_t num_steps;
};

struct OcclusionParams {
  float cull_distance_threshold;
  float min_intersect_area_fraction;
  float tested_instance_scale;
  int max_num_steps;
};

struct FoliageOcclusionSystem {
  uint32_t num_clusters() const {
    return uint32_t(clusters.size());
  }
  uint32_t num_cluster_groups() const {
    assert(cluster_group_offsets.size() == cluster_groups.num_groups());
    return uint32_t(cluster_group_offsets.size());
  }

  Grid grid;
  GridCellClusterLists grid_cluster_lists;
  ContiguousElementGroupAllocator cluster_groups;
  std::vector<uint32_t> cluster_group_offsets;
  std::vector<Cluster> clusters;
  std::vector<ClusterMeta> cluster_meta;
  std::vector<OcclusionCheckDebugContext> debug_contexts;
  std::vector<ClusterPendingProcessIndices> pending_process_indices;

  OcclusionParams occlusion_params{};
  bool data_structure_modified{};
  bool clusters_updated{};

  uint32_t num_pending_process_indices{};
  uint32_t culled_on_frame_id{1};
  uint32_t update_id{};
};

}