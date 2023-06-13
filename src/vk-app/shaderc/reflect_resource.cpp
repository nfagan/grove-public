#include "reflect_resource.hpp"
#include "reflect.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include <array>

GROVE_NAMESPACE_BEGIN

namespace {

[[maybe_unused]] constexpr const char* logging_id() {
  return "shaderc/reflect_resource";
}

std::array<glsl::refl::ShaderStage::Flag, 2> vert_frag_shader_stages() {
  return {{glsl::refl::ShaderStage::Vertex, glsl::refl::ShaderStage::Fragment}};
}

glsl::refl::DescriptorInfo descriptor_info_from_struct_resource(const glsl::StructResource& s,
                                                                glsl::refl::DescriptorType type) {
  glsl::refl::DescriptorInfo result{};
  result.set = s.set;
  result.binding = s.binding;
  result.type = type;
  result.count = 1;
  if (!s.array_sizes.empty()) {
    assert(s.array_sizes.size() == 1);
    result.count = s.array_sizes[0];
  }
  return result;
}

glsl::refl::DescriptorInfo descriptor_info_from_image_resource(const glsl::ImageResource& image,
                                                               glsl::refl::DescriptorType type) {
  glsl::refl::DescriptorInfo result{};
  result.set = image.set;
  result.binding = image.binding;
  result.type = type;
  result.count = 1;
  if (!image.array_sizes.empty()) {
    assert(image.array_sizes.size() == 1);
    result.count = image.array_sizes[0];
  }
  return result;
}

} //  anon

Optional<glsl::refl::LayoutInfosBySet>
glsl::reflect_descriptor_set_layouts(const refl::DescriptorInfo** infos,
                                     const refl::ShaderStage::Flag* stages,
                                     const uint32_t* info_counts,
                                     uint32_t num_infos) {
  using Result = std::unordered_map<uint32_t, glsl::refl::LayoutInfos>;
  struct Descriptor {
    refl::DescriptorType type;
    refl::ShaderStage::Flag stage;
    uint32_t count;
  };

  using Key = std::pair<uint32_t, uint32_t>;
  struct HashKey {
    std::size_t operator()(const Key& key) const noexcept {
      uint64_t v{key.first};
      uint64_t next{key.second};
      next <<= 32;
      v |= next;
      return std::hash<uint64_t>{}(v);
    }
  };

  std::unordered_map<Key, Descriptor, HashKey> descriptors;
  for (uint32_t i = 0; i < num_infos; i++) {
    const uint32_t info_count = info_counts[i];
    const auto stage = stages[i];

    for (uint32_t j = 0; j < info_count; j++) {
      const refl::DescriptorInfo& info = infos[i][j];
      const uint32_t s = info.set;
      const uint32_t b = info.binding;
      const uint32_t count = info.count;
      const auto type = info.type;

      if (s == glsl::missing_value()) {
        GROVE_LOG_ERROR_CAPTURE_META("Missing explicit set index decoration.", logging_id());
        return NullOpt{};
      } else if (b == glsl::missing_value()) {
        GROVE_LOG_ERROR_CAPTURE_META("Missing explicit binding index decoration.", logging_id());
        return NullOpt{};
      }

      auto key = Key{s, b};
      if (auto it = descriptors.find(key); it != descriptors.end()) {
        auto& existing_descr = it->second;
        if (existing_descr.type != type) {
          GROVE_LOG_ERROR_CAPTURE_META(
            "Different descriptor types at corresponding set/binding pair across stages.", logging_id());
          return NullOpt{};
        } else if ((existing_descr.stage & stage) == stage) {
          GROVE_LOG_ERROR_CAPTURE_META(
            "Duplicate descriptor set/binding pair within stage.", logging_id());
          return NullOpt{};
        } else if (existing_descr.count != count) {
          GROVE_LOG_ERROR_CAPTURE_META(
            "Inconsistent descriptor array size across stages.", logging_id());
          return NullOpt{};
        } else {
          existing_descr.stage |= stage;
        }
      } else {
        Descriptor new_descr{};
        new_descr.type = type;
        new_descr.stage = stage;
        new_descr.count = count;
        descriptors[key] = new_descr;
      }
    }
  }

  if (descriptors.empty()) {
    return Optional<Result>(Result{});
  }

  Result result;
  for (auto& [key, descr] : descriptors) {
    refl::DescriptorInfo info{};
    info.stage = descr.stage;
    info.type = descr.type;
    info.set = key.first;
    info.binding = key.second;
    info.count = descr.count;
    result[key.first].push_back(info);
  }

  for (auto& [set, descrs] : result) {
    std::sort(descrs.begin(), descrs.end(),
              [](const refl::DescriptorInfo& a, const refl::DescriptorInfo& b) {
      return a.binding < b.binding;
    });
  }

  return Optional<Result>(std::move(result));
}

