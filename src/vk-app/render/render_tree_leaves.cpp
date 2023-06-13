#include "render_tree_leaves.hpp"
#include "render_tree_leaves_types.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace foliage;

uint32_t pack_u16s(uint16_t a, uint16_t b) {
  return uint32_t(a) | (uint32_t(b) << 16u);
}

uint16_t pack_u8s(uint8_t a, uint8_t b) {
  return uint16_t(a) | (uint16_t(b) << 8u);
}

uint32_t pack_image_indices(uint16_t alpha_im, uint8_t color_im0, uint8_t color_im1) {
  const uint16_t color_images = pack_u8s(color_im0, color_im1);
  return pack_u16s(alpha_im, color_images);
}

#ifdef GROVE_DEBUG
Vec3<uint32_t> parse_cpu_occlusion_indices(uint32_t occlusion_cull_group_cluster_instance_index) {
  uint32_t group = occlusion_cull_group_cluster_instance_index & 0xffffu;
  uint32_t cluster_inst = (occlusion_cull_group_cluster_instance_index >> 16u) & 0xffffu;
  uint32_t cluster = cluster_inst & 0xfffu;
  uint32_t instance = (cluster_inst >> 12u) & 0xfu;
  return Vec3<uint32_t>(group, cluster, instance);
}
#endif

uint32_t pack_occlusion_component_indices(uint16_t group, uint16_t cluster, uint8_t instance) {
  assert(cluster < (1u << 12u));
  assert(instance < 16);
  uint32_t packed = ((uint32_t(cluster) & 0xfff) | (uint32_t(instance) << 12u)) << 16u;
  uint32_t res = uint32_t(group) | packed;
#ifdef GROVE_DEBUG
  auto parsed = parse_cpu_occlusion_indices(res);
  assert(parsed.x == group && parsed.y == cluster && parsed.z == instance);
#endif
  return res;
}

void set_image_indices(RenderInstanceGroup& group, uint16_t alpha, uint8_t color0, uint8_t color1) {
  group.alpha_image_color_image_indices_uv_offset_color_image_mix_unused.x =
    pack_image_indices(alpha, color0, color1);
}

void set_uv_offset(RenderInstanceGroup& group, float off) {
  auto& f = group.alpha_image_color_image_indices_uv_offset_color_image_mix_unused;
  memcpy(&f.y, &off, sizeof(float));
}

void set_color_image_mix(RenderInstanceGroup& group, float mix) {
  assert(mix >= 0.0f && mix <= 1.0f);
  auto& f = group.alpha_image_color_image_indices_uv_offset_color_image_mix_unused;
  memcpy(&f.z, &mix, sizeof(float));
}

RenderInstanceGroupMeta
instance_group_desc_to_render_instance_group_meta(const TreeLeavesRenderInstanceGroupDescriptor& desc) {
  RenderInstanceGroupMeta result{};
  result.canonical_global_scale = desc.global_scale;
  result.center_uv_offset = desc.uv_offset;
  result.scale01 = 1.0f;
  result.hidden = false;
  return result;
}

RenderInstanceGroup
instance_group_desc_to_render_instance_group(const TreeLeavesRenderInstanceGroupDescriptor& desc) {
  assert(desc.color_image0_index < 0xff && desc.color_image1_index < 0xff);

  RenderInstanceGroup result;
  set_image_indices(
    result, desc.alpha_image_index,
    uint8_t(desc.color_image0_index), uint8_t(desc.color_image1_index));
  set_uv_offset(result, desc.uv_offset);
  set_color_image_mix(result, desc.color_image_mix);

  result.aabb_p0_curl_scale = Vec4f{desc.aabb_p0, desc.curl_scale};
  result.aabb_p1_global_scale = Vec4f{desc.aabb_p1, desc.global_scale};
  return result;
}

