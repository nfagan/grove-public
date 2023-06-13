#pragma once

#include "../vk/vk.hpp"
#include "grove/math/vector.hpp"

namespace grove {

class Camera;

class PollenParticleRenderer {
public:
  struct InitInfo {
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::PipelineSystem& pipeline_system;
    vk::DescriptorSystem& desc_system;
    vk::BufferSystem& buffer_system;
    vk::StagingBufferSystem& staging_buffer_system;
    vk::CommandProcessor& command_processor;
    const vk::PipelineRenderPassInfo& forward_pass_info;
    uint32_t frame_queue_depth;
  };

  struct RenderInfo {
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::BufferSystem& buffer_system;
    vk::DescriptorSystem& desc_system;
    uint32_t frame_index;
    uint32_t frame_queue_depth;
    VkCommandBuffer cmd;
    VkViewport viewport;
    VkRect2D scissor_rect;
    const Camera& camera;
  };

  struct BeginFrameInfo {
    vk::Allocator* allocator;
    vk::BufferSystem& buffer_system;
    const vk::RenderFrameInfo& frame_info;
  };

  struct DrawableParams {
    Vec3f translation;
    float scale;
  };

public:
  bool is_valid() const;
  bool initialize(const InitInfo& info);
  void begin_update();
  void begin_frame(const BeginFrameInfo& info);
  void render(const RenderInfo& info);
  void push_drawable(const DrawableParams& params);

private:
  bool initialized{};

  vk::BufferSystem::BufferHandle geometry_buffer;
  vk::BufferSystem::BufferHandle instance_buffer;
  vk::BufferSystem::BufferHandle index_buffer;
  std::unique_ptr<unsigned char[]> cpu_instance_data;
  vk::DrawIndexedDescriptor draw_desc;

  vk::PipelineSystem::PipelineHandle pipeline;
  VkPipelineLayout pipeline_layout;
  vk::BorrowedDescriptorSetLayouts desc_set_layouts;

  size_t num_active_drawables{};
  size_t num_reserved_drawables{};
  bool need_remake_instance_buffer{};
};

}