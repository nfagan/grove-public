#include "common.hpp"
#include "grove/vk/common.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

inline size_t hash(const VkDescriptorSetLayoutBinding binding) {
  auto res = std::hash<uint64_t>{}((uint64_t) binding.descriptorType);
  res ^= std::hash<uint32_t>{}(binding.binding);
  return res;
}

inline size_t hash(const VkPushConstantRange range) {
  uint64_t v{range.offset};
  v |= (uint64_t(range.size) << 32);
  return std::hash<uint64_t>{}(v);
}

inline size_t hash(VkDescriptorSetLayout layout) {
  return std::hash<const void*>{}(static_cast<const void*>(layout));
}

bool equal(const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b) {
  return a.binding == b.binding &&
         a.descriptorType == b.descriptorType &&
         a.descriptorCount == b.descriptorCount &&
         a.stageFlags == b.stageFlags &&
         a.pImmutableSamplers == b.pImmutableSamplers;
}

bool equal(const VkPushConstantRange& a, const VkPushConstantRange& b) {
  return a.stageFlags == b.stageFlags &&
         a.size == b.size &&
         a.offset == b.offset;
}

template <typename T>
size_t hash_range(const T* elements, uint32_t size) {
  size_t result{std::hash<uint32_t>{}(size)};
  for (uint32_t i = 0; i < size; i++) {
    result ^= std::hash<size_t>{}(grove::hash(elements[i]));
  }
  return result;
}

template <typename T>
bool equal_ranges(const T* a, uint32_t num_a, const T* b, uint32_t num_b) {
  if (num_a != num_b) {
    return false;
  }
  for (uint32_t i = 0; i < num_a; i++) {
    if (!grove::equal(a[i], b[i])) {
      return false;
    }
  }
  return true;
}

} //  anon

size_t vk::hash_range(const VkDescriptorSetLayoutBinding* bindings, uint32_t num_bindings) {
  return grove::hash_range<VkDescriptorSetLayoutBinding>(bindings, num_bindings);
}

size_t vk::hash_range(const VkDescriptorSetLayout* layouts, uint32_t num_layouts) {
  return grove::hash_range<VkDescriptorSetLayout>(layouts, num_layouts);
}

size_t vk::hash_range(const VkPushConstantRange* ranges, uint32_t num_ranges) {
  return grove::hash_range<VkPushConstantRange>(ranges, num_ranges);
}

bool vk::equal_ranges(const VkDescriptorSetLayoutBinding* a, uint32_t num_a,
                      const VkDescriptorSetLayoutBinding* b, uint32_t num_b) {
  return grove::equal_ranges<VkDescriptorSetLayoutBinding>(a, num_a, b, num_b);
}

bool vk::equal_ranges(const VkDescriptorSetLayout* a, uint32_t num_a,
                      const VkDescriptorSetLayout* b, uint32_t num_b) {
  if (num_a != num_b) {
    return false;
  } else {
    return std::equal(a, a + num_a, b);
  }
}

bool vk::equal_ranges(const VkPushConstantRange* a, uint32_t num_a,
                      const VkPushConstantRange* b, uint32_t num_b) {
  return grove::equal_ranges<VkPushConstantRange>(a, num_a, b, num_b);
}

using Bindings = vk::DescriptorSetLayoutBindings;
std::size_t vk::HashDescriptorSetLayoutBindings::operator()(const Bindings& bindings) const noexcept {
  return hash_range(bindings.data(), uint32_t(bindings.size()));
}

bool vk::EqualDescriptorSetLayoutBindings::operator()(const DescriptorSetLayoutBindings& a,
                                                      const DescriptorSetLayoutBindings& b) const noexcept {
  return equal_ranges(a.data(), uint32_t(a.size()),
                      b.data(), uint32_t(b.size()));
}