RenderInstance instance_desc_to_render_instance(const TreeLeavesRenderInstanceDescriptor& desc,
                                                uint32_t instance_group) {
  assert(desc.rand01 >= 0.0f && desc.rand01 <= 1.0f);
  RenderInstance result;
  result.translation_forwards_x = Vec4f{desc.translation, desc.forwards.x,};
  result.forwards_yz_right_xy = Vec4f{desc.forwards.y, desc.forwards.z, desc.right.x, desc.right.y};

  memcpy(&result.right_z_instance_group_randomness_unused.x, &desc.right.z, sizeof(float));
  result.right_z_instance_group_randomness_unused.y = instance_group;
  memcpy(&result.right_z_instance_group_randomness_unused.z, &desc.rand01, sizeof(float));

  result.y_rotation_z_rotation_unused = Vec4f{
    desc.y_rotation, desc.z_rotation, 0.0f, 0.0f
  };

  result.wind_node_info0 = desc.wind_node.info0;
  result.wind_node_info1 = desc.wind_node.info1;
  result.wind_node_info2 = desc.wind_node.info2;
  return result;
}

RenderInstanceMeta instance_desc_to_render_instance_meta(const TreeLeavesRenderInstanceDescriptor& desc) {
  RenderInstanceMeta result{};
  result.enable_fixed_shadow = desc.enable_fixed_shadow;
  return result;
}

RenderInstanceComponentIndices
instance_desc_to_render_component_indices(const TreeLeavesRenderInstanceDescriptor& desc) {
  RenderInstanceComponentIndices result;
  result.frustum_cull_group = desc.frustum_cull_group;
  result.frustum_cull_instance_index = desc.frustum_cull_instance_index;
  result.is_active = uint32_t(desc.is_active);
  result.occlusion_cull_group_cluster_instance_index = pack_occlusion_component_indices(
    desc.occlusion_cull_group,
    desc.occlusion_cull_cluster_index,
    desc.occlusion_cull_instance_index);
  return result;
}

ComputeLODInstance
instance_desc_to_compute_lod_instance(const TreeLeavesRenderInstanceDescriptor& desc,
                                      const TreeLeavesRenderInstanceGroupDescriptor& group_desc) {
  ComputeLODInstance result;
  result.translation_fadeout_allowed = Vec4f{desc.translation, float(desc.can_fadeout)};
  result.scale_distance_limits_lod_distance_limits = Vec4f{
    group_desc.fadeout_scale_distance_limits.x,
    group_desc.fadeout_scale_distance_limits.y,
    group_desc.lod_distance_limits.x,
    group_desc.lod_distance_limits.y
  };
  return result;
}

void set_global_scale(RenderInstanceGroup& group, float scale) {
  group.aabb_p1_global_scale.w = scale;
}

float get_uv_offset_from_meta_group(const RenderInstanceGroupMeta& meta) {
//  return meta.center_uv_offset + std::sin(meta.uv_osc_time) * 2e-2f;
  return meta.center_uv_offset + meta.uv_osc_time * 4e-2f;
}

float get_scale_from_meta_group(const RenderInstanceGroupMeta& meta) {
  const float hidden_scale = meta.hidden ? 0.0f : 1.0f;
  return meta.canonical_global_scale * meta.scale01 * hidden_scale;
}

void set_alpha_image_index(RenderInstanceGroup& group, uint16_t ind) {
  uint32_t packed = group.alpha_image_color_image_indices_uv_offset_color_image_mix_unused.x;
  uint32_t col = packed & (uint32_t(0xffff) << 16u);
  col |= uint32_t(ind);
  group.alpha_image_color_image_indices_uv_offset_color_image_mix_unused.x = col;
}

uint16_t get_alpha_image_index(const RenderInstanceGroup& group) {
  return group.alpha_image_color_image_indices_uv_offset_color_image_mix_unused.x & 0xffff;
}

uint16_t get_color_image_indices(const RenderInstanceGroup& group) {
  return (group.alpha_image_color_image_indices_uv_offset_color_image_mix_unused.x >> 16u) & 0xffff;
}

uint8_t get_color_image0_index(const RenderInstanceGroup& group) {
  uint16_t inds = get_color_image_indices(group);
  return inds & 0xff;
}

uint8_t get_color_image1_index(const RenderInstanceGroup& group) {
  uint16_t inds = get_color_image_indices(group);
  return (inds >> 8u) & 0xff;
}

void set_color_image0_index(RenderInstanceGroup& group, uint8_t im0) {
  const uint16_t alpha = get_alpha_image_index(group);
  const uint8_t im1 = get_color_image1_index(group);
  set_image_indices(group, alpha, im0, im1);
}

