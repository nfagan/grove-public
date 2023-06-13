#include "foliage_drawable_components.hpp"
#include "../procedural_tree/distribute_foliage_outwards_from_nodes.hpp"
#include "../procedural_tree/fit_bounds.hpp"
#include "../procedural_tree/render.hpp"
#include "../procedural_tree/utility.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;
using namespace foliage;

struct Config {
  static constexpr uint32_t occlusion_cluster_create_interval = 2;
  static constexpr Vec2f lod_fadeout_distances{115.0f, 125.0f};
//  static constexpr Vec2f lod_distance_limits{64.0f, 72.0f};
  static constexpr uint32_t leaf_pool_size = 64;
};

struct RenderTreeLeavesInstanceMeta {
  PackedWindAxisRootInfo packed_wind_axis_root_info;
};

uint32_t make_frustum_cull_instance_descs(const FoliageDistributionEntry* entries, uint32_t num_entries,
                                          uint32_t num_steps, uint32_t num_instances_per_step,
                                          float global_scale, cull::FrustumCullInstanceDescriptor* dst_descs) {
  const auto instances_per_cluster = uint32_t(num_steps * num_instances_per_step);
  assert((num_entries % instances_per_cluster) == 0);
  const auto num_clusters = uint32_t(num_entries / instances_per_cluster);

  uint32_t num_descs{};
  for (uint32_t i = 0; i < num_clusters; i++) {
    const uint32_t cluster_off = i * instances_per_cluster;

    //  One frustum cull instance per step.
    for (uint32_t j = 0; j < num_steps; j++) {
      const uint32_t inst_off = cluster_off + j * num_instances_per_step;
      const auto& src_entry = entries[inst_off];
      auto& dst_inst = dst_descs[num_descs++];

      dst_inst.aabb_p0 = src_entry.translation - global_scale;
      dst_inst.aabb_p1 = src_entry.translation + global_scale;
#ifdef GROVE_DEBUG
      //  All entries belonging to this cull instance should have the same bounds, defined
      //  by the same translation and global scale.
      for (uint32_t k = 0; k < num_instances_per_step; k++) {
        auto& query_inst = entries[inst_off + k];
        assert(query_inst.translation == src_entry.translation);
      }
#endif
    }
  }

  return num_descs;
}

uint32_t make_foliage_occlusion_cluster_descriptors(const FoliageDistributionEntry* distribution_entries,
                                                    uint32_t num_entries, uint32_t num_steps,
                                                    uint32_t num_instances_per_step,
                                                    uint32_t cluster_create_interval, float global_scale,
                                                    foliage_occlusion::ClusterDescriptor* dst_cluster_descs) {
  const auto instances_per_cluster = uint32_t(num_steps * num_instances_per_step);
  assert((num_entries % instances_per_cluster) == 0);
  assert(uint32_t(num_steps) <= foliage_occlusion::ClusterDescriptor::max_num_instances);

  const auto num_clusters = num_entries / instances_per_cluster;
  cluster_create_interval = std::max(1u, cluster_create_interval);

  Temporary<OBB3f, 32> store_instance_bounds;
  OBB3f* instance_bounds = store_instance_bounds.require(int(num_steps));

  uint32_t num_clusters_created{};
  for (uint32_t i = 0; i < num_clusters; i++) {
    if ((i % cluster_create_interval) != 0) {
      continue;
    }

    foliage_occlusion::ClusterDescriptor cluster_desc{};

    const uint32_t base_inst_off = i * instances_per_cluster;
    for (uint32_t j = 0; j < num_steps; j++) {
      const uint32_t instance_off = base_inst_off + j * num_instances_per_step;
      auto& src_entry = distribution_entries[instance_off];

      Vec3f f = src_entry.forwards_dir;
      Vec3f r = src_entry.right_dir;
      auto up = normalize(cross(f, r));
      r = cross(up, f);

      OBB3f bounds;
      bounds.i = r;
      bounds.j = up;
      bounds.k = f;
      bounds.position = src_entry.translation;
      bounds.half_size = Vec3f{global_scale, 0.125f, global_scale};

      foliage_occlusion::ClusterInstanceDescriptor instance_desc{};
      instance_desc.s = Vec2f{global_scale};
      instance_desc.p = src_entry.translation;
      instance_desc.x = r;
      instance_desc.n = up;
      instance_desc.associated_render_instance = instance_off;
      cluster_desc.instances[cluster_desc.num_instances] = instance_desc;
      instance_bounds[cluster_desc.num_instances] = bounds;
      cluster_desc.num_instances++;
    }

    auto up_axis = ConstVec3f::positive_x;
    if (cluster_desc.num_instances > 1) {
      auto& p1 = instance_bounds[cluster_desc.num_instances - 1].position;
      auto& p0 = instance_bounds[0].position;
      up_axis = normalize(p1 - p0);
    }

    OBB3f dst_bounds;
    bounds::FitOBBsAroundAxisParams fit_params{};
    fit_params.axis_bounds = instance_bounds;
    fit_params.num_bounds = int(cluster_desc.num_instances);
    fit_params.test_type = bounds::FitOBBsAroundAxisParams::TestType::None;
    fit_params.preferred_axis = up_axis;
    fit_params.use_preferred_axis = true;
    fit_params.dst_bounds = &dst_bounds;
    bounds::fit_obbs_around_axis(fit_params);
    cluster_desc.bounds = dst_bounds;

    dst_cluster_descs[num_clusters_created++] = cluster_desc;
  }

  return num_clusters_created;
}

