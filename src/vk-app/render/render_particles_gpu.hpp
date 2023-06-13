#pragma once

#include "grove/math/vector.hpp"
#include "grove/common/Optional.hpp"
#include <vulkan/vulkan.h>

namespace grove {
class Camera;
}

namespace grove::gfx {
struct Context;
}

namespace grove::vk {
struct SampleImageView;
}

namespace grove::particle {

struct SegmentedQuadVertexDescriptor {
  Vec3f position;
  Vec3f color;
  float translucency;
  float min_depth_weight;
};

struct CircleQuadInstanceDescriptor {
  Vec3f position;
  float scale;
  Vec3f color;
  float translucency;
};

struct RenderParticlesRenderInfo {
  uint32_t frame_index;
  VkCommandBuffer cmd;
  VkViewport viewport;
  VkRect2D scissor;
  gfx::Context* graphics_context;
  const Camera& camera;
};

struct RenderParticlesBeginFrameInfo {
  gfx::Context* context;
  uint32_t frame_index;
  const Optional<vk::SampleImageView>& scene_depth_image;
};

struct Stats {
  int last_num_segmented_quad_vertices;
  int last_num_segmented_quad_sample_depth_vertices;
  int last_num_circle_quad_sample_depth_instances;
};

void push_segmented_quad_particle_vertices(const SegmentedQuadVertexDescriptor* descs, uint32_t num_verts);
void push_segmented_quad_sample_depth_image_particle_vertices(
  const SegmentedQuadVertexDescriptor* descs, uint32_t num_verts);
void push_circle_quad_sample_depth_instances(
  const CircleQuadInstanceDescriptor* descs, uint32_t num_insts);
void render_particles_begin_frame(const RenderParticlesBeginFrameInfo& info);
void render_particles_render_forward(const RenderParticlesRenderInfo& info);
void render_particles_render_post_process(const RenderParticlesRenderInfo& info);
void set_render_particles_need_remake_pipelines();
Stats get_render_particles_stats();
void terminate_particle_renderer();

}