#include "foliage_occlusion.hpp"
#include "foliage_occlusion_types.hpp"
#include "../render/debug_draw.hpp"
#include "grove/math/matrix.hpp"
#include "grove/math/bounds.hpp"
#include "grove/math/intersect.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/GridIterator3.hpp"
#include "grove/math/util.hpp"
#include "grove/common/SlotLists.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/common.hpp"

#define ENABLE_DEBUG (0)

GROVE_NAMESPACE_BEGIN

namespace {

using namespace foliage_occlusion;

static_assert(sizeof(GridCellClusterListNode) == sizeof(GridCellClusterListNodeData) + 8);
static_assert((sizeof(GridCellClusterListNode) % (4 * sizeof(float))) == 0);
static_assert(alignof(GridCellClusterListNodeData) == 4);
static_assert(sizeof(GridCellClusterList) == sizeof(uint32_t));

void init_grid(Grid* grid, const Vec3f& cell_size, const Vec3<int>& num_cells) {
  assert(uint32_t(prod(num_cells)) <= Grid::max_num_cells);
  std::fill(grid->cells, grid->cells + Grid::max_num_cells, GridCellClusterLists::invalid);
  auto span = cell_size * to_vec3f(num_cells);
  grid->origin = -span * 0.5f;
  grid->num_cells = num_cells;
  grid->cell_size = cell_size;
}

Vec3f grid_cell_index_to_world_position(const Grid& grid, const Vec3<int>& ci) {
  return to_vec3f(ci) * grid.cell_size + grid.origin;
}

Vec3<int> to_grid_cell_index(const Grid& grid, const Vec3f& p) {
  return to_vec3i(floor((p - grid.origin) / grid.cell_size));
}

Bounds3<int> to_quantized_range(const Bounds3f& aabb, const Vec3f& cell_size,
                                const Vec3f& grid_origin) {
  auto p0 = floor((aabb.min - grid_origin) / cell_size);
  auto p1_off = aabb.max - grid_origin;
  auto p1 = floor(p1_off / cell_size);
  auto p1_base = p1 * cell_size;
  p1.x += (p1_base.x == p1_off.x ? 0.0f : 1.0f);
  p1.y += (p1_base.y == p1_off.y ? 0.0f : 1.0f);
  p1.z += (p1_base.z == p1_off.z ? 0.0f : 1.0f);
  return Bounds3<int>{to_vec3i(p0), to_vec3i(p1)};
}

Bounds3<int> to_quantized_range(const Bounds3f& aabb, const Grid& grid) {
  return to_quantized_range(aabb, grid.cell_size, grid.origin);
}

bool is_valid_grid_cell_index(const Grid& grid, const Vec3<int>& index) {
  return index.x >= 0 && index.x < grid.num_cells.x &&
         index.y >= 0 && index.y < grid.num_cells.y &&
         index.z >= 0 && index.z < grid.num_cells.z;
}

uint32_t to_linear_grid_cell_index(const Grid& grid, const Vec3<int>& index) {
  assert(all(ge(index, Vec3<int>{})) && all(lt(index, grid.num_cells)));
  auto res = (grid.num_cells.x * grid.num_cells.y) * index.z + index.y * grid.num_cells.x + index.x;
  assert(uint32_t(res) < Grid::max_num_cells);
  return uint32_t(res);
}

Optional<uint32_t> maybe_get_linear_grid_cell_index(const Grid& grid, const Vec3<int>& index) {
  if (is_valid_grid_cell_index(grid, index)) {
    return Optional<uint32_t>(to_linear_grid_cell_index(grid, index));
  } else {
    return NullOpt{};
  }
}

ArrayView<const GridCellClusterListNode>
read_grid_cell_cluster_list_nodes(const FoliageOcclusionSystem* sys) {
  return {
    sys->grid_cluster_lists.read_node_begin(),
    sys->grid_cluster_lists.read_node_end()
  };
}

float sign_or_zero(float v) {
  return v == 0.0f ? 0.0f : v < 0.0f ? -1.0f : 1.0f;
}

//  @TODO: Move to intersect lib
bool ray_circle_intersect(const Vec3f& ro, const Vec3f& rd, const Vec3f& pp,
                          const Vec3f& pn, float pr) {
  float denom = dot(pn, rd);
  if (denom == 0.0) {
    return false;
  }

  float num = -dot(pn, ro) + dot(pn, pp);
  float t = num / denom;
  float r = ((ro + t * rd) - pp).length();
  return r <= pr;
}

bool ray_cluster_instance_intersect(const Vec3f& ro, const Vec3f& rd, const ClusterInstance& inst) {
  Vec3f pp = inst.get_position();
  Vec3f pn = inst.get_normal();
  auto ps = inst.get_scale();
  float r = std::max(ps.x, ps.y); //  @TODO
  return ray_circle_intersect(ro, rd, pp, pn, r);
}

Vec2f project(const Mat4f& proj_view, const Vec3f& p) {
  auto res = proj_view * Vec4f{p, 1.0f};
  return Vec2f{res.x / res.w, res.y / res.w};
}

Bounds2f cluster_instance_projected_aabb(const ClusterInstance& inst, const Mat4f& proj_view,
                                         float global_scale) {
  Vec3f p = inst.get_position();
  Vec2f s = inst.get_scale() * global_scale;
  Vec3f x = inst.get_right();
  Vec3f y = cross(inst.get_normal(), x);
  auto xs = x * s.x;
  auto ys = y * s.y;
  auto p0 = project(proj_view, p - xs - ys);
  auto p1 = project(proj_view, p - xs + ys);
  auto p2 = project(proj_view, p + xs - ys);
  auto p3 = project(proj_view, p + xs + ys);
  auto min0 = min(p0, p1);
  auto max0 = max(p0, p1);
  auto min1 = min(p2, p3);
  auto max1 = max(p2, p3);
  return Bounds2f{min(min0, min1), max(max0, max1)};
}

float projected_aabb_area(const Bounds2f& b) {
  auto sz = max(Vec2f{}, b.size());
  return sz.x * sz.y;
}

float intersect_area_fraction(const Bounds2f& src, float src_area, const Bounds2f& target) {
  return projected_aabb_area(intersect_of(src, target)) / src_area;
}

bool cluster_can_be_culled(const Vec3f& cluster_p, const Vec3f& camera_p, float dist_thresh) {
  float d = (cluster_p - camera_p).length();
  return d >= dist_thresh;
}

bool cluster_should_fade_back_in(const Vec3f& cluster_p, const Vec3f& camera_p, float dist_thresh) {
  float d = (cluster_p - camera_p).length();
  return d < dist_thresh;
}

bool occluded(const FoliageOcclusionSystem* sys, const Vec3f& camera_pos, const Vec3f& p,
              const Bounds2f& proj_aabb, const Mat4f& proj_view, uint32_t current_frame_id,
              const OcclusionParams& params,
              OcclusionCheckDebugContext* debug) {
  (void) debug;
  (void) proj_aabb;
  //  local functions
  const auto to_next_cell = [](const Vec3f& bounds, const Vec3f& ro, const Vec3f& rd) {
    auto cs = (bounds - ro) / rd;
    return Vec3f(
      rd.x == 0.0f ? infinityf() : cs.x,
      rd.y == 0.0f ? infinityf() : cs.y,
      rd.z == 0.0f ? infinityf() : cs.z
    );
  };

  const auto next_cell_bound = [](const Grid& grid, const Vec3f& ro_index, const Vec3f& rd) {
    auto incr = ro_index + to_vec3f(gt(rd, Vec3f(0.0f)));
    return incr * grid.cell_size + grid.origin;
  };

  auto ray_node_intersect = [](Vec3f ro, Vec3f rd, const GridCellClusterListNodeData& node) {
    const auto inv_frame = node.get_inv_frame();
    ro = inv_frame * (ro - node.get_position());
    rd = inv_frame * rd;
    auto hs = node.get_half_size();
    float ignore1;
    float ignore2;
    return ray_aabb_intersect(ro, rd, -hs, hs, &ignore1, &ignore2);
  };

  const auto sign_or_zero3 = [](const Vec3f& v) {
    return Vec3f{sign_or_zero(v.x), sign_or_zero(v.y), sign_or_zero(v.z)};
  };

  const auto skip_cluster_list_node = [](const GridCellClusterListNodeData& node_data,
    const Vec3f& camera_pos, const OcclusionParams& params) {
    //
#if 0
    (void) camera_pos;
    (void) params;
    (void) node_data;
    return false;
#else
    auto node_p = node_data.get_position();
    return cluster_can_be_culled(node_p, camera_pos, params.cull_distance_threshold);
#endif
  };

  //  begin procedure
  Vec3f rd = camera_pos - p;
  float len = rd.length();

  if (len == 0.0) {
    return false;
  } else {
    rd /= len;
  }

  Vec3f ro = p;
  ro += rd * std::max(0.0f, len - params.cull_distance_threshold);

  auto ro_index = to_grid_cell_index(sys->grid, ro);

  Vec3f cs = to_next_cell(next_cell_bound(sys->grid, to_vec3f(ro_index), rd), ro, rd);
  Vec3f ts = abs(sys->grid.cell_size / rd);
  Vec3<int> ss = to_vec3i(sign_or_zero3(rd));
  Vec3<int> is{};
  uint32_t step{};

  using ViewNodes = ArrayView<const GridCellClusterListNode>;
  ViewNodes grid_cell_cluster_list_nodes = read_grid_cell_cluster_list_nodes(sys);

  const float src_proj_aabb_area = projected_aabb_area(proj_aabb);
  const float min_area_frac = params.min_intersect_area_fraction;
  (void) src_proj_aabb_area;

  const float tested_instance_scale = params.tested_instance_scale;

#if ENABLE_DEBUG
  debug->ro = ro;
  debug->rd = rd;
  assert(uint32_t(params.max_num_steps) <= Config::max_num_debug_occlude_steps);
#endif

  const auto max_num_occlude_steps = uint32_t(params.max_num_steps);
  while (step < max_num_occlude_steps) {
    Vec3<int> cell_index = ro_index + is;
    if (!is_valid_grid_cell_index(sys->grid, cell_index)) {
      break;
    }

#if ENABLE_DEBUG
    debug->steps[debug->num_steps++] = cell_index;
#endif

    uint32_t linear_cell_index = to_linear_grid_cell_index(sys->grid, cell_index);
    uint32_t cluster_list = sys->grid.cells[linear_cell_index];

    while (cluster_list != GridCellClusterLists::invalid) {
      GridCellClusterListNode node = grid_cell_cluster_list_nodes[cluster_list];
      const GridCellClusterListNodeData& node_data = node.data;

      if (!skip_cluster_list_node(node_data, camera_pos, params) &&
          ray_node_intersect(ro, rd, node_data)) {
        uint32_t group_offset = sys->cluster_group_offsets[node_data.cluster_group_index];
        uint32_t cluster_index = group_offset + node_data.cluster_offset;
        const Cluster& cluster = sys->clusters[cluster_index];

        for (uint32_t i = 0; i < Config::max_num_instances_per_cluster; i++) {
          const ClusterInstance& cluster_instance = cluster.instances[i];
          if (cluster_instance.is_sentinel()) {
            break;
          }
#ifdef GROVE_DEBUG
          if (!cluster_instance.is_idle_state()) {
            assert(cluster_instance.get_culled_on_frame_id() != current_frame_id);
          }
#else
          (void) current_frame_id;
#endif
          if (cluster_instance.is_idle_state() &&
              ray_cluster_instance_intersect(ro, rd, cluster_instance)) {
#if 1
            auto target_aabb = cluster_instance_projected_aabb(
              cluster_instance, proj_view, tested_instance_scale);
            if (intersect_area_fraction(proj_aabb, src_proj_aabb_area, target_aabb) > min_area_frac) {
              return true;
            }
#else
            return true;
#endif
          }
        }
      }

      cluster_list = node.next;
    }

    if (cs.x < cs.y && cs.x < cs.z) {
      is.x += ss.x;
      cs.x += ts.x;
    } else if (cs.y < cs.z) {
      is.y += ss.y;
      cs.y += ts.y;
    } else {
      is.z += ss.z;
      cs.z += ts.z;
    }

    step++;
  }

  return false;
}

} //  anon

