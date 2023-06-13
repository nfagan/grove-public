#pragma once

#include "../vk/vk.hpp"
#include "SampledImageManager.hpp"
#include "DynamicSampledImageManager.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/Mat4.hpp"
#include "grove/math/Bounds3.hpp"
#include "grove/common/identifier.hpp"
#include "grove/common/Stopwatch.hpp"
#include <bitset>

namespace grove {

class Camera;

struct NewGrassRendererMaterialData;

namespace csm {
struct CSMDescriptor;
}

namespace gfx {
struct Context;
}

namespace vk {
struct GraphicsContext;
}

class TerrainRenderer {
public:
  struct InitInfo {
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::PipelineSystem& pipeline_system;
    vk::DescriptorSystem& desc_system;
    vk::BufferSystem& buffer_system;
    vk::CommandProcessor& uploader;
    const vk::PipelineRenderPassInfo& forward_pass_info;
    const vk::PipelineRenderPassInfo& shadow_pass_info;
    uint32_t frame_queue_depth;
  };

  struct BeginFrameInfo {
    gfx::Context& context;
    const Camera& camera;
    const csm::CSMDescriptor& csm_desc;
    uint32_t frame_index;
    const NewGrassRendererMaterialData& grass_material_data;
  };

  struct RenderInfo {
    gfx::Context& context;
    const vk::Core& core;
    vk::SamplerSystem& sampler_system;
    vk::DescriptorSystem& desc_system;
    const vk::SampledImageManager& sampled_image_manager;
    const vk::DynamicSampledImageManager& dynamic_sampled_image_manager;
    uint32_t frame_index;
    VkCommandBuffer cmd;
    VkViewport viewport;
    VkRect2D scissor_rect;
    const Camera& camera;
    const vk::SampleImageView& shadow_image;
    const csm::CSMDescriptor& csm_descriptor;
  };

  struct ShadowRenderInfo {
    uint32_t frame_index;
    VkCommandBuffer cmd;
    VkViewport viewport;
    VkRect2D scissor_rect;
    const Mat4f& light_view_proj;
  };

  struct CubeMarchVertex {
    static size_t stride() {
      return sizeof(Vec4f) * 2;
    }
    static size_t position_offset() {
      return 0;
    }
    static size_t normal_offset() {
      return sizeof(Vec4f);
    }

    Vec3f position;
    float pad1;
    Vec3f normal;
    float pad2;
  };

  struct TerrainGrassInstance {
    Vec4f translation_rand01;
    Vec4f direction_unused;
  };

  struct TerrainGrassDrawableHandle {
    GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
    GROVE_INTEGER_IDENTIFIER_EQUALITY(TerrainGrassDrawableHandle, id)
    uint32_t id;
  };

  struct TerrainGrassInstanceBuffer {
    vk::BufferSystem::BufferHandle buffer;
    std::vector<unsigned char> cpu_data;
    uint32_t num_instances_reserved;
    uint32_t num_instances;
    std::bitset<32> modified;
  };

  using GetGeometryData = std::function<void(const void**, size_t*)>;

  struct CubeMarchChunkHandle {
    GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
    GROVE_INTEGER_IDENTIFIER_EQUALITY(CubeMarchChunkHandle, id)
    uint32_t id;
  };

  struct AddResourceContext {
    uint32_t frame_queue_depth;
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::BufferSystem& buffer_system;
  };

  struct RenderParams {
    float terrain_dim;
    float min_shadow{0.0f};
    float global_color_scale{1.0f};
    float frac_global_color_scale{1.0f};
    Vec3f sun_position{};
    Vec3f sun_color{};
    Vec4f wind_world_bound_xz{};
  };

public:
  bool initialize(const InitInfo& init_info);
  void terminate();
  void begin_frame(const BeginFrameInfo& info);
  void render(const RenderInfo& info);
  void render_shadow(const ShadowRenderInfo& info);
  bool is_valid() const;
  void set_cube_march_geometries_hidden(bool hide) {
    hide_cube_map_geometries = hide;
  }
  void set_color_image(vk::SampledImageManager::Handle handle) {
    color_image_handle = handle;
  }
  void set_height_map_image(vk::DynamicSampledImageManager::Handle handle) {
    height_map_image_handle = handle;
  }
  void set_wind_displacement_image(vk::DynamicSampledImageManager::Handle handle) {
    wind_displacement_image_handle = handle;
  }
  void set_splotch_image(vk::SampledImageManager::Handle handle) {
    splotch_image_handle = handle;
  }
  void set_alt_color_image(vk::SampledImageManager::Handle handle) {
    alt_color_image_handle = handle;
  }
  void set_new_material_image(vk::SampledImageManager::Handle handle) {
    new_material_image_handle = handle;
  }
  RenderParams& get_render_params() {
    return render_params;
  }
  void remake_program(const InitInfo& info);

  void set_chunk_modified(const AddResourceContext& context, CubeMarchChunkHandle chunk);
  bool require_chunk(const AddResourceContext& context, CubeMarchChunkHandle* handle,
                     uint32_t num_reserve, GetGeometryData&& get_data, const Bounds3f& world_aabb);
  void destroy_chunk(CubeMarchChunkHandle handle);

