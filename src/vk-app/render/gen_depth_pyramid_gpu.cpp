#include "gen_depth_pyramid_gpu.hpp"
#include "graphics.hpp"
#include "graphics_context.hpp"
#include "debug_label.hpp"
#include "grove/math/vector.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

struct DepthPyramidLevel {
  vk::ManagedImageView view;
  VkExtent2D extent{};
};

struct DepthPyramidImage {
  bool matches_extent(const VkExtent2D& extent) const {
    return extent.width == image.contents().image.extent.width &&
           extent.height == image.contents().image.extent.height;
  }

  VkExtent2D get_extent() const {
    return VkExtent2D{
      image.contents().image.extent.width,
      image.contents().image.extent.height,
    };
  }

  uint32_t num_mip_levels() const {
    return num_levels;
  }

  vk::ManagedImage image;
  vk::ManagedImageView full_view;
  std::vector<DepthPyramidLevel> levels;
  uint32_t num_levels{};
  VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
};

struct GPUContext {
  Vec2<int> gen_level0_compute_local_size{32, 32};
  Vec2<int> gen_mip_levels_compute_local_size{32, 32};

  gfx::PipelineHandle gen_level0_pipeline;
  gfx::PipelineHandle gen_mip_levels_pipeline;

  Optional<DepthPyramidImage> depth_pyramid_image;

  bool tried_initialize{};
  bool disabled{};
  Optional<bool> set_disabled;
};

Optional<gfx::PipelineHandle> create_gen_level0_pipeline(
  gfx::Context* context, const Vec2<int>& local_size) {
  //
  glsl::LoadComputeProgramSourceParams params{};
  params.file = "depth-pyramid/gen-level0.comp";
  params.compile.defines.push_back(glsl::make_integer_define("LOCAL_SIZE_X", local_size.x));
  params.compile.defines.push_back(glsl::make_integer_define("LOCAL_SIZE_Y", local_size.y));
  auto src = glsl::make_compute_program_source(params);
  if (!src) {
    return NullOpt{};
  }

  return gfx::create_compute_pipeline(context, std::move(src.value()));
}

Optional<gfx::PipelineHandle> create_gen_mip_levels_pipeline(
  gfx::Context* context, const Vec2<int>& local_size) {
  //
  glsl::LoadComputeProgramSourceParams params{};
  params.file = "depth-pyramid/gen-mip-levels.comp";
  params.compile.defines.push_back(glsl::make_integer_define("LOCAL_SIZE_X", local_size.x));
  params.compile.defines.push_back(glsl::make_integer_define("LOCAL_SIZE_Y", local_size.y));
  auto src = glsl::make_compute_program_source(params);
  if (!src) {
    return NullOpt{};
  }
  return gfx::create_compute_pipeline(context, std::move(src.value()));
}

VkImageSubresourceRange mip_subresource(uint32_t mip) {
  VkImageSubresourceRange subresource_range{};
  subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subresource_range.baseMipLevel = mip;
  subresource_range.levelCount = 1;
  subresource_range.baseArrayLayer = 0;
  subresource_range.layerCount = 1;
  return subresource_range;
}

VkImageSubresourceRange all_mips_subresource(uint32_t num_mips) {
  VkImageSubresourceRange subresource_range{};
  subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subresource_range.baseMipLevel = 0;
  subresource_range.levelCount = num_mips;
  subresource_range.baseArrayLayer = 0;
  subresource_range.layerCount = 1;
  return subresource_range;
}