VkPipelineStageFlags vk::to_vk_pipeline_stages(PipelineStages stages) {
  VkPipelineStageFlags res{0};
  uint32_t flags{stages.flags};
  if (flags & PipelineStage::TopOfPipe) {
    res |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  }
  if (flags & PipelineStage::DrawIndirect) {
    res |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
  }
  if (flags & PipelineStage::VertexInput) {
    res |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
  }
  if (flags & PipelineStage::VertexShader) {
    res |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
  }
  if (flags & PipelineStage::TesselationControlShader) {
    res |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
  }
  if (flags & PipelineStage::TesselationEvaluationShader) {
    res |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
  }
  if (flags & PipelineStage::GeometryShader) {
    res |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
  }
  if (flags & PipelineStage::FragmentShader) {
    res |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }
  if (flags & PipelineStage::EarlyFragmentTests) {
    res |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  }
  if (flags & PipelineStage::LateFragmentTests) {
    res |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  }
  if (flags & PipelineStage::ColorAttachmentOutput) {
    res |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  }
  if (flags & PipelineStage::ComputeShader) {
    res |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  }
  if (flags & PipelineStage::Transfer) {
    res |= VK_PIPELINE_STAGE_TRANSFER_BIT;
  }
  if (flags & PipelineStage::BottomOfPipe) {
    res |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  }
  return res;
}

Optional<VkFormat> vk::to_vk_format(const image::Channels& channels, IntConversion int_conv) {
  return to_vk_format(channels.channels.data(), channels.num_channels, int_conv);
}

VkFormat vk::to_vk_format(IntegralType type, int num_types, IntConversion conv) {
  (void) conv;
  //  @TODO:
  //  Byte,
  //  UnsignedByte,
  //  Short,
  //  UnsignedShort,
  //  Int,
  //  UnsignedInt,
  //  HalfFloat,
  //  Float,
  //  Double
  switch (type) {
    case IntegralType::Float: {
      switch (num_types) {
        case 1:
          return VK_FORMAT_R32_SFLOAT;
        case 2:
          return VK_FORMAT_R32G32_SFLOAT;
        case 3:
          return VK_FORMAT_R32G32B32_SFLOAT;
        case 4:
          return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
          GROVE_ASSERT(false);
          return VkFormat{};
      }
    }
    case IntegralType::UnconvertedUnsignedInt: {
      GROVE_ASSERT(conv == IntConversion::None);
      switch (num_types) {
        case 1:
          return VK_FORMAT_R32_UINT;
        case 2:
          return VK_FORMAT_R32G32_UINT;
        case 3:
          return VK_FORMAT_R32G32B32_UINT;
        case 4:
          return VK_FORMAT_R32G32B32A32_UINT;
        default:
          GROVE_ASSERT(false);
          return VkFormat{};
      }
    }
    case IntegralType::UnsignedByte: {
      GROVE_ASSERT(conv == IntConversion::UNorm);
      switch (num_types) {
        case 1:
          return VK_FORMAT_R8_UNORM;
        case 2:
          return VK_FORMAT_R8G8_UNORM;
        case 3:
          return VK_FORMAT_R8G8B8_UNORM;
        case 4:
          return VK_FORMAT_R8G8B8A8_UNORM;
        default:
          GROVE_ASSERT(false);
          return VkFormat{};
      }
    }
    default: {
      GROVE_ASSERT(false && "Unhandled.");
      return VkFormat{};
    }
  }
}

Optional<VkFormat> vk::to_vk_format(const IntegralType* types, int num_types, IntConversion int_conv) {
  if (num_types == 0) {
    GROVE_ASSERT(false && "At least 1 type required.");
    return NullOpt{};
  }
  for (int i = 1; i < num_types; i++) {
    if (types[i] != types[0]) {
      GROVE_ASSERT(false && "Mixed integral types not supported.");
      return NullOpt{};
    }
  }
  return Optional<VkFormat>(to_vk_format(types[0], num_types, int_conv));
}

GROVE_NAMESPACE_END