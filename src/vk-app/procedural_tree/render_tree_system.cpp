#include "render_tree_system.hpp"
#include "utility.hpp"
#include "render.hpp"
#include "../render/foliage_drawable_components.hpp"
#include "../render/branch_node_drawable_components.hpp"
#include "../render/frustum_cull_data.hpp"
#include "../render/render_branch_nodes.hpp"
#include "../render/render_branch_nodes_types.hpp"
#include "../procedural_tree/fit_bounds.hpp"
#include "grove/math/ease.hpp"
#include "grove/math/util.hpp"
#include "grove/math/random.hpp"
#include "grove/audio/envelope.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/common/profile.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/logging.hpp"

#define ENABLE_OPTIM_DRAWABLE_DESTRUCTION (1)
#define MAX_NUM_DRAWABLES_DESTROY_PER_FRAME (1)

GROVE_NAMESPACE_BEGIN

namespace tree {

struct LeafSeasonChange {
  float current() const {
    float f = ease::in_out_quart(frac_target);
    return target_frac_fall == 0.0f ? 1.0f - f : f;
  }

  float frac_target{1.0f};
  float target_frac_fall{};
};

struct RenderTreeSystemInstance {
  TreeInstanceHandle tree;
  bounds::AccelInstanceHandle query_accel;
  BranchNodeDrawableComponents branch_node_drawable_components;
  Optional<cull::FrustumCullGroupHandle> branch_nodes_cull_group_handle;

  Optional<CreateRenderFoliageParams> create_foliage_components;
  Optional<foliage::FoliageDrawableComponents> foliage_drawable_components;
  bool enable_branch_node_drawable_components{};

  Optional<bool> set_hidden;
  Optional<int> set_foliage_components_lod;

  RenderTreeSystemLeafGrowthContext leaf_growth_context;
  audio::ExpInterpolated<float> global_leaf_scale;
  Vec2f static_leaves_uv_offset;
  LeafSeasonChange leaf_season_change{};
  bool marked_for_destruction;
  bool prepare_to_grow;
  bool need_create_drawables;
  bool can_create_drawables;
  bool need_update_branch_static_data;
  bool need_update_branch_dynamic_data;
  bool need_update_branch_nodes_dynamic_data;
  bool need_set_leaf_scale_fraction;
  RenderTreeSystemEvents events;
};

struct RenderTreeSystem {
  uint32_t next_instance_id{1};
  std::unordered_map<uint32_t, RenderTreeSystemInstance> instances;

  bounds::AccessorID bounds_accessor_id{bounds::AccessorID::create()};
  tree::Internodes temporary_internodes;
  uint32_t num_drawables_created_this_frame{};

  foliage::TreeLeavesPoolAllocator tree_leaves_pool_alloc;
  std::unordered_set<RenderTreeInstanceHandle, RenderTreeInstanceHandle::Hash> pending_deletion;

  int foliage_lod{};

  double ms_spent_deleting_foliage{};
  double ms_spent_deleting_branches{};
  double max_ms_spent_deleting_foliage{};
  double max_ms_spent_deleting_branches{};
  uint32_t num_drawables_destroyed_this_frame{};
  uint32_t max_num_drawables_destroyed_in_one_frame{};
};

} //  tree

