#pragma once

#include "grove/common/Optional.hpp"
#include "../vk/vk.hpp"

namespace grove::gfx {
struct Context;
}

namespace grove {
class Camera;
}

namespace grove::cull {

struct OcclusionCullAgainstDepthPyramidStats {
  uint32_t prev_num_occluded;
  uint32_t prev_num_visible;
  uint32_t prev_num_total;
  uint32_t prev_num_frustum_culled;
  uint32_t prev_num_purely_occlusion_culled;
};

enum class OcclusionCullAgainstDepthPyramidResultStatus : uint32_t {
  Occluded = 0,
  Visible = 1,
};

struct OcclusionCullAgainstDepthPyramidElementResult {
  uint32_t result;
};

struct OcclusionCullAgainstDepthPyramidResult {
  VkBuffer result_buffer;
  size_t num_elements;
};

struct OcclusionCullAgainstDepthPyramidResults {
  OcclusionCullAgainstDepthPyramidResult results[4];
};

struct OcclusionCullFrustumCullInfo {
  const vk::ManagedBuffer* instances;
  const vk::ManagedBuffer* cull_results;
  size_t num_instances;
};

struct OcclusionCullDepthPyramidInfo {
  vk::SampleImageView depth_pyramid_image;
  uint32_t depth_pyramid_image_max_mip;
  VkExtent2D depth_pyramid_image_extent;
};

struct OcclusionCullAgainstDepthPyramidInfo {
  gfx::Context& context;
  Optional<OcclusionCullDepthPyramidInfo> depth_pyramid_info;
  const Optional<OcclusionCullFrustumCullInfo>* frustum_cull_infos;
  uint32_t num_cull_infos;
  VkCommandBuffer cmd;
  uint32_t frame_index;
  const Camera& camera;
};

void occlusion_cull_against_depth_pyramid_begin_frame(
  const bool* frustum_cull_data_modified, uint32_t num_data_sets);

void occlusion_cull_against_depth_pyramid(const OcclusionCullAgainstDepthPyramidInfo& info);

Optional<OcclusionCullAgainstDepthPyramidResult>
get_previous_occlusion_cull_against_depth_pyramid_result(uint32_t input_index);

OcclusionCullAgainstDepthPyramidStats
get_occlusion_cull_against_depth_pyramid_stats(uint32_t input_index);

void push_read_occlusion_cull_preprocessor_defines(glsl::PreprocessorDefinitions& defines);

void terminate_occlusion_cull_against_depth_pyramid();

}