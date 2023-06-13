#include "fit_bounds.hpp"
#include "render.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/math/frame.hpp"
#include "grove/math/Mat3.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/bounds.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

Vec3f min_size(const OBB3f* bounds, int num_bounds) {
  Vec3f min_size{infinityf()};
  for (int i = 0; i < num_bounds; i++) {
    min_size = min(bounds[i].half_size, min_size);
  }
  return min_size;
}

Vec3f centroid(const OBB3f* bounds, int num_bounds) {
  if (num_bounds == 0) {
    return {};
  } else {
    Vec3f cent{};
    for (int i = 0; i < num_bounds; i++) {
      cent += bounds[i].position;
    }
    cent /= float(num_bounds);
    return cent;
  }
}

Vec3f mean_axis_j(const OBB3f* bounds, int num_bounds) {
  if (num_bounds == 0) {
    return Vec3f{0.0f, 1.0f, 0.0f};
  } else {
    Vec3f y{};
    for (int i = 0; i < num_bounds; i++) {
      y += bounds[i].j;
    }
    y /= float(num_bounds);
    const float len = y.length();
    if (len > 0.0f) {
      y /= len;
    } else {
      y = Vec3f{0.0f, 1.0f, 0.0f};
    }
    return y;
  }
}

OBB3f fit_around(const OBB3f* bounds, int num_bounds, const Vec3f& y) {
  if (num_bounds == 0) {
    return OBB3f::axis_aligned(Vec3f{}, Vec3f{});
  }

  OBB3f fit{};
  fit.position = centroid(bounds, num_bounds);
  make_coordinate_system_y(y, &fit.i, &fit.j, &fit.k);

  Mat3f m{fit.i, fit.j, fit.k};
  auto m_inv = inverse(m);

  Vec3f sz{-1.0f};
  for (int i = 0; i < num_bounds; i++) {
    Vec3f verts[8];
    gather_vertices(bounds[i], verts);
    for (auto& vert : verts) {
      auto t = m_inv * (vert - fit.position);
      sz = max(sz, abs(t) * 2.0f);
    }
  }

  fit.half_size = sz * 0.5f;
  return fit;
}

Vec3f get_axis(const OBB3f* bounds, int num_bounds, const bounds::FitOBBsAroundAxisParams& params) {
  if (params.use_preferred_axis) {
    assert(params.preferred_axis.length() > 0.0f);
    return params.preferred_axis;
  } else {
    return mean_axis_j(bounds, num_bounds);
  }
}

} //  anon

int bounds::fit_obbs_around_axis(const FitOBBsAroundAxisParams& params) {
  if (params.num_bounds == 0) {
    return 0;
  }

  auto* dst_bounds = params.dst_bounds;
  int num_dst_bounds{};

  if (params.test_type == FitOBBsAroundAxisParams::TestType::None) {
    auto axis = get_axis(params.axis_bounds, params.num_bounds, params);
    dst_bounds[num_dst_bounds++] = fit_around(params.axis_bounds, params.num_bounds, axis);
    return num_dst_bounds;
  }

  int beg{};
  int end{2};
  auto curr_bounds = params.axis_bounds[0];

  while (end <= params.num_bounds) {
    auto axis = get_axis(params.axis_bounds + beg, end - beg, params);
    auto min_s = min_size(params.axis_bounds + beg, end - beg);
    auto fit = fit_around(params.axis_bounds + beg, end - beg, axis);

    bool reject{};
    if (params.test_type == FitOBBsAroundAxisParams::TestType::SizeRatio) {
      auto size_ratio = fit.half_size / min_s;
      reject = any(gt(size_ratio, params.max_size_ratio));

    } else if (params.test_type == FitOBBsAroundAxisParams::TestType::MaxSize) {
      reject = any(gt(fit.half_size, params.max_size));

    } else {
      assert(false);
    }

    if (reject) {
      dst_bounds[num_dst_bounds++] = curr_bounds;
      beg = end - 1;
      curr_bounds = params.axis_bounds[beg];
    } else {
      curr_bounds = fit;
    }
    ++end;
  }

  assert(num_dst_bounds < params.num_bounds);
  dst_bounds[num_dst_bounds++] = curr_bounds;
  return num_dst_bounds;
}

