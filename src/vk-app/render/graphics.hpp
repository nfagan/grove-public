#pragma once

#include "grove/common/identifier.hpp"
#include "grove/common/Optional.hpp"
#include <vulkan/vulkan.h>
#include <memory>

namespace grove {

template <typename T>
class ArrayView;

class VertexBufferDescriptor;

}

namespace grove::glsl {
struct VertFragProgramSource;
struct ComputeProgramSource;
}

namespace grove::vk {
struct GraphicsContext;
struct DescriptorSetScaffold;
class Allocator;
}

namespace grove::gfx {

struct Context;
struct PipelineHandle;

struct ContextStats {
  uint32_t num_pipelines;
  uint32_t num_buffers;
  size_t buffer_mb;
};

struct RenderPassHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(RenderPassHandle, id)
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  uint32_t id;
};

struct BufferHandle {
public:
  BufferHandle() = default;
  ~BufferHandle();
  BufferHandle(const BufferHandle& other) = delete;
  BufferHandle& operator=(const BufferHandle& other) = delete;
  BufferHandle(BufferHandle&& other) noexcept;
  BufferHandle& operator=(BufferHandle&& other) noexcept;

  bool is_valid() const;
  void write(const void* data, size_t size, size_t off = 0) const;
  void read(void* into, size_t size, size_t off = 0) const;
  VkBuffer get() const;

public:
  Context* context{};
  std::shared_ptr<void> impl;

public:
  friend inline void swap(BufferHandle& a, BufferHandle& b) noexcept {
    std::swap(a.context, b.context);
    std::swap(a.impl, b.impl);
  }
};

struct DynamicUniformBuffer {
  bool is_valid() const {
    return buffer.is_valid();
  }

  BufferHandle buffer;
  size_t element_stride{};
};

struct PipelineHandle {
public:
  PipelineHandle() = default;
  ~PipelineHandle();
  PipelineHandle(const PipelineHandle& other) = delete;
  PipelineHandle& operator=(const PipelineHandle& other) = delete;
  PipelineHandle(PipelineHandle&& other) noexcept;
  PipelineHandle& operator=(PipelineHandle&& other) noexcept;

  bool is_valid() const;
  VkPipeline get() const;
  VkPipelineLayout get_layout() const;
  Optional<ArrayView<const VkDescriptorSetLayoutBinding>>
  get_descriptor_set_layout_bindings(uint32_t set) const;

public:
  Context* context{};
  std::shared_ptr<void> impl;

public:
  friend inline void swap(PipelineHandle& a, PipelineHandle& b) noexcept {
    std::swap(a.context, b.context);
    std::swap(a.impl, b.impl);
  }
};

struct MemoryType {
  uint8_t bits;
};

struct MemoryTypeFlagBits {
  static constexpr uint8_t DeviceLocal = 1;
  static constexpr uint8_t HostVisible = 2;
  static constexpr uint8_t HostCoherent = 4;
};

struct BufferUsageFlagBits {
  static constexpr uint8_t Uniform = 1;
  static constexpr uint8_t Storage = 2;
  static constexpr uint8_t Vertex = 4;
  static constexpr uint8_t Index = 8;
  static constexpr uint8_t Indirect = 16;
};

struct BufferUsage {
  uint8_t bits;
};

enum class DepthCompareOp {
  Unspecified = 0,
  LessOrEqual
};

enum class CullMode {
  Unspecified = 0,
  Front,
  Back
};

enum class PrimitiveTopology {
  TriangleList = 0,
  TriangleStrip
};

struct GraphicsPipelineCreateInfo {
  const VertexBufferDescriptor* vertex_buffer_descriptors;
  uint32_t num_vertex_buffer_descriptors;
  uint32_t num_color_attachments;
  bool enable_blend[16];
  bool disable_cull_face;
  bool enable_alpha_to_coverage;
  bool disable_depth_write;
  bool disable_depth_test;
  DepthCompareOp depth_compare_op;
  CullMode cull_mode;
  PrimitiveTopology primitive_topology;
};

Context* init_context(vk::GraphicsContext* vk_context);
void terminate_context(Context* context);
void begin_frame(Context* context);

Optional<RenderPassHandle> get_forward_write_back_render_pass_handle(const Context* context);
Optional<RenderPassHandle> get_post_forward_render_pass_handle(const Context* context);
Optional<RenderPassHandle> get_shadow_render_pass_handle(const Context* context);
Optional<RenderPassHandle> get_post_process_pass_handle(const Context* context);

VkSampler get_image_sampler_linear_repeat(const Context* context);
VkSampler get_image_sampler_linear_edge_clamp(const Context* context);
VkSampler get_image_sampler_nearest_edge_clamp(const Context* context);

uint32_t get_frame_queue_depth(const Context* context);

Optional<PipelineHandle> create_pipeline(
  Context* context,
  const uint32_t* vert_spv, size_t vert_spv_size, //  size = num words
  const uint32_t* frag_spv, size_t frag_spv_size, //  size = num words
  const GraphicsPipelineCreateInfo& info, RenderPassHandle in_pass);

Optional<PipelineHandle> create_pipeline(
  Context* context, glsl::VertFragProgramSource&& source,
  const GraphicsPipelineCreateInfo& info, RenderPassHandle in_pass);

Optional<PipelineHandle> create_compute_pipeline(
  Context* context, glsl::ComputeProgramSource&& source);

Optional<BufferHandle> create_buffer(
  Context* context, BufferUsage usage, MemoryType mem_type, size_t size);
Optional<BufferHandle> create_host_visible_vertex_buffer(Context* context, size_t size);
Optional<BufferHandle> create_host_visible_index_buffer(Context* context, size_t size);
Optional<BufferHandle> create_storage_buffer(Context* context, size_t size);
Optional<BufferHandle> create_device_local_storage_buffer(Context* context, size_t size);
Optional<BufferHandle> create_uniform_buffer(Context* context, size_t size);
Optional<BufferHandle> create_device_local_vertex_buffer_sync(
  Context* context, size_t size, const void* data);
Optional<BufferHandle> create_device_local_index_buffer_sync(
  Context* context, size_t size, const void* data);

Optional<DynamicUniformBuffer> create_dynamic_uniform_buffer(
  Context* context, size_t element_size, size_t num_elements);

template <typename Element>
Optional<DynamicUniformBuffer> create_dynamic_uniform_buffer(Context* context, size_t num_elements) {
  return create_dynamic_uniform_buffer(context, sizeof(Element), num_elements);
}

Optional<VkDescriptorSet> require_updated_descriptor_set(
  Context* context, const vk::DescriptorSetScaffold& scaffold, const gfx::PipelineHandle& pipeline,
  bool disable_cache = false);

vk::Allocator* get_vk_allocator(Context* context);

ContextStats get_stats(const Context* context);

}