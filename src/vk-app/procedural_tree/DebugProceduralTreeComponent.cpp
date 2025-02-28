#include "DebugProceduralTreeComponent.hpp"
#include "../render/frustum_cull_data.hpp"
#include "../render/debug_draw.hpp"
#include "../render/render_tree_leaves.hpp"
#include "leaf_geometry.hpp"
#include "growth_on_nodes.hpp"
#include "radius_limiter.hpp"
#include "resource_flow_along_nodes.hpp"
#include "../render/render_resource_flow_along_nodes_particles.hpp"
#include "vine_system.hpp"
#include "../render/render_vines.hpp"
#include "../terrain/terrain.hpp"
#include "serialize.hpp"
#include "utility.hpp"
#include "render.hpp"
#include "bud_fate.hpp"
#include "node_mesh.hpp"
#include "fit_bounds.hpp"
#include "debug_node_rendering.hpp"
#include "debug_health.hpp"
#include "leaf_geometry.hpp"
#include "../render/render_ornamental_foliage_data.hpp"
#include "ProceduralTreeComponent.hpp"
#include "grove/env.hpp"
#include "grove/math/random.hpp"
#include "grove/math/ease.hpp"
#include "grove/math/frame.hpp"
#include "grove/math/Frustum.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/common/common.hpp"
#include "grove/common/pack.hpp"
#include "grove/audio/oscillator.hpp"
#include "grove/visual/Image.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/load/image.hpp"
#include "grove/math/OBB3.hpp"
#include <imgui/imgui.h>
#include <iostream>
#include <numeric>

GROVE_NAMESPACE_BEGIN

#define DEBUG_GROW (0)