FoliageOcclusionSystem* foliage_occlusion::create_foliage_occlusion_system() {
  auto* sys = new FoliageOcclusionSystem();
//  init_grid(&sys->grid, Vec3f{16.0f}, Vec3<int>{32, 8, 32});
  init_grid(&sys->grid, Vec3f{8.0f}, Vec3<int>{64, 16, 64});
  return sys;
}

UpdateOcclusionSystemResult
foliage_occlusion::update_foliage_occlusion_system(FoliageOcclusionSystem* sys) {
  UpdateOcclusionSystemResult result{};
  if (sys->data_structure_modified) {
    result.data_structure_modified = true;
    sys->data_structure_modified = false;
  }
  if (sys->clusters_updated) {
    result.clusters_modified = true;
    sys->clusters_updated = false;
  }
  return result;
}

void foliage_occlusion::destroy_foliage_occlusion_system(FoliageOcclusionSystem** sys) {
  delete *sys;
  *sys = nullptr;
}

OcclusionSystemStats
foliage_occlusion::get_foliage_occlusion_system_stats(const FoliageOcclusionSystem* sys) {
  OcclusionSystemStats result{};
  for (uint32_t i = 0; i < sys->grid.num_active_cells(); i++) {
    if (sys->grid.cells[i] != GridCellClusterLists::invalid) {
      result.num_grid_lists++;
    }
  }

  result.num_clusters = uint32_t(sys->clusters.size());
  return result;
}

