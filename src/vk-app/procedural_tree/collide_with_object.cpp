#include "collide_with_object.hpp"
#include "render.hpp"
#include "utility.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

struct TreeNodeCollideParams {
  float min_diam;
  TreeNodeCollisionEntry* dst_entries;
  int max_num_entries;
};

struct TreeBoundsCollisionResult {
  int num_collided;
};

struct ProjectCollidingBoundsResult {
  int num_accepted_bounds;
  bool any_rejected;
};

Bounds2f exclude(const Bounds3f& a, int dim) {
  return Bounds2f{exclude(a.min, dim), exclude(a.max, dim)};
}

TreeBoundsCollisionResult find_colliding_internodes(const Internode* inodes,
                                                    int num_internodes,
                                                    const OBB3f& target_bounds,
                                                    const TreeNodeCollideParams& params) {
  std::vector<int> axes;
  if (num_internodes > 0) {
    axes.push_back(0);
  }

  TreeBoundsCollisionResult result{};
  while (!axes.empty()) {
    const int ax = axes.back();
    axes.pop_back();
    int ni = ax;
    while (ni != -1) {
      auto& node = inodes[ni];
      if (node.diameter < params.min_diam) {
        break;
      }
      auto obb = tree::internode_obb(node);
      if (obb_obb_intersect(target_bounds, obb) &&
          result.num_collided < params.max_num_entries) {
        TreeNodeCollisionEntry entry;
        entry.internode_index = ni;
        params.dst_entries[result.num_collided++] = entry;
      }
      if (node.has_lateral_child()) {
        axes.push_back(node.lateral_child);
      }
      ni = node.medial_child;
    }
  }
  return result;
}

ProjectCollidingBoundsResult project_colliding_bounds(const OBB3f* obbs, int num_obbs,
                                                      const OBB3f& target_obb, int forward_dim,
                                                      float proj_scale,
                                                      ProjectCollidingBoundsEntry* out) {
  ProjectCollidingBoundsResult result{};
  for (int i = 0; i < num_obbs; i++) {
    auto proj_res = obb_intersect_to_projected_aabb(target_obb, obbs[i], forward_dim, true);
    ProjectCollidingBoundsEntry entry{};
    if (proj_res.accept) {
      entry.accepted = true;
      entry.bounds = scale(exclude(proj_res.aabb, forward_dim), Vec2f{proj_scale});
      result.num_accepted_bounds++;
    } else {
      result.any_rejected = true;
    }
    out[i] = entry;
  }
  return result;
}

void gather_collided_internode_obbs(const tree::Internode* internodes,
                                    const TreeNodeCollisionEntry* entries, int num_entries,
                                    OBB3f* dst) {
  for (int i = 0; i < num_entries; i++) {
    dst[i] = tree::internode_obb(internodes[entries[i].internode_index]);
  }
}

void compute_aabb_components(Bounds2f* bounds, int* isles, int num_bounds) {
  for (int i = 0; i < num_bounds; i++) {
    isles[i] = i;
  }
  for (int i = 0; i < num_bounds; i++) {
    if (isles[i] != i) {
      continue;
    }
    while (true) {
      bool any_modified{};
      for (int j = 0; j < num_bounds; j++) {
        if (isles[j] == isles[i]) {
          continue;
        }
        if (aabb_aabb_intersect_closed(bounds[i], bounds[j])) {
          isles[j] = isles[i];
          bounds[i] = union_of(bounds[i], bounds[j]);
          any_modified = true;
        }
      }
      if (!any_modified) {
        break;
      }
    }
  }
}

int compute_unique_isles(int* isles, int count) {
  std::sort(isles, isles + count);
  return int(std::unique(isles, isles + count) - isles);
}

