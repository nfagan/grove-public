#include "segmented_structure_system.hpp"
#include "structure_growth.hpp"
#include "structure_geometry.hpp"
#include "render.hpp"
#include "grove/math/random.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"
#include "grove/common/DynamicArray.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace arch;

} //  anon

namespace arch {

struct StructureEvents {
  bool grew;
  bool receded;
  bool just_finished_growing;
  bool just_finished_receding;
  bool just_prepared_receding_piece;
};

struct SegmentedStructure {
  SegmentedStructureHandle handle;
  std::unique_ptr<arch::FitBoundsToPointsContext> fit_context;
  StructureGeometry geometry;
  GrowingStructureGeometry growing_geometry;
  RenderTriangleGrowthContext triangle_growth_context;
  RenderTriangleRecedeContext triangle_recede_context;
  StructureEvents events{};
  bool growing{};
  bool receding{};
  float growth_incr{0.05f};
  float recede_incr{0.01f};
  bool has_receding_piece{};
};

struct SegmentedStructureSystem {
  DynamicArray<SegmentedStructure, 8> structures;
  uint32_t next_structure_id{1};
  DynamicArray<arch::WallHole, 4> default_wall_holes;
};

} //  arch

namespace {

Vec3f random_wall_scale(float z) {
  const float scales[4]{16.0f, 20.0f, 24.0f, 32.0f};
  int x_ind = int(urand() * 4);
  int y_ind = int(urand() * 4);
  return Vec3f{scales[x_ind], scales[y_ind], z};
}

SegmentedStructure* find_structure(SegmentedStructureSystem* sys, SegmentedStructureHandle handle) {
  for (auto& s : sys->structures) {
    if (s.handle == handle) {
      return &s;
    }
  }
  return nullptr;
}

SegmentedStructure make_structure(SegmentedStructureHandle handle,
                                  const CreateSegmentedStructureParams& params) {
  auto ori = params.origin;
  SegmentedStructure result{};
  result.handle = handle;
  auto line_target = exclude(ori, 1) + Vec2f{32.0f, -32.0f};
  result.fit_context = std::make_unique<arch::FitBoundsToPointsContext>();
  arch::initialize_fit_bounds_to_points_context_default(result.fit_context.get(), ori, line_target);
  return result;
}

void set_growable_dst_vertices(const Vec3f* src, const uint32_t* src_tris, uint32_t num_src_tris,
                               Vec3f* dst) {
  uint32_t dst_pi{};
  for (uint32_t ti = 0; ti < num_src_tris; ti++) {
    for (int i = 0; i < 3; i++) {
      const uint32_t pi = src_tris[ti * 3 + i];
      memcpy(dst + dst_pi * 2, src + pi * 2, 2 * sizeof(Vec3f));  //  position + normal
      assert(dst_pi < (1u << 16u));
      dst_pi++;
    }
  }
}

void prepare_growing_piece(SegmentedStructure* structure, const StructureGeometryPiece* piece) {
  auto& growing_geom = structure->growing_geometry;
  auto& geom = structure->geometry;
  auto& growth_context = structure->triangle_growth_context;

  const uint32_t ni = piece->num_triangles * 3;
  const uint32_t np = piece->num_vertices;
  resize_and_prepare(&growing_geom, ni, np, false);

  copy_triangles_and_vertices_from_aggregate_geometry_to_src_growing_geometry(&geom, &growing_geom, piece);

  //  prepare the destination (target) geometry, copying from the src geometry
  set_growable_dst_vertices(
    growing_geom.src_geometry.data(),
    growing_geom.src_tris.data(),
    growing_geom.num_src_tris,
    growing_geom.dst_geometry.data());

  const auto stride = uint32_t(2 * sizeof(Vec3f));
  arch::initialize_triangle_growth(
    &growth_context,
    growing_geom.src_tris.data(),
    piece->num_triangles,
    growing_geom.src_geometry.data(), stride, 0,
    growing_geom.dst_geometry.data(), stride, 0);
}

void prepare_receding_piece(SegmentedStructure* structure, const StructureGeometryPiece* piece) {
  auto& geom = structure->geometry;
  auto& growing_geom = structure->growing_geometry;

  const uint32_t ni = piece->num_triangles * 3;
  const uint32_t np = piece->num_vertices;
  resize_and_prepare(&growing_geom, ni, np, true);

  copy_triangles_and_vertices_from_aggregate_geometry_to_src_growing_geometry(&geom, &growing_geom, piece);
  copy_triangles_and_vertices_from_src_to_dst(&growing_geom, piece->num_triangles);

  const auto stride = uint32_t(structure->geometry.vertex_stride_bytes());
  arch::initialize_triangle_recede(
    &structure->triangle_recede_context,
    growing_geom.src_tris.data(),
    growing_geom.num_src_tris,
    growing_geom.src_geometry.data(), stride, 0,
    growing_geom.dst_geometry.data(), stride, 0);

  structure->has_receding_piece = true;
  remove_last_piece(&geom);
}

bool tick_structure_recede(RenderTriangleRecedeContext* context, float growth_incr) {
  arch::RenderTriangleRecedeParams recede_params{};
  recede_params.incr = growth_incr;
  recede_params.incr_randomness_range = 0.4f;
  recede_params.num_target_sets = 128;
  return !tick_triangle_recede(*context, recede_params);
}

auto update_structure_recede(SegmentedStructure* structure) {
  struct Result {
    bool finished_receding;
    bool prepared_new_receding_piece;
  };

  Result result{};

  if (!structure->has_receding_piece) {
    if (structure->geometry.pieces.empty()) {
      result.finished_receding = true;
      return result;
    } else {
      prepare_receding_piece(structure, &structure->geometry.pieces.back());
      result.prepared_new_receding_piece = true;
    }
  }

  if (tick_structure_recede(&structure->triangle_recede_context, structure->recede_incr)) {
    structure->has_receding_piece = false;
  }

  return result;
}

bool update_structure_growth(RenderTriangleGrowthContext* context,
                             GrowingStructureGeometry& growing_geometry, float growth_incr) {
  uint32_t curr_num_tris = growing_geometry.num_dst_tris;
  uint32_t num_active_inds = tick_triangle_growth(
    context,
    growing_geometry.dst_tris.data(),
    uint32_t(growing_geometry.dst_tris.size()), growth_incr);

  bool finished_growing{};
  if (num_active_inds == 0) {
    growing_geometry.num_dst_tris = curr_num_tris;
    finished_growing = true;
  } else {
    growing_geometry.num_dst_tris = num_active_inds / 3;
  }

  assert(growing_geometry.num_dst_tris <= growing_geometry.num_src_tris);
  return finished_growing;
}

bool update_structure_growth(SegmentedStructure* structure) {
  return update_structure_growth(
    &structure->triangle_growth_context, structure->growing_geometry, structure->growth_incr);
}

struct {
  arch::SegmentedStructureSystem structure_system;
} globals;

} //  anon

