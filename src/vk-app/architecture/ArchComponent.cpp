#include "ArchComponent.hpp"
#include "structure_geometry.hpp"
#include "structure_growth.hpp"
#include "segmented_structure_system.hpp"
#include "collision_with_internodes.hpp"
#include "collision_with_roots.hpp"
#include "project_internodes_on_structure.hpp"
#include "../render/ArchRenderer.hpp"
#include "../render/debug_draw.hpp"
#include "../procedural_tree/vine_ornamental_foliage.hpp"
#include "../procedural_tree/vine_system.hpp"
#include "../render/debug_draw.hpp"
#include "../render/render_particles_gpu.hpp"
#include "../procedural_tree/utility.hpp"
#include "grove/math/random.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/scope.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

using UpdateInfo = ArchComponentUpdateInfo;
using InitInfo = ArchComponentInitInfo;

struct Config {
  static constexpr int max_num_pieces_per_structure = 32;
};

enum class StructureState {
  Idle = 0,
  ComputingBounds,
  ComputingCollision,
  PendingFinishPruning,
  Receding
};

struct ArchComponentStructurePieceBoundsElements {
  Optional<bounds::ElementID> bounds_element;
  Optional<bounds::RadiusLimiterAggregateID> radius_limiter_aggregate_id;
  Optional<bounds::RadiusLimiterElementHandle> radius_limiter_element_handle;
};

struct PendingFinishPruning {
  bool any() const {
    return !trees.empty() || !roots.empty();
  }
  int num_trees() const {
    return int(trees.size());
  }
  int num_roots() const {
    return int(roots.size());
  }

  std::vector<tree::TreeInstanceHandle> trees;
  std::vector<tree::RootsInstanceHandle> roots;
};

struct ArchComponentStructure {
  arch::SegmentedStructureHandle structure_handle{};
  StructureState state{};
  OBB3f next_bounds{};
  float growth_incr{0.05f};
  float recede_incr{0.1f};
  ArchRenderer::GeometryHandle growing_geom_handle{};
  ArchRenderer::DrawableHandle growing_drawable_handle{};
  ArchRenderer::GeometryHandle aggregate_geom_handle{};
  ArchRenderer::DrawableHandle aggregate_drawable_handle{};
  DynamicArray<ArchComponentStructurePieceBoundsElements, 64> bounds_elements;
  std::vector<arch::WallHole> pending_holes;
  PendingFinishPruning pending_finish_prune;
  std::vector<tree::VineInstanceHandle> vine_instances;
  bool need_compute_bounds{};
  bool need_start_receding{};
  bool waiting_on_roots_or_trees_to_become_pruneable{};
  bool growing{};
  bool receding{};
};

struct PendingProjectOntoMesh {
  arch::SegmentedStructureHandle structure{};
};

struct ArchComponent {
  ArchComponentStructure debug_structure;

  bounds::AccessorID bounds_accessor_id{bounds::AccessorID::create()};
  tree::TreeNodeCollisionWithObjectContext collision_context;

  bool use_collider_bounds{true};
  float bounds_theta{};
  Optional<PendingProjectOntoMesh> pending_project_onto_mesh;
  std::unique_ptr<arch::ProjectInternodesOnStructureFuture> project_nodes_on_structure_future;

  std::vector<uint16_t> tmp_u16;
  DynamicArray<ArchComponentStructurePieceBoundsElements, 16> bounds_pending_removal;

  bounds::ElementTag arch_bounds_element_tag{};
  bounds::RadiusLimiterElementTag arch_radius_limiter_element_tag{};

  bool disable_tentative_bounds_highlight{};
  bool disable_connection_to_parent{true};
  double repr_elapsed_time{};
};

