#include "growth_on_nodes.hpp"
#include "../cloud/distribute_points.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/DynamicArray.hpp"
#include "grove/math/random.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/util.hpp"
#include "grove/math/Mat3.hpp"
#include "grove/math/intersect.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

template <typename T>
T sdf_obb(const Vec3<T>& qp, const OBB3<T>& obb) {
  auto x0 = obb.position - obb.i * obb.half_size.x;
  auto x1 = obb.position + obb.i * obb.half_size.x;

  auto y0 = obb.position - obb.j * obb.half_size.y;
  auto y1 = obb.position + obb.j * obb.half_size.y;

  auto z0 = obb.position - obb.k * obb.half_size.z;
  auto z1 = obb.position + obb.k * obb.half_size.z;

  auto a = Mat3<T>{obb.i, obb.j, obb.k};
  auto rot_pos = transpose(a);  //  inv(a)
  Vec3<T> rot_ps[6] = {x0, y0, z0, x1, y1, z1};

  T ds[6];
  bool any_outside{};
  for (int i = 0; i < 3; i++) {
    auto v = rot_pos * (qp - rot_ps[i]);
    ds[i] = v[i];
    any_outside |= ds[i] < T(0);
  }

  for (int i = 0; i < 3; i++) {
    auto at = a;
    const Vec3<T> col = at[i];
    at[i] = -col;
    at = transpose(at); //  inv(at)
    auto v = at * (qp - rot_ps[i + 3]);
    ds[i + 3] = v[i];
    any_outside |= ds[i + 3] < T(0);
  }

  if (any_outside) {
    T max_d = std::numeric_limits<T>::lowest();
    for (T d : ds) {
      if (d < T(0)) {
        max_d = std::max(max_d, -d);
      }
    }
    return max_d;
  } else {
    auto elem = *std::min_element(ds, ds + 6);
    return -elem;
  }
}

bool inside_obb(const Vec3f& qp, const OBB3f& obb) {
  return sdf_obb(qp, obb) < 0.0f;
}

void gen_sample_points(Vec2f* dst, int num_samples) {
  Temporary<bool, 128> accept_sample;
  auto* accept = accept_sample.require(num_samples);
  const float r = points::place_outside_radius_default_radius(num_samples, 1.0f);
  points::place_outside_radius<Vec2f, float, 2>(dst, accept, num_samples, r);
}

void offset_x(Vec2f* ps, int num_samples, float off) {
  for (int i = 0; i < num_samples; i++) {
    ps[i].x += off;
    ps[i].x -= 1.0f * float(ps[i].x >= 1.0f);
  }
}

void evaluate_obb_cylinder(const OBB3f& obb, const Vec2f& p2, float expand_r,
                           Vec3f* dst_p3, Vec3f* dst_n3) {
  assert(p2.x >= 0.0f && p2.x <= 1.0f);
  assert(obb.half_size.x == obb.half_size.z);
  float r = obb.half_size.x + expand_r;
  float th = float(two_pi()) * p2.x;
  Vec2f xz = r * Vec2f{std::cos(th), std::sin(th)};

  Vec3f ori_xz = obb.i * xz.x + obb.k * xz.y;
  auto base = ori_xz - obb.j * obb.half_size.y;
  auto p3 = base + obb.j * obb.half_size.y * 2.0f * p2.y;
  p3 = p3 + obb.position;
  auto n3 = normalize(ori_xz);

  *dst_p3 = p3;
  *dst_n3 = n3;
}

Vec3f to_surface_position(const Vec3<uint16_t>& p, const Bounds3f& aabb) {
  auto v = clamp_each(to_vec3f(p) / float(0xffff), Vec3f{}, Vec3f{1.0f});
  return lerp(v, aabb.min, aabb.max);
}

int min_dist_ignoring_indices(const Vec3f& p, const tree::InternodeSurfaceEntry* entries,
                              int num_entries, const Bounds3f& aabb,
                              const int* ignore, int num_ignore) {
  float mn{infinityf()};
  int mn_index{-1};

  for (int i = 0; i < num_entries; i++) {
    bool consider_entry{true};
    for (int j = 0; j < num_ignore; j++) {
      if (ignore[j] == i) {
        consider_entry = false;
        break;
      }
    }

    if (consider_entry) {
      const float dist = (p - to_surface_position(entries[i].p, aabb)).length();
      if (dist < mn) {
        mn = dist;
        mn_index = i;
      }
    }
  }

  return mn_index;
}

