#pragma once

#include "../render/render_tree_leaves.hpp"
#include "../render/frustum_cull_data.hpp"
#include "../render/foliage_occlusion.hpp"
#include "grove/common/Optional.hpp"
#include <vector>
#include <deque>

namespace grove::tree {
struct Internode;
}

namespace grove::foliage {

struct TreeLeavesPoolAllocator {
  std::deque<TreeLeavesDrawableInstanceSetHandle> free_sets;
  std::deque<TreeLeavesDrawableGroupHandle> free_groups;
};

struct PooledLeafComponents {
  TreeLeavesDrawableGroupHandle group;
  std::vector<TreeLeavesDrawableInstanceSetHandle> sets;
};

struct FoliageDrawableComponents {
  void set_lod(int lod);
  void set_scale_fraction(float f);
  void set_uv_offset(float f);
  void set_color_mix_fraction(float f);
  void increment_uv_osc_time(float t);
  void set_hidden(bool hide);
  uint32_t num_instances() const {
    return num_clusters * num_steps * num_instances_per_step;
  }

  Optional<foliage::TreeLeavesDrawableHandle> leaves_drawable;
  Optional<PooledLeafComponents> pooled_leaf_components;
  Optional<cull::FrustumCullGroupHandle> cull_group_handle;
  Optional<foliage_occlusion::ClusterGroupHandle> occlusion_cluster_group_handle;
  uint32_t num_clusters{};
  uint32_t num_steps{};
  uint32_t num_instances_per_step{};
};

enum class FoliageDistributionStrategy {
  None = 0,
  TightHighN,
  TightLowN,
  Hanging,
  ThinCurledLowN,
};

struct CreateFoliageDrawableComponentParams {
  float initial_scale01;
  float uv_offset;
  float color_image_mix01;
  int preferred_lod;
  FoliageDistributionStrategy distribution_strategy;
  uint16_t alpha_image_index;
  uint16_t color_image0_index;
  uint16_t color_image1_index;
};

FoliageDrawableComponents create_foliage_drawable_components_from_internodes(
  cull::FrustumCullData* frustum_cull_data,
  foliage_occlusion::FoliageOcclusionSystem* occlusion_system,
  TreeLeavesPoolAllocator* pool_alloc,
  const CreateFoliageDrawableComponentParams& params,
  const std::vector<tree::Internode>& internodes, const std::vector<int>& subset_internodes);

void destroy_foliage_drawable_components(
  FoliageDrawableComponents* components,
  cull::FrustumCullData* frustum_cull_data,
  foliage_occlusion::FoliageOcclusionSystem* occlusion_system,
  TreeLeavesPoolAllocator* pool_alloc);

#if 0
void seed_urandf(uint32_t seed);
float get_urandf();
#endif

}