Optional<DepthPyramidImage> create_depth_pyramid_image(const gpu::GenDepthPyramidInfo& info) {
  if (info.scene_image_extent.width == 0 || info.scene_image_extent.height == 0) {
    return NullOpt{};
  }

  auto* alloc = gfx::get_vk_allocator(&info.context);

  auto max_dim = float(std::max(info.scene_image_extent.width, info.scene_image_extent.height));
  auto num_levels = 1 + uint32_t(std::floor(std::log2(max_dim)));

  VkExtent3D extent{info.scene_image_extent.width, info.scene_image_extent.height, 1};
  auto im_create_info = vk::make_image_create_info(
    VK_IMAGE_TYPE_2D, VK_FORMAT_R32_SFLOAT,
    extent, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    VK_IMAGE_TILING_OPTIMAL, 1, num_levels, VK_SAMPLE_COUNT_1_BIT, VK_SHARING_MODE_EXCLUSIVE);

  auto im_res = vk::create_device_local_image(alloc, &im_create_info);
  if (!im_res) {
    return NullOpt{};
  }

  vk::ManagedImageView full_view;
  {
    auto view_info = vk::make_image_view_create_info(
      im_res.value.contents().image.handle, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32_SFLOAT,
      vk::make_identity_component_mapping(), all_mips_subresource(num_levels));

    auto view_res = vk::create_image_view(info.vk_context.core.device.handle, &view_info);
    if (!view_res) {
      return NullOpt{};
    } else {
      full_view = vk::ManagedImageView{view_res.value, info.vk_context.core.device.handle};
    }
  }

  std::vector<DepthPyramidLevel> levels;
  uint32_t w = extent.width;
  uint32_t h = extent.height;

  for (uint32_t i = 0; i < num_levels; i++) {
    auto& level = levels.emplace_back();
    auto view_info = vk::make_image_view_create_info(
      im_res.value.contents().image.handle, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32_SFLOAT,
      vk::make_identity_component_mapping(), mip_subresource(i));

    auto view_res = vk::create_image_view(info.vk_context.core.device.handle, &view_info);
    if (!view_res) {
      return NullOpt{};
    } else {
      level.view = vk::ManagedImageView{view_res.value, info.vk_context.core.device.handle};
      level.extent = VkExtent2D{w, h};
      w = std::max(1u, w / 2);
      h = std::max(1u, h / 2);
    }
  }

  DepthPyramidImage result{};
  result.image = std::move(im_res.value);
  result.num_levels = num_levels;
  result.levels = std::move(levels);
  result.full_view = std::move(full_view);
  return Optional<DepthPyramidImage>(std::move(result));
}

bool maybe_create_depth_image_pyramid(GPUContext& context, const gpu::GenDepthPyramidInfo& info) {
  const bool need_create = !context.depth_pyramid_image ||
    (context.depth_pyramid_image &&
     !context.depth_pyramid_image.value().matches_extent(info.scene_image_extent));

  if (need_create) {
    if (context.depth_pyramid_image) {
      GROVE_LOG_SEVERE_CAPTURE_META("vkDeviceWaitIdle to recreate depth pyramid image", "gen_depth_pyramid_gpu");
      vkDeviceWaitIdle(info.vk_context.core.device.handle);
    }
    context.depth_pyramid_image = create_depth_pyramid_image(info);
  }

  return context.depth_pyramid_image.has_value();
}

VkMemoryBarrier make_empty_memory_barrier() {
  VkMemoryBarrier result{};
  result.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  return result;
}

void insert_depth_image_pipeline_barrier(GPUContext&, const gpu::GenDepthPyramidInfo& info) {
  VkMemoryBarrier barrier = make_empty_memory_barrier();
  barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(
    info.cmd,
    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void gen_level0(GPUContext& context, const gpu::GenDepthPyramidInfo& info) {
  struct PushConstants {
    Vec4f dimensions;
  };

  auto db_label = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "gen_depth_pyramid_level0");
  (void) db_label;

  auto& pyr = context.depth_pyramid_image.value();

  { //  layout transition
    VkImageMemoryBarrier barrier = vk::make_empty_image_memory_barrier();
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.oldLayout = pyr.layout;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image = pyr.image.contents().image.handle;
    barrier.subresourceRange = all_mips_subresource(pyr.num_levels);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
      info.cmd,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      0, 0, nullptr, 0, nullptr, 1, &barrier);

    pyr.layout = VK_IMAGE_LAYOUT_GENERAL;
  }

  auto sampler = gfx::get_image_sampler_nearest_edge_clamp(&info.context);

  auto& pipe = context.gen_level0_pipeline;
  vk::cmd::bind_compute_pipeline(info.cmd, pipe.get());

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};
  vk::push_combined_image_sampler(scaffold, bind++, info.sample_scene_depth_image.value(), sampler);
  vk::push_storage_image(scaffold, bind++, pyr.levels[0].view.contents().handle, VK_IMAGE_LAYOUT_GENERAL);

  auto desc_set = gfx::require_updated_descriptor_set(&info.context, scaffold, pipe);
  if (!desc_set) {
    return;
  }

  vk::cmd::bind_compute_descriptor_sets(info.cmd, pipe.get_layout(), 0, 1, &desc_set.value());

  const auto& extent = pyr.levels[0].extent;
  PushConstants pcs{};
  pcs.dimensions = Vec4f{float(extent.width), float(extent.height), 0, 0};
  vk::cmd::push_constants(info.cmd, pipe.get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, &pcs);

  const auto x = uint32_t(std::ceil(double(extent.width) / float(context.gen_level0_compute_local_size.x)));
  const auto y = uint32_t(std::ceil(double(extent.height) / float(context.gen_level0_compute_local_size.y)));
  vkCmdDispatch(info.cmd, x, y, 1);
}