namespace {

bounds::RadiusLimiterElement make_arch_radius_limiter_element(
  const OBB3f& arch_obb,
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

bool check_finished_pruning(PendingFinishPruning& pend, const tree::TreeSystem* tree_sys,
                            const tree::RootsSystem* roots_sys) {
  bool trees_finished{};
  {
    auto it = pend.trees.begin();
    while (it != pend.trees.end()) {
      auto tree = tree::read_tree(tree_sys, *it);
      if (tree.events.just_finished_pruning) {
        it = pend.trees.erase(it);
      } else {
        ++it;
      }
    }
    trees_finished = pend.trees.empty();
  }
  bool roots_finished{};
  {
    auto it = pend.roots.begin();
    while (it != pend.roots.end()) {
      auto roots = tree::read_roots_instance(roots_sys, *it);
      if (roots.events.just_finished_pruning) {
        it = pend.roots.erase(it);
      } else {
        ++it;
      }
    }
    roots_finished = pend.roots.empty();
  }
  return trees_finished && roots_finished;
}

ArchComponentStructurePieceBoundsElements
insert_piece_bounds(ArchComponent* component, bounds::Accel* accel,
                    bounds::RadiusLimiter* lim, const OBB3f& bounds) {
  ArchComponentStructurePieceBoundsElements result;
  {
    const auto id = bounds::ElementID::create();
    accel->insert(bounds::make_element(bounds, id.id, id.id, component->arch_bounds_element_tag.id));
    result.bounds_element = id;
  }
  {
    const auto id = bounds::RadiusLimiterAggregateID::create();
    auto el = make_arch_radius_limiter_element(bounds, id, component->arch_radius_limiter_element_tag);
    auto handle = bounds::insert(lim, el, false);
    result.radius_limiter_aggregate_id = id;
    result.radius_limiter_element_handle = handle;
  }
  return result;
}

std::vector<arch::WallHole> make_randomized_wall_holes(const OBB3f& bounds) {
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

auto maybe_compute_wall_holes_pruning_nodes(
  ArchComponent* component, const OBB3f& bounds,
  tree::TreeSystem* tree_system, tree::RootsSystem* roots_system,
  const bounds::Accel* accel, const bounds::RadiusLimiter* radius_limiter,
  const Optional<bounds::ElementID>& allow_element,
  const Optional<bounds::RadiusLimiterAggregateID>& allow_aggregate) {
  //
  struct Result {
    bool can_extrude;
    bool hit_something_unpruneable;
    std::vector<arch::WallHole> holes;
    PendingFinishPruning pending_finish_pruning;
  };

  Result result{};

  //  1. Check for roots
  auto root_isect_res = arch::root_bounds_intersect(
    radius_limiter, bounds,
    tree::get_roots_radius_limiter_element_tag(roots_system),
    tree::get_tree_radius_limiter_element_tag(tree_system),
    allow_aggregate);

  if (root_isect_res.any_hit_besides_tree_or_roots) {
    //  Hit something we can't prune
    result.hit_something_unpruneable = true;
    return result;
  }

  if (root_isect_res.any_hit_roots &&
      !arch::can_prune_all_candidates(roots_system, root_isect_res)) {
    //  Can't prune the roots we've hit
    return result;
  }

  //  2. Check for trees
  auto tree_isect_res = arch::internode_bounds_intersect(accel, bounds, tree_system, allow_element);
  if (tree_isect_res.any_hit_besides_trees_or_leaves) {
    //  Hit something we can't prune
    result.hit_something_unpruneable = true;
    return result;
  }

  if (tree_isect_res.any_hit &&
      !arch::can_prune_all_candidates(tree_system, tree_isect_res)) {
    //  Can't prune the trees we've hit
    return result;
  }

  std::vector<arch::WallHole> holes;
  if (root_isect_res.any_hit_roots) {
    arch::TreeNodeCollideThroughHoleParams collide_params{};
    arch::ComputeWallHolesAroundRootsParams hole_params{};
    hole_params.intersect_result = &root_isect_res;
    hole_params.wall_bounds = bounds;
    hole_params.roots_system = roots_system;
    hole_params.collision_context = &component->collision_context;
    hole_params.collide_through_hole_params = &collide_params;
    auto hole_res = arch::compute_wall_holes_around_roots(hole_params);
    holes = std::move(hole_res.holes);
    result.pending_finish_pruning.roots = arch::start_pruning_collided(std::move(hole_res), roots_system);
  }

  {
    const bool try_compute_holes = holes.empty();
    arch::TreeNodeCollideThroughHoleParams collide_params{};
    arch::ComputeWallHolesAroundInternodesParams hole_params{};
    hole_params.wall_bounds = bounds;
    hole_params.tree_system = tree_system;
    hole_params.collision_context = &component->collision_context;
    if (try_compute_holes) {
      hole_params.collide_through_hole_params = &collide_params;
    }
    auto hole_res = arch::compute_wall_holes_around_internodes(tree_isect_res, hole_params);

    result.pending_finish_pruning.trees = arch::start_pruning_collided(
      std::move(hole_res.pending_prune), std::move(hole_res.reevaluate_leaf_bounds), tree_system);

    if (try_compute_holes) {
      holes = std::move(hole_res.holes);
    }
  }

  result.holes = std::move(holes);
  result.can_extrude = true;
  return result;
}

void update_pending_projection_onto_structure(
  ArchComponent* component, ArchComponentStructure& structure, const UpdateInfo& info) {
  //
  if (component->project_nodes_on_structure_future) {
    auto& fut = component->project_nodes_on_structure_future;
    if (!fut->is_ready()) {
      return;
    }

//    const float radius = 0.04f;
    const float radius = 0.03f;
    auto& inodes = fut->result.post_process_res.internodes;
    auto& mesh_ns = fut->result.post_process_res.true_mesh_normals;
    assert(mesh_ns.size() == inodes.size());
#if 1
    for (auto& node : inodes) {
      node.diameter = radius * 2.0f;
    }
#endif
    if (!inodes.empty()) {
      auto vine_inst = tree::create_vine_instance(info.vine_system, radius);
      auto vine_seg = tree::emplace_vine_from_internodes(
        info.vine_system, info.render_vine_system, vine_inst,
        inodes.data(), mesh_ns.data(), int(inodes.size()));

      const int tip_ind = tree::axis_tip_index(inodes, 0);
      assert(tip_ind >= 0 && tip_ind < int(mesh_ns.size()));

      tree::VineSystemTryToJumpToNearbyTreeParams jump_params{};
      jump_params.use_initial_offset = true;
      jump_params.initial_offset = mesh_ns[tip_ind] * 0.5f;

      tree::try_to_jump_to_nearby_tree(info.vine_system, vine_inst, vine_seg, jump_params);
      tree::set_growth_rate_scale(info.vine_system, vine_inst, 6.0f);
#if 1
      tree::create_ornamental_foliage_on_vine_segment(vine_inst, vine_seg);
#endif
      structure.vine_instances.emplace_back() = vine_inst;
    }

    component->project_nodes_on_structure_future = nullptr;
  }

  if (!component->pending_project_onto_mesh || !info.left_clicked || info.num_proj_internodes == 0) {
    return;
  }

  auto* sys = arch::get_global_segmented_structure_system();
  auto& pend_proj = component->pending_project_onto_mesh.value();
  auto* geom = arch::get_geometry(sys, pend_proj.structure);
  auto proj_ti = geom->ray_intersect(info.mouse_ray);
  if (!proj_ti) {
    return;
  }

  arch::ProjectInternodesOnStructureParams proj_params{};
  proj_params.internodes = info.proj_internodes;
  proj_params.num_internodes = info.num_proj_internodes;
  proj_params.structure_pieces = geom->pieces.data();
  proj_params.num_pieces = uint32_t(geom->pieces.size());
  proj_params.tris = geom->triangles.data();
  proj_params.num_tris = geom->num_triangles();
  proj_params.positions_or_aggregate_geometry = geom->geometry.data();
  proj_params.normals_or_nullptr = nullptr;
  proj_params.aggregate_geometry_stride_bytes = geom->vertex_stride_bytes();
  proj_params.num_vertices = geom->num_vertices();
  proj_params.initial_proj_ti = proj_ti.value();
  proj_params.ray_theta_offset = 0.0; //  @TODO
  proj_params.ray_len = 8.0;  //  @TODO
  proj_params.diameter_power = 1.5f;
  component->project_nodes_on_structure_future = arch::project_internodes_onto_structure(proj_params);
  component->pending_project_onto_mesh = NullOpt{};
}

void init_drawables(ArchComponent* component, ArchComponentStructure* structure,
                    arch::SegmentedStructureSystem* sys, ArchRenderer* renderer) {
  { //  growing
    auto get_data = [sys, handle = structure->structure_handle](
      const void** geom_data, size_t* geom_size, const void** inds_data, size_t* inds_size) {
      //
      auto tri_data = arch::read_growing_triangle_data(sys, handle);
      assert(tri_data);

      *geom_data = tri_data.value().vertices;
      *geom_size = tri_data.value().num_vertices * sizeof(Vec3f) * 2;
      *inds_data = tri_data.value().indices;
      *inds_size = tri_data.value().num_active_indices * sizeof(uint16_t);
    };

    auto reserve_data = [sys, handle = structure->structure_handle](size_t* num_verts, size_t* num_inds) {
      auto tri_data = arch::read_growing_triangle_data(sys, handle);
      assert(tri_data);
      *num_verts = tri_data.value().num_vertices;
      *num_inds = tri_data.value().num_total_indices;
    };

    structure->growing_geom_handle = renderer->create_dynamic_geometry(
      std::move(get_data), std::move(reserve_data));
    ArchRenderer::DrawableParams params{};
    params.color = Vec3f{1.0f};
    structure->growing_drawable_handle = renderer->create_drawable(
      structure->growing_geom_handle, params);
  }
  { //  aggregate
    auto get_data = [sys, component, handle = structure->structure_handle](
      const void** geom_data, size_t* geom_size, const void** inds_data, size_t* inds_size) {

      auto* geom = arch::get_geometry(sys, handle);
      component->tmp_u16.resize(geom->triangles.size());
      for (uint32_t i = 0; i < uint32_t(geom->triangles.size()); i++) {
        assert(geom->triangles[i] < 0xffffu);
        component->tmp_u16[i] = uint16_t(geom->triangles[i]);
      }

      *geom_data = geom->geometry.data();
      *geom_size = geom->num_vertices() * geom->vertex_stride_bytes();
      *inds_data = component->tmp_u16.data();
      *inds_size = component->tmp_u16.size() * sizeof(uint16_t);
    };

    auto reserve_data = [sys, handle = structure->structure_handle](size_t* num_verts, size_t* num_inds) {
      auto* geom = arch::get_geometry(sys, handle);
      *num_verts = geom->num_vertices();
      *num_inds = geom->num_triangles() * 3;
    };

    structure->aggregate_geom_handle = renderer->create_dynamic_geometry(
      std::move(get_data), std::move(reserve_data));
    ArchRenderer::DrawableParams params{};
    params.color = Vec3f{1.0f};
    structure->aggregate_drawable_handle = renderer->create_drawable(
      structure->aggregate_geom_handle, params);
  }
}

void destroy_vine_instances(ArchComponentStructure& structure, tree::VineSystem* sys) {
  for (auto& inst : structure.vine_instances) {
    tree::destroy_vine_instance(sys, inst);
  }
  structure.vine_instances.clear();
}

void state_computing_bounds(ArchComponent* component, ArchComponentStructure& structure,
                            arch::SegmentedStructureSystem* sys, const UpdateInfo& info) {
  auto struct_handle = structure.structure_handle;
  if (structure.need_start_receding && arch::can_start_receding_structure(sys, struct_handle)) {
    structure.state = StructureState::Receding;
    arch::start_receding_structure(sys, struct_handle);
    //
    destroy_vine_instances(structure, info.vine_system);
    structure.need_start_receding = false;
    structure.receding = true;
  }

  Optional<OBB3f> next_bounds;
  if (structure.need_compute_bounds &&
      arch::num_pieces_in_structure(sys, struct_handle) < Config::max_num_pieces_per_structure &&
      arch::can_extrude_structure(sys, struct_handle)) {
    //
    auto par_bounds = arch::get_last_structure_piece_bounds(sys, struct_handle);
    if (!par_bounds || component->use_collider_bounds) {
      next_bounds = info.debug_collider_bounds;
    } else {
      next_bounds = arch::extrude_obb_xz(
        par_bounds.value(), component->bounds_theta, info.debug_collider_bounds.half_size * 2.0f);
    }
    structure.need_compute_bounds = false;
  }
  if (next_bounds) {
    structure.next_bounds = next_bounds.value();
    structure.state = StructureState::ComputingCollision;
  }
}

void state_computing_collision(ArchComponent* component, ArchComponentStructure& structure,
                               arch::SegmentedStructureSystem*, const UpdateInfo& info) {
  auto* accel = bounds::request_write(
    info.bounds_system, info.accel_handle, component->bounds_accessor_id);
  if (!accel) {
    return;
  }

  const OBB3f bounds = structure.next_bounds;
  assert(all(gt(bounds.half_size, Vec3f{})));

  ArchComponentStructurePieceBoundsElements last_piece_bounds_elements;
  if (!structure.bounds_elements.empty()) {
    last_piece_bounds_elements = structure.bounds_elements.back();
  }

  auto prune_res = maybe_compute_wall_holes_pruning_nodes(
    component, bounds, info.tree_system, info.roots_system, accel, info.radius_limiter,
    last_piece_bounds_elements.bounds_element,
    last_piece_bounds_elements.radius_limiter_aggregate_id);

  structure.waiting_on_roots_or_trees_to_become_pruneable = false;
  if (prune_res.can_extrude) {
    structure.bounds_elements.push_back(
      insert_piece_bounds(component, accel, info.radius_limiter, bounds));
    structure.pending_finish_prune = std::move(prune_res.pending_finish_pruning);
    structure.pending_holes = std::move(prune_res.holes);
    structure.state = StructureState::PendingFinishPruning;

  } else if (!prune_res.hit_something_unpruneable) {
    structure.waiting_on_roots_or_trees_to_become_pruneable = true;
  }

  bounds::release_write(info.bounds_system, info.accel_handle, component->bounds_accessor_id);
}

void state_pending_finish_pruning(ArchComponent* component, ArchComponentStructure& structure,
                                  arch::SegmentedStructureSystem* sys, const UpdateInfo& info) {
  const bool finished_pruning = check_finished_pruning(
    structure.pending_finish_prune, info.tree_system, info.roots_system);
  if (!finished_pruning) {
    return;
  }

  auto& computed_holes = structure.pending_holes;
#if 1
  if (computed_holes.empty() && urand() < 0.9) {
    computed_holes = make_randomized_wall_holes(structure.next_bounds);
  }
#endif
  arch::ExtrudeSegmentedStructureParams extrude_params{};
  if (component->use_collider_bounds || component->disable_connection_to_parent) {
    extrude_params.disable_connection_to_parent = true;
  } else {
    auto not_up = [](const OBB3f& b) { return b.j != ConstVec3f::positive_y; };
    auto par_bounds = arch::get_last_structure_piece_bounds(sys, structure.structure_handle);
    if (not_up(structure.next_bounds) || (par_bounds && not_up(par_bounds.value()))) {
      extrude_params.disable_connection_to_parent = true;
    }
  }

  if (computed_holes.empty()) {
    extrude_params.prefer_default_holes = true;
  } else {
    extrude_params.holes = computed_holes.data();
    extrude_params.num_holes = int(computed_holes.size());
  }

  arch::extrude_structure(sys, structure.structure_handle, structure.next_bounds, extrude_params);
  structure.state = StructureState::ComputingBounds;
  structure.growing = true;
}

void begin_update_structure(ArchComponent* component, ArchComponentStructure& structure,
                            arch::SegmentedStructureSystem* sys, const UpdateInfo& info) {
  switch (structure.state) {
    case StructureState::ComputingBounds: {
      state_computing_bounds(component, structure, sys, info);
      break;
    }
    case StructureState::ComputingCollision: {
      state_computing_collision(component, structure, sys, info);
      break;
    }
    case StructureState::PendingFinishPruning: {
      state_pending_finish_pruning(component, structure, sys, info);
      break;
    }
    case StructureState::Receding: {
      //
      break;
    }
    default: {
      assert(false);
    }
  }
}

void evaluate_updated_structure(ArchComponent* component, ArchComponentStructure& structure,
                                arch::SegmentedStructureSystem* sys, const UpdateInfo& info) {
  auto struct_handle = structure.structure_handle;
  if (structure.state == StructureState::Receding) {
    if (arch::structure_receded(sys, struct_handle)) {
      info.renderer->set_modified(structure.growing_geom_handle);
    }
    if (arch::structure_just_prepared_receding_piece(sys, struct_handle)) {
      info.renderer->set_modified(structure.aggregate_geom_handle);
      if (!structure.bounds_elements.empty()) {
        auto bounds_els = structure.bounds_elements.back();
        structure.bounds_elements.pop_back();
        component->bounds_pending_removal.push_back(bounds_els);
      }
    }
    if (arch::structure_just_finished_receding(sys, struct_handle)) {
      structure.state = StructureState::ComputingBounds;
      structure.receding = false;
    }
  } else {
    if (arch::structure_grew(sys, struct_handle)) {
      info.renderer->set_modified(structure.growing_geom_handle);
    }

    if (arch::structure_just_finished_growing(sys, struct_handle)) {
      info.renderer->set_modified(structure.aggregate_geom_handle);
      structure.growing = false;
    }
  }
}

void remove_pending_bounds(ArchComponent* component, const UpdateInfo& info) {
  if (component->bounds_pending_removal.empty()) {
    return;
  }

  auto* accel = bounds::request_write(
    info.bounds_system, info.accel_handle, component->bounds_accessor_id);
  if (!accel) {
    return;
  }

  for (auto& pend : component->bounds_pending_removal) {
    if (pend.bounds_element) {
      bounds::push_pending_deactivation(
        info.bounds_system, info.accel_handle, &pend.bounds_element.value(), 1);
    }
    if (pend.radius_limiter_element_handle) {
      assert(pend.radius_limiter_aggregate_id);
      bounds::remove(info.radius_limiter, pend.radius_limiter_element_handle.value());
    }
  }

  component->bounds_pending_removal.clear();
  bounds::release_write(info.bounds_system, info.accel_handle, component->bounds_accessor_id);
}

void draw_bounds_column_segment(
  const Vec3f& p0, const Vec3f& p1, const Vec3f& j, const Vec3f& k, float w2, float dw) {
  //
  particle::SegmentedQuadVertexDescriptor vert_descs[6];
  for (auto& v : vert_descs) {
    v.min_depth_weight = dw;
    v.color = Vec3f{1.0f, 0.0f, 0.0f};
    v.translucency = 0.25f;
  }

  auto p00 = p0 - j * w2;
  auto p01 = p0 + j * w2;
  auto p10 = p1 - j * w2;
  auto p11 = p1 + j * w2;

  vert_descs[0].position = p00 + k * w2;
  vert_descs[1].position = p01 + k * w2;
  vert_descs[2].position = p11 + k * w2;
  vert_descs[3].position = p11 + k * w2;
  vert_descs[4].position = p10 + k * w2;
  vert_descs[5].position = p00 + k * w2;

  particle::push_segmented_quad_sample_depth_image_particle_vertices(vert_descs, 6);
}

void draw_bounds_column(
  const Vec3f& p0, const Vec3f& p1,
  const Vec3f& i, const Vec3f& j, const Vec3f& k, float w2, float dw) {
  //
  const float w = w2 * 2.0f;
  draw_bounds_column_segment(p0 + i * w, p1 - i * w, j, k, w2, dw);   //  front
  draw_bounds_column_segment(p0 + i * w, p1 - i * w, j, -k, w2, dw);  //  back
  draw_bounds_column_segment(p0 + i * w, p1 - i * w, k, j, w2, dw);   //  top
  draw_bounds_column_segment(p0 + i * w, p1 - i * w, -k, j, w2, dw);  //  bottom
}

void draw_tentative_bounds(const OBB3f& bounds, float dw) {
  Vec3f vs[8];
  gather_vertices(bounds, vs);
  const float w = 0.125f;
  const float w2 = w * 0.5f;

  for (int i = 0; i < 2; i++) {
    const int o = i * 4;
    draw_bounds_column(vs[0 + o], vs[1 + o], {}, bounds.j, bounds.k, w2, dw);
    draw_bounds_column(vs[3 + o], vs[2 + o], {}, bounds.j, bounds.k, w2, dw);
    draw_bounds_column(vs[1 + o], vs[2 + o], {}, bounds.i, bounds.k, w2, dw);
    draw_bounds_column(vs[3 + o], vs[0 + o], {}, bounds.i, bounds.k, w2, dw);
  }

  draw_bounds_column(vs[0], vs[4], {}, bounds.j, bounds.i, w2, dw);
  draw_bounds_column(vs[1], vs[5], {}, bounds.j, bounds.i, w2, dw);
  draw_bounds_column(vs[2], vs[6], {}, bounds.j, bounds.i, w2, dw);
  draw_bounds_column(vs[3], vs[7], {}, bounds.j, bounds.i, w2, dw);
}

void draw_tentative_bounds(const ArchComponent* component, const OBB3f& tentative_bounds) {
  if (component->disable_tentative_bounds_highlight) {
    return;
  }

  auto& structure = component->debug_structure;
  if (structure.growing || structure.receding) {
    return;
  }

  double min_depth_weight = clamp01(std::sin(component->repr_elapsed_time * 8.0) * 0.5 + 0.5);
  draw_tentative_bounds(tentative_bounds, float(min_depth_weight));
}

struct {
  ArchComponent component;
} globals;

} //  anon

ArchComponent* get_global_arch_component() {
  return &globals.component;
}

void initialize_arch_component(ArchComponent* component, const InitInfo& info) {
  assert(info.arch_bounds_element_tag.is_valid());
  assert(info.arch_radius_limiter_element_tag.is_valid());

  arch::initialize_structure_geometry_context();

  auto* sys = arch::get_global_segmented_structure_system();
  arch::initialize_segmented_structure_system(sys);

  component->arch_bounds_element_tag = info.arch_bounds_element_tag;
  component->arch_radius_limiter_element_tag = info.arch_radius_limiter_element_tag;

  {
    arch::CreateSegmentedStructureParams create_params{};
    create_params.origin = Vec3f{8.0f, 5.5f, 16.0f};
    component->debug_structure.structure_handle = create_structure(sys, create_params);
    component->debug_structure.state = StructureState::ComputingBounds;
  }

  init_drawables(component, &component->debug_structure, sys, info.renderer);
}

void update_arch_component(ArchComponent* component, const UpdateInfo& info) {
  auto* sys = arch::get_global_segmented_structure_system();
  begin_update_structure(component, component->debug_structure, sys, info);

  arch::update_segmented_structure_system(sys, {
    info.real_dt
  });

  evaluate_updated_structure(component, component->debug_structure, sys, info);
  update_pending_projection_onto_structure(component, component->debug_structure, info);

  if (!component->use_collider_bounds) {
    auto struct_handle = component->debug_structure.structure_handle;
    if (auto par_bounds = arch::get_last_structure_piece_bounds(sys, struct_handle)) {
      auto tentative_bounds = arch::extrude_obb_xz(
        par_bounds.value(), component->bounds_theta, info.debug_collider_bounds.half_size * 2.0f);
      draw_tentative_bounds(component, tentative_bounds);
    }
  } else {
    draw_tentative_bounds(component, info.debug_collider_bounds);
  }

  remove_pending_bounds(component, info);
  component->repr_elapsed_time += info.real_dt;
}

void set_arch_component_params(ArchComponent* component, const ArchComponentParams& params) {
  component->use_collider_bounds = !params.extrude_from_parent;
  component->bounds_theta = params.extrude_theta;
  component->disable_tentative_bounds_highlight = params.disable_tentative_bounds_highlight;
}

ArchComponentParams get_arch_component_params(const ArchComponent* component) {
  ArchComponentParams result{};
  result.extrude_from_parent = !component->use_collider_bounds;
  result.extrude_theta = component->bounds_theta;
  result.disable_tentative_bounds_highlight = component->disable_tentative_bounds_highlight;
  return result;
}

void set_arch_component_need_extrude_structure(ArchComponent* component) {
  component->debug_structure.need_compute_bounds = true;
}

void set_arch_component_need_recede_structure(ArchComponent* component) {
  component->debug_structure.need_start_receding = true;
}

void set_arch_component_need_project_onto_structure(ArchComponent* component) {
  PendingProjectOntoMesh pend{};
  pend.structure = component->debug_structure.structure_handle;
  component->pending_project_onto_mesh = pend;
}

ArchComponentExtrudeInfo get_arch_component_extrude_info(const ArchComponent* component) {
  auto& structure = component->debug_structure;

  const bool can_modify = !structure.need_compute_bounds &&
    !structure.need_start_receding && !structure.growing && !structure.receding &&
    structure.state == StructureState::ComputingBounds;

  ArchComponentExtrudeInfo result{};
  result.waiting_on_trees_or_roots_to_finish_pruning = structure.pending_finish_prune.any() ||
    structure.waiting_on_roots_or_trees_to_become_pruneable;
  result.can_extrude = can_modify;
  result.can_recede = can_modify;
  result.growing = structure.growing;
  result.receding = structure.receding;
  return result;
}

void render_arch_component_gui(ArchComponent* component) {
  ImGui::Begin("Arch");

  auto* structure_sys = arch::get_global_segmented_structure_system();
  const auto* geom = arch::get_geometry(structure_sys, component->debug_structure.structure_handle);
  ImGui::Text("MaxVertexIndex: %d", int(geom->max_vertex_index_or_zero()));
  ImGui::Text("NumPieces: %d", int(geom->pieces.size()));

  ImGui::Checkbox("UseColliderBounds", &component->use_collider_bounds);
  if (ImGui::Button("Extrude")) {
    component->debug_structure.need_compute_bounds = true;
  }
  if (ImGui::Button("Recede")) {
    component->debug_structure.need_start_receding = true;
  }
  if (ImGui::Button("ProjectOntoMesh")) {
    PendingProjectOntoMesh pend{};
    pend.structure = component->debug_structure.structure_handle;
    component->pending_project_onto_mesh = pend;
  }
  if (ImGui::SliderFloat("StructureGrowthIncr", &component->debug_structure.growth_incr, 0.0f, 1.0f)) {
    arch::set_structure_growth_incr(
      structure_sys, component->debug_structure.structure_handle, component->debug_structure.growth_incr);
  }
  if (ImGui::SliderFloat("StructureRecedeIncr", &component->debug_structure.recede_incr, 0.0f, 1.0f)) {
    arch::set_structure_recede_incr(
      structure_sys, component->debug_structure.structure_handle, component->debug_structure.recede_incr);
  }
  ImGui::SliderFloat("BoundsTheta", &component->bounds_theta, -pif(), pif());
  ImGui::End();
}

GROVE_NAMESPACE_END
