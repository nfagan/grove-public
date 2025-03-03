#include "DebugTreeRootsComponent.hpp"
#include "roots_components.hpp"
#include "roots_growth.hpp"
#include "roots_render.hpp"
#include "growth_on_nodes.hpp"
#include "serialize_generic.hpp"
#include "fit_bounds.hpp"
#include "../imgui/ProceduralTreeRootsGUI.hpp"
#include "../render/debug_draw.hpp"
#include "../editor/editor.hpp"
#include "serialize.hpp"
#include "utility.hpp"
#include "render.hpp"
#include "../wind/SpatiallyVaryingWind.hpp"
#include "../wind/WindDisplacement.hpp"
#include "../util/texture_io.hpp"
#include "../terrain/terrain.hpp"
#include "grove/env.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/profile.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/pack.hpp"
#include "grove/load/image.hpp"
#include "grove/visual/Image.hpp"
#include "grove/math/util.hpp"
#include "grove/math/random.hpp"
#include "grove/math/frame.hpp"
#include "grove/math/window.hpp"
#include "grove/math/ease.hpp"

#define CONSTANT_INITIAL_RADIUS (1)

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

using UpdateInfo = DebugTreeRootsComponent::UpdateInfo;
using InitResult = DebugTreeRootsComponent::InitResult;
using RenderInstances = std::vector<ProceduralTreeRootsRenderer::Instance>;
using RenderWindInstances = std::vector<ProceduralTreeRootsRenderer::WindInstance>;
using RendererContext = ProceduralTreeRootsRenderer::AddResourceContext;

enum class TreeRootsGrowthState {
  Idle = 0,
  Growing,
  Alive,
  Receding,
};

struct TreeRootsMeta {
  TreeRootsGrowthState growth_state{};
  Stopwatch stopwatch;
};

struct AddRootsParams {
  bool is_tree;
  float node_length;
  float leaf_diameter;
  float diameter_power;
  int max_num_nodes;
};

constexpr float initial_radius_limiter_diameter() {
  return 0.25f * 2.0f;
}

constexpr int max_num_nodes_per_roots() {
  return 512;
//  return 128;
}

void make_node_obbs(const TreeRootNode* nodes, int num_nodes, OBB3f* dst) {
  for (int i = 0; i < num_nodes; i++) {
    dst[i] = make_tree_root_node_obb(nodes[i]);
  }
}

tree::io::Node to_serialized_node(const TreeRootNode& src) {
  tree::io::Node dst{};
  dst.position = src.position;
  dst.direction = src.direction;
  dst.diameter = src.target_diameter;
  dst.length = src.target_length;
  dst.parent = src.parent;
  dst.medial_child = src.medial_child;
  dst.lateral_child = src.lateral_child;
  return dst;
}

TreeRootNode from_serialized_node(const tree::io::Node& src) {
  TreeRootNode dst{};
  dst.parent = src.parent;
  dst.medial_child = src.medial_child;
  dst.lateral_child = src.lateral_child;
  dst.direction = src.direction;
  dst.position = src.position;
  dst.length = src.length;
  dst.target_length = src.length;
  dst.diameter = src.diameter;
  dst.target_diameter = src.diameter;
  return dst;
}

void to_serialized(const TreeRootNode* src, int num_nodes, tree::io::Node* dst) {
  for (int i = 0; i < num_nodes; i++) {
    dst[i] = to_serialized_node(src[i]);
  }
}

void from_serialized(const tree::io::Node* src, int num_nodes, TreeRootNode* dst) {
  for (int i = 0; i < num_nodes; i++) {
    dst[i] = from_serialized_node(src[i]);
  }
}

const TreeRootNode* child_of(const TreeRootNode& node, const TreeRootNode* nodes) {
  if (node.has_medial_child()) {
    return nodes + node.medial_child;
  } else if (node.has_lateral_child()) {
    return nodes + node.lateral_child;
  } else {
    return nullptr;
  }
}

tree::PackedWindAxisRootInfo 
make_packed_axis_root_info(int ni, const TreeRootNode* nodes, const TreeRootNode* eval_nodes,
                           const int* node_indices, const TreeRootRemappedWindAxisRoots& remapped_roots,
                           const TreeRootAxisRootIndices& axis_root_indices, const Bounds3f& aabb) {
  const int si = node_indices ? node_indices[ni] : ni;
  auto self_root_info = make_tree_root_wind_axis_root_info(
    si, eval_nodes, axis_root_indices, remapped_roots, aabb);

  WindAxisRootInfo child_root_info;
  if (auto* child = child_of(nodes[ni], nodes)) {
    const int ci = node_indices ? node_indices[int(child - nodes)] : int(child - nodes);
    child_root_info = make_tree_root_wind_axis_root_info(
      ci, eval_nodes, axis_root_indices, remapped_roots, aabb);
  } else {
    child_root_info = self_root_info;
  }

  return tree::to_packed_wind_info(self_root_info, child_root_info); 
}

void to_render_wind_instances(const TreeRootNode* nodes, const TreeRootNode* eval_nodes,
                              const int* node_indices, int num_nodes,
                              const TreeRootRemappedWindAxisRoots& remapped_roots,
                              const TreeRootAxisRootIndices& axis_root_indices, const Bounds3f& aabb,
                              ProceduralTreeRootsRenderer::WindInstance* dst) {
  for (int i = 0; i < num_nodes; i++) {
    auto packed_info = make_packed_axis_root_info(
      i, nodes, eval_nodes, node_indices, remapped_roots, axis_root_indices, aabb);
    auto& inst = dst[i];
    inst.packed_axis_root_info0 = packed_info[0];
    inst.packed_axis_root_info1 = packed_info[1];
    inst.packed_axis_root_info2 = packed_info[2];
  }
}

void to_render_instances(const TreeRootNode* nodes, const TreeRootNodeFrame* node_frames,
                         int num_nodes, bool atten_radius_by_length, float length_scale,
                         ProceduralTreeRootsRenderer::Instance* dst) {
  for (int i = 0; i < num_nodes; i++) {
    auto& inst = dst[i];
    auto& node = nodes[i];

    const auto& self_right = node_frames[i].i;
    const auto& self_up = node_frames[i].j;

    inst.self_position = node.position;
    inst.self_radius = node.diameter * 0.5f;

    if (atten_radius_by_length) {
      inst.self_radius *= (node.length / length_scale);
    }

    Vec3f child_right;
    Vec3f child_up;
    if (auto* child = child_of(node, nodes)) {
      inst.child_position = child->position;
      inst.child_radius = child->diameter * 0.5f;

      if (atten_radius_by_length) {
        inst.child_radius *= (child->length / length_scale);
      }

      int child_ind = int(child - nodes);
      child_right = node_frames[child_ind].i;
      child_up = node_frames[child_ind].j;
    } else {
      inst.child_position = node.tip_position();
      inst.child_radius = 0.0025f;
      if (atten_radius_by_length) {
        inst.child_radius *= (node.length / length_scale);
      }

      child_right = self_right;
      child_up = self_up;
    }

    ProceduralTreeRootsRenderer::encode_directions(
      self_right, self_up, child_right, child_up, &inst.directions0, &inst.directions1);
  }
}