  bool reserve(const AddResourceContext& context, TerrainGrassDrawableHandle* handle,
               uint32_t num_instances);
  void set_instances(const AddResourceContext& context, TerrainGrassDrawableHandle handle,
                     const TerrainGrassInstance* instances, uint32_t num_instances);

  void toggle_new_material_pipeline();
  static AddResourceContext make_add_resource_context(vk::GraphicsContext& graphics_context);

private:
  void render_original(const RenderInfo& info);
  void render_cube_march(const RenderInfo& info);
  void render_grass(const RenderInfo& info);
  void render_new_material(const RenderInfo& info);

  bool make_program(const InitInfo& info, glsl::VertFragProgramSource* source);
  void require_desc_set_allocators(vk::DescriptorSystem& desc_system,
                                   const glsl::VertFragProgramSource* sources,
                                   int num_sources);
  bool any_cube_march_active() const;
  bool any_grass_active() const;

  Optional<vk::SampledImageManager::ReadInstance>
  get_color_image(const vk::SampledImageManager& manager);

  Optional<vk::SampledImageManager::ReadInstance>
  get_new_material_image(const vk::SampledImageManager& manager);

  Optional<vk::SampledImageManager::ReadInstance>
  get_alt_color_image(const vk::SampledImageManager& manager);

  Optional<vk::SampledImageManager::ReadInstance>
  get_splotch_image(const vk::SampledImageManager& manager);

  Optional<vk::DynamicSampledImageManager::ReadInstance>
  get_height_image(const vk::DynamicSampledImageManager& manager);

  Optional<vk::DynamicSampledImageManager::ReadInstance>
  get_wind_displacement_image(const vk::DynamicSampledImageManager& manager);

public:
  Unique<vk::DescriptorSystem::PoolAllocatorHandle> desc_pool_allocator;
  Unique<vk::DescriptorSystem::SetAllocatorHandle> desc_set0_allocator;
  Unique<vk::DescriptorSystem::SetAllocatorHandle> cube_march_set0_allocator;
  Unique<vk::DescriptorSystem::SetAllocatorHandle> grass_set0_allocator;

  VkPipelineLayout pipeline_layout{};
  vk::PipelineSystem::PipelineHandle pipeline_handle;
  vk::BorrowedDescriptorSetLayouts desc_set_layouts;

  RenderParams render_params{};

  struct FrameData {
    vk::BufferSystem::BufferHandle uniform_buffer;
    vk::BufferSystem::BufferHandle shadow_uniform_buffer;
  };

  struct Set0UniformBuffer {
    vk::BufferSystem::BufferHandle buffer;
    size_t stride;
  };

  struct TerrainGrassGeometry {
    vk::BufferSystem::BufferHandle vertex;
    vk::BufferSystem::BufferHandle index;
    uint32_t num_indices;
  };

  struct CubeMarchGeometry {
    vk::BufferSystem::BufferHandle geometry;
    GetGeometryData get_geometry_data;
    uint32_t num_vertices_reserved;
    uint32_t num_vertices_active;
    Bounds3f world_bound;
    std::bitset<32> modified;
  };

  vk::BufferSystem::BufferHandle vertex_buffer;
  vk::BufferSystem::BufferHandle index_buffer;
  vk::DrawIndexedDescriptor draw_desc;

  Set0UniformBuffer set0_uniform_buffer{};
  Stopwatch stopwatch;

  std::vector<FrameData> frame_data;
  Optional<vk::SampledImageManager::Handle> color_image_handle;
  Optional<vk::DynamicSampledImageManager::Handle> height_map_image_handle;
  Optional<vk::DynamicSampledImageManager::Handle> wind_displacement_image_handle;
  Optional<vk::SampledImageManager::Handle> splotch_image_handle;
  Optional<vk::SampledImageManager::Handle> alt_color_image_handle;
  Optional<vk::SampledImageManager::Handle> new_material_image_handle;

  vk::PipelineSystem::PipelineData cube_march_pipeline_data{};
  vk::PipelineSystem::PipelineData cube_march_shadow_pipeline_data{};
  std::unordered_map<uint32_t, CubeMarchGeometry> cube_march_geometries;
  bool hide_cube_map_geometries{};
  uint32_t next_cube_march_chunk_id{1};
  uint32_t latest_num_cube_march_vertices_drawn{};
  uint32_t latest_num_cube_march_chunks_drawn{};

  std::unordered_map<uint32_t, TerrainGrassInstanceBuffer> grass_instance_buffers;
  vk::PipelineSystem::PipelineData grass_pipeline_data{};
  TerrainGrassGeometry grass_geometry{};
  uint32_t next_grass_instance_buffer_id{1};

  bool disabled{};
  bool prefer_new_material_pipeline{};
  bool prefer_inverted_winding_new_material_pipeline{};
  bool need_create_new_material_pipeline{};
  Optional<bool> set_pcf_enabled;
  bool pcf_enabled{};
};

}