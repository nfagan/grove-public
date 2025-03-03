#include "DebugArchComponent.hpp"
#include "debug.hpp"
#include "../render/SampledImageManager.hpp"
#include "../render/debug_draw.hpp"
#include "../render/memory.hpp"
#include "../imgui/ArchGUI.hpp"
#include "../procedural_tree/projected_nodes.hpp"
#include "../procedural_tree/collide_with_object.hpp"
#include "../terrain/terrain.hpp"
#include "../procedural_tree/serialize.hpp"
#include "../procedural_tree/render.hpp"
#include "../procedural_tree/bud_fate.hpp"
#include "../procedural_tree/utility.hpp"
#include "grove/env.hpp"
#include "grove/common/common.hpp"
#include "grove/common/memory.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/profile.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/debug/cdt.hpp"
#include "grove/math/triangle.hpp"
#include "grove/math/frame.hpp"
#include "grove/math/random.hpp"
#include "grove/math/ease.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/visual/Image.hpp"
#include "grove/load/image.hpp"

GROVE_NAMESPACE_BEGIN

namespace arch {
using IndexMap = std::unordered_map<uint32_t, uint32_t>;
}

namespace {

using DebugProjectedNodes = DebugArchComponent::DebugProjectedNodes;

struct GrowingTreeNodes {
  tree::ProjectedTreeInstanceHandle proj_instance_handle;
};

struct TreeNodeCollisionWithWallParams {
  tree::TreeNodeCollisionWithObjectContext* collision_context;
  const DebugArchComponent::CollideThroughHoleParams* collide_through_hole_params;
  OBB3f wall_bounds;
  const tree::Internode* src_internodes;
  int num_src_internodes;
  arch::WallHole* accepted_holes;
  int max_num_accepted_holes;
};

tree::TreeNodeCollisionWithObjectResult
compute_collision_with_wall(const TreeNodeCollisionWithWallParams& params);

struct StructurePiece {
  OBB3f bounds;
  bounds::ElementID bounds_element_id;
  bounds::RadiusLimiterAggregateID radius_limiter_aggregate_id;
  bounds::RadiusLimiterElementHandle radius_limiter_element;
  Optional<arch::FaceConnectorIndices> connector_positive_x;
  Optional<arch::FaceConnectorIndices> connector_negative_x;
  Optional<arch::FaceConnectorIndices> curved_connector_positive_x;
  Optional<arch::FaceConnectorIndices> curved_connector_negative_x;
  uint32_t curved_connector_xi;
  uint32_t aggregate_geometry_offset;
  uint32_t num_vertices;
  uint32_t num_triangles;
};

struct StructureGeometry {
  uint32_t num_aggregate_vertices() const {
    return uint32_t(aggregate_geometry.size() / 2);
  }
  uint32_t num_aggregate_triangles() const {
    return uint32_t(aggregate_triangles.size() / 3);
  }
  size_t aggregate_geometry_vertex_stride_bytes() const {
    return sizeof(Vec3f) * 2;
  }
  size_t growing_geometry_vertex_stride_bytes() const {
    return sizeof(Vec3f) * 2;
  }
  const Vec3f& ith_aggregate_position(uint32_t i) const {
    return aggregate_geometry[i * 2];
  }
  const Vec3f& ith_aggregate_normal(uint32_t i) const {
    return aggregate_geometry[i * 2 + 1];
  }
  const Vec3f& ith_growing_src_position(uint32_t i) const {
    return growing_geometry_src[i * 2];
  }
  const Vec3f& ith_growing_src_normal(uint32_t i) const {
    return growing_geometry_src[i * 2 + 1];
  }

  std::vector<Vec3f> aggregate_geometry;
  std::vector<uint16_t> aggregate_triangles;
  std::vector<Vec3f> growing_geometry_src;
  std::vector<Vec3f> growing_geometry_dst;
  std::vector<uint32_t> growing_triangles_src;
  std::vector<uint16_t> growing_triangles_dst;
  uint32_t num_growing_triangles_src;
  uint32_t num_growing_triangles_dst;
  uint32_t num_growing_vertices_src;
  uint32_t num_growing_vertices_dst;
};

enum class StructureGrowthState {
  Idle,
  Growing,
  Receding
};

enum class StructureGrowthPhase {
  Idle,
  PendingProjectedNodesFinishedReceding,
  StructureReceding,
};

bool is_idle(StructureGrowthState state) {
  return state == StructureGrowthState::Idle;
}

struct SegmentedStructure {
  std::vector<StructurePiece> pieces;
  std::vector<GrowingTreeNodes> growing_tree_nodes;
  std::vector<tree::TreeInstanceHandle> pending_finish_prune;
  StructureGeometry geometry;
  arch::IndexMap remapped_aggregate_geometry_indices_within_tol;
  ray_project::NonAdjacentConnections non_adjacent_connections;
  ArchRenderer::GeometryHandle aggregate_renderer_geometry;
  ArchRenderer::DrawableHandle aggregate_drawable;
  ArchRenderer::GeometryHandle growing_renderer_geometry;
  ArchRenderer::DrawableHandle growing_drawable;
  arch::RenderTriangleGrowthContext triangle_growth_context;
  arch::RenderTriangleRecedeContext triangle_recede_context;
  Vec3f origin;
  StructureGrowthState growth_state;
  StructureGrowthPhase growth_phase;
  Stopwatch state_stopwatch;
  bool extrude_disabled;
  float max_piece_x_length;
  bool need_start_receding;
  bool has_receding_piece;
  int next_receding_piece_index;
};

float piece_x_length(const SegmentedStructure& structure) {
  float s{};
  for (auto& piece : structure.pieces) {
    s += piece.bounds.half_size.x * 2.0f;
  }
  return s;
}

struct GlobalData {
  SegmentedStructure debug_segmented_structure;
  SegmentedStructure debug_growing_segmented_structure;
  arch::FitBoundsToPointsContext debug_growing_structure_context;
  LinearAllocator geom_allocs[4];
  std::unique_ptr<unsigned char[]> heap_data;
  tree::TreeNodeCollisionWithObjectContext debug_collision_context;
} global_data{};

void initialize_geometry_component_allocators(LinearAllocator allocs[4],
                                              std::unique_ptr<unsigned char[]>* heap_data) {
  size_t sizes[4] = {
    sizeof(Vec3f) * 4096,         //  positions
    sizeof(Vec3f) * 4096,         //  normals
    sizeof(uint32_t) * 4096 * 3,  //  triangles
    sizeof(uint32_t) * 4096       //  tmp
  };
  *heap_data = make_linear_allocators_from_heap(sizes, allocs, 4);
}

arch::GeometryAllocators make_geometry_allocators(LinearAllocator allocs[4]) {
  return arch::make_geometry_allocators(allocs, allocs + 1, allocs + 2, allocs + 3);
}

void update_debug_normals(vk::PointBufferRenderer& pb_renderer,
                          const vk::PointBufferRenderer::AddResourceContext& context,
                          vk::PointBufferRenderer::DrawableHandle handle,
                          const std::vector<Vec3f>& positions,
                          const std::vector<Vec3f>& normals) {
  pb_renderer.reserve_instances(
    context,
    handle,
    uint32_t(positions.size() * 2));
  std::vector<Vec3f> line_points(positions.size() * 2);
  for (uint32_t i = 0; i < uint32_t(positions.size()); i++) {
    auto& p0 = positions[i];
    auto& n = normals[i];
    line_points[i * 2] = p0;
    line_points[i * 2 + 1] = p0 + n * 0.25f;
  }
  pb_renderer.set_instances(
    context,
    handle,
    line_points.data(),
    int(line_points.size()),
    0);
}

bool reserve_arch_geometry(ArchRenderer& renderer,
                           const ArchRenderer::AddResourceContext& context,
                           ArchRenderer::GeometryHandle geometry_handle,
                           uint32_t num_points, uint32_t num_inds) {
  VertexBufferDescriptor desc;
  desc.add_attribute(AttributeDescriptor::float3(0));
  desc.add_attribute(AttributeDescriptor::float3(1));
  return renderer.update_geometry(
    context,
    geometry_handle,
    nullptr, num_points * 2 * sizeof(Vec3f),
    desc, 0, Optional<int>(1), nullptr, num_inds);
}

bool update_arch_geometry(ArchRenderer& renderer,
                          const ArchRenderer::AddResourceContext& context,
                          ArchRenderer::GeometryHandle geometry_handle,
                          const std::vector<Vec3f>& data,
                          const std::vector<uint16_t>& inds,
                          size_t num_vertices = 0, size_t num_indices = 0) {
  if (num_vertices == 0) {
    num_vertices = data.size() / 2;
  }
  if (num_indices == 0) {
    num_indices = inds.size();
  }
  VertexBufferDescriptor desc;
  desc.add_attribute(AttributeDescriptor::float3(0));
  desc.add_attribute(AttributeDescriptor::float3(1));
  return renderer.update_geometry(
    context,
    geometry_handle,
    data.data(), 2 * num_vertices * sizeof(Vec3f),
    desc, 0, Optional<int>(1), inds.data(), uint32_t(num_indices));
}

std::vector<Vec3f> interleave(const std::vector<Vec3f>& p, const std::vector<Vec3f>& n) {
  assert(p.size() == n.size());
  std::vector<Vec3f> interleaved_data(p.size() * 2);
  for (uint32_t i = 0; i < uint32_t(p.size()); i++) {
    interleaved_data[i * 2] = p[i];
    interleaved_data[i * 2 + 1] = n[i];
  }
  return interleaved_data;
}

void copy_uint32_to_uint16(const void* uint32_src, void* uint16_dst, uint32_t ni) {
  for (uint32_t i = 0; i < ni; i++) {
    uint32_t ind;
    read_ith(&ind, static_cast<const unsigned char*>(uint32_src), i);
    assert(ind < (1u << 16u));
    auto v = uint16_t(ind);
    write_ith(static_cast<unsigned char*>(uint16_dst), &v, i);
  }
}

void copy_interleaved(const void* ps, const void* ns, void* dst, uint32_t np) {
  VertexBufferDescriptor src_desc;
  src_desc.add_attribute(AttributeDescriptor::float3(0));
  VertexBufferDescriptor dst_desc;
  dst_desc.add_attribute(AttributeDescriptor::float3(0));
  dst_desc.add_attribute(AttributeDescriptor::float3(1));
  const int src_inds[1] = {0};
  const int dst_inds[2] = {0, 1};
  //  Copy positions
  copy_buffer(ps, src_desc, src_inds, dst, dst_desc, dst_inds, 1, np);
  //  Copy normals
  copy_buffer(ns, src_desc, src_inds, dst, dst_desc, dst_inds+1, 1, np);
}

void copy_deinterleaved(const void* ps_ns, void* dst_ps, void* dst_ns, uint32_t np) {
  VertexBufferDescriptor src_desc;
  src_desc.add_attribute(AttributeDescriptor::float3(0));
  src_desc.add_attribute(AttributeDescriptor::float3(1));
  VertexBufferDescriptor dst_desc;
  dst_desc.add_attribute(AttributeDescriptor::float3(0));
  const int src_inds[2] = {0, 1};
  const int dst_inds[1] = {0};
  //  Copy positions
  copy_buffer(ps_ns, src_desc, src_inds, dst_ps, dst_desc, dst_inds, 1, np);
  //  Copy normals
  copy_buffer(ps_ns, src_desc, src_inds+1, dst_ns, dst_desc, dst_inds, 1, np);
}

void make_default_holes(std::vector<arch::WallHole>& holes) {
  auto& hole0 = holes.emplace_back();
  hole0.scale = Vec2f{0.25f};
  hole0.curl = 0.4f;
  hole0.rot = 0.1f;
  hole0.off = Vec2f{0.1f, -0.1f};

  auto& hole1 = holes.emplace_back();
  hole1.scale = Vec2f{0.25f, 0.3f};
  hole1.curl = 0.2f;
  hole1.rot = -0.3f;
  hole1.off = Vec2f{-0.2f, 0.2f};

  auto& hole2 = holes.emplace_back();
  hole2.scale = Vec2f{0.1f, 0.2f};
  hole2.curl = 0.2f;
  hole2.rot = 0.3f;
  hole2.off = Vec2f{0.3f, 0.3f};
}

arch::TriangulationResult make_debug_straight_flat_segments() {
  arch::StraightFlatSegmentParams straight_flat_seg_params{};
  straight_flat_seg_params.grid_x_segments = 2;
  straight_flat_seg_params.grid_y_segments = 2;
  std::swap(straight_flat_seg_params.dim_perm[0], straight_flat_seg_params.dim_perm[2]);
  return arch::make_straight_flat_segment(straight_flat_seg_params);
}

arch::WallHoleResult make_debug_wall(const std::vector<arch::WallHole>& holes, float ar) {
  arch::WallHoleParams hole_params{};
  hole_params.grid_y_segments = 4;
  hole_params.grid_x_segments = 4;
  hole_params.holes = holes.data();
  hole_params.num_holes = uint32_t(holes.size());
  hole_params.aspect_ratio = ar;
  std::swap(hole_params.dim_perm[1], hole_params.dim_perm[2]);
  return arch::make_wall_hole(hole_params);
}

void add_adjoining_curved_segment(const Vec2f& p00, const Vec2f& p01,
                                  const Vec2f& p10, const Vec2f& p11,
                                  const Vec2f& n01, const Vec2f& n11,
                                  uint32_t index_offset,
                                  const arch::GeometryAllocators& alloc,
                                  const OBB3f& wall0_bounds,
                                  arch::FaceConnectorIndices* positive_x,
                                  arch::FaceConnectorIndices* negative_x,
                                  uint32_t* num_new_points, uint32_t* num_new_inds) {
  auto grid_ps = arch::make_grid<double>(5, 4);
  auto grid_t = cdt::triangulate_simple(grid_ps);

  arch::AdjoiningCurvedSegmentParams adj_params{};
  adj_params.p0 = p01;
  adj_params.p1 = p11;
  adj_params.v0 = adj_params.p0 - p00;
  adj_params.v1 = adj_params.p1 - p10;
  adj_params.n0 = n01;
  adj_params.n1 = n11;
  adj_params.grid = arch::make_triangulated_grid(grid_t, grid_ps);
  adj_params.y_scale = wall0_bounds.half_size.y * 2.0f;
  adj_params.y_offset = wall0_bounds.position.y - wall0_bounds.half_size.y;
  adj_params.index_offset = index_offset;
  adj_params.alloc = alloc;
  adj_params.num_points_added = num_new_points;
  adj_params.num_indices_added = num_new_inds;
  adj_params.positive_x = positive_x;
  adj_params.negative_x = negative_x;
  arch::make_adjoining_curved_segment(adj_params);
}

double compute_initial_ray_direction(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2) {
  auto v = transform_vector_to_projected_triangle_space(p0, p1, p2, Vec3f{0.0f, 1.0f, 0.0f});
  auto init_theta = atan2(v.y, v.x);
  return init_theta >= 0 ? init_theta : float(two_pi()) + init_theta;
}

double compute_initial_ray_direction(const uint32_t* tris, uint32_t ti, const Vec3f* ps) {
  return compute_initial_ray_direction(
    ps[tris[ti * 3 + 0]], ps[tris[ti * 3 + 1]], ps[tris[ti * 3 + 2]]);
}

Vec3<double> edge_uv_to_world_point(const uint32_t* tris, uint32_t ti,
                                    const Vec3f* ps, const Vec2f& uv = Vec2f{0.5f}) {
  return edge_uv_to_world_point(
    ps[tris[ti * 3 + 0]], ps[tris[ti * 3 + 1]], ps[tris[ti * 3 + 2]], uv);
}

tree::SpawnInternodeParams make_default_projected_node_spawn_params(float diam_power) {
  tree::SpawnInternodeParams spawn_params{};
  spawn_params.leaf_diameter *= 2.0f;
  spawn_params.diameter_power = diam_power;
  return spawn_params;
}

tree::PostProcessProjectedNodesParams
to_post_process_params(const DebugArchComponent::Params& params) {
  tree::PostProcessProjectedNodesParams pp_params{};
  pp_params.prune_intersecting_internode_queue_size =
    !params.prune_intersecting_tree_nodes ? 0 : params.intersecting_tree_node_queue_size;
  pp_params.reset_internode_diameter = params.reset_tree_node_diameter;
  pp_params.smooth_diameter_adjacent_count =
    !params.smooth_tree_node_diameter ? 0 : params.smooth_diameter_adjacent_count;
  pp_params.smooth_normals_adjacent_count =
    !params.smooth_tree_node_normals ? 0 : params.smooth_normals_adjacent_count;
  pp_params.offset_internodes_by_radius = params.offset_tree_nodes_by_radius;
  pp_params.constrain_lateral_child_diameter = params.constrain_child_node_diameter;
  if (params.constrain_internode_diameter) {
    assert(params.max_internode_diameter > 0.0f);
    pp_params.max_diameter = params.max_internode_diameter;
  }
  return pp_params;
}

void set_structure_growth_params_preset1(DebugArchComponent::StructureGrowthParams& p) {
  p.num_pieces = 4;
  p.encircle_point_params = arch::TryEncirclePointParams::make_default1(nullptr);
  p.piece_length = 8.0f;
//  p.structure_ori = Vec3f{0.0f, 5.5f, 0.0f};
  p.structure_ori = Vec3f{8.0f, 5.5f, 16.0f};
  p.use_variable_piece_length = true;
  p.target_length = 16.0f;
}

arch::TryEncirclePointParams
to_try_encircle_point_params(const DebugArchComponent::StructureGrowthParams& params) {
  auto point_params = params.encircle_point_params;
  point_params.constant_speed = params.use_variable_piece_length ? nullptr : &params.piece_length;
  return point_params;
}

size_t growing_geometry_dst_size(const StructureGeometry* geom) {
  auto sz = geom->num_growing_vertices_dst * 2;
  assert(sz <= geom->growing_geometry_dst.size());
  return sz;
}

void reset_structure_geometry(StructureGeometry* geom) {
  geom->aggregate_geometry.clear();
  geom->aggregate_triangles.clear();
  geom->growing_geometry_src.clear();
  geom->growing_geometry_dst.clear();
  geom->growing_triangles_src.clear();
  geom->growing_triangles_dst.clear();
  geom->num_growing_triangles_src = 0;
  geom->num_growing_triangles_dst = 0;
  geom->num_growing_vertices_src = 0;
  geom->num_growing_vertices_dst = 0;
}

void reset_structure(SegmentedStructure* structure, const Vec3f& ori, float max_piece_x_length) {
  structure->pieces.clear();
  structure->growing_tree_nodes.clear();
  reset_structure_geometry(&structure->geometry);
  structure->growth_state = StructureGrowthState::Idle;
  structure->origin = ori;
  structure->max_piece_x_length = max_piece_x_length;
}

void reserve_growing(StructureGeometry* geom, uint32_t np, uint32_t ni) {
  geom->growing_geometry_src.resize(np * 2);
  geom->growing_geometry_dst.resize(ni * 2);
  geom->growing_triangles_src.resize(ni);
  geom->growing_triangles_dst.resize(ni);

  const uint32_t num_tris = ni / 3;
  geom->num_growing_triangles_src = num_tris;
  geom->num_growing_triangles_dst = 0;
  geom->num_growing_vertices_src = np;
  geom->num_growing_vertices_dst = ni;
}

void copy_from_alloc_to_growing_src(StructureGeometry* geom, const arch::GeometryAllocators& alloc,
                                    uint32_t np, uint32_t dst_index_off, uint32_t dst_vertex_off) {
  assert(geom->growing_triangles_src.size() >= dst_index_off &&
         (geom->growing_triangles_src.size() - dst_index_off) * sizeof(uint32_t) >= size(alloc.tris) &&
         geom->growing_geometry_src.size() >= dst_vertex_off * 2 &&
         (geom->growing_geometry_src.size() - dst_vertex_off * 2) >= 2 * np);
  //  copy indices
  memcpy(geom->growing_triangles_src.data() + dst_index_off, alloc.tris->begin, size(alloc.tris));
  //  copy geometry
  copy_interleaved(
    alloc.ps->begin,
    alloc.ns->begin,
    geom->growing_geometry_src.data() + dst_vertex_off * 2,
    np);
}

void copy_from_growing_src_to_growing_dst(StructureGeometry* geom, uint32_t num_tris) {
  const auto& tri_src = geom->growing_triangles_src;
  const auto& geom_src = geom->growing_geometry_src;

  auto& tri_dst = geom->growing_triangles_dst;
  auto& geom_dst = geom->growing_geometry_dst;
  const auto* geom_src_p = static_cast<const unsigned char*>((const void*) geom_src.data());
  auto* geom_dst_p = static_cast<unsigned char*>((void*) geom_dst.data());

  assert(tri_dst.size() >= num_tris * 3);

  const size_t vert_stride = geom->growing_geometry_vertex_stride_bytes();
  for (uint32_t i = 0; i < num_tris; i++) {
    for (int j = 0; j < 3; j++) {
      const uint32_t src_pi = tri_src[i * 3 + j];
      const uint32_t dst_pi = i * 3 + j;
      assert(dst_pi < (1u << 16u));
      auto* src_beg = geom_src_p + vert_stride * src_pi;
      auto* dst_beg = geom_dst_p + vert_stride * dst_pi;
      memcpy(dst_beg, src_beg, vert_stride);
      tri_dst[i * 3 + j] = uint16_t(dst_pi);
    }
  }

  geom->num_growing_triangles_dst = num_tris;
  geom->num_growing_triangles_src = num_tris;
}

void copy_from_aggregate_to_growing_src(StructureGeometry* geom, uint32_t np, uint32_t ni,
                                        uint32_t ith_vert_off, uint32_t ith_ind_off,
                                        uint32_t sub_index_offset) {
  assert(geom->growing_geometry_src.size() >= np * 2 && geom->growing_triangles_src.size() >= ni);
  assert((ith_vert_off + np) * 2 <= geom->aggregate_geometry.size() &&
         ith_ind_off + ni <= geom->aggregate_triangles.size());
  assert(geom->aggregate_geometry_vertex_stride_bytes() == geom->growing_geometry_vertex_stride_bytes());

  auto* dst_geom = static_cast<unsigned char*>((void*) geom->growing_geometry_src.data());
  const auto* src_geom = static_cast<const unsigned char*>((void*) geom->aggregate_geometry.data());
  size_t geom_stride = geom->aggregate_geometry_vertex_stride_bytes();

  for (uint32_t i = 0; i < np; i++) {
    auto* src = src_geom + (i + ith_vert_off) * geom_stride;
    auto* dst = dst_geom + i * geom_stride;
    memcpy(dst, src, geom_stride);
  }

  for (uint32_t i = 0; i < ni; i++) {
    auto src_ind = uint32_t(geom->aggregate_triangles[i + ith_ind_off]);
    assert(src_ind >= sub_index_offset);
    src_ind -= sub_index_offset;
    geom->growing_triangles_src[i] = src_ind;
  }
}

StructurePiece make_structure_piece(const OBB3f& bounds) {
  StructurePiece result{};
  result.bounds_element_id = bounds::ElementID::create();
  result.radius_limiter_aggregate_id = bounds::RadiusLimiterAggregateID::create();
  result.bounds = bounds;
  return result;
}

void add_piece(SegmentedStructure* structure, const StructurePiece& piece) {
  structure->pieces.push_back(piece);
}

Optional<OBB3f> extrude_bounds(const SegmentedStructure* structure,
                               arch::FitBoundsToPointsContext* context,
                               const Vec3f& size) {
  const OBB3f* parent_bounds = structure->pieces.empty() ?
    nullptr : &structure->pieces.back().bounds;
  return arch::extrude_bounds(context, size, parent_bounds);
}

void compute_wall_segment_geometry(const OBB3f& bounds,
                                   const arch::WallHoleResult& hole_res,
                                   const arch::TriangulationResult& seg_res,
                                   const arch::GeometryAllocators& alloc,
                                   arch::FaceConnectorIndices* positive_x,
                                   arch::FaceConnectorIndices* negative_x,
                                   uint32_t index_offset, uint32_t* np_added, uint32_t* ni_added) {
  auto wall_p = arch::make_wall_params(
    bounds, index_offset, hole_res, seg_res, alloc, np_added, ni_added, positive_x, negative_x);
  arch::make_wall(wall_p);
}

void compute_wall_segment_geometry(const OBB3f& bounds,
                                   const std::vector<arch::WallHole>& holes,
                                   const arch::GeometryAllocators& alloc,
                                   arch::FaceConnectorIndices* positive_x,
                                   arch::FaceConnectorIndices* negative_x,
                                   uint32_t* np_added, uint32_t* ni_added) {
  auto hole_res = make_debug_wall(holes, 1.0f);
  auto seg_res = make_debug_straight_flat_segments();
  arch::clear_geometry_allocators(&alloc);
  compute_wall_segment_geometry(
    bounds, hole_res, seg_res, alloc, positive_x, negative_x, 0, np_added, ni_added);
}

[[maybe_unused]]
void compute_curved_vertical_connection(arch::GridCache* grid_cache, const OBB3f& bounds,
                                        const arch::GeometryAllocators& alloc, uint32_t index_offset,
                                        uint32_t* num_points_added, uint32_t* num_inds_added) {
  arch::require_triangulated_grid(grid_cache, 7, 4);
  auto grid = arch::acquire_triangulated_grid(grid_cache, 7, 4);
  arch::CurvedVerticalConnectionParams curved_p{};
  curved_p.xy = grid;
  curved_p.xz = grid;
  curved_p.min_y = 0.25f;
  curved_p.bounds = bounds;
  curved_p.index_offset = index_offset;
  curved_p.alloc = alloc;
  curved_p.power = 2.0f;
  curved_p.target_lower = true;
  curved_p.num_points_added = num_points_added;
  curved_p.num_indices_added = num_inds_added;
  arch::make_curved_vertical_connection(curved_p);
}

void compute_arch_wall(arch::GridCache* grid_cache, const OBB3f& bounds,
                       const arch::GeometryAllocators& alloc, uint32_t index_offset,
                       uint32_t* num_points_added, uint32_t* num_inds_added) {
  const int num_xt = 4;
  const int num_xz = 20;
  const int num_side = 3;

  arch::require_triangulated_grid(grid_cache, num_xt, num_xz);
  arch::require_triangulated_grid(grid_cache, num_side, num_xz);
  arch::require_triangulated_grid(grid_cache, num_side, 3);
  arch::require_triangulated_grid(grid_cache, num_xt, 3);

  auto arch_t_xz = arch::acquire_triangulated_grid(grid_cache, num_xt, num_xz);
  auto arch_t_yz = arch::acquire_triangulated_grid(grid_cache, num_side, num_xz);
  auto straight_t_yz = arch::acquire_triangulated_grid(grid_cache, num_side, 3);
  auto straight_t_xz = arch::acquire_triangulated_grid(grid_cache, num_xt, 3);

  arch::ArchWallParams arch_p{};
  arch_p.arch_xz = arch_t_xz;
  arch_p.arch_yz = arch_t_yz;
  arch_p.straight_yz = straight_t_yz;
  arch_p.straight_xz = straight_t_xz;
  arch_p.outer_radius = 2.0f;
  arch_p.inner_radius = 1.0f;
  arch_p.side_additional_width = 0.0f;
  arch_p.side_additional_width_power = 0.25f;
  arch_p.straight_length_scale = 2.0f;
  arch_p.width = 0.5f;
  arch_p.index_offset = index_offset;
  arch_p.alloc = alloc;
  arch_p.bounds = bounds;
  arch_p.num_points_added = num_points_added;
  arch_p.num_indices_added = num_inds_added;
  arch::make_arch_wall(arch_p);
}

struct DebugComputeWallGeometryResult {
  std::vector<Vec3f> ps;
  std::vector<Vec3f> ns;
  std::vector<uint32_t> inds;
  arch::FaceConnectorIndices debug_wall_positive_x;
  arch::FaceConnectorIndices debug_wall_negative_x;
  ray_project::NonAdjacentConnections non_adjacent_connections;
  std::vector<DebugArchComponent::DebugCube> debug_cubes;
};

void apply_remapping(uint32_t* inds, uint32_t num_inds, const arch::IndexMap& remap) {
  for (uint32_t i = 0; i < num_inds; i++) {
    if (auto it = remap.find(inds[i]); it != remap.end()) {
      inds[i] = it->second;
    }
  }
}

struct RemapWithinTolParams {
  const void* src;
  uint32_t src_stride;
  uint32_t src_p_offset;
  uint32_t src_n_offset;
  const void* src_indices;
  IntegralType src_index_type;
  uint32_t num_src_indices;
  int32_t src_read_index_offset;  //  read from `src` at index src_index + src_read_index_offset
  int32_t src_write_index_offset; //  remap using src_index + `src_write_index_offset`