Optional<glsl::refl::LayoutInfosBySet>
glsl::reflect_descriptor_set_layouts(const ReflectInfo** infos,
                                     const refl::ShaderStage::Flag* stages,
                                     uint32_t num_infos) {
  std::vector<std::vector<refl::DescriptorInfo>> descr_info(num_infos);
  std::vector<const refl::DescriptorInfo*> descr_info_ptrs(num_infos);
  std::vector<uint32_t> descr_info_sizes(num_infos);

  for (uint32_t i = 0; i < num_infos; i++) {
    auto& info = *infos[i];
    for (auto& buff : info.storage_buffers) {
      descr_info[i].push_back(descriptor_info_from_struct_resource(
        buff, refl::DescriptorType::StorageBuffer));
    }
    for (auto& buff : info.uniform_buffers) {
      descr_info[i].push_back(descriptor_info_from_struct_resource(
        buff, refl::DescriptorType::UniformBuffer));
    }
    for (auto& im : info.sampled_images) {
      descr_info[i].push_back(descriptor_info_from_image_resource(
        im, refl::DescriptorType::CombinedImageSampler));
    }
    for (auto& im : info.storage_images) {
      descr_info[i].push_back(descriptor_info_from_image_resource(
        im, refl::DescriptorType::StorageImage));
    }
    descr_info_ptrs[i] = descr_info[i].data();
    descr_info_sizes[i] = uint32_t(descr_info[i].size());
  }

  return reflect_descriptor_set_layouts(
    descr_info_ptrs.data(),
    stages,
    descr_info_sizes.data(),
    num_infos);
}

Optional<glsl::refl::LayoutInfosBySet>
glsl::reflect_vert_frag_descriptor_set_layouts(const glsl::ReflectInfo& vert_reflect_info,
                                               const glsl::ReflectInfo& frag_reflect_info) {
  const glsl::ReflectInfo* infos[2] = {&vert_reflect_info, &frag_reflect_info};
  const auto stages = vert_frag_shader_stages();
  return glsl::reflect_descriptor_set_layouts(infos, stages.data(), 2);
}

Optional<glsl::refl::LayoutInfosBySet>
glsl::reflect_compute_descriptor_set_layouts(const glsl::ReflectInfo& comp_reflect_info) {
  const auto stage = refl::ShaderStage::Compute;
  const glsl::ReflectInfo* infos[1] = {&comp_reflect_info};
  return glsl::reflect_descriptor_set_layouts(infos, &stage, 1);
}

glsl::refl::PushConstantRanges
glsl::reflect_push_constant_ranges(const glsl::PushConstantBuffer** buffers,
                                   const refl::ShaderStage::Flag* stages,
                                   const uint32_t* buffer_counts,
                                   uint32_t num_stages) {
  refl::PushConstantRanges result;
  for (uint32_t i = 0; i < num_stages; i++) {
    for (uint32_t j = 0; j < buffer_counts[i]; j++) {
      auto& buffer = buffers[i][j];
      glsl::refl::PushConstantRange range{};
      range.size = buffer.size;
      range.offset = 0;
      range.stage = stages[i];
      result.push_back(range);
    }
  }
  return result;
}

glsl::refl::PushConstantRanges
glsl::reflect_vert_frag_push_constant_ranges(const std::vector<PushConstantBuffer>& vert_pcs,
                                             const std::vector<PushConstantBuffer>& frag_pcs) {
  const glsl::PushConstantBuffer* buffers[2] = {vert_pcs.data(), frag_pcs.data()};
  uint32_t buffer_counts[2] = {uint32_t(vert_pcs.size()), uint32_t(frag_pcs.size())};
  auto stages = vert_frag_shader_stages();
  return reflect_push_constant_ranges(buffers, stages.data(), buffer_counts, 2);
}

glsl::refl::PushConstantRanges
glsl::reflect_compute_push_constant_ranges(const std::vector<PushConstantBuffer>& comp_pcs) {
  const glsl::PushConstantBuffer* buffers[1] = {comp_pcs.data()};
  uint32_t buffer_counts[1] = {uint32_t(comp_pcs.size())};
  auto stages = refl::ShaderStage::Compute;
  return reflect_push_constant_ranges(buffers, &stages, buffer_counts, 1);
}

GROVE_NAMESPACE_END
