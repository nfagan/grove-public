#pragma once

#include "../vk/vk.hpp"
#include "shadow.hpp"
#include "grove/common/identifier.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/Mat4.hpp"
#include <unordered_map>
#include <bitset>

namespace grove {

namespace vk {
struct GraphicsContext;
}

class Camera;

class ArchRenderer {
public:
  //  Vertex data, sizeof(Vertex), Index data, sizeof(uint16_t)
  using GetGeometryData = std::function<void(const void**, size_t*, const void**, size_t*)>;

  //  Number of vertices, number of indices
  using ReserveGeometryData = std::function<void(size_t*, size_t*)>;

  enum class DrawType {
    Static,
    Dynamic
  };

  struct DrawableParams {
    Vec3f translation{};
    float scale{1.0f};
    Vec3f color{};
  };

  struct GeometryHandle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(GeometryHandle, id)
    uint32_t id;
  };

  struct DrawableHandle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(DrawableHandle, id)
    uint32_t id;
  };

  struct Geometry {
    vk::BufferSystem::BufferHandle geometry_buffer;
    vk::BufferSystem::BufferHandle index_buffer;
    bool is_valid{};
    uint32_t num_indices_allocated{};
    uint32_t num_indices_active{};
    uint32_t num_vertices{};
    DrawType draw_type{DrawType::Static};
    GetGeometryData get_data{nullptr};
    ReserveGeometryData reserve_data{nullptr};
    bool modified{};
    std::bitset<32> buffers_need_update{};
  };

  struct Drawable {
    GeometryHandle geometry;
    DrawableParams params;
    bool inactive{};
  };

  struct AddResourceContext {
    vk::Allocator* allocator;
    const vk::Core& core;
    uint32_t frame_queue_depth;
    vk::BufferSystem& buffer_system;
    vk::StagingBufferSystem& staging_buffer_system;
    vk::CommandProcessor& command_processor;
  };

  struct InitInfo {
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::PipelineSystem& pipeline_system;
    vk::BufferSystem& buffer_system;
    vk::DescriptorSystem& desc_system;
    const vk::PipelineRenderPassInfo& forward_pass_info;
    const vk::PipelineRenderPassInfo& shadow_pass_info;
    uint32_t frame_queue_depth;
  };

  struct BeginFrameInfo {
    vk::Allocator* allocator;
    const vk::Core& core;
    uint32_t frame_queue_depth;
    vk::BufferSystem& buffer_system;
    vk::StagingBufferSystem& staging_buffer_system;
    vk::CommandProcessor& command_processor;
    const csm::CSMDescriptor& csm_descriptor;
    const Camera& camera;
    uint32_t frame_index;
  };

  struct RenderInfo {
    const vk::Core& core;
    vk::DescriptorSystem& desc_system;
    vk::SamplerSystem& sampler_system;
    uint32_t frame_index;
    VkCommandBuffer cmd;
    VkViewport viewport;
    VkRect2D scissor_rect;
    const vk::SampleImageView& shadow_image;
  };

  struct ShadowRenderInfo {
    const vk::Device& device;
    vk::DescriptorSystem& desc_system;
    VkCommandBuffer cmd_buffer;
    uint32_t frame_index;
    VkViewport viewport;
    VkRect2D scissor_rect;
    uint32_t cascade_index;
    const Mat4f& view_proj;
  };

  struct RenderParams {
    bool randomized_color{};
    Vec3f sun_position{};
    Vec3f sun_color{};
  };

public:
  bool is_valid() const;
  bool initialize(const InitInfo& info);
  void begin_frame(const BeginFrameInfo& info);
  void render(const RenderInfo& info);
  void render_shadow(const ShadowRenderInfo& info);
  void remake_programs(const InitInfo& info);

  DrawableHandle create_drawable(GeometryHandle geom, const DrawableParams& params);
  void set_active(DrawableHandle handle, bool active);
  void toggle_active(DrawableHandle handle);
  bool is_active(DrawableHandle handle) const;
  void destroy_drawable(DrawableHandle handle);
  bool is_hidden() const {
    return hidden;
  }
  void set_hidden(bool h) {
    hidden = h;
  }

  GeometryHandle create_static_geometry();
  GeometryHandle create_dynamic_geometry(GetGeometryData&& get_data,
                                         ReserveGeometryData&& reserve = nullptr);
  [[nodiscard]] bool update_geometry(const AddResourceContext& context,
                                     GeometryHandle handle,
                                     const void* data, size_t size,
                                     const VertexBufferDescriptor& desc,
                                     int pos_attr, const Optional<int>& norm_attr,
                                     const uint16_t* indices, uint32_t num_indices);
  void set_modified(GeometryHandle geom);

  DrawableParams* get_params(DrawableHandle handle);
  const RenderParams* get_render_params() const {
    return &render_params;
  }
  RenderParams* get_render_params() {
    return &render_params;
  }

  static AddResourceContext make_add_resource_context(vk::GraphicsContext& graphics_context);

public:
  struct PipelineData {
    vk::PipelineSystem::PipelineHandle pipeline;
    VkPipelineLayout pipeline_layout;
    vk::BorrowedDescriptorSetLayouts desc_set_layouts;
  };

  PipelineData forward_pipeline;
  PipelineData shadow_pipeline;

  vk::BufferSystem::BufferHandle forward_uniform_buffer;
  size_t forward_uniform_buffer_stride{};
  vk::BufferSystem::BufferHandle forward_shadow_uniform_buffer;
  size_t forward_shadow_uniform_buffer_stride{};

  Unique<vk::DescriptorSystem::PoolAllocatorHandle> desc_pool_alloc;
  Unique<vk::DescriptorSystem::SetAllocatorHandle> desc_set0_alloc;

  RenderParams render_params;

  std::unordered_map<uint32_t, Geometry> geometries;
  std::unordered_map<uint32_t, Drawable> drawables;

  uint32_t next_geometry_id{1};
  uint32_t next_drawable_id{1};

  bool initialized{};
  bool initialized_programs{};
  bool hidden{};
};

}