void set_color_image1_index(RenderInstanceGroup& group, uint8_t im1) {
  const uint16_t alpha = get_alpha_image_index(group);
  const uint8_t im0 = get_color_image0_index(group);
  set_image_indices(group, alpha, im0, im1);
}

RenderInstanceGroup* get_instance_group(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle handle) {
  //
  const uint32_t ind = handle.group_index;
  assert(ind < rd.num_instance_groups());
  return &rd.instance_groups[ind];
}

RenderInstanceGroupMeta* get_instance_group_meta(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle handle) {
  //
  const uint32_t ind = handle.group_index;
  assert(ind < uint32_t(rd.instance_group_meta.size()));
  return &rd.instance_group_meta[ind];
}

TreeLeavesRenderData::InstanceSetIndices* get_instance_set_indices(
  TreeLeavesRenderData& rd, TreeLeavesDrawableInstanceSetHandle sh) {
  //
  assert(sh.set_index < uint32_t(rd.instance_sets.size()));
  assert(rd.instance_sets[sh.set_index].in_use);
  return rd.instance_sets.data() + sh.set_index;
}

#if 0
void set_fadeout_scale_distance_limits(TreeLeavesRenderData& rd,
                                       TreeLeavesDrawableHandle handle, const Vec2f& lims) {
  uint32_t ind = handle.group_index;
  assert(ind < rd.num_instance_groups());
  auto* group = rd.group_alloc.read_group(
    ContiguousElementGroupAllocator::ElementGroupHandle{ind});
  for (uint32_t i = group->offset; i < group->offset + group->count; i++) {
    rd.compute_lod_instances[i].scale_distance_limits_lod_distance_limits.x = lims.x;
    rd.compute_lod_instances[i].scale_distance_limits_lod_distance_limits.y = lims.y;
  }
  rd.modified = true;
}

void set_lod_transition_distance_limits(TreeLeavesRenderData& rd,
                                        TreeLeavesDrawableHandle handle, const Vec2f& lims) {
  uint32_t ind = handle.group_index;
  assert(ind < rd.num_instance_groups());
  auto* group = rd.group_alloc.read_group(
    ContiguousElementGroupAllocator::ElementGroupHandle{ind});
  for (uint32_t i = group->offset; i < group->offset + group->count; i++) {
    rd.compute_lod_instances[i].scale_distance_limits_lod_distance_limits.z = lims.x;
    rd.compute_lod_instances[i].scale_distance_limits_lod_distance_limits.w = lims.y;
  }
  rd.modified = true;
}
#endif

void set_scale_fraction(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle handle, float scale01) {
  //
  assert(scale01 >= 0.0f && scale01 <= 1.0f);

  auto& meta_group = *get_instance_group_meta(rd, handle);
  meta_group.scale01 = scale01;

  set_global_scale(*get_instance_group(rd, handle), get_scale_from_meta_group(meta_group));
  rd.instance_groups_modified = true;
}

void set_color_image_mix_fraction(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle handle, float f) {
  //
  assert(f >= 0.0f && f <= 1.0f);
  set_color_image_mix(*get_instance_group(rd, handle), f);
  rd.instance_groups_modified = true;
}

void set_uv_offset(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle handle, float center_uv_off) {
  //
  auto& dst_group = *get_instance_group(rd, handle);
  auto& meta_group = *get_instance_group_meta(rd, handle);
  meta_group.center_uv_offset = center_uv_off;

  set_uv_offset(dst_group, get_uv_offset_from_meta_group(meta_group));
  rd.instance_groups_modified = true;
}

void increment_uv_osc_time(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle handle, float dt) {
  //
  auto& dst_group = *get_instance_group(rd, handle);
  auto& meta_group = *get_instance_group_meta(rd, handle);
  meta_group.uv_osc_time += dt;

  set_uv_offset(dst_group, get_uv_offset_from_meta_group(meta_group));
  rd.instance_groups_modified = true;
}

void set_alpha_image_index(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle handle, uint16_t im_index) {
  //
  set_alpha_image_index(*get_instance_group(rd, handle), im_index);
  rd.max_alpha_image_index = std::max(rd.max_alpha_image_index, uint32_t(im_index));
  rd.instance_groups_modified = true;
}

