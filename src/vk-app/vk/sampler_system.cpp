#include "sampler_system.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

void vk::SamplerSystem::terminate(VkDevice device) {
  destroy_sampler_cache(&sampler_cache, device);
}

VkSampler vk::SamplerSystem::require(VkDevice device, const VkSamplerCreateInfo& info) {
  auto res = require_sampler(sampler_cache, device, info);
  GROVE_ASSERT(res);
  return res.value;
}

VkSampler vk::SamplerSystem::require_simple(VkDevice device,
                                            VkFilter min_filter,
                                            VkFilter mag_filter,
                                            VkSamplerAddressMode addr_mode) {
  auto info = make_simple_sampler_create_info(min_filter, mag_filter, addr_mode);
  return require(device, info);
}

VkSampler vk::SamplerSystem::require_linear_edge_clamp(VkDevice device) {
  return require_simple(
    device,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
}

VkSampler vk::SamplerSystem::require_linear_repeat_mip_map_nearest(VkDevice device) {
  auto info = make_simple_sampler_create_info(
    VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
  info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  info.maxLod = VK_LOD_CLAMP_NONE;
  return require(device, info);
}

VkSampler vk::SamplerSystem::require_linear_edge_clamp_mip_map_nearest(VkDevice device) {
  auto info = make_simple_sampler_create_info(
    VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
  info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  info.maxLod = VK_LOD_CLAMP_NONE;
  return require(device, info);
}

VkSampler vk::SamplerSystem::require_linear_edge_clamp_mip_map_linear(VkDevice device) {
  auto info = make_simple_sampler_create_info(
    VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
  info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  info.maxLod = VK_LOD_CLAMP_NONE;
  return require(device, info);
}

VkSampler vk::SamplerSystem::require_linear_repeat(VkDevice device) {
  return require_simple(
    device,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,
    VK_SAMPLER_ADDRESS_MODE_REPEAT);
}

VkSampler vk::SamplerSystem::require_nearest_edge_clamp(VkDevice device) {
  return require_simple(
    device,
    VK_FILTER_NEAREST,
    VK_FILTER_NEAREST,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
}

size_t vk::SamplerSystem::num_samplers() const {
  return sampler_cache.cache.size();
}

GROVE_NAMESPACE_END