bool enable_shadow_criterion(int lod, uint32_t i, uint32_t j, uint32_t k) {
  if (lod == 0) {
    return k == 0;
  } else {
    assert(lod == 1);
    return j == 0 && (i % 2) == 0;
  }
}

bool can_fadeout_criterion(int lod, uint32_t i, uint32_t j, uint32_t k) {
  if (lod == 0) {
    return k > 1;
  } else {
    assert(lod == 1);
    return k > 1 || ((i % 2) == 0 && j == 0);
  }
}

uint32_t make_render_tree_leaves_instances(
  const FoliageDistributionEntry* distribution_entries, //  size = num_entries
  const RenderTreeLeavesInstanceMeta* instance_meta,    //  size = num_entries
  uint32_t num_entries,
  uint32_t num_steps,
  uint32_t num_instances_per_step,
  cull::FrustumCullGroupHandle cull_group_handle, int preferred_lod,
  foliage::TreeLeavesRenderInstanceDescriptor* dst_instance_descs) {

  const auto instances_per_cluster = num_steps * num_instances_per_step;
  assert((num_entries % instances_per_cluster) == 0);
  const uint32_t num_clusters = num_entries / instances_per_cluster;

  uint32_t dst_instance_ind{};
  for (uint32_t i = 0; i < num_clusters; i++) {
    uint32_t cluster_off = i * instances_per_cluster;

    for (uint32_t j = 0; j < num_steps; j++) {
      //  @NOTE assumes one frustum cull instance per step.
      const uint32_t frustum_instance_off = i * num_steps + j;

      for (uint32_t k = 0; k < num_instances_per_step; k++) {
        const uint32_t inst_off = cluster_off + j * num_instances_per_step + k;
        assert(inst_off < num_entries);

        const auto& src_entry = distribution_entries[inst_off];
        const auto& src_meta = instance_meta[inst_off];

        foliage::TreeLeavesRenderInstanceDescriptor dst_desc{};
        dst_desc.is_active = true;
        dst_desc.translation = src_entry.translation;
        dst_desc.forwards = src_entry.forwards_dir;
        dst_desc.right = src_entry.right_dir;
        dst_desc.wind_node.info0 = src_meta.packed_wind_axis_root_info[0];
        dst_desc.wind_node.info1 = src_meta.packed_wind_axis_root_info[1];
        dst_desc.wind_node.info2 = src_meta.packed_wind_axis_root_info[2];
        dst_desc.frustum_cull_group = cull_group_handle.group_index;
        dst_desc.frustum_cull_instance_index = frustum_instance_off;
        dst_desc.rand01 = src_entry.randomness;
        dst_desc.y_rotation = src_entry.y_rotation;
        dst_desc.z_rotation = src_entry.z_rotation;
        dst_desc.can_fadeout = can_fadeout_criterion(preferred_lod, i, j, k);
        dst_desc.enable_fixed_shadow = enable_shadow_criterion(preferred_lod, i, j, k);
        dst_instance_descs[dst_instance_ind++] = dst_desc;
      }
    }
  }

  return dst_instance_ind;
}