void set_color_image0_index(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle handle, uint8_t im_index) {
  //
  set_color_image0_index(*get_instance_group(rd, handle), im_index);
  rd.max_color_image_index = std::max(rd.max_color_image_index, uint32_t(im_index));
  rd.instance_groups_modified = true;
}

void set_color_image1_index(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle handle, uint8_t im_index) {
  //
  set_color_image1_index(*get_instance_group(rd, handle), im_index);
  rd.max_color_image_index = std::max(rd.max_color_image_index, uint32_t(im_index));
  rd.instance_groups_modified = true;
}

void set_hidden(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle handle, bool hidden) {
  //
  auto& dst_group = *get_instance_group(rd, handle);
  auto& meta_group = *get_instance_group_meta(rd, handle);
  meta_group.hidden = hidden;

  set_global_scale(dst_group, get_scale_from_meta_group(meta_group));
  rd.instance_groups_modified = true;
}

void set_group_data(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle gh,
  const TreeLeavesRenderInstanceGroupDescriptor& group_desc) {
  //
  assert(gh.group_index < uint32_t(rd.instance_groups.size()));
  assert(rd.instance_groups.size() == rd.instance_group_meta.size());
  assert(rd.instance_group_in_use.size() == rd.instance_groups.size());
  assert(rd.instance_group_in_use[gh.group_index]);

  auto& dst_group = rd.instance_groups[gh.group_index];
  dst_group = instance_group_desc_to_render_instance_group(group_desc);

  auto& dst_group_meta = rd.instance_group_meta[gh.group_index];
  dst_group_meta = instance_group_desc_to_render_instance_group_meta(group_desc);

  rd.instance_groups_modified = true;
}

void deactivate_instance_range(TreeLeavesRenderData& rd, uint32_t begin, uint32_t num_instances) {
  for (uint32_t i = 0; i < num_instances; i++) {
    rd.instances[i + begin] = {};
  }
  for (uint32_t i = 0; i < num_instances; i++) {
    rd.instance_component_indices[i + begin] = {};
  }
  for (uint32_t i = 0; i < num_instances; i++) {
    rd.compute_lod_instances[i + begin] = {};
  }
  for (uint32_t i = 0; i < num_instances; i++) {
    rd.instance_meta[i + begin] = {};
  }
}

void set_instance_data(
  TreeLeavesRenderData& rd, const TreeLeavesDrawableGroupHandle* gh,
  const TreeLeavesRenderInstanceGroupDescriptor* group_desc,
  TreeLeavesDrawableInstanceSetHandle sh,
  const TreeLeavesRenderInstanceDescriptor* instance_descs, uint32_t num_instances) {
  //
  if (gh) {
    assert(gh->group_index < uint32_t(rd.instance_groups.size()));
    assert(rd.instance_group_in_use[gh->group_index]);
  } else {
    assert(!instance_descs);
    assert(!group_desc);
  }

  const TreeLeavesRenderData::InstanceSetIndices* inst_set_inds = get_instance_set_indices(rd, sh);
  const uint32_t begin = inst_set_inds->offset;

  assert(begin < uint32_t(rd.instances.size()));
  assert(begin + inst_set_inds->count <= uint32_t(rd.instances.size()));
  assert(num_instances <= inst_set_inds->count);

  if (instance_descs) {
    for (uint32_t i = 0; i < num_instances; i++) {
      auto& dst = rd.instances[i + begin];
      dst = instance_desc_to_render_instance(instance_descs[i], gh->group_index);
    }
    for (uint32_t i = 0; i < num_instances; i++) {
      auto& dst = rd.instance_component_indices[i + begin];
      dst = instance_desc_to_render_component_indices(instance_descs[i]);
    }
    for (uint32_t i = 0; i < num_instances; i++) {
      auto& dst = rd.compute_lod_instances[i + begin];
      dst = instance_desc_to_compute_lod_instance(instance_descs[i], *group_desc);
    }
    for (uint32_t i = 0; i < num_instances; i++) {
      auto& dst = rd.instance_meta[i + begin];
      dst = instance_desc_to_render_instance_meta(instance_descs[i]);
    }
  } else {
    deactivate_instance_range(rd, begin, num_instances);
  }

  if (group_desc) {
    rd.max_alpha_image_index = std::max(
      rd.max_alpha_image_index, uint32_t(group_desc->alpha_image_index));
    rd.max_color_image_index = std::max(
      rd.max_color_image_index,
      std::max(uint32_t(group_desc->color_image0_index), uint32_t(group_desc->color_image1_index)));
  }

  rd.instances_modified = true;
  rd.modified_instance_ranges.push(begin, begin + num_instances);
}

