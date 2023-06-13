#pragma once

#include "SampledImageManager.hpp"
#include "csm.hpp"
#include "../vk/vk.hpp"
#include "grove/visual/types.hpp"
#include "grove/common/identifier.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/memory.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/matrix.hpp"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace grove {

class Camera;

namespace vk {
struct GraphicsContext;
}

class StaticModelRenderer {
public:
  struct GeometryHandle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(GeometryHandle, id)
    GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, GeometryHandle, id)
    GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
    uint32_t id{};
  };

  struct MaterialHandle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(MaterialHandle, id)
    GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, MaterialHandle, id)
    GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
    uint32_t id{};
  };

  struct DrawableHandle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(DrawableHandle, id)
    GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, DrawableHandle, id)
    GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
    uint32_t id{};
  };

  struct Geometry {
    vk::BufferSystem::BufferHandle buffer;
    vk::DrawDescriptor draw_descriptor;
  };

  struct Material {
    vk::SampledImageManager::Handle image_handle;
  };

  struct AddResourceContext {
    vk::Allocator* allocator;
    const vk::Core& core;
    vk::CommandProcessor& uploader;
    vk::SampledImageManager& sampled_image_manager;
    vk::BufferSystem& buffer_system;
  };

  struct DrawableParams {
    Mat4f transform;
  };

  struct Drawable {
    GeometryHandle geometry;
    MaterialHandle material;
    DrawableParams params;
    int buffer_index{};
    int buffer_element{};
  };

  struct InitInfo {
    const vk::Core& core;
    vk::Allocator* allocator;
    const vk::PipelineRenderPassInfo& forward_pass_info;
    const vk::PipelineRenderPassInfo& shadow_pass_info;
    uint32_t frame_queue_depth;
    vk::PipelineSystem& pipeline_system;
    vk::DescriptorSystem& desc_system;
  };

  struct BeginFrameInfo {
    const Camera& camera;
    const csm::CSMDescriptor& csm_desc;
    uint32_t frame_index;
  };

  struct RenderInfo {
    const vk::Core& core;
    vk::SamplerSystem& sampler_system;
    vk::DescriptorSystem& desc_system;
    const vk::SampledImageManager& sampled_image_manager;
    VkCommandBuffer cmd_buffer;
    VkViewport viewport;
    VkRect2D scissor_rect;
    uint32_t frame_index;
    const Camera& camera;
    const vk::SampleImageView& shadow_image;
    const csm::CSMDescriptor& csm_descriptor;
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
    Vec3f sun_position;
    Vec3f sun_color;
  };

public:
  bool is_valid() const;
  bool initialize(const InitInfo& init_info);
  bool remake_programs(const InitInfo& info);
  void destroy(const vk::Device& device);
  void begin_frame(const BeginFrameInfo& info);
  void render(const RenderInfo& render_info);
  void render_shadow(const ShadowRenderInfo& render_info);
  void set_params(DrawableHandle handle, const DrawableParams& params);
  RenderParams& get_render_params() {
    return render_params;
  }

  Optional<DrawableHandle> add_drawable(const AddResourceContext& context,
                                        GeometryHandle geometry,
                                        MaterialHandle material,
                                        const DrawableParams& params);

  Optional<GeometryHandle> add_geometry(const AddResourceContext& context,
                                        const void* data,
                                        const VertexBufferDescriptor& descriptor,
                                        std::size_t size,
                                        int pos_ind,
                                        int norm_ind,
                                        int uv_ind);
  bool require_geometry(const AddResourceContext& context,
                        const void* data,
                        const VertexBufferDescriptor& descriptor,
                        std::size_t size,
                        int pos_ind,
                        int norm_ind,
                        int uv_ind, GeometryHandle* handle);

  Optional<MaterialHandle> add_texture_material(const AddResourceContext& context,
                                                vk::SampledImageManager::Handle handle);

  static AddResourceContext make_add_resource_context(vk::GraphicsContext& graphics_context);

private:
  bool initialize_forward_pipeline(const InitInfo& info, glsl::VertFragProgramSource* prog_source);
  bool initialize_shadow_pipeline(const InitInfo& info, glsl::VertFragProgramSource* prog_source);
  void update_forward_buffers(const BeginFrameInfo& info);

private:
  struct PerDrawableUniformBuffers {
    vk::ManagedBuffer forward_gpu_data;
    vk::ManagedBuffer shadow_gpu_data;
    int count{};
  };

  struct UniformCPUData {
    UniquePtrWithDeleter<void> forward_cpu_data;
    UniquePtrWithDeleter<void> shadow_cpu_data;
  };

  struct UniformBufferInfo {
    size_t forward_stride{};
    size_t shadow_stride{};
    size_t forward_size{};
    size_t shadow_size{};
    size_t shadow_size_per_cascade{};
    size_t align{};
  };

  struct DrawableUniformBuffers {
    std::vector<PerDrawableUniformBuffers> buffers;
  };

  RenderParams render_params{};

  std::unordered_map<GeometryHandle, Geometry, GeometryHandle::Hash> geometries;
  std::unordered_map<MaterialHandle, Material, MaterialHandle::Hash> materials;
  std::unordered_map<DrawableHandle, Drawable, DrawableHandle::Hash> drawables;
  std::vector<DrawableUniformBuffers> drawable_uniform_buffers;
  std::vector<UniformCPUData> uniform_cpu_data;
  std::vector<int> drawable_uniform_buffer_free_list;
  std::vector<vk::ManagedBuffer> forward_shadow_data_uniform_buffers;

  UniformBufferInfo uniform_buffer_info{};

  Unique<vk::DescriptorSystem::PoolAllocatorHandle> desc_pool_allocator;
  Unique<vk::DescriptorSystem::SetAllocatorHandle> forward_set0_allocator;
  Unique<vk::DescriptorSystem::SetAllocatorHandle> forward_set1_allocator;
  Unique<vk::DescriptorSystem::SetAllocatorHandle> shadow_set0_allocator;

  VkPipelineLayout forward_pipeline_layout;
  vk::PipelineSystem::PipelineHandle forward_pipeline;

  VkPipelineLayout shadow_pipeline_layout;
  vk::PipelineSystem::PipelineHandle shadow_pipeline;

  vk::BorrowedDescriptorSetLayouts forward_layouts;
  vk::BorrowedDescriptorSetLayouts shadow_layouts;

  uint32_t next_geometry_id{1};
  uint32_t next_material_id{1};
  uint32_t next_drawable_id{1};

  bool initialized{};
  bool initialized_programs{};
};

}