template <typename T>
Vec3<T> cast_v3(const Vec3f& v) {
  return Vec3<T>{T(v.x), T(v.y), T(v.z)};
}

Vec3<uint16_t> quantize_surface_position(const Vec3f& p, const Bounds3f& aabb) {
  constexpr auto den = float(0xffff);
  auto p01 = clamp_each(aabb.to_fraction(p), Vec3f{}, Vec3f{1.0f}) * den;
  return cast_v3<uint16_t>(p01);
}

Vec3<uint8_t> quantize_normalized(const Vec3f& n) {
  constexpr auto de = float(0xff);
  auto n01 = clamp_each(n * 0.5f + 0.5f, Vec3f{}, Vec3f{1.0f});
  return cast_v3<uint8_t>(n01 * de);
}

Vec3f decode_normalized(const Vec3<uint8_t>& v) {
  auto vf = (to_vec3f(v) / 255.0f) * 2.0f - 1.0f;
  auto len = vf.length();
  return len > 0.0f ? vf / len : vf;
}

bool ray_internodes_intersect(const Vec3f& ro, const Vec3f& rd, const OBB3f* node_bounds,
                              int num_nodes, float* t, int* i, float r_scale = 1.0f) {
  float min_t{infinityf()};
  int hit_i{-1};
  for (int ni = 0; ni < num_nodes; ni++) {
    auto& obb = node_bounds[ni];
    auto frame = Mat3f{obb.i, obb.j, obb.k};
    float t0;
    float r = obb.half_size.x * r_scale;
    float l = obb.half_size.y;
    if (ray_capped_cylinder_intersect(ro, rd, frame, obb.position, r, l, &t0) && t0 < min_t) {
      hit_i = ni;
      min_t = t0;
    }
  }
  if (hit_i >= 0) {
    *t = min_t;
    *i = hit_i;
    return true;
  } else {
    return false;
  }
}

bool ray_internodes_intersect(const Vec3f& ro, const Vec3f& rd,
                              const OBB3f* node_bounds, int num_nodes) {
  float ignore_t;
  int ignore_i;
  return ray_internodes_intersect(ro, rd, node_bounds, num_nodes, &ignore_t, &ignore_i);
}

tree::InternodeSurfaceEntry
make_internode_surface_entry(const Vec3<uint16_t>& p, const Vec3<uint8_t>& n,
                             const Vec3<uint8_t>& up, int node_index) {
  tree::InternodeSurfaceEntry result{};
  result.p = p;
  result.n = n;
  result.up = up;
  result.node_index = node_index;
  return result;
}

tree::SpiralAroundNodesEntry
make_spiral_around_nodes_entry(const Vec3f& p, const Vec3f& n, int ni) {
  return {p, n, ni};
}

Vec3f spiral_around_nodes_initial_position(const tree::SpiralAroundNodesParams& params,
                                           const OBB3f* node_bounds, int ni) {
  if (params.use_manual_init_p) {
    return params.init_p;
  }

  auto& node_obb = node_bounds[ni];
  auto p = node_obb.position - node_obb.j * node_obb.half_size.y;
  Vec3f init_dir = node_obb.k;
  if (params.randomize_initial_position) {
    const float rand_theta = urandf() * 2.0f * pif();
    init_dir = node_obb.i * std::cos(rand_theta) + node_obb.k * std::sin(rand_theta);
  }
  p += init_dir * (node_obb.half_size.x + params.n_off);
  return p;
}

template <int N>
void gather_lateral_children(int ni, const int* med, const int* lat, const OBB3f* bounds,
                             int num_medial, DynamicArray<int, N>& dst_lat,
                             DynamicArray<OBB3f, N>& dst_bounds) {
  dst_lat.clear();
  dst_bounds.clear();
  while (ni != -1) {
    int lat_ni = lat[ni];
    int num_med{};
    while (lat_ni != -1) {
      dst_lat.push_back(lat_ni);
      dst_bounds.push_back(bounds[lat_ni]);
      if (num_med++ >= num_medial) {
        break;
      }
      lat_ni = med[lat_ni];
    }
    ni = med[ni];
  }
  assert(dst_lat.size() < N && "Alloc required.");
}

} //  anon

