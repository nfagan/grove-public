#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>
#include "grove/common/DynamicArray.hpp"
#include "grove/common/Optional.hpp"
#include "grove/visual/types.hpp"

namespace grove::vk {

struct RenderFrameInfo {
  uint64_t current_frame_id{};
  uint64_t finished_frame_id{~uint64_t(0)};     //  all processing associated with this frame id has finished.
  uint32_t current_frame_index{};
  uint32_t frame_queue_depth{};
};

using DescriptorSetLayoutBindings = DynamicArray<VkDescriptorSetLayoutBinding, 16>;

size_t hash_range(const VkDescriptorSetLayoutBinding* bindings, uint32_t num_bindings);
size_t hash_range(const VkDescriptorSetLayout* layouts, uint32_t num_layouts);
size_t hash_range(const VkPushConstantRange* ranges, uint32_t num_ranges);

bool equal_ranges(const VkDescriptorSetLayout* a, uint32_t num_a,
                  const VkDescriptorSetLayout* b, uint32_t num_b);
bool equal_ranges(const VkDescriptorSetLayoutBinding* a, uint32_t num_a,
                  const VkDescriptorSetLayoutBinding* b, uint32_t num_b);
bool equal_ranges(const VkPushConstantRange* a, uint32_t num_a,
                  const VkPushConstantRange* b, uint32_t num_b);

struct HashDescriptorSetLayoutBindings {
  std::size_t operator()(const DescriptorSetLayoutBindings& bindings) const noexcept;
};
struct EqualDescriptorSetLayoutBindings {
  bool operator()(const DescriptorSetLayoutBindings& a,
                  const DescriptorSetLayoutBindings& b) const noexcept;
};

enum class ShaderResourceType : uint16_t {
  UniformBuffer,
  DynamicUniformBuffer,
  StorageBuffer,
  DynamicStorageBuffer,
  CombinedImageSampler,
  UniformTexelBuffer,
  StorageImage,
};

struct PipelineStage {
  using Flag = uint32_t;
  static constexpr Flag TopOfPipe = 1u;
  static constexpr Flag DrawIndirect = 1u << 1u;
  static constexpr Flag VertexInput = 1u << 2u;
  static constexpr Flag VertexShader = 1u << 3u;
  static constexpr Flag TesselationControlShader = 1u << 4u;
  static constexpr Flag TesselationEvaluationShader = 1u << 5u;
  static constexpr Flag GeometryShader = 1u << 6u;
  static constexpr Flag FragmentShader = 1u << 7u;
  static constexpr Flag EarlyFragmentTests = 1u << 8u;
  static constexpr Flag LateFragmentTests = 1u << 9u;
  static constexpr Flag ColorAttachmentOutput = 1u << 10u;
  static constexpr Flag ComputeShader = 1u << 11u;
  static constexpr Flag Transfer = 1u << 12u;
  static constexpr Flag BottomOfPipe = 1u << 13;
};

struct PipelineStages {
  PipelineStage::Flag flags;
};

VkPipelineStageFlags to_vk_pipeline_stages(PipelineStages flags);

inline VkDescriptorType to_vk_descriptor_type(ShaderResourceType type) {
  switch (type) {
    case ShaderResourceType::UniformBuffer:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case ShaderResourceType::DynamicUniformBuffer:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case ShaderResourceType::StorageBuffer:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case ShaderResourceType::DynamicStorageBuffer:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    case ShaderResourceType::CombinedImageSampler:
      return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case ShaderResourceType::UniformTexelBuffer:
      return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    case ShaderResourceType::StorageImage:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    default:
      assert(false);
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  }
}

inline ShaderResourceType to_shader_resource_type(VkDescriptorType type) {
  switch (type) {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      return ShaderResourceType::UniformBuffer;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      return ShaderResourceType::DynamicUniformBuffer;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      return ShaderResourceType::StorageBuffer;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      return ShaderResourceType::DynamicStorageBuffer;
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      return ShaderResourceType::CombinedImageSampler;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      return ShaderResourceType::UniformTexelBuffer;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      return ShaderResourceType::StorageImage;
    default:
      assert(false);
      return ShaderResourceType::UniformBuffer;
  }
}

Optional<VkFormat> to_vk_format(const IntegralType* types, int num_types, IntConversion int_conv);
Optional<VkFormat> to_vk_format(const image::Channels& channels, IntConversion int_conv);
VkFormat to_vk_format(IntegralType type, int num_types, IntConversion int_conv);

}