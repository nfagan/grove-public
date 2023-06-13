#pragma once

#include "StaticModelRenderer.hpp"
#include "TerrainRenderer.hpp"
#include "GrassRenderer.hpp"
#include "SkyRenderer.hpp"
#include "ProceduralFlowerStemRenderer.hpp"
#include "ProceduralTreeRootsRenderer.hpp"
#include "WindParticleRenderer.hpp"
#include "SimpleShapeRenderer.hpp"
#include "PollenParticleRenderer.hpp"
#include "PointBufferRenderer.hpp"
#include "CloudRenderer.hpp"
#include "PostProcessBlitter.hpp"
#include "RainParticleRenderer.hpp"
#include "DebugImageRenderer.hpp"
#include "ArchRenderer.hpp"
#include "NoiseImages.hpp"

namespace grove {

namespace tree {
struct RenderVineSystem;
}

namespace gfx {
struct Context;
}

struct GraphicsGUIUpdateResult;

class RenderComponent {
public:
  struct InitInfo {
    gfx::Context* graphics_context;
    const vk::Core& core;
    vk::Allocator* allocator;
    const vk::PipelineRenderPassInfo forward_pass_info; //  by value
    const vk::PipelineRenderPassInfo shadow_pass_info;  //  by value
    const vk::PipelineRenderPassInfo post_process_pass_info;  //  by value
    const uint32_t frame_queue_depth;
    bool post_processing_enabled;
    vk::SamplerSystem& sampler_system;
    vk::BufferSystem& buffer_system;
    vk::StagingBufferSystem& staging_buffer_system;
    vk::PipelineSystem& pipeline_system;
    vk::DescriptorSystem& desc_system;
    vk::CommandProcessor& uploader;
    vk::DynamicSampledImageManager& dynamic_image_manager;
    vk::SampledImageManager& image_manager;
    const vk::DynamicSampledImageManager::CreateContext dynamic_image_manager_create_context; //  by value
  };

  struct RenderInfo {
    gfx::Context* graphics_context;
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::SamplerSystem& sampler_system;
    vk::DescriptorSystem& desc_system;
    vk::BufferSystem& buffer_system;
    vk::StagingBufferSystem& staging_buffer_system;
    vk::CommandProcessor& command_processor;
    vk::PipelineSystem& pipeline_system;
    const vk::PipelineRenderPassInfo& forward_pass_info;
    const vk::SampledImageManager& sampled_image_manager;
    const vk::DynamicSampledImageManager& dynamic_sampled_image_manager;
    VkCommandBuffer cmd;
    uint32_t frame_index;
    uint32_t frame_queue_depth;
    VkViewport viewport;
    VkRect2D scissor_rect;
    const vk::SampleImageView& shadow_image;
    bool post_processing_enabled;
    const Camera& camera;
    const csm::CSMDescriptor& csm_descriptor;
  };

  struct ShadowRenderInfo {
    const vk::Device& device;
    vk::DescriptorSystem& desc_system;
    vk::SamplerSystem& sampler_system;
    const vk::SampledImageManager& sampled_image_manager;
    VkCommandBuffer cmd_buffer;
    uint32_t frame_index;
    VkViewport viewport;
    VkRect2D scissor_rect;
    uint32_t cascade_index;
    const Mat4f& view_proj;
    const Camera& scene_camera;
  };

  struct PostProcessPassRenderInfo {
    gfx::Context* graphics_context;
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::SamplerSystem& sampler_system;
    vk::DescriptorSystem& desc_system;
    const vk::SampledImageManager& sampled_image_manager;
    const vk::DynamicSampledImageManager& dynamic_sampled_image_manager;
    VkCommandBuffer cmd;
    uint32_t frame_index;
    uint32_t frame_queue_depth;
    VkViewport viewport;
    VkRect2D scissor_rect;
    Optional<vk::SampleImageView> scene_color_image;
    Optional<vk::SampleImageView> scene_depth_image;
    bool post_processing_enabled;
    bool present_pass_enabled;
    const Camera& camera;
  };

  struct PresentPassRenderInfo {
    gfx::Context* graphics_context;
    const vk::Core& core;
    vk::SamplerSystem& sampler_system;
    vk::DescriptorSystem& descriptor_system;
    VkCommandBuffer cmd;
    uint32_t frame_index;
    VkViewport viewport;
    VkRect2D scissor_rect;
    vk::SampleImageView scene_color_image;
  };