std::vector<ProceduralTreeRootsRenderer::Instance> to_render_instances(const Internodes& inodes) {
  std::vector<ProceduralTreeRootsRenderer::Instance> instances(inodes.size());
  int i{};
  for (auto& node : inodes) {
    auto& inst = instances[i++];
    inst.self_position = node.position;
//    inst.self_direction = node.spherical_direction();
    inst.self_radius = node.radius();

    auto* child = node.has_medial_child() ?
      &inodes[node.medial_child] :
      node.has_lateral_child() ? &inodes[node.lateral_child] : nullptr;

    if (child) {
      inst.child_position = child->position;
//      inst.child_direction = child->spherical_direction();
      inst.child_radius = child->radius();
    } else {
      inst.child_position = node.tip_position();
//      inst.child_direction = inst.self_direction;
      inst.child_radius = 0.0025f;
    }
  }
  return instances;
}

float initial_radius_limiter_diameter(const TreeRootNode& node) {
#if CONSTANT_INITIAL_RADIUS
  (void) node;
  return initial_radius_limiter_diameter();
#else
  return node.target_diameter;
#endif
}

TreeRootsMeta make_roots_meta() {
  TreeRootsMeta result{};
  result.growth_state = TreeRootsGrowthState::Growing;
  return result;
}

TreeRoots make_deserialized_roots(int max_num_nodes, std::vector<TreeRootNode>&& nodes) {
  assert(max_num_nodes >= int(nodes.size()));
  TreeRoots result{};
  result.max_num_nodes = max_num_nodes;
  result.curr_num_nodes = int(nodes.size());
  result.nodes = std::move(nodes);
  result.nodes.resize(max_num_nodes);
  return result;
}

[[maybe_unused]] int axis_root_index(int node_index, const TreeRootNode* nodes) {
  auto* node = nodes + node_index;
  while (node->has_parent()) {
    auto* parent = nodes + node->parent;
    if (parent->medial_child == node_index) {
      node_index = node->parent;
      node = parent;
    } else {
      break;
    }
  }
  assert(nodes[node_index].is_axis_root(node_index, nodes));
  return node_index;
}

Vec3f randomly_offset(const Vec3f& curr, float rand_strength) {
  return normalize(curr + Vec3f{urand_11f(), urand_11f(), urand_11f()} * rand_strength);
}

void draw_intersecting(const bounds::RadiusLimiter* lim, const Vec3f& p, float s) {
  bounds::RadiusLimiterElement el{};
  el.i = ConstVec3f::positive_x;
  el.j = ConstVec3f::positive_y;
  el.k = ConstVec3f::positive_z;
  el.radius = s;
  el.half_length = s;
  el.p = p;

  auto query_obb = el.to_obb(el.radius);
  vk::debug::draw_obb3(query_obb, Vec3f{1.0f});

  std::vector<bounds::RadiusLimiterElement> isect;
  bounds::gather_intersecting(lim, el, isect);
  for (auto& hit : isect) {
    vk::debug::draw_obb3(hit.to_obb(hit.radius), Vec3f{0.0f, 0.0f, 1.0f});
  }
}

void draw_cube_grid(const bounds::RadiusLimiter* lim, const Vec3f& p) {
  int freqs[512];
  float filt[512];
  float tmp_filt[512];

  memset(freqs, 0, 512 * sizeof(int));
  auto hist_cell_counts = Vec3<int16_t>{8};
  const int16_t pow2_cell_size{};
  const float cell_size = std::pow(2.0f, float(pow2_cell_size));

  auto c_off = float(hist_cell_counts.x) * 0.5f * cell_size;
  auto orif = floor(p / cell_size) - c_off;
  auto ori = Vec3<int16_t>{int16_t(orif.x), int16_t(orif.y), int16_t(orif.z)};

  Vec3<int16_t> cell_size3{pow2_cell_size};
  bounds::histogram(lim, ori, cell_size3, hist_cell_counts, 0, freqs);
  bounds::filter_histogram(freqs, hist_cell_counts, tmp_filt, filt);
  auto mean_dir = bounds::mean_gradient(filt, hist_cell_counts);

  float s{};
  for (float f : filt) {
    s = std::max(s, f);
  }
  if (s > 0.0f) {
    for (auto& f : filt) {
      f /= s;
    }
  }

  for (int16_t k = 0; k < hist_cell_counts.z; k++) {
    for (int16_t i = 0; i < hist_cell_counts.x; i++) {
      for (int16_t j = 0; j < hist_cell_counts.y; j++) {
        const int page_offset = k * hist_cell_counts.x * hist_cell_counts.y;
        const int tile_offset = i * hist_cell_counts.y + j;
        const int ind = page_offset + tile_offset;
        auto grid_p = (to_vec3f(ori + Vec3<int16_t>{i, j, k}) + 0.5f) * cell_size;
        vk::debug::draw_cube(grid_p, Vec3f{0.25f} * filt[ind], Vec3f{filt[ind]});
      }
    }
  }

  Bounds3f grid_bounds{
    to_vec3f(ori) * cell_size,
    to_vec3f(ori) + cell_size * float(hist_cell_counts.x)
  };
  vk::debug::draw_aabb3(grid_bounds, Vec3f{1.0f});

  auto dir_len = mean_dir.length();
  if (dir_len > 1e-2f) {
    mean_dir /= dir_len;
    auto p0 = to_vec3f(ori) * cell_size + c_off;
    auto p1 = p0 - mean_dir * c_off;
    vk::debug::draw_line(p0, p1, Vec3f{1.0f, 0.0f, 0.0f});
  }
}

void to_render_instances(const TreeRoots& roots, RenderInstances& instances,
                         bool atten_radius_by_length) {
  TreeRootNodeFrame node_frames[2048];
  assert(roots.curr_num_nodes <= 2048);
  compute_tree_root_node_frames(roots.nodes.data(), roots.curr_num_nodes, node_frames);
  to_render_instances(
    roots.nodes.data(), node_frames,
    roots.curr_num_nodes,
    atten_radius_by_length, roots.node_length_scale,
    instances.data());
}

void to_render_wind_instances(const TreeRoots& roots, const Bounds3f& aabb,
                              RenderWindInstances& instances) {
  const auto* nodes = roots.nodes.data();
  const int num_nodes = roots.curr_num_nodes;
  auto remapped_roots = make_tree_root_remapped_wind_axis_roots(nodes, num_nodes);
  auto axis_root_indices = make_tree_root_axis_root_indices(nodes, num_nodes);
  to_render_wind_instances(
    nodes, nodes, nullptr, num_nodes, remapped_roots, axis_root_indices, aabb, instances.data());
}

