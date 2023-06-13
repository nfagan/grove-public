#pragma once

#include "../bounds/common.hpp"

namespace grove::bounds {

struct RadiusLimiter;
struct RadiusLimiterElementTag;
struct RadiusLimiterAggregateID;
struct RadiusLimiterElementHandle;

}

namespace grove::tree {

struct Internode;

struct InsertInternodeBoundsParams {
  bounds::Accel* accel;
  bounds::ElementID tree_element_id;
  bounds::ElementTag tree_element_tag;
  bounds::ElementTag leaf_element_tag;
  const OBB3f* bounds;
  bool* inserted; //  size = `num_bounds`
  bounds::ElementID* dst_element_ids; //  size = `num_bounds`
  int num_bounds;
};

[[nodiscard]] int insert_internode_bounds(const InsertInternodeBoundsParams& params);
[[nodiscard]] int insert_leaf_bounds(const InsertInternodeBoundsParams& params);

struct PruneIntersectingRadiusLimiterParams {
  const tree::Internode* nodes;
  int num_nodes;
  int root_index;
  bool lock_root_node_direction;
  Vec3f locked_root_node_direction;
  bounds::RadiusLimiter* lim;
  const bounds::RadiusLimiterAggregateID* aggregate_id;
  const bounds::RadiusLimiterElementTag* roots_tag;
  const bounds::RadiusLimiterElementTag* tree_tag;
  bool* accept_node;                                      //  size = num_nodes
  bounds::RadiusLimiterElementHandle* inserted_elements;  //  size = num_nodes
};

[[nodiscard]] int prune_intersecting_radius_limiter(const PruneIntersectingRadiusLimiterParams& params);

}