namespace {

using namespace tree;

using Instance = RenderTreeSystemInstance;
using InitInfo = RenderTreeSystemInitInfo;
using UpdateInfo = RenderTreeSystemUpdateInfo;
using LeafGrowthContext = RenderTreeSystemLeafGrowthContext;
using TreeInstance = TreeSystem::ReadInstance;

struct Config {
  static constexpr double reference_dt = 1.0 / 60.0;
  static constexpr float leaf_growth_incr = 0.01f;
};

[[maybe_unused]] constexpr const char* logging_id() {
  return "RenderTreeSystem";
}

template <typename System, typename Instance>
Instance* find_instance_impl(System* sys, RenderTreeInstanceHandle handle) {
  if (auto it = sys->instances.find(handle.id); it != sys->instances.end()) {
    return &it->second;
  } else {
    return nullptr;
  }
}

Instance* find_instance(RenderTreeSystem* sys, RenderTreeInstanceHandle handle) {
  return find_instance_impl<RenderTreeSystem, Instance>(sys, handle);
}

const Instance* find_instance(const RenderTreeSystem* sys, RenderTreeInstanceHandle handle) {
  return find_instance_impl<const RenderTreeSystem, const Instance>(sys, handle);
}

float current_scale(const LeafGrowthContext& context) {
  return lerp(ease::in_out_quart(context.t), context.scale0, context.scale1);
}

bool finished_growing(const LeafGrowthContext& context) {
  return context.t >= 1.0f;
}

void set_target_scale(LeafGrowthContext* context, float target) {
  auto curr_scale = current_scale(*context);
  context->t = 0.0f;
  context->scale0 = curr_scale;
  context->scale1 = target;
}

bool need_set_update_branch_static_data(const TreeInstance& inst) {
  using Modifying = tree::TreeSystem::ModifyingState;
  auto& modifying = inst.growth_state.modifying;
  return modifying == Modifying::RenderDying || modifying == Modifying::Pruning;
}

void process_events(Instance& render_inst, const TreeInstance& tree_inst) {
  if (tree_inst.events.node_structure_modified ||
      tree_inst.events.just_started_awaiting_finish_pruning_signal) {
    render_inst.need_create_drawables = true;
  }

  if (tree_inst.events.node_render_position_modified &&
      !tree_inst.events.just_started_render_growing) {
    render_inst.need_update_branch_dynamic_data = true;
    render_inst.need_update_branch_nodes_dynamic_data = true;
    if (need_set_update_branch_static_data(tree_inst)) {
      render_inst.need_update_branch_static_data = true;
    }
  }
}

OBB3f distribute_bounds_outwards(const Internode& node, const Bounds3f& nodes_aabb,
                                 const Vec3f& bounds_scale, const Vec3f& bounds_offset) {
  auto leaf_dir = node.position - nodes_aabb.center();
  auto leaf_dir_xz = Vec3f{leaf_dir.x, 0.0f, leaf_dir.z};
  leaf_dir_xz = normalize_or_default(leaf_dir_xz, Vec3f{1.0f, 0.0f, 0.0f});
  auto leaf_p = node.position + leaf_dir_xz * bounds_offset;
  return OBB3f::axis_aligned(leaf_p, bounds_scale);
}

OBB3f get_leaf_bounds(const Internode& node, const Bounds3f& nodes_aabb,
                      const Vec3f& bounds_scale, const Vec3f& bounds_offset,
                      TreeSystemLeafBoundsDistributionStrategy distrib_strategy) {
  switch (distrib_strategy) {
    case tree::TreeSystemLeafBoundsDistributionStrategy::Original:
      return tree::internode_relative_obb(node, bounds_scale, bounds_offset);

    case tree::TreeSystemLeafBoundsDistributionStrategy::AxisAlignedOutwardsFromNodes:
      return distribute_bounds_outwards(node, nodes_aabb, bounds_scale, bounds_offset);

    default: {
      assert(false);
      return tree::internode_relative_obb(node, bounds_scale, bounds_offset);
    }
  }
}

std::vector<const Internode*>
select_leaf_internodes(const Internodes& nodes, const Bounds3f& nodes_aabb,
                       const bounds::Accel* accel,
                       bounds::ElementTag tree_tag, bounds::ElementTag leaf_tag,
                       const Vec3f& bounds_scale, const Vec3f& bounds_offset,
                       TreeSystemLeafBoundsDistributionStrategy distrib_strategy) {
  std::vector<const tree::Internode*> leaf_ptrs;
  std::vector<const bounds::Element*> isect;
  for (auto& node : nodes) {
    if (node.is_leaf()) {
      auto node_bounds = get_leaf_bounds(
        node, nodes_aabb, bounds_scale, bounds_offset, distrib_strategy);
      accel->intersects(bounds::make_query_element(node_bounds), isect);
      bool accept = true;
      for (const bounds::Element* el : isect) {
        if (el->tag != tree_tag.id && el->tag != leaf_tag.id) {
          accept = false;
          break;
        }
      }
      if (accept) {
        leaf_ptrs.push_back(&node);
      }
      isect.clear();
    }
  }
  return leaf_ptrs;
}

std::vector<int> to_leaf_internode_indices(const std::vector<const tree::Internode*>& ptrs,
                                           const tree::Internode* p) {
  std::vector<int> result(ptrs.size());
  for (int i = 0; i < int(ptrs.size()); i++) {
    result[i] = int(ptrs[i] - p);
  }
  return result;
}

void set_zero_diameter(Internodes& inodes) {
  for (auto& node : inodes) {
    node.diameter = 0.0f;
  }
}

Bounds3f require_internode_bounds(const Bounds3f* maybe_src_bounds, const Internodes& inodes) {
  return maybe_src_bounds ? *maybe_src_bounds : internode_aabb(inodes);
}

void maybe_destroy_drawables(RenderTreeSystem& sys, Instance& render_inst, const UpdateInfo& info) {
  Stopwatch t0;
  if (render_inst.foliage_drawable_components) {
    t0.reset();
    foliage::destroy_foliage_drawable_components(
      &render_inst.foliage_drawable_components.value(),
      info.tree_leaves_frustum_cull_data, info.foliage_occlusion_system,
      &sys.tree_leaves_pool_alloc);
    render_inst.foliage_drawable_components = NullOpt{};
    sys.ms_spent_deleting_foliage += t0.delta().count() * 1e3;
  }

  t0.reset();
  tree::destroy_branch_node_drawable_components(
    info.render_branch_nodes_data, &render_inst.branch_node_drawable_components);

  if (render_inst.branch_nodes_cull_group_handle) {
    cull::destroy_frustum_cull_instance_group(
      info.branch_nodes_frustum_cull_data, render_inst.branch_nodes_cull_group_handle.value());
  }

  sys.ms_spent_deleting_branches += t0.delta().count() * 1e3;
  sys.num_drawables_destroyed_this_frame++;
}

struct CreateBranchNodesCullGroupParams {
  static CreateBranchNodesCullGroupParams make_default() {
    CreateBranchNodesCullGroupParams result{};
    result.fit_min_medial = 4;
    result.fit_max_medial = 4;
    result.fit_xz_thresh = 2.0f;
    return result;
  }

