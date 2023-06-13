#include "node_mesh.hpp"
#include "render.hpp"
#include "vk-app/procedural_flower/geometry.hpp"
#include "grove/common/common.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/frame.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

struct TransformData {
  Vec3f instance_position;
  Vec3f child_instance_position;
  Vec2f direction;
  Vec2f child_direction;
  float radius;
  float child_radius;
};

struct Vertex {
  Vec3f position;
  Vec3f normal;
  Vec2f uv;
};

Vec3f shape_function(const Vec2f& p, const Vec2f& num_points_xz) {
  float x_dim = std::floor(num_points_xz.x * 0.5f);
  float x_ind = p.x == x_dim ? 0.0f : p.x + x_dim;
  float theta = (2.0f * pif()) * (x_ind / (num_points_xz.x - 1.0f));
  float y = p.y / (num_points_xz.y - 1.0f);
  return Vec3f{std::cos(theta), y, std::sin(theta)};
}

Vec3f instance_scale(float radius) {
  return Vec3f{radius, 1.0f, radius};
}

Vec3f mat3_mul_vec3(const Vec3f& i, const Vec3f& j, const Vec3f& k, const Vec3f& v) {
  return i * v.x + j * v.y + k * v.z;
}

Vertex transform(const Vec2f& p, const Vec2f& num_points_xz, const TransformData& data,
                 const Vec3f& offset) {
  Vec3f s = shape_function(p, num_points_xz);
  const float y = s.y;
  Vec3f s0 = Vec3f{s.x, 0.0, s.z};
  Vec3f s_base = s0 * instance_scale(data.radius);
  Vec3f s_tip = s0 * instance_scale(data.child_radius);

  Vec3f is;
  Vec3f js;
  Vec3f ks;
  make_coordinate_system_y(spherical_to_cartesian(data.direction), &is, &js, &ks);

  Vec3f ic;
  Vec3f jc;
  Vec3f kc;
  make_coordinate_system_y(spherical_to_cartesian(data.child_direction), &ic, &jc, &kc);

  Vec3f s0n = normalize(s0);
  Vec3f n_base = normalize(mat3_mul_vec3(is, js, ks, s0n));
  Vec3f n_tip = normalize(mat3_mul_vec3(ic, jc, kc, s0n));
  Vec3f n = lerp(y, n_base, n_tip);

  if (n.length() == 0.0f) {
    n = ConstVec3f::positive_y;
  }

  Vec3f p_base = mat3_mul_vec3(is, js, ks, s_base) + data.instance_position;
  Vec3f p_tip = mat3_mul_vec3(ic, jc, kc, s_tip) + data.child_instance_position;
  Vec3f tp = lerp(y, p_base, p_tip) + offset;

  Vertex result{};
  result.position = tp;
  result.normal = n;
  result.uv = {}; //  @TODO
  return result;
}

TransformData make_transform_data(const tree::Internode& node,
                                  const tree::ChildRenderData& child,
                                  float scale) {
  TransformData result{};
  result.instance_position = node.render_position;
  result.child_instance_position = child.position;
  result.direction = node.spherical_direction();
  result.child_direction = child.direction;
  result.radius = node.diameter * 0.5f * scale;
  result.child_radius = child.radius * scale;
  return result;
}

} //  anon

size_t tree::compute_num_indices_in_node_mesh(const Vec2<int>& geom_sizes_xz,
                                              uint32_t num_internodes) {
  return 6 * (geom_sizes_xz.x - 1) * (geom_sizes_xz.y - 1) * num_internodes;
}

size_t tree::compute_num_vertices_in_node_mesh(const Vec2<int>& geom_sizes_xz,
                                               uint32_t num_internodes) {
  assert(geom_sizes_xz.x > 1 && geom_sizes_xz.y > 1);
  auto verts_per_node = uint32_t(geom_sizes_xz.x * geom_sizes_xz.y);
  return verts_per_node * num_internodes;
}

