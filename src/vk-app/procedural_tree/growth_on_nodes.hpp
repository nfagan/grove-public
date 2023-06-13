#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/OBB3.hpp"
#include "grove/math/Bounds3.hpp"

namespace grove::tree {

struct InternodeSurfaceEntry {
  Vec3f decode_normal() const;
  Vec3f decode_up() const;

  Vec3<uint16_t> p;
  Vec3<uint8_t> n;
  Vec3<uint8_t> up;
  int node_index;
};

struct PlacePointsOnInternodesParams {
  Bounds3f node_aabb;
  const OBB3f* node_bounds;
  float bounds_radius_offset;
  int num_nodes;
  int points_per_node;
  InternodeSurfaceEntry* dst_entries; //  size >= num_nodes * points_per_node
};

struct SamplePointsOnInternodesNodeMetaData {
  bool is_leaf;
};

struct SamplePointsOnInternodesParams {
  Bounds3f node_aabb;

  const InternodeSurfaceEntry* entries;
  int* entry_indices;  //  size = num_entries
  int num_entries;
  int init_entry_index;

  //  size = number of internodes, can be null
  const SamplePointsOnInternodesNodeMetaData* node_meta;
  bool stop_at_leaf;

  Vec3f step_axis;
  float target_step_length;
  float max_step_length;
  bool prefer_entry_up_axis;
  bool prefer_entry_down_axis;

  int num_samples;
  Vec3f* dst_samples;
};

struct SpiralAroundNodesParams {
  Vec3f init_p;
  bool use_manual_init_p;
  int init_ni;
  float n_off;
  float theta;
  float theta_randomness;
  float step_size;
  float step_size_randomness;
  bool randomize_initial_position;
  bool disable_node_intersect_check;
  int max_num_medial_lateral_intersect_bounds;
};

struct SpiralAroundNodesEntry {
  Vec3f p;
  Vec3f n;
  int node_index;
};

struct SpiralAroundNodesResult {
  int num_entries;
  bool reached_axis_end;
  Vec3f next_p;
  int next_ni;
};

int place_points_on_internodes(const PlacePointsOnInternodesParams& params);
int sample_points_on_internodes(const SamplePointsOnInternodesParams& params);
int spiral_around_nodes(const OBB3f* node_bounds, const int* medial_children,
                        const int* parents, int num_nodes,
                        const SpiralAroundNodesParams& params, int max_num_entries,
                        SpiralAroundNodesEntry* dst_entries);
SpiralAroundNodesResult
spiral_around_nodes2(const OBB3f* node_bounds, const int* medial_children,
                     const int* lateral_children, const int* parents, int num_nodes,
                     const SpiralAroundNodesParams& params, int max_num_entries,
                     SpiralAroundNodesEntry* dst_entries);
int downsample_spiral_around_nodes_entries(SpiralAroundNodesEntry* entries, int num_entries,
                                           const OBB3f* node_bounds, int num_nodes, int num_steps);
int keep_spiral_until_first_node_intersection(const SpiralAroundNodesEntry* entries, int num_entries,
                                              const OBB3f* node_bounds, int num_nodes);

}