  int fit_min_medial;
  int fit_max_medial;
  float fit_xz_thresh;
};

Optional<cull::FrustumCullGroupHandle> create_branch_nodes_cull_group(
  WindBranchNodeDrawableHandle drawable, const Internodes& inodes,
  const CreateBranchNodesCullGroupParams& params,
  tree::RenderBranchNodesData* rd, cull::FrustumCullData* cull_data) {
  //
  if (inodes.empty()) {
    return NullOpt{};
  }

  Temporary<Mat3f, 2048> store_frames;
  Temporary<int, 2048> store_bounds_indices;
  Temporary<Bounds3f, 2048> store_bounds;

  const auto* nodes = inodes.data();
  const int num_nodes = int(inodes.size());

  Mat3f* frames = store_frames.require(num_nodes);
  int* bounds_indices = store_bounds_indices.require(num_nodes);
  Bounds3f* bounds = store_bounds.require(num_nodes);

  const int num_fit = bounds::fit_aabbs_around_axes_radius_threshold_method(
    nodes, frames, num_nodes,
    params.fit_min_medial, params.fit_max_medial, params.fit_xz_thresh, bounds, bounds_indices);
  assert(num_fit > 0);

  Temporary<cull::FrustumCullInstanceDescriptor, 2048> cull_descs;
  auto* descs = cull_descs.require(num_fit);
  for (int i = 0; i < num_fit; i++) {
    auto& desc = descs[i];
    desc = {};
    desc.aabb_p0 = bounds[i].min;
    desc.aabb_p1 = bounds[i].max;
  }

  auto cull_group = cull::create_frustum_cull_instance_group(cull_data, descs, num_fit);
  auto lod_data = tree::get_branch_nodes_lod_data(rd, drawable);
  assert(lod_data.size() == num_nodes);

  assert(num_fit < int(0xffffu) && cull_group.group_index + 1 < 0xffffu);
  for (int i = 0; i < num_nodes; i++) {
    const auto cull_group_ind_one_based = uint16_t(cull_group.group_index + 1);
    const auto cull_inst = uint16_t(bounds_indices[i]);

    auto& lod_inst = lod_data[i];
    lod_inst.set_is_active(true);
    lod_inst.set_one_based_cull_group_and_zero_based_instance(cull_group_ind_one_based, cull_inst);
  }

  tree::set_branch_nodes_lod_data_modified(rd, drawable);
  return Optional<cull::FrustumCullGroupHandle>(cull_group);
}

bool require_drawables(RenderTreeSystem* sys, Instance& render_inst,
                       const TreeInstance& tree_inst, const UpdateInfo& info,
                       bool prepare_to_render_grow) {
  if (!tree_inst.nodes) {
    return false;
  }
  const auto* accel = bounds::request_read(
    info.bounds_system, render_inst.query_accel, sys->bounds_accessor_id);
  if (!accel) {
    return false;
  }
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("RenderTreeSystem/require_drawables");
  (void) profiler;
#ifdef GROVE_DEBUG
  {
    std::string log_msg{"Making drawables for: "};
    log_msg += std::to_string(render_inst.tree.id);
    GROVE_LOG_INFO_CAPTURE_META(log_msg.c_str(), logging_id());
  }
#endif

  maybe_destroy_drawables(*sys, render_inst, info);

  //  @NOTE: By selecting `src_aabb` over the true bounding box, the influence of wind becomes
  //  attenuated for pruned trees. This is necessary right now to avoid a visual discontinuity,
  //  but a more complicated approach would be to target the wind influence -> 0, remake
  //  the drawable, then target the wind influence back to its original value.
  const auto& internodes = tree_inst.nodes->internodes;
  const Bounds3f internode_aabb = require_internode_bounds(tree_inst.src_aabb, internodes);
  auto axis_root_info = tree::compute_axis_root_info(internodes);
  auto remapped_roots = tree::remap_axis_roots(internodes);

  if (render_inst.enable_branch_node_drawable_components) {
    render_inst.branch_node_drawable_components =
      tree::create_wind_branch_node_drawable_components_from_internodes(
        info.render_branch_nodes_data, internodes, internode_aabb, axis_root_info, remapped_roots);

#if 1
    if (render_inst.branch_node_drawable_components.wind_drawable) {
      render_inst.branch_nodes_cull_group_handle = create_branch_nodes_cull_group(
        render_inst.branch_node_drawable_components.wind_drawable.value(),
        internodes,
        CreateBranchNodesCullGroupParams::make_default(),
        info.render_branch_nodes_data, info.branch_nodes_frustum_cull_data);
    }
#endif
  }

  const auto leaf_ptrs = select_leaf_internodes(
    internodes, internode_aabb,
    accel,
    tree::get_bounds_tree_element_tag(info.tree_system),
    tree::get_bounds_leaf_element_tag(info.tree_system),
    tree_inst.leaves->internode_bounds_scale,
    tree_inst.leaves->internode_bounds_offset,
    tree_inst.leaves->bounds_distribution_strategy);
  bounds::release_read(
    info.bounds_system, render_inst.query_accel, sys->bounds_accessor_id);

  const float static_uv_offset = urandf();
  render_inst.static_leaves_uv_offset = Vec2f{static_uv_offset};

  if (render_inst.create_foliage_components) {
    auto& create_info = render_inst.create_foliage_components.value();

    auto distrib_strategy = foliage::FoliageDistributionStrategy::TightHighN;
    uint16_t alpha_image_index{};
    uint16_t color_image0_index{1};

    //  @TODO: Alpha and color image indices are defined by the order in which images are
    //  loaded in `render_tree_leaves_gpu.cpp`. Setting an out-of-bounds or incorrect image index
    //  here should be "fine" in the sense that the renderer will validate the indices given to it,
    //  but it'd be better not to have this implicit link between these systems.
    switch (create_info.leaves_type) {
      case CreateRenderFoliageParams::LeavesType::Maple:
        break;
      case CreateRenderFoliageParams::LeavesType::Willow:
        distrib_strategy = foliage::FoliageDistributionStrategy::Hanging;
        alpha_image_index = 2;
        break;
      case CreateRenderFoliageParams::LeavesType::ThinCurled:
        distrib_strategy = foliage::FoliageDistributionStrategy::ThinCurledLowN;
        alpha_image_index = 2;
//        color_image_index = 3;
        break;
      case CreateRenderFoliageParams::LeavesType::Broad:
        alpha_image_index = 3;
        break;
    }

    const uint16_t im_inds[3]{2, 3, 4};
//    uint16_t color_image1_index = urandf() < 0.5f ? 2 : 3;
    uint16_t color_image1_index = *uniform_array_sample(im_inds, 3);

    foliage::CreateFoliageDrawableComponentParams create_params{};
    create_params.distribution_strategy = distrib_strategy;
    create_params.initial_scale01 = 0.0f;
    create_params.alpha_image_index = alpha_image_index;
    create_params.color_image0_index = color_image0_index;
    create_params.color_image1_index = color_image1_index;
    create_params.uv_offset = static_uv_offset;
    create_params.color_image_mix01 = render_inst.leaf_season_change.current();
    create_params.preferred_lod = sys->foliage_lod;
    render_inst.foliage_drawable_components =
      foliage::create_foliage_drawable_components_from_internodes(
        info.tree_leaves_frustum_cull_data,
        info.foliage_occlusion_system, &sys->tree_leaves_pool_alloc, create_params,
        internodes, to_leaf_internode_indices(leaf_ptrs, internodes.data()));
  }

  //  @TODO: Avoid this copy.
  auto& tmp_internodes = sys->temporary_internodes;
  tmp_internodes.resize(internodes.size());
  std::copy(internodes.begin(), internodes.end(), tmp_internodes.begin());
  if (prepare_to_render_grow) {
    tree::set_render_length_scale(tmp_internodes, 0, 0.0f);
    set_zero_diameter(tmp_internodes);
  }

  tree::set_position_and_radii_from_internodes(
    info.render_branch_nodes_data, render_inst.branch_node_drawable_components, tmp_internodes);

  return true;
}

#define REQUIRE_MAX_ONE_DRAWABLE_PER_FRAME (1)

void maybe_require_drawables(RenderTreeSystem* sys, Instance& render_inst,
                             const TreeInstance& tree_inst, const UpdateInfo& info) {
  if (!render_inst.need_create_drawables || !render_inst.can_create_drawables) {
    return;
  }
#if REQUIRE_MAX_ONE_DRAWABLE_PER_FRAME
  if (sys->num_drawables_created_this_frame > 0) {
    return;
  }
#endif
#if ENABLE_OPTIM_DRAWABLE_DESTRUCTION
  if (!sys->pending_deletion.empty()) {
    return;
  }
#endif
  const bool prepare_to_grow = render_inst.prepare_to_grow;
  if (require_drawables(sys, render_inst, tree_inst, info, prepare_to_grow)) {
    sys->num_drawables_created_this_frame++;
    render_inst.prepare_to_grow = false;
    render_inst.need_create_drawables = false;
    render_inst.can_create_drawables = false;
    render_inst.need_update_branch_static_data = false;
    render_inst.need_update_branch_dynamic_data = false;
    render_inst.need_update_branch_nodes_dynamic_data = false;
    render_inst.events.just_created_drawables = true;
  }
}

void maybe_update_branch_data(Instance& render_inst, const TreeInstance& tree_inst,
                              const UpdateInfo& info) {
  if (render_inst.need_create_drawables || !tree_inst.nodes) {
    return;
  }

  const auto& internodes = tree_inst.nodes->internodes;
  if (render_inst.need_update_branch_nodes_dynamic_data) {
    tree::set_position_and_radii_from_internodes(
      info.render_branch_nodes_data, render_inst.branch_node_drawable_components, internodes);
    render_inst.need_update_branch_nodes_dynamic_data = false;
  }
}

bool tick_leaf_render_growth(LeafGrowthContext& context, double real_dt) {
  float growth_incr = Config::leaf_growth_incr * float(real_dt / Config::reference_dt);
  context.t += growth_incr;
  if (context.t >= 1.0f) {
    context.t = 1.0f;
    return true;
  } else {
    return false;
  }
}

float to_tree_leaves_scale_fraction(const LeafGrowthContext* leaf_gc) {
  float scale01;
  if (leaf_gc->t == 1.0f) {
    scale01 = leaf_gc->scale1 == 0.0f ? 0.0f : 1.0f;
  } else {
    scale01 = leaf_gc->t;
    if (leaf_gc->scale1 < leaf_gc->scale0) {
      scale01 = 1.0f - scale01;
    }
  }
  return ease::in_out_quart(scale01);
}

void update_leaf_growth(Instance& render_inst, const UpdateInfo& info) {
  if (finished_growing(render_inst.leaf_growth_context)) {
    return;
  }

  if (tick_leaf_render_growth(render_inst.leaf_growth_context, info.real_dt)) {
    render_inst.events.just_reached_leaf_target_scale = true;
  }

  render_inst.need_set_leaf_scale_fraction = true;
}

void update_global_leaf_scale(Instance& render_inst, const UpdateInfo& info) {
  if (!render_inst.global_leaf_scale.reached_target()) {
    render_inst.global_leaf_scale.tick(1.0f / std::max(1e-3f, float(info.real_dt)));
    render_inst.global_leaf_scale.reach_target_if(1e-3f);
    render_inst.need_set_leaf_scale_fraction = true;
  }
}

void update_leaf_scale_fraction(Instance& render_inst, const UpdateInfo&) {
  if (render_inst.need_set_leaf_scale_fraction && render_inst.foliage_drawable_components) {
    const float sf = to_tree_leaves_scale_fraction(&render_inst.leaf_growth_context);
    const float gf = render_inst.global_leaf_scale.current;
    render_inst.foliage_drawable_components.value().set_scale_fraction(sf * gf);
    render_inst.need_set_leaf_scale_fraction = false;
  }
}

void update_static_leaf_uvs(Instance& render_inst, const UpdateInfo& info) {
  auto& uv_off = render_inst.static_leaves_uv_offset;
  if (uv_off.y == uv_off.x) {
    return;
  }

  const auto t = float(1.0 - std::pow(0.25, info.real_dt));
  uv_off.x = lerp(t, uv_off.x, uv_off.y);

  if (render_inst.foliage_drawable_components) {
    render_inst.foliage_drawable_components.value().set_uv_offset(uv_off.x);
  }
}

void update_leaf_season_change(Instance& render_inst, const UpdateInfo& info) {
  auto& season_info = render_inst.leaf_season_change;
  if (season_info.frac_target == 1.0f) {
    return;
  }

  season_info.frac_target += 1e-2f * float(info.real_dt / Config::reference_dt);
  if (season_info.frac_target >= 1.0f) {
    season_info.frac_target = 1.0f;
    render_inst.events.just_reached_leaf_season_change_target = true;
  }

  if (render_inst.foliage_drawable_components) {
    auto& comp = render_inst.foliage_drawable_components.value();
    comp.set_color_mix_fraction(season_info.current());
  }
}

void update_set_hidden(Instance& render_inst, const UpdateInfo&) {
  if (!render_inst.set_hidden) {
    return;
  }

  if (render_inst.foliage_drawable_components) {
    render_inst.foliage_drawable_components.value().set_hidden(render_inst.set_hidden.value());
    render_inst.set_hidden = NullOpt{};
  }
}

void update_set_foliage_lod(Instance& render_inst, const UpdateInfo&) {
  if (!render_inst.set_foliage_components_lod) {
    return;
  }

  if (render_inst.foliage_drawable_components) {
    const int lod = render_inst.set_foliage_components_lod.value();
    render_inst.foliage_drawable_components.value().set_lod(lod);
    render_inst.set_foliage_components_lod = NullOpt{};
  }
}

void update_pending_deletion(RenderTreeSystem* sys, const UpdateInfo& info) {
#if ENABLE_OPTIM_DRAWABLE_DESTRUCTION
  while (!sys->pending_deletion.empty() &&
         sys->num_drawables_destroyed_this_frame < MAX_NUM_DRAWABLES_DESTROY_PER_FRAME) {
    auto del_it = sys->pending_deletion.begin();
    const RenderTreeInstanceHandle handle = *del_it;
    sys->pending_deletion.erase(del_it);

    auto it = sys->instances.find(handle.id);
    if (it != sys->instances.end()) {
      maybe_destroy_drawables(*sys, it->second, info);
      sys->instances.erase(it);
    } else {
      assert(false);
    }
  }
#else
  for (RenderTreeInstanceHandle handle : sys->pending_deletion) {
    auto it = sys->instances.find(handle.id);
    if (it != sys->instances.end()) {
      maybe_destroy_drawables(*sys, it->second, info);
      sys->instances.erase(it);
    } else {
      assert(false);
    }
  }
  sys->pending_deletion.clear();
#endif
}

Instance make_instance(const CreateRenderTreeInstanceParams& params) {
  assert(params.tree.is_valid());
  assert(params.query_accel.is_valid());

  Instance result{};
  result.tree = params.tree;
  result.query_accel = params.query_accel;
  result.prepare_to_grow = true;

  result.global_leaf_scale.current = 1.0f;
  result.global_leaf_scale.target = 1.0f;
  result.global_leaf_scale.set_time_constant95(0.5f);

  if (params.create_foliage_components) {
    auto& p = params.create_foliage_components.value();
    if (p.init_with_fall_colors) {
      result.leaf_season_change.target_frac_fall = 1.0f;
    }
    if (p.init_with_zero_global_scale) {
      result.global_leaf_scale.current = 0.0f;
      result.global_leaf_scale.target = 0.0f;
    }
    result.create_foliage_components = std::move(p);
  }

  result.enable_branch_node_drawable_components = params.enable_branch_nodes_drawable_components;
  return result;
}

} //  anon