void foliage_occlusion::remove_cluster_group(FoliageOcclusionSystem* sys,
                                             const ClusterGroupHandle& gh) {
  const uint32_t num_clusters = sys->cluster_groups.read_group(gh.element_group)->count;
  for (uint32_t i = 0; i < num_clusters; i++) {
    const uint32_t cluster_index = sys->cluster_group_offsets[gh.element_group.index] + i;
    assert(cluster_index < uint32_t(sys->clusters.size()));
    const auto& cluster = sys->clusters[cluster_index];

    auto cluster_aabb = Bounds3f{to_vec3(cluster.aabb_p0), to_vec3(cluster.aabb_p1)};
    const auto range = to_quantized_range(cluster_aabb, sys->grid);
    for (auto it = begin_it(range.min, range.max); is_valid(it); ++it) {
      if (const auto maybe_cell_index = maybe_get_linear_grid_cell_index(sys->grid, *it)) {
        bool found_cluster{};

        auto list = GridCellClusterList{sys->grid.cells[maybe_cell_index.value()]};
        auto list_it = sys->grid_cluster_lists.begin(list);
        while (list_it != sys->grid_cluster_lists.end()) {
          const GridCellClusterListNodeData& node_data = *list_it;
          if (node_data.cluster_group_index == gh.element_group.index) {
            list_it = sys->grid_cluster_lists.erase(&list, list_it);
            found_cluster = true;
            break;
          } else {
            ++list_it;
          }
        }

        sys->grid.cells[maybe_cell_index.value()] = list.head;
        assert(found_cluster);
        (void) found_cluster;
      }
    }
  }

  sys->cluster_groups.release(gh.element_group);
  ContiguousElementGroupAllocator::Movement move{};
  uint32_t new_num_clusters;
  (void) sys->cluster_groups.arrange_implicit(&move, &new_num_clusters);
  move.apply(sys->clusters.data(), sizeof(Cluster));
  move.apply(sys->cluster_meta.data(), sizeof(ClusterMeta));

  sys->clusters.resize(new_num_clusters);
  sys->cluster_meta.resize(new_num_clusters);
  sys->pending_process_indices.resize(new_num_clusters * Config::max_num_instances_per_cluster);

  uint32_t off{};
  for (auto* it = sys->cluster_groups.read_group_begin();
       it != sys->cluster_groups.read_group_end();
       ++it) {
    sys->cluster_group_offsets[off++] = it->offset;
  }

  sys->data_structure_modified = true;
}

