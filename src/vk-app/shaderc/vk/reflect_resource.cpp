#include "reflect_resource.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

VkShaderStageFlags to_vk_stage_flags(glsl::refl::ShaderStage::Flag stage) {
  VkShaderStageFlags res{0};
  if (stage & glsl::refl::ShaderStage::Vertex) {
    res |= VK_SHADER_STAGE_VERTEX_BIT;
  }
  if (stage & glsl::refl::ShaderStage::Fragment) {
    res |= VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  if (stage & glsl::refl::ShaderStage::Compute) {
    res |= VK_SHADER_STAGE_COMPUTE_BIT;
  }
  return res;
}

bool equal_descriptor_set_layout_bindings(const VkDescriptorSetLayoutBinding& a,
                                          const VkDescriptorSetLayoutBinding& b) {
  return a.binding == b.binding &&
         a.descriptorType == b.descriptorType &&
         a.descriptorCount == b.descriptorCount &&
         a.stageFlags == b.stageFlags &&
         a.pImmutableSamplers == b.pImmutableSamplers;
}

} //  anon

vk::refl::LayoutBindingsBySet
vk::refl::to_vk_descriptor_set_layout_bindings(const glsl::refl::LayoutInfosBySet& infos,
                                               const vk::refl::ToVkDescriptorType& to_descr_type) {
  vk::refl::LayoutBindingsBySet result;
  for (auto& [set, info] : infos) {
    //  @NOTE: `infos` might not contain set indices in range [0, infos.size()). It's legal for
    //  the shader to only use set 1 for example, in which case we need the array of bindings
    //  at index 0 to be empty.
    while (set >= uint32_t(result.size())) {
      result.emplace_back();
    }
    auto& bindings = result[set];
    for (auto& binding_info : info) {
      VkDescriptorSetLayoutBinding binding{};
      binding.binding = binding_info.binding;
      binding.descriptorType = to_descr_type(binding_info);
      binding.descriptorCount = binding_info.count;
      binding.stageFlags = to_vk_stage_flags(binding_info.stage);
      bindings.push_back(binding);
    }
  }
  return result;
}

VkDescriptorType vk::refl::to_vk_descriptor_type(glsl::refl::DescriptorType type) {
  switch (type) {
    case glsl::refl::DescriptorType::UniformBuffer:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case glsl::refl::DescriptorType::StorageBuffer:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case glsl::refl::DescriptorType::CombinedImageSampler:
      return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case glsl::refl::DescriptorType::StorageImage:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    default:
      assert(false);
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  }
}

VkDescriptorType vk::refl::identity_descriptor_type(const glsl::refl::DescriptorInfo& info) {
  return to_vk_descriptor_type(info.type);
}

VkDescriptorType
vk::refl::always_dynamic_uniform_buffer_descriptor_type(const glsl::refl::DescriptorInfo& info) {
  if (info.is_uniform_buffer()) {
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  } else {
    return identity_descriptor_type(info);
  }
}

VkDescriptorType
vk::refl::always_dynamic_storage_buffer_descriptor_type(const glsl::refl::DescriptorInfo& info) {
  if (info.is_storage_buffer()) {
    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
  } else {
    return identity_descriptor_type(info);
  }
}

bool vk::refl::matches_reflected(const LayoutBindingsBySet& reflected,
                                 uint32_t set,
                                 const VkDescriptorSetLayoutBinding* expected,
                                 uint32_t num_expected) {
  if (set < uint32_t(reflected.size())) {
    auto& bindings = reflected[set];
    if (bindings.size() == num_expected) {
      for (uint32_t i = 0; i < num_expected; i++) {
        const VkDescriptorSetLayoutBinding& refl_binding = bindings[i];
        const VkDescriptorSetLayoutBinding& expected_binding = expected[i];
        if (!equal_descriptor_set_layout_bindings(refl_binding, expected_binding)) {
          return false;
        }
      }
      return true;
    }
  }
  return false;
}

vk::refl::PushConstantRanges
vk::refl::to_vk_push_constant_ranges(const glsl::refl::PushConstantRanges& ranges) {
  vk::refl::PushConstantRanges result;
  for (auto& rng : ranges) {
    VkPushConstantRange range{};
    range.size = rng.size;
    range.offset = rng.offset;
    range.stageFlags = to_vk_stage_flags(rng.stage);
    result.push_back(range);
  }
  return result;
}

GROVE_NAMESPACE_END
