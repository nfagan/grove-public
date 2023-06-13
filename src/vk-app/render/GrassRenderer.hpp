#pragma once

#include "SampledImageManager.hpp"
#include "DynamicSampledImageManager.hpp"
#include "../grass/grass.hpp"
#include "../vk/vk.hpp"
#include "../render/csm.hpp"
#include "grove/common/Stopwatch.hpp"

namespace grove {

namespace gfx {
struct Context;
}

struct NewGrassRendererMaterialData {
  Vec4f base_color0_spec_scale;
  Vec4f base_color1_spec_power;
  Vec4f tip_color_overall_scale;
  Vec4f color_variation_unused;
};

class GrassRenderer {
public:
  struct InitInfo {
    gfx::Context& context;
    const vk::Core& core;
    vk::Allocator* allocator;
    uint32_t frame_queue_depth;
    const vk::PipelineRenderPassInfo& forward_pass_info;
    vk::PipelineSystem& pipeline_system;
    vk::DescriptorSystem& descriptor_system;
    vk::BufferSystem& buffer_system;
  };

  struct BeginFrameInfo {
    gfx::Context& context;
    const Camera& camera;
    const csm::CSMDescriptor& csm_desc;
    uint32_t frame_index;
  };

  struct RenderInfo {
    gfx::Context& context;
    VkDevice device;
    vk::SamplerSystem& sampler_system;
    vk::DescriptorSystem& descriptor_system;
    const vk::SampledImageManager& sampled_image_manager;
    const vk::DynamicSampledImageManager& dynamic_sampled_image_manager;
    uint32_t frame_index;
    VkCommandBuffer cmd;
    VkViewport viewport;
    VkRect2D scissor_rect;
    const csm::CSMDescriptor& csm_descriptor;
    const vk::SampleImageView& shadow_image;
    const Camera& camera;
  };

  struct SetDataContext {
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::BufferSystem& buffer_system;
    vk::CommandProcessor& uploader;
    const vk::RenderFrameInfo& frame_info;
  };

  struct NewMaterialParams {
    static NewMaterialParams from_frac_fall(float f, bool prefer_new);
    static NewMaterialParams config_fall();
    static NewMaterialParams config_default();
    static NewMaterialParams config_default_new();

    Vec3f base_color0{0.286f, 0.7835f, 0.1559f};
//    Vec3f base_color1{0.4432f, 1.4104f, 0.2807f};
    Vec3f base_color1{0.4432f, 1.0f, 0.2807f};
    Vec3f tip_color{1.0f};
    float spec_scale{0.4f};
    float spec_power{1.776f};
    float min_overall_scale{0.85f};
    float max_overall_scale{1.25f};
    float min_color_variation{0.2f};
    float max_color_variation{1.0f};
  };

  struct RenderParams {
    Vec3f sun_position;
    Vec3f sun_color;
    Vec4f wind_world_bound_xz;
    float terrain_grid_scale;
    float min_shadow{0.0f};
    float global_color_scale{1.0f};
    float frac_global_color_scale{1.0f};
    float max_diffuse{1.0f};
    float max_specular{1.0f};
    bool prefer_season_controlled_new_material_params{true};
    bool prefer_revised_new_material_params{true};
    NewMaterialParams new_material_params{};
    NewMaterialParams season_controlled_new_material_params{};
  };

public:
  bool is_valid() const;
  void set_high_lod_params(const GrassVisualParams& params);
  void set_low_lod_params(const GrassVisualParams& params);
  void set_high_lod_data(const SetDataContext& context,
                         const FrustumGridInstanceData& instance_data,
                         const std::vector<float>& grid_data);
  void set_low_lod_data(const SetDataContext& context,
                        const FrustumGridInstanceData& instance_data,
                        const std::vector<float>& grid_data);
  void remake_programs(const InitInfo& info,
                       Optional<bool> pcf_enabled);
  bool is_pcf_enabled() const {
    return !pcf_disabled;
  }
  RenderParams& get_render_params() {
    return render_params;
  }
  const RenderParams& get_render_params() const {
    return render_params;
  }
  void set_low_lod_enabled(bool v) {
    low_lod_info.disabled = !v;
  }
  bool is_low_lod_enabled() const {
    return !low_lod_info.disabled;
  }
  void set_high_lod_enabled(bool v) {
    high_lod_info.disabled = !v;
  }
  bool is_high_lod_enabled() const {
    return !high_lod_info.disabled;
  }
  bool is_high_lod_post_pass_enabled() const {
    return !high_lod_info.post_pass_disabled;
  }
  void set_high_lod_post_pass_enabled(bool v) {
    high_lod_info.post_pass_disabled = !v;
  }

