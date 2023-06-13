#include "leaf_geometry.hpp"
#include "grove/visual/distribute_along_axis.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/common/common.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

std::vector<float> make_base_plane_geometry() {
  auto plane = geometry::quad_positions(true, 0.0f);
  auto plane_inds = geometry::quad_indices();

  const std::array<int, 3> perms[3]{
    {0, 1, 2},
    {0, 2, 1},
    {2, 1, 0},
  };

  std::vector<float> result;
  for (auto& pi : perms) {
    for (uint16_t i : plane_inds) {
      float* p0 = plane.data() + i * 3;
      Vec3f p{p0[0], p0[1], p0[2]};
      Vec3f n{0.0f, 0.0f, 1.0f};
      Vec2f uv = Vec2f{p.x, p.y} * 0.5f + 0.5f;
      float vert[8] = {
        p[pi[0]], p[pi[1]], p[pi[2]],
        n[pi[0]], n[pi[1]], n[pi[2]],
        uv.x, uv.y};
      result.insert(result.end(), vert, vert + 8);
    }
  }

  return result;
}

} //  anon

tree::LeafGeometryResult
tree::make_planes_distributed_along_axis(const LeafGeometryParams& geom_params) {
  const int vert_size = 8;
  auto base_geom = make_base_plane_geometry();

  const int num_steps = 6;
  const int instances_per_step = 2;
  const Vec3f step_axis = ConstVec3f::positive_y;
  const float step_length = 0.5f;
  const Vec3f max_rotations{pif(), pif(), pif()};
//  const Vec3f max_rotations{};

  const int num_verts = int(base_geom.size()) / vert_size;
  assert(num_verts * 8 == int(base_geom.size()));
  const int new_num_verts = num_verts * num_steps * instances_per_step;
  std::vector<float> new_geom(new_num_verts * vert_size);

  VertexBufferDescriptor buffer_desc;
  buffer_desc.add_attribute(AttributeDescriptor::float3(0));
  buffer_desc.add_attribute(AttributeDescriptor::float3(1));
  buffer_desc.add_attribute(AttributeDescriptor::float2(2));
  geometry::DistributeAlongAxisBufferIndices buffer_indices{
    0, Optional<int>(1), Optional<int>(2)};

  geometry::DistributeAlongAxisParams params{};
  params.step_axis = step_axis;
  params.num_steps = num_steps;
  params.step_length = step_length;
  params.base_axis_offset = Vec3f{0.0f, -1.0f, 0.0f};
  params.step = [&](int si) -> geometry::DistributeAlongAxisStep {
    const float step_frac = float(si) / float(num_steps-1);
    geometry::DistributeAlongAxisStep step{};
    step.max_rotation = max_rotations;
    step.radius = std::pow(step_frac, geom_params.tip_radius_power) * geom_params.tip_radius;
    step.num_instances = instances_per_step;
    step.scale = geom_params.step_scale;
    float scale_rand = 0.2f;
    float theta_rand = 0.05f;
    step.scale_randomness_limits = Vec2f{-scale_rand, scale_rand};
    step.theta_randomness_limits = Vec2f{-pif() * theta_rand, pif() * theta_rand};
    return step;
  };

  size_t num_verts_written = geometry::distribute_along_axis(
    base_geom.data(), buffer_desc, base_geom.size() * sizeof(float), buffer_indices,
    new_geom.data(), buffer_desc, new_geom.size() * sizeof(float), buffer_indices,
    params);
  assert(int(num_verts_written) == new_num_verts);
  (void) num_verts_written;

  LeafGeometryResult result;
  result.descriptor = std::move(buffer_desc);
  result.data = std::move(new_geom);
  return result;
}

tree::LeafGeometryParams tree::LeafGeometryParams::make_original() {
  LeafGeometryParams result{};
  result.tip_radius = 2.0f;
  result.tip_radius_power = 2.0f;
  result.step_scale = Vec3f{0.75f};
  return result;
}

tree::LeafGeometryParams tree::LeafGeometryParams::make_flattened() {
  LeafGeometryParams result{};
  result.tip_radius = 1.0f;
  result.tip_radius_power = 2.0f;
  result.step_scale = Vec3f{0.25f, 0.75f, 0.25f} * 1.5f;
//  result.step_scale = Vec3f{0.25f, 0.75f * 1.5f, 0.25f} * 1.0f;
  return result;
}

GROVE_NAMESPACE_END
