#include "SampledImageManager.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

using Handle = SampledImageManager::Handle;
using Instance = SampledImageManager::Instance;
using ReadInstance = SampledImageManager::ReadInstance;
using ImageCreateInfo = SampledImageManager::ImageCreateInfo;

std::function<void(VkCommandBuffer)> make_image_upload_cmd(
  const Buffer& buffer, const Image& image,
  VkPipelineStageFlags sample_in_stages, uint32_t array_layers) {
  //
  return [image, buffer, sample_in_stages, array_layers](VkCommandBuffer cmd) {
    auto copy = make_buffer_image_copy_shader_read_only_dst(
      image,
      buffer.handle,
      make_color_aspect_image_subresource_range(0, array_layers),
      sample_in_stages);
    cmd::buffer_image_copy(cmd, &copy);
  };
}

void mip_mapped_image_upload(
  VkCommandBuffer cmd, const VkBuffer* buffers, uint32_t num_mips, VkImage image,
  VkExtent3D root_extent, VkPipelineStageFlags sample_in_stages, uint32_t array_layers) {
  //
  for (uint32_t i = 0; i < num_mips; i++) {
    const uint32_t w = std::max(1u, root_extent.width / (1u << i));
    const uint32_t  h = std::max(1u, root_extent.height / (1u << i));
    VkExtent3D im_extent{w, h, root_extent.depth};

    {
      auto barrier = vk::make_empty_image_memory_barrier();
      barrier.srcAccessMask = 0;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.image = image;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.subresourceRange = vk::make_color_aspect_image_subresource_range(0, array_layers, i, 1);
      vkCmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    {
      VkBufferImageCopy region{};
      region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.imageSubresource.baseArrayLayer = 0;
      region.imageSubresource.layerCount = array_layers;
      region.imageSubresource.mipLevel = i;
      region.imageExtent = im_extent;
      vkCmdCopyBufferToImage(
        cmd, buffers[i], image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }
    {
      auto barrier = vk::make_empty_image_memory_barrier();
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      barrier.image = image;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.subresourceRange = vk::make_color_aspect_image_subresource_range(0, array_layers, i, 1);
      vkCmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
        sample_in_stages, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
  }
}

std::function<void(VkCommandBuffer)> make_mip_mapped_image_upload_cmd(
  std::vector<VkBuffer>&& buffers, const Image& image, VkExtent3D root_extent,
  VkPipelineStageFlags sample_in_stages, uint32_t array_layers) {
  //
  return [image, root_extent, buffers = std::move(buffers), sample_in_stages, array_layers](VkCommandBuffer cmd) {
    mip_mapped_image_upload(
      cmd, buffers.data(), uint32_t(buffers.size()),
      image.handle, root_extent, sample_in_stages, array_layers);
  };
}

VkImageType to_vk_image_type(const SampledImageManager::ImageType type) {
  switch (type) {
    case SampledImageManager::ImageType::Image2D:
      return VK_IMAGE_TYPE_2D;
    case SampledImageManager::ImageType::Image2DArray:
      return VK_IMAGE_TYPE_2D;
    default:
      GROVE_ASSERT(false);
      return VK_IMAGE_TYPE_1D;
  }
}

void to_vk_image_properties(const SampledImageManager::ImageCreateInfo& info,
                            VkExtent3D* extent, uint32_t* array_layers) {
  auto& im_desc = info.descriptor;
  if (info.image_type == SampledImageManager::ImageType::Image2D) {
    assert(im_desc.shape.depth == 1);
    *extent = VkExtent3D{
      uint32_t(im_desc.shape.width),
      uint32_t(im_desc.shape.height),
      uint32_t(im_desc.shape.depth)
    };
    *array_layers = 1;  //  @NOTE
  } else if (info.image_type == SampledImageManager::ImageType::Image2DArray) {
    *extent = VkExtent3D{
      uint32_t(im_desc.shape.width),
      uint32_t(im_desc.shape.height),
      1
    };
    *array_layers = im_desc.shape.depth;
  } else {
    assert(false);
  }
}

VkImageViewType to_vk_image_view_type(const SampledImageManager::ImageType type) {
  switch (type) {
    case SampledImageManager::ImageType::Image2D:
      return VK_IMAGE_VIEW_TYPE_2D;
    case SampledImageManager::ImageType::Image2DArray:
      return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    default:
      GROVE_ASSERT(false);
      return VK_IMAGE_VIEW_TYPE_1D;
  }
}

ReadInstance to_read_instance(const Instance& instance) {
  ReadInstance read{};
  read.descriptor = instance.descriptor;
  read.layout = instance.layout;
  read.view = instance.image_view.contents().handle;
  read.sample_in_stages = instance.sample_in_stages;
  read.image_type = instance.image_type;
  read.format = instance.format;
  return read;
}

Optional<SampledImageManager::Instance> create_instance(const Core& core,
                                                        Allocator* allocator,
                                                        CommandProcessor* uploader,
                                                        const ImageCreateInfo& info) {
  GROVE_ASSERT(info.sample_in_stages.flags != 0 &&
               info.image_type != SampledImageManager::ImageType::None);

  const auto& im_desc = info.descriptor;

  VkFormat image_format{};
  if (info.format) {
    image_format = info.format.value();
  } else {
    if (auto format = to_vk_format(im_desc.channels, info.int_conversion)) {
      image_format = format.value();
    } else {
      GROVE_ASSERT(false);
      return NullOpt{};
    }
  }

  VkExtent3D image_extent;
  uint32_t array_layers;
  to_vk_image_properties(info, &image_extent, &array_layers);

  const bool has_mips = info.num_mip_levels > 0;
  const VkImageType image_type = to_vk_image_type(info.image_type);
  const VkImageViewType image_view_type = to_vk_image_view_type(info.image_type);
  const uint32_t mip_levels = !has_mips ? 1 : info.num_mip_levels;
  const VkSampleCountFlagBits num_samples = VK_SAMPLE_COUNT_1_BIT; //  @TODO
  const auto create_info = make_image_create_info(
    image_type, image_format, image_extent,
    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    VK_IMAGE_TILING_OPTIMAL, array_layers, mip_levels, num_samples, VK_SHARING_MODE_EXCLUSIVE);

  auto im_res = create_device_local_image(allocator, &create_info);
  if (!im_res) {
    return NullOpt{};
  }
  auto im = std::move(im_res.value);

  if (has_mips) {
    std::vector<vk::ManagedBuffer> staging_buffers;
    std::vector<VkBuffer> staging_buffer_handles;

    for (uint32_t i = 0; i < mip_levels; i++) {
      const size_t w = std::max(1u, uint32_t(im_desc.width()) / (1u << i));
      const size_t h = std::max(1u, uint32_t(im_desc.height()) / (1u << i));
      const size_t mip_size = array_layers * w * h * im_desc.element_size_bytes();
      auto buff = create_staging_buffer(allocator, mip_size);
      if (!buff) {
        GROVE_ASSERT(false);
        return NullOpt{};
      }
      staging_buffers.emplace_back() = std::move(buff.value);
      staging_buffers.back().write(info.mip_levels[i], mip_size);
      staging_buffer_handles.emplace_back() = staging_buffers.back().contents().buffer.handle;
    }

    auto upload_cmd = make_mip_mapped_image_upload_cmd(
      std::move(staging_buffer_handles),
      im.contents().image, image_extent,
      to_vk_pipeline_stages(info.sample_in_stages), array_layers);

    auto err = uploader->sync_graphics_queue(core, std::move(upload_cmd));
    if (err) {
      return NullOpt{};
    }
  } else {
    const size_t im_size = im_desc.total_size_bytes();
    auto stage_res = create_staging_buffer(allocator, im_size);
    if (!stage_res) {
      GROVE_ASSERT(false);
      return NullOpt{};
    }
    auto stage_buff = std::move(stage_res.value);
    stage_buff.write(info.data, im_size);

    auto upload_cmd = make_image_upload_cmd(
      stage_buff.contents().buffer,
      im.contents().image,
      to_vk_pipeline_stages(info.sample_in_stages),
      array_layers);

    auto err = uploader->sync_graphics_queue(core, std::move(upload_cmd));
    if (err) {
      return NullOpt{};
    }
  }

  const uint32_t layer = 0;
  const uint32_t num_layers = array_layers;
  const uint32_t mip = 0;
  const uint32_t num_mips = mip_levels;

  auto view_create_info = make_image_view_create_info(
    im.contents().image.handle,
    image_view_type,
    image_format,
    make_identity_component_mapping(),
    make_color_aspect_image_subresource_range(layer, num_layers, mip, num_mips));

  auto view_res = vk::create_image_view(core.device.handle, &view_create_info);
  if (!view_res) {
    return NullOpt{};
  }

  Instance instance{};
  instance.descriptor = im_desc;
  instance.image = std::move(im);
  instance.image_view = ManagedImageView{view_res.value, core.device.handle};
  instance.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  //  @TODO
  instance.format = image_format;
  instance.sample_in_stages = info.sample_in_stages;
  instance.image_type = info.image_type;
  return Optional<Instance>(std::move(instance));
}

} //  anon

void vk::SampledImageManager::initialize(const Core* vk_core, Allocator* vk_alloc,
                                         CommandProcessor* cmd) {
  core = vk_core;
  allocator = vk_alloc;
  command_processor = cmd;
}

void vk::SampledImageManager::begin_frame(const RenderFrameInfo& info) {
  int erase_beg{};
  for (int i = 0; i < int(pending_deletion.size()); i++) {
    auto& pend = pending_deletion[erase_beg];
    if (pend.frame_id == info.finished_frame_id) {
      pending_deletion.erase(pending_deletion.begin() + erase_beg);
    } else {
      ++erase_beg;
    }
  }

  frame_info = info;
}

void vk::SampledImageManager::destroy() {
  instances.clear();
  pending_deletion.clear();
}

Optional<ReadInstance> vk::SampledImageManager::get(Handle handle) const {
  if (auto it = instances.find(handle); it != instances.end()) {
    return Optional<ReadInstance>(to_read_instance(it->second));
  } else {
    GROVE_ASSERT(false);
    return NullOpt{};
  }
}

Optional<Handle> vk::SampledImageManager::create_sync(const ImageCreateInfo& info) {
  if (auto inst = grove::create_instance(*core, allocator, command_processor, info)) {
    Handle handle{next_instance_id++};
    instances[handle] = std::move(inst.value());
    return Optional<Handle>(handle);
  } else {
    return NullOpt{};
  }
}

bool vk::SampledImageManager::require_sync(Handle* handle, const ImageCreateInfo& info) {
  if (handle->is_valid()) {
    return recreate_sync(*handle, info);
  } else if (auto dst_handle = create_sync(info)) {
    *handle = dst_handle.value();
    return true;
  } else {
    return false;
  }
}

bool vk::SampledImageManager::recreate_sync(Handle handle, const ImageCreateInfo& info) {
  if (auto it = instances.find(handle); it != instances.end()) {
    if (auto inst = grove::create_instance(*core, allocator, command_processor, info)) {
      PendingDelete del;
      del.instance = std::move(it->second);
      del.frame_id = frame_info.current_frame_id;
      pending_deletion.push_back(std::move(del));

      it->second = std::move(inst.value());
      return true;
    }
  }
  return false;
}

size_t vk::SampledImageManager::num_instances() const {
  return instances.size();
}

size_t vk::SampledImageManager::approx_image_memory_usage() const {
  size_t res{};
  for (auto& [_, inst] : instances) {
    if (inst.image.is_valid()) {
      res += inst.image.get_allocation_size();
    }
  }
  return res;
}

GROVE_NAMESPACE_END
