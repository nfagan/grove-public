#pragma once

#include "common.hpp"
#include "grove/vk/shader.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/common/DynamicArray.hpp"
#include <unordered_map>

namespace grove::vk {

struct PipelineLayoutCache {
public:
  struct Key {
    VkPipelineLayoutCreateFlags flags{};
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
    std::vector<VkPushConstantRange> push_constant_ranges;
  };
  struct Entry {
    vk::PipelineLayout layout;
  };
  struct HashKey {
    size_t operator()(const Key& key) const noexcept;
  };
  struct EqualKey {
    bool operator()(const Key& a, const Key& b) const noexcept;
  };
  using Cache = std::unordered_map<Key, Entry, HashKey, EqualKey>;

public:
  Cache cache;
};

Result<PipelineLayout> create_pipeline_layout(VkDevice device,
                                              const ArrayView<const VkDescriptorSetLayout>& set_layouts,
                                              const ArrayView<const VkPushConstantRange>& push_constants,
                                              VkPipelineLayoutCreateFlags flags = 0);

Result<VkPipelineLayout> require_pipeline_layout(VkDevice device,
                                                 PipelineLayoutCache& cache,
                                                 const ArrayView<const VkDescriptorSetLayout>& set_layouts,
                                                 const ArrayView<const VkPushConstantRange>& push_constants,
                                                 VkPipelineLayoutCreateFlags flags = 0);

void destroy_pipeline_layout_cache(PipelineLayoutCache* cache, VkDevice device);

}