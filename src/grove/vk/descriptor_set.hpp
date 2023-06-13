#pragma once

#include "common.hpp"

namespace grove::vk {

struct DescriptorSetLayout {
  VkDescriptorSetLayout handle{VK_NULL_HANDLE};
};

struct DescriptorPool {
  VkDescriptorPool handle{VK_NULL_HANDLE};
};

Result<DescriptorPool> create_descriptor_pool(VkDevice device,
                                              const VkDescriptorPoolCreateInfo* info);
void destroy_descriptor_pool(DescriptorPool* pool, VkDevice device);
void reset_descriptor_pool(VkDevice device, VkDescriptorPool pool, VkDescriptorPoolResetFlags = 0);

Result<DescriptorSetLayout> create_descriptor_set_layout(VkDevice device,
                                                         const VkDescriptorSetLayoutCreateInfo* info);
void destroy_descriptor_set_layout(vk::DescriptorSetLayout* layout, VkDevice device);

VkDescriptorPoolSize make_descriptor_pool_size(VkDescriptorType type, uint32_t descriptor_count);
inline VkDescriptorPoolCreateInfo make_empty_descriptor_pool_create_info() {
  VkDescriptorPoolCreateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  return result;
}

inline VkWriteDescriptorSet make_empty_write_descriptor_set() {
  VkWriteDescriptorSet result{};
  result.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  return result;
}

inline VkDescriptorSetLayoutBinding make_descriptor_set_layout_binding(uint32_t binding,
                                                                       VkDescriptorType type,
                                                                       uint32_t count,
                                                                       VkShaderStageFlags stage_flags) {
  VkDescriptorSetLayoutBinding result{};
  result.binding = binding;
  result.descriptorType = type;
  result.descriptorCount = count;
  result.stageFlags = stage_flags;
  return result;
}

VkDescriptorSetLayoutCreateInfo
make_descriptor_set_layout_create_info(uint32_t binding_count,
                                       const VkDescriptorSetLayoutBinding* bindings,
                                       VkDescriptorSetLayoutCreateFlags flags = 0);

VkDescriptorSetAllocateInfo make_descriptor_set_allocate_info(VkDescriptorPool pool,
                                                              const VkDescriptorSetLayout* layouts,
                                                              uint32_t num_layouts);

[[nodiscard]] Error allocate_descriptor_sets(VkDevice device,
                                             const VkDescriptorSetAllocateInfo* info,
                                             VkDescriptorSet* out);

}