void link_render_instances_to_occlusion_clusters(
  foliage_occlusion::ClusterGroupHandle occlusion_cluster_group_handle,
  const foliage_occlusion::ClusterDescriptor* occlusion_cluster_descs,
  uint32_t num_occlusion_clusters,
  TreeLeavesRenderInstanceDescriptor* render_instance_descs,
  uint32_t num_render_instance_descs,
  uint32_t num_instances_per_step) {

  assert(occlusion_cluster_group_handle.is_valid());
  assert(occlusion_cluster_group_handle.element_group.index + 1 < 0xffff);
  (void) num_render_instance_descs;

  for (uint32_t i = 0; i < num_occlusion_clusters; i++) {
    auto& cluster = occlusion_cluster_descs[i];
    for (uint32_t j = 0; j < cluster.num_instances; j++) {
      auto& inst = cluster.instances[j];
      //  @TODO @HACK: Assumes associated render instance is always step 0
      uint32_t inst_beg = inst.associated_render_instance;
      assert(i < 0xffff && j < 0xff);
      for (uint32_t k = 0; k < num_instances_per_step; k++) {
        const uint32_t associated_render_instance = inst_beg + uint32_t(k);
        assert(associated_render_instance < num_render_instance_descs);
        auto& dst_desc = render_instance_descs[associated_render_instance];
        //  @NOTE: 1-based index.
        dst_desc.occlusion_cull_group = uint16_t(
          occlusion_cluster_group_handle.element_group.index + 1);
        dst_desc.occlusion_cull_cluster_index = uint16_t(i);
        dst_desc.occlusion_cull_instance_index = uint8_t(j);
      }
    }
  }
}

FoliageDistributionParams make_thin_foliage_instance_params(float* global_scale,
                                                            float* curl_scale,
                                                            Vec2f* lod_distance_limits) {
  FoliageDistributionParams result{};
  result.num_steps = 3;
  result.num_instances_per_step = 3;
  result.translation_log_min_x = 5.0f;
  result.translation_log_max_x = 5.0f;
  result.translation_step_power = 1.0f;
  result.translation_step_spread_scale = 0.25f;
  result.translation_x_scale = 2.0f;
  result.translation_y_scale = 0.0f;
  result.rand_z_rotation_scale = 1.0f;
  *curl_scale = 1.0f;
  *global_scale = 1.5f;
  *lod_distance_limits = Vec2f{100.0f, 108.0f};
  return result;
}

FoliageDistributionParams make_tighter_foliage_instance_params(bool low_lod,
                                                               float* global_scale,
                                                               float* curl_scale,
                                                               Vec2f* lod_distance_limits) {
  FoliageDistributionParams result{};
  result.num_steps = low_lod ? 3 : 5;
  result.num_instances_per_step = 3;
  result.translation_log_min_x = 1.0f;
  result.translation_log_max_x = 5.0f;
  result.translation_step_power = 0.5f;
  result.translation_step_spread_scale = 1.0f;
  result.translation_x_scale = 2.0f;
  result.translation_y_scale = 1.0f;
  result.rand_z_rotation_scale = 0.125f;
  *global_scale = low_lod ? 1.25f : 1.0f;
  *curl_scale = 0.5f;
  *lod_distance_limits = Vec2f{64.0f, 72.0f};
  return result;
}

FoliageDistributionParams make_hanging_foliage_instance_params(float* global_scale,
                                                               float* curl_scale,
                                                               Vec2f* lod_distance_limits) {
  FoliageDistributionParams result{};
  result.num_steps = 5;
  result.num_instances_per_step = 3;
  result.translation_log_min_x = 0.1f;
  result.translation_log_max_x = 2.0f;
  result.translation_step_power = 0.25f;
  result.translation_step_spread_scale = 0.1f;
  result.translation_x_scale = 1.5f;
  result.translation_y_scale = 2.0f;
  result.rand_z_rotation_scale = 0.125f;
  *global_scale = 1.0f;
  *curl_scale = 0.5f;
  *lod_distance_limits = Vec2f{64.0f, 72.0f};
  return result;
}

