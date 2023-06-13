#include "structure_geometry.hpp"
#include "../render/memory.hpp"
#include "ray_project_adjacency.hpp"
#include "grove/math/triangle_search.hpp"
#include "grove/math/intersect.hpp"
#include "grove/common/common.hpp"
#include "grove/visual/types.hpp"
#include "grove/common/memory.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace arch;

struct Config {
  static constexpr size_t vertex_stride_bytes = sizeof(Vec3f) * 2;
};

struct StructureGeometryContext {
  bool initialized{};
  LinearAllocator geom_allocs[4]{};
  std::unique_ptr<unsigned char[]> geom_heap_data;
  uint32_t next_id{1};
};

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

const Vec3f& ith_position(const StructureGeometry& geom, uint32_t i) {
  return geom.geometry[i * 2];
}

const Vec3f& ith_normal(const StructureGeometry& geom, uint32_t i) {
  return geom.geometry[i * 2 + 1];
}

Vec2f keep_xz(const Vec3f& v) {
  return Vec2f{v.x, v.z};
}

auto prepare_adjoining_curved_segment(const StructureGeometry& geom,
                                      const StructureGeometryPiece& prev_piece,
                                      uint32_t curr_geom_offset,
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
  };

  Result result{};
  if (!prev_piece.connector_positive_x) {
    return result;
  }

  auto& prev_pos = prev_piece.connector_positive_x.value();
  if (prev_pos.xi_size(0) != curr_neg_x_connector.xi_size(0) ||
      prev_pos.xi_size(1) != curr_neg_x_connector.xi_size(1)) {
    return result;
  }

  float max_length{-1.0f};
  float lengths[2];
  Result candidates[2]{};
  for (uint32_t i = 0; i < 2; i++) {
    auto& candidate = candidates[i];
    auto ind_00 = prev_piece.geometry_offset + prev_pos.xi_ith(i, 0);
    auto ind_01 = prev_piece.geometry_offset + prev_pos.xi_ith(1 - i, 0);
    auto ind_10 = curr_geom_offset + curr_neg_x_connector.xi_ith(i, 0);
    auto ind_11 = curr_geom_offset + curr_neg_x_connector.xi_ith(1 - i, 0);
    candidate.p00 = keep_xz(ith_position(geom, ind_00));
    candidate.p01 = keep_xz(ith_position(geom, ind_01));
    candidate.p10 = keep_xz(ith_position(geom, ind_10));
    candidate.p11 = keep_xz(ith_position(geom, ind_11));
    candidate.n01 = keep_xz(ith_normal(geom, ind_01));
    candidate.n11 = keep_xz(ith_normal(geom, ind_11));
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
  result.can_compute = true;
  return result;
}