namespace {

using InitInfo = DebugProceduralTreeComponent::InitInfo;
using UpdateInfo = DebugProceduralTreeComponent::UpdateInfo;

#if DEBUG_GROW
tree::SpawnInternodeParams make_thin_spawn_params(float tree_scale) {
  auto spawn_p = tree::SpawnInternodeParams::make_debug(tree_scale);
  return spawn_p;
}

tree::RenderAxisGrowthContext initialize_axis_render_growth(tree::TreeNodeStore& tree_nodes) {
  const tree::TreeNodeIndex root_index = 0;
  tree::RenderAxisGrowthContext context;
  context.root_axis_index = root_index;
  context.growing.push_back(root_index);
  tree::set_render_length_scale(tree_nodes.internodes, root_index, 0.0f);
  return context;
}

void apply_render_growth_change(tree::TreeNodeStore& tree_nodes,
                                const tree::SpawnInternodeParams& spawn_params,
                                tree::TreeNodeIndex root_axis_index) {
  tree::set_render_position(tree_nodes.internodes, root_axis_index);
  auto spawn_p = spawn_params;
  spawn_p.attenuate_diameter_by_length_scale = true;
  tree::set_diameter(tree_nodes.internodes, spawn_p, root_axis_index);
}

bool update_render_growth(tree::TreeNodeStore& tree_nodes,
                          const tree::SpawnInternodeParams& spawn_params,
                          tree::RenderAxisGrowthContext& growth_context,
                          float incr) {
  if (tree::tick_render_axis_growth(tree_nodes.internodes, growth_context, incr)) {
    apply_render_growth_change(tree_nodes, spawn_params, growth_context.root_axis_index);
    return true;
  } else {
    return false;
  }
}
#endif

[[maybe_unused]] Optional<Image<uint8_t>> load_image_in_resource_dir(const char* p) {
  auto im_p = std::string{GROVE_ASSET_DIR} + p;
  bool success{};
  auto im = load_image(im_p.c_str(), &success, true);
  if (success) {
    return Optional<Image<uint8_t>>(std::move(im));
  } else {
    return NullOpt{};
  }
}

[[maybe_unused]] Optional<Image<uint8_t>> load_debug_plane_texture_image() {
  return load_image_in_resource_dir("/models/petal1/petal1-style-512.png");
}

[[maybe_unused]] Optional<Image<uint8_t>> load_debug_daffodil_texture() {
  return load_image_in_resource_dir("/models/petal1/daffodil3.png");
}

void create_debug_tree_mesh_data(const tree::Internodes& nodes, std::vector<float>* verts,
                                 std::vector<uint16_t>* inds) {
  tree::MakeNodeMeshParams mesh_params{};
  mesh_params.offset = {};
  mesh_params.include_uv = true;

  const auto num_inodes = uint32_t(nodes.size());
  Vec2<int> grid_xz{5, 2};
  size_t num_verts = tree::compute_num_vertices_in_node_mesh(grid_xz, num_inodes);
  size_t num_inds = tree::compute_num_indices_in_node_mesh(grid_xz, num_inodes);

  *verts = std::vector<float>(num_verts * 8);  //  pos, normal, uv
  *inds = std::vector<uint16_t>(num_inds);
  tree::make_node_mesh(nodes.data(), num_inodes, grid_xz, mesh_params, verts->data(), inds->data());
}

void create_debug_tree_mesh_leaves_data(const tree::Internodes& nodes, std::vector<float>* verts) {
  std::vector<Vec3f> ps;
  std::vector<Vec3f> ns;
  for (auto& node : nodes) {
    if (node.is_leaf()) {
      ps.push_back(node.render_position);
      ns.push_back(node.direction);
    }
  }

  const auto num_elements = uint32_t(ps.size());
  auto geom = tree::make_planes_distributed_along_axis(tree::LeafGeometryParams::make_original());
  assert(geom.descriptor.vertex_size_bytes() == sizeof(Vec3f) * 2 + sizeof(Vec2f));

  auto num_src_vertices = geom.descriptor.num_vertices(geom.data.size() * sizeof(float));
  auto max_num_dst_verts = uint32_t(num_elements * num_src_vertices);
  *verts = std::vector<float>(max_num_dst_verts * 8);

  tree::AmplifyGeometryOrientedAtInternodesParams amplify_params{};
  amplify_params.positions = ps.data();
  amplify_params.directions = ns.data();
  amplify_params.num_elements = num_elements;
  amplify_params.src = geom.data.data();
  amplify_params.src_byte_stride = uint32_t(geom.descriptor.vertex_size_bytes());
  amplify_params.src_position_byte_offset = uint32_t(geom.descriptor.ith_attribute_offset_bytes(0));
  amplify_params.src_normal_byte_offset = uint32_t(geom.descriptor.ith_attribute_offset_bytes(1));
  amplify_params.src_uv_byte_offset = uint32_t(geom.descriptor.ith_attribute_offset_bytes(2));
  amplify_params.num_src_vertices = uint32_t(num_src_vertices);
  amplify_params.dst = verts->data();
  amplify_params.dst_byte_stride = amplify_params.src_byte_stride;
  amplify_params.dst_position_byte_offset = amplify_params.src_position_byte_offset;
  amplify_params.dst_normal_byte_offset = amplify_params.src_normal_byte_offset.value();
  amplify_params.dst_uv_byte_offset = amplify_params.src_uv_byte_offset.value();
  amplify_params.max_num_dst_vertices = max_num_dst_verts;
  amplify_params.scale = 0.1f;
  tree::amplify_geometry_oriented_at_internodes(amplify_params);
}

VertexBufferDescriptor make_tree_mesh_buffer_desc() {
  VertexBufferDescriptor result;
  result.add_attribute(AttributeDescriptor::float3(0));
  result.add_attribute(AttributeDescriptor::float3(1));
  result.add_attribute(AttributeDescriptor::float2(2));
  return result;
}

ArchRenderer::DrawableHandle
create_debug_tree_mesh_drawable(ArchRenderer& renderer,
                                const ArchRenderer::AddResourceContext& renderer_context,
                                const std::vector<float>& verts,
                                const std::vector<uint16_t>& inds) {
  auto geom = renderer.create_static_geometry();
  bool success = renderer.update_geometry(
    renderer_context, geom,
    verts.data(), verts.size() * sizeof(float),
    make_tree_mesh_buffer_desc(), 0, Optional<int>(1),
      inds.data(), uint32_t(inds.size()));
  assert(success);
  (void) success;

  ArchRenderer::DrawableParams drawable_params{};
  drawable_params.scale = 1.0f;
  drawable_params.color = Vec3f{1.0f};
  drawable_params.translation = {};
  return renderer.create_drawable(geom, drawable_params);
}

[[maybe_unused]]
void create_debug_tree_mesh(DebugProceduralTreeComponent& component,
                            const tree::Internodes& nodes, const InitInfo& info) {
  std::vector<float> verts;
  std::vector<uint16_t> inds;
  create_debug_tree_mesh_data(nodes, &verts, &inds);

  std::vector<float> leaf_verts;
  std::vector<uint16_t> leaf_inds;
  create_debug_tree_mesh_leaves_data(nodes, &leaf_verts);
  leaf_inds.resize(leaf_verts.size() / 8);
  std::iota(leaf_inds.begin(), leaf_inds.end(), uint16_t(verts.size() / 8));
  verts.insert(verts.end(), leaf_verts.begin(), leaf_verts.end());
  inds.insert(inds.end(), leaf_inds.begin(), leaf_inds.end());

  component.tree_mesh_drawable = create_debug_tree_mesh_drawable(
    info.arch_renderer, info.arch_renderer_context, verts, inds);
  if (component.tree_mesh_drawable) {
    info.arch_renderer.set_active(component.tree_mesh_drawable.value(), true);
  }
}

#if 0
void create_debug_tree_with_flowers(DebugProceduralTreeComponent::Tree& dst_tree,
                                    const InitInfo& info) {
  auto draw_handle = info.proc_flower_stem_renderer.create_drawable(
    info.stem_create_context, dst_tree.nodes.internodes, {});
  if (draw_handle) {
    dst_tree.stem_drawable = draw_handle.value();
  }

  auto im = load_debug_plane_texture_image();
  if (!im || im.value().num_components_per_pixel != 4) {
    return;
  }

  auto create_info = make_image_create_info_2d_srgb(
    im.value().width, im.value().height, 4, im.value().data.get());
  auto im_handle = info.sampled_image_manager.create_sync(info.image_create_context, create_info);
  if (!im_handle) {
    return;
  }

  const auto internode_aabb = tree::internode_aabb(dst_tree.nodes.internodes);
  auto leaf_ptrs = gather_leaf_internode_ptrs(dst_tree.nodes.internodes);
  auto make_ornament_shape = [](float growth, float death) {
    return petal::ShapeParams::plane(growth, death, 1.0f);
  };
  auto shape_params = make_ornament_shape(1.0f, 0.0f);
  auto material_params = petal::MaterialParams::type2();
  auto& orn_renderer = info.proc_flower_ornament_renderer;
  auto geom_handle = orn_renderer.add_alpha_test_procedural_drawable(
    info.ornament_create_context,
    leaf_ptrs,
    internode_aabb,
    shape_params,
    material_params,
    im_handle.value(),
    Vec3f{1.0f});
  if (!geom_handle) {
    GROVE_ASSERT(false);
  } else {
    dst_tree.proc_ornament_drawable = geom_handle.value();
    dst_tree.make_ornament_shape = std::move(make_ornament_shape);
  }
}
#endif

using FoliageInstanceParams = DebugProceduralTreeComponent::FoliageInstanceParams;

FoliageInstanceParams make_wide_spread_out_foliage_instance_params() {
  FoliageInstanceParams result{};
  result.n = 5;
  result.translation_log_min_x = 0.1f;
  result.translation_log_max_x = 2.0f;
  result.translation_step_power = 0.5f;
  result.translation_step_spread_scale = 1.0f;
  result.translation_x_scale = 4.0f;
  result.translation_y_scale = 1.5f;
  result.rand_z_rotation_scale = 0.125f;
  result.curl_scale = 0.5f;
  result.global_scale = 1.0f;
  return result;
}

FoliageInstanceParams make_tighter_foliage_instance_params(bool low_lod) {
  FoliageInstanceParams result{};
  result.n = low_lod ? 3 : 5;
  result.translation_log_min_x = 1.0f;
  result.translation_log_max_x = 5.0f;
  result.translation_step_power = 0.5f;
  result.translation_step_spread_scale = 1.0f;
  result.translation_x_scale = 2.0f;
  result.translation_y_scale = 1.0f;
  result.rand_z_rotation_scale = 0.125f;
  result.curl_scale = 0.5f;
  result.global_scale =  low_lod ? 1.25f : 1.0f;
  return result;
}

FoliageInstanceParams make_hanging_foliage_instance_params() {
  FoliageInstanceParams result{};
  result.n = 5;
  result.translation_log_min_x = 0.1f;
  result.translation_log_max_x = 2.0f;
  result.translation_step_power = 0.25f;
  result.translation_step_spread_scale = 0.1f;
  result.translation_x_scale = 1.5f;
  result.translation_y_scale = 2.0f;
  result.rand_z_rotation_scale = 0.125f;
  result.curl_scale = 0.5f;
  result.global_scale = 1.0f;
  return result;
}

FoliageInstanceParams make_thin_long_foliage_instance_params(bool larger_curl) {
  FoliageInstanceParams result{};
  result.n = 5;
  result.translation_log_min_x = 5.0f;
  result.translation_log_max_x = 5.0f;
  result.translation_step_power = 1.0f;
  result.translation_step_spread_scale = 0.25f;
  result.translation_x_scale = 4.0f;
  result.translation_y_scale = 0.0f;
  result.rand_z_rotation_scale = 1.0f;
  result.curl_scale = larger_curl ? 2.0f : 0.5f;
  result.global_scale = 1.0f;
  return result;
}

FoliageInstanceParams make_thin_foliage_instance_params() {
  FoliageInstanceParams result{};
  result.n = 3;
  result.translation_log_min_x = 5.0f;
  result.translation_log_max_x = 5.0f;
  result.translation_step_power = 1.0f;
  result.translation_step_spread_scale = 0.25f;
  result.translation_x_scale = 2.0f;
  result.translation_y_scale = 0.0f;
  result.rand_z_rotation_scale = 1.0f;
  result.curl_scale = 1.0f;
  result.global_scale = 1.5f;
  return result;
}

FoliageInstanceParams make_floofy_instance_params() {
  FoliageInstanceParams result{};
  result.n = 3;
  result.translation_log_min_x = 1.0f;
  result.translation_log_max_x = 5.0f;
  result.translation_step_power = 0.5f;
  result.translation_step_spread_scale = 0.1f;
  result.translation_x_scale = 2.0f;
  result.translation_y_scale = 1.0f;
  result.rand_z_rotation_scale = 0.5f;
  result.curl_scale = -0.5f;
  result.global_scale = 1.5f;
  return result;
}

[[maybe_unused]] int get_random_foliage_color_image_index() {
  const int values[4] = {0, 1, 2, 3};
  return *uniform_array_sample(values, 4);
}

[[maybe_unused]]
std::vector<int> leaf_indices(const tree::Internodes& inodes) {
  std::vector<int> result;
  for (int i = 0; i < int(inodes.size()); i++) {
    if (inodes[i].is_leaf()) {
      result.push_back(i);
    }
  }
  return result;
}

void update_foliage_occlusion_system(DebugProceduralTreeComponent& component, const UpdateInfo& info) {
  if (component.debug_foliage_lod_system && component.set_foliage_occlusion_check_fade_in_out) {
    //  Clear culled when switching from fade in/out to binary method.
    component.foliage_occlusion_check_fade_in_out =
      component.set_foliage_occlusion_check_fade_in_out.value();
    foliage_occlusion::clear_culled(component.debug_foliage_lod_system);
    component.set_foliage_occlusion_check_fade_in_out = NullOpt{};
  }

  const bool check_occlusion = component.need_check_foliage_lod_system_occlusion ||
    component.continuously_check_occlusion || component.foliage_occlusion_check_fade_in_out;
  if (check_occlusion && component.debug_foliage_lod_system) {
    auto proj = info.camera.get_projection();
    proj[1] = -proj[1];
    auto proj_view = proj * info.camera.get_view();

    foliage_occlusion::CheckOccludedParams occlusion_params{};
    occlusion_params.cull_distance_threshold = component.foliage_lod_cull_distance_threshold;
    occlusion_params.fade_back_in_distance_threshold =
      component.foliage_cull_fade_back_in_distance_threshold;
    occlusion_params.fade_back_in_only_when_below_distance_threshold =
      component.foliage_occlusion_only_fade_back_in_below_distance_threshold;
    occlusion_params.min_intersect_area_fraction = component.foliage_min_intersect_area_fraction;
    occlusion_params.tested_instance_scale = component.foliage_tested_instance_scale;
    occlusion_params.camera_position = info.camera.get_position();
    occlusion_params.camera_projection_view = proj_view;
    occlusion_params.camera_frustum = info.camera.make_world_space_frustum(256.0f);
    occlusion_params.interval = component.occlusion_system_update_interval;
    occlusion_params.fade_in_time_scale = component.occlusion_fade_in_time_scale;
    occlusion_params.fade_out_time_scale = component.occlusion_fade_out_time_scale;
    occlusion_params.cull_time_scale = component.occlusion_cull_time_scale;
    occlusion_params.disable_cpu_check = component.foliage_occlusion_disable_cpu_check;
    occlusion_params.max_num_steps = component.max_num_foliage_occlusion_steps;

    if (component.foliage_occlusion_check_fade_in_out) {
      component.latest_occlusion_check_result = foliage_occlusion::update_clusters(
        component.debug_foliage_lod_system, info.real_dt, occlusion_params);
    } else {
      component.latest_occlusion_check_result = foliage_occlusion::check_occluded(
        component.debug_foliage_lod_system, occlusion_params);
    }
    component.need_check_foliage_lod_system_occlusion = false;
  }

  if (component.need_clear_foliage_lod_system_culled && component.debug_foliage_lod_system) {
    foliage_occlusion::clear_culled(component.debug_foliage_lod_system);
    component.need_clear_foliage_lod_system_culled = false;
  }

  if (component.debug_draw_foliage_lod_system && component.debug_foliage_lod_system) {
    foliage_occlusion::DebugDrawFoliageOcclusionSystemParams draw_params{};
    draw_params.draw_cluster_bounds = component.draw_cluster_bounds;
    draw_params.draw_occluded = component.draw_occluded_instances;
    draw_params.colorize_instances = component.colorize_cluster_instances;
    draw_params.mouse_ro = info.mouse_ray.origin;
    draw_params.mouse_rd = info.mouse_ray.direction;
    foliage_occlusion::debug_draw(component.debug_foliage_lod_system, draw_params);
  }
}

void update_debug_frustum_cull(DebugProceduralTreeComponent& component, const UpdateInfo& info) {
  if (component.update_debug_frustum) {
    component.camera_projection_info = info.camera.get_projection_info();
    component.camera_view = info.camera.get_view();
    component.camera_position = info.camera.get_position();
  }

  const float eval_far = component.far_plane_distance;

  const auto inv_view = inverse(component.camera_view);
  const auto& proj_info = component.camera_projection_info;
  const float s = proj_info.aspect_ratio;
  const float g = proj_info.projection_plane_distance();
  const float n = proj_info.near;
  const float f = eval_far;

  auto v0 = to_vec3(inv_view[0]);
  auto v1 = to_vec3(inv_view[1]);
  auto v2 = to_vec3(inv_view[2]);
  auto frust_world = make_world_space_frustum(s, g, n, f, v0, v1, v2, component.camera_position);

  auto s2 = component.cube_size * 0.5f;
  auto cube_bounds = Bounds3f{-s2 + component.cube_position, s2 + component.cube_position};
  component.cube_visible = frustum_aabb_intersect(frust_world, cube_bounds);

  if (component.draw_debug_frustum_components) {
    vk::debug::draw_frustum_lines(s, g, n, f, inv_view, Vec3f{1.0f, 0.0f, 0.0f});

    auto cube_color = component.cube_visible ?
      Vec3f{0.0f, 1.0f, 0.0f} : Vec3f{1.0f, 0.0f, 0.0f};
    vk::debug::draw_cube(component.cube_position, s2, cube_color);
  }
}

void debug_grid_traverse(const Vec3f& ro, const Vec3f& rd, const Vec3f& cell_dim,
                         Vec3f* traversed_indices, int num_traverse) {
  /*
   * https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.42.3443&rep=rep1&type=pdf
   */
  auto ro_index = floor(ro / cell_dim);

  Vec3f ss{rd.x > 0.0f ? 1.0f : rd.x == 0.0f ? 0.0f : -1.0f,
           rd.y > 0.0f ? 1.0f : rd.y == 0.0f ? 0.0f : -1.0f,
           rd.z > 0.0f ? 1.0f : rd.z == 0.0f ? 0.0f : -1.0f};

  Vec3f incr{rd.x > 0.0f ? ro_index.x + 1.0f : ro_index.x,
             rd.y > 0.0f ? ro_index.y + 1.0f : ro_index.y,
             rd.z > 0.0f ? ro_index.z + 1.0f : ro_index.z};

  const auto ts = abs(cell_dim / rd);

  const auto bounds = incr * cell_dim;
  auto cs = (bounds - ro) / rd;
  cs = Vec3f{rd.x == 0.0f ? infinityf() : cs.x,
             rd.y == 0.0f ? infinityf() : cs.y,
             rd.z == 0.0f ? infinityf() : cs.z};

  Vec3f is{};
  for (int i = 0; i < num_traverse; i++) {
    traversed_indices[i] = ro_index + is;

    if (cs.x < cs.y && cs.x < cs.z) {
      assert(std::isfinite(ts.x));
      is.x += ss.x;
      cs.x += ts.x;
    } else if (cs.y < cs.z) {
      assert(std::isfinite(ts.y));
      is.y += ss.y;
      cs.y += ts.y;
    } else {
      assert(std::isfinite(ts.z));
      is.z += ss.z;
      cs.z += ts.z;
    }
  }
}

void update_debug_grid_traverse(DebugProceduralTreeComponent& component) {
  constexpr int max_num_grid_steps = 1024;
  Vec3f traversed_indices[max_num_grid_steps];
  int num_grid_steps = std::min(max_num_grid_steps, component.num_grid_steps);

  const auto ro = component.grid_traverse_ray_origin;
  const auto rd = component.grid_traverse_ray_direction;
  const auto cell_size = component.grid_traverse_grid_dim;

  if (rd.length() == 0.0f) {
    return;
  }

  debug_grid_traverse(ro, rd, cell_size, traversed_indices, num_grid_steps);

  const Vec3f p1 = ro + rd * float(num_grid_steps) * cell_size.length();
  vk::debug::draw_line(ro, p1, Vec3f{0.0f, 0.0f, 1.0f});

  for (int i = 0; i < num_grid_steps; i++) {
    auto p_min = traversed_indices[i] * cell_size;
    auto p_max = p_min + cell_size;
    vk::debug::draw_aabb3(Bounds3f{p_min, p_max}, Vec3f{1.0f, 1.0f, 1.0f});
  }
}

bool ray_internodes_intersect(const Vec3f& ro, const Vec3f& rd, const OBB3f* node_bounds,
                              int num_nodes, float* t, int* i) {
  float min_t{infinityf()};
  int hit_i{-1};
  for (int ni = 0; ni < num_nodes; ni++) {
    auto& obb = node_bounds[ni];
    auto frame = Mat3f{obb.i, obb.j, obb.k};
    float t0;
    float r = obb.half_size.x;
    float l = obb.half_size.y;
    if (ray_capped_cylinder_intersect(ro, rd, frame, obb.position, r, l, &t0) && t0 < min_t) {
      hit_i = ni;
      min_t = t0;
    }
  }
  if (hit_i >= 0) {
    *t = min_t;
    *i = hit_i;
    return true;
  } else {
    return false;
  }
}

[[maybe_unused]] void update_debug_ray_cylinder_intersect(const UpdateInfo& info) {
  auto* trees = info.proc_tree_component.maybe_read_trees();
  if (!trees) {
    return;
  }

  float min_t{infinityf()};
  OBB3f hit_obb;
  bool any_hit{};
  for (auto& [_, tree] : *trees) {
    auto inst = tree::read_tree(info.tree_system, tree.instance);
    if (!inst.nodes) {
      continue;
    }

    const Vec3f ro = info.mouse_ray.origin;
    const Vec3f rd = info.mouse_ray.direction;
    auto* nodes = inst.nodes->internodes.data();
    const auto num_nodes = int(inst.nodes->internodes.size());

    Temporary<OBB3f, 1024> store_tmp_bounds;
    auto* node_bounds = store_tmp_bounds.require(num_nodes);
    tree::internode_obbs(nodes, num_nodes, node_bounds);

    float t;
    int ni;
    if (ray_internodes_intersect(ro, rd, node_bounds, num_nodes, &t, &ni) && t < min_t) {
      any_hit = true;
      min_t = t;
      hit_obb = tree::internode_obb(inst.nodes->internodes[ni]);
    }
  }

  if (any_hit) {
    vk::debug::draw_obb3(hit_obb, Vec3f{0.0f, 1.0f, 0.0f});
  }
}

std::vector<Vec3f> compute_growth_on_nodes_sample_points(DebugProceduralTreeComponent&,
                                                         const tree::Internodes& internodes,
                                                         bool target_down, int init_node_index) {
  auto num_nodes = int(internodes.size());
  const int points_per_node = 32;
  const float step_size = 0.95f;
  const int max_num_samples = 64;

  std::vector<tree::InternodeSurfaceEntry> store_entries(points_per_node * num_nodes);
  std::vector<int> store_entry_indices(store_entries.size());

  Temporary<OBB3f, 2048> store_bounds;
  Temporary<tree::SamplePointsOnInternodesNodeMetaData, 2048> store_meta;

  auto* bounds = store_bounds.require(num_nodes);
  auto* node_meta = store_meta.require(num_nodes);
  for (int i = 0; i < num_nodes; i++) {
    auto& node = internodes[i];
    bounds[i] = tree::internode_obb(node);
    tree::SamplePointsOnInternodesNodeMetaData meta{};
    meta.is_leaf = node.is_leaf();
    node_meta[i] = meta;
  }
  auto node_aabb = tree::internode_aabb(internodes);

  tree::PlacePointsOnInternodesParams place_params{};
  place_params.node_aabb = node_aabb;
  place_params.node_bounds = bounds;
  place_params.bounds_radius_offset = 0.1f; //  @TODO
  place_params.num_nodes = num_nodes;
  place_params.points_per_node = points_per_node;
  place_params.dst_entries = store_entries.data();
  const int num_entries = tree::place_points_on_internodes(place_params);

  int init_entry_index{-1};
  for (int i = 0; i < num_entries; i++) {
    if (store_entries[i].node_index == init_node_index) {
      init_entry_index = i;
      break;
    }
  }

  if (init_entry_index == -1) {
    return {};
  }

  std::vector<Vec3f> dst_samples(max_num_samples);
  tree::SamplePointsOnInternodesParams sample_params{};
  sample_params.node_aabb = node_aabb;
  sample_params.entries = store_entries.data();
  sample_params.entry_indices = store_entry_indices.data();
  sample_params.num_entries = num_entries;
  sample_params.init_entry_index = init_entry_index;
  sample_params.step_axis = {};
  sample_params.target_step_length = step_size;
  sample_params.max_step_length = 4.0f * step_size;
  sample_params.prefer_entry_up_axis = !target_down;
  sample_params.prefer_entry_down_axis = target_down;
  sample_params.num_samples = max_num_samples;
  sample_params.dst_samples = dst_samples.data();
  sample_params.node_meta = node_meta;
  sample_params.stop_at_leaf = !target_down;
  const int num_samples = tree::sample_points_on_internodes(sample_params);
  dst_samples.resize(num_samples);
  return dst_samples;
}

auto downsample_entries(const tree::SpiralAroundNodesEntry* entries, int num_entries,
                        const OBB3f* node_bounds, int num_nodes, int num_steps) {
  assert(num_steps > 0);
  std::vector<tree::SpiralAroundNodesEntry> result;
  int ei{};
  while (ei < num_entries) {
    result.push_back(entries[ei]);
    auto& p0 = entries[ei].p;

    int dsi = ei + num_steps;
    while (dsi < num_entries && dsi > ei) {
      auto& p1 = entries[dsi].p;
      float t;
      int ni;
      if (ray_internodes_intersect(p0, normalize(p1 - p0), node_bounds, num_nodes, &t, &ni)) {
        --dsi;
      } else {
        break;
      }
    }

    ei = std::max(dsi, ei + 1);
  }

  return result;
}

void decompose_internodes(const tree::Internode* nodes, int num_nodes,
                          OBB3f* bounds, int* medial_children, int* lateral_children, int* parents) {
  tree::internode_obbs(nodes, num_nodes, bounds);
  for (int i = 0; i < num_nodes; i++) {
    medial_children[i] = nodes[i].medial_child;
    if (lateral_children) {
      lateral_children[i] = nodes[i].lateral_child;
    }
    parents[i] = nodes[i].parent;
  }
}

auto compute_spiral_around_nodes(DebugProceduralTreeComponent& component,
                                 const tree::Internode* nodes, int num_nodes,
                                 int init_ni, float theta,
                                 tree::SpiralAroundNodesEntry* query_entry = nullptr,
                                 const Optional<Vec3f>& init_p = NullOpt{}) {
  auto& growth_p = component.growth_on_nodes_params;

  tree::SpiralAroundNodesParams spiral_params{};
  spiral_params.init_ni = init_ni;
  spiral_params.step_size = growth_p.spiral_step_size;
  spiral_params.step_size_randomness = growth_p.spiral_step_size_randomness;
  spiral_params.theta = theta;
  spiral_params.theta_randomness = growth_p.spiral_theta_randomness;
  spiral_params.n_off = growth_p.spiral_n_off;
  spiral_params.randomize_initial_position = growth_p.spiral_randomize_initial_position;
  spiral_params.disable_node_intersect_check = growth_p.spiral_disable_node_intersect_check;
  if (init_p) {
    spiral_params.init_p = init_p.value();
    spiral_params.use_manual_init_p = true;
  }

  Temporary<int, 2048> store_med_children;
  Temporary<int, 2048> store_parents;
  Temporary<OBB3f, 2048> store_bounds;
  int* med_children = store_med_children.require(num_nodes);
  int* parents = store_parents.require(num_nodes);
  OBB3f* node_bounds = store_bounds.require(num_nodes);
  decompose_internodes(nodes, num_nodes, node_bounds, med_children, nullptr, parents);

  const int max_num_entries = 1024;
  std::vector<tree::SpiralAroundNodesEntry> entries(max_num_entries);
  int num_entries = spiral_around_nodes(
    node_bounds, med_children, parents, num_nodes, spiral_params, max_num_entries, entries.data());
  entries.resize(num_entries);

  if (growth_p.spiral_downsample_interval > 0) {
    entries = downsample_entries(
      entries.data(), num_entries, node_bounds, num_nodes, growth_p.spiral_downsample_interval);
    num_entries = int(entries.size());
  }

  std::vector<Vec3f> result(num_entries);
  for (int i = 0; i < num_entries; i++) {
    result[i] = entries[i].p;
  }

  if (query_entry) {
    int desired_entry_index = query_entry->node_index;
    if (desired_entry_index >= 0 && desired_entry_index < num_entries) {
      *query_entry = entries[desired_entry_index];
    } else {
      query_entry->node_index = -1;
    }
  }

  return result;
}

tree::Internode make_line_as_node(const Vec3f& p0, const Vec3f& p1, float radius) {
  tree::Internode result{};
  result.direction = normalize(p1 - p0);
  result.position = p0;
  result.length = (p1 - p0).length();
  result.diameter = radius * 2.0f;
  return result;
}

void update_debug_growth_on_nodes(DebugProceduralTreeComponent& component, const UpdateInfo& info) {
  auto& growth_p = component.growth_on_nodes_params;

  tree::set_global_growth_rate_scale(info.vine_system, growth_p.growth_rate_scale);

  for (auto& ps : growth_p.sample_points) {
    for (int i = 0; i < int(ps.size()); i++) {
      auto& p = ps[i];
      if (growth_p.draw_point_cubes) {
        vk::debug::draw_cube(p, Vec3f{0.0125f}, growth_p.line_color);
      }
      if (i + 1 < int(ps.size())) {
        vk::debug::draw_line(p, ps[i + 1], growth_p.line_color);
      }
    }
  }
//  vk::debug::draw_line(growth_p.source_p, growth_p.target_p, Vec3f{1.0f, 0.0f, 0.0f});

  if (!growth_p.need_recompute) {
    return;
  }

  auto* trees = info.proc_tree_component.maybe_read_trees();
  if (!trees) {
    return;
  }

  if (!info.tree_bounds_accel || !info.radius_limiter) {
    return;
  }

  int ith{};
  bool found_source{};
  tree::TreeInstanceHandle source_instance{};
  for (auto& [_, tree] : *trees) {
    if (ith++ == growth_p.ith_source) {
      source_instance = tree.instance;
      found_source = true;
      break;
    }
  }

  if (!found_source) {
    return;
  }

#if 1
  {
//    const float spiral_theta = pif() * 0.25f;
    const float spiral_theta = growth_p.spiral_theta;
    auto vine_inst = tree::create_vine_instance(info.vine_system, growth_p.vine_radius);
    auto seg_handle = tree::start_new_vine_on_tree(
      info.vine_system, vine_inst, source_instance, spiral_theta);
    tree::VineSystemTryToJumpToNearbyTreeParams jump_params{};
    tree::try_to_jump_to_nearby_tree(info.vine_system, vine_inst, seg_handle, jump_params);
    growth_p.need_recompute = false;
    return;
  }
#endif

  auto inst = tree::read_tree(info.tree_system, source_instance);
  if (!inst.nodes) {
    return;
  }

  Stopwatch stopwatch;

  std::vector<Vec3f> dst_samples;
  tree::SpiralAroundNodesEntry query_entry{};
  query_entry.node_index = growth_p.spiral_branch_entry_index;  //  desire nth entry
  if (growth_p.method == 1) {
    const auto& inodes = inst.nodes->internodes;
    dst_samples = compute_spiral_around_nodes(
      component, inodes.data(), int(inodes.size()),
      growth_p.spiral_init_ni, growth_p.spiral_theta, &query_entry);
  } else {
    dst_samples = compute_growth_on_nodes_sample_points(
      component, inst.nodes->internodes, false, 0);
  }

  const auto num_samples = int(dst_samples.size());

  if (num_samples > 0) {
    const Vec3f last_p = dst_samples[num_samples - 1];
    const float examine_radius = 8.0f;
    auto examine_bounds = OBB3f::axis_aligned(last_p, Vec3f{examine_radius});

    std::vector<const bounds::Element*> bounds_elements;
    info.tree_bounds_accel->intersects(bounds::make_query_element(examine_bounds), bounds_elements);

    Optional<tree::TreeInstanceHandle> closest_leaf_tree_instance;
    Optional<int> closest_leaf_index;
    tree::Internode hit_internode;
    float closest_leaf_distance{infinityf()};

    for (auto& el : bounds_elements) {
      tree::TreeInstanceHandle hit_inst;
      int hit_internode_index;
      bool found_inst = tree::lookup_by_bounds_element_ids(
        info.tree_system,
        bounds::ElementID{el->parent_id},
        bounds::ElementID{el->id},
        &hit_inst, &hit_internode, &hit_internode_index);

      if (found_inst && hit_inst != source_instance && hit_internode.is_leaf()) {
        float dist = (hit_internode.position - last_p).length();
        if (dist < closest_leaf_distance) {
          closest_leaf_tree_instance = hit_inst;
          closest_leaf_index = hit_internode_index;
          closest_leaf_distance = dist;
        }
      }
    }

    Vec3f targ_p{};
    if (closest_leaf_tree_instance) {
      auto closest_inst = tree::read_tree(info.tree_system, closest_leaf_tree_instance.value());
      assert(closest_inst.nodes);
      assert(closest_leaf_index.value() < int(closest_inst.nodes->internodes.size()));
      auto& closest_leaf = closest_inst.nodes->internodes[closest_leaf_index.value()];
      assert(closest_leaf.is_leaf());
      targ_p = closest_leaf.position;
#if 0
      std::vector<const bounds::RadiusLimiterElement*> radius_limiter_elements;
      bounds::gather_intersecting_line(info.radius_limiter, last_p, targ_p, radius_limiter_elements);
#endif

      std::vector<Vec3f> connect_samples;
      std::vector<Vec3f> next_samples;
      if (growth_p.method == 1) {
        const auto& closest_nodes = closest_inst.nodes->internodes;
        auto connect_node = make_line_as_node(dst_samples.back(), targ_p, 0.25f);
        connect_samples = compute_spiral_around_nodes(
          component, &connect_node, 1, 0, growth_p.spiral_theta);

        float down_theta = growth_p.spiral_theta + pif();
        next_samples = compute_spiral_around_nodes(
          component, closest_nodes.data(), int(closest_nodes.size()),
          closest_leaf_index.value(), down_theta);

      } else {
        next_samples = compute_growth_on_nodes_sample_points(
          component, closest_inst.nodes->internodes, true, closest_leaf_index.value());
      }

      dst_samples.insert(dst_samples.end(), connect_samples.begin(), connect_samples.end());
      dst_samples.insert(dst_samples.end(), next_samples.begin(), next_samples.end());
    }

    std::vector<Vec3f> next_branch;
#if 1
    if (growth_p.method == 1 && query_entry.node_index >= 0) {
      const auto& inodes = inst.nodes->internodes;
      float next_theta = growth_p.spiral_branch_theta;
      auto init_p = Optional<Vec3f>(query_entry.p);
      next_branch = compute_spiral_around_nodes(
        component, inodes.data(), int(inodes.size()), query_entry.node_index,
        next_theta, nullptr, init_p);
    }
#endif

    growth_p.sample_points.clear();
    growth_p.sample_points.push_back(std::move(dst_samples));
    if (!next_branch.empty()) {
      growth_p.sample_points.push_back(std::move(next_branch));
    }
    growth_p.source_p = last_p;
    growth_p.target_p = targ_p;
  }

  growth_p.need_recompute = false;
  growth_p.last_compute_time_ms = float(stopwatch.delta().count() * 1e3);
}

struct SpiralAroundNodesQuadVertexTransform {
  Vec3f p;
  Mat3f frame;
};

void extract_spiral_around_nodes_quad_vertex_transforms(
  const tree::SpiralAroundNodesEntry* dst_entries, int num_entries,
  SpiralAroundNodesQuadVertexTransform* tforms) {
  //
  if (num_entries == 0) {
    return;

  } else if (num_entries == 1) {
    tforms[0] = {dst_entries[0].p, Mat3f{1.0f}};

  } else {
    for (int i = 0; i < num_entries - 1; i++) {
      auto up = normalize(dst_entries[i + 1].p - dst_entries[i].p);
      Vec3f zs = dst_entries[i].n;
      float weight{1.0f};
      if (i > 0) {
        zs += dst_entries[i - 1].n * 0.25f;
        weight += 0.25f;
      }
      if (i + 1 < num_entries) {
        zs += dst_entries[i + 1].n * 0.25f;
        weight += 0.25f;
      }
      auto z = zs / weight;
      auto x = normalize(cross(up, z));
      z = normalize(cross(x, up));
      tforms[i] = {dst_entries[i].p, Mat3f{x, up, z}};
    }
    tforms[num_entries - 1] = {dst_entries[num_entries - 1].p, tforms[num_entries - 2].frame};
  }
}

struct SpiralAroundNodes2Params {
  float vel{0.0f};
  float scale{0.25f};
  Vec3f color{1.0f};
  float theta{pif() * 0.25f};
  float n_off{0.1f};
  float taper_frac{1.0f};
  float vel_expo_frac{};
  bool draw_frames{};
  int max_num_medial_lateral_intersect_bounds{};
  bool disable_intersect_check{true};
  float target_segment_length{4.0f};
  int num_points_per_segment{16};
  int num_quad_segments{8};
  float compute_time_ms{};
  float last_adjust_time_ms{};
  float lod_distance{64.0f};
  bool enable_lod{true};
  bool disabled{true};
  bool enable_resource_sys{};
  bool use_resource_sys{};
};

struct DebugSpiralAroundNodesUpdateContext {
  static constexpr int max_num_points_per_segment = 32;

