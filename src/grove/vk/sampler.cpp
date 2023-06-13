#include "sampler.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

vk::Result<vk::Sampler> vk::create_sampler(VkDevice device, const VkSamplerCreateInfo* info) {
  VkSampler handle;
  auto res = vkCreateSampler(device, info, GROVE_VK_ALLOC, &handle);
  if (res != VK_SUCCESS) {
    return {res, "Failed to create sampler."};
  } else {
    Sampler sampler{};
    sampler.handle = handle;
    return sampler;
  }
}

void vk::destroy_sampler(Sampler* sampler, VkDevice device) {
  if (device != VK_NULL_HANDLE) {
    vkDestroySampler(device, sampler->handle, GROVE_VK_ALLOC);
    sampler->handle = VK_NULL_HANDLE;
  } else {
    GROVE_ASSERT(sampler->handle == VK_NULL_HANDLE);
  }
}

GROVE_NAMESPACE_END
