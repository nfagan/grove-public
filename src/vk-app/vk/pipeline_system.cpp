#include "pipeline_system.hpp"
#include "program.hpp"
#include "pipeline.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

using PipelineHandle = PipelineSystem::PipelineHandle;

PipelineHandle::PipelineHandle(PipelineSystem* sys, std::shared_ptr<Pipeline> pipeline) :
  system{sys},
  pipeline{std::move(pipeline)} {
  //
}

PipelineHandle::~PipelineHandle() {
  if (system && pipeline && pipeline->is_valid()) {
    system->destroy_pipeline(std::move(pipeline));
    system = nullptr;
    pipeline = nullptr;
  }
}

PipelineHandle::PipelineHandle(PipelineHandle&& other) noexcept :
  system{other.system},
  pipeline{std::move(other.pipeline)} {
  other.system = nullptr;
}

PipelineSystem::PipelineHandle& PipelineHandle::operator=(PipelineHandle&& other) noexcept {
  PipelineHandle tmp{std::move(other)};
  swap(*this, tmp);
  return *this;
}

void PipelineSystem::terminate(VkDevice device) {
  for (auto& pipe : pipelines) {
    if (pipe->is_valid()) {
      auto& pipeline = *pipe;
      vk::destroy_pipeline(&pipeline, device);
    }
  }

  vk::destroy_pipeline_layout_cache(&pipeline_layout_cache, device);
  for (int i = 0; i < num_descriptor_set_layout_caches; i++) {
    vk::destroy_descriptor_set_layout_cache(
      &descriptor_set_layout_caches_by_layout_create_flags[i], device);
  }
  pipelines.clear();
}

PipelineHandle PipelineSystem::emplace(Pipeline&& pipeline) {
  auto pipe = std::make_shared<Pipeline>(std::move(pipeline));
  pipelines.insert(pipe);
  PipelineHandle result{this, std::move(pipe)};
  return result;
}

void PipelineSystem::begin_frame(const RenderFrameInfo& info, VkDevice device) {
  frame_info = info;
  auto del_it = pending_destruction.begin();
  while (del_it != pending_destruction.end()) {
    auto& pend = *del_it;
    if (pend.frame_id == info.finished_frame_id) {
      auto& pend_pipeline = pend.pipeline;
      if (pend_pipeline->is_valid()) {
        auto& pipeline = *pend_pipeline;
        vk::destroy_pipeline(&pipeline, device);
      }
      pipelines.erase(pend.pipeline);
      del_it = pending_destruction.erase(del_it);
    } else {
      ++del_it;
    }
  }
}

void PipelineSystem::destroy_pipeline(PipelineHandle&& handle) {
  GROVE_ASSERT(handle.pipeline && handle.pipeline->is_valid());
  destroy_pipeline(std::move(handle.pipeline));
  handle = {};
}

void PipelineSystem::destroy_pipeline(std::shared_ptr<Pipeline>&& pipe) {
  GROVE_ASSERT(pipe && pipe->is_valid());
  PendingDestruction pend{};
  pend.pipeline = std::move(pipe);
  pend.frame_id = frame_info.current_frame_id;
  pending_destruction.push_back(std::move(pend));
}

DescriptorSetLayoutCache* PipelineSystem::require_cache(VkDescriptorSetLayoutCreateFlags flags) {
  for (int i = 0; i < num_descriptor_set_layout_caches; i++) {
    auto& cache = descriptor_set_layout_caches_by_layout_create_flags[i];
    if (cache.descriptor_set_layout_create_flags == flags) {
      return &cache;
    }
  }

  assert(num_descriptor_set_layout_caches < max_num_descriptor_set_layout_caches);
  auto& dst = descriptor_set_layout_caches_by_layout_create_flags[num_descriptor_set_layout_caches++];
  dst.descriptor_set_layout_create_flags = flags;
  return &dst;
}

Result<VkPipelineLayout>
PipelineSystem::require_pipeline_layout(VkDevice device,
                                        const ArrayView<const VkDescriptorSetLayout>& set_layouts,
                                        const ArrayView<const VkPushConstantRange>& push_constants,
                                        VkPipelineLayoutCreateFlags flags) {
  return grove::vk::require_pipeline_layout(
    device, pipeline_layout_cache, set_layouts, push_constants, flags);
}

