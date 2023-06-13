#pragma once

#include "../vk/vk.hpp"
#include "grove/math/vector.hpp"
#include <bitset>

namespace grove {
class Camera;
}

namespace grove::vk {

struct GraphicsContext;

class PointBufferRenderer {
public:
  struct DrawableHandle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(DrawableHandle, id)
    uint32_t id;
  };

  enum class DrawableType {
    Points,
    Lines
  };

  struct DrawableParams {
    float point_size{1.0f};
    Vec3f color{1.0f};
  };

  struct Drawable {
    vk::BufferSystem::BufferHandle vertex_buffer;
    std::unique_ptr<unsigned char[]> cpu_vertex_data;
    uint32_t num_vertices_reserved;
    uint32_t num_vertices_active;
    uint32_t vertex_size_bytes;
    std::bitset<32> vertex_buffer_needs_update;
    DrawableParams params;
    DrawableType type{};
  };

  struct AddResourceContext {
    const Core& core;
    Allocator* allocator;
    BufferSystem& buffer_system;
    uint32_t frame_queue_depth;
  };

  struct InitInfo {
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::PipelineSystem& pipeline_system;
    vk::BufferSystem& buffer_system;
    const vk::PipelineRenderPassInfo& forward_pass_info;
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

public:
  bool is_valid() const;
  bool initialize(const InitInfo& info);
  void render(const RenderInfo& info);
  void begin_frame(uint32_t frame_index);

  void add_active_drawable(DrawableHandle handle);
  void require_active_drawable(DrawableHandle handle);
  void remove_active_drawable(DrawableHandle handle);
  void toggle_active_drawable(DrawableHandle handle);
  DrawableHandle create_drawable(DrawableType type, const DrawableParams& params);
  void destroy_drawable(DrawableHandle handle);
  void clear_active_instances(DrawableHandle handle);
  void reserve_instances(const AddResourceContext& context,
                         DrawableHandle handle,
                         uint32_t num_verts);
  void update_instances(const AddResourceContext& context,
                        DrawableHandle handle,
                        const void* data,
                        size_t size,
                        const VertexBufferDescriptor& desc,
                        int pos_attr,
                        int color_attr);
  void update_instances(const AddResourceContext& context,
                        DrawableHandle handle,
                        const Vec3f* positions,
                        int num_points);
  void set_instances(const AddResourceContext& context,
                     DrawableHandle handle,
                     const void* data,
                     size_t size,
                     const VertexBufferDescriptor& desc,
                     int pos_attr,
                     int color_attr,
                     int ith_element_offset);
  void set_instances(const AddResourceContext& context,
                     DrawableHandle handle,
                     const Vec3f* positions,
                     int num_points,
                     int offset);
  void set_instance_color_range(const AddResourceContext& context,
                                DrawableHandle handle,
                                const Vec3f* colors,
                                int num_colors,
                                int offset);
  void set_point_color(DrawableHandle handle, const Vec3f& color);

  static AddResourceContext make_add_resource_context(vk::GraphicsContext& graphics_context);

private:
  void update_buffers(uint32_t frame_index);
  bool initialize_point_program(const InitInfo& info);
  bool initialize_line_program(const InitInfo& info);
  void render_points(const RenderInfo& info);
  void render_lines(const RenderInfo& info);

private:
  struct PipelineData {
    vk::PipelineSystem::PipelineHandle pipeline;
    VkPipelineLayout pipeline_layout;
    vk::BorrowedDescriptorSetLayouts desc_set_layouts;
  };

  PipelineData point_pipeline;
  PipelineData line_pipeline;

  std::unordered_map<uint32_t, Drawable> drawables;
  std::vector<DrawableHandle> active_drawables;
  uint32_t next_drawable_id{1};

  bool initialized{};
};

}