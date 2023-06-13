#pragma once

#include "../vk/vk.hpp"
#include "SampledImageManager.hpp"
#include "DynamicSampledImageManager.hpp"

namespace grove {

class Camera;

class SkyRenderer {
public:
  struct InitInfo {
    vk::Allocator* allocator;
    const vk::Core& core;
    vk::BufferSystem& buffer_system;
    vk::PipelineSystem& pipeline_system;
    vk::DescriptorSystem& desc_system;
    vk::CommandProcessor& uploader;
    uint32_t frame_queue_depth;
    const vk::PipelineRenderPassInfo& pass_info;
  };
  struct RenderInfo {
    const vk::Core& core;
    const vk::SampledImageManager& sampled_image_manager;
    const vk::DynamicSampledImageManager& dynamic_sampled_image_manager;
    vk::DescriptorSystem& desc_system;
    vk::SamplerSystem& sampler_system;
    uint32_t frame_index;
    const Camera& camera;
    VkCommandBuffer cmd;
    VkViewport viewport;
    VkRect2D scissor_rect;
  };

public:
  bool is_valid() const;
  bool initialize(const InitInfo& info);
  void render(const RenderInfo& info);

  void set_bayer_image(vk::SampledImageManager::Handle handle) {
    bayer_image = handle;
  }
  void set_color_image(vk::DynamicSampledImageManager::Handle handle) {
    color_image = handle;
  }

private:
  Unique<vk::DescriptorSystem::PoolAllocatorHandle> desc_pool_allocator;
  Unique<vk::DescriptorSystem::SetAllocatorHandle> desc_set0_allocator;

  vk::PipelineSystem::PipelineHandle pipeline_handle;
  VkPipelineLayout pipeline_layout;
  vk::BorrowedDescriptorSetLayouts desc_set_layouts;

  vk::BufferSystem::BufferHandle vertex_buffer;
  vk::BufferSystem::BufferHandle index_buffer;
  vk::DrawIndexedDescriptor draw_desc;

  Optional<vk::SampledImageManager::Handle> bayer_image;
  Optional<vk::DynamicSampledImageManager::Handle> color_image;
};

}