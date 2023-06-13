#pragma once

#include "common.hpp"
#include "pipeline_layout.hpp"
#include "descriptor_set.hpp"
#include <unordered_set>
#include <vector>

namespace grove::glsl {
struct VertFragProgramSource;
struct ComputeProgramSource;
}

namespace grove::vk {

class PipelineSystem {
public:
  struct RequireLayoutParams {
    bool enable_push_descriptors_in_descriptor_sets;
  };

  class PipelineHandle {
    friend class PipelineSystem;
  private:
    PipelineHandle(PipelineSystem* sys, std::shared_ptr<Pipeline> pipeline);

  public:
    PipelineHandle() = default;
    PipelineHandle(PipelineHandle&& other) noexcept;
    GROVE_NONCOPYABLE(PipelineHandle)
    PipelineHandle& operator=(PipelineHandle&& other) noexcept;
    ~PipelineHandle();

    bool is_valid() const {
      return pipeline && pipeline->is_valid();
    }
    Pipeline& get() {
      GROVE_ASSERT(is_valid());
      return *pipeline;
    }
    const Pipeline& get() const {
      GROVE_ASSERT(is_valid());
      return *pipeline;
    }
    friend inline void swap(PipelineHandle& a, PipelineHandle& b) noexcept {
      using std::swap;
      swap(a.system, b.system);
      swap(a.pipeline, b.pipeline);
    }
  private:
    PipelineSystem* system{};
    std::shared_ptr<Pipeline> pipeline;
  };

  struct PipelineData {
    PipelineHandle pipeline;
    VkPipelineLayout layout;
    BorrowedDescriptorSetLayouts descriptor_set_layouts;
  };

public:
  void begin_frame(const RenderFrameInfo& info, VkDevice device);
  void terminate(VkDevice device);
  PipelineHandle emplace(Pipeline&& pipeline);
  void destroy_pipeline(PipelineHandle&& handle);
  Result<VkPipelineLayout> require_pipeline_layout(VkDevice device,
                                                   const ArrayView<const VkDescriptorSetLayout>& set_layouts,
                                                   const ArrayView<const VkPushConstantRange>& push_constants,
                                                   VkPipelineLayoutCreateFlags flags = 0);
  Result<BorrowedDescriptorSetLayouts>
  make_borrowed_descriptor_set_layouts(VkDevice device,
                                       const ArrayView<const DescriptorSetLayoutBindings>& bindings,
                                       const VkDescriptorSetLayoutCreateFlags* set_flags, int num_flags);

  bool require_layouts(VkDevice device,
                       const ArrayView<const VkPushConstantRange>& push_constants,
                       const ArrayView<const DescriptorSetLayoutBindings>& bindings,
                       VkPipelineLayout* pipe_layout,
                       BorrowedDescriptorSetLayouts* borrowed_descriptor_set_layouts,
                       const RequireLayoutParams& params = {});
  bool require_layouts(VkDevice device, const glsl::VertFragProgramSource& source,
                       PipelineData* dst, const RequireLayoutParams& params = {});
  bool require_layouts(VkDevice device, const glsl::ComputeProgramSource& source,
                       PipelineData* dst, const RequireLayoutParams& params = {});

  template <typename GetSource, typename CreatePipeline>
  Optional<PipelineData>
  create_pipeline_data(VkDevice device, const GetSource& get_source,
                       const CreatePipeline& create_pipeline,
                       glsl::VertFragProgramSource* dst_source,
                       const RequireLayoutParams& params = {});

  Optional<PipelineData> create_compute_pipeline_data(VkDevice device,
                                                      const glsl::ComputeProgramSource& source,
                                                      const RequireLayoutParams& params = {});

  size_t num_pipelines() const;
  size_t num_pipeline_layouts() const;
  size_t num_descriptor_set_layouts() const;
  DescriptorSetLayoutCache& get_default_descriptor_set_layout_cache() {
    return descriptor_set_layout_caches_by_layout_create_flags[0];
  }

private:
  void destroy_pipeline(std::shared_ptr<Pipeline>&& pipe);
  DescriptorSetLayoutCache* require_cache(VkDescriptorSetLayoutCreateFlags flags);

private:
  static constexpr int max_num_descriptor_set_layout_caches = 2;

  struct PendingDestruction {
    uint64_t frame_id{};
    std::shared_ptr<Pipeline> pipeline;
  };

  std::unordered_set<std::shared_ptr<Pipeline>> pipelines;
  std::vector<PendingDestruction> pending_destruction;
  PipelineLayoutCache pipeline_layout_cache;
  DescriptorSetLayoutCache descriptor_set_layout_caches_by_layout_create_flags[2];
  int num_descriptor_set_layout_caches{1};
  RenderFrameInfo frame_info{};
};

template <typename GetSource, typename CreatePipeline>
Optional<PipelineSystem::PipelineData>
PipelineSystem::create_pipeline_data(VkDevice device, const GetSource& get_source,
                                     const CreatePipeline& create_pipeline,
                                     glsl::VertFragProgramSource* dst_source,
                                     const RequireLayoutParams& params) {
  PipelineSystem::PipelineData result{};
  auto source = get_source();
  if (!source) {
    return NullOpt{};
  }

  bool req_success = require_layouts(device, source.value(), &result, params);
  if (!req_success) {
    return NullOpt{};
  }

  auto cm_pipe = create_pipeline(device, source.value(), result.layout);
  if (!cm_pipe) {
    return NullOpt{};
  } else {
    if (dst_source) {
      *dst_source = std::move(source.value());
    }
    result.pipeline = emplace(std::move(cm_pipe.value));
    return Optional<PipelineSystem::PipelineData>(std::move(result));
  }
}

}