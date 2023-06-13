#pragma once

#include "../../vk/common.hpp"
#include "../reflect_resource.hpp"
#include <functional>
#include <vector>

namespace grove::vk::refl {

using LayoutBindingsBySet = std::vector<DescriptorSetLayoutBindings>;
using ToVkDescriptorType = std::function<VkDescriptorType(const glsl::refl::DescriptorInfo&)>;
using PushConstantRanges = DynamicArray<VkPushConstantRange, 2>;

LayoutBindingsBySet
to_vk_descriptor_set_layout_bindings(const glsl::refl::LayoutInfosBySet& infos,
                                     const ToVkDescriptorType& to_vk_descriptor_type);

PushConstantRanges to_vk_push_constant_ranges(const glsl::refl::PushConstantRanges& ranges);

bool matches_reflected(const LayoutBindingsBySet& reflected,
                       uint32_t set,
                       const VkDescriptorSetLayoutBinding* expected,
                       uint32_t num_expected);

VkDescriptorType to_vk_descriptor_type(glsl::refl::DescriptorType type);
VkDescriptorType identity_descriptor_type(const glsl::refl::DescriptorInfo& info);
VkDescriptorType always_dynamic_uniform_buffer_descriptor_type(const glsl::refl::DescriptorInfo& info);
VkDescriptorType always_dynamic_storage_buffer_descriptor_type(const glsl::refl::DescriptorInfo& info);

}