void growing_nodes_to_render_instances(const TreeRootNode* growing, const int* growing_on_indices,
                                       const TreeRootNodeFrame* node_frames,
                                       int num_growing, const TreeRoots& growing_on,
                                       const Bounds3f& growing_on_aabb,
                                       RenderInstances& instances,
                                       RenderWindInstances& wind_instances) {
  to_render_instances(growing, node_frames, num_growing, false, 1.0f, instances.data());

  auto* growing_on_nodes = growing_on.nodes.data();
  auto remapped_roots = make_tree_root_remapped_wind_axis_roots(
    growing_on_nodes, growing_on.curr_num_nodes);
  auto axis_root_indices = make_tree_root_axis_root_indices(
    growing_on_nodes, growing_on.curr_num_nodes);

  to_render_wind_instances(
    growing, growing_on_nodes, growing_on_indices, num_growing,
    remapped_roots, axis_root_indices, growing_on_aabb, wind_instances.data());
}

float sample_wind_strength(const Vec3f& p, const UpdateInfo& info) {
  Vec2f wind_p = info.wind.to_normalized_position(exclude(p, 1));
  return info.wind_displacement.evaluate(wind_p).length();
}

void update_no_wind_drawable(ProceduralTreeRootsRenderer::DrawableHandle drawable,
                             const RenderInstances& instances, int curr_num_nodes,
                             const UpdateInfo& info) {
  assert(curr_num_nodes <= int(instances.size()));
  auto& renderer = info.roots_renderer;
  auto& ctx = info.roots_renderer_context;
  renderer.fill_activate(ctx, drawable, instances.data(), uint32_t(curr_num_nodes));
}

void update_wind_drawable(ProceduralTreeRootsRenderer::DrawableHandle drawable,
                          const RenderInstances& instances,
                          const RenderWindInstances& wind_instances, int curr_num_nodes,
                          const Bounds3f& aabb, const UpdateInfo& info) {
  assert(curr_num_nodes <= int(instances.size()));
  auto& renderer = info.roots_renderer;
  auto& ctx = info.roots_renderer_context;
  auto* inst = instances.data();
  auto* wind_inst = wind_instances.data();
  renderer.fill_activate(ctx, drawable, inst, wind_inst, uint32_t(curr_num_nodes));
  renderer.set_aabb(drawable, aabb);
}

TreeRootsGrowthContext make_growth_context() {
  TreeRootsGrowthContext result{};
  return result;
}

TreeRootsRecedeContext make_recede_context() {
  TreeRootsRecedeContext result{};
  return result;
}

struct SamplePointsOnNodesParams {
  float step_size;
  float bounds_radius_offset;
  Vec3f step_axis;
  bool prefer_entry_up_axis;
};

void smooth_sampled_points_on_nodes(const Vec3f* ps, int num_ps, Vec3f* dst_ps) {
  constexpr int k_size = 5;
  constexpr int k2 = k_size / 2;

  float kernel[k_size];
  win::gauss1d(kernel, k_size);

  for (int i = 0; i < num_ps; i++) {
    Vec3f s{};
    float den{};
    for (int j = 0; j < k_size; j++) {
      const int pi = clamp(i + (j - k2), 0, num_ps - 1);
      s += ps[pi] * kernel[j];
      den += kernel[j];
    }
    dst_ps[i] = s / den;
  }
}

void sampled_points_to_nodes(const Vec3f* ps, int num_points, TreeRootNode* dst) {
  for (int i = 0; i < num_points; i++) {
    const auto& p = ps[i];

    Vec3f axis;
    if (i + 1 < num_points) {
      axis = ps[i + 1] - p;
    } else if (num_points > 1) {
      axis = p - ps[i - 1];
    } else {
      axis = Vec3f{0.0f, 1.0f, 0.0f};  //  arbitrary
    }

    auto node = make_tree_root_root_node(p, normalize(axis), axis.length(), 0.0f);
    node.medial_child = i + 1 < num_points ? i + 1 : -1;
    node.parent = i > 0 ? i - 1 : -1;
    dst[i] = node;
  }
}

int sample_points_on_nodes(const TreeRoots& roots, Vec3f* samples, Vec3f* ns, int* node_indices,
                           int num_samples, const SamplePointsOnNodesParams& params) {
  OBB3f node_bounds[2048];
  assert(roots.curr_num_nodes <= 2048);
  make_node_obbs(roots.nodes.data(), roots.curr_num_nodes, node_bounds);
  auto node_aabb = compute_tree_root_node_position_aabb(roots.nodes.data(), roots.curr_num_nodes);

  const int points_per_node = 32;
  std::vector<tree::InternodeSurfaceEntry> surface_entries(roots.curr_num_nodes * points_per_node);

  PlacePointsOnInternodesParams place_params{};
  place_params.node_aabb = node_aabb;
  place_params.node_bounds = node_bounds;
  place_params.num_nodes = roots.curr_num_nodes;
  place_params.points_per_node = points_per_node;
  place_params.dst_entries = surface_entries.data();
  place_params.bounds_radius_offset = params.bounds_radius_offset;
  const int num_entries = tree::place_points_on_internodes(place_params);

  Temporary<int, 128> store_entry_indices;
  auto* entry_indices = store_entry_indices.require(num_samples);
  SamplePointsOnInternodesParams sample_params{};
  sample_params.node_aabb = node_aabb;
  sample_params.entries = surface_entries.data();
  sample_params.entry_indices = entry_indices;
  sample_params.num_entries = num_entries;
  sample_params.init_entry_index = 0;
  sample_params.step_axis = params.step_axis;
  sample_params.target_step_length = params.step_size;
  sample_params.max_step_length = params.step_size * 4.0f;
  sample_params.num_samples = num_samples;
  sample_params.dst_samples = samples;
  sample_params.prefer_entry_up_axis = params.prefer_entry_up_axis;
  const int num_sampled = tree::sample_points_on_internodes(sample_params);

  for (int i = 0; i < num_sampled; i++) {
    auto& entry = surface_entries[entry_indices[i]];
    node_indices[i] = entry.node_index;
    ns[i] = entry.decode_normal();
  }

  return num_sampled;
}

struct GrowthOnNodesData {
  std::vector<Vec3f> sampled_points;
  std::vector<int> sampled_indices;
  Vec3f roots_origin;
  ProceduralTreeRootsRenderer::DrawableHandle debug_drawable;
  bool wind_enabled_for_associated_roots;
};

struct FitBoundsData {
  std::vector<OBB3f> src_bounds;
  std::vector<OBB3f> fit_bounds;
};

struct GlobalData {
  std::vector<TreeRootsGrowthContext> growth_contexts;
  std::vector<TreeRootsRecedeContext> recede_contexts;
  std::vector<TreeRoots> roots;
  std::vector<TreeRootsMeta> roots_meta;
  std::vector<std::vector<bounds::RadiusLimiterElementHandle>> radius_limiter_elements;
  std::vector<ProceduralTreeRootsRenderer::Instance> render_instances;
  std::vector<ProceduralTreeRootsRenderer::WindInstance> render_wind_instances;
  std::vector<ProceduralTreeRootsRenderer::DrawableHandle> drawables;
  GrowthOnNodesData growth_on_nodes_data;
  FitBoundsData fit_bounds_data;
  DynamicArray<AddRootsParams, 4> pending_add_roots;
} global_data;

