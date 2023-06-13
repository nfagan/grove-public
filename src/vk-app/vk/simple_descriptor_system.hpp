#pragma once

#include "descriptor_set.hpp"

namespace grove::vk {

struct SimpleDescriptorSystem {
public:
  struct CachedDescriptorSet {
    vk::DescriptorSetScaffold scaffold;
    VkDescriptorSet set{};
  };

  struct FrameContext {
    DescriptorPoolAllocator pool_allocator;
    DynamicArray<CachedDescriptorSet, 4> cached_descriptor_sets;
    float ms_spent_requiring_descriptor_sets{};
  };

public:
  void initialize(VkDevice device, uint32_t frame_queue_depth);
  void begin_frame(VkDevice device, uint32_t frame_index);
  void terminate(VkDevice device);

  Optional<VkDescriptorSet> require_updated_descriptor_set(
    VkDevice device,
    vk::DescriptorSetLayoutCache* layout_cache,
    const DescriptorSetScaffold& desired_scaffold,
    const ArrayView<const VkDescriptorSetLayoutBinding>& pipeline_bindings,
    bool disable_cache);

  uint32_t total_num_descriptor_pools() const;
  uint32_t total_num_descriptor_sets() const;

public:
  DynamicArray<FrameContext, 3> frame_contexts;
  uint32_t current_frame_index{};
  float max_ms_spent_requiring_descriptor_sets{0.0f};
  float latest_ms_spent_requiring_descriptor_sets{0.0f};
};

}