uint32_t require_instance_group(TreeLeavesRenderData& rd) {
  uint32_t ind{};
  bool found_group{};
  for (uint32_t i = 0; i < uint32_t(rd.instance_group_in_use.size()); i++) {
    if (!rd.instance_group_in_use[i]) {
      ind = i;
      found_group = true;
      break;
    }
  }
  if (!found_group) {
    ind = uint32_t(rd.instance_group_in_use.size());
    rd.instance_group_in_use.emplace_back();
    rd.instance_groups.emplace_back();
    rd.instance_group_meta.emplace_back();
  }
  assert(!rd.instance_group_in_use[ind]);
  rd.instance_group_in_use[ind] = 1;
  return ind;
}

uint32_t require_instance_set(TreeLeavesRenderData& rd) {
  uint32_t ind{};
  bool found_set{};
  for (uint32_t i = 0; i < uint32_t(rd.instance_sets.size()); i++) {
    if (!rd.instance_sets[i].in_use) {
      ind = i;
      found_set = true;
      break;
    }
  }
  if (!found_set) {
    ind = uint32_t(rd.instance_sets.size());
    rd.instance_sets.emplace_back();
  }
  assert(!rd.instance_sets[ind].in_use);
  rd.instance_sets[ind].in_use = true;
  return ind;
}

template <typename T>
void destroy_range(std::vector<T>& insts, uint32_t beg, uint32_t end) {
  insts.erase(insts.begin() + beg, insts.begin() + end);
}

void invalidate_modified_instance_ranges(TreeLeavesRenderData& rd) {
  rd.instances_modified = true;
  rd.modified_instance_ranges.clear();
  rd.modified_instance_ranges_invalidated = true;
}

struct {
  TreeLeavesRenderData render_data;
} globals;

} //  anon

TreeLeavesDrawableGroupHandle foliage::create_tree_leaves_drawable_group(
  TreeLeavesRenderData& rd, const TreeLeavesRenderInstanceGroupDescriptor& group_desc) {
  //
  const TreeLeavesDrawableGroupHandle result{require_instance_group(rd)};
  set_group_data(rd, result, group_desc);
  return result;
}

void foliage::set_tree_leaves_drawable_group_data(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle gh,
  const TreeLeavesRenderInstanceGroupDescriptor& group_desc) {
  //
  set_group_data(rd, gh, group_desc);
}

void foliage::destroy_tree_leaves_drawable_group(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle gh) {
  //
  assert(gh.group_index < uint32_t(rd.instance_group_in_use.size()));
  assert(rd.instance_group_in_use[gh.group_index]);
  rd.instance_group_in_use[gh.group_index] = 0;
}

TreeLeavesDrawableInstanceSetHandle foliage::reserve_tree_leaves_drawable_instance_data(
  TreeLeavesRenderData& rd, uint32_t num_instances) {
  //
  const auto curr_num_insts = uint32_t(rd.instances.size());
  const uint32_t new_num_insts = curr_num_insts + num_instances;

  rd.instances.resize(new_num_insts);
  rd.instance_component_indices.resize(new_num_insts);
  rd.compute_lod_instances.resize(new_num_insts);
  rd.instance_meta.resize(new_num_insts);

  TreeLeavesDrawableInstanceSetHandle result{};
  result.set_index = require_instance_set(rd);

  auto& inst_set = rd.instance_sets[result.set_index];
  assert(inst_set.in_use);
  inst_set.offset = curr_num_insts;
  inst_set.count = num_instances;

  set_instance_data(rd, nullptr, nullptr, result, nullptr, num_instances);
  return result;
}