Vec3f new_root_origin(const DebugTreeRootsComponent& component, const Terrain* terrain) {
  if (component.params.add_roots_at_tform) {
    return component.debug_grid_tform->get_current().translation;
  } else {
    auto off = Vec3f{urand_11f(), 0.0f, urand_11f()} * component.params.rand_root_origin_span;
    auto base = component.params.default_root_origin + off;
    if (terrain) {
      base.y = terrain->height_nearest_position_xz(base);
    }
    return base;
  }
}

AddRootsParams to_add_roots_params(const DebugTreeRootsComponent& component, int max_num_nodes) {
  assert(max_num_nodes <= max_num_nodes_per_roots());
  AddRootsParams result{};
  result.is_tree = component.params.make_tree;
  result.node_length = component.params.node_length;
  result.leaf_diameter = component.params.leaf_diameter;
  result.diameter_power = component.params.diameter_power;
  result.max_num_nodes = max_num_nodes;
  return result;
}

AddRootsParams make_short_tree_add_roots_params() {
  AddRootsParams result{};
  result.is_tree = true;
  result.node_length = 1.0f;
  result.leaf_diameter = 0.075f;
  result.diameter_power = 1.8f;
  result.max_num_nodes = 64;
  return result;
}

void add_roots(bounds::RadiusLimiter* radius_limiter, const AddRootsParams& params,
               const Vec3f& root_p,
               ProceduralTreeRootsRenderer& renderer,
               const RendererContext& renderer_ctx, bounds::RadiusLimiterElementTag roots_tag) {
  const auto roots_id = bounds::RadiusLimiterAggregateID::create();

  auto& roots = global_data.roots.emplace_back();
  const auto root_dir = Vec3f{0.0f, 1.0f, 0.0f} * (params.is_tree ? 1.0f : -1.0f);
  roots = make_tree_roots(
    roots_id, params.max_num_nodes, root_p, root_dir,
    params.node_length, params.leaf_diameter, params.leaf_diameter, params.diameter_power);

  auto& roots_meta = global_data.roots_meta.emplace_back();
  roots_meta = make_roots_meta();

  auto& growth_ctx = global_data.growth_contexts.emplace_back();
  growth_ctx = make_growth_context();
  growth_ctx.growing.push_back(make_growing_tree_root_node(0));

  auto& recede_context = global_data.recede_contexts.emplace_back();
  recede_context = make_recede_context();

  auto& rad_lims = global_data.radius_limiter_elements.emplace_back();
  rad_lims.resize(params.max_num_nodes);
  auto root_el = make_tree_root_node_radius_limiter_element(
    make_tree_root_node_obb(roots.nodes[0]), roots_id, roots_tag);
  rad_lims[0] = bounds::insert(radius_limiter, root_el);

  auto& drawable = global_data.drawables.emplace_back();
  const auto draw_type = params.is_tree ?
    ProceduralTreeRootsRenderer::DrawableType::Wind :
    ProceduralTreeRootsRenderer::DrawableType::NoWind;
  drawable = renderer.create(draw_type);
  renderer.reserve(renderer_ctx, drawable, params.max_num_nodes);
}

void add_deserialized_roots(bounds::RadiusLimiter* radius_limiter, int max_num_nodes,
                            std::vector<TreeRootNode>&& nodes,
                            ProceduralTreeRootsRenderer& renderer,
                            const RendererContext& renderer_ctx,
                            bounds::RadiusLimiterElementTag roots_tag) {
  const auto roots_id = bounds::RadiusLimiterAggregateID::create();

  auto& roots = global_data.roots.emplace_back();
  roots = make_deserialized_roots(max_num_nodes, std::move(nodes));

  auto& roots_meta = global_data.roots_meta.emplace_back();
  roots_meta = make_roots_meta();

  auto& growth_ctx = global_data.growth_contexts.emplace_back();
  growth_ctx = make_growth_context();

  auto& rad_lims = global_data.radius_limiter_elements.emplace_back();
  rad_lims.resize(max_num_nodes);

  for (int i = 0; i < roots.curr_num_nodes; i++) {
    rad_lims[i] = bounds::insert(
      radius_limiter,
      make_tree_root_node_radius_limiter_element(
        make_tree_root_node_obb(roots.nodes[i]), roots_id, roots_tag));
  }

  auto& drawable = global_data.drawables.emplace_back();
  drawable = renderer.create(ProceduralTreeRootsRenderer::DrawableType::NoWind);
  renderer.reserve(renderer_ctx, drawable, max_num_nodes);
}

AssignRootsDiameterParams to_assign_diameter_params(const TreeRoots& roots) {
  AssignRootsDiameterParams diam_params{};
  diam_params.leaf_diameter = roots.leaf_diameter;
  diam_params.diameter_power = roots.diameter_power;
  return diam_params;
}

GrowRootsParams to_grow_roots_params(const DebugTreeRootsComponent& component,
                                     const TreeRoots& roots, const UpdateInfo& info) {
  float gr = component.params.growth_rate;
  if (component.params.scale_growth_rate_by_signal) {
    gr *= component.spectral_fraction;
  }

  GrowRootsParams grow_params{};
  grow_params.real_dt = info.real_dt;
  grow_params.growth_rate = gr;
  grow_params.attractor_point_scale = component.params.attractor_point_scale;
  if (component.params.camera_position_attractor) {
    grow_params.attractor_point = info.camera_position;
  } else {
    grow_params.attractor_point = component.debug_attractor_tform->get_current().translation;
  }
  grow_params.p_spawn_lateral = component.params.p_spawn_lateral;
  grow_params.node_length_scale = roots.node_length_scale;
  grow_params.min_axis_length_spawn_lateral = component.params.min_axis_length_spawn_lateral;
  grow_params.disable_node_creation = false;
  return grow_params;
}