bool gen_mip_levels(GPUContext& context, const gpu::GenDepthPyramidInfo& info) {
  struct GenMipLevelsPushConstants {
    Vec4f src_dst_dimensions;
  };

  auto db_label = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "gen_depth_pyramid_mip_levels");
  (void) db_label;

  bool success{true};

  auto& pyr = context.depth_pyramid_image.value();

  auto& pipe = context.gen_mip_levels_pipeline;
  vk::cmd::bind_compute_pipeline(info.cmd, pipe.get());

  for (int i = 0; i < int(pyr.num_levels)-1; i++) {
    auto& src = pyr.levels[i];
    auto& dst = pyr.levels[i + 1];

    {
      VkImageMemoryBarrier barrier = vk::make_empty_image_memory_barrier();
      barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      barrier.image = pyr.image.contents().image.handle;
      barrier.subresourceRange = mip_subresource(i);
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

      vkCmdPipelineBarrier(
        info.cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    vk::DescriptorSetScaffold scaffold;
    scaffold.set = 0;
    uint32_t bind{};
    vk::push_storage_image(scaffold, bind++, src.view.contents().handle, VK_IMAGE_LAYOUT_GENERAL);
    vk::push_storage_image(scaffold, bind++, dst.view.contents().handle, VK_IMAGE_LAYOUT_GENERAL);

    auto desc_set = gfx::require_updated_descriptor_set(&info.context, scaffold, pipe);
    if (!desc_set) {
      success = false;
      break;  //  break so that layout transition to read-only still happens
    }

    vk::cmd::bind_compute_descriptor_sets(info.cmd, pipe.get_layout(), 0, 1, &desc_set.value());

    GenMipLevelsPushConstants pc{};
    pc.src_dst_dimensions = Vec4f{
      float(src.extent.width), float(src.extent.height),
      float(dst.extent.width), float(dst.extent.height)
    };

    vk::cmd::push_constants(info.cmd, pipe.get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, &pc);
    const auto sx = float(context.gen_mip_levels_compute_local_size.x);
    const auto sy = float(context.gen_mip_levels_compute_local_size.y);
    const auto x = uint32_t(std::ceil(double(dst.extent.width) / sx));
    const auto y = uint32_t(std::ceil(double(dst.extent.height) / sy));
    vkCmdDispatch(info.cmd, x, y, 1);
  }

  {
    VkImageMemoryBarrier barrier = vk::make_empty_image_memory_barrier();
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.image = pyr.image.contents().image.handle;
    barrier.subresourceRange = all_mips_subresource(pyr.num_levels);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
      info.cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      0, 0, nullptr, 0, nullptr, 1, &barrier);

    pyr.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  return success;
}

void try_initialize(GPUContext& context, const gpu::GenDepthPyramidInfo& info) {
  if (auto pipe = create_gen_level0_pipeline(&info.context, context.gen_level0_compute_local_size)) {
    context.gen_level0_pipeline = std::move(pipe.value());
  }
  if (auto pipe = create_gen_mip_levels_pipeline(&info.context, context.gen_mip_levels_compute_local_size)) {
    context.gen_mip_levels_pipeline = std::move(pipe.value());
  }
}

gpu::GenDepthPyramidResult gen_depth_pyramid(GPUContext& context, const gpu::GenDepthPyramidInfo& info) {
  gpu::GenDepthPyramidResult result{};

  if (context.set_disabled) {
    context.disabled = context.set_disabled.value();
    context.set_disabled = NullOpt{};
  }

  if (!context.disabled) {
    if (!context.tried_initialize) {
      try_initialize(context, info);
      context.tried_initialize = true;
    }

    if (maybe_create_depth_image_pyramid(context, info)) {
      insert_depth_image_pipeline_barrier(context, info);
      if (context.gen_level0_pipeline.is_valid() && context.gen_mip_levels_pipeline.is_valid()) {
        gen_level0(context, info);
        if (gen_mip_levels(context, info)) {
          result.sample_depth_pyramid = vk::SampleImageView{
            context.depth_pyramid_image.value().full_view.contents().handle,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
          };
          result.depth_pyramid_image_extent = context.depth_pyramid_image.value().get_extent();
          result.depth_pyramid_image_num_mips = context.depth_pyramid_image.value().num_mip_levels();
        }
      }
    }
  }

  return result;
}

struct {
  GPUContext context;
} globals;

} //  anon

gpu::GenDepthPyramidResult gpu::gen_depth_pyramid(const GenDepthPyramidInfo& info) {
  return gen_depth_pyramid(globals.context, info);
}

void gpu::terminate_gen_depth_pyramid() {
  globals.context = {};
}

bool gpu::get_set_gen_depth_pyramid_enabled(const bool* v) {
  if (v) {
    globals.context.set_disabled = !*v;
  }
  return !globals.context.disabled;
}

GROVE_NAMESPACE_END