int bounds::fit_aabbs_around_axes_radius_threshold_method(
  const tree::Internode* nodes, const Mat3f* node_frames, int num_nodes,
  int min_medial, int max_medial, float xz_thresh,
  Bounds3f* dst_bounds, int* assigned_to_bounds) {
  //
  if (num_nodes == 0) {
    return 0;
  }

  Temporary<int, 2048> store_root_indices;
  auto* root_indices = store_root_indices.require(num_nodes);

  std::fill(assigned_to_bounds, assigned_to_bounds + num_nodes, -1);

  auto root_half_sz = Vec3f{nodes[0].radius(), nodes[0].length * 0.5f, nodes[0].radius()};
  assigned_to_bounds[0] = 0;
  dst_bounds[0] = {-root_half_sz, root_half_sz};
  root_indices[0] = 0;

  int num_dst_bounds = 1;

  Temporary<int, 2048> store_stack;
  auto* stack = store_stack.require(num_nodes);
  int si{};
  stack[si++] = 0;

  while (si > 0) {
    int ni = stack[--si];

    int num_medial{};
    while (ni != -1) {
      auto& self = nodes[ni];

      if (self.has_lateral_child()) {
        stack[si++] = self.lateral_child;
      }

      if (assigned_to_bounds[ni] == -1) {
        assert(self.parent >= 0 && assigned_to_bounds[self.parent] != -1);
        const int candidate_bi = assigned_to_bounds[self.parent];
        auto& candidate = dst_bounds[candidate_bi];

        const int par_root = root_indices[self.parent];
        auto inv_root_frame = transpose(node_frames[par_root]);

        const auto& root_node = nodes[par_root];
        auto root_p = root_node.position + root_node.direction * root_node.length * 0.5f;

        auto self_trans = (self.position + self.direction * self.length * 0.5f) - root_p;
        auto frame_rel = inv_root_frame * node_frames[ni];

        OBB3f trans_obb;
        trans_obb.half_size = Vec3f{self.radius(), self.length * 0.5f, self.radius()};
        trans_obb.position = self_trans;
        trans_obb.i = frame_rel[0];
        trans_obb.j = frame_rel[1];
        trans_obb.k = frame_rel[2];

        auto trans_aabb = obb3_to_aabb(trans_obb);
        auto candidate_aabb = union_of(candidate, trans_aabb);
        auto trans_size = trans_aabb.size();
        auto size_delta = candidate_aabb.size() - trans_size;
        float max_delta_xz = std::max(1e-3f, std::max(size_delta.x, size_delta.z));

        bool accept = (max_delta_xz < xz_thresh || num_medial < min_medial) && num_medial < max_medial;
        if (accept) {
          candidate = candidate_aabb;
          assigned_to_bounds[ni] = candidate_bi;
          root_indices[ni] = par_root;
        } else {
          const int bi = num_dst_bounds++;
          assigned_to_bounds[ni] = bi;
          root_indices[ni] = ni;
          dst_bounds[bi] = {-trans_obb.half_size, trans_obb.half_size};
        }
      }

      ni = self.medial_child;
      num_medial++;
    }
  }

  for (int i = 0; i < num_dst_bounds; i++) {
    dst_bounds[i] = {};
  }

  for (int i = 0; i < num_nodes; i++) {
    const int bi = assigned_to_bounds[i];
    assert(bi != -1);
    dst_bounds[bi] = union_of(dst_bounds[bi], obb3_to_aabb(internode_obb(nodes[i])));
  }

  return num_dst_bounds;
}

namespace {

Bounds3f fit_axis(const tree::Internode* nodes, int* src, int n, int i, int* assigned_to_bounds) {
  assert(*src != -1);
  Bounds3f result;
  int ct{};
  while (*src != -1) {
    auto& node = nodes[*src];
    assigned_to_bounds[*src] = i;
    result = union_of(result, obb3_to_aabb(tree::internode_obb(node)));
    *src = node.medial_child;
    if (++ct == n) {
      break;
    }
  }
  return result;
}

} //  anon

int bounds::fit_aabbs_around_axes_only_medial_children_method(
  const tree::Internode* nodes, int num_nodes, int interval,
  Bounds3f* dst_bounds, int* assigned_to_bounds) {
  //
  if (num_nodes == 0) {
    return 0;
  }

  Temporary<int, 2048> store_stack;
  auto* stack = store_stack.require(num_nodes);
  int si{};
  stack[si++] = 0;

  int n{};
  while (si > 0) {
    int ni = stack[--si];
    int ar = ni;
    while (ar != -1) {
      if (nodes[ar].has_lateral_child()) {
        stack[si++] = nodes[ar].lateral_child;
      }
      ar = nodes[ar].medial_child;
    }
    while (ni != -1) {
      dst_bounds[n] = fit_axis(nodes, &ni, interval, n, assigned_to_bounds);
      n++;
    }
  }

  return n;
}

GROVE_NAMESPACE_END