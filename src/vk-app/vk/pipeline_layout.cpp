#include "pipeline_layout.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

Result<PipelineLayout>
vk::create_pipeline_layout(VkDevice device,
                           const ArrayView<const VkDescriptorSetLayout>& set_layouts,
                           const ArrayView<const VkPushConstantRange>& push_constants,
                           VkPipelineLayoutCreateFlags flags) {
  auto info = make_pipeline_layout_create_info(
    set_layouts.begin(),
    uint32_t(set_layouts.size()),
    push_constants.begin(),
    uint32_t(push_constants.size()),
    flags);
  return create_pipeline_layout(device, &info);
}

Result<VkPipelineLayout> vk::require_pipeline_layout(VkDevice device,
                                                     PipelineLayoutCache& cache,
                                                     const ArrayView<const VkDescriptorSetLayout>& set_layouts,
                                                     const ArrayView<const VkPushConstantRange>& push_constants,
                                                     VkPipelineLayoutCreateFlags flags) {
  //  @TODO: Revisit lookup approach. Prefer not to allocate here.
  PipelineLayoutCache::Key key{};
  key.flags = flags;
  key.descriptor_set_layouts.resize(set_layouts.size());
  key.push_constant_ranges.resize(push_constants.size());
  std::copy(set_layouts.begin(), set_layouts.end(), key.descriptor_set_layouts.begin());
  std::copy(push_constants.begin(), push_constants.end(), key.push_constant_ranges.begin());
  if (auto it = cache.cache.find(key); it != cache.cache.end()) {
    return it->second.layout.handle;
  } else {
    auto layout_res = create_pipeline_layout(device, set_layouts, push_constants, flags);
    if (!layout_res) {
      return error_cast<VkPipelineLayout>(layout_res);
    }
    PipelineLayoutCache::Entry entry{};
    entry.layout = layout_res.value;
    auto handle = entry.layout.handle;
    cache.cache[std::move(key)] = entry;
    return handle;
  }
}

void vk::destroy_pipeline_layout_cache(PipelineLayoutCache* cache, VkDevice device) {
  for (auto& [_, entry] : cache->cache) {
    vk::destroy_pipeline_layout(&entry.layout, device);
  }
  cache->cache.clear();
}

size_t vk::PipelineLayoutCache::HashKey::operator()(const Key& key) const noexcept {
  auto bind_hash = hash_range(key.descriptor_set_layouts.data(),
                              uint32_t(key.descriptor_set_layouts.size()));
  auto pc_hash = hash_range(key.push_constant_ranges.data(),
                            uint32_t(key.push_constant_ranges.size()));
  auto flag_hash = std::hash<uint32_t>{}(key.flags);
  return bind_hash ^ pc_hash ^ flag_hash;
}

bool vk::PipelineLayoutCache::EqualKey::operator()(const Key& a, const Key& b) const noexcept {
  if (a.flags != b.flags) {
    return false;
  }
  auto& layouts_a = a.descriptor_set_layouts;
  auto& layouts_b = b.descriptor_set_layouts;
  if (!equal_ranges(layouts_a.data(), uint32_t(layouts_a.size()),
                    layouts_b.data(), uint32_t(layouts_b.size()))) {
    return false;
  }
  auto& pc_a = a.push_constant_ranges;
  auto& pc_b = b.push_constant_ranges;
  if (!equal_ranges(pc_a.data(), uint32_t(pc_a.size()),
                    pc_b.data(), uint32_t(pc_b.size()))) {
    return false;
  }
  return true;
}

GROVE_NAMESPACE_END