int tree::place_points_on_internodes(const PlacePointsOnInternodesParams& params) {
  assert(params.bounds_radius_offset >= 0.0f);
  Temporary<Vec2f, 128> store_src_sample_points;
  Temporary<Vec2f, 128> store_curr_sample_points;

  auto* src_sample_points = store_src_sample_points.require(params.points_per_node);
  auto* curr_sample_points = store_curr_sample_points.require(params.points_per_node);

  gen_sample_points(src_sample_points, params.points_per_node);

  int dst_i{};
  for (int i = 0; i < params.num_nodes; i++) {
    std::copy(src_sample_points, src_sample_points + params.points_per_node, curr_sample_points);
    offset_x(curr_sample_points, params.points_per_node, urandf());
    auto& obb = params.node_bounds[i];
    const auto up = quantize_normalized(obb.j);

    for (int j = 0; j < params.points_per_node; j++) {
      auto& qp = curr_sample_points[j];
      Vec3f p3;
      Vec3f n3;
      evaluate_obb_cylinder(obb, qp, params.bounds_radius_offset, &p3, &n3);

      bool accept{true};
      for (int k = 0; k < params.num_nodes; k++) {
        if (k != i && inside_obb(p3, params.node_bounds[k])) {
          accept = false;
          break;
        }
      }

      if (accept) {
        auto quant_p = quantize_surface_position(p3, params.node_aabb);
        auto quant_n = quantize_normalized(n3);
        params.dst_entries[dst_i++] = make_internode_surface_entry(quant_p, quant_n, up, i);
      }
    }
  }

  return dst_i;
}

int tree::sample_points_on_internodes(const SamplePointsOnInternodesParams& params) {
  assert((!params.prefer_entry_up_axis && !params.prefer_entry_down_axis) ||
         params.prefer_entry_up_axis != params.prefer_entry_down_axis);
  assert(!params.stop_at_leaf || params.node_meta);

  if (params.num_samples == 0 || params.num_entries == 0) {
    return 0;
  }

  auto* entries = params.entries;
  int ci = params.init_entry_index;
  Vec3f curr_p = to_surface_position(entries[ci].p, params.node_aabb);

  auto step_axis = params.step_axis;
  if (params.prefer_entry_up_axis) {
    step_axis = entries[ci].decode_up();
  } else if (params.prefer_entry_down_axis) {
    step_axis = -entries[ci].decode_up();
  }

  auto* entry_indices = params.entry_indices;
  auto* samples = params.dst_samples;

  int sample_index{};
  entry_indices[sample_index] = ci;
  samples[sample_index] = curr_p;
  sample_index++;

  for (int i = 0; i < params.num_samples-1; i++) {
    auto qp = curr_p + step_axis * params.target_step_length;
    int next_ci = min_dist_ignoring_indices(
      qp, entries, params.num_entries, params.node_aabb, entry_indices, sample_index);

    if (next_ci < 0) {
      break;
    }

    const auto& next_entry = entries[next_ci];
    if (params.stop_at_leaf && params.node_meta[next_entry.node_index].is_leaf) {
      break;
    }

    auto next_p = to_surface_position(next_entry.p, params.node_aabb);
    auto next_dist = (next_p - curr_p).length();
    if (next_dist > params.max_step_length) {
      break;
    }

    curr_p = next_p;
    if (params.prefer_entry_up_axis) {
      step_axis = next_entry.decode_up();
    } else if (params.prefer_entry_down_axis) {
      step_axis = -next_entry.decode_up();
    }
    ci = next_ci;
    samples[sample_index] = curr_p;
    entry_indices[sample_index] = ci;
    sample_index++;
  }

  return sample_index;
}