void update_roots(DebugTreeRootsComponent& component, const UpdateInfo& info) {
  auto& instances = global_data.render_instances;
  const auto roots_tag = info.roots_tag;
  auto& wind_instances = global_data.render_wind_instances;
  auto* lim = info.radius_limiter;

  for (int i = 0; i < int(global_data.roots.size()); i++) {
    auto& roots = global_data.roots[i];
    auto& growth_ctx = global_data.growth_contexts[i];
    auto& recede_ctx = global_data.recede_contexts[i];
    auto& rad_lims = global_data.radius_limiter_elements[i];
    auto& roots_meta = global_data.roots_meta[i];
    auto drawable = global_data.drawables[i];

    auto diam_params = to_assign_diameter_params(roots);
    auto grow_params = to_grow_roots_params(component, roots, info);

    bool need_modify_drawable{};
    bool atten_radius_by_length{};
    switch (roots_meta.growth_state) {
      case TreeRootsGrowthState::Growing: {
        need_modify_drawable = true;
        const auto grow_res = grow_roots(
          &roots, lim, rad_lims.data(), roots_tag, growth_ctx, grow_params, diam_params);
        if (grow_res.finished) {
          //  finished growing
          roots_meta.growth_state = TreeRootsGrowthState::Alive;
          roots_meta.stopwatch.reset();
        }
        break;
      }
      case TreeRootsGrowthState::Alive: {
        if (roots_meta.stopwatch.delta().count() > 10.0 && component.params.allow_recede) {
          roots_meta.growth_state = TreeRootsGrowthState::Receding;
          init_roots_recede_context(&recede_ctx, roots.nodes.data(), roots.curr_num_nodes);
        }
        break;
      }
      case TreeRootsGrowthState::Receding: {
        need_modify_drawable = true;
        atten_radius_by_length = true;
        const auto recede_res = recede_roots(&roots, lim, rad_lims.data(), recede_ctx, grow_params);
        if (recede_res.finished) {
          roots_meta.growth_state = TreeRootsGrowthState::Idle;
        }
        break;
      }
      case TreeRootsGrowthState::Idle: {
        break;
      }
      default: {
        assert(false);
      }
    }

    if (need_modify_drawable) {
      to_render_instances(roots, instances, atten_radius_by_length);
      if (drawable.type == ProceduralTreeRootsRenderer::DrawableType::Wind) {
        auto aabb = compute_tree_root_node_position_aabb(roots.nodes.data(), roots.curr_num_nodes);
        to_render_wind_instances(roots, aabb, wind_instances);
        update_wind_drawable(drawable, instances, wind_instances, roots.curr_num_nodes, aabb, info);
      } else {
        update_no_wind_drawable(drawable, instances, roots.curr_num_nodes, info);
      }
    }

    if (drawable.type == ProceduralTreeRootsRenderer::DrawableType::Wind) {
      info.roots_renderer.set_wind_disabled(drawable, component.params.wind_disabled);
      info.roots_renderer.set_wind_strength(drawable, sample_wind_strength(roots.origin, info));
    }
  }
}

void update_growth_on_nodes(DebugTreeRootsComponent& component, const UpdateInfo& info) {
  auto& params = component.params;

  if (params.need_generate_sample_points_on_nodes &&
      params.selected_root_index < int(global_data.roots.size())) {
    auto& roots = global_data.roots[params.selected_root_index];
    auto& assoc_drawable = global_data.drawables[params.selected_root_index];

    //  Gen samples
    SamplePointsOnNodesParams sample_params{};
    sample_params.bounds_radius_offset = params.points_on_nodes_radius_offset;
    sample_params.step_size = params.points_on_nodes_step_size;
    sample_params.step_axis = params.points_on_nodes_step_axis;
    sample_params.prefer_entry_up_axis = params.points_on_nodes_prefer_entry_up_axis;

    const int num_samples = 64;
    auto& growth_data = global_data.growth_on_nodes_data;
    growth_data.roots_origin = roots.origin;
    growth_data.wind_enabled_for_associated_roots =
      assoc_drawable.is_valid() && assoc_drawable.is_wind_type();

    auto& sampled_points = growth_data.sampled_points;
    auto& sampled_inds = growth_data.sampled_indices;

    sampled_points.resize(num_samples);
    sampled_inds.resize(num_samples);
    std::vector<Vec3f> sampled_ns(num_samples);

    const int num_sampled_nodes = sample_points_on_nodes(
      roots, sampled_points.data(), sampled_ns.data(),
      sampled_inds.data(), num_samples, sample_params);
    sampled_points.resize(num_sampled_nodes);
    sampled_inds.resize(num_sampled_nodes);
    sampled_ns.resize(num_sampled_nodes);

    if (num_sampled_nodes > 0) {
      if (params.smooth_points_on_nodes) {
        auto* dst_points = sampled_points.data();
        std::vector<Vec3f> src_samples(num_sampled_nodes);
        std::copy(dst_points, dst_points + num_sampled_nodes, src_samples.data());
        smooth_sampled_points_on_nodes(src_samples.data(), num_sampled_nodes, dst_points);
      }

      //  Convert to nodes
      std::vector<TreeRootNode> sampled_nodes(num_sampled_nodes);
      sampled_points_to_nodes(sampled_points.data(), num_sampled_nodes, sampled_nodes.data());

      AssignRootsDiameterParams diam_params{};
      diam_params.leaf_diameter = params.points_on_nodes_leaf_diameter;
      diam_params.diameter_power = params.points_on_nodes_diameter_power;
      assign_diameter(sampled_nodes.data(), diam_params);
      for (auto& node: sampled_nodes) {
        node.diameter = node.target_diameter;
        node.length = node.target_length;
      }

      //  Convert to render instances
      const auto root_aabb = compute_tree_root_node_position_aabb(
        roots.nodes.data(), roots.curr_num_nodes);
      RenderInstances render_insts(num_sampled_nodes);
      RenderWindInstances render_wind_insts(num_sampled_nodes);

      TreeRootNodeFrame node_frames[2048];
      assert(num_sampled_nodes <= 2048);
      compute_tree_root_node_frames(sampled_nodes.data(), num_sampled_nodes, node_frames);

      growing_nodes_to_render_instances(
        sampled_nodes.data(), sampled_inds.data(), node_frames, num_sampled_nodes, roots, root_aabb,
        render_insts, render_wind_insts);

      //  Make drawable
      auto& drawable = growth_data.debug_drawable;
      auto& renderer = info.roots_renderer;
      auto& renderer_ctx = info.roots_renderer_context;
      if (!drawable.is_valid()) {
        drawable = info.roots_renderer.create(ProceduralTreeRootsRenderer::DrawableType::Wind);
      }
      renderer.reserve(renderer_ctx, drawable, uint32_t(num_sampled_nodes));
      renderer.fill_activate(
        renderer_ctx, drawable,
        render_insts.data(), render_wind_insts.data(), uint32_t(num_sampled_nodes));
      renderer.set_aabb(drawable, root_aabb);
    }

    params.need_generate_sample_points_on_nodes = false;
  }

  auto& drawable = global_data.growth_on_nodes_data.debug_drawable;
  if (global_data.growth_on_nodes_data.debug_drawable.is_valid()) {
    bool wind_enabled = global_data.growth_on_nodes_data.wind_enabled_for_associated_roots;
    auto roots_ori = global_data.growth_on_nodes_data.roots_origin;
    float wind_f = wind_enabled ? sample_wind_strength(roots_ori, info) : 0.0f;
    info.roots_renderer.set_wind_strength(drawable, wind_f);
    info.roots_renderer.set_linear_color(drawable, params.points_on_nodes_color);
  }

#if 0
  vk::debug::draw_connected_line(
    global_data.growth_on_nodes_data.sampled_points.data(),
    int(global_data.growth_on_nodes_data.sampled_points.size()),
    Vec3f{1.0f, 0.0f, 0.0f});
#endif
#if 0
  for (auto& p : global_data.growth_on_nodes_data.sampled_points) {
    vk::debug::draw_cube(p, Vec3f{0.1f}, Vec3f{1.0f});
  }
#endif
}

