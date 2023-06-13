#pragma once

#include "../vk/vk.hpp"
#include "DynamicSampledImageManager.hpp"
#include "grove/math/vector.hpp"

namespace grove {

class Camera;

namespace vk {
struct GraphicsContext;
}

class CloudRenderer {
public:
  struct VolumeDrawableHandle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(VolumeDrawableHandle, id)
    uint32_t id;
  };
  struct BillboardDrawableHandle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(BillboardDrawableHandle, id)
    uint32_t id;
  };

  struct VolumeDrawableParams {
    Vec3f translation{};
    Vec3f scale{1.0f};
    Vec3f uvw_offset{};
    Vec3f uvw_scale{1.0f};
    bool depth_test_enable{};
    float density_scale{1.0f};
  };

  struct BillboardDrawableParams {
    Vec3f translation{};
    Vec3f scale{1.0f};
    bool depth_test_enabled{true};
    float opacity_scale{1.0f};
    Vec3f uvw_offset{};
  };

  struct VolumeDrawable {
    vk::DynamicSampledImageManager::Handle image_handle;
    vk::BufferSystem::BufferHandle uniform_buffer;
    size_t uniform_buffer_stride{};
    VolumeDrawableParams params;
    bool inactive{};
  };

  struct BillboardDrawable {
    vk::DynamicSampledImageManager::Handle image_handle;
    BillboardDrawableParams params;
    bool inactive{};
  };

  struct AddResourceContext {
    const vk::Core* core;
    vk::Allocator* allocator;
    vk::CommandProcessor* uploader;
    vk::BufferSystem* buffer_system;
    vk::StagingBufferSystem* staging_buffer_system;
    uint32_t frame_queue_depth;
  };

  struct InitInfo {
    vk::Allocator* allocator;
    const vk::Core& core;
    vk::BufferSystem& buffer_system;
    vk::StagingBufferSystem& staging_buffer_system;
    vk::PipelineSystem& pipeline_system;
    vk::DescriptorSystem& desc_system;
    vk::CommandProcessor& uploader;
    uint32_t frame_queue_depth;
    const vk::PipelineRenderPassInfo& post_process_pass_info;
    const vk::PipelineRenderPassInfo& forward_pass_info;
  };

  struct BeginFrameInfo {
    const Camera& camera;
    uint32_t frame_index;
  };

  struct RenderInfo {
    VkDevice device;
    vk::Allocator* allocator;
    vk::SamplerSystem& sampler_system;
    vk::DescriptorSystem& descriptor_system;
    const vk::DynamicSampledImageManager& dynamic_sampled_image_manager;
    Optional<vk::SampleImageView> scene_color_image;
    Optional<vk::SampleImageView> scene_depth_image;
    bool post_processing_enabled;
    uint32_t frame_index;
    VkCommandBuffer cmd;
    VkViewport viewport;
    VkRect2D scissor_rect;
    const Camera& camera;
  };

  struct RenderParams {
    Vec3f cloud_color{1.0f};
  };

  struct PipelineData {
    vk::BorrowedDescriptorSetLayouts desc_set_layouts;
    VkPipelineLayout layout;
    vk::PipelineSystem::PipelineHandle pipeline;
  };

public:
  bool is_valid() const;
  bool initialize(const InitInfo& info);
  bool remake_programs(const InitInfo& info);
  void begin_frame(const BeginFrameInfo& info);
  void render_forward(const RenderInfo& info);
  void render_post_process(const RenderInfo& info);
  RenderParams& get_render_params() {
    return render_params;
  }

  Optional<BillboardDrawableHandle> create_billboard_drawable(const AddResourceContext& context,
                                                              vk::DynamicSampledImageManager::Handle image,
                                                              const BillboardDrawableParams& params);

  Optional<VolumeDrawableHandle> create_volume_drawable(const AddResourceContext& context,
                                                        vk::DynamicSampledImageManager::Handle handle,
                                                        const VolumeDrawableParams& params);
  void set_drawable_params(VolumeDrawableHandle handle, const VolumeDrawableParams& params);
  void set_drawable_params(BillboardDrawableHandle handle, const BillboardDrawableParams& params);
  void set_active(VolumeDrawableHandle handle, bool active);
  void set_active(BillboardDrawableHandle handle, bool active);
  void set_enabled(bool value) {
    enabled = value;
  }
  bool is_enabled() const {
    return enabled;
  }
  bool is_volume_enabled() const {
    return !volume_disabled;
  }
  void set_volume_enabled(bool v) {
    volume_disabled = !v;
  }

  static AddResourceContext make_add_resource_context(vk::GraphicsContext& graphics_context);

private:
  bool initialize_forward_program(const InitInfo& info, glsl::VertFragProgramSource* prog_source);
  bool initialize_post_process_program(const InitInfo& info,
                                       glsl::VertFragProgramSource* prog_source);
  bool initialize_billboard_program(const InitInfo& info, glsl::VertFragProgramSource* source);
  void update_buffers(const Camera& camera, uint32_t frame_index);
  int num_active_volume_drawables() const;
  int num_active_billboard_drawables() const;
  void render_volume_post_process(const RenderInfo& info);
  void render_billboard_post_process(const RenderInfo& info);

private:
  PipelineData forward_pipeline_data;
  PipelineData post_process_pipeline_data;
  PipelineData billboard_pipeline_data;

  vk::BufferSystem::BufferHandle global_uniform_buffer;
  size_t global_uniform_buffer_stride{};

  vk::BufferSystem::BufferHandle vertex_geometry;
  vk::BufferSystem::BufferHandle vertex_indices;
  vk::DrawIndexedDescriptor aabb_draw_desc;

  Unique<vk::DescriptorSystem::SetAllocatorHandle> forward_desc_set0_alloc;
  Unique<vk::DescriptorSystem::SetAllocatorHandle> forward_desc_set1_alloc;
  Unique<vk::DescriptorSystem::SetAllocatorHandle> post_process_desc_set0_alloc;
  Unique<vk::DescriptorSystem::SetAllocatorHandle> post_process_desc_set1_alloc;
  Unique<vk::DescriptorSystem::SetAllocatorHandle> billboard_desc_set0_alloc;
  Unique<vk::DescriptorSystem::PoolAllocatorHandle> desc_pool_alloc;

  RenderParams render_params{};
  std::unordered_map<uint32_t, VolumeDrawable> volume_drawables;
  std::unordered_map<uint32_t, BillboardDrawable> billboard_drawables;
  uint32_t next_drawable_id{1};

  bool initialized{};
  bool initialized_post_process_program{};
  bool initialized_billboard_program{};
  bool initialized_forward_program{};
  bool enabled{true};
  bool volume_disabled{};
};

}