void tree::require_drawables(RenderTreeSystem* sys, RenderTreeInstanceHandle instance) {
  if (auto* inst = find_instance(sys, instance)) {
    inst->can_create_drawables = true;
  } else {
    assert(false);
  }
}

void tree::set_leaf_scale_target(RenderTreeSystem* sys, RenderTreeInstanceHandle instance,
                                 float target) {
  if (auto* inst = find_instance(sys, instance)) {
    set_target_scale(&inst->leaf_growth_context, target);
  } else {
    assert(false);
  }
}

void tree::set_leaf_global_scale_fraction(RenderTreeSystem* sys, RenderTreeInstanceHandle instance,
                                          float scale01) {
  assert(scale01 >= 0.0f && scale01 <= 1.0f);
  if (auto* inst = find_instance(sys, instance)) {
    inst->global_leaf_scale.target = scale01;
  } else {
    assert(false);
  }
}

void tree::set_static_leaf_uv_offset_target(RenderTreeSystem* sys, RenderTreeInstanceHandle instance,
                                            float off) {
  if (auto* inst = find_instance(sys, instance)) {
    inst->static_leaves_uv_offset.y = off;
  } else {
    assert(false);
  }
}

void tree::set_frac_fall_target(RenderTreeSystem* sys, RenderTreeInstanceHandle instance, float target) {
  if (auto* inst = find_instance(sys, instance)) {
    if (inst->leaf_season_change.target_frac_fall != target) {
      inst->leaf_season_change.target_frac_fall = target;
      inst->leaf_season_change.frac_target = 0.0f;
    }
  } else {
    assert(false);
  }
}