void maybe_spawn_axis(DebugTreeRootsComponent& component, bounds::RadiusLimiter* lim,
                      bounds::RadiusLimiterElementTag roots_tag) {
  const int root_index = component.params.selected_root_index;
  const int node_index = component.params.selected_node_index;

  auto* root = root_index >= 0 && root_index < int(global_data.roots.size()) ?
               &global_data.roots[root_index] : nullptr;
  if (!root) {
    return;
  }

  if (node_index >= 0 && node_index < root->curr_num_nodes &&
      root->curr_num_nodes < root->max_num_nodes &&
      !root->nodes[node_index].has_lateral_child()) {
    auto& gc = global_data.growth_contexts[root_index];
    auto is_node = [node_index](const GrowingTreeRootNode& node) {
      return node.index == node_index;
    };

    auto it = std::find_if(gc.growing.begin(), gc.growing.end(), is_node);
    if (it != gc.growing.end()) {
      return;
    }

    auto& parent = root->nodes[node_index];
    assert(parent.lateral_child == -1);
    auto new_ind = root->curr_num_nodes++;
    parent.lateral_child = new_ind;

    auto new_dir = randomly_offset(parent.direction, 0.5f);
    const float node_len = root->node_length_scale;
    root->nodes[new_ind] = copy_make_tree_root_node(
      parent, root_index, new_dir, parent.position, node_len);
    gc.growing.push_back(make_growing_tree_root_node(new_ind));

    auto& node = root->nodes[new_ind];
    const float diam = initial_radius_limiter_diameter(node);
    auto query_obb = make_tree_root_node_obb(node.position, new_dir, node.target_length, diam);
    auto query_el = make_tree_root_node_radius_limiter_element(query_obb, root->id, roots_tag);

    assert(root_index < int(global_data.radius_limiter_elements.size()));
    auto& radius_limiter_handles = global_data.radius_limiter_elements[root_index];
    assert(new_ind < int(radius_limiter_handles.size()) &&
           radius_limiter_handles[new_ind].index == 0);
    radius_limiter_handles[new_ind] = bounds::insert(lim, query_el);
  }
}

void update_fit_around_axis(DebugTreeRootsComponent& component, const UpdateInfo&) {
  if (component.params.need_fit_bounds_around_axis && 
      component.params.selected_root_index < int(global_data.roots.size())) {
    auto& roots = global_data.roots[component.params.selected_root_index];
    if (component.params.selected_node_index < roots.curr_num_nodes) {
      int axis_ind = axis_root_index(component.params.selected_node_index, roots.nodes.data());

      std::vector<OBB3f> src_bounds;
      while (axis_ind != -1) {
        auto* node = roots.nodes.data() + axis_ind;
        src_bounds.push_back(make_tree_root_node_obb(*node));
        axis_ind = node->medial_child;
      }

      auto& dst_bounds = global_data.fit_bounds_data.fit_bounds;
      dst_bounds.resize(src_bounds.size());

      bounds::FitOBBsAroundAxisParams fit_params{};
      fit_params.axis_bounds = src_bounds.data();
      fit_params.num_bounds = int(src_bounds.size());
      fit_params.max_size_ratio = Vec3f{2.0f, infinityf(), 2.0f};
      fit_params.test_type = bounds::FitOBBsAroundAxisParams::TestType::SizeRatio;
      fit_params.dst_bounds = dst_bounds.data();
      const int num_fit = bounds::fit_obbs_around_axis(fit_params);

      global_data.fit_bounds_data.src_bounds = std::move(src_bounds);
      global_data.fit_bounds_data.fit_bounds.resize(num_fit);

      component.params.need_fit_bounds_around_axis = false;
    }
  }

  for (auto& src : global_data.fit_bounds_data.src_bounds) {
    vk::debug::draw_obb3(src, Vec3f{1.0f, 0.0f, 0.0f});
  }
  for (auto& fit : global_data.fit_bounds_data.fit_bounds) {
#if 1
    auto draw = fit;
    const float r = std::max(draw.half_size.x, draw.half_size.z);
    draw.half_size.x = r;
    draw.half_size.z = r;
    vk::debug::draw_obb3(draw, Vec3f{0.0f, 1.0f, 0.0f});
#else
    vk::debug::draw_obb3(fit, Vec3f{0.0f, 1.0f, 0.0f});
#endif
  }
}

Optional<std::vector<TreeRootNode>> deserialize_nodes(const std::string& file_path) {
  auto deser_res = tree::io::deserialize(file_path);
  if (!deser_res) {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to deserialize nodes.", "DebugTreeRootsComponent");
    return NullOpt{};
  } else {
    auto& deser = deser_res.value();
    std::vector<TreeRootNode> result(deser.size());
    from_serialized(deser.data(), int(deser.size()), result.data());
    return Optional<decltype(result)>(std::move(result));
  }
}

void serialize_nodes(const std::string& file_path, const TreeRootNode* src, int num_nodes) {
  std::vector<tree::io::Node> dst(num_nodes);
  to_serialized(src, num_nodes, dst.data());
  if (!tree::io::serialize(dst.data(), num_nodes, file_path)) {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to serialize nodes.", "DebugTreeRootsComponent");
  }
}

void set_sample_points_on_nodes_preset1(DebugTreeRootsComponent& component) {
  auto& params = component.params;
  params.smooth_points_on_nodes = true;
  params.points_on_nodes_radius_offset = 0.07f;
  params.points_on_nodes_step_size = 0.95f;
  params.points_on_nodes_leaf_diameter = 0.04f;
  params.points_on_nodes_diameter_power = 3.0f;
  params.points_on_nodes_color = Vec3<uint8_t>{82, 168, 48};
  params.points_on_nodes_prefer_entry_up_axis = true;
}

} //  anon

InitResult DebugTreeRootsComponent::initialize(const InitInfo& info) {
  InitResult result{};

  params.max_num_nodes_per_roots = std::min(
    params.max_num_nodes_per_roots, max_num_nodes_per_roots());

  std::string model_p{GROVE_ASSET_DIR};
  auto debug_model = model_p + "/serialized_trees/test.dat";
  auto model_res = tree::deserialize_file(debug_model.c_str());
  if (!model_res) {
    return result;
  }

  auto& renderer = info.roots_renderer;
  auto& renderer_ctx = info.roots_renderer_context;

  auto& inodes = model_res.value().internodes;
//  auto num_inodes = uint32_t(inodes.size());
//  auto insts = to_render_instances(inodes);

//  debug_drawable = renderer.create();
//  renderer.fill_activate(renderer_ctx, debug_drawable.value(), insts.data(), num_inodes);
  debug_internodes = std::move(inodes);
  tree::copy_diameter_to_lateral_q(debug_internodes);

  global_data.render_instances.resize(max_num_nodes_per_roots());
  global_data.render_wind_instances.resize(max_num_nodes_per_roots());

  const int num_init_roots = 0;
//  const int num_init_roots = 8;
  for (int i = 0; i < num_init_roots; i++) {
    const auto ori = new_root_origin(*this, nullptr);
    const auto add_params = to_add_roots_params(*this, max_num_nodes_per_roots());
    add_roots(info.radius_limiter, add_params, ori, renderer, renderer_ctx, info.roots_tag);
  }

  debug_grid_tform = info.transform_system.create(
    TRS<float>::make_translation_scale(Vec3f{0.0f, 16.0f, 0.0f}, Vec3f{1.0f}));
  debug_attractor_tform = info.transform_system.create(
    TRS<float>::make_translation_scale(Vec3f{2.0f, 16.0f, 0.0f}, Vec3f{1.0f}));

#if 1
  grid_tform_editor = editor::create_transform_editor(info.editor, debug_grid_tform, {});
  info.editor->transform_editor.set_color(grid_tform_editor, Vec3f{0.0f, 0.0f, 1.0f});

  attractor_tform_editor = editor::create_transform_editor(info.editor, debug_attractor_tform, {});
  info.editor->transform_editor.set_color(attractor_tform_editor, Vec3f{0.0f, 1.0f, 1.0f});
#endif

//  params.add_roots_at_new_tree_origins = true;

  return result;
}