FoliageDistributionParams make_from_distribution_strategy(FoliageDistributionStrategy strat,
                                                          float* global_scale, float* curl_scale,
                                                          Vec2f* lod_dist_lims) {
  switch (strat) {
    case FoliageDistributionStrategy::TightLowN:
      return make_tighter_foliage_instance_params(true, global_scale, curl_scale, lod_dist_lims);
    case FoliageDistributionStrategy::TightHighN:
      return make_tighter_foliage_instance_params(false, global_scale, curl_scale, lod_dist_lims);
    case FoliageDistributionStrategy::Hanging:
      return make_hanging_foliage_instance_params(global_scale, curl_scale, lod_dist_lims);
    case FoliageDistributionStrategy::ThinCurledLowN:
      return make_thin_foliage_instance_params(global_scale, curl_scale, lod_dist_lims);
    default:
      assert(false);
      return {};
  }
}

auto make_distribution_entries_from_internodes(const Internodes& internodes,
                                               const std::vector<int>& on_internodes,
                                               const Bounds3f& aabb,
                                               FoliageDistributionParams distrib_params) {
  struct Result {
    std::vector<FoliageDistributionEntry> entries;
    std::vector<RenderTreeLeavesInstanceMeta> instance_meta;
  };

  Result result{};
  auto& entries = result.entries;
  auto& instance_meta = result.instance_meta;

  auto axis_root_info = tree::compute_axis_root_info(internodes);
  auto remapped_roots = tree::remap_axis_roots(internodes);

  const auto num_instances_per_step = uint32_t(distrib_params.num_instances_per_step);
  const auto num_steps = uint32_t(distrib_params.num_steps);
  const auto instances_per_node = num_instances_per_step * num_steps;

  for (const int index : on_internodes) {
    auto& node = internodes[index];

    auto root_info = tree::make_wind_axis_root_info(
      node, internodes, axis_root_info, remapped_roots, aabb);
    auto packed_root_info = tree::to_packed_wind_info(root_info, root_info);

    const auto curr_offset = uint32_t(entries.size());
    entries.resize(curr_offset + instances_per_node);
    instance_meta.resize(curr_offset + instances_per_node);

    distrib_params.tip_position = node.tip_position();
    distrib_params.outwards_direction = aabb.to_fraction(
      clamp_each(node.tip_position(), aabb.min, aabb.max));

    distribute_foliage_outwards_from_nodes(distrib_params, entries.data() + curr_offset);
    for (uint32_t j = 0; j < instances_per_node; j++) {
      RenderTreeLeavesInstanceMeta meta{};
      meta.packed_wind_axis_root_info = packed_root_info;
      instance_meta[curr_offset + j] = meta;
    }
  }

  assert(entries.size() == instance_meta.size());
  return result;
}

TreeLeavesRenderInstanceGroupDescriptor
make_render_instance_group_desc(
  float global_scale, float curl_scale, const Bounds3f& aabb,
  uint16_t alpha_image_index, uint16_t color_image0_index, uint16_t color_image1_index,
  float uv_off, float color_image_mix, const Vec2f& lod_distance_limits) {
  //
  TreeLeavesRenderInstanceGroupDescriptor render_group_desc{};
  render_group_desc.alpha_image_index = alpha_image_index;
  render_group_desc.color_image0_index = color_image0_index;
  render_group_desc.color_image1_index = color_image1_index;
  render_group_desc.aabb_p0 = aabb.min;
  render_group_desc.aabb_p1 = aabb.max;
  render_group_desc.curl_scale = curl_scale;
  render_group_desc.global_scale = global_scale;
  render_group_desc.uv_offset = uv_off;
  render_group_desc.color_image_mix = color_image_mix;
  render_group_desc.lod_distance_limits = lod_distance_limits;
  render_group_desc.fadeout_scale_distance_limits = Config::lod_fadeout_distances;
  return render_group_desc;
}

uint32_t compute_num_required_leaf_pools(uint32_t num_descs) {
  const uint32_t pool_size = Config::leaf_pool_size;
  return num_descs / pool_size + uint32_t(num_descs % pool_size != 0);
}