  NewGrassRendererMaterialData get_new_material_data() const;

  void begin_frame_set_high_lod_grid_data(const SetDataContext& context,
                                          const FrustumGrid& grid);
  void begin_frame_set_low_lod_grid_data(const SetDataContext& context,
                                         const FrustumGrid& grid);

  void set_terrain_color_image(vk::SampledImageManager::Handle handle) {
    terrain_color_image = handle;
  }
  void set_alt_terrain_color_image(vk::SampledImageManager::Handle handle) {
    alt_terrain_color_image = handle;
  }
  void set_wind_displacement_image(vk::DynamicSampledImageManager::Handle handle) {
    wind_displacement_image = handle;
  }
  void set_height_map_image(vk::DynamicSampledImageManager::Handle handle) {
    height_map_image = handle;
  }

  void initialize(const InitInfo& init_info);
  void terminate();
  void render(const RenderInfo& render_info);
  void begin_frame(const BeginFrameInfo& info);

  uint32_t get_latest_total_num_vertices_drawn() const {
    return latest_total_num_vertices_drawn;
  }
  void toggle_new_material_pipeline();

private:
  bool make_low_lod_program(const InitInfo& info, glsl::VertFragProgramSource* source);
  bool make_high_lod_program(const InitInfo& info, glsl::VertFragProgramSource* source);
  void make_desc_set_allocators(vk::DescriptorSystem& desc_system,
                                const glsl::VertFragProgramSource& high_lod,
                                const glsl::VertFragProgramSource& low_lod);

  void render_high_lod(const RenderInfo& render_info, size_t un_buff_dyn_off);
  void render_low_lod(const RenderInfo& render_info);
  void render_new_material_high_lod(const RenderInfo& render_info, size_t un_buff_dyn_off);
  void render_new_material_low_lod(const RenderInfo& render_info);

  Optional<vk::SampledImageManager::ReadInstance>
  get_terrain_color_image(const vk::SampledImageManager& manager);

  Optional<vk::DynamicSampledImageManager::ReadInstance>
  get_wind_displacement_image(const vk::DynamicSampledImageManager& manager);

  Optional<vk::DynamicSampledImageManager::ReadInstance>
  get_height_map_image(const vk::DynamicSampledImageManager& manager);

private:
  struct Buffers {
    vk::BufferSystem::BufferHandle instance;
    vk::BufferSystem::BufferHandle geometry;
    vk::BufferSystem::BufferHandle index;
    vk::BufferSystem::BufferHandle grid;
    std::size_t current_grid_data_size{};
    DynamicArray<vk::BufferSystem::BufferHandle, 2> uniform;
    size_t uniform_stride{};
  };

  struct Info {
    vk::DrawDescriptor draw_desc;
    vk::DrawIndexedDescriptor draw_indexed_desc;
    GrassVisualParams visual_params;
    bool has_data{};
    Vec2f grid_cell_size{};
    float grid_z_extent{};
    float grid_z_offset{};
    bool disabled{};
    bool post_pass_disabled{};
  };

  struct ProgramComponents {
    Unique<vk::DescriptorSystem::SetAllocatorHandle> desc_set0_allocator;
    vk::BorrowedDescriptorSetLayouts set_layouts;
    vk::PipelineSystem::PipelineHandle pipeline_handle;
    VkPipelineLayout pipeline_layout;
  };

  RenderParams render_params{};
  bool pcf_disabled{};

  DynamicArray<vk::BufferSystem::BufferHandle, 2> shadow_uniform_buffers;
  Buffers high_lod_buffers;
  Info high_lod_info;
  ProgramComponents high_lod_program_components;

  Buffers low_lod_buffers;
  Info low_lod_info;
  ProgramComponents low_lod_program_components;

  uint32_t latest_total_num_vertices_drawn{};

  Optional<vk::SampledImageManager::Handle> terrain_color_image;
  Optional<vk::SampledImageManager::Handle> alt_terrain_color_image;
  Optional<vk::DynamicSampledImageManager::Handle> wind_displacement_image;
  Optional<vk::DynamicSampledImageManager::Handle> height_map_image;

  Unique<vk::DescriptorSystem::PoolAllocatorHandle> desc_pool_allocator;
  Stopwatch stopwatch;

public:
  bool prefer_alt_color_image{};
  bool prefer_new_material_pipeline{};
  bool need_recreate_new_pipelines{};
};

}