void tree::maybe_set_preferred_foliage_lod(RenderTreeSystem* sys, int lod) {
  if (lod >= 0 && lod <= 1) {
    for (auto& [_, inst] : sys->instances) {
      inst.set_foliage_components_lod = lod;
    }
    sys->foliage_lod = lod;
  }
}

int tree::get_preferred_foliage_lod(const RenderTreeSystem* sys) {
  return sys->foliage_lod;
}

void tree::increment_static_leaf_uv_osc_time(RenderTreeSystem* sys,
                                             RenderTreeInstanceHandle instance, float dt) {
  auto* inst = find_instance(sys, instance);
  if (!inst) {
    assert(false);
    return;
  }
  if (inst->foliage_drawable_components) {
    inst->foliage_drawable_components.value().increment_uv_osc_time(dt);
  }
}

RenderTreeInstanceHandle tree::create_instance(RenderTreeSystem* sys,
                                               const CreateRenderTreeInstanceParams& params) {
  uint32_t id{sys->next_instance_id++};
  RenderTreeInstanceHandle handle{id};
  sys->instances[id] = make_instance(params);
  return handle;
}

void tree::destroy_instance(RenderTreeSystem* sys, RenderTreeInstanceHandle instance) {
  auto inst_it = sys->instances.find(instance.id);
  if (inst_it != sys->instances.end()) {
    assert(!inst_it->second.marked_for_destruction);
    inst_it->second.marked_for_destruction = true;
  } else {
    assert(false);
  }
  sys->pending_deletion.insert(instance);
}

