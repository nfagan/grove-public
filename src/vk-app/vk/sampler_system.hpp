#pragma once

#include "sampler.hpp"

namespace grove::vk {

class SamplerSystem {
public:
  void terminate(VkDevice device);
  VkSampler require(VkDevice device, const VkSamplerCreateInfo& info);
  VkSampler require_simple(VkDevice device,
                           VkFilter min_filter,
                           VkFilter mag_filter,
                           VkSamplerAddressMode address_mode_uvw);
  VkSampler require_linear_edge_clamp(VkDevice device);
  VkSampler require_linear_repeat_mip_map_nearest(VkDevice device);
  VkSampler require_linear_edge_clamp_mip_map_nearest(VkDevice device);
  VkSampler require_linear_edge_clamp_mip_map_linear(VkDevice device);
  VkSampler require_linear_repeat(VkDevice device);
  VkSampler require_nearest_edge_clamp(VkDevice device);
  size_t num_samplers() const;

private:
  SamplerCache sampler_cache;
};

}