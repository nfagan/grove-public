#pragma once

#include "../vk/vk.hpp"
#include "shadow.hpp"
#include "DynamicSampledImageManager.hpp"
#include "../procedural_flower/geometry.hpp"
#include "../procedural_tree/components.hpp"
#include "grove/math/vector.hpp"
#include <bitset>

namespace grove {

class Camera;

namespace vk {
struct GraphicsContext;
}

class ProceduralFlowerStemRenderer {
public:
  struct DrawableHandle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(DrawableHandle, id)
    uint32_t id;
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
    const vk::PipelineRenderPassInfo& forward_pass_info;
  };

  struct RenderInfo {
    VkDevice device;
    vk::Allocator* allocator;
    vk::BufferSystem& buffer_system;
    vk::SamplerSystem& sampler_system;
    vk::DescriptorSystem& desc_system;
    const vk::DynamicSampledImageManager& dynamic_sampled_image_manager;
    uint32_t frame_index;
    VkCommandBuffer cmd;
    VkViewport viewport;
    VkRect2D scissor_rect;
    const Camera& camera;
    const vk::SampleImageView& shadow_image;
  };

  struct BeginFrameInfo {
    const Camera& camera;
    uint32_t frame_index;
    const csm::CSMDescriptor& csm_desc;
  };

  struct RenderParams {
    Vec4f wind_world_bound_xz;
    float elapsed_time;
    Vec3f sun_color;
  };

  struct DrawableParams {
    Vec3f color{};
    bool wind_influence_enabled{true};
    bool allow_lateral_branch{true};
  };

  struct Drawable {
    vk::BufferSystem::BufferHandle static_instance_buffer;
    vk::BufferSystem::BufferHandle dynamic_instance_buffer;
    uint32_t num_instances;
    std::unique_ptr<unsigned char[]> cpu_dynamic_instance_data;
    std::bitset<32> dynamic_instance_buffer_needs_update;
    uint32_t frame_queue_depth;
    DrawableParams params;
    bool inactive;
  };

  struct AddResourceContext {
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::BufferSystem& buffer_system;
    vk::CommandProcessor& command_processor;
    uint32_t frame_queue_depth;
  };

public:
  bool is_valid() const;
  bool initialize(const InitInfo& info);
  void render(const RenderInfo& info);
  void begin_frame(const BeginFrameInfo& info);
  RenderParams& get_render_params() {
    return render_params;
  }
  void set_wind_displacement_image(vk::DynamicSampledImageManager::Handle handle) {
    wind_displacement_image = handle;
  }
  void set_dynamic_data(DrawableHandle handle, const tree::Internodes& internodes);
  Optional<DrawableHandle> create_drawable(const AddResourceContext& context,
                                           const tree::Internodes& internodes,
                                           const DrawableParams& params);
  bool update_drawable(const AddResourceContext& context,
                       DrawableHandle handle,
                       const tree::Internodes& internodes,
                       const Vec3f& color);
  void set_active(DrawableHandle handle, bool active);

  static AddResourceContext make_add_resource_context(vk::GraphicsContext& context);

private:
  bool make_pipeline(const vk::Core& core,
                     vk::PipelineSystem& pipe_sys,
                     const vk::PipelineRenderPassInfo& forward_pass_info,
                     glsl::VertFragProgramSource* prog_source);
  void update_buffers(const BeginFrameInfo& info);

private:
  struct UniformBuffer {
    vk::BufferSystem::BufferHandle handle;
    size_t stride;
  };

  vk::PipelineSystem::PipelineHandle pipeline;
  VkPipelineLayout pipeline_layout;
  vk::BorrowedDescriptorSetLayouts desc_set_layouts;

  Unique<vk::DescriptorSystem::PoolAllocatorHandle> desc_pool_alloc;
  Unique<vk::DescriptorSystem::SetAllocatorHandle> desc_set0_alloc;

  GridGeometryParams geom_params;
  UniformBuffer global_uniform_buffer;
  UniformBuffer sample_shadow_uniform_buffer;
  vk::BufferSystem::BufferHandle geom_buffer;
  vk::BufferSystem::BufferHandle index_buffer;
  uint32_t num_geom_indices{};

  Optional<vk::DynamicSampledImageManager::Handle> wind_displacement_image;
  RenderParams render_params{};

  std::unordered_map<uint32_t, Drawable> drawables;

  bool initialized_program{};
  bool initialized{};
  uint32_t next_drawable_id{1};
};

}