TreeLeavesDrawableInstanceSetHandle foliage::create_tree_leaves_drawable_instances(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle gh,
  const TreeLeavesRenderInstanceGroupDescriptor& group_desc,
  const TreeLeavesRenderInstanceDescriptor* instance_descs, uint32_t num_instances) {
  //
  const auto curr_num_insts = uint32_t(rd.instances.size());
  const uint32_t new_num_insts = curr_num_insts + num_instances;

  rd.instances.resize(new_num_insts);
  rd.instance_component_indices.resize(new_num_insts);
  rd.compute_lod_instances.resize(new_num_insts);
  rd.instance_meta.resize(new_num_insts);

  TreeLeavesDrawableInstanceSetHandle result{};
  result.set_index = require_instance_set(rd);

  auto& inst_set = rd.instance_sets[result.set_index];
  assert(inst_set.in_use);
  inst_set.offset = curr_num_insts;
  inst_set.count = num_instances;

  set_instance_data(rd, &gh, &group_desc, result, instance_descs, num_instances);
  return result;
}

void foliage::set_tree_leaves_drawable_instance_data(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle gh,
  const TreeLeavesRenderInstanceGroupDescriptor& group_desc,
  TreeLeavesDrawableInstanceSetHandle sh,
  const TreeLeavesRenderInstanceDescriptor* instance_descs, uint32_t num_instances) {
  //
  set_instance_data(rd, &gh, &group_desc, sh, instance_descs, num_instances);
}

void foliage::set_tree_leaves_drawable_instance_meta_slow(
  TreeLeavesRenderData& rd, TreeLeavesDrawableInstanceSetHandle sh,
  uint32_t offset, bool can_fadeout, bool shadow_enabled) {
  //
  auto* inst_inds = get_instance_set_indices(rd, sh);
  assert(offset < inst_inds->count);
  const uint32_t index = offset + inst_inds->offset;

  auto& meta = rd.instance_meta[index];
  meta.enable_fixed_shadow = shadow_enabled;

  auto& lod_inst = rd.compute_lod_instances[index];
  lod_inst.translation_fadeout_allowed.w = float(can_fadeout);

  //  @NOTE: We don't technically need to invalidate here; we could keep track of scalar instance
  //  ranges, but this seems potentially more wasteful than not. We can profile later to be sure,
  //  although this method is intended to function as part of a graphics quality toggle rather than
  //  enable some game play feature.
  invalidate_modified_instance_ranges(rd);
}

void foliage::deactivate_tree_leaves_drawable_instances(
  TreeLeavesRenderData& rd, TreeLeavesDrawableInstanceSetHandle sh) {
  //
  auto* inst_inds = get_instance_set_indices(rd, sh);
  set_instance_data(rd, nullptr, nullptr, sh, nullptr, inst_inds->count);
}

void foliage::destroy_tree_leaves_drawable_instances(
  TreeLeavesRenderData& rd, TreeLeavesDrawableInstanceSetHandle sh) {
  //
  assert(sh.set_index < uint32_t(rd.instance_sets.size()));

  auto& inst_set = rd.instance_sets[sh.set_index];
  assert(inst_set.in_use);
  inst_set.in_use = false;

  const uint32_t beg = inst_set.offset;
  const uint32_t count = inst_set.count;
  const uint32_t end = beg + count;

  for (auto& set : rd.instance_sets) {
    if (set.offset >= beg) {
      set.offset -= count;
    }
  }

  destroy_range(rd.instances, beg, end);
  destroy_range(rd.instance_component_indices, beg, end);
  destroy_range(rd.compute_lod_instances, beg, end);
  destroy_range(rd.instance_meta, beg, end);

  invalidate_modified_instance_ranges(rd);
}

TreeLeavesRenderData* foliage::get_global_tree_leaves_render_data() {
  return &globals.render_data;
}

TreeLeavesDrawableHandle
foliage::create_tree_leaves_drawable(
  const TreeLeavesRenderInstanceDescriptor* instance_descs, uint32_t num_instances,
  const TreeLeavesRenderInstanceGroupDescriptor& group_desc) {
  //
  TreeLeavesDrawableHandle result{};
  result.group = create_tree_leaves_drawable_group(globals.render_data, group_desc);
  result.instances = create_tree_leaves_drawable_instances(
    globals.render_data, result.group, group_desc, instance_descs, num_instances);
  return result;
}