void DebugTreeRootsComponent::update(const UpdateInfo& info) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("DebugTreeRootsComponent/update");
  (void) profiler;

  if (serialize_to_file && params.selected_root_index < int(global_data.roots.size())) {
    auto& roots = global_data.roots[params.selected_root_index];
    auto& nodes = roots.nodes;
    serialize_nodes(serialize_to_file.value(), nodes.data(), roots.curr_num_nodes);
    serialize_to_file = NullOpt{};
  }
  if (deserialize_from_file) {
    if (auto deser_nodes = deserialize_nodes(deserialize_from_file.value())) {
      if (int(deser_nodes.value().size()) <= max_num_nodes_per_roots()) {
        add_deserialized_roots(
          info.radius_limiter, max_num_nodes_per_roots(), std::move(deser_nodes.value()),
          info.roots_renderer, info.roots_renderer_context, info.roots_tag);
      } else {
        GROVE_LOG_ERROR_CAPTURE_META("Too many nodes.", "DebugTreeRootsComponent");
      }
    }
    deserialize_from_file = NullOpt{};
  }

  if (params.need_create_roots) {
    for (int i = 0; i < params.num_roots_create; i++) {
      global_data.pending_add_roots.push_back(
        to_add_roots_params(*this, params.max_num_nodes_per_roots));
    }
    params.need_create_roots = false;
  }

  if (params.need_create_short_tree) {
    for (int i = 0; i < params.num_roots_create; i++) {
      global_data.pending_add_roots.push_back(make_short_tree_add_roots_params());
    }
    params.need_create_short_tree = false;
  }

  {
    const int num_add = int(global_data.pending_add_roots.size());
    for (int i = 0; i < num_add; i++) {
      const auto add_params = global_data.pending_add_roots[0];
      add_roots(
        info.radius_limiter, add_params, new_root_origin(*this, &info.terrain),
        info.roots_renderer, info.roots_renderer_context, info.roots_tag);
      global_data.pending_add_roots.erase(global_data.pending_add_roots.begin());
    }
  }

  if (params.add_roots_at_new_tree_origins) {
    auto& renderer = info.roots_renderer;
    auto& ctx = info.roots_renderer_context;
    for (int i = 0; i < info.num_newly_created_trees; i++) {
      const auto& ori = info.newly_created_tree_origins[i];
      auto add_at = ori - Vec3f{0.0f, 0.125f, 0.0f};
      const auto add_params = to_add_roots_params(*this, max_num_nodes_per_roots());
      add_roots(info.radius_limiter, add_params, add_at, renderer, ctx, info.roots_tag);
    }
  }

  if (params.selected_root_index < int(global_data.roots.size())) {
    auto& roots = global_data.roots[params.selected_root_index];
    if (roots.curr_num_nodes > 0 && params.draw_node_frames) {
      auto& root_p = roots.nodes[0].position;
      vk::debug::draw_cube(root_p + Vec3f{0.0f, 2.0f, 0.0f}, Vec3f{1.0f}, Vec3f{1.0f, 0.0, 0.0f});
    }

    if (params.draw_node_frames) {
      TreeRootNodeFrame root_frames[1024];
      const int num_compute = std::min(1024, roots.curr_num_nodes);
      compute_tree_root_node_frames(roots.nodes.data(), num_compute, root_frames);
      for (int i = 0; i < num_compute; i++) {
        auto& node = roots.nodes[i];
        auto p0 = node.position + node.direction * node.target_length * 0.5f;
        auto p1r = p0 + root_frames[i].i * 1.5f;
        auto p1f = p0 + root_frames[i].k * 1.5f;
        vk::debug::draw_line(p0, p1r, Vec3f{1.0f, 0.0f, 0.0f});
        vk::debug::draw_line(p0, p1f, Vec3f{0.0f, 1.0f, 0.0f});
      }
    }
  }

  update_roots(*this, info);
  update_fit_around_axis(*this, info);

  if (params.debug_draw_enabled) {
    info.editor->transform_editor.set_disabled(grid_tform_editor, false);

    if (params.draw_cube_grid) {
      draw_cube_grid(info.radius_limiter, debug_grid_tform->get_current().translation);
    } else {
      draw_intersecting(info.radius_limiter, debug_grid_tform->get_current().translation, 4.0f);
    }
  } else {
    info.editor->transform_editor.set_disabled(grid_tform_editor, true);
  }

  if (params.need_spawn_axis) {
    maybe_spawn_axis(*this, info.radius_limiter, info.roots_tag);
    params.need_spawn_axis = false;
  }

  if (params.drawable_needs_update && debug_drawable) {
    auto insts = to_render_instances(debug_internodes);
    const auto num_inodes = uint32_t(insts.size());
    auto& renderer = info.roots_renderer;
    auto& renderer_ctx = info.roots_renderer_context;
    renderer.fill_activate(renderer_ctx, debug_drawable.value(), insts.data(), num_inodes);
    params.drawable_needs_update = false;
  }

  update_growth_on_nodes(*this, info);

  spectral_fraction = float(
    lerp(ease::expo_dt_aware(0.99, info.real_dt), double(spectral_fraction), 0.0));

#ifdef GROVE_DEBUG
  if (params.validate_radius_limiter) {
    bounds::validate(info.radius_limiter);
  }
#endif
}

int DebugTreeRootsComponent::num_growing() const {
  int res{};
  for (auto& ctx : global_data.growth_contexts) {
    res += int(ctx.growing.size());
  }
  return res;
}

int DebugTreeRootsComponent::num_receding() const {
  int res{};
  for (auto& ctx : global_data.recede_contexts) {
    res += int(ctx.receding.size());
  }
  return res;
}

int DebugTreeRootsComponent::num_root_aggregates() const {
  return int(global_data.roots.size());
}

float DebugTreeRootsComponent::max_radius() const {
  float mx{0.0f};
  for (auto& roots : global_data.roots) {
    for (int i = 0; i < int(roots.curr_num_nodes); i++) {
      auto radius = roots.nodes[i].target_radius();
      if (radius > mx) {
        mx = radius;
      }
    }
  }
  return mx;
}

