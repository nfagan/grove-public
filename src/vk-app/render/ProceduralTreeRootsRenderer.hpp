#pragma once

#include "../vk/vk.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/Bounds3.hpp"
#include "grove/math/Mat4.hpp"
#include "grove/common/identifier.hpp"
#include "grove/common/Stopwatch.hpp"
#include <bitset>

namespace grove {

class Camera;

namespace vk {
struct GraphicsContext;
}

class ProceduralTreeRootsRenderer {
public:
  enum class DrawableType {
    NoWind,
    Wind
  };

  struct InitInfo {
    vk::Allocator* allocator;
    const vk::Core& core;
    vk::BufferSystem& buffer_system;
    vk::StagingBufferSystem& staging_buffer_system;
    vk::PipelineSystem& pipeline_system;
    vk::DescriptorSystem& desc_system;
    vk::CommandProcessor& command_processor;
    uint32_t frame_queue_depth;
    const vk::PipelineRenderPassInfo& forward_pass_info;
    const vk::PipelineRenderPassInfo& shadow_pass_info;
  };

  struct BeginFrameInfo {
    uint32_t frame_index;
  };

  struct RenderInfo {
    VkDevice device;
    vk::Allocator* allocator;
    vk::BufferSystem& buffer_system;
    vk::SamplerSystem& sampler_system;
    vk::DescriptorSystem& descriptor_system;
    uint32_t frame_index;
    VkCommandBuffer cmd;
    VkViewport viewport;
    VkRect2D scissor_rect;
    const Camera& camera;
  };

  struct ShadowRenderInfo {
    VkDevice device;
    uint32_t frame_index;
    VkCommandBuffer cmd;
    VkViewport viewport;
    VkRect2D scissor_rect;
    const Mat4f& shadow_view_proj;
    uint32_t cascade_index;
  };

  struct AddResourceContext {
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::CommandProcessor& command_processor;
    vk::BufferSystem& buffer_system;
    vk::StagingBufferSystem& staging_buffer_system;
    uint32_t frame_queue_depth;
  };

  struct DrawableHandle {
    bool is_wind_type() const {
      return type == DrawableType::Wind;
    }

    GROVE_INTEGER_IDENTIFIER_EQUALITY(DrawableHandle, id)
    GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
    uint32_t id;
    DrawableType type;
  };

  struct Instance {
    Vec4<uint32_t> directions0;
    Vec4<uint32_t> directions1;
    Vec3f self_position;
    float self_radius;
    Vec3f child_position;
    float child_radius;
  };

  struct WindInstance {
    Vec4<uint32_t> packed_axis_root_info0;
    Vec4<uint32_t> packed_axis_root_info1;
    Vec4<uint32_t> packed_axis_root_info2;
  };

  struct Drawable {
    DrawableType type;
    vk::BufferSystem::BufferHandle instance_buffer;
    vk::BufferSystem::BufferHandle wind_instance_buffer;
    std::vector<unsigned char> cpu_data;
    std::vector<unsigned char> wind_cpu_data;
    uint32_t num_instances_reserved;
    uint32_t num_instances_active;
    std::bitset<32> needs_update;
    bool hidden;
    Bounds3f aabb;
    float wind_strength;
    bool wind_disabled;
    Vec4<uint8_t> color;
  };

  struct GeometryBuffer {
    vk::BufferSystem::BufferHandle geom_buff;
    vk::BufferSystem::BufferHandle index_buff;
    uint32_t num_indices;
  };

  struct RenderParams {
    Vec3f sun_position;
    Vec3f sun_color;
    float elapsed_time;
  };

public:
  bool is_valid() const;
  bool initialize(const InitInfo& info);
  void remake_programs(const InitInfo& info);
  void begin_frame(const BeginFrameInfo& info);
  void render(const RenderInfo& info);
  void render_shadow(const ShadowRenderInfo& info);

  RenderParams& get_render_params() {
    return render_params;
  }

  DrawableHandle create(DrawableType type);
  bool reserve(const AddResourceContext& context, DrawableHandle handle, uint32_t num_instances);
  void set(const AddResourceContext& context, DrawableHandle handle,
           const Instance* instances, const WindInstance* wind_instances,
           uint32_t num_instances, uint32_t instance_offset);
  void activate(DrawableHandle handle, uint32_t num_instances);
  void fill_activate(const AddResourceContext& context, DrawableHandle handle,
                     const Instance* instances, uint32_t num_instances);
  void fill_activate(const AddResourceContext& context, DrawableHandle handle,
                     const Instance* instances, const WindInstance* wind_instances,
                     uint32_t num_instances);
  void set_hidden(DrawableHandle handle, bool v);
  void set_aabb(DrawableHandle handle, const Bounds3f& aabb);
  void set_wind_strength(DrawableHandle handle, float v);
  void set_wind_disabled(DrawableHandle handle, bool disable);
  void set_linear_color(DrawableHandle handle, const Vec3<uint8_t>& color);

  void render_non_wind(const RenderInfo& info);
  void render_wind(const RenderInfo& info);
  void draw_non_wind(VkCommandBuffer cmd, uint32_t frame_index, bool enforce_drawable_type);

  static void encode_directions(const Vec3f& self_right, const Vec3f& self_up,
                                const Vec3f& child_right, const Vec3f& child_up,
                                Vec4<uint32_t>* directions0, Vec4<uint32_t>* directions1);

  static AddResourceContext make_add_resource_context(vk::GraphicsContext& context);

public:
  bool initialized{};
  vk::PipelineSystem::PipelineData pipeline_data{};
  vk::PipelineSystem::PipelineData wind_pipeline_data{};
  vk::PipelineSystem::PipelineData shadow_pipeline_data{};
  GeometryBuffer geometry_buffer{};
  std::unordered_map<uint32_t, Drawable> drawables;
  RenderParams render_params{};
  uint32_t next_drawable_id{1};

  Stopwatch stopwatch;
};

}