  const void* target;
  uint32_t target_stride;
  uint32_t target_p_offset;
  uint32_t target_n_offset;
  const void* target_indices;
  IntegralType target_index_type;
  uint32_t num_target_indices;
  int32_t target_read_index_offset;
  int32_t target_write_index_offset;

  float tol;
  float n_cos_theta;  //  if > 0.0, test whether dot(src_normal, target_normal) > this thresh.
};

//  For each index in `target`, see if the point is within `tol` of a point in `src`. If so,
//  set the target index equal to the source index.
uint32_t remap_within_tol(arch::IndexMap& remap, const RemapWithinTolParams& params) {
  auto apply_offset = [](uint32_t pi, int32_t off) {
    if (off < 0) {
      assert(pi >= uint32_t(std::abs(off)));
      pi -= std::abs(off);
    } else {
      pi += off;
    }
    return pi;
  };

  auto to_u32_index = [](IntegralType index_type, const void* indices, uint32_t i) -> uint32_t {
    switch (index_type) {
      case IntegralType::UnsignedShort: {
        auto* srci = static_cast<const unsigned char*>(indices) + i * sizeof(uint16_t);
        uint16_t ind;
        memcpy(&ind, srci, sizeof(uint16_t));
        return uint32_t(ind);
      }
      case IntegralType::UnsignedInt: {
        auto* srci = static_cast<const unsigned char*>(indices) + i * sizeof(uint32_t);
        uint32_t ind;
        memcpy(&ind, srci, sizeof(uint32_t));
        return ind;
      }
      default: {
        assert(false);
        return 0;
      }
    }
  };

  auto src_ptr = [&](uint32_t pi, uint32_t off) {
    auto* src = static_cast<const unsigned char*>(params.src);
    return src + pi * params.src_stride + off;
  };

  auto target_ptr = [&](uint32_t pi, uint32_t off) {
    auto* target = static_cast<const unsigned char*>(params.target);
    return target + pi * params.target_stride + off;
  };

  auto read_target = [&](uint32_t pi, uint32_t off) {
    Vec3f p;
    memcpy(&p, target_ptr(pi, off), sizeof(Vec3f));
    return p;
  };

  auto read_src = [&](uint32_t pi, uint32_t off) {
    Vec3f p;
    memcpy(&p, src_ptr(pi, off), sizeof(Vec3f));
    return p;
  };

  uint32_t num_remapped{};
  const bool use_normal_crit = params.n_cos_theta > 0.0f;
  for (uint32_t i = 0; i < params.num_target_indices; i++) {
    const uint32_t targi = to_u32_index(params.target_index_type, params.target_indices, i);
    const uint32_t read_target_pi = apply_offset(targi, params.target_read_index_offset);
    const uint32_t write_target_pi = apply_offset(targi, params.target_write_index_offset);
    const Vec3f target_p = read_target(read_target_pi, params.target_p_offset);
    Vec3f target_n{};
    if (use_normal_crit) {
      target_n = read_target(read_target_pi, params.target_n_offset);
    }

    for (uint32_t j = 0; j < params.num_src_indices; j++) {
      const uint32_t srci = to_u32_index(params.src_index_type, params.src_indices, j);
      const uint32_t read_src_pi = apply_offset(srci, params.src_read_index_offset);
      const uint32_t write_src_pi = apply_offset(srci, params.src_write_index_offset);
      const Vec3f src_p = read_src(read_src_pi, params.src_p_offset);
      bool normal_crit = true;
      if (use_normal_crit) {
        const Vec3f src_n = read_src(read_src_pi, params.src_n_offset);
        normal_crit = dot(src_n, target_n) > params.n_cos_theta;
      }

      const Vec3f diff = abs(target_p - src_p);
      if (normal_crit && diff.x < params.tol && diff.y < params.tol && diff.z < params.tol) {
        remap[write_target_pi] = write_src_pi;
        num_remapped++;
        break;
      }
    }
  }

  return num_remapped;
}

struct RemapRangeWithTolParams {
  const arch::FaceConnectorIndices* i0;
  const arch::FaceConnectorIndices* i1;
  uint32_t i0_offset;
  uint32_t i1_offset;
  arch::IndexMap* remap;
  const void* data0;
  const void* data1;
  uint32_t stride;
  uint32_t p_off;
  float tol;
};

bool is_range_equal_within_tol(uint32_t xi, const RemapRangeWithTolParams& params) {
  auto& i0 = *params.i0;
  auto& i1 = *params.i1;
  const uint32_t i0_offset = params.i0_offset;
  const uint32_t i1_offset = params.i1_offset;

  uint32_t np_match = i0.xi_size(xi);
  assert(np_match == i1.xi_size(xi));
  const auto* ps_char0 = static_cast<const unsigned char*>(params.data0);
  const auto* ps_char1 = static_cast<const unsigned char*>(params.data1);

  for (uint32_t i = 0; i < np_match; i++) {
    uint32_t pi_old = i0.xi_ith(xi, i) + i0_offset;
    uint32_t pi_new = i1.xi_ith(xi, i) + i1_offset;
    Vec3f p_old;
    Vec3f p_new;
    memcpy(&p_old, ps_char0 + pi_old * params.stride + params.p_off, sizeof(Vec3f));
    memcpy(&p_new, ps_char1 + pi_new * params.stride + params.p_off, sizeof(Vec3f));
    auto diff = abs(p_old - p_new);
    if (diff.x >= params.tol || diff.y >= params.tol || diff.z >= params.tol) {
      return false;
    }
  }

  return true;
}

bool remap_range_within_tol(uint32_t xi, const RemapRangeWithTolParams& params) {
  auto& i0 = *params.i0;
  auto& i1 = *params.i1;
  const uint32_t i0_offset = params.i0_offset;
  const uint32_t i1_offset = params.i1_offset;

  uint32_t np_match = i0.xi_size(xi);
  assert(np_match == i1.xi_size(xi));
  if (!is_range_equal_within_tol(xi, params)) {
    return false;
  }

  for (uint32_t i = 0; i < np_match; i++) {
    uint32_t pi_old = i0.xi_ith(xi, i) + i0_offset;
    uint32_t pi_new = i1.xi_ith(xi, i) + i1_offset;
    (*params.remap)[pi_new] = pi_old;
  }
  return true;
}

Vec2f keep_xz(const Vec3f& v) {
  return Vec2f{v.x, v.z};
}

[[maybe_unused]]
float ray_ray_distance(const Vec2f& p0, const Vec2f& d0, const Vec2f& p1, const Vec2f& d1) {
  //  Lengyel, E. Mathematics for 3D Game Programming and Computer Graphics. pp 96.
  auto d = dot(d0, d1);
  float denom = (d * d - (dot(d0, d0) * dot(d1, d1)));

  if (denom == 0.0f) {
    //  rays are parallel.
    auto q = p0 + d0;
    auto qs = q - p1;
    auto qs_proj = dot(qs, d1) / dot(d1, d1) * d1;
    auto d2 = dot(qs, qs) - dot(qs_proj, qs_proj);
    return std::sqrt(d2);
  }

  Vec2f col0{-dot(d1, d1), -dot(d0, d1)};
  Vec2f col1{dot(d0, d1), dot(d0, d0)};
  Vec2f t{dot(p1 - p0, d0), dot(p1 - p0, d1)};
  Vec2f ts = (1.0f / denom) * (t.x * col0 + t.y * col1);
  Vec2f v = (p0 + d0 * ts.x) - (p1 + d1 * ts.y);
  return v.length();
}

uint32_t compute_num_non_adjacent_edge_indices(const arch::FaceConnectorIndices& i0, uint32_t xi) {
  auto xi_size = i0.xi_size(xi);
  return xi_size == 0 ? 0 : (xi_size - 1) * 2;
}

void push_face_connector_edge_indices(const arch::FaceConnectorIndices& i0, uint32_t xi,
                                      uint32_t* dst) {
  for (uint32_t i = 1; i < i0.xi_size(xi); i++) {
    *dst++ = i0.xi_ith(xi, i - 1);
    *dst++ = i0.xi_ith(xi, i);
  }
}

void push_mutual_non_adjacent_connections_y(ray_project::NonAdjacentConnections* connections,
                                            const std::vector<uint32_t>& i0,
                                            const std::vector<uint32_t>& i1,
                                            const tri::EdgeToIndex<uint32_t>& edge_indices,
                                            const void* data, uint32_t stride,
                                            uint32_t p_off, float tol) {
  constexpr int axis = 1;
  //  connect edges i0 -> i1
  push_axis_aligned_non_adjacent_connections(
    connections,
    i0.data(), uint32_t(i0.size()),
    i1.data(), uint32_t(i1.size()),
    edge_indices, data, stride, p_off, tol, axis);
  //  connect edges i1 -> i0
  push_axis_aligned_non_adjacent_connections(
    connections,
    i1.data(), uint32_t(i1.size()),
    i0.data(), uint32_t(i0.size()),
    edge_indices, data, stride, p_off, tol, axis);
}

DebugComputeWallGeometryResult compute_wall_geometry(DebugArchComponent& component) {
  constexpr uint32_t max_num_points_per_segment = 4096;
  constexpr uint32_t max_num_indices_per_segment = max_num_points_per_segment * 3;

  LinearAllocator ps_alloc;
  LinearAllocator ns_alloc;
  LinearAllocator inds_alloc;
  LinearAllocator tmp_alloc;
  std::unique_ptr<unsigned char[]> heap_data;

  {
    size_t sizes[4] = {
      sizeof(Vec3f) * max_num_points_per_segment,
      sizeof(Vec3f) * max_num_points_per_segment,
      sizeof(uint32_t) * max_num_indices_per_segment,
      sizeof(uint32_t) * max_num_points_per_segment
    };
    LinearAllocator allocs[4] = {};
    heap_data = make_linear_allocators_from_heap(sizes, allocs, 4);
    ps_alloc = allocs[0];
    ns_alloc = allocs[1];
    inds_alloc = allocs[2];
    tmp_alloc = allocs[3];
  }

  auto& params = component.params;
  auto& wall_holes = component.wall_holes;
  auto& store_wall_hole_res = component.store_wall_hole_result;
#if 0
  place_walls_on_grid(*this);
#else
  params.debug_wall_bounds = arch::make_obb_xz(
    params.debug_wall_offset, params.debug_wall_theta, params.debug_wall_scale);
  params.debug_wall_bounds2 = arch::extrude_obb_xz(
    params.debug_wall_bounds, params.extruded_theta, Vec3f{22.0f, 22.0f, params.debug_wall_scale.z});
  const auto bounds3 = arch::extrude_obb_xz(
    params.debug_wall_bounds2, params.extruded_theta, Vec3f{11.0f, 11.0f, params.debug_wall_scale.z});
  const auto bounds4 = arch::extrude_obb_xz(
    bounds3, params.extruded_theta, Vec3f{32.0f, 32.0f, params.debug_wall_scale.z});
#endif

  auto wall_hole_res = make_debug_wall(wall_holes, params.debug_wall_aspect_ratio);
  auto seg_res = make_debug_straight_flat_segments();
  std::vector<Vec3f> wall_p;
  std::vector<Vec3f> wall_n;
  std::vector<uint32_t> wall_tris;
  arch::IndexMap remap_indices;

  arch::FaceConnectorIndices wall_positive_x{};
  arch::FaceConnectorIndices wall_negative_x{};

  auto make_wall_params = [&](const OBB3f& obb, uint32_t np,
                             uint32_t* np_added, uint32_t* ni_added) {
    auto alloc = arch::make_geometry_allocators(&ps_alloc, &ns_alloc, &inds_alloc, &tmp_alloc);
    return arch::make_wall_params(
      obb, np, wall_hole_res, seg_res, alloc,
      np_added, ni_added, &wall_positive_x, &wall_negative_x);
  };
  auto append = [&](uint32_t np_added, uint32_t ni_added) {
    auto curr_num_points = wall_p.size();
    auto curr_num_inds = wall_tris.size();
    wall_p.resize(wall_p.size() + np_added);
    wall_n.resize(wall_n.size() + np_added);
    wall_tris.resize(wall_tris.size() + ni_added);
    memcpy(wall_p.data() + curr_num_points, ps_alloc.begin, size(&ps_alloc));
    memcpy(wall_n.data() + curr_num_points, ns_alloc.begin, size(&ns_alloc));
    memcpy(wall_tris.data() + curr_num_inds, inds_alloc.begin, size(&inds_alloc));
  };
  auto clear_allocs = [&]() {
    clear(&ps_alloc);
    clear(&ns_alloc);
    clear(&inds_alloc);
    clear(&tmp_alloc);
  };
  auto make_allocs = [&]() {
    return arch::make_geometry_allocators(&ps_alloc, &ns_alloc, &inds_alloc, &tmp_alloc);
  };

  const OBB3f obbs[4] = {params.debug_wall_bounds, params.debug_wall_bounds2, bounds3, bounds4};
  arch::FaceConnectorIndices pos_connectors[4];
  arch::FaceConnectorIndices neg_connectors[4];

  arch::FaceConnectorIndices curved_pos_connectors[4];
  arch::FaceConnectorIndices curved_neg_connectors[4];

  uint32_t wall_i{};
  uint32_t last_offset{};
  arch::FaceConnectorIndices last_wall_positive_x{};
  arch::FaceConnectorIndices last_wall_negative_x{};

  std::vector<DebugArchComponent::DebugCube> debug_cubes;

  for (auto& obb : obbs) {
    uint32_t np_added;
    uint32_t ni_added;
    auto curr_num_points = uint32_t(wall_p.size());
    auto wall_params = make_wall_params(obb, curr_num_points, &np_added, &ni_added);
    clear_allocs();
    arch::make_wall(wall_params);
    append(np_added, ni_added);
#if 0 //  remap
    if (wall_i > 0) {
      RemapRangeWithTolParams remap_params{};
      remap_params.tol = 1e-4f;
      remap_params.i0 = &last_wall_positive_x;
      remap_params.i1 = &wall_negative_x;
      remap_params.data0 = wall_p.data();
      remap_params.data1 = wall_p.data();
      remap_params.remap = &remap_indices;
      remap_params.stride = sizeof(Vec3f);
      remap_params.p_off = 0;
      remap_params.tol = 1e-4f;
      remap_range_within_tol(0, remap_params);
      remap_range_within_tol(1, remap_params);
    }
#endif
#if 0
    if (wall_i == 0) {
      store_wall_hole_res.positions = wall_p;
      store_wall_hole_res.normals = wall_n;
      store_wall_hole_res.triangles.resize(wall_tris.size() / 3);
      for (int i = 0; i < int(store_wall_hole_res.triangles.size()); i++) {
        auto& t = store_wall_hole_res.triangles[i];
        uint32_t i3[3]{wall_tris[i * 3], wall_tris[i * 3 + 1], wall_tris[i * 3 + 2]};
        memcpy(t.i, i3, 3 * sizeof(uint32_t));
      }
    }
#endif
#if 1 //  add curved segments
    if (wall_i > 0) {
      auto& curve_pos_x = curved_pos_connectors[wall_i];
      curve_pos_x = {};
      auto& curve_neg_x = curved_neg_connectors[wall_i];
      curve_neg_x = {};

      auto& last_obb = obbs[wall_i-1];
      auto& curr_obb = obbs[wall_i];
      auto use_obb = last_obb.half_size.y < curr_obb.half_size.y ? last_obb : curr_obb;

      auto num_wall_ps = uint32_t(wall_hole_res.positions.size());
      const auto ind_p00 = last_offset + wall_hole_res.bot_r_ind;
      const auto ind_p01 = last_offset + wall_hole_res.bot_r_ind + num_wall_ps;
      const auto ind_p10 = curr_num_points + wall_hole_res.bot_l_ind;
      const auto ind_p11 = curr_num_points + wall_hole_res.bot_l_ind + num_wall_ps;
      uint32_t np_added_adj{};
      uint32_t ni_added_adj{};
      clear_allocs();
      add_adjoining_curved_segment(
        keep_xz(wall_p[ind_p00]), keep_xz(wall_p[ind_p01]),
        keep_xz(wall_p[ind_p10]), keep_xz(wall_p[ind_p11]),
        keep_xz(wall_n[ind_p01]), keep_xz(wall_n[ind_p11]),
        uint32_t(wall_p.size()), make_allocs(),
        use_obb, &curve_pos_x, &curve_neg_x, &np_added_adj, &ni_added_adj);
      append(np_added_adj, ni_added_adj);

      for (uint32_t i = 0; i < curve_pos_x.xi_size(0); i++) {
        DebugArchComponent::DebugCube cube{};
        cube.color = Vec3f{0.0f, 1.0f, float(i)/float(curve_pos_x.xi_size(0)-1)};
        cube.p = wall_p[curve_pos_x.xi_ith(0, i)];
        cube.s = Vec3f{0.1f};
        debug_cubes.push_back(cube);
      }
      for (uint32_t i = 0; i < curve_neg_x.xi_size(0); i++) {
        DebugArchComponent::DebugCube cube{};
        cube.color = Vec3f{1.0f, float(i)/float(curve_neg_x.xi_size(0)-1), 0.0f};
        cube.p = wall_p[curve_neg_x.xi_ith(0, i)];
        cube.s = Vec3f{0.1f};
        debug_cubes.push_back(cube);
      }
    }
#endif
    auto pos_connector = wall_positive_x;
    auto neg_connector = wall_negative_x;
    pos_connectors[wall_i] = pos_connector;
    neg_connectors[wall_i] = neg_connector;

    wall_i++;
    last_offset = curr_num_points;
    last_wall_positive_x = wall_positive_x;
    last_wall_negative_x = wall_negative_x;
  }

#if 0
  {
    auto rot = make_rotation(pif() * 0.2f);
    auto next_x = rot * Vec2f{bounds4.i.x, bounds4.i.z};
    auto next_z = rot * Vec2f{bounds4.k.x, bounds4.k.z};
    auto next_x3 = Vec3f{next_x.x, 0.0f, next_x.y};
    auto next_z3 = Vec3f{next_z.x, 0.0f, next_z.y};
    auto base_p = bounds4.position + bounds4.half_size.x * bounds4.i;
    base_p += bounds4.half_size.z * bounds4.k;
    auto next_p = base_p + next_x3 * bounds4.half_size.x;
    next_p -= bounds4.half_size.z * next_z3;
    OBB3f next_obb;
    {
      next_obb.half_size = bounds4.half_size;
      next_obb.position = next_p;
      next_obb.i = next_x3;
      next_obb.j = bounds4.j;
      next_obb.k = next_z3;

      uint32_t np_added;
      uint32_t ni_added;
      auto wall_params = make_wall_params(next_obb, uint32_t(wall_p.size()), &np_added, &ni_added);
      clear_allocs();
      arch::make_wall(wall_params);
      append(np_added, ni_added);
    }
    {
      auto base_obb = next_obb;
      arch::require_triangulated_grid(&component.grid_cache, 8, 2);
      auto grid = arch::acquire_triangulated_grid(&component.grid_cache, 8, 2);

      base_obb.position.y -= base_obb.half_size.y * 2.0f;
      base_obb.position += (next_obb.half_size.x - next_obb.half_size.z) * base_obb.i;
      base_obb.half_size.z *= 0.25f;
      base_obb.half_size.x = base_obb.half_size.z;
      uint32_t np_added;
      uint32_t ni_added;
      arch::PoleParams pole_p{};
      pole_p.bounds = base_obb;
      pole_p.grid = grid;
      pole_p.index_offset = uint32_t(wall_p.size());
      pole_p.alloc = make_allocs();
      pole_p.num_points_added = &np_added;
      pole_p.num_indices_added = &ni_added;
      clear_allocs();
      arch::make_pole(pole_p);
      append(np_added, ni_added);
    }

    OBB3f next_obb2;
    {
      next_obb2 = arch::extrude_obb_xz(bounds4, -pif() * 0.2f, params.debug_wall_scale);
      uint32_t np_added;
      uint32_t ni_added;
      auto wall_params = make_wall_params(next_obb2, uint32_t(wall_p.size()), &np_added, &ni_added);
      clear_allocs();
      arch::make_wall(wall_params);
      append(np_added, ni_added);
    }

    OBB3f curve_obb;
    {
      curve_obb = arch::extrude_obb_xz(next_obb2, -pif() * 0.2f, params.debug_wall_scale);
      uint32_t np_added{};
      uint32_t ni_added{};
      clear_allocs();
      compute_curved_vertical_connection(
        &component.grid_cache,
        curve_obb,
        make_allocs(),
        uint32_t(wall_p.size()),
        &np_added, &ni_added);
      append(np_added, ni_added);
    }
    OBB3f little_obb;
    {
      uint32_t np_added;
      uint32_t ni_added;
      auto next_scl = params.debug_wall_scale;
      next_scl.y *= 0.25f;
      little_obb = arch::extrude_obb_xz(curve_obb, 0.0f, next_scl);
      auto wall_params = make_wall_params(little_obb, uint32_t(wall_p.size()), &np_added, &ni_added);
      clear_allocs();
      arch::make_wall(wall_params);
      append(np_added, ni_added);
    }
    {
      auto next_scl = params.debug_wall_scale;
//      next_scl.x *= 0.5f;
      auto arch_obb = arch::extrude_obb_xz(little_obb, -pif() * 0.5f, next_scl);
      uint32_t num_points_added{};
      uint32_t num_inds_added{};
      clear_allocs();
      compute_arch_wall(
        &component.grid_cache,
        arch_obb,
        make_allocs(),
        uint32_t(wall_p.size()),
        &num_points_added, &num_inds_added);
      append(num_points_added, num_inds_added);
    }
  }
#endif

  ray_project::NonAdjacentConnections non_adjacent_connections;
  auto edge_indices = tri::build_edge_to_index_map(wall_tris.data(), uint32_t(wall_tris.size()/3));
  const float non_adj_eps = 1e-3f;
//  const float non_adj_eps = 2.0f;
  for (uint32_t i = 1; i < wall_i; i++) {
    const uint32_t stride = sizeof(Vec3f);

    auto pos0 = pos_connectors[i-1];
    auto neg1 = neg_connectors[i];
    std::vector<uint32_t> posi(compute_num_non_adjacent_edge_indices(pos0, 0));
    std::vector<uint32_t> negi(compute_num_non_adjacent_edge_indices(neg1, 0));

    for (uint32_t j = 0; j < 2; j++) {
      push_face_connector_edge_indices(pos0, j, posi.data());
      push_face_connector_edge_indices(neg1, j, negi.data());
      push_mutual_non_adjacent_connections_y(
        &non_adjacent_connections, posi, negi, edge_indices, wall_p.data(), stride, 0, non_adj_eps);
    }

#if 1
    push_face_connector_edge_indices(pos0, 1, posi.data());
    push_face_connector_edge_indices(neg1, 1, negi.data());

    auto& curved_neg1 = curved_neg_connectors[i];
    auto& curved_pos1 = curved_pos_connectors[i];

    std::vector<uint32_t> curvedi(compute_num_non_adjacent_edge_indices(curved_neg1, 0));
    push_face_connector_edge_indices(curved_neg1, 0, curvedi.data());
    push_mutual_non_adjacent_connections_y(
      &non_adjacent_connections, posi, curvedi, edge_indices, wall_p.data(), stride, 0, non_adj_eps);

    push_face_connector_edge_indices(curved_pos1, 0, curvedi.data());
    push_mutual_non_adjacent_connections_y(
      &non_adjacent_connections, curvedi, negi, edge_indices, wall_p.data(), stride, 0, non_adj_eps);
#endif
  }
  ray_project::build_non_adjacent_connections(&non_adjacent_connections);

#if 1
//  apply_remapping(wall_tris.data(), uint32_t(wall_tris.size()), remap_indices);
  store_wall_hole_res.positions = wall_p;
  store_wall_hole_res.normals = wall_n;
  store_wall_hole_res.triangles.resize(wall_tris.size() / 3);
  for (int i = 0; i < int(store_wall_hole_res.triangles.size()); i++) {
    auto& t = store_wall_hole_res.triangles[i];
    uint32_t i3[3]{wall_tris[i * 3], wall_tris[i * 3 + 1], wall_tris[i * 3 + 2]};
    memcpy(t.i, i3, 3 * sizeof(uint32_t));
  }
#endif

  DebugComputeWallGeometryResult result;
  result.debug_wall_positive_x = wall_positive_x;
  result.debug_wall_negative_x = wall_negative_x;
  result.ps = std::move(wall_p);
  result.ns = std::move(wall_n);
  result.inds = std::move(wall_tris);
  result.non_adjacent_connections = std::move(non_adjacent_connections);
  result.debug_cubes = std::move(debug_cubes);
  return result;
}

void visualize_non_adjacent_connection(const ray_project::NonAdjacentConnections& connections,
                                       uint32_t ith_tri, const uint32_t* tris, const Vec3f* ps) {
  if (ith_tri == 0) {
    return;
  }

  uint32_t last_ti{~0u};
  uint32_t ith{};
  uint32_t entry_ind{};
  const auto num_entries = uint32_t(connections.entries.size());
  for (uint32_t i = 0; i < num_entries; i++) {
    auto& entry = connections.entries[i];
    if (entry.src.ti != last_ti) {
      last_ti = entry.src.ti;
      entry_ind = i;
      if (++ith == ith_tri) {
        break;
      }
    }
  }

  if (ith != ith_tri) {
    return;
  }

  auto i0 = entry_ind;
  while (i0 < num_entries) {
    auto& entry = connections.entries[i0];
    if (entry.src.ti != last_ti) {
      break;
    } else {
      i0++;
    }
  }

  auto entry_size = i0 - entry_ind;
  if (entry_size > 0) {
    auto* tri_beg = tris + last_ti * 3;
    auto& p0 = ps[tri_beg[0]];
    auto& p1 = ps[tri_beg[1]];
    auto& p2 = ps[tri_beg[2]];
    vk::debug::draw_triangle_edges(p0, p1, p2, Vec3f{1.0f});
  }

  uint32_t entry_count{};
  while (entry_count < entry_size) {
    auto& entry = connections.entries[entry_ind];
    auto& src_p0 = ps[entry.src.edge.i0];
    auto& src_p1 = ps[entry.src.edge.i1];
    auto& targ_p0 = ps[entry.target.edge.i0];
    auto& targ_p1 = ps[entry.target.edge.i1];
    vk::debug::draw_line(targ_p0, targ_p1, Vec3f{float(entry_ind) / float(entry_size), 0.0f, 0.0f});
    vk::debug::draw_line(src_p0, src_p1, Vec3f{0.0f, 1.0f, 0.0f});
    entry_ind++;
    entry_count++;
  }
}

void prepare_growable_geometry(const Vec3f* src, Vec3f* dst,
                               const uint32_t* src_tris, uint32_t num_src_tris) {
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

void append_grown_geometry(StructureGeometry* geom) {
  const auto& src = geom->growing_geometry_src;
  const auto& srci = geom->growing_triangles_src;
  auto& dst = geom->aggregate_geometry;
  auto& dsti = geom->aggregate_triangles;

  const auto orig_size = uint32_t(dst.size());
  const auto incoming_size = uint32_t(src.size());
  dst.resize(dst.size() + src.size());
  memcpy(dst.data() + orig_size, src.data(), incoming_size * sizeof(Vec3f));

  const uint32_t orig_num_verts = orig_size / 2;
  const auto orig_tri_size = uint32_t(dsti.size());
  dsti.resize(dsti.size() + srci.size());

  uint32_t ind{};
  for (uint32_t pi : srci) {
    pi += orig_num_verts;
    assert(pi < (1u << 16u));
    dsti[orig_tri_size + ind] = uint16_t(pi);
    ind++;
  }
}

void initialize_triangle_growth(StructureGeometry* geom,
                                arch::RenderTriangleGrowthContext* context) {
  const auto stride = uint32_t(geom->growing_geometry_vertex_stride_bytes());
  arch::initialize_triangle_growth(
    context,
    geom->growing_triangles_src.data(),
    geom->num_growing_triangles_src,
    geom->growing_geometry_src.data(), stride, 0,
    geom->growing_geometry_dst.data(), stride, 0);
}

void initialize_triangle_growth(SegmentedStructure* structure) {
  assert(structure->growth_state == StructureGrowthState::Idle);
  initialize_triangle_growth(&structure->geometry, &structure->triangle_growth_context);
  structure->growth_state = StructureGrowthState::Growing;
}

void initialize_triangle_recede(StructureGeometry* geom,
                                arch::RenderTriangleRecedeContext* context) {
  const auto stride = uint32_t(geom->growing_geometry_vertex_stride_bytes());
  arch::initialize_triangle_recede(
    context,
    geom->growing_triangles_src.data(),
    geom->num_growing_triangles_src,
    geom->growing_geometry_src.data(), stride, 0,
    geom->growing_geometry_dst.data(), stride, 0);
}

struct TreeNodesPendingPrune {
  tree::TreeInstanceHandle handle{};
  tree::Internodes dst_internodes;
  std::vector<int> dst_to_src;
};

using BoundsIDVec = std::vector<bounds::ElementID>;
using BoundsIDSet = std::unordered_set<bounds::ElementID, bounds::ElementID::Hash>;
using LeafBoundsIDMap = std::unordered_map<bounds::ElementID, BoundsIDSet, bounds::ElementID::Hash>;
using ReevaluateLeafBoundsMap = std::unordered_map<tree::TreeInstanceHandle,
                                                   BoundsIDVec,
                                                   tree::TreeInstanceHandle::Hash>;
struct ComputeWallHolesAroundTreeNodesResult {
  std::vector<arch::WallHole> holes;
  std::vector<TreeNodesPendingPrune> pending_prune;
  ReevaluateLeafBoundsMap reevaluate_leaf_bounds;
};

struct ComputeWallHolesAroundTreeNodesParams {
  OBB3f wall_bounds;
  const tree::TreeSystem* tree_system;
  tree::TreeNodeCollisionWithObjectContext* collision_context;
  const DebugArchComponent::CollideThroughHoleParams* collide_through_hole_params;
};

struct TreeNodeBoundsIntersectResult {
  bool any_hit;
  BoundsIDSet parent_ids_from_internodes;
  LeafBoundsIDMap leaf_element_ids_by_parent_id;
};

TreeNodeBoundsIntersectResult tree_node_bounds_intersect(const bounds::Accel* accel,
                                                         const OBB3f& query_bounds,
                                                         bounds::ElementTag tree_bounds_tag,
                                                         bounds::ElementTag leaf_bounds_tag) {
  TreeNodeBoundsIntersectResult result{};

  std::vector<const bounds::Element*> isect;
  accel->intersects(bounds::make_query_element(query_bounds), isect);
  result.any_hit = !isect.empty();

  for (const bounds::Element* el : isect) {
    if (el->tag == tree_bounds_tag.id) {
      result.parent_ids_from_internodes.insert(bounds::ElementID{el->parent_id});

    } else if (el->tag == leaf_bounds_tag.id) {
      bounds::ElementID parent_id{el->parent_id};
      bounds::ElementID el_id{el->id};
      if (result.leaf_element_ids_by_parent_id.count(parent_id) == 0) {
        result.leaf_element_ids_by_parent_id[parent_id] = BoundsIDSet{el_id};
      } else {
        result.leaf_element_ids_by_parent_id.at(parent_id).insert(el_id);
      }
    }
  }

  return result;
}

template <typename Container>
bool can_prune_candidates(const tree::TreeSystem* sys, const Container& candidates) {
  for (tree::TreeInstanceHandle candidate : candidates) {
    if (!tree::can_start_pruning(sys, candidate)) {
      return false;
    }
  }
  return true;
}

std::vector<tree::TreeInstanceHandle> lookup_tree_instances(const tree::TreeSystem* sys,
                                                            const BoundsIDSet& from_bounds_ids) {
  std::vector<tree::TreeInstanceHandle> result;
  for (bounds::ElementID parent_id : from_bounds_ids) {
    if (auto handle = tree::lookup_instance_by_bounds_element_id(sys, parent_id)) {
      result.push_back(handle.value());
    }
  }
  return result;
}

std::vector<tree::TreeInstanceHandle> lookup_tree_instances(const tree::TreeSystem* sys,
                                                            const LeafBoundsIDMap& leaf_bounds_ids) {
  std::vector<tree::TreeInstanceHandle> result;
  for (auto& [parent_id, _] : leaf_bounds_ids) {
    if (auto handle = tree::lookup_instance_by_bounds_element_id(sys, parent_id)) {
      result.push_back(handle.value());
    }
  }
  return result;
}

ComputeWallHolesAroundTreeNodesResult
compute_wall_holes_around_tree_nodes(const TreeNodeBoundsIntersectResult& isect_res,
                                     const ComputeWallHolesAroundTreeNodesParams& params) {
  ComputeWallHolesAroundTreeNodesResult result{};

  if (!isect_res.any_hit) {
    return result;
  }

  auto& leaf_ids = isect_res.leaf_element_ids_by_parent_id;
  auto& candidate_tree_ids = isect_res.parent_ids_from_internodes;
  for (auto& [leaf_parent_id, element_ids] : leaf_ids) {
    auto tree_handle = tree::lookup_instance_by_bounds_element_id(
      params.tree_system, leaf_parent_id);
    if (tree_handle) {
      result.reevaluate_leaf_bounds[tree_handle.value()] = BoundsIDVec{
        element_ids.begin(), element_ids.end()};
    }
  }

  if (candidate_tree_ids.empty()) {
    return result;
  }

  std::vector<const tree::Internodes*> candidate_internodes;
  std::vector<tree::TreeInstanceHandle> candidate_handles;

  for (bounds::ElementID candidate_id : candidate_tree_ids) {
    auto tree_handle = tree::lookup_instance_by_bounds_element_id(
      params.tree_system, candidate_id);
    if (tree_handle) {
      const auto read_inst = tree::read_tree(params.tree_system, tree_handle.value());
      if (read_inst.nodes) {
        candidate_internodes.push_back(&read_inst.nodes->internodes);
        candidate_handles.push_back(tree_handle.value());
      }
    }
  }

  if (candidate_internodes.empty()) {
    return result;
  }

  constexpr int max_num_wall_holes = 4;
  std::vector<std::vector<arch::WallHole>> candidate_wall_holes;
  std::vector<tree::Internodes> pruned_internodes;
  std::vector<std::vector<int>> pruned_to_src;
  int max_num_found_holes_ind{};
  int max_num_found_holes{-1};

  for (int i = 0; i < int(candidate_internodes.size()); i++) {
    const tree::Internodes* src_nodes = candidate_internodes[i];

    auto& holes = candidate_wall_holes.emplace_back();
    holes.resize(max_num_wall_holes);

    TreeNodeCollisionWithWallParams collide_params{};
    collide_params.collision_context = params.collision_context;
    collide_params.collide_through_hole_params = params.collide_through_hole_params;
    collide_params.wall_bounds = params.wall_bounds;
    collide_params.src_internodes = src_nodes->data();
    collide_params.num_src_internodes = int(src_nodes->size());
    collide_params.accepted_holes = holes.data();
    collide_params.max_num_accepted_holes = max_num_wall_holes;
    auto collide_res = compute_collision_with_wall(collide_params);

    holes.resize(collide_res.num_accepted_bounds_components);
    if (collide_res.num_accepted_bounds_components > max_num_found_holes) {
      max_num_found_holes = collide_res.num_accepted_bounds_components;
      max_num_found_holes_ind = i;
    }

    auto& dst_inodes = pruned_internodes.emplace_back();
    dst_inodes.resize(collide_res.num_dst_internodes);
    std::copy(
      collide_res.dst_internodes,
      collide_res.dst_internodes + collide_res.num_dst_internodes,
      dst_inodes.data());

    auto& dst_to_src = pruned_to_src.emplace_back();
    dst_to_src.resize(collide_res.num_dst_internodes);
    std::copy(
      collide_res.dst_to_src,
      collide_res.dst_to_src + collide_res.num_dst_internodes,
      dst_to_src.data());
  }

  {
    auto& prune_through_hole = result.pending_prune.emplace_back();
    prune_through_hole.handle = candidate_handles[max_num_found_holes_ind];
    prune_through_hole.dst_internodes = std::move(pruned_internodes[max_num_found_holes_ind]);
    prune_through_hole.dst_to_src = std::move(pruned_to_src[max_num_found_holes_ind]);
  }

  for (int i = 0; i < int(candidate_internodes.size()); i++) {
    if (i == max_num_found_holes_ind) {
      continue;
    }

    const tree::Internodes* src_inodes = candidate_internodes[i];
    auto accept = std::make_unique<bool[]>(src_inodes->size());
    std::fill(accept.get(), accept.get() + src_inodes->size(), true);

    int ni{};
    for (auto& node : *src_inodes) {
      auto node_obb = tree::internode_obb(node);
      if (obb_obb_intersect(params.wall_bounds, node_obb)) {
        accept[ni] = false;
      }
      ni++;
    }

    auto dst_inodes = *src_inodes;
    std::vector<int> dst_to_src(src_inodes->size());
    dst_inodes.resize(tree::prune_rejected_axes(
      src_inodes->data(),
      accept.get(),
      int(src_inodes->size()),
      dst_inodes.data(),
      dst_to_src.data()));
    dst_to_src.resize(dst_inodes.size());

    auto& prune_through_hole = result.pending_prune.emplace_back();
    prune_through_hole.handle = candidate_handles[i];
    prune_through_hole.dst_internodes = std::move(dst_inodes);
    prune_through_hole.dst_to_src = std::move(dst_to_src);
  }

  result.holes = std::move(candidate_wall_holes[max_num_found_holes_ind]);
  return result;
}

Vec3f select_piece_scale(const DebugArchComponent& component) {
  const auto& wall_scale = component.params.debug_wall_scale;
  if (component.structure_growth_params.randomize_wall_scale) {
    const float scales[4]{16.0f, 20.0f, 24.0f, 32.0f};
    int x_ind = int(urand() * 4);
    int y_ind = int(urand() * 4);
    return Vec3f{scales[x_ind], scales[y_ind], wall_scale.z};
  } else {
    return wall_scale;
  }
}

struct ExtrudeGrowingStructureParams {
  bounds::Accel* accel;
  bounds::ElementTag terrain_bounds_tag;
  bounds::ElementTag arch_bounds_tag;
  bounds::RadiusLimiter* radius_limiter;
  bounds::RadiusLimiterElementTag roots_radius_limiter_tag;
  bounds::RadiusLimiterElementTag arch_radius_limiter_tag;
  Vec2f fit_target;
};

Optional<StructurePiece> generate_piece(const SegmentedStructure& structure,
                                        const DebugArchComponent& component,
                                        arch::FitBoundsToPointsContext* fit_context) {
  if (component.structure_growth_params.use_isect_wall_obb) {
    return Optional<StructurePiece>(make_structure_piece(component.isect_wall_obb));
  } else {
    auto next_bounds = extrude_bounds(&structure, fit_context, select_piece_scale(component));
    if (next_bounds) {
      return Optional<StructurePiece>(make_structure_piece(next_bounds.value()));
    } else {
      return NullOpt{};
    }
  }
}

bool accept_piece(const SegmentedStructure* structure, const StructurePiece& piece,
                  const ExtrudeGrowingStructureParams& params) {
  {
    std::vector<const bounds::Element*> hit;
    params.accel->intersects(bounds::make_query_element(piece.bounds), hit);
    for (const bounds::Element* el: hit) {
      if (el->tag == params.arch_bounds_tag.id) {
        const bool permit_isect = !structure->pieces.empty() &&
          el->id == structure->pieces.back().bounds_element_id.id;
        if (!permit_isect) {
          return false;
        }
      } else if (el->tag == params.terrain_bounds_tag.id) {
        return false;
      }
    }
  }
  {
    const bool hit_roots = bounds::intersects_other_tag(
      params.radius_limiter, piece.bounds, params.roots_radius_limiter_tag);
    if (hit_roots) {
      return false;
    }
  }
  return true;
}

Optional<StructurePiece> next_piece(const SegmentedStructure& structure,
                                    const DebugArchComponent& component,
                                    arch::FitBoundsToPointsContext* fit_context,
                                    const ExtrudeGrowingStructureParams& params) {
  auto maybe_piece = generate_piece(structure, component, fit_context);
  if (maybe_piece) {
    if (!accept_piece(&structure, maybe_piece.value(), params)) {
      maybe_piece = NullOpt{};
    }
  }
  return maybe_piece;
}

void deactivate_accel_bounds(const SegmentedStructure* structure,
                             bounds::BoundsSystem* bounds_system,
                             bounds::AccelInstanceHandle accel,
                             bounds::RadiusLimiter* radius_lim) {
  std::vector<bounds::ElementID> pending_deactivate;
  for (auto& piece : structure->pieces) {
    pending_deactivate.push_back(piece.bounds_element_id);
  }
  bounds::push_pending_deactivation(
    bounds_system, accel, std::move(pending_deactivate));

  for (auto& piece : structure->pieces) {
    bounds::remove(radius_lim, piece.radius_limiter_element);
  }
}

void clear_projected_tree_nodes(const SegmentedStructure* structure,
                                tree::ProjectedNodesSystem* proj_nodes_sys) {
  for (auto& growing : structure->growing_tree_nodes) {
    tree::destroy_instance(proj_nodes_sys, growing.proj_instance_handle);
  }
}

void initialize_params(ComputeWallHolesAroundTreeNodesParams* hole_params,
                       const OBB3f& bounds,
                       DebugArchComponent& component,
                       const DebugArchComponent::UpdateInfo& info) {
  hole_params->wall_bounds = bounds;
  hole_params->tree_system = &info.tree_system;
  hole_params->collision_context = &global_data.debug_collision_context;
  hole_params->collide_through_hole_params = &component.collide_through_hole_params;
}

bounds::Accel* request_accel_write(const DebugArchComponent& component,
                                   const DebugArchComponent::UpdateInfo& info) {
  return bounds::request_write(
    &info.bounds_system, info.accel_instance_handle, component.bounds_accessor_id);
}

void release_accel_write(const DebugArchComponent& component,
                         const DebugArchComponent::UpdateInfo& info) {
  bounds::release_write(
    &info.bounds_system, info.accel_instance_handle, component.bounds_accessor_id);
}

bounds::RadiusLimiterElement to_radius_limiter_element(const OBB3f& arch_obb,
                                                       bounds::RadiusLimiterAggregateID aggregate,
                                                       bounds::RadiusLimiterElementTag tag) {
  //  @NOTE: Swaps y and z.
  bounds::RadiusLimiterElement result{};
  result.radius = std::max(arch_obb.half_size.x, arch_obb.half_size.y);
  result.half_length = arch_obb.half_size.z;
  result.i = arch_obb.i;
  result.j = arch_obb.k;
  result.k = arch_obb.j;
  result.p = arch_obb.position;
  result.aggregate_id = aggregate;
  result.tag = tag;

  if (arch_obb.half_size.x > arch_obb.half_size.y) {
    result.p.y += arch_obb.half_size.x - arch_obb.half_size.y;
  }

  return result;
}

bounds::RadiusLimiterElement to_radius_limiter_element(const StructurePiece& piece,
                                                       bounds::RadiusLimiterElementTag arch_tag) {
  return to_radius_limiter_element(piece.bounds, piece.radius_limiter_aggregate_id, arch_tag);
}

bool extrude_growing_structure(DebugArchComponent& component,
                               SegmentedStructure& structure,
                               arch::FitBoundsToPointsContext& fit_context,
                               const ExtrudeGrowingStructureParams& params) {
  Optional<StructurePiece> maybe_piece;
  if (!component.structure_growth_params.restrict_structure_x_length ||
      piece_x_length(structure) < structure.max_piece_x_length) {
    if (structure.pieces.empty()) {
      arch::set_line_target(&fit_context, params.fit_target);
    }
    maybe_piece = next_piece(structure, component, &fit_context, params);
  }

  if (maybe_piece) {
    auto& piece = maybe_piece.value();

    params.accel->insert(bounds::make_element(
      piece.bounds,
      piece.bounds_element_id.id,
      piece.bounds_element_id.id,
      params.arch_bounds_tag.id));

    auto radius_lim_el = to_radius_limiter_element(piece, params.arch_radius_limiter_tag);
    piece.radius_limiter_element = bounds::insert(params.radius_limiter, radius_lim_el, false);

    add_piece(&structure, piece);

    return true;
  } else {
    return false;
  }
}

bool can_prune_all_candidates(const tree::TreeSystem* sys,
                              const TreeNodeBoundsIntersectResult& isect_res) {
  auto inst_handles0 = lookup_tree_instances(sys, isect_res.parent_ids_from_internodes);
  auto inst_handles1 = lookup_tree_instances(sys, isect_res.leaf_element_ids_by_parent_id);
  return can_prune_candidates(sys, inst_handles0) && can_prune_candidates(sys, inst_handles1);
}

std::vector<tree::TreeInstanceHandle>
start_pruning(std::vector<TreeNodesPendingPrune>&& pending_prune,
              ReevaluateLeafBoundsMap&& reevaluate_leaf_bounds,
              tree::TreeSystem* tree_sys) {
  std::vector<tree::TreeInstanceHandle> all_pending;

  for (auto& pend : pending_prune) {
    tree::TreeSystem::PruningInternodes pruning_inodes;
    pruning_inodes.dst_to_src = std::move(pend.dst_to_src);
    pruning_inodes.internodes = std::move(pend.dst_internodes);

    tree::TreeSystem::PruningData pruning_data;
    pruning_data.internodes = std::move(pruning_inodes);

    auto leaf_it = reevaluate_leaf_bounds.find(pend.handle);
    if (leaf_it != reevaluate_leaf_bounds.end()) {
      pruning_data.leaves.remove_bounds = std::move(leaf_it->second);
      reevaluate_leaf_bounds.erase(leaf_it);
    }

    tree::start_pruning(tree_sys, pend.handle, std::move(pruning_data));
    all_pending.push_back(pend.handle);
  }

  //  Remaining
  for (auto& [handle, element_ids] : reevaluate_leaf_bounds) {
    tree::TreeSystem::PruningData pruning_data;
    pruning_data.leaves.remove_bounds = std::move(element_ids);
    tree::start_pruning(tree_sys, handle, std::move(pruning_data));
    all_pending.push_back(handle);
  }

  return all_pending;
}

auto prepare_adjoining_curved_segment(const SegmentedStructure& structure,
                                      const arch::FaceConnectorIndices& curr_neg_x_connector) {
  struct Result {
    bool can_compute;
    bool flipped;
    Vec2f p00;
    Vec2f p01;
    Vec2f p10;
    Vec2f p11;
    Vec2f n01;
    Vec2f n11;
    uint32_t xi;
    OBB3f bounds;
  };

  Result result{};
  if (structure.pieces.size() < 2) {
    return result;
  }

  auto& prev = *(structure.pieces.end() - 2);
  auto& curr = *(structure.pieces.end() - 1);
  if (!prev.connector_positive_x) {
    return result;
  }

  auto& prev_pos = prev.connector_positive_x.value();
  if (prev_pos.xi_size(0) != curr_neg_x_connector.xi_size(0) ||
      prev_pos.xi_size(1) != curr_neg_x_connector.xi_size(1)) {
    return result;
  }

  float max_length{-1.0f};
  float lengths[2];
  Result candidates[2]{};
  for (uint32_t i = 0; i < 2; i++) {
    auto& candidate = candidates[i];
    auto ind_00 = prev.aggregate_geometry_offset + prev_pos.xi_ith(i, 0);
    auto ind_01 = prev.aggregate_geometry_offset + prev_pos.xi_ith(1 - i, 0);
    auto ind_10 = curr_neg_x_connector.xi_ith(i, 0);
    auto ind_11 = curr_neg_x_connector.xi_ith(1 - i, 0);
    candidate.p00 = keep_xz(structure.geometry.ith_aggregate_position(ind_00));
    candidate.p01 = keep_xz(structure.geometry.ith_aggregate_position(ind_01));
    candidate.p10 = keep_xz(structure.geometry.ith_growing_src_position(ind_10));
    candidate.p11 = keep_xz(structure.geometry.ith_growing_src_position(ind_11));
    candidate.n01 = keep_xz(structure.geometry.ith_aggregate_normal(ind_01));
    candidate.n11 = keep_xz(structure.geometry.ith_growing_src_normal(ind_11));
    candidate.xi = 1 - i;
    if (i == 1) {
      std::swap(candidate.p00, candidate.p10);
      std::swap(candidate.p01, candidate.p11);
      std::swap(candidate.n01, candidate.n11);
      candidate.flipped = true;
    }
    auto delta = (candidate.p11 - candidate.p01);
    lengths[i] = delta.length();
    max_length = std::max(max_length, lengths[i]);
  }
  if (max_length < 1e-3f) {
    return result;
  } else {
    result = lengths[0] > lengths[1] ? candidates[0] : candidates[1];
  }
  //  Use smaller of bounds
  result.bounds = prev.bounds.half_size.y < curr.bounds.half_size.y ? prev.bounds : curr.bounds;
  result.can_compute = true;
  return result;
}

[[maybe_unused]] std::vector<arch::WallHole> make_randomized_wall_holes(const OBB3f& bounds) {
  std::vector<arch::WallHole> result;
  auto& hole = result.emplace_back();
  hole.scale = Vec2f{lerp(urandf(), 0.05f, 0.75f), lerp(urandf(), 0.05f, 0.75f)};
  hole.curl = 0.2f;
//  hole.rot = urandf();
  hole.off = Vec2f{lerp(urandf(), -0.1f, 0.1f), lerp(urandf(), -0.1f, 0.1f)};
  if (bounds.half_size.x == bounds.half_size.y) {
    hole.rot = urandf() > 0.5f ? pif() * 0.25f : pif() * -0.25f;
    hole.off = {};
  }
  return result;
}

bool compute_extruded_structure_geometry(DebugArchComponent& component,
                                         const DebugArchComponent::UpdateInfo& info,
                                         SegmentedStructure& structure,
                                         const bounds::Accel* accel) {
  assert(!structure.pieces.empty());
  auto& piece = structure.pieces.back();
  const bool use_wall_piece_type =
    !component.structure_growth_params.randomize_piece_type || urand() > 0.25;

  auto internode_isect_res = tree_node_bounds_intersect(
    accel,
    piece.bounds,
    tree::get_bounds_tree_element_tag(&info.tree_system),
    tree::get_bounds_leaf_element_tag(&info.tree_system));
  if (internode_isect_res.any_hit) {
    if (!can_prune_all_candidates(&info.tree_system, internode_isect_res)) {
      return false;
    }
  }

  assert(structure.pending_finish_prune.empty());
  ComputeWallHolesAroundTreeNodesResult wall_hole_res{};
  if (use_wall_piece_type) {
    ComputeWallHolesAroundTreeNodesParams hole_params{};
    initialize_params(&hole_params, piece.bounds, component, info);
    wall_hole_res = compute_wall_holes_around_tree_nodes(internode_isect_res, hole_params);
    if (wall_hole_res.holes.empty()) {
      //  No acceptable holes found.
      if (piece.bounds.half_size.x == piece.bounds.half_size.y && urandf() < 0.5f) {
        wall_hole_res.holes = component.wall_holes;
      } else {
        wall_hole_res.holes = make_randomized_wall_holes(piece.bounds);
      }
    }

    auto pend = start_pruning(
      std::move(wall_hole_res.pending_prune),
      std::move(wall_hole_res.reevaluate_leaf_bounds),
      &info.tree_system);

    structure.pending_finish_prune.insert(
      structure.pending_finish_prune.end(),
      pend.begin(), pend.end());
  }

  auto alloc = make_geometry_allocators(global_data.geom_allocs);
  arch::clear_geometry_allocators(&alloc);

  uint32_t np_added{};
  uint32_t ni_added{};
  Optional<arch::FaceConnectorIndices> connector_positive_x;
  Optional<arch::FaceConnectorIndices> connector_negative_x;

  if (use_wall_piece_type) {
    auto hole_res = make_debug_wall(wall_hole_res.holes, 1.0f);
    auto seg_res = make_debug_straight_flat_segments();
    arch::FaceConnectorIndices wall_pos_x{};
    arch::FaceConnectorIndices wall_neg_x{};
    compute_wall_segment_geometry(
      piece.bounds, hole_res, seg_res, alloc, &wall_pos_x, &wall_neg_x, 0, &np_added, &ni_added);
    connector_positive_x = wall_pos_x;
    connector_negative_x = wall_neg_x;
  } else {
    compute_arch_wall(
      &component.grid_cache, piece.bounds, alloc, 0, &np_added, &ni_added);
  }

  reserve_growing(&structure.geometry, np_added, ni_added);
  copy_from_alloc_to_growing_src(&structure.geometry, alloc, np_added, 0, 0);

  if (connector_negative_x) {
    auto prep_res = prepare_adjoining_curved_segment(structure, connector_negative_x.value());
    if (prep_res.can_compute) {
      arch::FaceConnectorIndices curve_positive_x{};
      arch::FaceConnectorIndices curve_negative_x{};

      uint32_t adj_np_added{};
      uint32_t adj_ni_added{};
      arch::clear_geometry_allocators(&alloc);
      add_adjoining_curved_segment(
        prep_res.p00, prep_res.p01,
        prep_res.p10, prep_res.p11,
        prep_res.n01, prep_res.n11,
        np_added, alloc, prep_res.bounds,
        &curve_positive_x, &curve_negative_x,
        &adj_np_added, &adj_ni_added);
      reserve_growing(&structure.geometry, np_added + adj_np_added, ni_added + adj_ni_added);
      copy_from_alloc_to_growing_src(&structure.geometry, alloc, adj_np_added, ni_added, np_added);
      np_added += adj_np_added;
      ni_added += adj_ni_added;

      if (prep_res.flipped) {
        std::swap(curve_positive_x, curve_negative_x);
      }

      assert(!piece.curved_connector_negative_x && !piece.curved_connector_positive_x);
      piece.curved_connector_positive_x = curve_positive_x;
      piece.curved_connector_negative_x = curve_negative_x;
      piece.curved_connector_xi = prep_res.xi;
    }
  }

  assert(!piece.connector_negative_x && !piece.connector_positive_x);
  piece.connector_positive_x = connector_positive_x;
  piece.connector_negative_x = connector_negative_x;
  piece.aggregate_geometry_offset = structure.geometry.num_aggregate_vertices();
  piece.num_vertices = np_added;
  piece.num_triangles = ni_added / 3;

  prepare_growable_geometry(
    structure.geometry.growing_geometry_src.data(),
    structure.geometry.growing_geometry_dst.data(),
    structure.geometry.growing_triangles_src.data(),
    structure.geometry.num_growing_triangles_src);

  initialize_triangle_growth(&structure);

  reserve_arch_geometry(
    info.arch_renderer, info.arch_renderer_context,
    structure.growing_renderer_geometry, ni_added, ni_added);

  info.arch_renderer.set_modified(structure.growing_renderer_geometry);

  return true;
}

void clear_growing_structure_drawables(const SegmentedStructure& structure,
                                       ArchRenderer& renderer,
                                       const ArchRenderer::AddResourceContext& renderer_context) {
  reserve_arch_geometry(
    renderer, renderer_context, structure.growing_renderer_geometry, 0, 0);
  renderer.set_active(structure.growing_drawable, false);
  renderer.set_active(structure.aggregate_drawable, false);
}

void initialize_fit_bounds_to_points_context(const DebugArchComponent& component,
                                             arch::FitBoundsToPointsContext* fit_context,
                                             const Vec3f& structure_origin,
                                             const Vec2f& fit_target) {
  arch::initialize_fit_bounds_to_points_context(
    fit_context,
    structure_origin,
    fit_target,
    to_try_encircle_point_params(component.structure_growth_params),
    1);
}

void reset_growing_structure(DebugArchComponent& component,
                             const DebugArchComponent::UpdateInfo& info,
                             SegmentedStructure* structure,
                             arch::FitBoundsToPointsContext* fit_context,
                             const Vec2f& fit_target) {
  const auto& ori = component.structure_growth_params.structure_ori;
  deactivate_accel_bounds(
    structure, &info.bounds_system, info.accel_instance_handle, info.radius_limiter);
  clear_projected_tree_nodes(structure, info.projected_nodes_system);
  reset_structure(structure, ori, component.structure_growth_params.max_piece_x_length);
  initialize_fit_bounds_to_points_context(component, fit_context, ori, fit_target);
  clear_growing_structure_drawables(*structure, info.arch_renderer, info.arch_renderer_context);
}

float growing_structure_increment(const DebugArchComponent& component) {
  auto& p = component.render_growth_params;
  float incr = p.growth_incr;
  if (p.grow_by_instrument) {
    if (!component.instrument_signal_value) {
      incr = 0.0f;
    } else {
      incr = component.instrument_signal_value.value() * p.instrument_scale;
    }
  }
  return incr;
}

bool tick_render_growing_structure(DebugArchComponent& component,
                                   SegmentedStructure* structure,
                                   ArchRenderer& renderer,
                                   const ArchRenderer::AddResourceContext& renderer_context) {
  uint32_t curr_num_tris = structure->geometry.num_growing_triangles_dst;
  uint32_t num_active_inds = tick_triangle_growth(
    &structure->triangle_growth_context,
    structure->geometry.growing_triangles_dst.data(),
    uint32_t(structure->geometry.growing_geometry_dst.size()),
    growing_structure_increment(component));

  bool finished_growing{};
  if (num_active_inds == 0) {
    structure->geometry.num_growing_triangles_dst = curr_num_tris;
    structure->growth_state = StructureGrowthState::Idle;
    finished_growing = true;

    append_grown_geometry(&structure->geometry);
    update_arch_geometry(
      renderer,
      renderer_context,
      structure->aggregate_renderer_geometry,
      structure->geometry.aggregate_geometry,
      structure->geometry.aggregate_triangles);
    renderer.set_active(structure->growing_drawable, false);
    renderer.set_active(structure->aggregate_drawable, true);

  } else {
    structure->geometry.num_growing_triangles_dst = num_active_inds / 3;
    renderer.set_active(structure->growing_drawable, true);
  }

  renderer.set_modified(structure->growing_renderer_geometry);
  return finished_growing;
}

auto update_render_receding_structure(const DebugArchComponent& component,
                                      SegmentedStructure* structure,
                                      const DebugArchComponent::UpdateInfo& info) {
  struct Result {
    bool finished_receding;
  };

  Result result{};
  if (!structure->has_receding_piece) {
    if (structure->next_receding_piece_index < 0) {
      result.finished_receding = true;
      return result;
    }

    uint32_t num_truncated_verts{};
    uint32_t num_truncated_indices{};
    for (int i = 0; i < structure->next_receding_piece_index; i++) {
      auto& piece = structure->pieces[i];
      num_truncated_verts += piece.num_vertices;
      num_truncated_indices += piece.num_triangles * 3;
    }

    structure->has_receding_piece = true;
    auto& receding_piece = structure->pieces[structure->next_receding_piece_index--];
    const uint32_t num_growing_verts = receding_piece.num_vertices;
    const uint32_t num_growing_inds = receding_piece.num_triangles * 3;

    reserve_growing(&structure->geometry, num_growing_verts, num_growing_inds);
    copy_from_aggregate_to_growing_src(
      &structure->geometry,
      num_growing_verts, num_growing_inds,
      num_truncated_verts, num_truncated_indices,
      num_truncated_verts);
    copy_from_growing_src_to_growing_dst(&structure->geometry, num_growing_inds/3);

    initialize_triangle_recede(&structure->geometry, &structure->triangle_recede_context);

    if (num_truncated_verts > 0) {
      update_arch_geometry(
        info.arch_renderer,
        info.arch_renderer_context,
        structure->aggregate_renderer_geometry,
        structure->geometry.aggregate_geometry,
        structure->geometry.aggregate_triangles,
        num_truncated_verts, num_truncated_indices);
    } else {
      info.arch_renderer.set_active(structure->aggregate_drawable, false);
    }

    reserve_arch_geometry(
      info.arch_renderer, info.arch_renderer_context,
      structure->growing_renderer_geometry, num_growing_inds, num_growing_inds);
  }

  arch::RenderTriangleRecedeParams recede_params{};
  recede_params.incr = component.render_growth_params.growth_incr;
  recede_params.incr_randomness_range = 0.4f;
  recede_params.num_target_sets = 128;
  if (!arch::tick_triangle_recede(structure->triangle_recede_context, recede_params)) {
    structure->has_receding_piece = false;
  }

  info.arch_renderer.set_active(structure->growing_drawable, true);
  info.arch_renderer.set_modified(structure->growing_renderer_geometry);

  return result;
}

[[maybe_unused]]
void maybe_remap_latest_structure_piece_geometry_indices(SegmentedStructure* structure) {
  if (structure->pieces.size() < 2) {
    return;
  }

  auto& curr_piece = *(structure->pieces.end() - 1);
  auto& prev_piece = *(structure->pieces.end() - 2);

  uint32_t prev_offset{};
  const uint32_t num_src_indices = prev_piece.num_triangles * 3;
  for (uint32_t i = 0; i < uint32_t(structure->pieces.size())-2; i++) {
    auto& piece = structure->pieces[i];
    prev_offset += piece.num_triangles * 3;
  }

  auto* tris = structure->geometry.aggregate_triangles.data();
  RemapWithinTolParams remap_params{};
  remap_params.src = structure->geometry.aggregate_geometry.data();
  remap_params.src_stride = uint32_t(structure->geometry.aggregate_geometry_vertex_stride_bytes());
  remap_params.src_indices = tris + prev_offset;
  remap_params.num_src_indices = num_src_indices;
  remap_params.src_read_index_offset = 0;
  remap_params.src_write_index_offset = 0;
  remap_params.src_index_type = IntegralType::UnsignedShort;
  remap_params.src_n_offset = sizeof(Vec3f);

  remap_params.target = remap_params.src;
  remap_params.target_stride = remap_params.src_stride;
  remap_params.target_indices = tris + prev_offset + num_src_indices;
  remap_params.num_target_indices = curr_piece.num_triangles * 3;
  remap_params.target_read_index_offset = 0;
  remap_params.target_write_index_offset = 0;
  remap_params.target_index_type = remap_params.src_index_type;
  remap_params.target_n_offset = sizeof(Vec3f);
  remap_params.tol = 1e-3f;
  remap_params.n_cos_theta = 0.7f;

  auto num_remapped = remap_within_tol(
    structure->remapped_aggregate_geometry_indices_within_tol, remap_params);
  (void) num_remapped;
}

[[maybe_unused]]
void maybe_update_connected_structure_piece_geometry_indices(SegmentedStructure* structure) {
  if (structure->pieces.size() < 2) {
    return;
  }
  auto& curr = *(structure->pieces.end() - 1);
  auto& prev = *(structure->pieces.end() - 2);
  if (!curr.connector_negative_x || !prev.connector_positive_x) {
    return;
  }

  auto& curr_neg = curr.connector_negative_x.value();
  auto& prev_pos = prev.connector_positive_x.value();
  if (curr_neg.xi_size(0) != prev_pos.xi_size(0) ||
      curr_neg.xi_size(1) != prev_pos.xi_size(1)) {
    return;
  }

  const uint32_t curr_off = curr.aggregate_geometry_offset;
  const uint32_t prev_off = prev.aggregate_geometry_offset;

  auto& remap = structure->remapped_aggregate_geometry_indices_within_tol;
  auto& geom = structure->geometry.aggregate_geometry;

  RemapRangeWithTolParams remap_params{};
  remap_params.tol = 1e-3f;
  remap_params.i0 = &prev_pos;
  remap_params.i0_offset = prev_off;
  remap_params.i1 = &curr_neg;
  remap_params.i1_offset = curr_off;
  remap_params.remap = &remap;
  remap_params.data0 = geom.data();
  remap_params.data1 = geom.data();
  remap_params.stride = uint32_t(structure->geometry.aggregate_geometry_vertex_stride_bytes());
  remap_params.p_off = 0;
  remap_range_within_tol(0, remap_params);
  remap_range_within_tol(1, remap_params);
}

void maybe_connect_non_adjacent_structure_pieces(SegmentedStructure* structure) {
  if (structure->pieces.size() < 2) {
    return;
  }

  auto& curr = *(structure->pieces.end() - 1);
  auto& prev = *(structure->pieces.end() - 2);
  if (!curr.connector_negative_x || !prev.connector_positive_x) {
    return;
  }

  arch::FaceConnectorIndices curr_neg = curr.connector_negative_x.value();
  arch::FaceConnectorIndices prev_pos = prev.connector_positive_x.value();
  if (curr_neg.xi_size(0) != curr_neg.xi_size(1) ||
      prev_pos.xi_size(0) != prev_pos.xi_size(1)) {
    return;
  }

  curr_neg.add_offset(curr.aggregate_geometry_offset);
  prev_pos.add_offset(prev.aggregate_geometry_offset);

  auto& geom = structure->geometry.aggregate_geometry;
  auto& tris = structure->geometry.aggregate_triangles;
  const Vec3f* verts = geom.data();
  const auto vert_stride = uint32_t(structure->geometry.aggregate_geometry_vertex_stride_bytes());

  auto* connections = &structure->non_adjacent_connections;
  auto edge_indices = tri::build_edge_to_index_map(
    tris.data(), structure->geometry.num_aggregate_triangles());

  std::vector<uint32_t> posi(compute_num_non_adjacent_edge_indices(prev_pos, 0));
  std::vector<uint32_t> negi(compute_num_non_adjacent_edge_indices(curr_neg, 0));
  const float tol = 1e-3f;
  for (uint32_t i = 0; i < 2; i++) {
    push_face_connector_edge_indices(prev_pos, i, posi.data());
    push_face_connector_edge_indices(curr_neg, i, negi.data());
    push_mutual_non_adjacent_connections_y(
      connections, negi, posi, edge_indices, verts, vert_stride, 0, tol);
  }

  if (curr.curved_connector_positive_x && curr.curved_connector_negative_x) {
    auto curved_pos = curr.curved_connector_positive_x.value();
    curved_pos.add_offset(curr.aggregate_geometry_offset);

    auto curved_neg = curr.curved_connector_negative_x.value();
    curved_neg.add_offset(curr.aggregate_geometry_offset);

    const uint32_t curved_xi = curr.curved_connector_xi;
    assert(curved_xi <= 1);

    std::vector<uint32_t> curved_posi(compute_num_non_adjacent_edge_indices(curved_pos, 0));
    std::vector<uint32_t> curved_negi(compute_num_non_adjacent_edge_indices(curved_neg, 0));

    push_face_connector_edge_indices(curved_pos, 0, curved_posi.data());
    push_face_connector_edge_indices(curved_neg, 0, curved_negi.data());
    push_face_connector_edge_indices(prev_pos, curved_xi, posi.data());
    push_face_connector_edge_indices(curr_neg, curved_xi, negi.data());

    //  connect prev pos -> curved neg
    push_mutual_non_adjacent_connections_y(
      connections, posi, curved_negi, edge_indices, verts, vert_stride, 0, tol);
    //  connect curved pos -> curr neg
    push_mutual_non_adjacent_connections_y(
      connections, curved_posi, negi, edge_indices, verts, vert_stride, 0, tol);
  }

  ray_project::build_non_adjacent_connections(connections);
}

Optional<uint32_t> pick_growing_structure_triangle(const StructureGeometry& geom, const Ray& ray) {
  size_t hit_tri{};
  float hit_t{};
  bool any_hit = ray_triangle_intersect(
    ray,
    geom.aggregate_geometry.data(),
    geom.aggregate_geometry_vertex_stride_bytes(),
    0,
    geom.aggregate_triangles.data(),
    geom.num_aggregate_triangles(),
    0,
    nullptr, &hit_tri, &hit_t);
  if (any_hit) {
    return Optional<uint32_t>(uint32_t(hit_tri));
  } else {
    return NullOpt{};
  }
}

Optional<uint32_t>
pick_debug_structure_triangle(const DebugArchComponent& component, const Ray& ray) {
  const auto& geom = component.store_wall_hole_result;
  size_t hit_tri{};
  float hit_t{};
  bool any_hit = ray_triangle_intersect(
    ray,
    geom.positions.data(),
    sizeof(Vec3f),
    0,
    cdt::unsafe_cast_to_uint32(geom.triangles.data()),
    geom.triangles.size(),
    0, nullptr, &hit_tri, &hit_t);
  if (any_hit) {
    return Optional<uint32_t>(uint32_t(hit_tri));
  } else {
    return NullOpt{};
  }
}

struct UpdateGrowingStructureResult {
  bool finished_growing;
};

void remove_expired(std::vector<tree::TreeInstanceHandle>& pend,
                    const tree::TreeSystem::DeletedInstances& just_deleted) {
  auto it = pend.begin();
  while (it != pend.end()) {
    if (just_deleted.count(*it)) {
      it = pend.erase(it);
    } else {
      ++it;
    }
  }
}

bool ensure_finished_pruning(std::vector<tree::TreeInstanceHandle>& pend,
                             const tree::TreeSystem* tree_sys) {
  auto it = pend.begin();
  while (it != pend.end()) {
    const auto read_inst = tree::read_tree(tree_sys, *it);
    if (read_inst.events.just_finished_pruning) {
      it = pend.erase(it);
    } else {
      ++it;
    }
  }
  return pend.empty();
}

bool all_finished_receding(const tree::ProjectedNodesSystem* system, const GrowingTreeNodes* nodes,
                           int num_nodes) {
  for (int i = 0; i < num_nodes; i++) {
    if (!tree::is_finished_receding(system, nodes[i].proj_instance_handle)) {
      return false;
    }
  }
  return true;
}

void maybe_update_receding_structure(const DebugArchComponent& component,
                                     SegmentedStructure& structure,
                                     const DebugArchComponent::UpdateInfo& info) {
  const float delay_to_recede = component.structure_growth_params.delay_to_recede_s;
  if (structure.need_start_receding &&
      is_idle(structure.growth_state) &&
      component.structure_growth_params.allow_recede &&
      float(structure.state_stopwatch.delta().count()) >= delay_to_recede) {

    for (auto& nodes : structure.growing_tree_nodes) {
      tree::set_need_start_receding(info.projected_nodes_system, nodes.proj_instance_handle);
    }

    structure.need_start_receding = false;
    structure.has_receding_piece = false;
    structure.next_receding_piece_index = int(structure.pieces.size()) - 1;
    structure.growth_state = StructureGrowthState::Receding;
    structure.growth_phase = StructureGrowthPhase::PendingProjectedNodesFinishedReceding;
  }

  if (structure.growth_state == StructureGrowthState::Receding) {
    switch (structure.growth_phase) {
      case StructureGrowthPhase::PendingProjectedNodesFinishedReceding: {
        auto& growing_nodes = structure.growing_tree_nodes;
        bool finished_receding = all_finished_receding(
          info.projected_nodes_system, growing_nodes.data(), int(growing_nodes.size()));
        if (finished_receding) {
          //  Destroy projected instances.
          for (auto& node : growing_nodes) {
            tree::destroy_instance(info.projected_nodes_system, node.proj_instance_handle);
          }
          growing_nodes.clear();
          structure.growth_phase = StructureGrowthPhase::StructureReceding;
        }
        break;
      }
      case StructureGrowthPhase::StructureReceding: {
        auto recede_res = update_render_receding_structure(component, &structure, info);
        if (recede_res.finished_receding) {
          structure.growth_state = StructureGrowthState::Idle;
        }
        break;
      }
      default: {
        assert(false);
      }
    }
  }
}

UpdateGrowingStructureResult
update_growing_structure(DebugArchComponent& component,
                         const DebugArchComponent::UpdateInfo& info,
                         SegmentedStructure& structure,
                         arch::FitBoundsToPointsContext& fit_context) {
  UpdateGrowingStructureResult result{};
  remove_expired(structure.pending_finish_prune, info.deleted_tree_instances);

  const Vec2f fit_target{
    info.centroid_of_tree_origins.x, info.centroid_of_tree_origins.z
  };

  if (component.need_reset_structure && is_idle(structure.growth_state)) {
    reset_growing_structure(component, info, &structure, &fit_context, fit_target);
    component.need_reset_structure = false;
    return result;
  }

  const bool need_compute_geom = component.need_compute_extruded_structure_geometry;
  if (component.need_extrude_structure && !structure.extrude_disabled &&
      is_idle(structure.growth_state) && !need_compute_geom) {
    if (auto* accel = request_accel_write(component, info)) {
      ExtrudeGrowingStructureParams extrude_params{};
      extrude_params.accel = accel;
      extrude_params.terrain_bounds_tag = info.terrain_bounds_element_tag;
      extrude_params.arch_bounds_tag = component.bounds_arch_element_tag;
      extrude_params.radius_limiter = info.radius_limiter;
      extrude_params.roots_radius_limiter_tag = info.roots_radius_limiter_tag;
      extrude_params.arch_radius_limiter_tag = component.arch_radius_limiter_element_tag;
      extrude_params.fit_target = fit_target;

      const bool did_extrude = extrude_growing_structure(
        component, structure, fit_context, extrude_params);
      if (did_extrude) {
        component.need_extrude_structure = false;
        component.need_compute_extruded_structure_geometry = true;
      }
      release_accel_write(component, info);
    }
  }

  if (need_compute_geom && is_idle(structure.growth_state)) {
    if (auto* accel = request_accel_write(component, info)) {
      bool did_compute = compute_extruded_structure_geometry(
        component, info, structure, accel);
      if (did_compute) {
        component.need_compute_extruded_structure_geometry = false;
      }
      release_accel_write(component, info);
    }
  }

  if (structure.growth_state == StructureGrowthState::Growing) {
    if (ensure_finished_pruning(structure.pending_finish_prune, &info.tree_system)) {
      result.finished_growing = tick_render_growing_structure(
        component,
        &structure,
        info.arch_renderer,
        info.arch_renderer_context);
      if (result.finished_growing) {
//        maybe_remap_latest_structure_piece_geometry_indices(&structure);
//        maybe_update_connected_structure_piece_geometry_indices(&structure);
        maybe_connect_non_adjacent_structure_pieces(&structure);

        if (piece_x_length(structure) >= structure.max_piece_x_length) {
          structure.need_start_receding = true;
          structure.extrude_disabled = true;
          structure.state_stopwatch.reset();
        }
      }
    }
  }

  maybe_update_receding_structure(component, structure, info);

  return result;
}

uint32_t default_select_projected_tree_nodes_ti(const uint32_t* tris, uint32_t num_tris,
                                                const Vec3f* ps, uint32_t num_ps) {
  assert(num_tris > 0);
  uint32_t tmp_tis[8];
  uint32_t num_tmp_tis = tree::find_largest_triangles_containing_lowest_y(
    tris, num_tris, ps, num_ps, tmp_tis, 8);
  assert(num_tmp_tis > 0);
  auto ti_ind = uint32_t(urandf() * float(num_tmp_tis));
  return tmp_tis[ti_ind];
}

ArchRenderer::GeometryHandle
create_dynamic_segmented_structure_geometry(ArchRenderer& renderer,
                                            const SegmentedStructure* structure) {
  return renderer.create_dynamic_geometry(
    [structure](const void** geom_data, size_t* geom_size,
                const void** inds_data, size_t* inds_size) {
      *geom_data = structure->geometry.growing_geometry_dst.data();
      *geom_size = growing_geometry_dst_size(&structure->geometry) * sizeof(Vec3f);
      *inds_data = structure->geometry.growing_triangles_dst.data();
      *inds_size = structure->geometry.num_growing_triangles_dst * 3 * sizeof(uint16_t);
    });
}

ArchRenderer::DrawableHandle
create_arch_drawable(ArchRenderer& renderer, ArchRenderer::GeometryHandle geom,
                     const Vec3f& color) {
  ArchRenderer::DrawableParams draw_params{};
  draw_params.color = color;
  return renderer.create_drawable(geom, draw_params);
}

OBB3f make_obb_from_angles(const Vec3f& p, const Vec3f& s, const Vec3f& a) {
  auto mx = make_x_rotation(a.x);
  auto my = make_y_rotation(a.y);
  auto mz = make_z_rotation(a.z);
#if 1
  auto rot = mz * my * mx;
#else
  auto rot = mx * mz * my;
#endif
  OBB3f result;
  result.position = p;
  result.half_size = s * 0.5f;
  result.i = to_vec3(rot[0]);
  result.j = to_vec3(rot[1]);
  result.k = to_vec3(rot[2]);
  return result;
}

OBB3f make_obb_from_angles(const transform::TransformInstance* tform, const Vec3f& a) {
  return make_obb_from_angles(
    tform->get_current().translation, tform->get_current().scale, a);
}

arch::WallHole projected_aabb_to_wall_hole(const Bounds2f& proj_aabb, const Vec2f& world_sz,
                                           float curl, float size_scale, float rot = 0.0f) {
  arch::WallHole result{};
  //  @NOTE: `size_scale` is just a hack to get around the fact that windows curl inwards, so the
  //  inner dimensions of the opening are smaller than the specified `scale`.
  auto sz = proj_aabb.size() / world_sz * size_scale;
  auto center = proj_aabb.center() / world_sz;
  result.scale = sz;
  result.off = center;
  result.curl = curl;
  result.rot = rot;
  return result;
}

void update_wall_collision_geometry(ArchRenderer::GeometryHandle geom_handle,
                                    const OBB3f& isect_wall_obb,
                                    const arch::GeometryAllocators& alloc,
                                    const std::vector<arch::WallHole>& holes,
                                    const DebugArchComponent::UpdateInfo& info) {
  uint32_t np_added{};
  uint32_t ni_added{};
  arch::FaceConnectorIndices pos_x{};
  arch::FaceConnectorIndices neg_x{};
  compute_wall_segment_geometry(isect_wall_obb, holes, alloc, &pos_x, &neg_x, &np_added, &ni_added);

  std::vector<Vec3f> dst_data(np_added * 2);
  std::vector<uint16_t> dst_inds(ni_added);
  copy_interleaved(alloc.ps->begin, alloc.ns->begin, dst_data.data(), np_added);
  copy_uint32_to_uint16(alloc.tris->begin, dst_inds.data(), ni_added);

  update_arch_geometry(
    info.arch_renderer,
    info.arch_renderer_context,
    geom_handle, dst_data, dst_inds);
}

bool accept_wall_hole(const arch::WallHole& hole) {
  for (int i = 0; i < 2; i++) {
    assert(hole.scale[i] > 0.0f);
    float mn = hole.off[i] - hole.scale[i] * 0.5f;
    float mx = hole.off[i] + hole.scale[i] * 0.5f;
    if (mn < -0.5f || mx > 0.5f) {
      return false;
    }
  }
  return true;
}

void accept_none(const tree::AcceptCollisionComponentBoundsParams& accept_params) {
  *accept_params.num_accepted = 0;
}

void default_accept_wall_holes(const tree::AcceptCollisionComponentBoundsParams& accept_params,
                               const std::function<arch::WallHole(const Bounds2f&)>& make_hole,
                               int max_num_holes, arch::WallHole* dst_holes) {
  struct WallHoleInfo {
    arch::WallHole hole;
    int isle_id;
  };

  DynamicArray<WallHoleInfo, 4> info;
  for (int i = 0; i < accept_params.num_components; i++) {
    int isle_id = accept_params.unique_component_ids[i];
    auto hole = make_hole(accept_params.projected_component_bounds[isle_id]);
    if (accept_wall_hole(hole)) {
      info.push_back({hole, isle_id});
    }
  }

  auto area = [](const arch::WallHole& hole) {
    return hole.scale.x * hole.scale.y;
  };
  std::sort(info.begin(), info.end(), [&](const WallHoleInfo& a, const WallHoleInfo& b) {
    return area(a.hole) > area(b.hole);
  });

  *accept_params.num_accepted = std::min(max_num_holes, int(info.size()));
  for (int i = 0; i < *accept_params.num_accepted; i++) {
    accept_params.accept_component_ids[i] = info[i].isle_id;
    dst_holes[i] = info[i].hole;
  }
}

tree::TreeNodeCollisionWithObjectResult
compute_collision_with_wall(const TreeNodeCollisionWithWallParams& params) {
  const auto& collide_through_params = *params.collide_through_hole_params;

  tree::TreeNodeCollisionWithObjectParams collision_params{};
  collision_params.object_bounds = params.wall_bounds;
  collision_params.src_internodes = params.src_internodes;
  collision_params.num_src_internodes = params.num_src_internodes;
  collision_params.min_colliding_node_diameter = collide_through_params.min_collide_node_diam;
  collision_params.project_forward_dim = collide_through_params.forward_dim;
  collision_params.projected_aabb_scale = collide_through_params.projected_aabb_scale;
  collision_params.prune_initially_rejected = collide_through_params.prune_initially_rejected;

  const auto world_sz = exclude(
    params.wall_bounds.half_size, collide_through_params.forward_dim) * 2.0f;

  const auto make_hole = [&collide_through_params, &world_sz](const Bounds2f& b) {
    return projected_aabb_to_wall_hole(b, world_sz, collide_through_params.hole_curl, 1.0f);
  };

  if (collide_through_params.reject_all_holes) {
    collision_params.accept_collision_component_bounds = accept_none;
  } else {
    collision_params.accept_collision_component_bounds =
      [&](const tree::AcceptCollisionComponentBoundsParams& accept_params) {
        default_accept_wall_holes(
          accept_params, make_hole, params.max_num_accepted_holes, params.accepted_holes);
      };
  }
  return tree::compute_collision_with_object(params.collision_context, collision_params);
}

bool update_debug_tree_node_collision_new_method(DebugArchComponent& component,
                                                 const DebugArchComponent::UpdateInfo& info) {
  if (!component.src_tree_collider) {
    return false;
  }

  auto& collision_ctx = global_data.debug_collision_context;
  auto& collide_through_params = component.collide_through_hole_params;
  const auto* src_inodes = &component.src_tree_collider.value().internodes;
  auto& dst_inodes = component.pruned_tree_collider_internodes;
  auto& dst_to_src = component.pruned_tree_collider_dst_to_src;
  auto* obb_isect_wall_tform = component.obb_isect_wall_tform;
  auto isect_wall_obb = make_obb_from_angles(
    obb_isect_wall_tform, collide_through_params.wall_angles);
  component.isect_wall_obb = isect_wall_obb;

  constexpr int max_num_accept = 4;
  arch::WallHole accepted_holes[max_num_accept];

  TreeNodeCollisionWithWallParams collide_with_wall_params{};
  collide_with_wall_params.collision_context = &collision_ctx;
  collide_with_wall_params.collide_through_hole_params = &collide_through_params;
  collide_with_wall_params.wall_bounds = isect_wall_obb;
  collide_with_wall_params.src_internodes = src_inodes->data();
  collide_with_wall_params.num_src_internodes = int(src_inodes->size());
  collide_with_wall_params.accepted_holes = accepted_holes;
  collide_with_wall_params.max_num_accepted_holes = max_num_accept;
  auto collision_res = compute_collision_with_wall(collide_with_wall_params);

  bool did_compute = false;
  if ((collide_through_params.continuous_compute ||
       collide_through_params.compute_wall) &&
      collision_res.num_dst_internodes > 0) {
    dst_inodes.resize(collision_res.num_dst_internodes);
    dst_to_src.resize(collision_res.num_dst_internodes);
    std::copy(
      collision_res.dst_internodes,
      collision_res.dst_internodes + collision_res.num_dst_internodes,
      dst_inodes.data());
    std::copy(
      collision_res.dst_to_src,
      collision_res.dst_to_src + collision_res.num_dst_internodes,
      dst_to_src.data());

    component.pruning_src_internodes = *src_inodes;
    tree::initialize_axis_pruning(
      &component.pruned_axis_death_context,
      component.pruning_src_internodes,
      std::unordered_set<int>{dst_to_src.begin(), dst_to_src.end()});
    component.render_pruning = true;

    int num_accepted = collision_res.num_accepted_bounds_components;
    std::vector<arch::WallHole> holes(num_accepted);
    std::copy(accepted_holes, accepted_holes + num_accepted, holes.data());

    update_wall_collision_geometry(
      component.collide_through_hole_geometry,
      isect_wall_obb,
      make_geometry_allocators(global_data.geom_allocs),
      holes,
      info);
    info.arch_renderer.set_active(component.collide_through_hole_drawable, true);

    collide_through_params.compute_wall = false;
    did_compute = true;
  }

  if (component.params.draw_wall_bounds) {
    for (int i = 0; i < collision_res.num_collided_bounds; i++) {
      vk::debug::draw_obb3(collision_res.collided_bounds[i], Vec3f{1.0f, 0.0f, 0.0f});
    }
  }

//  vk::debug::draw_obb3(isect_wall_obb, Vec3f{0.0f, 1.0f, 0.0f});
  return did_compute;
}

void update_debug_collision_through_hole(DebugArchComponent& component,
                                         const DebugArchComponent::UpdateInfo& info) {
  const auto exclude_bounds = [](const Bounds3f& a, int dim) -> Bounds2f {
    return Bounds2f{exclude(a.min, dim), exclude(a.max, dim)};
  };

  auto& isect_wall_obb = component.isect_wall_obb;
  auto& isect_collider_obb = component.isect_collider_obb;
  auto& params = component.collide_through_hole_params;
  auto* obb_isect_wall_tform = component.obb_isect_wall_tform;
  auto* obb_isect_collider_tform = component.obb_isect_collider_tform;

  isect_wall_obb = make_obb_from_angles(
    obb_isect_wall_tform->get_current().translation,
    obb_isect_wall_tform->get_current().scale,
    params.wall_angles);
  isect_collider_obb = make_obb_from_angles(
    obb_isect_collider_tform->get_current().translation,
    obb_isect_collider_tform->get_current().scale,
    params.collider_angles);

  auto proj_res = obb_intersect_to_projected_aabb(
    isect_wall_obb, isect_collider_obb, params.forward_dim, true);

  if (proj_res.accept && params.compute_wall) {
    std::vector<arch::WallHole> holes;
    auto world_sz = exclude(isect_wall_obb.half_size, params.forward_dim) * 2.0f;
    holes.emplace_back() = projected_aabb_to_wall_hole(
      exclude_bounds(proj_res.aabb, params.forward_dim), world_sz, 0.2f, 2.0f);

    update_wall_collision_geometry(
      component.collide_through_hole_geometry,
      isect_wall_obb,
      make_geometry_allocators(global_data.geom_allocs),
      holes,
      info);
    info.arch_renderer.set_active(component.collide_through_hole_drawable, true);
    params.compute_wall = false;
  }

  if (proj_res.found_aabb) {
    Vec3f aabb_verts[8];
    Vec3f color = proj_res.accept ? Vec3f{0.0f, 1.0f, 0.0f} : Vec3f{1.0f, 0.0f, 0.0f};

    gather_vertices(proj_res.aabb, aabb_verts);
    for (auto& v : aabb_verts) {
      v = orient(isect_wall_obb, v) + isect_wall_obb.position;
    }

    info.pb_renderer.set_instances(
      info.pb_renderer_context,
      component.collide_through_hole_point_drawable,
      aabb_verts, 8, 0);
    info.pb_renderer.set_point_color(component.collide_through_hole_point_drawable, color);
  }

//  vk::debug::draw_obb3(isect_wall_obb, Vec3f{0.0f, 1.0f, 0.0f});
  vk::debug::draw_obb3(isect_collider_obb, Vec3f{0.0f, 0.0f, 1.0f});
}

void draw_line_fit_points(const Vec2f* points, uint32_t num_points, float height) {
  for (uint32_t i = 0; i < num_points; i++) {
    auto color = i == 0 ? Vec3f{0.0f, 0.0f, 1.0f} : Vec3f{1.0f, 1.0f, 0.0f};
    auto& p2 = points[i];
    Vec3f p3{p2.x, height, p2.y};
    vk::debug::draw_cube(p3, Vec3f{0.1f}, color);
    if (i + 1 < num_points) {
      auto& p2_next = points[i+1];
      Vec3f p3_next{p2_next.x, height, p2_next.y};
      vk::debug::draw_line(p3, p3_next, Vec3f{1.0f, 0.0f, 0.0f});
    }
  }
}

struct DebugNodeProjectResult {
  std::vector<Vec3f> extracted_normals;
  std::vector<Vec3f> true_normals;
  tree::Internodes internodes;
  std::vector<ProjectRayResultEntry> project_ray_results;
};

void update_debug_projected_nodes_drawables(ProceduralFlowerStemRenderer::DrawableHandle stem_drawable,
                                            bool draw_stem_drawable,
                                            const tree::Internodes& inodes,
                                            const std::vector<Vec3f>& extracted_normals,
                                            const DebugArchComponent::UpdateInfo& info) {
  info.stem_renderer.update_drawable(
    info.stem_renderer_context,
    stem_drawable,
    inodes,
    Vec3f{0.47f, 0.26f, 0.02f});
  info.stem_renderer.set_active(stem_drawable, draw_stem_drawable);
  (void) extracted_normals;
}

void update_debug_projected_nodes_drawables(const DebugProjectedNodes& nodes,
                                            const DebugArchComponent& component,
                                            const DebugArchComponent::UpdateInfo& info) {
  if (nodes.stem_drawable) {
    update_debug_projected_nodes_drawables(
      nodes.stem_drawable.value(),
      component.params.draw_stem_drawable,
      nodes.internodes,
      nodes.extracted_normals,
      info);
  }
}

DebugNodeProjectResult debug_project_internodes_onto_mesh(const DebugArchComponent& component,
                                                          const tree::Internodes& src_internodes,
                                                          double ray_length, double ray_theta) {
  auto& params = component.params;
  auto& store_wall_hole_result = component.store_wall_hole_result;

  uint32_t ti = params.debug_ray_ti;
  if (params.use_minimum_y_ti) {
    ti = tree::find_triangle_containing_min_y_point(
      cdt::unsafe_cast_to_uint32(store_wall_hole_result.triangles.data()),
      uint32_t(store_wall_hole_result.triangles.size()),
      store_wall_hole_result.positions.data(),
      uint32_t(store_wall_hole_result.positions.size()));
  }

  if (ti >= store_wall_hole_result.triangles.size()) {
    return {};
  }

  auto proj_tris = store_wall_hole_result.triangles;  //  @NOTE: copy
  auto& proj_ps = store_wall_hole_result.positions;
  uint32_t* proj_tri_u32 = cdt::unsafe_cast_to_uint32(proj_tris.data());
  tri::require_ccw(proj_tri_u32, uint32_t(proj_tris.size()), proj_ps.data());
  auto edge_indices = tri::build_edge_to_index_map(proj_tri_u32, uint32_t(proj_tris.size()));
  auto* non_adjacent_connections = &component.debug_non_adjacent_connections;

  tree::Internodes alt_internodes;
  const tree::Internodes* eval_internodes = &src_internodes;
  if (component.params.project_medial_axis_only && !src_internodes.empty()) {
    auto medial = tree::collect_medial_indices(
      src_internodes.data(), int(src_internodes.size()), 0);
    for (int mi = 0; mi < int(medial.size()); mi++) {
      auto node = src_internodes[medial[mi]];
      node.id = tree::TreeInternodeID::create();
      node.lateral_child = -1;
      node.medial_child = mi == int(medial.size()-1) ? -1 : mi + 1;
      node.parent = mi == 0 ? -1 : mi - 1;
      alt_internodes.push_back(node);
    }
    tree::validate_internode_relationships(alt_internodes);
    eval_internodes = &alt_internodes;
  }

  auto proj_res = tree::project_internodes_onto_mesh(
    proj_tri_u32,
    uint32_t(proj_tris.size()),
    proj_ps.data(),
    ti,
    edge_uv_to_world_point(proj_tri_u32, ti, proj_ps.data()),
    *eval_internodes,
    ray_theta + compute_initial_ray_direction(proj_tri_u32, ti, proj_ps.data()),
    ray_length,
    &edge_indices,
    non_adjacent_connections);

  const auto spawn_p = make_default_projected_node_spawn_params(params.node_diameter_power);
  auto pp_params = to_post_process_params(params);
  auto post_process_res = tree::post_process_projected_internodes(
    proj_res.internodes,
    spawn_p,
    store_wall_hole_result.normals.data(),
    proj_res.project_ray_results.data(),
    uint32_t(proj_res.project_ray_results.size()),
    pp_params);

  DebugNodeProjectResult result;
  result.true_normals = std::move(post_process_res.true_mesh_normals);
  result.extracted_normals = std::move(post_process_res.processed_mesh_normals);
  result.internodes = std::move(post_process_res.internodes);
  result.project_ray_results = std::move(proj_res.project_ray_results);
  return result;
}

uint32_t ith_piece_cumulative_triangle_offset(const SegmentedStructure& structure, int ith) {
  assert(ith >= 0 && ith < int(structure.pieces.size()));
  uint32_t off{};
  for (int i = 0; i < ith; i++) {
    off += structure.pieces[i].num_triangles;
  }
  return off;
}

void project_internodes_onto_structure(DebugArchComponent& component,
                                       SegmentedStructure& structure,
                                       const tree::Internodes& src_internodes,
                                       const DebugArchComponent::UpdateInfo& info) {
  const auto& params = component.params;
  const auto& pieces = structure.pieces;
#if 0
  uint32_t proj_ti_offset{};
  const auto& tris = structure.geometry.growing_triangles_src;
  const uint32_t num_tris = structure.geometry.num_growing_triangles_src;
  const uint32_t num_ps = structure.geometry.num_growing_vertices_src;

  std::vector<Vec3f> tmp_ps(num_ps);
  std::vector<Vec3f> tmp_ns(num_ps);
  copy_deinterleaved(
    structure.geometry.growing_geometry_src.data(), tmp_ps.data(), tmp_ns.data(), num_ps);
#else
  std::vector<uint32_t> tris(structure.geometry.aggregate_triangles.size());
  const uint32_t num_tris = uint32_t(tris.size()) / 3;
  const uint32_t num_ps = structure.geometry.num_aggregate_vertices();
  std::copy(
    structure.geometry.aggregate_triangles.begin(),
    structure.geometry.aggregate_triangles.end(),
    tris.begin());
  std::vector<Vec3f> tmp_ps(num_ps);
  std::vector<Vec3f> tmp_ns(num_ps);
  copy_deinterleaved(
    structure.geometry.aggregate_geometry.data(), tmp_ps.data(), tmp_ns.data(), num_ps);
  apply_remapping(
    tris.data(), uint32_t(tris.size()),
    structure.remapped_aggregate_geometry_indices_within_tol);

  uint32_t proj_ti_offset{};
  if (pieces.size() > 2) {
    proj_ti_offset = ith_piece_cumulative_triangle_offset(structure, int(pieces.size()-2));
  }
#endif
  assert(proj_ti_offset < num_tris);

  tree::CreateProjectedTreeInstanceParams proj_inst_params{};
  proj_inst_params.diameter_power = params.node_diameter_power;
  proj_inst_params.ornament_growth_incr = 0.025f;
  proj_inst_params.axis_growth_incr = params.axis_growth_incr;

  auto& growing = structure.growing_tree_nodes.emplace_back();
  growing.proj_instance_handle = tree::create_instance(
    info.projected_nodes_system, proj_inst_params);

  float len_scale{1.0f};
  if (!pieces.empty()) {
    auto& piece = pieces.back();
    len_scale = piece.bounds.half_size.y / 8.0f;
  }

  auto edge_indices = tri::build_edge_to_index_map(tris.data(), num_tris);
  tree::ProjectNodesOntoMeshParams proj_params{};
  proj_params.tris = tris.data();
  proj_params.num_tris = num_tris;
  proj_params.edge_indices = &edge_indices;
  proj_params.non_adjacent_connections = &structure.non_adjacent_connections;
  proj_params.ps = tmp_ps.data();
  proj_params.ns = tmp_ns.data();
  proj_params.ti = proj_ti_offset + default_select_projected_tree_nodes_ti(
    tris.data() + proj_ti_offset * 3,
    num_tris - proj_ti_offset, tmp_ps.data(), num_ps);

  if (component.picked_growing_structure_triangle) {
    uint32_t picked_ti = component.picked_growing_structure_triangle.value();
    if (picked_ti < num_tris) {
      proj_params.ti = picked_ti;
      component.picked_growing_structure_triangle = NullOpt{};
    };
  }

  proj_params.initial_ray_theta_offset = params.debug_ray1_theta;
  if (params.randomize_ray1_direction) {
    proj_params.initial_ray_theta_offset += urand_11() * pi() * params.debug_ray1_theta_rand_scale;
  }
  proj_params.ray_length =
    (params.debug_ray1_len + urand() * params.debug_ray1_len_rand_scale) * len_scale;

  tree::project_nodes_onto_mesh(
    info.projected_nodes_system, growing.proj_instance_handle, src_internodes, proj_params);
}

float projected_internode_growth_increment(const DebugArchComponent& component) {
  float incr = component.params.axis_growth_incr;
  if (component.params.grow_internodes_by_instrument) {
    if (!component.instrument_signal_value) {
      incr = 0.0f;
    } else {
      float scale = component.params.internode_growth_signal_scale;
      incr = component.instrument_signal_value.value() * scale;
    }
  }
  return incr;
}

[[maybe_unused]] tree::Internodes
make_fractal_by_z_rotation(tree::Internodes src, float theta,
                           const Vec3f& root_position, const Vec3f& root_direction,
                           float root_length, float length_scale) {
  auto z_rot = make_z_rotation(theta);

  auto dst = src;
  for (auto& node : dst) {
    auto node_pos = node.position;
    auto& nd = node.direction;
    nd = to_vec3(z_rot * Vec4f{nd, 0.0f});

    node_pos -= dst[0].position;
    node_pos = to_vec3(z_rot * Vec4f{node_pos, 1.0f});
    node_pos += dst[0].position;
    node.position = node_pos;
    node.render_position = node_pos;
  }

  auto rd = normalize(root_direction);
  auto new_root = tree::make_internode(-1, root_position, rd, root_length, 0);
  auto tip_off = rd * root_length;

  tree::set_render_length_scale(dst, 0, length_scale);
  for (auto& node : dst) {
    node.length *= length_scale;
    node.length_scale = 1.0f;
    node.position = node.render_position;
    node.id = tree::TreeInternodeID::create();
    node.translate(tip_off);
    node.offset_valid_node_indices(1 + int(src.size()));
  }

  for (auto& node : src) {
    node.translate(tip_off);
    node.offset_valid_node_indices(1);  //  +1 for new root
  }

  if (!src.empty()) {
    new_root.medial_child = 1;
    new_root.lateral_child = 1 + int(src.size());
    assert(src[0].parent == -1 && dst[0].parent == -1);
    src[0].parent = 0;
    dst[0].parent = 0;
  }

  tree::Internodes result(src.size() * 2 + 1);
  result[0] = new_root;
  std::copy(src.begin(), src.end(), result.begin() + 1);
  std::copy(dst.begin(), dst.end(), result.begin() + 1 + int(src.size()));
#ifdef GROVE_DEBUG
  tree::validate_internode_relationships(result);
#endif
  return result;
}

void initialize_debug_arch_recede(DebugArchComponent& component, SegmentedStructure* structure,
                                  const DebugArchComponent::UpdateInfo& info) {
  auto alloc = make_geometry_allocators(global_data.geom_allocs);
  arch::clear_geometry_allocators(&alloc);

  arch::FaceConnectorIndices pos_x{};
  arch::FaceConnectorIndices neg_x{};
  uint32_t np_added{};
  uint32_t ni_added{};
  compute_wall_segment_geometry(
    component.isect_wall_obb, component.wall_holes, alloc, &pos_x, &neg_x, &np_added, &ni_added);

  reserve_growing(&structure->geometry, np_added, ni_added);
  copy_from_alloc_to_growing_src(&structure->geometry, alloc, np_added, 0, 0);
  copy_from_growing_src_to_growing_dst(&structure->geometry, ni_added/3);

  reserve_arch_geometry(
    info.arch_renderer, info.arch_renderer_context,
    structure->growing_renderer_geometry, ni_added, ni_added);

  assert(is_idle(structure->growth_state));
  initialize_triangle_recede(&structure->geometry, &structure->triangle_recede_context);
  structure->growth_state = StructureGrowthState::Receding;

  info.arch_renderer.set_modified(structure->growing_renderer_geometry);
  info.arch_renderer.set_active(structure->growing_drawable, true);
}

void update_debug_arch_recede(DebugArchComponent& component, SegmentedStructure* structure,
                              const DebugArchComponent::UpdateInfo& info) {
  if (structure->growth_state != StructureGrowthState::Receding) {
    return;
  }

  arch::RenderTriangleRecedeParams recede_params{};
  recede_params.incr = component.render_growth_params.growth_incr;
  recede_params.incr_randomness_range = 0.4f;
  recede_params.num_target_sets = 128;

  if (!arch::tick_triangle_recede(structure->triangle_recede_context, recede_params)) {
    structure->growth_state = StructureGrowthState::Idle;
  }

  info.arch_renderer.set_modified(structure->growing_renderer_geometry);
}

} //  anon

DebugArchComponent::InitResult DebugArchComponent::initialize(const InitInfo& info) {
  InitResult result{};

  initialize_geometry_component_allocators(global_data.geom_allocs, &global_data.heap_data);

  arch_geometry = info.arch_renderer.create_static_geometry();
  arch_drawable = info.arch_renderer.create_drawable(arch_geometry.value(), {});
  info.arch_renderer.set_active(arch_drawable.value(), false);
  {
    auto& structure = global_data.debug_segmented_structure;
    //  growing
    structure.growing_renderer_geometry = create_dynamic_segmented_structure_geometry(
      info.arch_renderer, &structure);
    structure.growing_drawable = create_arch_drawable(
      info.arch_renderer, structure.growing_renderer_geometry, Vec3f{1.0f});
    //  aggregate
    structure.aggregate_renderer_geometry = info.arch_renderer.create_static_geometry();
    structure.aggregate_drawable = create_arch_drawable(
      info.arch_renderer, structure.aggregate_renderer_geometry, Vec3f{1.0f});
  }
  {
    auto& structure = global_data.debug_growing_segmented_structure;
    structure.growing_renderer_geometry = create_dynamic_segmented_structure_geometry(
      info.arch_renderer, &structure);
    structure.growing_drawable = create_arch_drawable(
      info.arch_renderer, structure.growing_renderer_geometry, Vec3f{1.0f});
  }

  debug_normals_drawable = info.pb_renderer.create_drawable(
    vk::PointBufferRenderer::DrawableType::Lines, {});
  params.debug_wall_theta = pif() / 4.0f;
  params.debug_wall_bounds = arch::make_obb_xz(
    params.debug_wall_offset, params.debug_wall_theta, params.debug_wall_scale);
  params.debug_wall_bounds2 = params.debug_wall_bounds;
  make_default_holes(wall_holes);

  for (int i = 0; i < 1; i++) {
    auto& nodes = debug_projected_nodes.emplace_back();
    nodes.ray_theta_offset = pi() * 0.25 * double(i);
  }

  {
    auto tree_p = std::string{GROVE_ASSET_DIR} + "/architecture/dump/nodes6.dat";
    auto tree_p1 = std::string{GROVE_ASSET_DIR} + "/architecture/dump/nodes5.dat";

    if (auto tree = tree::deserialize_file(tree_p1.c_str())) {
      src_tree_internodes1 = std::move(tree.value().internodes);
    }

    if (auto tree = tree::deserialize_file(tree_p.c_str())) {
      ProceduralFlowerStemRenderer::DrawableParams draw_params{};
      draw_params.wind_influence_enabled = false;
      draw_params.allow_lateral_branch = false;
      src_tree_internodes = std::move(tree.value().internodes);
#if 0
      src_tree_internodes = make_fractal_by_z_rotation(
        src_tree_internodes,
        0.25f * pif(),
        src_tree_internodes[0].position,
        src_tree_internodes[0].direction,
        src_tree_internodes[0].length,
        0.5f);
#endif

      for (auto& nodes : debug_projected_nodes) {
        nodes.stem_drawable = info.stem_renderer.create_drawable(
          info.stem_renderer_context,
          src_tree_internodes,
          draw_params);
      }
    }
  }

  {
    Vec3f collider_scale{1.0f, 1.0f, 4.0f};
    Vec3f wall_scale{16.0f, 16.0f, 2.0f};

    obb_isect_wall_tform = info.transform_system->create(
      TRS<float>::make_translation_scale(Vec3f{16.0f, 8.0f, 16.0f}, wall_scale));
    obb_isect_collider_tform = info.transform_system->create(
      TRS<float>::make_translation_scale(Vec3f{16.0f, 8.0f, 16.0f}, collider_scale));
    // result.add_transform_editors.push_back(obb_isect_wall_tform);
//    result.add_transform_editors.push_back(obb_isect_collider_tform);

    vk::PointBufferRenderer::DrawableParams point_params{};
    point_params.point_size = 6.0f;
    collide_through_hole_point_drawable = info.pb_renderer.create_drawable(
      vk::PointBufferRenderer::DrawableType::Points,
      point_params);
    info.pb_renderer.reserve_instances(
      info.pb_renderer_context,
      collide_through_hole_point_drawable,
      32);
    info.pb_renderer.add_active_drawable(collide_through_hole_point_drawable);

    ArchRenderer::DrawableParams arch_params;
    arch_params.color = Vec3f{1.0f};
    collide_through_hole_geometry = info.arch_renderer.create_static_geometry();
    collide_through_hole_drawable = info.arch_renderer.create_drawable(
      collide_through_hole_geometry, arch_params);
  }

  {
    auto tree_p = std::string{GROVE_ASSET_DIR} + "/serialized_trees/t3.dat";
    auto deser_res = tree::deserialize_file(tree_p.c_str());
    if (deser_res) {
      src_tree_collider = std::move(deser_res.value());
      auto& tree = src_tree_collider.value();
      tree.translate(-tree.origin());
      tree.translate(Vec3f{32.0f, 8.0f, 32.0f});
      tree::copy_diameter_to_lateral_q(src_tree_collider.value().internodes);
    }
  }

  set_structure_growth_params_preset1(structure_growth_params);
  need_update_drawable = true;
  need_reset_structure = true;
  need_toggle_debug_nodes_visible = true;
  return result;
}

void DebugArchComponent::update(const UpdateInfo& info) {
  if (toggle_arch_visibility && arch_drawable) {
    info.arch_renderer.toggle_active(arch_drawable.value());
    toggle_arch_visibility = false;
  }
  if (toggle_normal_visibility && debug_normals_drawable) {
    info.pb_renderer.toggle_active_drawable(debug_normals_drawable.value());
    toggle_normal_visibility = false;
  }
  if (structure_growth_params.auto_extrude) {
    need_extrude_structure = true;
  }

  if (need_pick_growing_structure_triangle && info.left_clicked) {
    picked_growing_structure_triangle = pick_growing_structure_triangle(
      global_data.debug_segmented_structure.geometry, info.mouse_ray);
    need_pick_growing_structure_triangle = false;
  }
  if (need_pick_debug_structure_triangle && info.left_clicked) {
    if (auto ti = pick_debug_structure_triangle(*this, info.mouse_ray)) {
      params.debug_ray_ti = ti.value();
      need_update_projected_ray = true;
    }
    need_pick_debug_structure_triangle = false;
  }

  {
    const float growth_incr = projected_internode_growth_increment(*this);
    for (auto& growing : global_data.debug_segmented_structure.growing_tree_nodes) {
      tree::set_axis_growth_increment(
        info.projected_nodes_system, growing.proj_instance_handle, growth_incr);
    }
  }

  auto update_growth_res = update_growing_structure(
    *this, info,
    global_data.debug_segmented_structure,
    global_data.debug_growing_structure_context);

  if (need_project_nodes_onto_structure ||
      (update_growth_res.finished_growing && structure_growth_params.auto_extrude)) {
    auto& structure = global_data.debug_segmented_structure;
#if 1
    auto& proj_inodes = src_tree_internodes;
#else //  use different source internodes for alternating pieces
    const int nodes_index = int(structure.pieces.size()) % 2;
    auto& proj_inodes = nodes_index == 0 ? src_tree_internodes : src_tree_internodes1;
#endif
    project_internodes_onto_structure(*this, structure, proj_inodes, info);
    need_project_nodes_onto_structure = false;
  }

#if 0
  const Vec3f ray_proj_scale{32.0f};
  const Vec3f ray_proj_offset{0.0f, 32.0f, 0.0f};
#else
  const Vec3f ray_proj_scale{1.0f};
  const Vec3f ray_proj_offset{};
#endif

  if (need_update_drawable) {
    auto geom_res = compute_wall_geometry(*this);
    debug_non_adjacent_connections = std::move(geom_res.non_adjacent_connections);
    debug_cubes.clear();
    debug_cubes.insert(debug_cubes.end(), geom_res.debug_cubes.begin(), geom_res.debug_cubes.end());

    {
      auto& x0_y0 = geom_res.ps[geom_res.debug_wall_positive_x.x0_y0];
      auto& x0_y1 = geom_res.ps[geom_res.debug_wall_positive_x.x0_y1];
      auto& x1_y0 = geom_res.ps[geom_res.debug_wall_positive_x.x1_y0];
      auto& x1_y1 = geom_res.ps[geom_res.debug_wall_positive_x.x1_y1];
      debug_cubes.push_back({x0_y0, Vec3f{0.25f}, Vec3f{1.0f}});
      debug_cubes.push_back({x0_y1, Vec3f{0.25f}, Vec3f{0.0f}});
      debug_cubes.push_back({x1_y0, Vec3f{0.25f}, Vec3f{1.0f, 0.0f, 1.0f}});
      debug_cubes.push_back({x1_y1, Vec3f{0.25f}, Vec3f{0.0f, 1.0f, 0.0f}});
    }
    {
      auto& x0_y0 = geom_res.ps[geom_res.debug_wall_negative_x.x0_y0];
      auto& x0_y1 = geom_res.ps[geom_res.debug_wall_negative_x.x0_y1];
      auto& x1_y0 = geom_res.ps[geom_res.debug_wall_negative_x.x1_y0];
      auto& x1_y1 = geom_res.ps[geom_res.debug_wall_negative_x.x1_y1];
      debug_cubes.push_back({x0_y0, Vec3f{0.25f}, Vec3f{1.0f}});
      debug_cubes.push_back({x0_y1, Vec3f{0.25f}, Vec3f{0.0f}});
      debug_cubes.push_back({x1_y0, Vec3f{0.25f}, Vec3f{1.0f, 0.0f, 1.0f}});
      debug_cubes.push_back({x1_y1, Vec3f{0.25f}, Vec3f{0.0f, 1.0f, 0.0f}});
    }

    auto& ps = geom_res.ps;
    auto& ns = geom_res.ns;
    auto& tris = geom_res.inds;
    auto geom_data = interleave(ps, ns);
    auto geom_inds = std::vector<uint16_t>(tris.size());
    arch::truncate_to_uint16(tris.data(), geom_inds.data(), geom_inds.size());
    bool geom_success = update_arch_geometry(
      info.arch_renderer,
      info.arch_renderer_context,
      arch_geometry.value(),
      geom_data,
      geom_inds);
    if (geom_success) {
      info.arch_renderer.get_params(arch_drawable.value())->color = Vec3f{1.0f};
//      info.arch_renderer.require_active_drawable(arch_drawable.value());
    }
#if 1
    update_debug_normals(
      info.pb_renderer,
      info.pb_renderer_context,
      debug_normals_drawable.value(),
      ps,
      ns);
#endif
    params.num_triangles = uint32_t(geom_inds.size()) / 3u;
    params.num_vertices = uint32_t(ps.size());
    need_update_drawable = false;
    need_update_projected_ray = true;
  }

  if (need_update_projected_ray && params.debug_ray_ti < store_wall_hole_result.triangles.size()) {
    for (auto& nodes : debug_projected_nodes) {
      auto proj_res = debug_project_internodes_onto_mesh(
        *this,
        src_tree_internodes,
        params.debug_ray1_len,
        params.debug_ray1_theta + nodes.ray_theta_offset);
      nodes.internodes = std::move(proj_res.internodes);
      nodes.extracted_normals = std::move(proj_res.extracted_normals);
      nodes.true_normals = std::move(proj_res.true_normals);
      nodes.project_ray_results = std::move(proj_res.project_ray_results);
      update_debug_projected_nodes_drawables(nodes, *this, info);
    }
    need_update_projected_ray = false;
  }

  if (new_leaves_scale) {
    params.leaves_scale = new_leaves_scale.value();
    new_leaves_scale = NullOpt{};
  }

  if (need_trigger_axis_growth) {
    for (auto& nodes : debug_projected_nodes) {
      if (nodes.growth_state == DebugTreeNodeGrowthState::Idle && !nodes.internodes.empty()) {
        tree::copy_diameter_to_lateral_q(nodes.internodes);
        for (auto& node : nodes.internodes) {
          node.diameter = 0.0f;
          node.length_scale = 0.0f;
        }
        tree::initialize_depth_first_axis_render_growth_context(
          &nodes.axis_growth_context, nodes.internodes, 0);
//        tree::set_render_length_scale(nodes.internodes, 0, 0.0f);
        nodes.growth_state = DebugTreeNodeGrowthState::Growing;
        nodes.growing_axis_root = 0;
      }
    }
    need_trigger_axis_growth = false;
  }

  if (need_toggle_debug_nodes_visible) {
    need_toggle_debug_nodes_visible = false;
  }

  for (auto& nodes : debug_projected_nodes) {
    if (nodes.growth_state == DebugTreeNodeGrowthState::Growing) {
      bool new_axis{};
      bool still_growing = tree::update_render_growth_depth_first(
        nodes.internodes, nodes.axis_growth_context, params.axis_growth_incr, &new_axis);
      if (still_growing) {
        for (auto& inode : nodes.internodes) {
          inode.diameter = lerp(inode.length_scale, 0.0f, inode.lateral_q);
        }
      } else if (!still_growing) {
        nodes.growth_state = DebugTreeNodeGrowthState::Idle;
        nodes.growing_axis_root = NullOpt{};
      }
      if (new_axis) {
        assert(nodes.growing_axis_root);
        auto root_inds = tree::collect_medial_indices(
          nodes.internodes.data(), int(nodes.internodes.size()), nodes.growing_axis_root.value());
        nodes.growing_leaf_instance_indices.resize(root_inds.size());
        std::copy(root_inds.begin(), root_inds.end(), nodes.growing_leaf_instance_indices.begin());
        nodes.growth_state = DebugTreeNodeGrowthState::PendingNextAxis;
        nodes.growing_axis_root = nodes.axis_growth_context.depth_first_growing;
        nodes.growth_stopwatch.reset();
        nodes.growing_leaf_t = 0.0f;
      }
    } else if (nodes.growth_state == DebugTreeNodeGrowthState::PendingNextAxis) {
      nodes.growing_leaf_t += 0.01f * float(info.real_dt / (1.0 / 60.0));
      if (nodes.growing_leaf_t >= 1.0f) {
        nodes.growing_leaf_t = 1.0f;
        nodes.growth_state = DebugTreeNodeGrowthState::Growing;
      }
    }
  }

  if (collide_through_hole_params.with_tree_nodes) {
    if (update_debug_tree_node_collision_new_method(*this, info)) {
      //
    }

    if (params.draw_wall_bounds) {
      const auto& obb_scl = collide_through_hole_params.leaf_obb_scale;
      const auto& obb_off = collide_through_hole_params.leaf_obb_offset;
      for (const auto& inode : pruned_tree_collider_internodes) {
        if (inode.is_leaf()) {
          OBB3f node_obb = tree::internode_relative_obb(inode, obb_scl, obb_off);
          vk::debug::draw_obb3(node_obb, Vec3f{0.0f, 1.0f, 0.0f});
        }
      }
    }
  } else {
    update_debug_collision_through_hole(*this, info);
  }

  if (need_retrigger_arch_recede &&
      is_idle(global_data.debug_growing_segmented_structure.growth_state)) {
    initialize_debug_arch_recede(*this, &global_data.debug_growing_segmented_structure, info);
    need_retrigger_arch_recede = false;
  }
  update_debug_arch_recede(*this, &global_data.debug_growing_segmented_structure, info);

  if (params.draw_debug_cubes) {
    for (auto& cube : debug_cubes) {
      vk::debug::draw_cube(cube.p, cube.s, cube.color);
    }
  }

  if (true) {
    visualize_non_adjacent_connection(
      debug_non_adjacent_connections,
      uint32_t(params.ith_non_adjacent_tri),
      cdt::unsafe_cast_to_uint32(store_wall_hole_result.triangles.data()),
      store_wall_hole_result.positions.data());
  }

  if (params.draw_wall_bounds) {
    for (auto& wb : wall_bounds) {
      vk::debug::draw_obb3(wb, Vec3f{1.0f, 0.0f, 0.0f});
    }
    vk::debug::draw_obb3(params.debug_wall_bounds, Vec3f{0.0f, 1.0f, 0.0f});
    vk::debug::draw_obb3(params.debug_wall_bounds2, Vec3f{0.0f, 0.0f, 1.0f});
  }

  if (params.draw_project_ray_result) {
    debug::RenderProjectRayParams render_params{};
    render_params.offset = ray_proj_offset;
    render_params.scale = ray_proj_scale;
    render_params.ns = store_wall_hole_result.normals.data();
    render_params.offset_normal_length = 0.0f;

    for (auto& nodes : debug_projected_nodes) {
      debug::render_project_ray_results(
        nodes.project_ray_results.data(),
        uint32_t(nodes.project_ray_results.size()),
        cdt::unsafe_cast_to_uint32(store_wall_hole_result.triangles.data()),
        store_wall_hole_result.positions.data(),
        render_params);
    }

    vk::debug::draw_triangle_edges(
      cdt::unsafe_cast_to_uint32(store_wall_hole_result.triangles.data()),
      uint32_t(store_wall_hole_result.triangles.size()),
      store_wall_hole_result.positions.data(),
      Vec3f{1.0f},
      ray_proj_scale,
      ray_proj_offset);
  }

  instrument_signal_value = NullOpt{};

  if (params.draw_tree_node_bounds) {
    for (auto& nodes : debug_projected_nodes) {
      for (auto& node : nodes.internodes) {
        auto obb = tree::internode_obb(node);
        vk::debug::draw_obb3(obb, Vec3f{0.0f, 0.0f, 1.0f});
      }
    }
  }

  if (params.draw_extracted_tree_node_normals) {
    for (auto& nodes : debug_projected_nodes) {
      int ni{};
      for (auto& node : nodes.internodes) {
        auto p0 = node.render_position;
        auto p1 = p0 + nodes.extracted_normals[ni++] * 0.25f;
        auto dir = node.direction;
        auto p2 = p0 + dir * 0.1f;
        vk::debug::draw_line(p0, p1, Vec3f{1.0f, 0.0f, 0.0f});
        vk::debug::draw_line(p0, p2, Vec3f{0.0f, 1.0f, 0.0f});
      }
    }
  }
  if (params.draw_projected_grid) {
    for (auto& q : grid_quads) {
      for (int i = 0; i < q.size(); i++) {
        const int next = (i + 1) % q.size();
        auto& p0 = grid_terrain_projected_points[q.i[i]];
        auto& p1 = grid_terrain_projected_points[q.i[next]];
        vk::debug::draw_line(p0, p1, Vec3f{1.0f, 0.0f, 0.0f});
      }
    }
  }

#if 0
  {
    static Stopwatch stopwatch;
    auto amt = float(stopwatch.delta_update().count() * 0.5);
    for (auto& hole : wall_holes) {
      hole.rot += amt;
      while (hole.rot >= 2.0f * pif()) {
        hole.rot -= 2.0f * pif();
      }
    }
    need_update_drawable = true;
  }
#endif

  if (params.draw_wall_bounds) {
    Vec3f cent = Vec3f{info.centroid_of_tree_origins.x, 8.0f, info.centroid_of_tree_origins.z};
    vk::debug::draw_cube(cent, Vec3f{0.1f}, Vec3f{1.0f, 0.0f, 0.0f});

    draw_line_fit_points(
      global_data.debug_growing_structure_context.line_ps.data(),
      uint32_t(global_data.debug_growing_structure_context.line_ps.size()),
      8.0f);

    int i{};
    for (auto& b : debug_structure_growth_bounds) {
      auto color = i % 2 == 0 ? Vec3f{1.0f, 0.0f, 0.0f} : Vec3f{1.0f, 0.0f, 1.0f};
      vk::debug::draw_obb3(b, color);
      i++;
    }

    i = 0;
    for (auto& piece : global_data.debug_segmented_structure.pieces) {
      auto color = i % 2 == 0 ? Vec3f{1.0f, 0.0f, 0.0f} : Vec3f{1.0f, 0.0f, 1.0f};
      vk::debug::draw_obb3(piece.bounds, color);
      i++;
    }
  }
}

void DebugArchComponent::set_instrument_signal_value(float v) {
  instrument_signal_value = v;
}

void DebugArchComponent::set_instrument_connected() {
  structure_growth_params.auto_extrude = true;
}

int DebugArchComponent::gather_wall_bounds(OBB3f* dst, int max_num_dst) {
  int ct{};
  for (auto& piece : global_data.debug_segmented_structure.pieces) {
    if (ct < max_num_dst) {
      dst[ct++] = piece.bounds;
    }
  }
  return ct;
}

OBB3f DebugArchComponent::get_tentative_wall_bounds_at_position(const Vec3f& p) const {
  auto res = isect_wall_obb;
  res.position = p;
  return res;
}

void DebugArchComponent::on_gui_update(const ArchGUIUpdateResult& gui_res) {
  if (gui_res.new_theta) {
    params.debug_wall_theta = gui_res.new_theta.value();
    need_update_drawable = true;
  }
  if (gui_res.ith_non_adjacent_tri) {
    params.ith_non_adjacent_tri = gui_res.ith_non_adjacent_tri.value();
  }
  if (gui_res.new_aspect_ratio) {
    params.debug_wall_aspect_ratio = gui_res.new_aspect_ratio.value();
    need_update_drawable = true;
  }
  if (gui_res.need_project_nodes_onto_structure) {
    need_project_nodes_onto_structure = true;
  }
  if (gui_res.new_extruded_theta) {
    params.extruded_theta = gui_res.new_extruded_theta.value();
    need_update_drawable = true;
  }
  if (gui_res.new_scale) {
    params.debug_wall_scale = gui_res.new_scale.value();
    need_update_drawable = true;
  }
  if (gui_res.new_offset) {
    params.debug_wall_offset = gui_res.new_offset.value();
    need_update_drawable = true;
  }
  if (gui_res.toggle_normal_visibility) {
    toggle_normal_visibility = true;
  }
  if (gui_res.toggle_arch_visibility) {
    toggle_arch_visibility = true;
  }
  if (gui_res.toggle_debug_nodes_visibility) {
    need_toggle_debug_nodes_visible = true;
  }
  if (gui_res.remake_wall) {
    need_update_drawable = true;
  }
  if (!gui_res.new_holes.empty()) {
    wall_holes = gui_res.new_holes;
    need_update_drawable = true;
  }
  if (gui_res.draw_wall_bounds) {
    params.draw_wall_bounds = gui_res.draw_wall_bounds.value();
  }
  if (gui_res.draw_debug_cubes) {
    params.draw_debug_cubes = gui_res.draw_debug_cubes.value();
  }
  if (gui_res.draw_tree_node_bounds) {
    params.draw_tree_node_bounds = gui_res.draw_tree_node_bounds.value();
  }
  if (gui_res.draw_project_ray_result) {
    params.draw_project_ray_result = gui_res.draw_project_ray_result.value();
  }
  if (gui_res.draw_extracted_tree_node_normals) {
    params.draw_extracted_tree_node_normals = gui_res.draw_extracted_tree_node_normals.value();
  }
  if (gui_res.draw_stem_drawable) {
    params.draw_stem_drawable = gui_res.draw_stem_drawable.value();
    need_update_projected_ray = true;
  }
  if (gui_res.save_triangulation_file_path) {
    cdt::debug::write_triangulation3(
      gui_res.save_triangulation_file_path.value().c_str(),
      store_wall_hole_result.triangles.data(),
      uint32_t(store_wall_hole_result.triangles.size()),
      store_wall_hole_result.positions.data(),
      uint32_t(store_wall_hole_result.positions.size()));
  }
  if (gui_res.projected_ray1_theta) {
    params.debug_ray1_theta = gui_res.projected_ray1_theta.value();
    need_update_projected_ray = true;
  }
  if (gui_res.project_medial_axis_only) {
    params.project_medial_axis_only = gui_res.project_medial_axis_only.value();
    need_update_projected_ray = true;
  }
  if (gui_res.projected_ray1_length) {
    params.debug_ray1_len = gui_res.projected_ray1_length.value();
    need_update_projected_ray = true;
  }
  if (gui_res.randomize_projected_ray_theta) {
    params.randomize_ray1_direction = gui_res.randomize_projected_ray_theta.value();
  }
  if (gui_res.projected_ray_ti) {
    auto new_ti = gui_res.projected_ray_ti.value();
    if (new_ti < store_wall_hole_result.triangles.size()) {
      params.debug_ray_ti = new_ti;
      need_update_projected_ray = true;
    }
  }
  if (gui_res.prune_intersecting_tree_nodes) {
    params.prune_intersecting_tree_nodes = gui_res.prune_intersecting_tree_nodes.value();
    need_update_projected_ray = true;
  }
  if (gui_res.intersecting_tree_node_queue_size) {
    params.intersecting_tree_node_queue_size = gui_res.intersecting_tree_node_queue_size.value();
    need_update_projected_ray = true;
  }
  if (gui_res.reset_tree_node_diameter) {
    params.reset_tree_node_diameter = gui_res.reset_tree_node_diameter.value();
    need_update_projected_ray = true;
  }
  if (gui_res.smooth_tree_node_diameter) {
    params.smooth_tree_node_diameter = gui_res.smooth_tree_node_diameter.value();
    need_update_projected_ray = true;
  }
  if (gui_res.smooth_tree_node_normals) {
    params.smooth_tree_node_normals = gui_res.smooth_tree_node_normals.value();
    need_update_projected_ray = true;
  }
  if (gui_res.smooth_normals_adjacent_count) {
    params.smooth_normals_adjacent_count = gui_res.smooth_normals_adjacent_count.value();
    need_update_projected_ray = true;
  }
  if (gui_res.smooth_diameter_adjacent_count) {
    params.smooth_diameter_adjacent_count = gui_res.smooth_diameter_adjacent_count.value();
    need_update_projected_ray = true;
  }
  if (gui_res.constrain_child_node_diameter) {
    params.constrain_child_node_diameter = gui_res.constrain_child_node_diameter.value();
    need_update_projected_ray = true;
  }
  if (gui_res.constrain_internode_diameter) {
    params.constrain_internode_diameter = gui_res.constrain_internode_diameter.value();
    need_update_projected_ray = true;
  }
  if (gui_res.max_internode_diameter) {
    params.max_internode_diameter = gui_res.max_internode_diameter.value();
    need_update_projected_ray = true;
  }
  if (gui_res.offset_tree_nodes_by_radius) {
    params.offset_tree_nodes_by_radius = gui_res.offset_tree_nodes_by_radius.value();
    need_update_projected_ray = true;
  }
  if (gui_res.node_diameter_power) {
    params.node_diameter_power = gui_res.node_diameter_power.value();
    need_update_projected_ray = true;
  }
  if (gui_res.use_minimum_y_ti) {
    params.use_minimum_y_ti = gui_res.use_minimum_y_ti.value();
    need_update_projected_ray = true;
  }
  if (gui_res.leaves_scale) {
    new_leaves_scale = gui_res.leaves_scale.value();
  }
  if (gui_res.retrigger_axis_growth) {
    need_trigger_axis_growth = true;
  }
  if (gui_res.axis_growth_incr) {
    params.axis_growth_incr = gui_res.axis_growth_incr.value();
  }
  if (gui_res.set_preset1) {
    params.reset_tree_node_diameter = true;
    params.prune_intersecting_tree_nodes = true;
    params.constrain_child_node_diameter = true;
    params.smooth_tree_node_normals = true;
    params.smooth_tree_node_diameter = true;
    params.smooth_diameter_adjacent_count = 3;
    params.smooth_normals_adjacent_count = 11;
    params.offset_tree_nodes_by_radius = true;
    need_update_projected_ray = true;
  }
  if (gui_res.remake_grid) {
    need_update_drawable = true;
  }
  if (gui_res.grid_params) {
    auto& p = gui_res.grid_params.value();
    params.grid_fib_n = p.fib_n;
    params.grid_permit_quad_probability = p.permit_quad_probability;
    params.grid_relax_params.iters = p.relax_iters;
    params.grid_relax_params.neighbor_length_scale = p.neighbor_length_scale;
    params.grid_relax_params.quad_scale = p.quad_scale;
    params.grid_projected_terrain_offset = p.grid_projected_terrain_offset;
    params.grid_projected_terrain_scale = p.grid_projected_terrain_scale;
    params.draw_projected_grid = p.draw_grid;
    params.grid_update_enabled = p.update_enabled;
    params.apply_height_map_to_grid = p.apply_height_map;
    if (p.set_preset1) {
      params.grid_fib_n = 6;
      params.grid_projected_terrain_scale = Vec2f{300.0f};
      params.draw_projected_grid = true;
    }
    if (params.grid_update_enabled) {
      need_update_drawable = true;
    }
  }
  if (gui_res.internode_growth_signal_scale) {
    params.internode_growth_signal_scale = gui_res.internode_growth_signal_scale.value();
  }
  if (gui_res.grow_internodes_by_instrument) {
    params.grow_internodes_by_instrument = gui_res.grow_internodes_by_instrument.value();
  }
  if (gui_res.structure_growth_params) {
    auto& p = gui_res.structure_growth_params.value();
    structure_growth_params.num_pieces = p.num_pieces;
    structure_growth_params.encircle_point_params.attract_force_scale = p.attract_force_scale;
    structure_growth_params.encircle_point_params.propel_force_scale = p.propel_force_scale;
    structure_growth_params.encircle_point_params.dist_attract_until = p.dist_attract_until;
    structure_growth_params.encircle_point_params.dist_begin_propel = p.dist_begin_propel;
    structure_growth_params.encircle_point_params.dt = p.dt;
    structure_growth_params.piece_length = p.piece_length;
    structure_growth_params.structure_ori = p.structure_ori;
    structure_growth_params.use_variable_piece_length = p.use_variable_piece_length;
    structure_growth_params.target_length = p.target_length;
    structure_growth_params.use_isect_wall_obb = p.use_isect_wall_obb;
    structure_growth_params.auto_extrude = p.auto_extrude;
    structure_growth_params.randomize_wall_scale = p.randomize_wall_scale;
    structure_growth_params.randomize_piece_type = p.randomize_piece_type;
    structure_growth_params.restrict_structure_x_length = p.restrict_structure_x_length;
    structure_growth_params.auto_project_internodes = p.auto_project_internodes;
    structure_growth_params.delay_to_recede_s = p.delay_to_recede_s;
    structure_growth_params.allow_recede = p.allow_recede;

    if (p.set_preset1) {
      set_structure_growth_params_preset1(structure_growth_params);
    }
  }
  if (gui_res.collide_through_hole_params) {
    auto& p = gui_res.collide_through_hole_params.value();
    collide_through_hole_params.wall_angles = p.wall_angles;
    collide_through_hole_params.collider_angles = p.collider_angles;
    collide_through_hole_params.forward_dim = clamp(p.forward_dim, 0, 2);
    collide_through_hole_params.with_tree_nodes = p.with_tree_nodes;
    collide_through_hole_params.min_collide_node_diam = p.min_collide_node_diam;
    collide_through_hole_params.projected_aabb_scale = p.projected_aabb_scale;
    collide_through_hole_params.hole_curl = p.hole_curl;
    collide_through_hole_params.continuous_compute = p.continuous_compute;
    collide_through_hole_params.prune_initially_rejected = p.prune_initially_rejected;
    collide_through_hole_params.leaf_obb_scale = p.leaf_obb_scale;
    collide_through_hole_params.leaf_obb_offset = p.leaf_obb_offset;
    collide_through_hole_params.reject_all_holes = p.reject_all_holes;

    auto collider_curr = obb_isect_collider_tform->get_current();
    collider_curr.scale = p.collider_scale;
    obb_isect_collider_tform->set(collider_curr);

    auto wall_curr = obb_isect_wall_tform->get_current();
    wall_curr.scale = p.wall_scale;
    obb_isect_wall_tform->set(wall_curr);
  }
  if (gui_res.reset_growing_structure) {
    need_reset_structure = true;
  }
  if (gui_res.extrude_growing_structure) {
    need_extrude_structure = true;
  }
  if (gui_res.render_growth_params) {
    auto& p = gui_res.render_growth_params.value();
    if (p.retrigger_growth) {
      need_retrigger_arch_growth = true;
    }
    if (p.retrigger_recede) {
      need_retrigger_arch_recede = true;
    }
    render_growth_params.growth_incr = p.growth_incr;
    render_growth_params.instrument_scale = p.instrument_scale;
    render_growth_params.grow_by_instrument = p.grow_by_instrument;
  }
  if (gui_res.recompute_collide_through_hole_geometry) {
    collide_through_hole_params.compute_wall = true;
  }
  if (gui_res.pick_growing_structure_triangle) {
    need_pick_growing_structure_triangle = true;
  }
  if (gui_res.pick_debug_structure_triangle) {
    need_pick_debug_structure_triangle = true;
  }
}

GROVE_NAMESPACE_END