void set_accepted_axes(const tree::Internode* src, int num_src,
                       //  size = num_collision_entries
                       const TreeNodeCollisionEntry* collision_entries,
                       //  size = num_collision_entries
                       const int* isle_ids,
                       int num_collision_entries,
                       const int* accepted_isle_ids,
                       int num_accepted_isles,
                       bool* dst) {
  const auto did_collide = [&](int ni, int* ind) -> bool {
    for (int i = 0; i < num_collision_entries; i++) {
      if (collision_entries[i].internode_index == ni) {
        *ind = i;
        return true;
      }
    }
    return false;
  };
  const auto did_accept = [&](int isle_id) {
    for (int i = 0; i < num_accepted_isles; i++) {
      if (accepted_isle_ids[i] == isle_id) {
        return true;
      }
    }
    return false;
  };

  std::fill(dst, dst + num_src, false);
  std::vector<int> axes;
  if (num_src > 0) {
    axes.push_back(0);
  }
  while (!axes.empty()) {
    const int axis_root = axes.back();
    axes.pop_back();
    int src_self_ind = axis_root;
    while (src_self_ind != -1) {
      bool accept = true;
      int entry_ind;
      if (did_collide(src_self_ind, &entry_ind)) {
        accept = did_accept(isle_ids[entry_ind]);
      }
      dst[src_self_ind] = accept;
      if (!accept) {
        break;
      }
      auto& src_node = src[src_self_ind];
      if (src_node.has_lateral_child()) {
        axes.push_back(src_node.lateral_child);
      }
      src_self_ind = src_node.medial_child;
    }
  }
}

TreeNodeCollideParams make_collide_params(TreeNodeCollisionWithObjectContext* ctx,
                                          int num_src_internodes,
                                          float min_collide_node_diam) {
  TreeNodeCollideParams result{};
  result.dst_entries = ctx->collision_entries.get();
  result.max_num_entries = num_src_internodes;
  result.min_diam = min_collide_node_diam;
  return result;
}

struct CollideWithObjectResult {
  TreeBoundsCollisionResult find_result;
  ProjectCollidingBoundsResult project_result;
};

CollideWithObjectResult collide_with_object(TreeNodeCollisionWithObjectContext* ctx,
                                            const tree::Internode* src_inodes,
                                            int num_src_inodes,
                                            const TreeNodeCollisionWithObjectParams& params) {
  TreeNodeCollideParams collide_params = make_collide_params(
    ctx,
    num_src_inodes,
    params.min_colliding_node_diameter);

  auto find_res = find_colliding_internodes(
    src_inodes,
    num_src_inodes,
    params.object_bounds, collide_params);

  gather_collided_internode_obbs(
    src_inodes, ctx->collision_entries.get(),
    find_res.num_collided, ctx->internode_bounds.get());

  auto* proj_entries = ctx->project_bounds_entries.get();
  auto proj_res = project_colliding_bounds(
    ctx->internode_bounds.get(),
    find_res.num_collided,
    params.object_bounds,
    params.project_forward_dim,
    params.projected_aabb_scale,
    proj_entries);

  {
    //  Copy accepted bounds.
    auto* proj_bounds = ctx->projected_bounds.get();
    int nb{};
    for (int i = 0; i < find_res.num_collided; i++) {
      if (proj_entries[i].accepted) {
        proj_bounds[nb++] = proj_entries[i].bounds;
      }
    }
    assert(nb == proj_res.num_accepted_bounds);
  }

  CollideWithObjectResult result;
  result.find_result = find_res;
  result.project_result = proj_res;
  return result;
}

} //  anon