ClusterGroupHandle foliage_occlusion::insert_cluster_group(FoliageOcclusionSystem* sys,
                                                           const ClusterDescriptor* cluster_desc,
                                                           uint32_t num_clusters) {
  ContiguousElementGroupAllocator::ElementGroupHandle group_handle{};
  const uint32_t new_num_clusters = sys->cluster_groups.reserve(num_clusters, &group_handle);

  if (group_handle.index >= sys->cluster_group_offsets.size()) {
    sys->cluster_group_offsets.resize(group_handle.index + 1);
  }

  const auto current_num_clusters = uint32_t(sys->clusters.size());
  sys->clusters.resize(new_num_clusters);
  sys->cluster_meta.resize(new_num_clusters);
  sys->pending_process_indices.resize(new_num_clusters * Config::max_num_instances_per_cluster);
  sys->cluster_group_offsets[group_handle.index] = current_num_clusters;
  sys->data_structure_modified = true;

  for (uint32_t i = 0; i < num_clusters; i++) {
    const auto& cluster = cluster_desc[i];
    assert(cluster.num_instances <= Config::max_num_instances_per_cluster);

    const auto cluster_aabb = obb3_to_aabb(cluster.bounds);
    auto cluster_frame = Mat3f{cluster.bounds.i, cluster.bounds.j, cluster.bounds.k};
    auto cluster_inv_frame = transpose(cluster_frame);
    //  @NOTE: Make sure `cluster_p` is used for both canonical cluster position and grid cell list
    //  node position.
    auto cluster_p = cluster.bounds.position;
    auto cluster_half_size = cluster.bounds.half_size;

    auto& dst_cluster = sys->clusters[i + current_num_clusters];
    dst_cluster = {};
    dst_cluster.aabb_p0 = Vec4f{cluster_aabb.min, 0.0f};
    dst_cluster.aabb_p1 = Vec4f{cluster_aabb.max, 0.0f};
    dst_cluster.canonical_position = Vec4f{cluster_p, 0.0f};

    auto& dst_meta = sys->cluster_meta[i + current_num_clusters];
    dst_meta = {};
    dst_meta.src_bounds = cluster.bounds;

    const uint32_t num_inst_add = std::min(
      cluster.num_instances, Config::max_num_instances_per_cluster);
    for (uint32_t j = 0; j < num_inst_add; j++) {
      auto& src_inst = cluster.instances[j];
      auto& dst_inst = dst_cluster.instances[j];

      assert(std::abs(src_inst.x.length() - 1.0f) < 1e-2f);
      assert(std::abs(src_inst.n.length() - 1.0f) < 1e-2f);

      dst_inst = {};
      dst_inst.set_position(src_inst.p);
      dst_inst.set_right_normal(src_inst.x, src_inst.n);
      dst_inst.set_scale(src_inst.s);
    }

    if (num_inst_add < Config::max_num_instances_per_cluster) {
      dst_cluster.instances[num_inst_add].set_sentinel();
    }

    const auto range = to_quantized_range(cluster_aabb, sys->grid);
    for (auto it = begin_it(range.min, range.max); is_valid(it); ++it) {
      if (const auto maybe_cell_index = maybe_get_linear_grid_cell_index(sys->grid, *it)) {
        auto list = GridCellClusterList{sys->grid.cells[maybe_cell_index.value()]};

        GridCellClusterListNodeData node_data{};
        node_data.inv_frame_x_position_x = Vec4f{cluster_inv_frame[0], cluster_p.x};
        node_data.inv_frame_y_position_y = Vec4f{cluster_inv_frame[1], cluster_p.y};
        node_data.inv_frame_z_position_z = Vec4f{cluster_inv_frame[2], cluster_p.z};
        node_data.half_size = Vec4f{cluster_half_size, 0.0f};
        node_data.cluster_group_index = group_handle.index;
        node_data.cluster_offset = i;

        list = sys->grid_cluster_lists.insert(list, node_data);
        sys->grid.cells[maybe_cell_index.value()] = list.head;
      }
    }
  }

  ClusterGroupHandle result{};
  result.element_group = group_handle;
  return result;
}