void tree::make_node_mesh(const tree::Internode* internodes, uint32_t num_internodes,
                          const Vec2<int>& geom_sizes_xz, const MakeNodeMeshParams& params,
                          float* out_v, uint16_t* out_i) {
  const auto [npx, npz] = geom_sizes_xz;
  const Vec2f num_points_xz{float(npx), float(npz)};

  std::vector<float> grid = make_reflected_grid_indices(npx, npz);
  std::vector<uint16_t> grid_indices = triangulate_reflected_grid(npx, npz);

  size_t vi{};
  size_t num_grid_verts = grid.size() / 2;
  size_t num_grid_indices = grid_indices.size();

  for (uint32_t ni = 0; ni < num_internodes; ni++) {
    auto& node = internodes[ni];
    auto child = get_child_render_data(
      node,
      internodes,
      params.allow_branch_to_lateral_child,
      params.leaf_tip_radius);

    auto tform_data = make_transform_data(node, child, params.scale);

    //  Copy indices before adding offset.
    for (size_t i = 0; i < num_grid_indices; i++) {
      size_t dst_ind = grid_indices[i] + vi;
      assert(dst_ind < (1u << 16u));
      *out_i++ = uint16_t(dst_ind);
    }

    for (size_t i = 0; i < num_grid_verts; i++) {
      Vec2f p2{grid[i * 2], grid[i * 2 + 1]};
      auto vert = transform(p2, num_points_xz, tform_data, params.offset);
      for (int j = 0; j < 3; j++) {
        *out_v++ = vert.position[j];
      }
      for (int j = 0; j < 3; j++) {
        *out_v++ = vert.normal[j];
      }
      if (params.include_uv) {
        for (int j = 0; j < 2; j++) {
          *out_v++ = vert.uv[j];
        }
      }
    }

    vi += num_grid_verts;
  }
}

using AmplifyParams = tree::AmplifyGeometryOrientedAtInternodesParams;
void tree::amplify_geometry_oriented_at_internodes(const AmplifyParams& params) {
  if (params.num_src_vertices == 0) {
    return;
  }

  uint32_t num_target_src = params.num_src_vertices * params.num_elements;
  uint32_t num_elements_process = std::min(
    num_target_src, params.max_num_dst_vertices) / params.num_src_vertices;

  const auto* src = static_cast<const unsigned char*>(params.src);
  auto* dst = static_cast<unsigned char*>(params.dst);

  const auto has_normal = bool(params.src_normal_byte_offset);
  const auto has_uv = bool(params.src_uv_byte_offset);

  uint32_t dst_vi{};
  for (uint32_t i = 0; i < num_elements_process; i++) {
    const Vec3f translation = params.positions[i];
    const Vec3f up = params.directions[i];

    Vec3f ni;
    Vec3f nj;
    Vec3f nk;
    make_coordinate_system_y(up, &ni, &nj, &nk);

    for (uint32_t vi = 0; vi < params.num_src_vertices; vi++) {
      assert(dst_vi < params.max_num_dst_vertices);
      auto* src_v = src + params.src_byte_stride * vi;
      auto* dst_v = dst + params.dst_byte_stride * dst_vi;

      Vec3f p;
      memcpy(&p, src_v + params.src_position_byte_offset, sizeof(Vec3f));
      p = mat3_mul_vec3(ni, nj, nk, p * params.scale) + translation;

      Vec3f n;
      if (has_normal) {
        memcpy(&n, src_v + params.src_normal_byte_offset.value(), sizeof(Vec3f));
        n = mat3_mul_vec3(ni, nj, nk, n);
      }

      Vec2f uv;
      if (has_uv) {
        memcpy(&uv, src_v + params.src_uv_byte_offset.value(), sizeof(Vec2f));
      }

      memcpy(dst_v + params.dst_position_byte_offset, &p, sizeof(Vec3f));
      if (has_normal) {
        memcpy(dst_v + params.dst_normal_byte_offset, &n, sizeof(Vec3f));
      }
      if (has_uv) {
        memcpy(dst_v + params.dst_uv_byte_offset, &uv, sizeof(Vec2f));
      }

      dst_vi++;
    }
  }
}

GROVE_NAMESPACE_END
