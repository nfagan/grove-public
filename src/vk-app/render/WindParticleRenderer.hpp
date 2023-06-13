#pragma once

#include "../vk/vk.hpp"
#include "../particle/WindParticles.hpp"

namespace grove {

class Camera;

class WindParticleRenderer {
public:
  struct InitInfo {
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::PipelineSystem& pipeline_system;
    vk::DescriptorSystem& desc_system;
    vk::BufferSystem& buffer_system;
    vk::CommandProcessor& uploader;
    const vk::PipelineRenderPassInfo& pass_info;
    uint32_t frame_queue_depth;
  };

  struct RenderInfo {
    const vk::Core& core;
    uint32_t frame_index;
    VkCommandBuffer cmd;
    VkViewport viewport;
    VkRect2D scissor_rect;
    const Camera& camera;
  };

  struct SetDataContext {
    vk::Allocator* allocator;
    const vk::Core& core;
    vk::BufferSystem& buffer_system;
    const vk::RenderFrameInfo& frame_info;
  };

public:
  bool is_valid() const;
  bool initialize(const InitInfo& info);
  void begin_frame_set_data(const SetDataContext& context,
                            const WindParticles::ParticleInstanceData* instances,
                            uint32_t num_instances);
  void render(const RenderInfo& info);

private:
  vk::PipelineSystem::PipelineData pipeline_data;
  DynamicArray<vk::BufferSystem::BufferHandle, 2> instance_buffers;
  vk::BufferSystem::BufferHandle geometry_buffer;
  vk::BufferSystem::BufferHandle index_buffer;
  vk::DrawIndexedDescriptor draw_desc{};
  bool initialized{};
};

}