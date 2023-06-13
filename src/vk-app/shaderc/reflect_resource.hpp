#pragma once

#include "grove/common/Optional.hpp"
#include "grove/common/DynamicArray.hpp"
#include <unordered_map>

namespace grove::glsl {

struct ImageResource;
struct StructResource;
struct ReflectInfo;
struct PushConstantBuffer;

namespace refl {
  struct ShaderStage {
    using Flag = uint32_t;
    static constexpr Flag Vertex = 1u;
    static constexpr Flag Fragment = 1u << 1u;
    static constexpr Flag Compute = 1u << 2u;
  };

  enum class DescriptorType {
    UniformBuffer,
    StorageBuffer,
    CombinedImageSampler,
    StorageImage
  };

  struct DescriptorInfo {
    bool is_uniform_buffer() const {
      return type == DescriptorType::UniformBuffer;
    }
    bool is_storage_buffer() const {
      return type == DescriptorType::StorageBuffer;
    }
    bool is_combined_image_sampler() const {
      return type == DescriptorType::CombinedImageSampler;
    }
    bool is_storage_image() const {
      return type == DescriptorType::StorageImage;
    }

    ShaderStage::Flag stage;
    DescriptorType type;
    uint32_t set;
    uint32_t binding;
    uint32_t count;
  };

  struct PushConstantRange {
    ShaderStage::Flag stage;
    uint32_t offset;
    uint32_t size;
  };

  using LayoutInfos = DynamicArray<DescriptorInfo, 16>;
  using LayoutInfosBySet = std::unordered_map<uint32_t, refl::LayoutInfos>;
  using PushConstantRanges = DynamicArray<PushConstantRange, 2>;
}

Optional<refl::LayoutInfosBySet>
reflect_descriptor_set_layouts(const refl::DescriptorInfo** infos,
                               const refl::ShaderStage::Flag* stages,
                               const uint32_t* info_counts,
                               uint32_t num_infos);

Optional<refl::LayoutInfosBySet>
reflect_descriptor_set_layouts(const ReflectInfo** infos,
                               const refl::ShaderStage::Flag* stages,
                               uint32_t num_infos);

Optional<refl::LayoutInfosBySet>
reflect_vert_frag_descriptor_set_layouts(const glsl::ReflectInfo& vert_reflect_info,
                                         const glsl::ReflectInfo& frag_reflect_info);

Optional<refl::LayoutInfosBySet>
reflect_compute_descriptor_set_layouts(const glsl::ReflectInfo& comp_reflect_info);

refl::PushConstantRanges reflect_push_constant_ranges(const glsl::PushConstantBuffer** buffers,
                                                      const refl::ShaderStage::Flag* stages,
                                                      const uint32_t* buffer_counts,
                                                      uint32_t num_stages);

refl::PushConstantRanges
reflect_vert_frag_push_constant_ranges(const std::vector<PushConstantBuffer>& vert_pcs,
                                       const std::vector<PushConstantBuffer>& frag_pcs);

refl::PushConstantRanges
reflect_compute_push_constant_ranges(const std::vector<PushConstantBuffer>& comp_pcs);

}