  struct BeginFrameInfo {
    gfx::Context* graphics_context;
    const vk::Core& core;
    vk::Allocator* allocator;
    vk::BufferSystem& buffer_system;
    vk::DescriptorSystem& descriptor_system;
    vk::SamplerSystem& sampler_system;
    vk::StagingBufferSystem& staging_buffer_system;
    vk::CommandProcessor& command_processor;
    vk::PipelineSystem& pipeline_system;
    vk::SampledImageManager& sampled_image_manager;
    vk::DynamicSampledImageManager& dynamic_sampled_image_manager;
    const Camera& camera;
    const csm::CSMDescriptor& csm_desc;
    const vk::RenderFrameInfo& frame_info;
    const vk::PipelineRenderPassInfo& forward_pass_info;
    const vk::PipelineRenderPassInfo& shadow_pass_info;
    const vk::SampleImageView& sample_shadow_image;
    const Optional<vk::SampleImageView>& sample_scene_depth_image;
    tree::RenderVineSystem* render_vine_system;
  };

  struct EarlyGraphicsComputeInfo {
    gfx::Context& context;
    const vk::Core& core;
    VkCommandBuffer cmd;
    uint32_t frame_index;
  };

  struct PostForwardComputeInfo {
    gfx::Context& context;
    vk::GraphicsContext& vk_context;
    VkCommandBuffer cmd;
    uint32_t frame_index;
    VkExtent2D scene_depth_image_extent;
    Optional<vk::SampleImageView> sample_scene_depth_image;
    const Camera& camera;
  };

  struct PostForwardRenderInfo {
    VkCommandBuffer cmd;
    uint32_t frame_index;
    VkViewport viewport;
    VkRect2D scissor_rect;
  };

  struct CommonRenderParams {
    float elapsed_time;
    Vec4f wind_world_bound_xz;
    Vec2f wind_displacement_limits;
    Vec2f branch_wind_strength_limits;
    Vec3f sun_position;
    Vec3f sun_color;
  };

public:
  void initialize(const InitInfo& init_info);
  void terminate(const vk::Core& core);

  void begin_update();
  void begin_frame(const BeginFrameInfo& info);
  void end_frame();
  void early_graphics_compute(const EarlyGraphicsComputeInfo& info);
  void render_forward(const RenderInfo& render_info);
  void post_forward_compute(const PostForwardComputeInfo& info);
  void render_post_forward(const PostForwardRenderInfo& info);
  void render_shadow(const ShadowRenderInfo& render_info);
  void render_post_process_pass(const PostProcessPassRenderInfo& info);
  void render_present_pass(const PresentPassRenderInfo& info);
  void set_foliage_occlusion_system_modified(bool structure_modified,
                                             bool clusters_modified);
  void set_tree_leaves_renderer_enabled(bool enable);
  void set_wind_displacement_image(vk::DynamicSampledImageManager::Handle handle);
  void on_gui_update(const InitInfo& context, const GraphicsGUIUpdateResult& res);
  int get_num_foliage_material1_alpha_texture_layers();

private:
  void render_grass(const RenderInfo& info);

public:
  StaticModelRenderer static_model_renderer;
  TerrainRenderer terrain_renderer;
  GrassRenderer grass_renderer;
  SkyRenderer sky_renderer;
  ProceduralTreeRootsRenderer procedural_tree_roots_renderer;
  ProceduralFlowerStemRenderer procedural_flower_stem_renderer;
  WindParticleRenderer wind_particle_renderer;
  SimpleShapeRenderer simple_shape_renderer;
  PollenParticleRenderer pollen_particle_renderer;
  vk::PointBufferRenderer point_buffer_renderer;
  CloudRenderer cloud_renderer;
  PostProcessBlitter post_process_blitter;
  RainParticleRenderer rain_particle_renderer;
  DebugImageRenderer debug_image_renderer;
  ArchRenderer arch_renderer;
  vk::NoiseImages noise_images;
  bool prefer_to_render_ui_at_native_resolution{};
  bool render_grass_late{};
  CommonRenderParams common_render_params{};
};

}