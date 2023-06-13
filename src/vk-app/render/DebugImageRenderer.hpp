#pragma once

#include "../vk/vk.hpp"
#include "DynamicSampledImageManager.hpp"
#include "SampledImageManager.hpp"
#include "grove/math/vector.hpp"

namespace grove {

class DebugImageRenderer {
public:
  struct DrawableParams {
    Vec2f translation{};
    Vec2f scale{1.0f};
    float min_alpha{};
  };

  struct Drawable {
    vk::SampledImageManager::Handle static_image;
    vk::DynamicSampledImageManager::Handle dynamic_image;
    DrawableParams params;
  };

  struct PipelineData {
    vk::PipelineSystem::PipelineHandle pipeline;
    VkPipelineLayout layout;
    int num_image_components{};
  };

  struct RenderInfo {
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::CommandProcessor& command_processor;
    vk::BufferSystem& buffer_system;
    vk::StagingBufferSystem& staging_buffer_system;
    vk::PipelineSystem& pipeline_system;
    vk::DescriptorSystem& desc_system;
    const vk::PipelineRenderPassInfo& pass_info;
    const vk::SampledImageManager& image_manager;
    const vk::DynamicSampledImageManager& dynamic_image_manager;
    vk::SamplerSystem& sampler_system;
    VkCommandBuffer cmd;
    VkViewport viewport;
    VkRect2D scissor_rect;
  };

public:
  void push_drawable(vk::SampledImageManager::Handle image, const DrawableParams& params);
  void push_drawable(vk::DynamicSampledImageManager::Handle image, const DrawableParams& params);
  void render(const RenderInfo& info);

private:
  bool require_geometry_buffers(const RenderInfo& info);
  Optional<int> require_pipeline(const RenderInfo& info, int num_image_components);

private:
  vk::BufferSystem::BufferHandle vertex_geometry_buffer;
  vk::BufferSystem::BufferHandle vertex_index_buffer;
  vk::DrawIndexedDescriptor draw_desc;

  std::vector<PipelineData> pipelines;
  vk::BorrowedDescriptorSetLayouts desc_set_layouts;
  bool acquired_desc_set_layouts{};

  Unique<vk::DescriptorSystem::SetAllocatorHandle> desc_set_allocator;
  Unique<vk::DescriptorSystem::PoolAllocatorHandle> desc_pool_allocator;

  std::vector<Drawable> pending_drawables;
  std::vector<int> drawable_pipeline_indices;
  std::vector<int> drawable_indices;
};

}