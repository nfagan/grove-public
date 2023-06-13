#pragma once

#include "geometry.hpp"
#include "grove/common/identifier.hpp"
#include "grove/common/Optional.hpp"
#include "grove/math/OBB3.hpp"

namespace grove {
struct Ray;
}

namespace grove::tri {
template <typename T>
struct EdgeToIndex;
}

namespace grove::ray_project {
struct NonAdjacentConnections;
}

namespace grove::arch {

struct WallHole;

struct StructureGeometryPieceHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(StructureGeometryPieceHandle, id)
  uint32_t id;
};

struct StructureGeometryPiece {
  StructureGeometryPieceHandle handle;
  Optional<StructureGeometryPieceHandle> parent;
  OBB3f bounds;
  uint32_t geometry_offset;
  uint32_t triangle_offset;
  uint32_t num_vertices;
  uint32_t num_triangles;
  Optional<arch::FaceConnectorIndices> connector_positive_x;
  Optional<arch::FaceConnectorIndices> connector_negative_x;
  Optional<arch::FaceConnectorIndices> curved_connector_positive_x;
  Optional<arch::FaceConnectorIndices> curved_connector_negative_x;
  uint32_t curved_connector_xi;
};

struct StructureGeometry {
  uint32_t num_vertices() const;
  uint32_t num_triangles() const;
  const StructureGeometryPiece* read_piece(StructureGeometryPieceHandle handle) const;
  size_t vertex_stride_bytes() const;
  Optional<uint32_t> ray_intersect(const Ray& ray) const;
  uint32_t max_vertex_index_or_zero() const;

  std::vector<StructureGeometryPiece> pieces;
  std::vector<Vec3f> geometry;
  std::vector<uint32_t> triangles;
};

struct GrowingStructureGeometry {
  std::vector<uint32_t> src_tris;
  std::vector<uint16_t> dst_tris;
  std::vector<Vec3f> src_geometry;
  std::vector<Vec3f> dst_geometry;
  uint32_t num_src_vertices{};
  uint32_t num_dst_vertices{};
  uint32_t num_src_tris{};
  uint32_t num_dst_tris{};
};

void initialize_structure_geometry_context();

StructureGeometryPieceHandle extrude_wall(
  StructureGeometry* structure, const OBB3f& bounds, const WallHole* holes, int num_holes,
  const Optional<StructureGeometryPieceHandle>& parent_piece);

bool try_connect_non_adjacent_structure_pieces(
  const std::vector<Vec3f>& geometry,
  bool interleaved_geometry,  //  true if geometry is position + normal + ... ; false if geometry is positions only
  const tri::EdgeToIndex<uint32_t>& tri_edge_indices,
  const StructureGeometryPiece& prev, const StructureGeometryPiece& curr,
  ray_project::NonAdjacentConnections* connects);

void copy_structure_geometry_deinterleaved(const Vec3f* ps_ns, Vec3f* ps, Vec3f* ns, uint32_t np);
void copy_triangles_and_vertices_from_src_to_dst(GrowingStructureGeometry* geom, uint32_t num_tris);
void copy_triangles_and_vertices_from_aggregate_geometry_to_src_growing_geometry(
  const StructureGeometry* aggregate_geom, GrowingStructureGeometry* growing_geom,
  const StructureGeometryPiece* piece);

void resize_and_prepare(GrowingStructureGeometry* geom, uint32_t ni, uint32_t np, bool receding);
void remove_last_piece(StructureGeometry* geom);

}