ReadRenderTreeSystemInstance tree::read_instance(const RenderTreeSystem* sys,
                                                 RenderTreeInstanceHandle handle) {
  ReadRenderTreeSystemInstance result{};
  if (auto* inst = find_instance(sys, handle)) {
    result.events = inst->events;
  } else {
    assert(false);
  }
  return result;
}

const RenderTreeSystemLeafGrowthContext*
tree::read_leaf_growth_context(const RenderTreeSystem* sys, RenderTreeInstanceHandle handle) {
  if (auto* inst = find_instance(sys, handle)) {
    return &inst->leaf_growth_context;
  } else {
    assert(false);
    return nullptr;
  }
}

float tree::read_current_static_leaves_uv_offset(const RenderTreeSystem* sys,
                                                 RenderTreeInstanceHandle handle) {
  if (auto* inst = find_instance(sys, handle)) {
    return inst->static_leaves_uv_offset.x;
  } else {
    assert(false);
    return 0.0f;
  }
}

void tree::set_hidden(RenderTreeSystem* sys, RenderTreeInstanceHandle handle, bool hide) {
  if (auto* inst = find_instance(sys, handle)) {
    inst->set_hidden = hide;
  } else {
    assert(false);
  }
}

void tree::set_all_hidden(RenderTreeSystem* sys, bool hide) {
  for (auto& [_, inst] : sys->instances) {
    inst.set_hidden = hide;
  }
}

