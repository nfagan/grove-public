#include "wall_holes_around_tree_nodes.hpp"
#include "../procedural_tree/collide_with_object.hpp"
#include "geometry.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

arch::WallHole projected_aabb_to_wall_hole(const Bounds2f& proj_aabb, const Vec2f& world_sz,
                                           float curl, float size_scale, float rot = 0.0f) {
  arch::WallHole result{};
  //  @NOTE: `size_scale` is just a hack to get around the fact that windows curl inwards, so the
  //  inner dimensions of the opening are smaller than the specified `scale`.
  auto sz = proj_aabb.size() / world_sz * size_scale;
  auto center = proj_aabb.center() / world_sz;
  result.scale = sz;
  result.off = center;
  result.curl = curl;
  result.rot = rot;
  return result;
}

bool accept_wall_hole(const arch::WallHole& hole) {
  for (int i = 0; i < 2; i++) {
    assert(hole.scale[i] > 0.0f);
    float mn = hole.off[i] - hole.scale[i] * 0.5f;
    float mx = hole.off[i] + hole.scale[i] * 0.5f;
    if (mn < -0.5f || mx > 0.5f) {
      return false;
    }
  }
  return true;
}

void accept_none(const tree::AcceptCollisionComponentBoundsParams& accept_params) {
  *accept_params.num_accepted = 0;
}

void default_accept_wall_holes(const tree::AcceptCollisionComponentBoundsParams& accept_params,
                               const std::function<arch::WallHole(const Bounds2f&)>& make_hole,
                               int max_num_holes, arch::WallHole* dst_holes) {
  struct WallHoleInfo {
    arch::WallHole hole;
    int isle_id;
  };

  DynamicArray<WallHoleInfo, 4> info;
  for (int i = 0; i < accept_params.num_components; i++) {
    int isle_id = accept_params.unique_component_ids[i];
    auto hole = make_hole(accept_params.projected_component_bounds[isle_id]);
    if (accept_wall_hole(hole)) {
      info.push_back({hole, isle_id});
    }
  }

  auto area = [](const arch::WallHole& hole) {
    return hole.scale.x * hole.scale.y;
  };
  std::sort(info.begin(), info.end(), [&](const WallHoleInfo& a, const WallHoleInfo& b) {
    return area(a.hole) > area(b.hole);
  });

  *accept_params.num_accepted = std::min(max_num_holes, int(info.size()));
  for (int i = 0; i < *accept_params.num_accepted; i++) {
    accept_params.accept_component_ids[i] = info[i].isle_id;
    dst_holes[i] = info[i].hole;
  }
}

} //  anon

tree::TreeNodeCollisionWithObjectResult
arch::compute_collision_with_wall(const arch::TreeNodeCollisionWithWallParams& params) {
  const auto& collide_through_params = *params.collide_through_hole_params;

  tree::TreeNodeCollisionWithObjectParams collision_params{};
  collision_params.object_bounds = params.wall_bounds;
  collision_params.src_internodes = params.src_internodes;
  collision_params.num_src_internodes = params.num_src_internodes;
  collision_params.min_colliding_node_diameter = collide_through_params.min_collide_node_diam;
  collision_params.project_forward_dim = collide_through_params.forward_dim;
  collision_params.projected_aabb_scale = collide_through_params.projected_aabb_scale;
  collision_params.prune_initially_rejected = collide_through_params.prune_initially_rejected;

  const auto world_sz = exclude(
    params.wall_bounds.half_size, collide_through_params.forward_dim) * 2.0f;

  const auto make_hole = [&collide_through_params, &world_sz](const Bounds2f& b) {
    return projected_aabb_to_wall_hole(b, world_sz, collide_through_params.hole_curl, 1.0f);
  };

  if (collide_through_params.reject_all_holes) {
    collision_params.accept_collision_component_bounds = accept_none;
  } else {
    collision_params.accept_collision_component_bounds =
      [&](const tree::AcceptCollisionComponentBoundsParams& accept_params) {
        default_accept_wall_holes(
          accept_params, make_hole, params.max_num_accepted_holes, params.accepted_holes);
      };
  }
  return tree::compute_collision_with_object(params.collision_context, collision_params);
}

GROVE_NAMESPACE_END