uint32_t foliage_occlusion::total_num_instances(const FoliageOcclusionSystem* sys) {
  uint32_t s{};
  for (auto& cluster : sys->clusters) {
    s += cluster.iteratively_count_num_instances();
  }
  return s;
}

bool foliage_occlusion::renderer_check_is_culled_instance_binary(const FoliageOcclusionSystem* sys,
                                                                 uint32_t maybe_group_handle,
                                                                 uint32_t cluster_offset,
                                                                 uint8_t instance_index) {
  if (maybe_group_handle == ContiguousElementGroupAllocator::invalid_element_group.index) {
    return false;
  } else {
    assert(maybe_group_handle < uint32_t(sys->cluster_group_offsets.size()));
    assert(instance_index < Config::max_num_instances_per_cluster);
    cluster_offset += sys->cluster_group_offsets[maybe_group_handle];
    assert(cluster_offset < uint32_t(sys->clusters.size()));
    return !sys->clusters[cluster_offset].instances[instance_index].is_idle_state();
  }
}

bool foliage_occlusion::renderer_check_is_culled_instance_fade_in_out(const FoliageOcclusionSystem* sys,
                                                                      uint32_t maybe_group_handle,
                                                                      uint32_t cluster_offset,
                                                                      uint8_t instance_index,
                                                                      float* scale01) {
  if (maybe_group_handle == ContiguousElementGroupAllocator::invalid_element_group.index) {
    *scale01 = 1.0f;
    return false;
  } else {
    assert(maybe_group_handle < uint32_t(sys->cluster_group_offsets.size()));
    assert(instance_index < Config::max_num_instances_per_cluster);
    cluster_offset += sys->cluster_group_offsets[maybe_group_handle];
    assert(cluster_offset < uint32_t(sys->clusters.size()));
    const auto& inst = sys->clusters[cluster_offset].instances[instance_index];
    const CullingState state = inst.get_culling_state();
    switch (state) {
      case CullingState::Idle:
        *scale01 = 1.0f;
        return false;
      case CullingState::FadingOut:
        *scale01 = 1.0f - inst.get_transition_fraction();
        return false;
      case CullingState::FullyFadedOut:
      case CullingState::PendingFadeIn:
        *scale01 = 0.0f;
        return true;
      case CullingState::FadingIn:
        *scale01 = inst.get_transition_fraction();
        return false;
      default:
        assert(false);
        return false;
    }
  }
}


