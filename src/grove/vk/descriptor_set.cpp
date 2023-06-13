#include "descriptor_set.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

vk::Result<vk::DescriptorPool> vk::create_descriptor_pool(VkDevice device,
                                                          const VkDescriptorPoolCreateInfo* info) {
  VkDescriptorPool handle;
  auto res = vkCreateDescriptorPool(device, info, GROVE_VK_ALLOC, &handle);
  if (res != VK_SUCCESS) {
    return {res, "Failed to create descriptor pool."};
  } else {
    DescriptorPool result{};
    result.handle = handle;
    return result;
  }
}

void vk::destroy_descriptor_pool(DescriptorPool* pool, VkDevice device) {
  if (device != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, pool->handle, GROVE_VK_ALLOC);
    pool->handle = VK_NULL_HANDLE;
  } else {
    GROVE_ASSERT(pool->handle == VK_NULL_HANDLE);
  }
}

void vk::reset_descriptor_pool(VkDevice device,
                               VkDescriptorPool pool,
                               VkDescriptorPoolResetFlags flags) {
  GROVE_VK_CHECK(vkResetDescriptorPool(device, pool, flags))
}

vk::Result<vk::DescriptorSetLayout>
vk::create_descriptor_set_layout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* info) {
  VkDescriptorSetLayout handle;
  auto res = vkCreateDescriptorSetLayout(device, info, GROVE_VK_ALLOC, &handle);
  if (res != VK_SUCCESS) {
    return {res, "Failed to create descriptor set layout."};
  } else {
    DescriptorSetLayout result{};
    result.handle = handle;
    return result;
  }
}

void vk::destroy_descriptor_set_layout(vk::DescriptorSetLayout* layout, VkDevice device) {
  if (device != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, layout->handle, GROVE_VK_ALLOC);
    layout->handle = VK_NULL_HANDLE;
  } else {
    GROVE_ASSERT(layout->handle == VK_NULL_HANDLE);
  }
}

VkDescriptorSetLayoutCreateInfo
vk::make_descriptor_set_layout_create_info(uint32_t binding_count,
                                           const VkDescriptorSetLayoutBinding* bindings,
                                           VkDescriptorSetLayoutCreateFlags flags) {
  VkDescriptorSetLayoutCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  create_info.bindingCount = binding_count;
  create_info.pBindings = bindings;
  create_info.flags = flags;
  return create_info;
}

VkDescriptorPoolSize vk::make_descriptor_pool_size(VkDescriptorType type, uint32_t count) {
  VkDescriptorPoolSize pool_size{};
  pool_size.type = type;
  pool_size.descriptorCount = count;
  return pool_size;
}

VkDescriptorSetAllocateInfo vk::make_descriptor_set_allocate_info(VkDescriptorPool pool,
                                                                  const VkDescriptorSetLayout* layouts,
                                                                  uint32_t num_layouts) {
  VkDescriptorSetAllocateInfo result{};
  result.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  result.descriptorPool = pool;
  result.pSetLayouts = layouts;
  result.descriptorSetCount = num_layouts;
  return result;
}

vk::Error vk::allocate_descriptor_sets(VkDevice device,
                                       const VkDescriptorSetAllocateInfo* info,
                                       VkDescriptorSet* out) {
  auto res = vkAllocateDescriptorSets(device, info, out);
  if (res != VK_SUCCESS) {
    return {res, "Failed to allocate descriptor sets."};
  } else {
    return {};
  }
}

GROVE_NAMESPACE_END