Result<BorrowedDescriptorSetLayouts>
PipelineSystem::make_borrowed_descriptor_set_layouts(VkDevice device,
                                                     const ArrayView<const DescriptorSetLayoutBindings>& bindings,
                                                     const VkDescriptorSetLayoutCreateFlags* set_flags,
                                                     int num_flags) {
  if (!set_flags) {
    return grove::vk::make_borrowed_descriptor_set_layouts(
      descriptor_set_layout_caches_by_layout_create_flags[0], device, bindings);

  } else {
    assert(num_flags > 0);
    BorrowedDescriptorSetLayouts result;

    for (int i = 0; i < int(bindings.size()); i++) {
      auto flags = num_flags == 1 ? set_flags[0] : set_flags[i];
      auto* cache = require_cache(flags);
      auto* binds = bindings.data() + i;
      ArrayView<const DescriptorSetLayoutBindings> view{binds, binds + 1};
      auto res = grove::vk::make_borrowed_descriptor_set_layouts(*cache, device, view);
      if (!res) {
        return res;
      } else {
        result.append(res.value);
      }
    }

    return std::move(result);
  }
}

bool PipelineSystem::require_layouts(VkDevice device,
                                     const ArrayView<const VkPushConstantRange>& push_constants,
                                     const ArrayView<const DescriptorSetLayoutBindings>& bindings,
                                     VkPipelineLayout* pipe_layout,
                                     BorrowedDescriptorSetLayouts* desc_set_layout,
                                     const RequireLayoutParams& params) {
  VkDescriptorSetLayoutCreateFlags desc_set_flags{};
  VkDescriptorSetLayoutCreateFlags* flags_ptr{};
  int num_desc_set_flags{};
  if (params.enable_push_descriptors_in_descriptor_sets) {
    assert(false);
    desc_set_flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    num_desc_set_flags = 1;
    flags_ptr = &desc_set_flags;
  }

  auto desc_layout_res = make_borrowed_descriptor_set_layouts(
    device, bindings, flags_ptr, num_desc_set_flags);
  if (!desc_layout_res) {
    return false;
  }
  auto pipe_layout_res = require_pipeline_layout(
    device, make_view(desc_layout_res.value.layouts), push_constants);
  if (!pipe_layout_res) {
    return false;
  }
  *pipe_layout = pipe_layout_res.value;
  *desc_set_layout = std::move(desc_layout_res.value);
  return true;
}

bool PipelineSystem::require_layouts(VkDevice device, const glsl::VertFragProgramSource& source,
                                     PipelineData* dst, const RequireLayoutParams& params) {
  return require_layouts(
    device,
    make_view(source.push_constant_ranges),
    make_view(source.descriptor_set_layout_bindings),
    &dst->layout,
    &dst->descriptor_set_layouts, params);
}

bool PipelineSystem::require_layouts(VkDevice device, const glsl::ComputeProgramSource& source,
                                     PipelineData* dst, const RequireLayoutParams& params) {
  return require_layouts(
    device,
    make_view(source.push_constant_ranges),
    make_view(source.descriptor_set_layout_bindings),
    &dst->layout,
    &dst->descriptor_set_layouts, params);
}

Optional<PipelineSystem::PipelineData>
PipelineSystem::create_compute_pipeline_data(VkDevice device,
                                             const glsl::ComputeProgramSource& source,
                                             const RequireLayoutParams& params) {
  PipelineData result;
  if (!require_layouts(device, source, &result, params)) {
    return NullOpt{};
  }

  auto pipe = create_compute_pipeline(device, source.bytecode, result.layout);
  if (!pipe) {
    return NullOpt{};
  }

  result.pipeline = emplace(std::move(pipe.value));
  return Optional<PipelineSystem::PipelineData>(std::move(result));
}

size_t PipelineSystem::num_pipelines() const {
  return pipelines.size();
}

size_t PipelineSystem::num_pipeline_layouts() const {
  return pipeline_layout_cache.cache.size();
}

size_t PipelineSystem::num_descriptor_set_layouts() const {
  size_t s{};
  for (int i = 0; i < num_descriptor_set_layout_caches; i++) {
    s += descriptor_set_layout_caches_by_layout_create_flags[i].cache.size();
  }
  return s;
}

GROVE_NAMESPACE_END