void foliage_occlusion::clear_culled(FoliageOcclusionSystem* sys) {
  for (auto& cluster : sys->clusters) {
    for (auto& inst : cluster.instances) {
      inst.set_culling_state(CullingState::Idle);
      inst.set_culled_on_frame_id(0);
    }
  }
  sys->clusters_updated = true;
}

CheckOccludedResult foliage_occlusion::check_occluded(FoliageOcclusionSystem* sys,
                                                      const CheckOccludedParams& params) {
#if ENABLE_DEBUG
  sys->debug_contexts.clear();
#endif
  clear_culled(sys);
  sys->culled_on_frame_id++;

  assert(params.min_intersect_area_fraction >= 0.0f && params.min_intersect_area_fraction <= 1.0f);
  OcclusionParams occlusion_params{};
  occlusion_params.cull_distance_threshold = params.cull_distance_threshold;
  occlusion_params.min_intersect_area_fraction = params.min_intersect_area_fraction;
  occlusion_params.tested_instance_scale = std::max(0.1f, params.tested_instance_scale);
  occlusion_params.max_num_steps = std::max(1, params.max_num_steps);

  CheckOccludedResult result{};
  Stopwatch stopwatch;

  for (auto& cluster : sys->clusters) {
    if (!frustum_aabb_intersect(params.camera_frustum, cluster.get_aabb())) {
      continue;
    }

    result.num_passed_frustum_cull += cluster.iteratively_count_num_instances();

    const bool can_be_culled = cluster_can_be_culled(
      cluster.get_canonical_position(),
      params.camera_position, params.cull_distance_threshold);
    if (!can_be_culled) {
      continue;
    }

    for (auto& inst : cluster.instances) {
#if ENABLE_DEBUG
      OcclusionCheckDebugContext ctx{};
      OcclusionCheckDebugContext* debug_ctx = &ctx;
#else
      OcclusionCheckDebugContext* debug_ctx = nullptr;
#endif
      result.num_newly_tested++;

      auto proj_aabb = cluster_instance_projected_aabb(inst, params.camera_projection_view, 1.0f);
      bool is_occluded = occluded(
        sys, params.camera_position, inst.get_position(), proj_aabb,
        params.camera_projection_view, sys->culled_on_frame_id, occlusion_params, debug_ctx);
      if (is_occluded) {
        inst.set_culling_state(CullingState::FullyFadedOut);
        inst.set_culled_on_frame_id(sys->culled_on_frame_id);
        result.num_newly_occluded++;
      }

#if ENABLE_DEBUG
      sys->debug_contexts.push_back(ctx);
#endif
    }
  }

#ifdef GROVE_DEBUG
  for (auto& cluster : sys->clusters) {
    for (auto& inst : cluster.instances) {
      if (inst.is_sentinel()) {
        break;
      }
      result.total_num_occluded += uint32_t(!inst.is_idle_state());
    }
  }
  assert(result.total_num_occluded == result.num_newly_occluded);
#else
  result.total_num_occluded = result.num_newly_occluded;
#endif

  result.ms = float(stopwatch.delta().count() * 1e3);
  return result;
}

namespace {

float new_transition_fraction(const ClusterInstance& inst, float real_dt, float fade_time) {
  const float frac = inst.get_transition_fraction();
  return clamp01(clamp(frac * fade_time + real_dt, 0.0f, fade_time) / fade_time);
}

} //  anon

