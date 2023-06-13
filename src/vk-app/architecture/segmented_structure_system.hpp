#pragma once

#include "grove/common/identifier.hpp"
#include "grove/common/Optional.hpp"
#include "grove/math/OBB3.hpp"
#include "grove/math/Vec2.hpp"

namespace grove::arch {

struct SegmentedStructureSystem;
struct StructureGeometry;
struct WallHole;

struct SegmentedStructureSystemUpdateInfo {
  double real_dt;
};

struct SegmentedStructureHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(SegmentedStructureHandle, id)
  uint32_t id;
};

struct CreateSegmentedStructureParams {
  Vec3f origin;
};

struct ExtrudeSegmentedStructureParams {
  const arch::WallHole* holes;
  int num_holes;
  bool prefer_default_holes;
  bool disable_connection_to_parent;
};

struct ReadGrowingTriangleData {
  const Vec3f* vertices;
  uint32_t num_vertices;
  const uint16_t* indices;
  uint32_t num_active_indices;
  uint32_t num_total_indices;
};

SegmentedStructureSystem* get_global_segmented_structure_system();
void initialize_segmented_structure_system(SegmentedStructureSystem* sys);
void update_segmented_structure_system(SegmentedStructureSystem* sys,
                                       const SegmentedStructureSystemUpdateInfo& info);

SegmentedStructureHandle create_structure(SegmentedStructureSystem* sys,
                                          const CreateSegmentedStructureParams& params);
Optional<OBB3f> get_last_structure_piece_bounds(SegmentedStructureSystem* sys,
                                                SegmentedStructureHandle handle);
bool can_extrude_structure(SegmentedStructureSystem* sys, SegmentedStructureHandle handle);
Optional<OBB3f> extrude_structure_bounds(SegmentedStructureSystem* sys, SegmentedStructureHandle handle);
void extrude_structure(SegmentedStructureSystem* sys, SegmentedStructureHandle handle, const OBB3f& bounds,
                       const ExtrudeSegmentedStructureParams& params);

void set_structure_growth_incr(SegmentedStructureSystem* sys,
                               SegmentedStructureHandle handle, float incr);
void set_structure_recede_incr(SegmentedStructureSystem* sys,
                               SegmentedStructureHandle handle, float incr);
int num_pieces_in_structure(SegmentedStructureSystem* sys, SegmentedStructureHandle handle);

bool can_start_receding_structure(SegmentedStructureSystem* sys, SegmentedStructureHandle handle);
void start_receding_structure(SegmentedStructureSystem* sys, SegmentedStructureHandle handle);

StructureGeometry* get_geometry(SegmentedStructureSystem* sys, SegmentedStructureHandle handle);
Optional<ReadGrowingTriangleData> read_growing_triangle_data(SegmentedStructureSystem* sys,
                                                             SegmentedStructureHandle handle);
bool structure_grew(SegmentedStructureSystem* sys, SegmentedStructureHandle handle);
bool structure_just_finished_growing(SegmentedStructureSystem* sys, SegmentedStructureHandle handle);

bool structure_receded(SegmentedStructureSystem* sys, SegmentedStructureHandle handle);
bool structure_just_finished_receding(SegmentedStructureSystem* sys, SegmentedStructureHandle handle);
bool structure_just_prepared_receding_piece(SegmentedStructureSystem* sys, SegmentedStructureHandle handle);

}