void foliage::destroy_tree_leaves_drawable(TreeLeavesDrawableHandle handle) {
  destroy_tree_leaves_drawable_instances(globals.render_data, handle.instances);
  destroy_tree_leaves_drawable_group(globals.render_data, handle.group);
}

void foliage::set_tree_leaves_scale_fraction(TreeLeavesDrawableGroupHandle handle, float scale01) {
  set_scale_fraction(globals.render_data, handle, scale01);
}

void foliage::set_tree_leaves_uv_offset(TreeLeavesDrawableGroupHandle handle, float uv_off) {
  set_uv_offset(globals.render_data, handle, uv_off);
}

void foliage::set_tree_leaves_color_image_mix_fraction(TreeLeavesDrawableGroupHandle handle, float f) {
  set_color_image_mix_fraction(globals.render_data, handle, f);
}

void foliage::set_tree_leaves_color_image_mix_fraction_all_groups(float f) {
  assert(f >= 0.0f && f <= 1.0f);

  if (globals.render_data.instance_groups.empty()) {
    return;
  }

  for (auto& group : globals.render_data.instance_groups) {
    set_color_image_mix(group, f);
  }

  globals.render_data.instance_groups_modified = true;
}

void foliage::increment_tree_leaves_uv_osc_time(TreeLeavesDrawableGroupHandle handle, float dt) {
  if (dt != 0.0f) {
    increment_uv_osc_time(globals.render_data, handle, dt);
  }
}

#if 0
void foliage::set_tree_leaves_fadeout_scale_distance_limits(TreeLeavesDrawableHandle handle,
                                                            const Vec2f& lims) {
  set_fadeout_scale_distance_limits(globals.render_data, handle, lims);
}

void foliage::set_tree_leaves_lod_transition_distance_limits(TreeLeavesDrawableHandle handle,
                                                             const Vec2f& lims) {
  set_lod_transition_distance_limits(globals.render_data, handle, lims);
}
#endif

void foliage::set_tree_leaves_alpha_image_index(TreeLeavesDrawableGroupHandle handle, uint16_t index) {
  set_alpha_image_index(globals.render_data, handle, index);
}

void foliage::set_tree_leaves_color_image0_index(TreeLeavesDrawableGroupHandle handle, uint8_t index) {
  set_color_image0_index(globals.render_data, handle, index);
}

void foliage::set_tree_leaves_color_image1_index(TreeLeavesDrawableGroupHandle handle, uint8_t index) {
  set_color_image1_index(globals.render_data, handle, index);
}

void foliage::set_tree_leaves_hidden(TreeLeavesDrawableGroupHandle handle, bool hidden) {
  set_hidden(globals.render_data, handle, hidden);
}

TreeLeavesRenderDataStats foliage::get_tree_leaves_render_data_stats(
  const TreeLeavesRenderData* data, uint32_t query_pool_size) {
  //
  TreeLeavesRenderDataStats result{};
  uint32_t min_n = ~0u;
  uint32_t max_n{};
  uint32_t tot_n{};
  uint32_t num_groups{};

  for (auto& inst : data->instance_component_indices) {
    result.num_inactive_instances += 1 - inst.is_active;
    result.num_active_instances += inst.is_active;
  }

  for (auto& set : data->instance_sets) {
    if (!set.in_use) {
      continue;
    }

    tot_n += set.count;
    min_n = std::min(min_n, set.count);
    max_n = std::max(max_n, set.count);

    if (query_pool_size > 0) {
      bool one = set.count % query_pool_size != 0;
      uint32_t num_pools = set.count / query_pool_size + uint32_t(one);
      result.num_would_overdraw_with_query_pool_size += num_pools * query_pool_size - set.count;
    }

    num_groups++;
  }

  if (tot_n > 0) {
    result.frac_would_overdraw_with_query_pool_size =
      double(result.num_would_overdraw_with_query_pool_size) / double(tot_n);
    result.mean_num_instances_per_group = double(tot_n) / double(num_groups);
  }

  result.min_num_instances_in_group = min_n;
  result.max_num_instances_in_group = max_n;

  return result;
}

GROVE_NAMESPACE_END