CheckOccludedResult foliage_occlusion::update_clusters(FoliageOcclusionSystem* sys, double real_dt,
                                                       const CheckOccludedParams& params) {
  assert(sys->pending_process_indices.size() >= uint32_t(sys->clusters.size()) * Config::max_num_instances_per_cluster);

  sys->culled_on_frame_id++;
  sys->num_pending_process_indices = 0;
  const uint32_t update_id = sys->update_id++;
  const int update_interval = params.interval;

  assert(params.min_intersect_area_fraction >= 0.0f && params.min_intersect_area_fraction <= 1.0f);
  OcclusionParams occlusion_params{};
  occlusion_params.cull_distance_threshold = params.cull_distance_threshold;
  occlusion_params.min_intersect_area_fraction = params.min_intersect_area_fraction;
  occlusion_params.tested_instance_scale = std::max(0.1f, params.tested_instance_scale);
  occlusion_params.max_num_steps = std::max(1, params.max_num_steps);
  sys->occlusion_params = occlusion_params;
  sys->clusters_updated = true;

  CheckOccludedResult result{};
  Stopwatch stopwatch;

  uint32_t cluster_begin{};
  auto cluster_end = uint32_t(sys->clusters.size());

  const float min_time_scale = 0.1f;
  float fade_in_time = 0.25f * std::max(min_time_scale, params.fade_in_time_scale);
  float fade_out_time = 0.25f * std::max(min_time_scale, params.fade_out_time_scale);
  float cull_time = 0.25f * std::max(min_time_scale, params.cull_time_scale);
  const bool disable_check = params.disable_cpu_check;

  const float fade_back_in_distance_threshold = clamp(
    params.fade_back_in_distance_threshold, 0.0f, params.cull_distance_threshold);
  const bool fade_back_in_only_when_below_distance_threshold =
    params.fade_back_in_only_when_below_distance_threshold;

  if (update_interval > 1) {
    uint32_t interval_id = update_id % uint32_t(update_interval);
    auto clusters_per_update = uint32_t(sys->clusters.size() / update_interval);

    cluster_begin = clusters_per_update * interval_id;
    if (interval_id != uint32_t(update_interval) - 1) {
      cluster_end = std::min(cluster_begin + clusters_per_update, cluster_end);
    }

    fade_in_time /= float(update_interval);
    fade_out_time /= float(update_interval);
    cull_time /= float(update_interval);
  }

  for (uint32_t c = cluster_begin; c < cluster_end; c++) {
    auto& cluster = sys->clusters[c];
    if (!frustum_aabb_intersect(params.camera_frustum, cluster.get_aabb())) {
      //  Skip this cluster.
      continue;
    }

    result.num_passed_frustum_cull += cluster.iteratively_count_num_instances();

    //  Only allow cull if distance >= cull_distance_threshold
    const bool can_be_culled = cluster_can_be_culled(
      cluster.get_canonical_position(),
      params.camera_position, params.cull_distance_threshold);
    //  Fade back in if culled and distance < fade_back_in_distance_threshold
    const bool should_fade_back_in = cluster_should_fade_back_in(
      cluster.get_canonical_position(),
      params.camera_position, fade_back_in_distance_threshold);

    for (auto& inst : cluster.instances) {
      if (inst.is_sentinel()) {
        break;
      }

      if (should_fade_back_in) {
        if (inst.get_culling_state() == CullingState::PendingFadeIn) {
          inst.set_culling_state(CullingState::FadingIn);
          inst.set_transition_fraction(0.0f);
        }
      }

      if (inst.get_culling_state() == CullingState::FadingIn) {
        const float fade_t = new_transition_fraction(inst, float(real_dt), fade_in_time);
        inst.set_transition_fraction(fade_t);
        if (fade_t == 1.0f) {
          inst.set_culling_state(CullingState::Idle);
          inst.set_transition_fraction(0.0f);
        }
      } else if (inst.get_culling_state() == CullingState::FadingOut) {
        const float fade_t = new_transition_fraction(inst, float(real_dt), fade_out_time);
        inst.set_transition_fraction(fade_t);
        if (fade_t == 1.0f) {
          inst.set_culling_state(CullingState::FullyFadedOut);
          inst.set_transition_fraction(0.0f);
        }
      } else if (inst.get_culling_state() == CullingState::FullyFadedOut) {
        const float fade_t = new_transition_fraction(inst, float(real_dt), cull_time);
        inst.set_transition_fraction(fade_t);
        if (fade_t == 1.0f) {
          inst.set_culling_state(CullingState::PendingFadeIn);
          inst.set_transition_fraction(0.0f);
        }
      }

      bool check_occlude;
      if (fade_back_in_only_when_below_distance_threshold) {
        check_occlude = inst.is_idle_state() && can_be_culled;
      } else {
        check_occlude = inst.get_culling_state() == CullingState::PendingFadeIn ||
          (inst.is_idle_state() && can_be_culled);
      }
#if 1
      if (check_occlude) {
        ClusterPendingProcessIndices pend{c, uint32_t(&inst - cluster.instances)};
        sys->pending_process_indices[sys->num_pending_process_indices++] = pend;
      }
#endif

      if (check_occlude && !disable_check) {
        result.num_newly_tested++;

        auto proj_aabb = cluster_instance_projected_aabb(inst, params.camera_projection_view, 1.0f);
        bool is_occluded = occluded(
          sys, params.camera_position, inst.get_position(), proj_aabb,
          params.camera_projection_view, sys->culled_on_frame_id, occlusion_params, nullptr);

        if (!is_occluded && inst.get_culling_state() == CullingState::PendingFadeIn) {
          inst.set_culling_state(CullingState::FadingIn);
          inst.set_transition_fraction(0.0f);

        } else if (is_occluded && inst.is_idle_state()) {
          inst.set_culling_state(CullingState::FadingOut);
          inst.set_transition_fraction(0.0f);
          inst.set_culled_on_frame_id(sys->culled_on_frame_id);
        }
      }
    }
  }

  for (auto& cluster : sys->clusters) {
    for (auto& inst : cluster.instances) {
      if (inst.is_sentinel()) {
        break;
      }
      bool is_culled = inst.get_culling_state() == CullingState::FullyFadedOut ||
        inst.get_culling_state() == CullingState::PendingFadeIn;
      result.total_num_occluded += uint32_t(is_culled);
    }
  }

  result.ms = float(stopwatch.delta().count() * 1e3);
  return result;
}