TreeLeavesDrawableGroupHandle acquire_group(
  TreeLeavesPoolAllocator& alloc, foliage::TreeLeavesRenderData& rd,
  const TreeLeavesRenderInstanceGroupDescriptor& desc) {
  //
  TreeLeavesDrawableGroupHandle group{};
  if (!alloc.free_groups.empty()) {
    group = alloc.free_groups.front();
    alloc.free_groups.pop_front();
    set_tree_leaves_drawable_group_data(rd, group, desc);
  } else {
    group = create_tree_leaves_drawable_group(rd, desc);
  }
  return group;
}

TreeLeavesDrawableInstanceSetHandle acquire_set(
  TreeLeavesPoolAllocator& alloc, foliage::TreeLeavesRenderData& rd) {
  //
  TreeLeavesDrawableInstanceSetHandle set{};
  if (!alloc.free_sets.empty()) {
    set = alloc.free_sets.front();
    alloc.free_sets.pop_front();
  } else {
    set = reserve_tree_leaves_drawable_instance_data(rd, Config::leaf_pool_size);
  }
  return set;
}

PooledLeafComponents create_pooled_leaf_components(
  TreeLeavesPoolAllocator& alloc, foliage::TreeLeavesRenderData& rd,
  const TreeLeavesRenderInstanceGroupDescriptor& group_desc,
  const TreeLeavesRenderInstanceDescriptor* instance_descs, uint32_t num_instances) {
  //
  PooledLeafComponents result;
  result.group = acquire_group(alloc, rd, group_desc);

  const auto num_pools = compute_num_required_leaf_pools(num_instances);
  for (uint32_t i = 0; i < num_pools; i++) {
    auto set = acquire_set(alloc, rd);
    const uint32_t inst_beg = i * Config::leaf_pool_size;
    const uint32_t inst_end = std::min(inst_beg + Config::leaf_pool_size, num_instances);
    assert(inst_end > inst_beg);

    set_tree_leaves_drawable_instance_data(
      rd, result.group, group_desc, set, instance_descs + inst_beg, inst_end - inst_beg);
    result.sets.emplace_back() = set;
  }

  return result;
}

void release_pooled_leaf_components(
  TreeLeavesPoolAllocator& alloc, const PooledLeafComponents& components) {
  //
  for (auto& set : components.sets) {
    foliage::deactivate_tree_leaves_drawable_instances(
      *foliage::get_global_tree_leaves_render_data(), set);
    alloc.free_sets.push_back(set);
  }
  alloc.free_groups.push_back(components.group);
}

void set_lod_instance_meta(
  const PooledLeafComponents& components, TreeLeavesRenderData& rd,
  uint32_t num_clusters, uint32_t num_steps, uint32_t num_instances_per_step, int lod) {
  //
  assert(lod == 0 || lod == 1);

  uint32_t inst_ind{};
  for (uint32_t i = 0; i < num_clusters; i++) {
    for (uint32_t j = 0; j < num_steps; j++) {
      for (uint32_t k = 0; k < num_instances_per_step; k++) {
        auto set_ind = inst_ind / Config::leaf_pool_size;
        assert(set_ind < uint32_t(components.sets.size()));
        uint32_t inst_off = inst_ind - set_ind * Config::leaf_pool_size;

        const bool can_fadeout = can_fadeout_criterion(lod, i, j, k);
        const bool enable_fixed_shadow = enable_shadow_criterion(lod, i, j, k);
        foliage::set_tree_leaves_drawable_instance_meta_slow(
          rd, components.sets[set_ind], inst_off, can_fadeout, enable_fixed_shadow);

        inst_ind++;
      }
    }
  }
}

void set_lod_instance_meta(const FoliageDrawableComponents& comp, TreeLeavesRenderData& rd, int lod) {
  if (comp.pooled_leaf_components) {
    set_lod_instance_meta(
      comp.pooled_leaf_components.value(), rd,
      comp.num_clusters, comp.num_steps, comp.num_instances_per_step, lod);
  }
}

