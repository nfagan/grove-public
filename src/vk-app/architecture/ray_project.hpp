#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/triangle_search.hpp"
#include <cstdint>
#include <vector>

namespace grove {

namespace ray_project {
struct NonAdjacentConnections;
}

using ProjectRayEdgeIndices = tri::EdgeToIndex<uint32_t>;

struct ProjectRayResultEntry {
  Vec3<double> entry_p;
  Vec3<double> exit_p;
  uint32_t ti;
  uint32_t tri[3];
  double theta;
  bool required_flip;
};

struct ProjectRayResult {
  std::vector<ProjectRayResultEntry> entries;
  double traversed_length;
  bool completed;
};

struct ProjectRayNextIteration {
  uint32_t tri[3];
  uint32_t ti;
  Vec3<double> p;
  double ray_theta;
};

ProjectRayResult project_ray_onto_mesh(const uint32_t* tris, uint32_t num_tris, const Vec3f* ps,
                                       const uint32_t src_tri[3], uint32_t src_ti,
                                       const Vec3<double>& src_p, double ray_theta, double ray_len,
                                       const ProjectRayEdgeIndices* edge_indices,
                                       const ray_project::NonAdjacentConnections* non_adjacent_connections);
ProjectRayResult project_ray_onto_mesh(const uint32_t* tris, uint32_t num_tris, const Vec3f* ps,
                                       const ProjectRayNextIteration& next, double ray_len,
                                       const ProjectRayEdgeIndices* edge_indices,
                                       const ray_project::NonAdjacentConnections* non_adjacent_connections);

Vec3<double> edge_uv_to_world_point(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2,
                                    const Vec2f& uv);
Vec3f transform_vector_to_projected_triangle_space(const Vec3f& p0, const Vec3f& p1,
                                                   const Vec3f& p2, const Vec3f& v);

ProjectRayNextIteration prepare_next_iteration(const ProjectRayResult& res, double theta_offset);

}