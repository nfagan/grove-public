#pragma once

#include "common.hpp"

namespace grove::vk {

struct Sampler {
  VkSampler handle{VK_NULL_HANDLE};
};

Result<Sampler> create_sampler(VkDevice device, const VkSamplerCreateInfo* info);
void destroy_sampler(Sampler* sampler, VkDevice device);

}