FoliageDrawableComponents
create_components_from_internodes(
  const Internodes& internodes, const std::vector<int>& on_internodes,
  const CreateFoliageDrawableComponentParams& create_params, FoliageDistributionParams distrib_params,
  float global_scale, float curl_scale, const Vec2f& lod_distance_limits,
  cull::FrustumCullData* cull_data,
  foliage_occlusion::FoliageOcclusionSystem* occlusion_sys,
  foliage::TreeLeavesPoolAllocator& pool_alloc) {
  //
  FoliageDrawableComponents result{};

  const auto aabb = tree::internode_aabb(internodes);

  const auto num_instances_per_step = uint32_t(distrib_params.num_instances_per_step);
  const auto num_steps = uint32_t(distrib_params.num_steps);

  auto distrib_res = make_distribution_entries_from_internodes(
    internodes, on_internodes, aabb, distrib_params);

  auto& entries = distrib_res.entries;
  auto& instance_meta = distrib_res.instance_meta;
  const auto num_created_nodes = uint32_t(on_internodes.size());
  const auto num_entries = uint32_t(entries.size());

  result.num_clusters = num_entries / (num_steps * num_instances_per_step);
  result.num_steps = num_steps;
  result.num_instances_per_step = num_instances_per_step;

  //  frustum cull instances
  std::vector<cull::FrustumCullInstanceDescriptor> cull_descs(num_entries);
  const uint32_t num_cull_instances = make_frustum_cull_instance_descs(
    entries.data(), num_entries, num_steps, num_instances_per_step, global_scale, cull_descs.data());
  const auto cull_group_handle = cull::create_frustum_cull_instance_group(
    cull_data, cull_descs.data(), num_cull_instances);

  //  occlusion cluster instances
  bool enable_cpu_occlusion_clusters{};
  uint32_t num_occlusion_clusters{};
  foliage_occlusion::ClusterGroupHandle occlusion_cluster_group_handle{};

  std::vector<foliage_occlusion::ClusterDescriptor> occlusion_cluster_descs;
  if (enable_cpu_occlusion_clusters) {
    occlusion_cluster_descs.resize(num_created_nodes);

    num_occlusion_clusters = make_foliage_occlusion_cluster_descriptors(
      entries.data(), num_entries, num_steps, num_instances_per_step,
      Config::occlusion_cluster_create_interval, global_scale, occlusion_cluster_descs.data());

    occlusion_cluster_group_handle = foliage_occlusion::insert_cluster_group(
      occlusion_sys, occlusion_cluster_descs.data(), num_occlusion_clusters);
  }

  //  render instances
  std::vector<TreeLeavesRenderInstanceDescriptor> render_instances(num_entries);
  const uint32_t num_insts = make_render_tree_leaves_instances(
    entries.data(), instance_meta.data(),
    num_entries, num_steps, num_instances_per_step,
    cull_group_handle, create_params.preferred_lod, render_instances.data());

  if (enable_cpu_occlusion_clusters) {
    link_render_instances_to_occlusion_clusters(
      occlusion_cluster_group_handle,
      occlusion_cluster_descs.data(), num_occlusion_clusters,
      render_instances.data(), num_insts, num_instances_per_step);
  }

  const auto render_group_desc = make_render_instance_group_desc(
    global_scale, curl_scale, aabb,
    create_params.alpha_image_index,
    create_params.color_image0_index,
    create_params.color_image1_index,
    create_params.uv_offset, create_params.color_image_mix01, lod_distance_limits);

#if 1
  result.pooled_leaf_components = create_pooled_leaf_components(
    pool_alloc, *foliage::get_global_tree_leaves_render_data(),
    render_group_desc, render_instances.data(), num_insts);
#else
  (void) pool_alloc;
  result.leaves_drawable = foliage::create_tree_leaves_drawable(
    render_instances.data(), num_insts, render_group_desc);
#endif

  result.cull_group_handle = cull_group_handle;
  if (enable_cpu_occlusion_clusters) {
    result.occlusion_cluster_group_handle = occlusion_cluster_group_handle;
  }

  assert(result.num_instances() == num_insts);
  return result;
}

