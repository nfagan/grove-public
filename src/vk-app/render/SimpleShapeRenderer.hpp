#pragma once

#include "../vk/vk.hpp"
#include "grove/math/vector.hpp"
#include <bitset>

namespace grove {

class Camera;

struct SimpleShapeRendererNewGraphicsContextImpl;

namespace gfx {
struct Context;
}

namespace vk {
struct GraphicsContext;
}

class SimpleShapeRenderer {
public:
  struct GeometryHandle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(GeometryHandle, id)
    GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, GeometryHandle, id)
    uint32_t id{};
  };
  struct DrawableHandle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(DrawableHandle, id)
    GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, DrawableHandle, id)
    GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
    uint32_t id{};
  };

  struct TwoSidedTriangleVertex {
    Vec4<uint32_t> data;
  };

  enum class PipelineType : int {
    NonOriented = 0,
    Oriented
  };

  struct AddResourceContext {
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::CommandProcessor& command_processor;
    vk::BufferSystem& buffer_system;
    vk::StagingBufferSystem& staging_buffer_system;
    uint32_t frame_queue_depth;
  };

  struct InstanceData {
    Vec4f color;
    Vec4f scale_active;
    Vec4f translation;
  };

  struct Geometry {
    vk::BufferSystem::BufferHandle geometry_buffer;
    vk::BufferSystem::BufferHandle index_buffer;
    uint32_t num_vertices;
    uint32_t num_indices;
  };

  struct Drawable {
    GeometryHandle geometry_handle;
    uint32_t num_instances;
    uint32_t num_active_instances;
    std::unique_ptr<InstanceData[]> cpu_instance_data;
    vk::BufferSystem::BufferHandle instance_buffer;
    std::bitset<32> instance_buffer_needs_update{};
    PipelineType pipeline_type;
  };

  struct InitInfo {
    gfx::Context* graphics_context;
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::PipelineSystem& pipeline_system;
    vk::DescriptorSystem& desc_system;
    vk::BufferSystem& buffer_system;
    vk::CommandProcessor& uploader;
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
  SimpleShapeRenderer();
  ~SimpleShapeRenderer();

  SimpleShapeRenderer(const SimpleShapeRenderer& other) = delete;
  SimpleShapeRenderer(SimpleShapeRenderer&& other) noexcept = delete;
  SimpleShapeRenderer& operator=(SimpleShapeRenderer&& other) noexcept = delete;
  SimpleShapeRenderer& operator=(const SimpleShapeRenderer& other) = delete;

  bool is_valid() const;
  bool initialize(const InitInfo& info);
  void terminate();

  void push_two_sided_triangles(const Vec3f* p, uint32_t num_ps, const Vec3f& color);

  Optional<GeometryHandle> require_cube(const AddResourceContext& context);
  Optional<GeometryHandle> require_sphere(const AddResourceContext& context);
  Optional<GeometryHandle> require_plane(const AddResourceContext& context);
  Optional<GeometryHandle> add_geometry(const AddResourceContext& context,
                                        const void* data, size_t size,
                                        const VertexBufferDescriptor& desc, int pos_attr_index,
                                        const uint16_t* indices, uint32_t num_indices);
  Optional<DrawableHandle> add_instances(const AddResourceContext& context,
                                         GeometryHandle geometry, int num_instances,
                                         PipelineType type = PipelineType::NonOriented);
  void destroy_instances(DrawableHandle handle);

  void begin_frame(gfx::Context* graphics_context, uint32_t frame_index);
  void render(const RenderInfo& info);

  void add_active_drawable(DrawableHandle handle);
  void remove_active_drawable(DrawableHandle handle);

  void set_instance_params(DrawableHandle handle, uint32_t instance,
                           const Vec3f& color, const Vec3f& scale, const Vec3f& translation);
  void set_oriented_instance_params(DrawableHandle handle, uint32_t instance,
                                    const Vec3f& color, const Vec3f& scale, const Vec3f& translation,
                                    const Vec3f& right, const Vec3f& up);
  void set_active_instance(DrawableHandle handle, uint32_t instance, bool active);
  void clear_active_instances(DrawableHandle handle);
  void attenuate_active_instance_scales(DrawableHandle handle, float s);

  void set_disabled(bool disable) {
    disabled = disable;
  }
  bool is_disabled() const {
    return disabled;
  }
  static AddResourceContext make_add_resource_context(vk::GraphicsContext& graphics_context);

private:
  void render_pipeline_type(const RenderInfo& info, PipelineType type);
  void prepare_two_sided(gfx::Context* graphics_context, uint32_t frame_index);
  void render_two_sided(const RenderInfo& info);

private:
  bool initialized{};
  bool disabled{};
  uint32_t frame_queue_depth{};

  vk::PipelineSystem::PipelineData non_oriented_pipeline_data;
  vk::PipelineSystem::PipelineData oriented_pipeline_data;

  std::unordered_map<GeometryHandle, Geometry, GeometryHandle::Hash> geometries;
  std::unordered_map<DrawableHandle, Drawable, DrawableHandle::Hash> drawables;
  std::vector<DrawableHandle> active_drawables;

  Optional<GeometryHandle> cube_geometry;
  Optional<GeometryHandle> sphere_geometry;
  Optional<GeometryHandle> plane_geometry;

  SimpleShapeRendererNewGraphicsContextImpl* graphics_context_impl{};
  std::vector<TwoSidedTriangleVertex> two_sided_vertices;
  uint32_t num_two_sided_vertices_reserved{};
  uint32_t num_two_sided_vertices_active{};

  uint32_t next_geometry_id{1};
  uint32_t next_drawable_id{1};
};

}