bool DebugTreeRootsComponent::is_root_node_radius_constrained(const bounds::RadiusLimiter* lim,
                                                              int ri) const {
  if (ri >= 0 && ri < int(global_data.roots.size())) {
    auto& roots = global_data.roots[ri];
    if (roots.curr_num_nodes > 0) {
      assert(ri < int(global_data.radius_limiter_elements.size()));
      assert(!global_data.radius_limiter_elements[ri].empty());
      auto& handle = global_data.radius_limiter_elements[ri][0];
      if (auto* el = bounds::read_element(lim, handle)) {
        return el->reached_maximum_radius;
      }
    }
  }
  return false;
}

bool DebugTreeRootsComponent::any_root_nodes_radius_constrained(const bounds::RadiusLimiter* lim) const {
  for (int i = 0; i < int(global_data.roots.size()); i++) {
    if (is_root_node_radius_constrained(lim, i)) {
      return true;
    }
  }
  return false;
}

void DebugTreeRootsComponent::set_spectral_fraction(float f01) {
  assert(f01 >= 0.0f && f01 <= 1.0f);
  spectral_fraction = f01;
}

Vec3f DebugTreeRootsComponent::get_attractor_point() const {
  if (debug_attractor_tform) {
    return debug_attractor_tform->get_current().translation;
  } else {
    return Vec3f{};
  }
}

void DebugTreeRootsComponent::set_attractor_point(const Vec3f& ap) {
  if (debug_attractor_tform) {
    auto curr = debug_attractor_tform->get_current();
    curr.translation = ap;
    debug_attractor_tform->set(curr);
  }
}

void DebugTreeRootsComponent::on_gui_update(const ProceduralTreeRootsGUIUpdateResult& res) {
  if (res.make_tree) {
    params.make_tree = res.make_tree.value();
  }
  if (res.diameter_scale) {
    for (auto& node : debug_internodes) {
      node.diameter = node.lateral_q * res.diameter_scale.value();
    }
    params.diameter_scale = res.diameter_scale.value();
    params.drawable_needs_update = true;
  }
  if (res.growth_rate) {
    params.growth_rate = res.growth_rate.value();
  }
  if (res.selected_node_index) {
    params.selected_node_index = res.selected_node_index.value();
  }
  if (res.selected_root_index) {
    params.selected_root_index = res.selected_root_index.value();
  }
  if (res.spawn_axis) {
    params.need_spawn_axis = true;
  }
  if (res.add_roots_at_new_tree_origins) {
    params.add_roots_at_new_tree_origins = res.add_roots_at_new_tree_origins.value();
  }
  if (res.camera_position_attractor) {
    params.camera_position_attractor = res.camera_position_attractor.value();
  }
  if (res.p_spawn_lateral) {
    params.p_spawn_lateral = res.p_spawn_lateral.value();
  }
  if (res.min_axis_length_spawn_lateral) {
    params.min_axis_length_spawn_lateral = res.min_axis_length_spawn_lateral.value();
  }
  if (res.validate_radius_limiter) {
    params.validate_radius_limiter = res.validate_radius_limiter.value();
  }
  if (res.add_roots_at_transform) {
    params.add_roots_at_tform = res.add_roots_at_transform.value();
  }
  if (res.draw_node_frames) {
    params.draw_node_frames = res.draw_node_frames.value();
  }
#if 0
  if (res.create_roots) {
    params.need_create_roots = true;
  }
#endif
  if (res.rand_root_origin_span) {
    params.rand_root_origin_span = res.rand_root_origin_span.value();
  }
  if (res.max_num_nodes_per_roots) {
    params.max_num_nodes_per_roots = clamp(
      res.max_num_nodes_per_roots.value(), 1, max_num_nodes_per_roots());
  }
  if (res.num_roots_create) {
    params.num_roots_create = std::max(1, res.num_roots_create.value());
  }
  if (res.create_short_tree) {
    params.need_create_short_tree = true;
  }
  if (res.default_root_origin) {
    params.default_root_origin = res.default_root_origin.value();
  }
  if (res.attractor_point_scale) {
    params.attractor_point_scale = res.attractor_point_scale.value();
  }
  if (res.allow_recede) {
    params.allow_recede = res.allow_recede.value();
  }
  if (res.leaf_diameter) {
    params.leaf_diameter = res.leaf_diameter.value();
  }
  if (res.diameter_power) {
    params.diameter_power = res.diameter_power.value();
  }
  if (res.node_length) {
    params.node_length = res.node_length.value();
  }
  if (res.deserialize) {
    deserialize_from_file = std::string{GROVE_ASSET_DIR} +
      "/serialized_roots/" + res.deserialize.value();
  }
  if (res.serialize) {
    serialize_to_file = std::string{GROVE_ASSET_DIR} +
      "/serialized_roots/" + res.serialize.value();
  }
  if (res.wind_disabled) {
    params.wind_disabled = res.wind_disabled.value();
  }
  if (res.scale_growth_rate_by_signal) {
    params.scale_growth_rate_by_signal = res.scale_growth_rate_by_signal.value();
  }
  if (res.generate_sample_points) {
    params.need_generate_sample_points_on_nodes = true;
  }
  if (res.points_on_nodes_step_size) {
    params.points_on_nodes_step_size = res.points_on_nodes_step_size.value();
  }
  if (res.points_on_nodes_radius_offset) {
    params.points_on_nodes_radius_offset = res.points_on_nodes_radius_offset.value();
  }
  if (res.points_on_nodes_leaf_diameter) {
    params.points_on_nodes_leaf_diameter = res.points_on_nodes_leaf_diameter.value();
  }
  if (res.points_on_nodes_diameter_power) {
    params.points_on_nodes_diameter_power = res.points_on_nodes_diameter_power.value();
  }
  if (res.points_on_nodes_color) {
    params.points_on_nodes_color = res.points_on_nodes_color.value();
  }
  if (res.smooth_points_on_nodes) {
    params.smooth_points_on_nodes = res.smooth_points_on_nodes.value();
  }
  if (res.points_on_nodes_target_down) {
    bool down = res.points_on_nodes_target_down.value();
    params.points_on_nodes_step_axis = Vec3f{0.0f, 1.0f, 0.0f} * (down ? -1.0f : 1.0f);
  }
  if (res.points_on_nodes_prefer_entry_up_axis) {
    params.points_on_nodes_prefer_entry_up_axis = res.points_on_nodes_prefer_entry_up_axis.value();
  }
  if (res.set_points_on_nodes_preset1) {
    set_sample_points_on_nodes_preset1(*this);
  }
  if (res.material1_colors) {
    auto& cs = res.material1_colors.value();
    params.material1_colors.c0 = cs.c0;
    params.material1_colors.c1 = cs.c1;
    params.material1_colors.c2 = cs.c2;
    params.material1_colors.c3 = cs.c3;
  }
  if (res.need_fit_bounds_around_axis) {
    params.need_fit_bounds_around_axis = true;
  }
  if (res.draw_cube_grid) {
    params.draw_cube_grid = res.draw_cube_grid.value();
  }
  if (res.debug_draw_enabled) {
    params.debug_draw_enabled = res.debug_draw_enabled.value();
  }
  if (res.prefer_global_p_spawn_lateral) {
    params.prefer_global_p_spawn_lateral = res.prefer_global_p_spawn_lateral.value();
  }
}

GROVE_NAMESPACE_END
