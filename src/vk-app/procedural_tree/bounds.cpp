#include "bounds.hpp"
#include "../bounds/accel_insert.hpp"
#include "radius_limiter.hpp"
#include "fit_bounds.hpp"
#include "components.hpp"
#include "render.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/math/constants.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

bool is_leaf_type(const bounds::Element* el, const InsertInternodeBoundsParams& params) {
  return el->tag == params.leaf_element_tag.id;
}

bool is_tree_type(const bounds::Element* el, const InsertInternodeBoundsParams& params) {
  return el->tag == params.tree_element_tag.id;
}

bool permit_internode_intersection(const bounds::Element* el,
                                   const InsertInternodeBoundsParams& params) {
  if (is_leaf_type(el, params)) {
    //  Allow internodes to intersect other leaves.
    return true;
  } else {
    //  Only allow the nodes of the same tree to self-intersect.
    return is_tree_type(el, params) && el->parent_id == params.tree_element_id.id;
  }
}

bool permit_leaf_intersection(const bounds::Element* el,
                              const InsertInternodeBoundsParams& params) {
  return is_leaf_type(el, params) || is_tree_type(el, params);
}

bounds::InsertBoundsParams prepare_params(const InsertInternodeBoundsParams& src) {
  bounds::InsertBoundsParams result{};
  result.accel = src.accel;
  result.bounds = src.bounds;
  result.inserted = src.inserted;
  result.dst_element_ids = src.dst_element_ids;
  result.num_bounds = src.num_bounds;
  return result;
}

bounds::RadiusLimiterElement to_radius_limiter_element(const OBB3f& obb,
                                                       bounds::RadiusLimiterAggregateID aggregate,
                                                       bounds::RadiusLimiterElementTag tag) {
  bounds::RadiusLimiterElement result{};
  result.i = obb.i;
  result.j = obb.j;
  result.k = obb.k;
  result.p = obb.position;
  result.half_length = obb.half_size.y;
  result.radius = std::max(obb.half_size.x, obb.half_size.z);
  result.tag = tag;
  result.aggregate_id = aggregate;
  return result;
}

} //  anon

int tree::insert_internode_bounds(const InsertInternodeBoundsParams& params) {
  bounds::InsertBoundsParams insert_params = prepare_params(params);
  insert_params.permit_intersection = [&params](const bounds::Element* el) {
    return permit_internode_intersection(el, params);
  };
  insert_params.make_element = [&params](bounds::ElementID el_id, const OBB3f& obb) {
    return bounds::make_element(
      obb,
      el_id.id,
      params.tree_element_id.id,
      params.tree_element_tag.id);
  };
  return bounds::insert_bounds(insert_params);
}

int tree::insert_leaf_bounds(const InsertInternodeBoundsParams& params) {
  bounds::InsertBoundsParams insert_params = prepare_params(params);
  insert_params.permit_intersection = [&params](const bounds::Element* el) {
    return permit_leaf_intersection(el, params);
  };
  insert_params.make_element = [&params](bounds::ElementID el_id, const OBB3f& obb) {
    return bounds::make_element(
      obb,
      el_id.id,
      params.tree_element_id.id,
      params.leaf_element_tag.id);
  };
  return bounds::insert_bounds(insert_params);
}

int tree::prune_intersecting_radius_limiter(const PruneIntersectingRadiusLimiterParams& params) {
  if (params.num_nodes == 0) {
    return 0;
  }

  assert(params.root_index >= 0 && params.root_index < params.num_nodes);

  const auto roots_tag = *params.roots_tag;
  const auto tree_tag = *params.tree_tag;
  const auto aggregate_id = *params.aggregate_id;
  assert(roots_tag != tree_tag);

  Temporary<OBB3f, 1024> store_src_bounds;
  auto* src_bounds = store_src_bounds.require(params.num_nodes);

  Temporary<OBB3f, 1024> store_dst_bounds;
  auto* dst_bounds = store_dst_bounds.require(params.num_nodes);

  Temporary<int, 1024> store_axes;
  int* axes = store_axes.require(params.num_nodes);

  int num_pending_axes{};
  axes[num_pending_axes++] = params.root_index;

  int num_inserted_bounds{};
  while (num_pending_axes > 0) {
    int num_src_bounds{};

    const int ar = axes[--num_pending_axes];
    const bool first_axis = ar == params.root_index;
    int ni = ar;
    while (ni != -1) {
      auto& node = params.nodes[ni];
      src_bounds[num_src_bounds++] = internode_obb(node);
      ni = node.medial_child;
    }

    bounds::FitOBBsAroundAxisParams fit_params{};
    fit_params.axis_bounds = src_bounds;
    fit_params.num_bounds = num_src_bounds;
    fit_params.test_type = bounds::FitOBBsAroundAxisParams::TestType::SizeRatio;
    fit_params.max_size_ratio = Vec3f{2.0f, infinityf(), 2.0f};
    fit_params.dst_bounds = dst_bounds;
    if (first_axis && params.lock_root_node_direction) {
      fit_params.preferred_axis = params.locked_root_node_direction;
      fit_params.use_preferred_axis = true;
    }
    const int num_fit = bounds::fit_obbs_around_axis(fit_params);

    bool reject_axis{};
    for (int i = 0; i < num_fit; i++) {
      auto el = to_radius_limiter_element(dst_bounds[i], aggregate_id, tree_tag);
      if (bounds::intersects_other_tag(params.lim, el.to_obb(el.radius), roots_tag)) {
        reject_axis = true;
        break;
      }
    }

    if (reject_axis) {
      continue;
    }

    ni = ar;
    while (ni != -1) {
      auto& node = params.nodes[ni];
      params.accept_node[ni] = true;
      ni = node.medial_child;
      if (node.has_lateral_child()) {
        axes[num_pending_axes++] = node.lateral_child;
      }
    }

    for (int i = 0; i < num_fit; i++) {
      auto el = to_radius_limiter_element(dst_bounds[i], aggregate_id, tree_tag);
      params.inserted_elements[num_inserted_bounds++] = insert(params.lim, el);
    }
  }

  return num_inserted_bounds;
}

GROVE_NAMESPACE_END