tree::SpiralAroundNodesResult
tree::spiral_around_nodes2(const OBB3f* node_bounds, const int* medial_children,
                           const int* lateral_children, const int* parents, int num_nodes,
                           const SpiralAroundNodesParams& params, int max_num_entries,
                           SpiralAroundNodesEntry* dst_entries) {
  SpiralAroundNodesResult result{};

  if (max_num_entries == 0 || params.init_ni >= num_nodes) {
    return result;
  }

  //  Only check for intersection with lateral children of current axis.
  DynamicArray<OBB3f, 64> lateral_axes_bounds;
  DynamicArray<int, 64> lateral_axes;
  if (!params.disable_node_intersect_check) {
    gather_lateral_children(
      params.init_ni, medial_children, lateral_children,
      node_bounds, params.max_num_medial_lateral_intersect_bounds,
      lateral_axes, lateral_axes_bounds);
  }

  const float n_off = params.n_off;
  int ni = params.init_ni;
  auto p = spiral_around_nodes_initial_position(params, node_bounds, ni);

  int num_entries{};
  bool reached_end{};
  for (int i = 0; i < max_num_entries; i++) {
    const auto& node_obb = node_bounds[ni];

    Vec3f up = node_obb.j;
    auto proj_p = up * dot((p - node_obb.position), up) + node_obb.position;
    auto z = normalize(p - proj_p);
    auto x = cross(z, up);
    //  project back to surface
    p = proj_p + z * (node_obb.half_size.x + n_off);
    dst_entries[num_entries++] = make_spiral_around_nodes_entry(p, z, ni);

    float stp = params.step_size;
    stp += 0.5f * params.step_size_randomness * urand_11f() * stp;

    float theta = params.theta;
    theta += params.theta_randomness * urand_11f() * pif() * 0.5f;
    const Vec2f dir{std::cos(theta), std::sin(theta)};

    Vec3f incr_right = x * stp * dir.x;
    Vec3f incr_up = up * stp * dir.y;
    auto proj_next_dist = dot(((p + incr_up) - node_obb.position), up);

    if (dir.y >= 0.0f) {
      if (proj_next_dist >= node_obb.half_size.y) {
        if (medial_children[ni] >= 0) {
          ni = medial_children[ni];
        } else {
          reached_end = true;
          break;
        }
      }
    } else {
      if (proj_next_dist <= -node_obb.half_size.y) {
        if (parents[ni] >= 0) {
          ni = parents[ni];
        } else {
          reached_end = true;
          break;
        }
      }
    }

    auto next_p = p + incr_right + incr_up;

    if (!params.disable_node_intersect_check) {
      const Vec3f ro = p;
      Vec3f rd = next_p - p;
      const float dist_to_next = rd.length();
      rd /= dist_to_next;

      int hit_ni{};
      float hit_t{};
      const bool hit_lat = ray_internodes_intersect(
        ro, rd, lateral_axes_bounds.data(), int(lateral_axes_bounds.size()), &hit_t, &hit_ni);

      if (hit_lat && hit_t < dist_to_next) {
        next_p = ro + rd * hit_t;
        ni = lateral_axes[hit_ni];
        //  Jump to lateral axis.
        gather_lateral_children(
          ni, medial_children, lateral_children, node_bounds,
          params.max_num_medial_lateral_intersect_bounds, lateral_axes, lateral_axes_bounds);
      }
    }

    p = next_p;
  }

  result.next_p = p;
  result.next_ni = ni;
  result.num_entries = num_entries;
  result.reached_axis_end = reached_end;
  return result;
}