  float t;
  bool tried_initialize;

  int num_points_per_segment;
  SpiralAroundNodesQuadVertexTransform points[max_num_points_per_segment * 2];
  int point_segment0_end;
  int point_segment1_end;

  int next_ni;
  Vec3f next_p;
};

void initialize_spiral_around_nodes_update_context(
  DebugSpiralAroundNodesUpdateContext* context,
  const int* med, const int* lat, const int* par, const OBB3f* bounds,
  int num_internodes, const SpiralAroundNodes2Params& spiral_params) {
  //
  *context = {};

  constexpr int max_num_points = DebugSpiralAroundNodesUpdateContext::max_num_points_per_segment;
  const int num_points = std::min(spiral_params.num_points_per_segment, max_num_points);
  const float target_step_size = spiral_params.target_segment_length / float(num_points);

  context->tried_initialize = true;
  context->num_points_per_segment = num_points;

  for (int s = 0; s < 2; s++) {
    tree::SpiralAroundNodesParams params{};
    params.init_p = context->next_p;
    params.use_manual_init_p = s == 1;
    params.init_ni = s == 1 ? context->next_ni : 0;
    params.n_off = spiral_params.n_off;
    params.theta = spiral_params.theta;
    params.step_size = target_step_size;
    params.max_num_medial_lateral_intersect_bounds = spiral_params.max_num_medial_lateral_intersect_bounds;

    tree::SpiralAroundNodesEntry dst_entries[max_num_points];
    auto res = tree::spiral_around_nodes2(
      bounds, med, lat, par, num_internodes, params, num_points, dst_entries);

    if (res.num_entries < 2) {
      break;
    }

    SpiralAroundNodesQuadVertexTransform tforms[max_num_points];
    extract_spiral_around_nodes_quad_vertex_transforms(dst_entries, res.num_entries, tforms);
    for (int i = 0; i < res.num_entries; i++) {
      assert(context->point_segment1_end < num_points * 2);
      context->points[context->point_segment1_end++] = tforms[i];
    }

    if (s == 0) {
      context->point_segment0_end = context->point_segment1_end;
    }

    context->next_p = res.next_p;
    context->next_ni = res.next_ni;
  }
}

bool tick_t(DebugSpiralAroundNodesUpdateContext* context, double real_dt,
            const SpiralAroundNodes2Params& spiral_params) {
  context->t += spiral_params.vel * float(
    real_dt * (0.25 + spiral_params.vel_expo_frac * (ease::in_out_expo(context->t) * 0.5)));
  bool need_adjust = context->t >= 1.0f;
  while (context->t >= 1.0f) {
    context->t -= 1.0f;
  }
  return need_adjust;
}

void compute_next_spiral_around_nodes_segment(
  DebugSpiralAroundNodesUpdateContext* context,
  const int* med, const int* lat, const int* par, const OBB3f* bounds, int num_internodes,
  const SpiralAroundNodes2Params& spiral_params) {
  //
  std::rotate(
    context->points,
    context->points + context->point_segment0_end,
    context->points + context->point_segment1_end);
  context->point_segment0_end = context->point_segment1_end - context->point_segment0_end;
  context->point_segment1_end = context->point_segment0_end;

  constexpr int max_num_points = DebugSpiralAroundNodesUpdateContext::max_num_points_per_segment;
  const int num_points = context->num_points_per_segment;
  assert(num_points > 0 && num_points <= max_num_points);
  const float target_step_size = spiral_params.target_segment_length / float(num_points);

  tree::SpiralAroundNodesParams params{};
  params.init_p = context->next_p;
  params.use_manual_init_p = true;
  params.init_ni = context->next_ni;
  params.n_off = spiral_params.n_off;
  params.theta = spiral_params.theta;
  params.step_size = target_step_size;
  params.max_num_medial_lateral_intersect_bounds = spiral_params.max_num_medial_lateral_intersect_bounds;
  tree::SpiralAroundNodesEntry dst_entries[max_num_points];
  auto res = tree::spiral_around_nodes2(
    bounds, med, lat, par, num_internodes, params, num_points, dst_entries);

  SpiralAroundNodesQuadVertexTransform tforms[max_num_points];
  extract_spiral_around_nodes_quad_vertex_transforms(dst_entries, res.num_entries, tforms);
  for (int i = 0; i < res.num_entries; i++) {
    assert(context->point_segment1_end < context->num_points_per_segment * 2);
    context->points[context->point_segment1_end++] = tforms[i];
  }

  context->next_ni = res.next_ni;
  context->next_p = res.next_p;

  if (res.reached_axis_end) {
    context->tried_initialize = false;
    context->t = 0.0f;
  }
}

void gen_spiral_around_nodes_quad_vertices(
  const DebugSpiralAroundNodesUpdateContext* context, int num_segments,
  const float* src_verts, float* dst_verts, float taper_frac, float scale) {
  //
  auto apply_tform = [](const Vec3f& p, const SpiralAroundNodesQuadVertexTransform& tform, float s) {
    auto p0 = tform.frame * Vec3f{-s, 0.0f, 0.0f} + tform.p;
    auto p1 = tform.frame * Vec3f{s, 0.0f, 0.0f} + tform.p;
    return lerp(p.x * 0.5f + 0.5f, p0, p1);
  };

  const int num_ps = context->point_segment1_end;
  const float eval_t = context->t;
  int seg1_size = num_ps - context->point_segment0_end;
  float i0f = float(context->point_segment0_end) * eval_t;
  float i1f = float(context->point_segment0_end - 1) + float(seg1_size) * eval_t;

  for (int i = 0; i < num_segments * 6; i++) {
    Vec3f p{src_verts[i * 3], src_verts[i * 3 + 1], src_verts[i * 3 + 2]};

    float i0_base = std::max(lerp(p.y, i0f, i1f), 0.0f);
    float i0_t = i0_base - std::floor(i0_base);

    int i0 = clamp(int(i0_base), 0, num_ps - 1);
    int i1 = clamp(i0 + 1, 0, num_ps - 1);
    auto& tform0 = context->points[i0];
    auto& tform1 = context->points[i1];

    float s = scale * 0.125f * lerp(
      taper_frac, 1.0f, (1.0f - std::pow(std::abs(p.y * 2.0f - 1.0f), 2.0f)));
    auto p0 = apply_tform(p, tform0, s);
    auto p1 = apply_tform(p, tform1, s);
    p = lerp(i0_t, p0, p1);

    for (int j = 0; j < 3; j++) {
      dst_verts[i * 3 + j] = p[j];
    }
  }
}

void update_spiral_around_nodes(
  DebugProceduralTreeComponent&, const SpiralAroundNodes2Params& spiral_params,
  const tree::TreeNodeStore* tree_nodes, DebugSpiralAroundNodesUpdateContext* context,
  const UpdateInfo& info) {
  //
  if (!context->tried_initialize) {
    const int num_nodes = int(tree_nodes->internodes.size());
    Temporary<int, 1024> store_lat_children;
    Temporary<int, 1024> store_med_children;
    Temporary<int, 1024> store_parents;
    Temporary<OBB3f, 1024> store_bounds;

    auto* lat = store_lat_children.require(num_nodes);
    auto* med = store_med_children.require(num_nodes);
    auto* par = store_parents.require(num_nodes);
    auto* bounds = store_bounds.require(num_nodes);
    decompose_internodes(tree_nodes->internodes.data(), num_nodes, bounds, med, lat, par);

    initialize_spiral_around_nodes_update_context(
      context, med, lat, par, bounds, num_nodes, spiral_params);
  }

  if (context->point_segment0_end >= context->point_segment1_end ||
      context->next_ni >= int(tree_nodes->internodes.size())) {
    context->tried_initialize = false;
    return;
  }

  const bool need_adjust = tick_t(context, info.real_dt, spiral_params);
  if (need_adjust) {
    const int num_nodes = int(tree_nodes->internodes.size());
    Temporary<int, 1024> store_lat_children;
    Temporary<int, 1024> store_med_children;
    Temporary<int, 1024> store_parents;
    Temporary<OBB3f, 1024> store_bounds;

    auto* lat = store_lat_children.require(num_nodes);
    auto* med = store_med_children.require(num_nodes);
    auto* par = store_parents.require(num_nodes);
    auto* bounds = store_bounds.require(num_nodes);
    decompose_internodes(tree_nodes->internodes.data(), num_nodes, bounds, med, lat, par);

    compute_next_spiral_around_nodes_segment(
      context, med, lat, par, bounds, num_nodes, spiral_params);

//    spiral_params.last_adjust_time_ms = float(sw.delta().count() * 1e3);
  }

  const int num_ps = context->point_segment1_end;
  if (context->point_segment0_end < num_ps) {
    constexpr int max_num_segments = 32;
    float src_verts[max_num_segments * 6 * 3];

    const int num_segments = std::min(max_num_segments, spiral_params.num_quad_segments);
    geometry::get_segmented_quad_positions(num_segments, true, src_verts);

    float dst_verts[max_num_segments * 6 * 3];
    gen_spiral_around_nodes_quad_vertices(
      context, num_segments, src_verts, dst_verts,
      spiral_params.taper_frac, spiral_params.scale);

//    spiral_params.compute_time_ms = float(sw.delta().count() * 1e3);

    vk::debug::draw_two_sided_triangles(
      (const Vec3f*) dst_verts, num_segments * 6, spiral_params.color);
  }

  if (spiral_params.draw_frames) {
    for (int i = 0; i < num_ps; i++) {
      auto& tform = context->points[i];
      float l = 0.1f;
      vk::debug::draw_line(tform.p, tform.p + tform.frame[0] * l, Vec3f{1.0f, 0.0f, 0.0f});
      vk::debug::draw_line(tform.p, tform.p + tform.frame[1] * l, Vec3f{0.0f, 1.0f, 0.0f});
      vk::debug::draw_line(tform.p, tform.p + tform.frame[2] * l, Vec3f{0.0f, 0.0f, 1.0f});
    }
  }
}

void update_debug_spiral_around_nodes3(DebugProceduralTreeComponent& component,
                                       SpiralAroundNodes2Params& spiral_params,
                                       const UpdateInfo& info) {
  struct UpdateContext {
    std::vector<DebugSpiralAroundNodesUpdateContext> contexts;
    bool enabled_resource_sys;
  };
  static UpdateContext context{};

  auto* trees = info.proc_tree_component.maybe_read_trees();
  if (!trees || spiral_params.disabled) {
    return;
  }

  bool need_create_resource_insts{};
  if (spiral_params.enable_resource_sys && !context.enabled_resource_sys) {
    need_create_resource_insts = true;
    context.enabled_resource_sys = true;
  }

  Stopwatch sw;

  if (spiral_params.use_resource_sys && !need_create_resource_insts) {
    //
  } else {
    int ind{};
    for (auto& [_, tree]: *trees) {
      auto tree0 = tree::read_tree(info.tree_system, tree.instance);
      if (!tree0.nodes) {
        continue;
      }

      auto ori = tree0.nodes->origin();
      auto cam_dist = (info.camera.get_position() - ori).length();
      bool high_lod = cam_dist < spiral_params.lod_distance;

      for (int i = 0; i < 4; i++) {
        while (ind >= int(context.contexts.size())) {
          context.contexts.emplace_back();
        }
        auto& ctx = context.contexts[ind];
        auto p = spiral_params;
        p.theta += float(i) * pif() * 0.1f;
        if (spiral_params.enable_lod && !high_lod) {
          p.num_quad_segments = 4;
        }
        update_spiral_around_nodes(component, p, tree0.nodes, &ctx, info);
        ind++;

        if (need_create_resource_insts) {
          auto* sys = tree::get_global_resource_spiral_around_nodes_system();
          tree::CreateResourceSpiralParams create_params{};
          create_params.theta_offset = float(i) * pif() * 0.1f;
          create_params.scale = 0.25f;
          tree::create_resource_spiral_around_tree(sys, tree.instance, create_params);
        }
      }
    }
  }

  spiral_params.compute_time_ms = float(sw.delta().count() * 1e3);
}

void update_debug_render_branch_nodes(DebugProceduralTreeComponent& component,
                                      const UpdateInfo& info) {
  if (component.disable_debug_branch_node_drawable_components) {
    return;
  }

  auto* trees = info.proc_tree_component.maybe_read_trees();
  if (!trees) {
    return;
  }

  auto& drawables = component.debug_branch_node_drawable_components;
  for (auto& [tree_id, tree] : *trees) {
    if (drawables.count(tree_id) == 0) {
      drawables[tree_id] = {};
    }

    auto inst = tree::read_tree(info.tree_system, tree.instance);
    if (!inst.nodes) {
      continue;
    }

    auto& inodes = inst.nodes->internodes;
    auto& components = drawables.at(tree_id);
    if (!components.wind_drawable && !components.base_drawable) {
      auto axis_roots = tree::compute_axis_root_info(inodes);
      auto remapped_roots = tree::remap_axis_roots(inodes);
      components = tree::create_wind_branch_node_drawable_components_from_internodes(
        info.render_branch_nodes_data, inodes, *inst.src_aabb, axis_roots, remapped_roots);
    }

    if (inst.events.node_render_position_modified) {
      tree::set_position_and_radii_from_internodes(info.render_branch_nodes_data, components, inodes);
    }
  }
}

void set_gpu_driven_foliage_preset1(DebugProceduralTreeComponent& component) {
  component.foliage_occlusion_only_fade_back_in_below_distance_threshold = true;
  component.foliage_lod_cull_distance_threshold = 100.0f;
  component.foliage_cull_fade_back_in_distance_threshold = 32.0f;
  component.foliage_hidden = true;
  component.set_foliage_occlusion_check_fade_in_out = true;
  component.disable_experimental_foliage_drawable_creation = true;
  component.allow_multiple_foliage_param_types = true;
}

Vec3<uint8_t> to_uint8_3(const Vec3f& c) {
  using u8 = uint8_t;
  auto resf = clamp_each(c, Vec3f{}, Vec3f{1.0f}) * 255.0f;
  return Vec3<uint8_t>{u8(resf.x), u8(resf.y), u8(resf.z)};
}

[[maybe_unused]] auto create_debug_curved_plane_drawables(DebugProceduralTreeComponent&) {
  struct Result {
    foliage::OrnamentalFoliageInstanceHandle instances;
  };

  Result result{};

  {
    foliage::OrnamentalFoliageInstanceGroupDescriptor group_desc{};
    group_desc.material_type = foliage::OrnamentalFoliageMaterialType::Material1;
    group_desc.geometry_type = foliage::OrnamentalFoliageGeometryType::CurvedPlane;
    group_desc.wind_type = foliage::OrnamentalFoliageWindType::OnPlantStem;

    foliage::OrnamentalFoliageInstanceDescriptor desc{};
    desc.translation = Vec3f{16.0f};
    desc.orientation = normalize(Vec3f{1.0f, 1.0f, 0.0f});
    desc.material.material1.texture_layer_index = 1;  //  @TODO
    desc.material.material1.color0 = Vec3<uint8_t>{255, 0, 255};
    desc.material.material1.color1 = Vec3<uint8_t>{255, 255, 0};
    desc.material.material1.color2 = Vec3<uint8_t>{255, 255, 0};
    desc.material.material1.color3 = Vec3<uint8_t>{255, 0, 255};
    desc.geometry_descriptor.curved_plane.min_radius = 0.01f;
    desc.geometry_descriptor.curved_plane.curl_scale = 0.0f;
    desc.geometry_descriptor.curved_plane.radius_power = 0.5f;
    desc.geometry_descriptor.curved_plane.radius = 1.0f;
    desc.wind_data.on_plant_stem.world_origin_xz = Vec2f{desc.translation.x, desc.translation.z};
    desc.wind_data.on_plant_stem.tip_y_fraction = 0.0f;

    auto instances = foliage::create_ornamental_foliage_instances(
      foliage::get_global_ornamental_foliage_data(), group_desc, &desc, 1);
    (void) instances;
  }
#if 0
  {
    Vec3f base_color0{0.145f, 0.71f, 0.155f};
    Vec3f base_color1{};
    Vec3f base_color2{0.681f, 0.116f, 0.0f};
    Vec3f base_color3{0.0f, 0.623f, 0.0f};

    auto rand_color = [](const Vec3f& c, float s) {
      return to_uint8_3(c + c * Vec3f{urand_11f(), urand_11f(), urand_11f()} * s);
    };

    foliage::OrnamentalFoliageInstanceGroupDescriptor group_desc{};
    group_desc.material_type = foliage::OrnamentalFoliageMaterialType::Material2;
    group_desc.geometry_type = foliage::OrnamentalFoliageGeometryType::FlatPlane;
    group_desc.wind_type = foliage::OrnamentalFoliageWindType::OnPlantStem;

    constexpr int num_descs = 256;
    foliage::OrnamentalFoliageInstanceDescriptor descs[num_descs]{};
    for (int i = 0; i < num_descs; i++) {
      foliage::OrnamentalFoliageInstanceDescriptor desc{};
      desc.translation = Vec3f{16.0f} + Vec3f{urand_11f(), 0.0f, urand_11f()} * 16.0f;
      desc.orientation = normalize(Vec3f{1.0f, 1.0f, 0.0f});
      desc.material.material2.texture_layer_index = 1;  //  @TODO

      desc.material.material2.color0 = rand_color(base_color0, 0.1f);
      desc.material.material2.color1 = rand_color(base_color1, 0.1f);
      desc.material.material2.color2 = rand_color(base_color2, 0.1f);
      desc.material.material2.color3 = rand_color(base_color3, 0.1f);

      desc.geometry_descriptor.flat_plane.aspect = 1.0f;
      desc.geometry_descriptor.flat_plane.scale = 1.0f;
      desc.wind_data.on_plant_stem.world_origin_xz = Vec2f{desc.translation.x, desc.translation.z};
      desc.wind_data.on_plant_stem.tip_y_fraction = 0.0f;
      descs[i] = desc;
    }

    result.instances = foliage::create_ornamental_foliage_instances(
      foliage::get_global_ornamental_foliage_data(), group_desc, descs, num_descs);
  }
#endif
  {
    foliage::OrnamentalFoliageInstanceGroupDescriptor group_desc{};
    group_desc.material_type = foliage::OrnamentalFoliageMaterialType::Material1;
    group_desc.geometry_type = foliage::OrnamentalFoliageGeometryType::CurvedPlane;
    group_desc.wind_type = foliage::OrnamentalFoliageWindType::OnPlantStem;

    foliage::OrnamentalFoliageInstanceDescriptor desc{};
    desc.translation = Vec3f{16.0f} - Vec3f{1.0f, 0.0f, 0.0f};
    desc.orientation = normalize(Vec3f{1.0f, 1.0f, 0.0f});
    desc.material.material1.texture_layer_index = 0;  //  @TODO
    desc.material.material1.color0 = Vec3<uint8_t>{255, 255, 255};
    desc.material.material1.color1 = Vec3<uint8_t>{255, 255, 0};
    desc.material.material1.color2 = Vec3<uint8_t>{255, 255, 0};
    desc.material.material1.color3 = Vec3<uint8_t>{255, 0, 255};
    desc.geometry_descriptor.curved_plane.min_radius = 0.01f;
    desc.geometry_descriptor.curved_plane.curl_scale = 0.0f;
    desc.geometry_descriptor.curved_plane.radius_power = 0.5f;
    desc.geometry_descriptor.curved_plane.radius = 1.0f;
    desc.wind_data.on_plant_stem.world_origin_xz = Vec2f{desc.translation.x, desc.translation.z};
    desc.wind_data.on_plant_stem.tip_y_fraction = 0.0f;

    auto instances = foliage::create_ornamental_foliage_instances(
      foliage::get_global_ornamental_foliage_data(), group_desc, &desc, 1);
    (void) instances;
  }

  return result;
}

void update_debug_ornamental_foliage_instances(
  foliage::OrnamentalFoliageInstanceHandle instances,
  const Vec3f& color0, const Vec3f& color1,
  const Vec3f& color2, const Vec3f& color3) {
  //
  (void) instances;
  auto to_u8 = [](const Vec3f& c) { return to_uint8_3(c); };
  foliage::set_global_ornamental_foliage_material2_colors(
    foliage::get_global_ornamental_foliage_data(),
    to_u8(color0), to_u8(color1), to_u8(color2), to_u8(color3));
}

struct {
  foliage::OrnamentalFoliageInstanceHandle debug_foliage_instances{};
  Vec3f debug_foliage_color0{0.145f, 0.71f, 0.155f};
  Vec3f debug_foliage_color1{};
  Vec3f debug_foliage_color2{0.681f, 0.116f, 0.0f};
  Vec3f debug_foliage_color3{0.246f, 0.449f, 0.0f};
  SpiralAroundNodes2Params spiral_around_nodes_2_params;
} globals;

} //  anon

