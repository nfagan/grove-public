#include "project_internodes_on_structure.hpp"
#include "../procedural_tree/projected_nodes.hpp"
#include "structure_geometry.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace arch;

void copy_to_context(ProjectInternodesOnStructureContext& ctx,
                     const ProjectInternodesOnStructureParams& params) {
  ctx.initial_proj_ti = params.initial_proj_ti;
  ctx.ray_theta_offset = params.ray_theta_offset;
  ctx.ray_len = params.ray_len;
  ctx.diameter_power = params.diameter_power;

  ctx.internodes.resize(params.num_internodes);
  ctx.structure_pieces.resize(params.num_pieces);
  ctx.tris.resize(params.num_tris * 3);
  ctx.ps.resize(params.num_vertices);
  ctx.ns.resize(params.num_vertices);

  std::copy(params.internodes, params.internodes + params.num_internodes, ctx.internodes.data());
  std::copy(params.structure_pieces,
            params.structure_pieces + params.num_pieces, ctx.structure_pieces.data());
  std::copy(params.tris, params.tris + params.num_tris * 3, ctx.tris.data());

  if (params.normals_or_nullptr) {
    //  already de-interleaved
    std::copy(params.normals_or_nullptr,
              params.normals_or_nullptr + params.num_vertices, ctx.ns.data());
    std::copy(params.positions_or_aggregate_geometry,
              params.positions_or_aggregate_geometry + params.num_vertices, ctx.ps.data());
  } else {
    copy_structure_geometry_deinterleaved(
      params.positions_or_aggregate_geometry, ctx.ps.data(), ctx.ns.data(), params.num_vertices);
  }
}

const StructureGeometryPiece*
find_piece(const std::vector<StructureGeometryPiece>& pieces, StructureGeometryPieceHandle handle) {
  for (auto& piece : pieces) {
    if (piece.handle == handle) {
      return &piece;
    }
  }
  return nullptr;
}

void build_tri_edge_index_map(ProjectInternodesOnStructureContext& ctx) {
  ctx.edge_index_map = tri::build_edge_to_index_map(ctx.tris.data(), uint32_t(ctx.tris.size() / 3));
}

void build_non_adjacent_connections(ProjectInternodesOnStructureContext& ctx) {
  for (auto& piece : ctx.structure_pieces) {
    if (!piece.parent) {
      continue;
    }

    auto* parent_piece = find_piece(ctx.structure_pieces, piece.parent.value());
    if (!parent_piece) {
      continue;
    }

    (void) try_connect_non_adjacent_structure_pieces(
      ctx.ps, false, ctx.edge_index_map, *parent_piece, piece, &ctx.non_adjacent_connections);
  }
}

tree::ProjectNodesOntoMeshParams
make_projection_params(ProjectInternodesOnStructureContext& ctx) {
  tree::ProjectNodesOntoMeshParams proj_params{};
  proj_params.tris = ctx.tris.data();
  proj_params.num_tris = uint32_t(ctx.tris.size()) / 3;
  proj_params.edge_indices = &ctx.edge_index_map;
  proj_params.non_adjacent_connections = &ctx.non_adjacent_connections;
  proj_params.ps = ctx.ps.data();
  proj_params.ns = ctx.ns.data();
  proj_params.ti = ctx.initial_proj_ti;
  proj_params.ray_length = ctx.ray_len;
  proj_params.initial_ray_theta_offset = ctx.ray_theta_offset;
  return proj_params;
}

void do_async_project(ProjectInternodesOnStructureFuture& fut) {
  ProjectInternodesOnStructureContext& ctx = fut.context;

  build_tri_edge_index_map(ctx);
  build_non_adjacent_connections(ctx);

  auto proj_params = make_projection_params(ctx);
  auto proj_res = tree::default_project_nodes_onto_mesh(
    ctx.internodes, &proj_params, ctx.diameter_power);

  fut.result.project_ray_results = std::move(proj_res.project_ray_results);
  fut.result.post_process_res = std::move(proj_res.post_process_res);
}

} //  anon

std::unique_ptr<ProjectInternodesOnStructureFuture>
arch::project_internodes_onto_structure(const ProjectInternodesOnStructureParams& params) {
  assert(params.aggregate_geometry_stride_bytes == 2 * sizeof(Vec3f));
  assert(params.initial_proj_ti < params.num_tris);

  auto res = std::make_unique<ProjectInternodesOnStructureFuture>();
  res->ready.store(false);
  copy_to_context(res->context, params);

  auto* ptr = res.get();
  ptr->async_future = std::async(std::launch::async, [ptr]() {
    do_async_project(*ptr);
    ptr->ready.store(true);
  });

  return res;
}

GROVE_NAMESPACE_END