SegmentedStructureSystem* arch::get_global_segmented_structure_system() {
  return &globals.structure_system;
}

void arch::initialize_segmented_structure_system(SegmentedStructureSystem* sys) {
  sys->default_wall_holes.resize(3);
  WallHole::push_default3(sys->default_wall_holes.data());
}

void arch::update_segmented_structure_system(SegmentedStructureSystem* sys,
                                             const SegmentedStructureSystemUpdateInfo&) {
  for (auto& s : sys->structures) {
    s.events = {};
  }

  for (auto& structure : sys->structures) {
    assert(!(structure.growing && structure.receding));
    if (structure.growing) {
      structure.events.grew = true;
      if (update_structure_growth(&structure)) {
        structure.growing = false;
        structure.events.just_finished_growing = true;
      }
    } else if (structure.receding) {
      structure.events.receded = true;
      auto recede_res = update_structure_recede(&structure);
      if (recede_res.prepared_new_receding_piece) {
        structure.events.just_prepared_receding_piece = true;
      }
      if (recede_res.finished_receding) {
        structure.receding = false;
        structure.events.just_finished_receding = true;
      }
    }
  }
}

SegmentedStructureHandle arch::create_structure(SegmentedStructureSystem* sys,
                                                const CreateSegmentedStructureParams& params) {
  SegmentedStructureHandle result{sys->next_structure_id++};
  sys->structures.emplace_back();
  sys->structures.back() = make_structure(result, params);
  return result;
}

Optional<OBB3f> arch::get_last_structure_piece_bounds(SegmentedStructureSystem* sys,
                                                      SegmentedStructureHandle handle) {
  auto* structure = find_structure(sys, handle);
  assert(structure);
  if (!structure->geometry.pieces.empty()) {
    return Optional<OBB3f>(structure->geometry.pieces.back().bounds);
  } else {
    return NullOpt{};
  }
}

bool arch::can_extrude_structure(SegmentedStructureSystem* sys, SegmentedStructureHandle handle) {
  auto* structure = find_structure(sys, handle);
  assert(structure);
  return !structure->growing && !structure->receding;
}

Optional<OBB3f> arch::extrude_structure_bounds(SegmentedStructureSystem* sys,
                                               SegmentedStructureHandle handle) {
  assert(can_extrude_structure(sys, handle));
  auto* structure = find_structure(sys, handle);
  assert(structure);

  Optional<OBB3f> parent_bounds;
  float default_depth{2.0f};
  if (!structure->geometry.pieces.empty()) {
    auto& piece = structure->geometry.pieces.back();
    parent_bounds = piece.bounds;
    default_depth = piece.bounds.half_size.z * 2.0f;
  }

  auto bounds_size = random_wall_scale(default_depth);
  auto bounds_ptr = parent_bounds ? &parent_bounds.value() : nullptr;
  auto next_bounds = arch::extrude_bounds(structure->fit_context.get(), bounds_size, bounds_ptr);
  return next_bounds;
}

