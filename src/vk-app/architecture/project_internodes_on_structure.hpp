#pragma once

#include "../procedural_tree/growth_on_mesh.hpp"
#include "ray_project_adjacency.hpp"
#include <atomic>
#include <memory>
#include <future>

namespace grove::tree {
struct Internode;
}

namespace grove::arch {
struct StructureGeometryPiece;
}

namespace grove::arch {

struct ProjectInternodesOnStructureContext {
  std::vector<tree::Internode> internodes;
  std::vector<StructureGeometryPiece> structure_pieces;
  std::vector<uint32_t> tris;
  std::vector<Vec3f> ps;
  std::vector<Vec3f> ns;
  ray_project::NonAdjacentConnections non_adjacent_connections;
  tri::EdgeToIndex<uint32_t> edge_index_map;

  uint32_t initial_proj_ti;
  double ray_theta_offset;
  double ray_len;
  float diameter_power;
};

struct ProjectInternodesOnStructureResult {
  tree::PostProcessProjectedNodesResult post_process_res;
  std::vector<ProjectRayResultEntry> project_ray_results;
};

struct ProjectInternodesOnStructureFuture {
  bool is_ready() const {
    return ready.load();
  }

  std::atomic<bool> ready;
  std::future<void> async_future;
  ProjectInternodesOnStructureContext context;
  ProjectInternodesOnStructureResult result;
};

struct ProjectInternodesOnStructureParams {
  const tree::Internode* internodes;
  uint32_t num_internodes;
  const StructureGeometryPiece* structure_pieces;
  uint32_t num_pieces;
  const uint32_t* tris;
  uint32_t num_tris;
  const Vec3f* positions_or_aggregate_geometry;
  const Vec3f* normals_or_nullptr;
  size_t aggregate_geometry_stride_bytes;
  uint32_t num_vertices;

  uint32_t initial_proj_ti;
  double ray_theta_offset;
  double ray_len;
  float diameter_power;
};

std::unique_ptr<ProjectInternodesOnStructureFuture>
project_internodes_onto_structure(const ProjectInternodesOnStructureParams& params);

}