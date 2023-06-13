#pragma once

#include "../vk/vk.hpp"
#include "../environment/RainParticles.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/Mat4.hpp"
#include <bitset>

namespace grove {

class Camera;

namespace vk {
struct GraphicsContext;
}

class RainParticleRenderer {
public:
  using Particles = std::vector<RainParticles::Particle>;

  struct DrawableHandle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(DrawableHandle, id)
    uint32_t id;
  };

  struct InstanceData {
    Vec4f translation_alpha;
    Vec4f rand01_rotation;
  };

  struct Drawable {
    vk::BufferSystem::BufferHandle instance_buffer;
    std::unique_ptr<unsigned char[]> cpu_instance_data;
    uint32_t num_instances{};
    std::bitset<32> instance_buffer_needs_update{};
    uint32_t frame_queue_depth{};
  };

  struct AddResourceContext {
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::BufferSystem& buffer_system;
    uint32_t frame_queue_depth;
  };

  struct InitInfo {
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::BufferSystem& buffer_system;
    vk::StagingBufferSystem& staging_buffer_system;
    vk::PipelineSystem& pipeline_system;
    vk::DescriptorSystem& desc_system;
    vk::CommandProcessor& command_processor;
    const vk::PipelineRenderPassInfo& pass_info;
    uint32_t frame_queue_depth;
  };

  struct BeginFrameInfo {
    uint32_t frame_index;
    const Camera& camera;
  };

  struct RenderInfo {
    VkDevice device;
    vk::DescriptorSystem& desc_system;
    VkCommandBuffer cmd;
    VkViewport viewport;
    VkRect2D scissor_rect;
    uint32_t frame_index;
  };

  struct InstanceVertexBufferIndices {
    int translation;
    int alpha;
    int rand01;
    int rotation;
  };

  struct RenderParams {
    Vec2f global_particle_scale{1.0f};
    float global_alpha_scale{1.0f};
  };

public:
  bool is_valid() const;
  bool initialize(const InitInfo& info);
  void begin_frame(const BeginFrameInfo& info);
  void render(const RenderInfo& info);
  RenderParams& get_render_params() {
    return render_params;
  }

  Optional<DrawableHandle> create_drawable(const AddResourceContext& context,
                                           uint32_t num_instances);
  void set_data(DrawableHandle handle,
                const void* src,
                const VertexBufferDescriptor& src_desc,
                const InstanceVertexBufferIndices& src_inds,
                uint32_t num_instances);
  void set_data(DrawableHandle handle, const Particles& particles, const Mat4f& view);
  static AddResourceContext make_add_resource_context(vk::GraphicsContext& graphics_context);

private:
  void fill_scratch_instance_data(const Particles& particles, const Mat4f& view);

private:
  VkPipelineLayout pipeline_layout;
  vk::BorrowedDescriptorSetLayouts desc_set_layouts;
  vk::PipelineSystem::PipelineHandle pipeline;

  Unique<vk::DescriptorSystem::PoolAllocatorHandle> desc_pool_alloc;
  Unique<vk::DescriptorSystem::SetAllocatorHandle> desc_set0_alloc;

  vk::BufferSystem::BufferHandle vertex_geometry_buffer;
  vk::BufferSystem::BufferHandle vertex_index_buffer;
  uint32_t num_vertex_indices;
  vk::BufferSystem::BufferHandle global_uniform_buffer;
  size_t global_uniform_buffer_stride;

  std::unordered_map<uint32_t, Drawable> drawables;
  uint32_t next_drawable_id{1};

  std::vector<InstanceData> scratch_instances;
  std::vector<float> scratch_depths;
  std::vector<int> scratch_depth_inds;

  RenderParams render_params{};
  bool initialized{};
};

}