bool arch::can_start_receding_structure(SegmentedStructureSystem* sys,
                                        SegmentedStructureHandle handle) {
  auto* structure = find_structure(sys, handle);
  assert(structure);
  return !structure->growing && !structure->receding && !structure->geometry.pieces.empty();
}

void arch::start_receding_structure(SegmentedStructureSystem* sys,
                                    SegmentedStructureHandle handle) {
  assert(can_start_receding_structure(sys, handle));
  auto* structure = find_structure(sys, handle);
  structure->receding = true;
}

void arch::extrude_structure(SegmentedStructureSystem* sys, SegmentedStructureHandle handle,
                             const OBB3f& bounds, const ExtrudeSegmentedStructureParams& params) {
  assert(can_extrude_structure(sys, handle));
  auto* structure = find_structure(sys, handle);
  assert(structure);

  Optional<StructureGeometryPieceHandle> parent_piece;
  if (!structure->geometry.pieces.empty() && !params.disable_connection_to_parent) {
    parent_piece = structure->geometry.pieces.back().handle;
  }

  auto* holes = params.holes;
  int num_holes = params.num_holes;
  if (params.prefer_default_holes) {
    holes = sys->default_wall_holes.data();
    num_holes = int(sys->default_wall_holes.size());
  }

  auto piece_handle = arch::extrude_wall(&structure->geometry, bounds, holes, num_holes, parent_piece);
  auto* piece = structure->geometry.read_piece(piece_handle);
  assert(piece);
  prepare_growing_piece(structure, piece);
  structure->growing = true;
}

void arch::set_structure_growth_incr(SegmentedStructureSystem* sys,
                                     SegmentedStructureHandle handle, float incr) {
  assert(incr >= 0.0f && incr <= 1.0f);
  auto* structure = find_structure(sys, handle);
  assert(structure);
  structure->growth_incr = clamp(incr, 0.0f, 1.0f);
}

void arch::set_structure_recede_incr(SegmentedStructureSystem* sys,
                                     SegmentedStructureHandle handle, float incr) {
  assert(incr >= 0.0f && incr <= 1.0f);
  auto* structure = find_structure(sys, handle);
  assert(structure);
  structure->recede_incr = lerp(clamp01(incr), 0.0f, 0.01f);
}

int arch::num_pieces_in_structure(SegmentedStructureSystem* sys, SegmentedStructureHandle handle) {
  auto* structure = find_structure(sys, handle);
  assert(structure);
  return int(structure->geometry.pieces.size());
}

StructureGeometry* arch::get_geometry(SegmentedStructureSystem* sys,
                                      SegmentedStructureHandle handle) {
  auto* structure = find_structure(sys, handle);
  assert(structure);
  return &structure->geometry;
}

Optional<ReadGrowingTriangleData> arch::read_growing_triangle_data(SegmentedStructureSystem* sys,
                                                                   SegmentedStructureHandle handle) {
  auto* structure = find_structure(sys, handle);
  assert(structure);
#if 0 //  @TODO
  if (!structure->grew) {
    return NullOpt{};
  }
#endif

  auto& geom = structure->growing_geometry;
  ReadGrowingTriangleData result{};
  result.vertices = geom.dst_geometry.data();
  result.num_vertices = geom.num_dst_vertices;
  result.indices = geom.dst_tris.data();
  result.num_active_indices = geom.num_dst_tris * 3;
  result.num_total_indices = geom.num_src_tris * 3;
  return Optional<ReadGrowingTriangleData>(result);
}

bool arch::structure_grew(SegmentedStructureSystem* sys, SegmentedStructureHandle handle) {
  auto* structure = find_structure(sys, handle);
  assert(structure);
  return structure->events.grew;
}

bool arch::structure_just_finished_growing(SegmentedStructureSystem* sys,
                                           SegmentedStructureHandle handle) {
  auto* structure = find_structure(sys, handle);
  assert(structure);
  return structure->events.just_finished_growing;
}

bool arch::structure_receded(SegmentedStructureSystem* sys, SegmentedStructureHandle handle) {
  auto* structure = find_structure(sys, handle);
  assert(structure);
  return structure->events.receded;
}

bool arch::structure_just_finished_receding(SegmentedStructureSystem* sys,
                                            SegmentedStructureHandle handle) {
  auto* structure = find_structure(sys, handle);
  assert(structure);
  return structure->events.just_finished_receding;
}

bool arch::structure_just_prepared_receding_piece(SegmentedStructureSystem* sys,
                                                  SegmentedStructureHandle handle) {
  auto* structure = find_structure(sys, handle);
  assert(structure);
  return structure->events.just_prepared_receding_piece;
}

GROVE_NAMESPACE_END
