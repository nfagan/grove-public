#pragma once

#include "../architecture/ray_project.hpp"
#include "components.hpp"

namespace grove::tree {

struct ProjectNodesResult {
  std::vector<ProjectRayResultEntry> project_ray_results;
  tree::Internodes internodes;
};

struct PostProcessProjectedNodesResult {
  tree::Internodes internodes;
  std::vector<Vec3f> true_mesh_normals;
  std::vector<Vec3f> processed_mesh_normals;
};

struct PostProcessProjectedNodesParams {
  int prune_intersecting_internode_queue_size{2};
  bool reset_internode_diameter{true};
  int smooth_diameter_adjacent_count{3};
  int smooth_normals_adjacent_count{11};
  bool offset_internodes_by_radius{true};
  bool constrain_lateral_child_diameter{true};
  bool preserve_source_internode_ids{};
  Optional<float> max_diameter;
};

ProjectNodesResult
project_internodes_onto_mesh(const uint32_t* tris, uint32_t num_tris,
                             const Vec3f* ps, uint32_t ti,
                             const Vec3<double>& src_p, const Internodes& internodes,
                             double initial_theta_offset, double length_scale,
                             const ProjectRayEdgeIndices* edge_indices,
                             const ray_project::NonAdjacentConnections* non_adjacent_connections);

PostProcessProjectedNodesResult
post_process_projected_internodes(Internodes src,
                                  const tree::SpawnInternodeParams& spawn_params,
                                  const Vec3f* mesh_normals,
                                  const ProjectRayResultEntry* proj_ray_results,
                                  uint32_t num_proj_ray_results,
                                  const PostProcessProjectedNodesParams& params);

uint32_t find_triangle_containing_min_y_point(const uint32_t* tris, uint32_t num_tris,
                                              const Vec3f* ps, uint32_t num_ps);
uint32_t find_largest_triangles_containing_lowest_y(const uint32_t* tris, uint32_t num_tris,
                                                    const Vec3f* ps, uint32_t num_ps,
                                                    uint32_t* out, uint32_t max_num_out);

void extract_mesh_normals_at_projected_internodes(const Vec3f* ns,
                                                  const ProjectRayResultEntry* ray_proj_results,
                                                  uint32_t num_ray_proj_results,
                                                  const Internodes& internodes,
                                                  Vec3f* dst_ns);
void smooth_extracted_mesh_normals(const Internodes& nodes, Vec3f* ns, int adjacent_count);
void smooth_internode_diameters(Internodes& nodes, int adjacent_count);
Internodes prune_intersecting(const Internodes& internodes, int queue_size, float custom_obb_diam);
void offset_internodes_by_normal_and_radius(Internodes& internodes, const Vec3f* ns);

}