void make_adjoining_curved_segment(const Vec2f& p00, const Vec2f& p01,
                                   const Vec2f& p10, const Vec2f& p11,
                                   const Vec2f& n01, const Vec2f& n11,
                                   uint32_t index_offset, const arch::GeometryAllocators& alloc,
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

arch::TriangulationResult make_straight_flat_segments() {
  arch::StraightFlatSegmentParams straight_flat_seg_params{};
  straight_flat_seg_params.grid_x_segments = 2;
  straight_flat_seg_params.grid_y_segments = 2;
  std::swap(straight_flat_seg_params.dim_perm[0], straight_flat_seg_params.dim_perm[2]);
  return arch::make_straight_flat_segment(straight_flat_seg_params);
}

arch::WallHoleResult make_wall(const arch::WallHole* holes, int num_holes) {
  arch::WallHoleParams hole_params{};
  hole_params.grid_y_segments = 4;
  hole_params.grid_x_segments = 4;
  hole_params.holes = holes;
  hole_params.num_holes = uint32_t(num_holes);
  hole_params.aspect_ratio = 1.0f;
  std::swap(hole_params.dim_perm[1], hole_params.dim_perm[2]);
  return arch::make_wall_hole(hole_params);
}

void push(StructureGeometry* geom, uint32_t np, uint32_t ni) {
  geom->geometry.resize(geom->geometry.size() + np * 2);
  geom->triangles.resize(geom->triangles.size() + ni);
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

void copy_from_alloc(StructureGeometry* geom, const arch::GeometryAllocators& alloc,
                     uint32_t np, uint32_t dst_index_off, uint32_t dst_vertex_off) {
  assert(geom->triangles.size() >= dst_index_off &&
         (geom->triangles.size() - dst_index_off) * sizeof(uint32_t) >= size(alloc.tris) &&
         geom->triangles.size() >= dst_vertex_off * 2 &&
         (geom->triangles.size() - dst_vertex_off * 2) >= 2 * np);
  //  copy indices
  memcpy(geom->triangles.data() + dst_index_off, alloc.tris->begin, size(alloc.tris));
  //  copy geometry
  copy_interleaved(alloc.ps->begin, alloc.ns->begin, geom->geometry.data() + dst_vertex_off * 2, np);
}

void add_index_offset(StructureGeometry* geom, uint32_t beg, uint32_t end, uint32_t off) {
  for (; beg < end; ++beg) {
    assert(beg < uint32_t(geom->triangles.size()));
    geom->triangles[beg] += off;
    assert(geom->triangles[beg] < geom->num_vertices());
  }
}

const StructureGeometryPiece* find_piece(const StructureGeometry& geom,
                                         StructureGeometryPieceHandle handle) {
  for (auto& piece : geom.pieces) {
    if (piece.handle == handle) {
      return &piece;
    }
  }
  return nullptr;
}

struct {
  StructureGeometryContext context;
} globals;

} //  anon

Optional<uint32_t> arch::StructureGeometry::ray_intersect(const Ray& ray) const {
  size_t hit_tri{};
  float hit_t{};
  bool any_hit = ray_triangle_intersect(
    ray, geometry.data(), vertex_stride_bytes(), 0,
    triangles.data(), num_triangles(), 0, nullptr, &hit_tri, &hit_t);
  if (any_hit) {
    return Optional<uint32_t>(uint32_t(hit_tri));
  } else {
    return NullOpt{};
  }
}

uint32_t arch::StructureGeometry::max_vertex_index_or_zero() const {
  if (triangles.empty()) {
    return 0;
  } else {
    return *std::max_element(triangles.begin(), triangles.end());
  }
}

size_t arch::StructureGeometry::vertex_stride_bytes() const {
  return Config::vertex_stride_bytes;
}

uint32_t arch::StructureGeometry::num_vertices() const {
  return uint32_t(geometry.size()) / 2;
}

uint32_t arch::StructureGeometry::num_triangles() const {
  return uint32_t(triangles.size()) / 3;
}

const StructureGeometryPiece*
arch::StructureGeometry::read_piece(StructureGeometryPieceHandle handle) const {
  for (auto& piece : pieces) {
    if (piece.handle == handle) {
      return &piece;
    }
  }
  return nullptr;
}

void arch::initialize_structure_geometry_context() {
  initialize_geometry_component_allocators(
    globals.context.geom_allocs, &globals.context.geom_heap_data);
  globals.context.initialized = true;
}

StructureGeometryPieceHandle arch::extrude_wall(
  StructureGeometry* structure, const OBB3f& bounds, const WallHole* holes, int num_holes,
  const Optional<StructureGeometryPieceHandle>& parent_piece) {
  //
  assert(globals.context.initialized);
  auto alloc = grove::make_geometry_allocators(globals.context.geom_allocs);
  arch::clear_geometry_allocators(&alloc);

  uint32_t np_added{};
  uint32_t ni_added{};

  arch::FaceConnectorIndices wall_pos_x{};
  arch::FaceConnectorIndices wall_neg_x{};

  auto hole_res = grove::make_wall(holes, num_holes);
  auto seg_res = make_straight_flat_segments();
  auto wall_p = arch::make_wall_params(
    bounds, 0, hole_res, seg_res, alloc, &np_added, &ni_added, &wall_pos_x, &wall_neg_x);
  arch::make_wall(wall_p);

  const uint32_t geom_off = structure->num_vertices();
  const uint32_t tri_off = structure->num_triangles();
  const uint32_t ind_off = tri_off * 3;

  push(structure, np_added, ni_added);
  copy_from_alloc(structure, alloc, np_added, ind_off, geom_off);
  add_index_offset(structure, ind_off, ind_off + ni_added, geom_off);

  Optional<arch::FaceConnectorIndices> curved_connector_positive_x;
  Optional<arch::FaceConnectorIndices> curved_connector_negative_x;
  uint32_t curved_connector_xi{};

  const StructureGeometryPiece* prev_piece{};
  if (parent_piece) {
    prev_piece = find_piece(*structure, parent_piece.value());
  }

  if (prev_piece) {
    auto prep_res = prepare_adjoining_curved_segment(*structure, *prev_piece, geom_off, wall_neg_x);
    if (prep_res.can_compute) {
      auto eval_bounds =
        prev_piece->bounds.half_size.y < bounds.half_size.y ? prev_piece->bounds : bounds;

      arch::FaceConnectorIndices curve_positive_x{};
      arch::FaceConnectorIndices curve_negative_x{};

      uint32_t adj_np_added{};
      uint32_t adj_ni_added{};
      arch::clear_geometry_allocators(&alloc);
      grove::make_adjoining_curved_segment(
        prep_res.p00, prep_res.p01,
        prep_res.p10, prep_res.p11,
        prep_res.n01, prep_res.n11,
        np_added, alloc, eval_bounds,
        &curve_positive_x, &curve_negative_x,
        &adj_np_added, &adj_ni_added);

      const auto adj_geom_off = geom_off + np_added;
      const auto adj_ind_off = ind_off + ni_added;

      push(structure, adj_np_added, adj_ni_added);
      copy_from_alloc(structure, alloc, adj_np_added, adj_ind_off, adj_geom_off);
      //  @NOTE: Use `geom_off` here rather than `adj_geom_off` because `np_added` is already
      //  incorporated in the call to make the adjoining curved segment.
      add_index_offset(structure, adj_ind_off, adj_ind_off + adj_ni_added, geom_off);

      np_added += adj_np_added;
      ni_added += adj_ni_added;

      if (prep_res.flipped) {
        std::swap(curve_positive_x, curve_negative_x);
      }

      curved_connector_positive_x = curve_positive_x;
      curved_connector_negative_x = curve_negative_x;
      curved_connector_xi = prep_res.xi;
    }
  }

  StructureGeometryPiece& next_piece = structure->pieces.emplace_back();
  next_piece = {};
  next_piece.handle = StructureGeometryPieceHandle{globals.context.next_id++};
  next_piece.parent = parent_piece;
  next_piece.bounds = bounds;
  next_piece.geometry_offset = geom_off;
  next_piece.triangle_offset = tri_off;
  next_piece.num_vertices = np_added;
  next_piece.num_triangles = ni_added / 3;
  next_piece.connector_positive_x = wall_pos_x;
  next_piece.connector_negative_x = wall_neg_x;
  next_piece.curved_connector_positive_x = curved_connector_positive_x;
  next_piece.curved_connector_negative_x = curved_connector_negative_x;
  next_piece.curved_connector_xi = curved_connector_xi;
  return next_piece.handle;
}

namespace {

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

} //  anon

bool arch::try_connect_non_adjacent_structure_pieces(
  const std::vector<Vec3f>& geom,
  bool geom_is_interleaved,
  const tri::EdgeToIndex<uint32_t>& edge_indices,
  const StructureGeometryPiece& prev, const StructureGeometryPiece& curr,
  ray_project::NonAdjacentConnections* connections) {
  //
  if (!curr.connector_negative_x || !prev.connector_positive_x) {
    return false;
  }

  arch::FaceConnectorIndices curr_neg = curr.connector_negative_x.value();
  arch::FaceConnectorIndices prev_pos = prev.connector_positive_x.value();
  if (curr_neg.xi_size(0) != curr_neg.xi_size(1) ||
      prev_pos.xi_size(0) != prev_pos.xi_size(1)) {
    return false;
  }

  curr_neg.add_offset(curr.geometry_offset);
  prev_pos.add_offset(prev.geometry_offset);

  const Vec3f* verts = geom.data();
  const auto vert_stride = geom_is_interleaved ?
    uint32_t(Config::vertex_stride_bytes) : uint32_t(sizeof(Vec3f));

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
    curved_pos.add_offset(curr.geometry_offset);

    auto curved_neg = curr.curved_connector_negative_x.value();
    curved_neg.add_offset(curr.geometry_offset);

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
  return true;
}

void arch::copy_structure_geometry_deinterleaved(const Vec3f* ps_ns, Vec3f* ps, Vec3f* ns, uint32_t np) {
  VertexBufferDescriptor src_desc;
  src_desc.add_attribute(AttributeDescriptor::float3(0));
  src_desc.add_attribute(AttributeDescriptor::float3(1));
  VertexBufferDescriptor dst_desc;
  dst_desc.add_attribute(AttributeDescriptor::float3(0));
  const int src_inds[2] = {0, 1};
  const int dst_inds[1] = {0};
  //  Copy positions
  copy_buffer(ps_ns, src_desc, src_inds, ps, dst_desc, dst_inds, 1, np);
  //  Copy normals
  copy_buffer(ps_ns, src_desc, src_inds+1, ns, dst_desc, dst_inds, 1, np);
}

void arch::copy_triangles_and_vertices_from_src_to_dst(GrowingStructureGeometry* geom, uint32_t num_tris) {
  const auto& tri_src = geom->src_tris;
  const auto& geom_src = geom->src_geometry;

  auto& tri_dst = geom->dst_tris;
  auto& geom_dst = geom->dst_geometry;
  const auto* geom_src_p = static_cast<const unsigned char*>((const void*) geom_src.data());
  auto* geom_dst_p = static_cast<unsigned char*>((void*) geom_dst.data());

  assert(tri_dst.size() == num_tris * 3);

  const size_t vert_stride = 2 * sizeof(Vec3f);
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
}

void arch::copy_triangles_and_vertices_from_aggregate_geometry_to_src_growing_geometry(
  const StructureGeometry* aggregate_geom, GrowingStructureGeometry* growing_geom,
  const StructureGeometryPiece* piece) {
  //
  const uint32_t np = piece->num_vertices;
  const uint32_t ni = piece->num_triangles * 3;
  const uint32_t ith_vert_off = piece->geometry_offset;
  const uint32_t ith_ind_off = piece->triangle_offset * 3;

  assert(growing_geom->src_geometry.size() == np * 2 && growing_geom->src_tris.size() == ni);
  assert((ith_vert_off + np) * 2 <= aggregate_geom->geometry.size() &&
         ith_ind_off + ni <= aggregate_geom->triangles.size());

  assert(growing_geom->src_geometry.size() == piece->num_vertices * 2);
  std::copy(
    aggregate_geom->geometry.begin() + ith_vert_off * 2,
    aggregate_geom->geometry.begin() + (ith_vert_off + np) * 2,
    growing_geom->src_geometry.begin());

  for (uint32_t i = 0; i < ni; i++) {
    auto src_ind = uint32_t(aggregate_geom->triangles[i + ith_ind_off]);
    assert(src_ind >= ith_vert_off);
    src_ind -= ith_vert_off;
    growing_geom->src_tris[i] = src_ind;
  }
}

void arch::resize_and_prepare(GrowingStructureGeometry* geom, uint32_t ni, uint32_t np, bool recede) {
  geom->src_geometry.resize(np * 2);
  geom->dst_geometry.resize(ni * 2);  //  @NOTE
  geom->src_tris.resize(ni);
  geom->dst_tris.resize(ni);

  assert((ni % 3) == 0);
  const uint32_t num_tris = ni / 3;
  geom->num_src_tris = num_tris;
  geom->num_dst_tris = recede ? num_tris : 0;
  geom->num_src_vertices = np;
  geom->num_dst_vertices = ni;
}

void arch::remove_last_piece(StructureGeometry* geom) {
  assert(!geom->pieces.empty());
  auto piece = geom->pieces.back();
  geom->pieces.pop_back();
  assert(piece.num_vertices <= geom->num_vertices() && piece.num_triangles <= geom->num_triangles());
  uint32_t new_np = geom->num_vertices() - piece.num_vertices;
  uint32_t new_ni = (geom->num_triangles() - piece.num_triangles) * 3;
  geom->geometry.resize(new_np * 2);
  geom->triangles.resize(new_ni);
}

GROVE_NAMESPACE_END