void destroy_components(FoliageDrawableComponents& components,
                        cull::FrustumCullData* cull_data,
                        foliage_occlusion::FoliageOcclusionSystem* occlusion_sys,
                        TreeLeavesPoolAllocator* pool_alloc) {
  if (components.leaves_drawable) {
    foliage::destroy_tree_leaves_drawable(components.leaves_drawable.value());
    components.leaves_drawable = NullOpt{};
  }

  if (components.cull_group_handle) {
    cull::destroy_frustum_cull_instance_group(cull_data, components.cull_group_handle.value());
    components.cull_group_handle = NullOpt{};
  }

  if (components.occlusion_cluster_group_handle) {
    foliage_occlusion::remove_cluster_group(
      occlusion_sys, components.occlusion_cluster_group_handle.value());
    components.occlusion_cluster_group_handle = NullOpt{};
  }

  if (components.pooled_leaf_components) {
    release_pooled_leaf_components(*pool_alloc, components.pooled_leaf_components.value());
    components.pooled_leaf_components = NullOpt{};
  }
}

} //  anon

FoliageDrawableComponents foliage::create_foliage_drawable_components_from_internodes(
  cull::FrustumCullData* frustum_cull_data,
  foliage_occlusion::FoliageOcclusionSystem* occlusion_system,
  TreeLeavesPoolAllocator* pool_alloc, const CreateFoliageDrawableComponentParams& create_params,
  const std::vector<Internode>& internodes, const std::vector<int>& subset_internodes) {
  //
  float global_scale{};
  float curl_scale{};
  Vec2f lod_dist_lims{};
  auto distrib_params = make_from_distribution_strategy(
    create_params.distribution_strategy, &global_scale, &curl_scale, &lod_dist_lims);

#if 0
  foliage::seed_urandf(234234);
#endif

  auto res = create_components_from_internodes(
    internodes, subset_internodes,
    create_params, distrib_params, global_scale, curl_scale, lod_dist_lims,
    frustum_cull_data, occlusion_system, *pool_alloc);

  res.set_scale_fraction(create_params.initial_scale01);

  return res;
}

void foliage::destroy_foliage_drawable_components(
  FoliageDrawableComponents* components,
  cull::FrustumCullData* frustum_cull_data,
  foliage_occlusion::FoliageOcclusionSystem* occlusion_system,
  TreeLeavesPoolAllocator* pool_alloc) {
  //
  destroy_components(*components, frustum_cull_data, occlusion_system, pool_alloc);
}

void foliage::FoliageDrawableComponents::set_hidden(bool hide) {
  if (leaves_drawable) {
    foliage::set_tree_leaves_hidden(leaves_drawable.value().group, hide);
  }
  if (pooled_leaf_components) {
    foliage::set_tree_leaves_hidden(pooled_leaf_components.value().group, hide);
  }
}

void foliage::FoliageDrawableComponents::increment_uv_osc_time(float dt) {
  if (leaves_drawable) {
    foliage::increment_tree_leaves_uv_osc_time(leaves_drawable.value().group, dt);
  }
  if (pooled_leaf_components) {
    foliage::increment_tree_leaves_uv_osc_time(pooled_leaf_components.value().group, dt);
  }
}

void foliage::FoliageDrawableComponents::set_color_mix_fraction(float f) {
  if (leaves_drawable) {
    foliage::set_tree_leaves_color_image_mix_fraction(leaves_drawable.value().group, f);
  }
  if (pooled_leaf_components) {
    foliage::set_tree_leaves_color_image_mix_fraction(pooled_leaf_components.value().group, f);
  }
}

void foliage::FoliageDrawableComponents::set_scale_fraction(float f) {
  if (leaves_drawable) {
    foliage::set_tree_leaves_scale_fraction(leaves_drawable.value().group, f);
  }
  if (pooled_leaf_components) {
    foliage::set_tree_leaves_scale_fraction(pooled_leaf_components.value().group, f);
  }
}

void foliage::FoliageDrawableComponents::set_uv_offset(float f) {
  if (leaves_drawable) {
    foliage::set_tree_leaves_uv_offset(leaves_drawable.value().group, f);
  }
  if (pooled_leaf_components) {
    foliage::set_tree_leaves_uv_offset(pooled_leaf_components.value().group, f);
  }
}

void foliage::FoliageDrawableComponents::set_lod(int lod) {
  set_lod_instance_meta(*this, *get_global_tree_leaves_render_data(), lod);
}

#if 0
namespace {
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<> dis(0.0, 1.0);
} //  anon

void foliage::seed_urandf(uint32_t seed) {
  gen.seed(seed);
}

float foliage::get_urandf() {
  return float(dis(gen));
}
#endif

GROVE_NAMESPACE_END
