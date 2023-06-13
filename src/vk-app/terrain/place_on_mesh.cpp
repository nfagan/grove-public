#include "place_on_mesh.hpp"
#include "grove/common/common.hpp"
#include "grove/visual/image_process.hpp"
#include "grove/math/frame.hpp"
#include "grove/math/intersect.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace mesh;

Vec3f mean_of_vertices(const uint32_t* pi, const Vec3f* elements) {
  Vec3f r{};
  for (int i = 0; i < 3; i++) {
    r += elements[pi[i]];
  }
  return r / 3.0f;
}

} //  anon

GenOBB3OriginDirectionResult
mesh::gen_obb3_origin_direction(const GenOBB3OriginDirectionParams& params) {
  GenOBB3OriginDirectionResult result{};

  const auto p0 = params.image_sample_center_position - params.image_sample_size * 0.5f;
  int num_sampled{};

  Vec3f sampled_normals{};
  Vec3f sampled_positions{};

  for (int i = 0; i < params.num_samples; i++) {
    auto p_sample = p0 + params.image_sample_positions[i] * params.image_sample_size;
    int r;
    int c;
    image::uv_to_ij_clamped(p_sample.x, p_sample.y, params.ti_im_rows, params.ti_im_cols, &r, &c);

    int ti = params.ti_im[image::ij_to_linear(r, c, params.ti_im_cols, 1)];
    if (ti > 0) {
      ti--;
      sampled_normals += mean_of_vertices(params.tris + ti * 3, params.ns);
      sampled_positions += mean_of_vertices(params.tris + ti * 3, params.ps);
      num_sampled++;
    }
  }

  if (num_sampled > 0) {
    result.success = true;
    result.n = normalize(sampled_normals / float(num_sampled));
    result.p = sampled_positions / float(num_sampled);

    Vec3f i;
    Vec3f j;
    Vec3f k;
    make_coordinate_system_y(result.n, &i, &j, &k);
    result.frame = Mat3f{i, j, k};
  }

  return result;
}

PlacePointsWithinOBB3Result
mesh::place_points_within_obb3(const PlacePointsWithinOBB3Params& params) {
  PlacePointsWithinOBB3Result result{};

  Vec2f box_size_xz{params.obb3_size.x, params.obb3_size.z};
  const auto& surface_n = params.obb3_frame[1];

  float max_t = -infinityf();
  float min_t = infinityf();

  for (int i = 0; i < params.num_samples; i++) {
    Vec2f pp = 0.5f * (params.sample_positions[i] * 2.0f - 1.0f) * box_size_xz;
    const Vec3f pp3 = Vec3f{pp.x, 0.0f, pp.y};

    auto p_xz = params.obb3_frame * pp3 + params.surface_p;
    auto rc_p = p_xz + surface_n * params.obb3_size.y;
    Ray ray{};
    ray.origin = rc_p;
    ray.direction = -surface_n;

    int hit_tri{};
    float hit_t{};
    const bool hit = ray_triangle_intersect(
      ray, params.ps, params.tris, int(params.num_tris), &hit_tri, &hit_t);
    if (hit && hit_t > 0.0f) {
      auto hit_p = ray(hit_t);
      PlacePointsWithinOBB3Entry hit_entry{};
      hit_entry.position = hit_p;
      params.result_entries[result.num_hits++] = hit_entry;
      max_t = std::max(max_t, hit_t);
      min_t = std::min(min_t, hit_t);
    }
  }

  result.min_ray_t = min_t;
  result.max_ray_t = max_t;
  return result;
}

OBB3f mesh::gen_obb3(const Vec3f& surface_p, const Mat3f& frame, const Vec3f& size,
                     float ray_min_t, float ray_max_t) {
  const auto& surface_n = frame[1];

  auto top_p = surface_p + surface_n * size.y;
  auto max_p = top_p - surface_n * ray_max_t;  //  farthest surface point from top of box
  auto min_p = top_p - surface_n * ray_min_t;  //  closest surface point to top of box
  auto tip_p = min_p + size.y * surface_n;
  auto box_p = lerp(0.5f, max_p, tip_p);
  auto size_y = (tip_p - max_p).length();

  OBB3f result;
  result.position = box_p;
  result.i = frame[0];
  result.j = frame[1];
  result.k = frame[2];
  result.half_size = Vec3f{size.x * 0.5f, size_y * 0.5f, size.z * 0.5f};
  return result;
}

void mesh::rasterize_bounds(const Bounds2f* bounds, const float* zs, int num_bounds, int rows,
                            int cols, int* im, float* depths) {
  std::fill(im, im + rows * cols, 0);
  std::fill(depths, depths + rows * cols, -infinityf());

  for (int i = 0; i < num_bounds; i++) {
    auto& p0 = bounds[i].min;
    auto& p1 = bounds[i].max;
    assert(all(ge(p1, p0)) && all(ge(p0, Vec2f{})) && all(le(p0, Vec2f{1.0f})) &&
           all(ge(p1, Vec2f{})) && all(le(p1, Vec2f{1.0f})));

    int r0;
    int c0;
    image::uv_to_ij_clamped(p0.x, p0.y, rows, cols, &r0, &c0);
    int r1;
    int c1;
    image::uv_to_ij_clamped(p1.x, p1.y, rows, cols, &r1, &c1);

    const float z = zs[i];
    for (int r = r0; r <= r1; r++) {
      for (int c = c0; c <= c1; c++) {
        const int ind = image::ij_to_linear(r, c, cols, 1);
        if (z > depths[ind]) {
          depths[ind] = z;
          im[ind] = i + 1;  //  note, +1
        }
      }
    }
  }
}

void mesh::project_vertices_to_aabbs(const uint32_t* tris, uint32_t num_tris, const Vec3f* ps,
                                     uint32_t num_ps, const Vec3f& cube_face_normal,
                                     Bounds2f* dst_bounds, float* collapsed_depths) {
  Bounds3f tot_bounds;
  union_of(ps, int(num_ps), &tot_bounds.min, &tot_bounds.max);

  int exclude_dim{};
  float exclude_sign{1.0f};
  for (int i = 0; i < 3; i++) {
    if (cube_face_normal[i] != 0.0f) {
      assert(cube_face_normal[i] == 1.0f || cube_face_normal[i] == -1.0f);
      exclude_dim = i;
      exclude_sign = cube_face_normal[i] < 0.0f ? -1.0f : 1.0f;
      break;
    }
  }

  for (uint32_t i = 0; i < num_tris; i++) {
    auto& p0 = ps[tris[i * 3 + 0]];
    auto& p1 = ps[tris[i * 3 + 1]];
    auto& p2 = ps[tris[i * 3 + 2]];

    Vec3f un_ps[3] = {
      tot_bounds.to_fraction(p0),
      tot_bounds.to_fraction(p1),
      tot_bounds.to_fraction(p2)
    };
    Bounds3f bounds;
    union_of(un_ps, 3, &bounds.min, &bounds.max);

    float d0 = (un_ps[0][exclude_dim] * 2.0f - 1.0f) * exclude_sign;
    float d1 = (un_ps[1][exclude_dim] * 2.0f - 1.0f) * exclude_sign;
    float d2 = (un_ps[2][exclude_dim] * 2.0f - 1.0f) * exclude_sign;
    collapsed_depths[i] = std::max(std::max(d0, d1), d2);

    dst_bounds[i] = Bounds2f{
      exclude(bounds.min, exclude_dim),
      exclude(bounds.max, exclude_dim)
    };
  }
}

GROVE_NAMESPACE_END