RenderTreeSystemUpdateResult tree::update(RenderTreeSystem* sys, const UpdateInfo& info) {
  RenderTreeSystemUpdateResult result{};

  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("RenderTreeSystem/update");
  (void) profiler;

  sys->num_drawables_created_this_frame = 0;
  sys->num_drawables_destroyed_this_frame = 0;
  sys->ms_spent_deleting_branches = 0;
  sys->ms_spent_deleting_foliage = 0;

  for (auto& [_, inst] : sys->instances) {
    inst.events = {};
  }

  update_pending_deletion(sys, info);

  for (auto& [_, render_inst] : sys->instances) {
    if (render_inst.marked_for_destruction) {
      continue;
    }
    auto tree_inst = read_tree(info.tree_system, render_inst.tree);
    process_events(render_inst, tree_inst);
    maybe_require_drawables(sys, render_inst, tree_inst, info);
    maybe_update_branch_data(render_inst, tree_inst, info);
    update_leaf_growth(render_inst, info);
    update_global_leaf_scale(render_inst, info);
    update_leaf_scale_fraction(render_inst, info);
    update_static_leaf_uvs(render_inst, info);
    update_leaf_season_change(render_inst, info);
    update_set_hidden(render_inst, info);
    update_set_foliage_lod(render_inst, info);
    result.num_just_reached_leaf_season_change_target += int(
      render_inst.events.just_reached_leaf_season_change_target);
  }

  sys->max_ms_spent_deleting_branches = std::max
    (sys->max_ms_spent_deleting_branches, sys->ms_spent_deleting_branches);
  sys->max_ms_spent_deleting_foliage = std::max(
    sys->max_ms_spent_deleting_foliage, sys->ms_spent_deleting_foliage);
  sys->max_num_drawables_destroyed_in_one_frame = std::max(
    sys->max_num_drawables_destroyed_in_one_frame, sys->num_drawables_destroyed_this_frame);

  return result;
}