int tree::spiral_around_nodes(const OBB3f* node_bounds, const int* medial_children,
                              const int* parents, int num_nodes,
                              const SpiralAroundNodesParams& params, int max_num_entries,
                              SpiralAroundNodesEntry* dst_entries) {
  if (max_num_entries == 0 || params.init_ni >= num_nodes) {
    return 0;
  }

  const float n_off = params.n_off;
  int ni = params.init_ni;

  Vec3f p;
  if (params.use_manual_init_p) {
    p = params.init_p;
  } else {
    auto& node_obb = node_bounds[ni];
    p = node_obb.position - node_obb.j * node_obb.half_size.y;
    Vec3f init_dir = node_obb.k;
    if (params.randomize_initial_position) {
      const float rand_theta = urandf() * 2.0f * pif();
      init_dir = node_obb.i * std::cos(rand_theta) + node_obb.k * std::sin(rand_theta);
    }
    p += init_dir * (node_obb.half_size.x + n_off);
  }

  int num_entries{};
  for (int i = 0; i < max_num_entries; i++) {
    const auto& node_obb = node_bounds[ni];

    Vec3f up = node_obb.j;
    auto proj_p = up * dot((p - node_obb.position), up) + node_obb.position;
    auto z = normalize(p - proj_p);
    auto x = cross(z, up);
    //  project back to surface
    p = proj_p + z * (node_obb.half_size.x + n_off);
    dst_entries[num_entries++] = make_spiral_around_nodes_entry(p, z, ni);

    float stp = params.step_size;
    stp += 0.5f * params.step_size_randomness * urand_11f() * stp;

    float theta = params.theta;
    theta += params.theta_randomness * urand_11f() * pif() * 0.5f;
    const Vec2f dir{std::cos(theta), std::sin(theta)};

    Vec3f incr_right = x * stp * dir.x;
    Vec3f incr_up = up * stp * dir.y;
    auto proj_next_dist = dot(((p + incr_up) - node_obb.position), up);

    if (dir.y >= 0.0f) {
      if (proj_next_dist >= node_obb.half_size.y) {
        if (medial_children[ni] >= 0) {
          ni = medial_children[ni];
        } else {
          break;
        }
      }
    } else {
      if (proj_next_dist <= -node_obb.half_size.y) {
        if (parents[ni] >= 0) {
          ni = parents[ni];
        } else {
          break;
        }
      }
    }

    auto next_p = p + incr_right + incr_up;

    if (!params.disable_node_intersect_check) {
      const Vec3f ro = p;
      Vec3f rd = next_p - p;
      const float dist_to_next = rd.length();
      rd /= dist_to_next;

      int hit_ni{};
      float hit_t{};
      if (ray_internodes_intersect(ro, rd, node_bounds, num_nodes, &hit_t, &hit_ni) &&
          hit_t < dist_to_next) {
        next_p = ro + rd * hit_t;
        ni = hit_ni;
      }
    }

    p = next_p;
  }

  return num_entries;
}

int tree::downsample_spiral_around_nodes_entries(SpiralAroundNodesEntry* entries, int num_entries,
                                                 const OBB3f* node_bounds, int num_nodes,
                                                 int num_steps) {
  assert(num_steps > 0);
  int src_ei{};
  int dst_ei{};

  while (src_ei < num_entries) {
    entries[dst_ei++] = entries[src_ei];
    auto& p0 = entries[src_ei].p;

    int dsi = src_ei + num_steps;
    while (dsi < num_entries && dsi > src_ei) {
      auto& p1 = entries[dsi].p;
      if (ray_internodes_intersect(p0, normalize(p1 - p0), node_bounds, num_nodes)) {
        --dsi;
      } else {
        break;
      }
    }

    src_ei = std::max(dsi, src_ei + 1);
  }

  return dst_ei;
}

int tree::keep_spiral_until_first_node_intersection(const SpiralAroundNodesEntry* entries,
                                                    int num_entries, const OBB3f* node_bounds,
                                                    int num_nodes) {
  if (num_entries == 0) {
    return 0;
  }

  int num_kept{1};
  while (num_kept < num_entries) {
    int p0i = num_kept - 1;
    int p1i = num_kept;
    auto& p0 = entries[p0i].p;
    auto& p1 = entries[p1i].p;

    auto rd = p1 - p0;
    const float dist = rd.length();
    if (dist > 0.0f) {
      rd /= dist;
      int ni{};
      float t{};
      if (ray_internodes_intersect(p0, rd, node_bounds, num_nodes, &t, &ni, 0.75f) && t < dist) {
        break;
      }
    }
    ++num_kept;
  }

  return num_kept;
}

Vec3f tree::InternodeSurfaceEntry::decode_normal() const {
  return decode_normalized(n);
}

Vec3f tree::InternodeSurfaceEntry::decode_up() const {
  return decode_normalized(up);
}

GROVE_NAMESPACE_END