DebugProceduralTreeComponent::~DebugProceduralTreeComponent() {
  foliage_occlusion::destroy_foliage_occlusion_system(&debug_foliage_lod_system);
}

void DebugProceduralTreeComponent::initialize(const InitInfo&) {
  debug_foliage_instance_params = make_tighter_foliage_instance_params(false);
  foliage_distribution_strategy = foliage::FoliageDistributionStrategy::TightHighN;
#if 1
  debug_foliage_lod_system = foliage_occlusion::create_foliage_occlusion_system();
//  debug_foliage_density_system = foliage_density::create_foliage_density_system();
#endif

#if 0
  disable_auto_foliage_drawable_creation = false;
  #if 0
  enable_debug_foliage_drawable_creation = true;
  enable_foliage_drawable_component_creation = false;
  #else
  enable_debug_foliage_drawable_creation = false;
  enable_foliage_drawable_component_creation = true;
  #endif
#endif

#if 1
  set_gpu_driven_foliage_preset1(*this);
#endif
  disable_debug_branch_node_drawable_components = true;

#if 0
  auto create_res = create_debug_curved_plane_drawables(*this);
  globals.debug_foliage_instances = create_res.instances;
#endif
}

DebugProceduralTreeComponent::UpdateResult
DebugProceduralTreeComponent::update(const UpdateInfo& info) {
  UpdateResult result{};

  if (set_tree_leaves_renderer_enabled) {
    result.set_tree_leaves_renderer_enabled = set_tree_leaves_renderer_enabled.value();
    set_tree_leaves_renderer_enabled = NullOpt{};
  }

#if DEBUG_GROW
  const float growth_incr = 0.05f;
  const auto spawn_p = make_thin_spawn_params(10.0f);
  for (int i = 0; i < int(debug_trees.size()); i++) {
    auto& tree = debug_trees[i];
    auto& branch_handle = tree.branch_drawable;
    if (auto it = render_growth_contexts.find(tree.nodes.id); it != render_growth_contexts.end()) {
      if (update_render_growth(tree.nodes, spawn_p, it->second, growth_incr)) {
        apply_render_growth_change(tree.nodes, spawn_p, 0);
        info.proc_tree_renderer.set_dynamic_data(
          info.set_data_context, branch_handle, tree.nodes.internodes);
      } else {
        render_growth_contexts.erase(it);
      }
    } else {
      render_growth_contexts[tree.nodes.id] = initialize_axis_render_growth(tree.nodes);
    }
  }
#endif

  update_foliage_occlusion_system(*this, info);
  update_debug_frustum_cull(*this, info);
  if (debug_grid_traverse_enabled) {
    update_debug_grid_traverse(*this);
  }

  update_debug_growth_on_nodes(*this, info);
//  update_debug_ray_cylinder_intersect(info);

  if (debug_foliage_lod_system) {
    //  @NOTE: Always update occlusion system to keep cpu and gpu data in sync, even if
    //  not updating cluster visibility.
    auto res = foliage_occlusion::update_foliage_occlusion_system(debug_foliage_lod_system);
    result.occlusion_system_data_structure_modified = res.data_structure_modified;
    result.occlusion_system_clusters_modified = res.clusters_modified;
  }

  update_debug_render_branch_nodes(*this, info);
//  update_debug_spiral_around_nodes2(*this, info);
  update_debug_spiral_around_nodes3(*this, globals.spiral_around_nodes_2_params, info);

#if 1
  tree::debug::update_fit_node_aabbs({
    info.proc_tree_component,
    info.tree_system,
    info.roots_system,
    info.camera
  });
#endif

#if 1
  tree::update_debug_health({
    info.proc_tree_component,
    info.resource_spiral_sys
  });
#endif

  return result;
}