RenderTreeSystemStats tree::get_stats(const RenderTreeSystem* sys) {
  RenderTreeSystemStats result{};
  result.max_ms_spent_deleting_branches = sys->max_ms_spent_deleting_branches;
  result.max_ms_spent_deleting_foliage = sys->max_ms_spent_deleting_foliage;
  result.max_num_drawables_destroyed_in_one_frame = sys->max_num_drawables_destroyed_in_one_frame;
  return result;
}

Optional<RenderTreeInstanceHandle>
tree::debug::get_ith_instance(const RenderTreeSystem* sys, int i) {
  if (i < 0 || i >= int(sys->instances.size())) {
    return NullOpt{};
  }

  int idx{};
  for (auto& [handle, _] : sys->instances) {
    if (idx++ == i) {
      return Optional<RenderTreeInstanceHandle>(RenderTreeInstanceHandle{handle});
    }
  }

  assert(false);
  return NullOpt{};
}

void tree::initialize(RenderTreeSystem*, const InitInfo&) {
  //
}

RenderTreeSystem* tree::create_render_tree_system() {
  return new RenderTreeSystem();
}

void tree::destroy_render_tree_system(RenderTreeSystem** sys) {
  delete *sys;
  *sys = nullptr;
}

GROVE_NAMESPACE_END
