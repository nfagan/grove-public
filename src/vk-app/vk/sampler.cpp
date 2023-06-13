#include "sampler.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

VkSamplerCreateInfo make_empty_sampler_create_info() {
  VkSamplerCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  return result;
}

} //  anon

VkSamplerCreateInfo vk::make_simple_sampler_create_info(VkFilter min_filter,
                                                        VkFilter mag_filter,
                                                        VkSamplerAddressMode address_mode_uvw) {
  auto res = make_empty_sampler_create_info();
  res.minFilter = min_filter;
  res.magFilter = mag_filter;
  res.addressModeU = address_mode_uvw;
  res.addressModeV = address_mode_uvw;
  res.addressModeW = address_mode_uvw;
  return res;
}

size_t vk::hash(const VkSamplerCreateInfo& info) {
  auto u32_hash = [](uint32_t a) {
    return std::hash<uint32_t>{}(a);
  };
  VkSamplerAddressMode modes[3] = {
    info.addressModeU,
    info.addressModeV,
    info.addressModeW
  };
  VkFilter filters[2] = {
    info.minFilter,
    info.magFilter
  };

  size_t hash{};
  for (auto mode : modes) {
    hash ^= u32_hash((uint32_t) mode);
  }
  for (auto filter : filters) {
    hash ^= u32_hash((uint32_t) filter);
  }
  hash ^= u32_hash((uint32_t) info.compareOp);
  hash ^= u32_hash((uint32_t) info.mipmapMode);
  return hash;
}

bool vk::equal(const VkSamplerCreateInfo& a, const VkSamplerCreateInfo& b) {
  return a.flags == b.flags &&
         a.magFilter == b.magFilter &&
         a.minFilter == b.minFilter &&
         a.mipmapMode == b.mipmapMode &&
         a.addressModeU == b.addressModeU &&
         a.addressModeV == b.addressModeV &&
         a.addressModeW == b.addressModeW &&
         a.mipLodBias == b.mipLodBias &&
         a.anisotropyEnable == b.anisotropyEnable &&
         a.maxAnisotropy == b.maxAnisotropy &&
         a.compareEnable == b.compareEnable &&
         a.compareOp == b.compareOp &&
         a.minLod == b.minLod &&
         a.maxLod == b.maxLod &&
         a.borderColor == b.borderColor &&
         a.unnormalizedCoordinates == b.unnormalizedCoordinates;
}

Result<VkSampler> vk::require_sampler(SamplerCache& cache,
                                      VkDevice device,
                                      const VkSamplerCreateInfo& info) {
  SamplerCache::Key key{};
  key.info = info;
  if (auto it = cache.cache.find(key); it != cache.cache.end()) {
    return it->second.sampler.handle;
  } else {
    auto res = create_sampler(device, &info);
    if (!res) {
      return error_cast<VkSampler>(res);
    }
    SamplerCache::Entry entry{};
    entry.sampler = res.value;
    auto handle = res.value.handle;
    cache.cache[key] = entry;
    return handle;
  }
}

void vk::destroy_sampler_cache(SamplerCache* cache, VkDevice device) {
  for (auto& [key, entry] : cache->cache) {
    vk::destroy_sampler(&entry.sampler, device);
  }
  cache->cache.clear();
}

GROVE_NAMESPACE_END