void foliage_occlusion::debug_draw(FoliageOcclusionSystem* sys,
                                   const DebugDrawFoliageOcclusionSystemParams& params) {
  for (auto& ctx : sys->debug_contexts) {
    if (ctx.num_steps > 0) {
      auto first_cell_p = grid_cell_index_to_world_position(sys->grid, ctx.steps[0]);
      auto last_cell_p = grid_cell_index_to_world_position(sys->grid, ctx.steps[ctx.num_steps - 1]);
      float cell_span = (last_cell_p - first_cell_p).length();
      vk::debug::draw_line(ctx.ro, ctx.ro + ctx.rd * cell_span, Vec3f{1.0f, 0.0f, 0.0f});
    }
    for (uint32_t i = 0; i < ctx.num_steps; i++) {
      auto cell_p0 = grid_cell_index_to_world_position(sys->grid, ctx.steps[i]);
      auto cell_p1 = cell_p0 + sys->grid.cell_size;
      vk::debug::draw_aabb3(Bounds3f{cell_p0, cell_p1}, Vec3f{1.0f});
    }
    break;
  }

  for (auto& cluster : sys->clusters) {
    for (auto& inst : cluster.instances) {
      if (inst.is_sentinel()) {
        break;
      } else if (!params.draw_occluded && !inst.is_idle_state()) {
        continue;
      }

      auto scl = inst.get_scale();
      assert(scl.x * scl.y > 0.0f);

      auto color = !inst.is_idle_state() ? Vec3f{0.0f, 0.0f, 1.0f} : Vec3f{1.0f, 0.0f, 0.0f};
      if (ray_cluster_instance_intersect(params.mouse_ro, params.mouse_rd, inst)) {
        color = Vec3f{1.0f, 1.0f, 0.0f};
      } else if (params.colorize_instances) {
        float len = (inst.get_position() * inst.get_normal()).length();
        color *= clamp01(std::sin(len * 18.0f) * 0.5f + 0.5f);
      }

      vk::debug::draw_plane(inst.get_position(), inst.get_right(), inst.get_up(), scl, color);
    }
  }
  if (params.draw_cluster_bounds) {
    for (auto& meta : sys->cluster_meta) {
      float t0{};
      float t1{};
      Vec3f color{};
      if (ray_obb_intersect(params.mouse_ro, params.mouse_rd, meta.src_bounds, &t0, &t1)) {
        color = Vec3f{1.0f};
      }
      vk::debug::draw_obb3(meta.src_bounds, color);
    }
  }
}

GROVE_NAMESPACE_END

#undef ENABLE_DEBUG
