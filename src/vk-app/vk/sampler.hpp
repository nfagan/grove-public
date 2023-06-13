#pragma once

#include "common.hpp"
#include "grove/vk/sampler.hpp"
#include <unordered_map>

namespace grove::vk {

size_t hash(const VkSamplerCreateInfo& info);
bool equal(const VkSamplerCreateInfo& a, const VkSamplerCreateInfo& b);

VkSamplerCreateInfo make_simple_sampler_create_info(VkFilter min_filter,
                                                    VkFilter mag_filter,
                                                    VkSamplerAddressMode address_mode_uvw);

struct SamplerCache {
public:
  struct Key {
    VkSamplerCreateInfo info;
  };
  struct Entry {
    Sampler sampler;
  };

  struct HashKey {
    size_t operator()(const Key& key) const noexcept {
      return hash(key.info);
    }
  };
  struct EqualKey {
    bool operator()(const Key& a, const Key& b) const noexcept {
      return equal(a.info, b.info);
    }
  };

  using Cache = std::unordered_map<Key, Entry, HashKey, EqualKey>;
public:
  Cache cache;
};

Result<VkSampler> require_sampler(SamplerCache& cache,
                                  VkDevice device,
                                  const VkSamplerCreateInfo& info);
void destroy_sampler_cache(SamplerCache* cache, VkDevice device);

}