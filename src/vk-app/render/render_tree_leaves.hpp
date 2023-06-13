#pragma once

#include "grove/math/vector.hpp"

namespace grove::foliage {

struct TreeLeavesRenderData;
struct TreeLeavesRenderDataStats;

struct TreeLeavesDrawableGroupHandle {
  uint32_t group_index;
};

struct TreeLeavesDrawableInstanceSetHandle {
  uint32_t set_index;
};

struct TreeLeavesDrawableHandle {
  TreeLeavesDrawableGroupHandle group;
  TreeLeavesDrawableInstanceSetHandle instances;
};

struct TreeLeavesRenderInstanceDescriptor {
  struct WindNode {
    Vec4<uint32_t> info0;
    Vec4<uint32_t> info1;
    Vec4<uint32_t> info2;
  };

  bool is_active;

  Vec3f translation;
  Vec3f forwards;
  Vec3f right;
  float rand01;
  float y_rotation;
  float z_rotation;

  WindNode wind_node;

  uint32_t frustum_cull_group;
  uint32_t frustum_cull_instance_index;
  uint16_t occlusion_cull_group;
  uint16_t occlusion_cull_cluster_index;
  uint8_t occlusion_cull_instance_index;

  bool can_fadeout;
  bool enable_fixed_shadow;
};

struct TreeLeavesRenderInstanceGroupDescriptor {
  uint16_t alpha_image_index;
  uint16_t color_image0_index;
  uint16_t color_image1_index;
  Vec3f aabb_p0;
  Vec3f aabb_p1;
  float curl_scale;
  float global_scale;
  float uv_offset;
  float color_image_mix;
  Vec2f lod_distance_limits;
  Vec2f fadeout_scale_distance_limits;
};

TreeLeavesDrawableGroupHandle create_tree_leaves_drawable_group(
  TreeLeavesRenderData& rd, const TreeLeavesRenderInstanceGroupDescriptor& group_desc);

void set_tree_leaves_drawable_group_data(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle gh,
  const TreeLeavesRenderInstanceGroupDescriptor& group_desc);

void destroy_tree_leaves_drawable_group(TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle gh);

TreeLeavesDrawableInstanceSetHandle create_tree_leaves_drawable_instances(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle group,
  const TreeLeavesRenderInstanceGroupDescriptor& group_desc,
  const TreeLeavesRenderInstanceDescriptor* instance_descs, uint32_t num_instances);

TreeLeavesDrawableInstanceSetHandle reserve_tree_leaves_drawable_instance_data(
  TreeLeavesRenderData& rd, uint32_t num_instances);

void set_tree_leaves_drawable_instance_data(
  TreeLeavesRenderData& rd, TreeLeavesDrawableGroupHandle group,
  const TreeLeavesRenderInstanceGroupDescriptor& group_desc,
  TreeLeavesDrawableInstanceSetHandle sh,
  const TreeLeavesRenderInstanceDescriptor* instance_descs, uint32_t num_instances);

void set_tree_leaves_drawable_instance_meta_slow(
  TreeLeavesRenderData& rd,
  TreeLeavesDrawableInstanceSetHandle sh, uint32_t offset,
  bool can_fadeout, bool shadow_enabled);

void deactivate_tree_leaves_drawable_instances(
  TreeLeavesRenderData& rd, TreeLeavesDrawableInstanceSetHandle sh);

void destroy_tree_leaves_drawable_instances(
  TreeLeavesRenderData& rd, TreeLeavesDrawableInstanceSetHandle sh);

TreeLeavesDrawableHandle
create_tree_leaves_drawable(const TreeLeavesRenderInstanceDescriptor* instance_descs,
                            uint32_t num_instances,
                            const TreeLeavesRenderInstanceGroupDescriptor& group_desc);
void destroy_tree_leaves_drawable(TreeLeavesDrawableHandle handle);

void set_tree_leaves_uv_offset(TreeLeavesDrawableGroupHandle handle, float uv_off);
void set_tree_leaves_color_image_mix_fraction(TreeLeavesDrawableGroupHandle handle, float f);
void set_tree_leaves_color_image_mix_fraction_all_groups(float f);
void increment_tree_leaves_uv_osc_time(TreeLeavesDrawableGroupHandle handle, float dt);
void set_tree_leaves_scale_fraction(TreeLeavesDrawableGroupHandle handle, float scale01);
#if 0
void set_tree_leaves_fadeout_scale_distance_limits(TreeLeavesDrawableHandle handle, const Vec2f& lims);
void set_tree_leaves_lod_transition_distance_limits(TreeLeavesDrawableHandle handle, const Vec2f& lims);
#endif
void set_tree_leaves_alpha_image_index(TreeLeavesDrawableGroupHandle handle, uint16_t index);
void set_tree_leaves_color_image0_index(TreeLeavesDrawableGroupHandle handle, uint8_t index);
void set_tree_leaves_color_image1_index(TreeLeavesDrawableGroupHandle handle, uint8_t index);
void set_tree_leaves_hidden(TreeLeavesDrawableGroupHandle handle, bool hidden);

TreeLeavesRenderDataStats get_tree_leaves_render_data_stats(
  const TreeLeavesRenderData* data, uint32_t query_pool_size);

TreeLeavesRenderData* get_global_tree_leaves_render_data();

}