#define SET_MODIFIED(cond) if ((cond)) { \
  modified = true;                       \
}

void DebugProceduralTreeComponent::render_gui(
  const tree::VineSystem* vine_system, ProceduralTreeComponent& comp) {
  //
  tree::render_debug_health_gui();

  ImGui::Begin("DebugProceduralTreeGUI");

  constexpr auto enter_flag = ImGuiInputTextFlags_EnterReturnsTrue;

  if (ImGui::Button("CreateDemoTrees")) {
    comp.create_tree_patches();
  }

  if (ImGui::Button("SetGPUDrivenPreset1")) {
    set_gpu_driven_foliage_preset1(*this);
  }

  if (ImGui::Button("ToggleRenderOptimized")) {
    if (foliage_hidden) {
      foliage_hidden = false;
      set_tree_leaves_renderer_enabled = false;
    } else {
      foliage_hidden = true;
      set_tree_leaves_renderer_enabled = true;
    }
  }

  ImGui::Checkbox("DisableDebugBranchNodes", &disable_debug_branch_node_drawable_components);

  if (debug_foliage_lod_system && ImGui::TreeNode("FoliageOcclusion")) {
    const uint32_t num_inst = foliage_occlusion::total_num_instances(debug_foliage_lod_system);
//    const uint32_t num_newly_occ = latest_occlusion_check_result.num_newly_occluded;
    const uint32_t total_num_occ = latest_occlusion_check_result.total_num_occluded;
    float occlude_frac = clamp01(float(total_num_occ) / float(num_inst));
    float occlude_test_frac = clamp01(
      float(total_num_occ) / float(latest_occlusion_check_result.num_passed_frustum_cull));

    if (ImGui::TreeNode("Stats")) {
      auto stats = foliage_occlusion::get_foliage_occlusion_system_stats(debug_foliage_lod_system);
      ImGui::Text("NumClusters: %d", int(stats.num_clusters));
      ImGui::Text("NumGridLists: %d", int(stats.num_grid_lists));
      ImGui::TreePop();
    }

    ImGui::Text("Total: %d", int(num_inst));
    ImGui::Text("NumOccluded: %d", int(total_num_occ));
    ImGui::Text("NumTested: %d", int(latest_occlusion_check_result.num_newly_tested));
    ImGui::Text("%d%% Occluded", int(occlude_frac * 100.0f));
    ImGui::Text("%d%% Occluded out of frustum culled", int(occlude_test_frac * 100.0f));
    ImGui::Text("Time: %0.3f ms", latest_occlusion_check_result.ms);
    ImGui::Checkbox("DebugDraw", &debug_draw_foliage_lod_system);
    ImGui::InputInt("MaxNumSteps", &max_num_foliage_occlusion_steps, 1, 100, enter_flag);
    ImGui::InputInt("ClusterCreateInterval", &foliage_occlusion_cluster_create_interval);
    ImGui::InputFloat("CullDistanceThreshold", &foliage_lod_cull_distance_threshold);
    ImGui::InputFloat("FadeBackInDistanceThreshold", &foliage_cull_fade_back_in_distance_threshold);
    ImGui::Checkbox(
      "FadeInOnlyWhenBelowDistanceThreshold",
      &foliage_occlusion_only_fade_back_in_below_distance_threshold);
    ImGui::SliderFloat("MinIntersectAreaFraction", &foliage_min_intersect_area_fraction, 0.0f, 1.0f);
    ImGui::SliderFloat("TestedInstanceScale", &foliage_tested_instance_scale, 0.0f, 4.0f);
    if (ImGui::SmallButton("ResetTestedInstanceScale")) {
      foliage_tested_instance_scale = 1.0f;
    }
    ImGui::Checkbox("ContinuousCheck", &continuously_check_occlusion);
    ImGui::Checkbox("DrawOccluded", &draw_occluded_instances);
    ImGui::Checkbox("DrawClusterBounds", &draw_cluster_bounds);
    ImGui::Checkbox("ColorizeInstances", &colorize_cluster_instances);
    ImGui::SliderInt("UpdateInterval", &occlusion_system_update_interval, 1, 8);
    bool use_fade_in_out = foliage_occlusion_check_fade_in_out;
    if (ImGui::Checkbox("UseFadeInOut", &use_fade_in_out)) {
      set_foliage_occlusion_check_fade_in_out = use_fade_in_out;
    }
    ImGui::Checkbox("DisableCPUCheck", &foliage_occlusion_disable_cpu_check);
    ImGui::SliderFloat("FadeInTimeScale", &occlusion_fade_in_time_scale, 0.0f, 2.0f);
    ImGui::SliderFloat("FadeOutTimeScale", &occlusion_fade_out_time_scale, 0.0f, 2.0f);
    ImGui::SliderFloat("CullTimeScale", &occlusion_cull_time_scale, 0.0f, 2.0f);
    if (ImGui::Button("CheckOcclusion")) {
      need_check_foliage_lod_system_occlusion = true;
    }
    if (ImGui::Button("ClearCulled")) {
      need_clear_foliage_lod_system_culled = true;
    }

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("FrustumCull")) {
    ImGui::Text(cube_visible ? "CubeVisible" : "CubeNotVisible");
    ImGui::Checkbox("UpdateFrustum", &update_debug_frustum);
    ImGui::InputFloat("FarPlane", &far_plane_distance);
    ImGui::InputFloat3("CubePosition", &cube_position.x);
    ImGui::InputFloat3("CubeSize", &cube_size.x);
    ImGui::Checkbox("DebugDraw", &draw_debug_frustum_components);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("GridTraverse")) {
    ImGui::Checkbox("Enabled", &debug_grid_traverse_enabled);
    ImGui::InputFloat3("GridDim", &grid_traverse_grid_dim.x);
    ImGui::InputFloat3("RayOrigin", &grid_traverse_ray_origin.x);
    ImGui::InputFloat3("RayDirection", &grid_traverse_ray_direction.x);
    ImGui::InputInt("NumGridSteps", &num_grid_steps);
    if (ImGui::Button("RandomizeDirection")) {
      grid_traverse_ray_direction = Vec3f{urand_11f(), urand_11f(), urand_11f()};
      grid_traverse_ray_direction = normalize(grid_traverse_ray_direction);
    }
    if (ImGui::Button("NormalizeDirection")) {
      grid_traverse_ray_direction = normalize(grid_traverse_ray_direction);
    }
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Renderer")) {
    ImGui::Checkbox("RenderOptimized", &render_optimized_foliage);
    ImGui::Checkbox(
      "DisableExperimentalFoliageDrawables", &disable_experimental_foliage_drawable_creation);

    auto curr_fadeout_dist = optim_fadeout_distances;
    ImGui::InputFloat2("FadeoutDistances", &optim_fadeout_distances.x, "%0.2f", enter_flag);
    if (ImGui::SmallButton("SetAltFadeoutDistances")) {
      optim_fadeout_distances = Vec2f{170.0f, 180.0f};
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("SetDefaultFadeoutDistances")) {
      optim_fadeout_distances = Vec2f{115.0f, 125.0f};
    }
    if (curr_fadeout_dist != optim_fadeout_distances) {
      need_set_leaves_renderer_fadeout_distances = true;
    }
    if (ImGui::InputFloat2("LODDistances", &optim_lod_distances.x, "%0.2f", enter_flag)) {
      need_set_leaves_renderer_lod_distances = true;
    }
    ImGui::InputFloat("FarPlane", &renderer_far_plane_distance);
    ImGui::Checkbox("DistanceSort", &renderer_distance_sort);
    ImGui::Checkbox("DisableFrustumCull", &renderer_disable_frustum_cull);
    ImGui::Checkbox("DisableOptim", &renderer_disable_optim_update);
    ImGui::Checkbox("DisableInstanceUpdate", &disable_renderer_instance_update);
    ImGui::Checkbox("EnableOcclusionSystem", &renderer_enable_occlusion_system_culling);
    ImGui::Checkbox("EnableDensitySystem", &renderer_enable_density_system_culling);
    ImGui::Checkbox("DensitySystemFadesInOut", &renderer_enable_density_system_fade_in_out);
    ImGui::Checkbox("ShadowDisabled", &foliage_shadow_disabled);
    ImGui::Checkbox("UseIndexBuffer", &renderer_use_index_buffer);
    ImGui::SliderFloat("ShadowScale", &renderer_shadow_scale, 0.0f, 4.0f);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Foliage")) {
    if (ImGui::Button("HideOrigInstances")) {
      set_foliage_instances_hidden = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("ShowOrigInstances")) {
      set_foliage_instances_hidden = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("ShowOrigInstancesOnly")) {
      set_foliage_instances_hidden = false;
      set_render_foliage_system_instances_hidden = true;
    }

    if (ImGui::Button("HideRFSInstances")) {
      set_render_foliage_system_instances_hidden = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("ShowRFSInstances")) {
      set_render_foliage_system_instances_hidden = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("ShowRFSInstancesOnly")) {
      set_render_foliage_system_instances_hidden = false;
      set_foliage_instances_hidden = true;
    }

    ImGui::Checkbox("DisableUpdate", &disable_foliage_update);
    ImGui::Checkbox("DisableAutoCreate", &disable_auto_foliage_drawable_creation);
    ImGui::Checkbox("EnableDebugInstances", &enable_debug_foliage_drawable_creation);
    ImGui::Checkbox("EnableFoliageDrawableComponents", &enable_foliage_drawable_component_creation);
    if (ImGui::InputInt("FoliageImageIndex", &foliage_leaf_image_index)) {
      need_update_foliage_alpha_test_image = true;
    }
    if (ImGui::InputInt("ColorImageIndex", &foliage_hemisphere_color_image_index)) {
      need_update_foliage_color_image = true;
    }

    auto& foliage_params = debug_foliage_instance_params;
    bool modified{};

    SET_MODIFIED(ImGui::InputInt("N", &foliage_params.n))
    SET_MODIFIED(ImGui::InputFloat("TranslationLogMinX", &foliage_params.translation_log_min_x))
    SET_MODIFIED(ImGui::InputFloat("TranslationLogMaxX", &foliage_params.translation_log_max_x))
    SET_MODIFIED(ImGui::InputFloat("TranslationXScale", &foliage_params.translation_x_scale))
    SET_MODIFIED(ImGui::InputFloat("TranslationYScale", &foliage_params.translation_y_scale))
    SET_MODIFIED(ImGui::InputFloat("TranslationStepPower", &foliage_params.translation_step_power))
    SET_MODIFIED(ImGui::InputFloat("TranslationStepSpreadScale", &foliage_params.translation_step_spread_scale))
    SET_MODIFIED(ImGui::InputFloat("RandZRotationScale", &foliage_params.rand_z_rotation_scale))
    SET_MODIFIED(ImGui::InputFloat("CurlScale", &foliage_params.curl_scale))
    SET_MODIFIED(ImGui::InputFloat("GlobalScale", &foliage_params.global_scale))
    SET_MODIFIED(ImGui::Checkbox("OnlyOneInstance", &foliage_params.only_one_instance))

    ImGui::Checkbox("ManualOverrideLeafScale", &override_renderer_leaf_scale);

    if (override_renderer_leaf_scale) {
      if (ImGui::SliderFloat("ScaleFraction", &renderer_leaf_scale_fraction, 0.0f, 1.0f)) {
        need_set_renderer_leaf_scale_fraction = true;
      }
    }

    if (ImGui::SmallButton("MakeWideSpreadOutHi")) {
      foliage_params = make_wide_spread_out_foliage_instance_params();
      need_remake_foliage_drawables = true;
    }
    if (ImGui::SmallButton("MakeTighterFoliageParamsLo")) {
      foliage_params = make_tighter_foliage_instance_params(true);
      foliage_distribution_strategy = foliage::FoliageDistributionStrategy::TightLowN;
      need_remake_foliage_drawables = true;
    }
    if (ImGui::SmallButton("MakeTighterFoliageParamsHi")) {
      foliage_params = make_tighter_foliage_instance_params(false);
      foliage_distribution_strategy = foliage::FoliageDistributionStrategy::TightHighN;
      need_remake_foliage_drawables = true;
    }
    if (ImGui::SmallButton("MakeHangingFoliageParams")) {
      foliage_params = make_hanging_foliage_instance_params();
      foliage_distribution_strategy = foliage::FoliageDistributionStrategy::Hanging;
      need_remake_foliage_drawables = true;
    }
    if (ImGui::SmallButton("MakeThinLongFoliageParams0")) {
      foliage_params = make_thin_long_foliage_instance_params(false);
      need_remake_foliage_drawables = true;
    }
    if (ImGui::SmallButton("MakeThinLongFoliageParams1")) {
      foliage_params = make_thin_long_foliage_instance_params(true);
      need_remake_foliage_drawables = true;
    }
    if (ImGui::SmallButton("MakeThinLongFoliageParams2")) {
      foliage_params = make_thin_foliage_instance_params();
      need_remake_foliage_drawables = true;
    }
    if (ImGui::SmallButton("MakeFloofyFoliageParams")) {
      foliage_params = make_floofy_instance_params();
      need_remake_foliage_drawables = true;
    }
    bool explicitly_requested_to_remake_drawables{};
    if (ImGui::Button("RemakeFoliageDrawables")) {
      need_remake_foliage_drawables = true;
      explicitly_requested_to_remake_drawables = true;
    }
    if (ImGui::Button("RandomizeAlphaTestImages")) {
      need_randomize_foliage_alpha_test_image = true;
    }
    if (ImGui::Button("RandomizeColorImages")) {
      need_randomize_foliage_color = true;
    }

    ImGui::SliderFloat("WindStrengthScale", &wind_strength_scale, 0.0f, 2.0f);
    ImGui::Checkbox("WindDisabled", &wind_disabled);
    ImGui::Checkbox("Hidden", &foliage_hidden);
    ImGui::Checkbox("DisableAlphaTest", &foliage_alpha_test_disabled);
    ImGui::Checkbox("AllowMultipleFoliageTypes", &allow_multiple_foliage_param_types);
    if (modified) {
      need_remake_foliage_drawables = true;
    }

    if (need_remake_foliage_drawables) {
      if (!explicitly_requested_to_remake_drawables && allow_multiple_foliage_param_types) {
        need_remake_foliage_drawables = false;
      }
    }

    ImGui::TreePop();
  }

  bool need_update_orn_foliage_insts{};

  if (ImGui::TreeNode("GrowthOnNodes")) {
    auto& growth_p = growth_on_nodes_params;

    if (ImGui::Button("MakeDarker")) {
      globals.debug_foliage_color0 = Vec3f{0.145f, 0.028f, 0.07f};
      globals.debug_foliage_color1 = {};
      globals.debug_foliage_color2 = {};
      globals.debug_foliage_color3 = Vec3f{0.394f, 0.449f, 0.0f};

      tree::set_render_vines_color(Vec3f{0.07f, 0.056f, 0.0f});

      need_update_orn_foliage_insts = true;
    }

    int num_sample_ps{};
    for (auto& ps : growth_p.sample_points) {
      num_sample_ps += int(ps.size());
    }

    ImGui::Text("Last compute time: %0.3fms", growth_p.last_compute_time_ms);
    ImGui::Text("NumSamplePs: %d", num_sample_ps);

    if (ImGui::TreeNode("VineSystemStats")) {
      auto stats = tree::get_stats(vine_system);
      ImGui::Text("NumInstances: %d", stats.num_instances);
      ImGui::Text("NumSegments: %d", stats.num_segments);
      ImGui::Text("NumNodes: %d", stats.num_nodes);
      ImGui::TreePop();
    }

    if (ImGui::Button("Recompute")) {
      growth_p.need_recompute = true;
    }
    if (ImGui::Button("RemakeVinePrograms")) {
      tree::set_render_vines_need_remake_programs();
    }
    if (ImGui::InputInt("IthSource", &growth_p.ith_source)) {
      growth_p.ith_source = std::max(0, growth_p.ith_source);
      growth_p.need_recompute = true;
    }
    ImGui::InputInt("Method", &growth_p.method);
    if (ImGui::InputInt("SpiralInitNodeIndex", &growth_p.spiral_init_ni)) {
      growth_p.spiral_init_ni = std::max(0, growth_p.spiral_init_ni);
      growth_p.need_recompute = true;
    }
    ImGui::InputFloat("SpiralStepSize", &growth_p.spiral_step_size);
    ImGui::SliderFloat("SpiralStepSizeRandomness", &growth_p.spiral_step_size_randomness, 0.0f, 1.0f);
    ImGui::InputFloat("SpiralNOff", &growth_p.spiral_n_off);
    if (ImGui::SliderFloat("SpiralTheta", &growth_p.spiral_theta, 0.0f, 2.0f * pif())) {
      growth_p.need_recompute = true;
    }
    if (ImGui::SliderFloat("SpiralBranchTheta", &growth_p.spiral_branch_theta, 0.0f, 2.0f * pif())) {
      growth_p.need_recompute = true;
    }
    if (ImGui::InputInt("SpiralBranchEntryIndex", &growth_p.spiral_branch_entry_index)) {
      growth_p.spiral_branch_entry_index = std::max(0, growth_p.spiral_branch_entry_index);
      growth_p.need_recompute = true;
    }
    if (ImGui::InputInt("SpiralDownsampleInterval", &growth_p.spiral_downsample_interval)) {
      growth_p.spiral_downsample_interval = std::max(0, growth_p.spiral_downsample_interval);
      growth_p.need_recompute = true;
    }
    ImGui::SliderFloat("SpiralThetaRandomness", &growth_p.spiral_theta_randomness, 0.0f, 1.0f);
    ImGui::Checkbox("SpiralRandomInitialPosition", &growth_p.spiral_randomize_initial_position);
    if (ImGui::Checkbox("SpiralDisableNodeIntersectCheck", &growth_p.spiral_disable_node_intersect_check)) {
      growth_p.need_recompute = true;
    }
    ImGui::SliderFloat("GlobalGrowthRateScale", &growth_p.growth_rate_scale, 0.0f, 10.0f);
    ImGui::SliderFloat("VineRadius", &growth_p.vine_radius, 0.005f, 0.2f);

    auto vine_color = tree::get_render_vines_color();
    if (ImGui::SliderFloat3("VinesColor", &vine_color.x, 0.0f, 1.0f)) {
      tree::set_render_vines_color(vine_color);
    }

    ImGui::Checkbox("DrawCubes", &growth_p.draw_point_cubes);
    ImGui::SliderFloat3("LineColor", &growth_p.line_color.x, 0.0f, 1.0f);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("OrnFoliage")) {
    if (ImGui::SliderFloat3("Color0", &globals.debug_foliage_color0.x, 0.0f, 1.0f)) {
      need_update_orn_foliage_insts = true;
    }
    if (ImGui::SliderFloat3("Color1", &globals.debug_foliage_color1.x, 0.0f, 1.0f)) {
      need_update_orn_foliage_insts = true;
    }
    if (ImGui::SliderFloat3("Color2", &globals.debug_foliage_color2.x, 0.0f, 1.0f)) {
      need_update_orn_foliage_insts = true;
    }
    if (ImGui::SliderFloat3("Color3", &globals.debug_foliage_color3.x, 0.0f, 1.0f)) {
      need_update_orn_foliage_insts = true;
    }

    ImGui::TreePop();
  }

  if (need_update_orn_foliage_insts) {
    update_debug_ornamental_foliage_instances(
      globals.debug_foliage_instances,
      globals.debug_foliage_color0, globals.debug_foliage_color1,
      globals.debug_foliage_color2, globals.debug_foliage_color3);
  }

  if (ImGui::TreeNode("SpiralAroundNodes2")) {
    auto& p = globals.spiral_around_nodes_2_params;
    ImGui::Text("Time: %0.4fms", p.compute_time_ms);
    ImGui::Text("Adjust time: %0.4fms", p.last_adjust_time_ms);
    ImGui::SliderFloat("Vel", &p.vel, 0.0f, 8.0f);
    ImGui::SliderFloat("Scale", &p.scale, 0.0f, 4.0f);
    ImGui::SliderFloat3("Color", &p.color.x, 0.0f, 1.0f);
    ImGui::SliderFloat("Theta", &p.theta, -pif(), pif());
    ImGui::SliderFloat("NormalOffset", &p.n_off, 0.0f, 1.0f);
    ImGui::SliderFloat("TaperFrac", &p.taper_frac, 0.0f, 1.0f);
    ImGui::SliderFloat("VelExpoFrac", &p.vel_expo_frac, 0.0f, 1.0f);
    ImGui::SliderInt("NumQuadSegments", &p.num_quad_segments, 4, 32);
    ImGui::SliderInt("NumPointsPerSegment", &p.num_points_per_segment, 4, 32);
    ImGui::Checkbox("DrawFrames", &p.draw_frames);
    ImGui::Checkbox("DisableIntersectCheck", &p.disable_intersect_check);
    ImGui::Checkbox("Disable", &p.disabled);
    ImGui::SliderInt("MaxNumMedLatIsectBounds", &p.max_num_medial_lateral_intersect_bounds, 0, 8);
    ImGui::Checkbox("LodEnabled", &p.enable_lod);
    ImGui::SliderFloat("LodDistance", &p.lod_distance, 0.0f, 128.0f);
    ImGui::Checkbox("EnableResourceSys", &p.enable_resource_sys);
    ImGui::Checkbox("UseResourceSys", &p.use_resource_sys);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("FitAABBs")) {
    tree::debug::render_fit_node_aabbs_gui_dropdown();
    ImGui::TreePop();
  }

  ImGui::End();
}

#undef SET_MODIFIED

GROVE_NAMESPACE_END