TreeNodeCollisionWithObjectResult
tree::compute_collision_with_object(TreeNodeCollisionWithObjectContext* ctx,
                                    const TreeNodeCollisionWithObjectParams& params) {
  reserve(ctx, params.num_src_internodes);

  ProjectCollidingBoundsEntry* proj_entries = ctx->project_bounds_entries.get();
  TreeNodeCollisionEntry* collide_entries = ctx->collision_entries.get();
  bool* accept_internode = ctx->accept_internode.get();
  const tree::Internode* src_inodes = params.src_internodes;
  int num_src_inodes = params.num_src_internodes;

  auto collide_res = collide_with_object(ctx, src_inodes, num_src_inodes, params);
  bool using_aux_src{};
  if (params.prune_initially_rejected && collide_res.project_result.any_rejected) {
    using_aux_src = true;
    //  Prune the rejected axes, then try again.
    std::fill(accept_internode, accept_internode + num_src_inodes, true);
    for (int i = 0; i < collide_res.find_result.num_collided; i++) {
      if (!proj_entries[i].accepted) {
        const int src_ind = collide_entries[i].internode_index;
        assert(accept_internode[src_ind]);
        accept_internode[src_ind] = false;
      }
    }

    auto* aux_src_inodes = ctx->aux_src_internodes.get();
    auto* aux_dst_to_src = ctx->aux_dst_to_src.get();
    num_src_inodes = prune_rejected_axes(
      src_inodes,
      accept_internode,
      num_src_inodes,
      aux_src_inodes,
      aux_dst_to_src);
    src_inodes = aux_src_inodes;
    collide_res = collide_with_object(ctx, src_inodes, num_src_inodes, params);
  }

  const int num_accepted_bounds = collide_res.project_result.num_accepted_bounds;
  const bool any_rejected = collide_res.project_result.any_rejected;

  auto* isle_ids = ctx->bounds_component_ids.get();
  auto* unique_isle_ids = ctx->unique_bounds_component_ids.get();
  auto* proj_bounds = ctx->projected_bounds.get();
  compute_aabb_components(proj_bounds, isle_ids, num_accepted_bounds);
  memcpy(unique_isle_ids, isle_ids, num_accepted_bounds * sizeof(int));
  const int num_unique_isles = compute_unique_isles(unique_isle_ids, num_accepted_bounds);

  TreeNodeCollisionWithObjectResult result{};
  if (!any_rejected) {
    assert(num_accepted_bounds == collide_res.find_result.num_collided);
    int num_accepted_isles{};
    auto* accepted_isle_ids = ctx->accept_bounds_component_ids.get();

    AcceptCollisionComponentBoundsParams accept_params{};
    accept_params.projected_component_bounds = proj_bounds;
    accept_params.unique_component_ids = unique_isle_ids;
    accept_params.num_components = num_unique_isles;
    accept_params.accept_component_ids = accepted_isle_ids;
    accept_params.num_accepted = &num_accepted_isles;
    params.accept_collision_component_bounds(accept_params);

    set_accepted_axes(
      src_inodes,
      num_src_inodes,
      collide_entries,
      isle_ids,
      num_accepted_bounds,
      accepted_isle_ids, num_accepted_isles, accept_internode);

    auto* dst_inodes = ctx->dst_internodes.get();
    auto* dst_to_src = ctx->dst_to_src.get();
    const int num_dst = prune_rejected_axes(
      src_inodes,
      accept_internode,
      num_src_inodes,
      dst_inodes,
      dst_to_src);

    if (using_aux_src) {
      for (int i = 0; i < num_dst; i++) {
        dst_to_src[i] = ctx->aux_dst_to_src[dst_to_src[i]];
      }
    }

    result.dst_to_src = dst_to_src;
    result.dst_internodes = dst_inodes;
    result.num_dst_internodes = num_dst;
    result.num_accepted_bounds_components = num_accepted_isles;
    result.collided_bounds = ctx->internode_bounds.get();
    result.num_collided_bounds = collide_res.find_result.num_collided;
  }

#ifdef GROVE_DEBUG
  for (int i = 0; i < result.num_dst_internodes; i++) {
    const int src_ind = result.dst_to_src[i];
    assert(src_ind >= 0 &&
           src_ind < params.num_src_internodes &&
           params.src_internodes[src_ind].id == result.dst_internodes[i].id);
  }
#endif

  return result;
}

void tree::reserve(TreeNodeCollisionWithObjectContext* ctx, int num_inodes) {
  if (ctx->num_reserved_instances < num_inodes) {
    ctx->collision_entries = std::make_unique<TreeNodeCollisionEntry[]>(num_inodes);
    ctx->project_bounds_entries = std::make_unique<ProjectCollidingBoundsEntry[]>(num_inodes);
    ctx->internode_bounds = std::make_unique<OBB3f[]>(num_inodes);
    ctx->projected_bounds = std::make_unique<Bounds2f[]>(num_inodes);
    ctx->aux_src_internodes = std::make_unique<Internode[]>(num_inodes);
    ctx->aux_dst_to_src = std::make_unique<int[]>(num_inodes);
    ctx->bounds_component_ids = std::make_unique<int[]>(num_inodes);
    ctx->unique_bounds_component_ids = std::make_unique<int[]>(num_inodes);
    ctx->accept_bounds_component_ids = std::make_unique<int[]>(num_inodes);
    ctx->dst_to_src = std::make_unique<int[]>(num_inodes);
    ctx->dst_internodes = std::make_unique<Internode[]>(num_inodes);
    ctx->accept_internode = std::make_unique<bool[]>(num_inodes);
    ctx->num_reserved_instances = num_inodes;
  }
}

GROVE_NAMESPACE_END
