#include "DynamicSampledImageManager.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace vk;

namespace {

using Handle = DynamicSampledImageManager::Handle;
using Instance = DynamicSampledImageManager::Instance;
using ReadInstance = DynamicSampledImageManager::ReadInstance;
using ImageType = DynamicSampledImageManager::ImageType;

VkImageType to_vk_image_type(const ImageType type) {
  switch (type) {
    case ImageType::Image2D:
      return VK_IMAGE_TYPE_2D;
    case ImageType::Image3D:
      return VK_IMAGE_TYPE_3D;
    default:
      GROVE_ASSERT(false);
      return VK_IMAGE_TYPE_1D;
  }
}

VkImageViewType to_vk_image_view_type(const ImageType type) {
  switch (type) {
    case ImageType::Image2D:
      return VK_IMAGE_VIEW_TYPE_2D;
    case ImageType::Image3D:
      return VK_IMAGE_VIEW_TYPE_3D;
    default:
      GROVE_ASSERT(false);
      return VK_IMAGE_VIEW_TYPE_1D;
  }
}

VkExtent3D to_vk_extent(const image::Shape& shape) {
  return {
    uint32_t(shape.width),
    uint32_t(shape.height),
    uint32_t(shape.depth)
  };
}

ReadInstance to_read_instance(const Instance& instance, uint32_t frame) {
  const auto& fd = instance.frame_data[frame];
  ReadInstance read{};
  read.image_type = instance.image_type;
  read.sample_in_stages = instance.sample_in_stages;
  read.layout = instance.image_layout;
  read.view = fd.view.contents().handle;
  read.descriptor = instance.descriptor;
  return read;
}

void cmd_image_upload(VkCommandBuffer cmd,
                      const ManagedBuffer* managed_buffer,
                      const ManagedImage* managed_image,
                      PipelineStages sample_stage_flags) {
  const auto image = managed_image->contents().image;
  const auto buffer = managed_buffer->contents().buffer.handle;
  auto copy = make_buffer_image_copy_shader_read_only_dst(
    image,
    buffer,
    make_color_aspect_image_subresource_range(),
    to_vk_pipeline_stages(sample_stage_flags));
  cmd::buffer_image_copy(cmd, &copy);
}

} //  anon

void vk::DynamicSampledImageManager::Instance::set_needs_update() {
  for (auto& fd : frame_data) {
    fd.needs_update = true;
  }
}

void vk::DynamicSampledImageManager::destroy() {
  instances.clear();
}

void vk::DynamicSampledImageManager::set_data(Handle handle, const void* data) {
  if (auto it = instances.find(handle); it != instances.end()) {
    auto& instance = it->second;
    memcpy(instance.cpu_data.get(), data, instance.descriptor.total_size_bytes());
    instance.set_needs_update();
  } else {
    GROVE_ASSERT(false);
  }
}

void vk::DynamicSampledImageManager::set_data_from_contiguous_subset(Handle handle,
                                                                     const void* src_data,
                                                                     const image::Descriptor& src_desc) {
  auto it = instances.find(handle);
  if (it == instances.end()) {
    GROVE_ASSERT(false);
    return;
  }
  auto& instance = it->second;
  const auto& dst_desc = instance.descriptor;
  if (src_desc.channels.num_channels > dst_desc.channels.num_channels ||
      src_desc.num_elements() != dst_desc.num_elements()) {
    GROVE_ASSERT(false);
    return;
  }
  const int num_channels = src_desc.channels.num_channels;
  for (int i = 0; i < num_channels; i++) {
    if (dst_desc.channels[i] != src_desc.channels[i]) {
      GROVE_ASSERT(false);
      return;
    }
  }
  const size_t src_stride = src_desc.element_size_bytes();
  const size_t dst_stride = dst_desc.element_size_bytes();
  unsigned char* dst = instance.cpu_data.get();
  for (int64_t i = 0; i < dst_desc.num_elements(); i++) {
    auto src_ptr = static_cast<const unsigned char*>(src_data) + i * src_stride;
    auto dst_ptr = dst + dst_stride * i;
    memcpy(dst_ptr, src_ptr, src_stride);
  }
  instance.set_needs_update();
}

void DynamicSampledImageManager::modify_data(Handle handle, const ModifyData& modifier) {
  if (auto it = instances.find(handle); it != instances.end()) {
    auto& instance = it->second;
    if (modifier(instance.cpu_data.get(), instance.descriptor)) {
      instance.set_needs_update();
    }
  }
}

Optional<ReadInstance> vk::DynamicSampledImageManager::get(Handle handle) const {
  if (auto it = instances.find(handle); it != instances.end()) {
    return Optional<ReadInstance>(to_read_instance(it->second, current_frame_index));
  } else {
    GROVE_ASSERT(false);
    return NullOpt{};
  }
}

Optional<DynamicSampledImageManager::Instance>
vk::DynamicSampledImageManager::create_instance(const CreateContext& context,
                                                const ImageCreateInfo& info) {
  GROVE_ASSERT(info.sample_in_stages.flags != 0 && info.image_type != ImageType::None);

  const auto& im_desc = info.descriptor;
  const size_t size = im_desc.total_size_bytes();

  VkFormat image_format{};
  if (info.format) {
    image_format = info.format.value();
  } else if (auto format = to_vk_format(im_desc.channels, info.int_conversion)) {
    image_format = format.value();
  } else {
    GROVE_ASSERT(false);
    return NullOpt{};
  }

  Instance instance{};
  instance.image_type = info.image_type;
  instance.descriptor = im_desc;
  instance.cpu_data = std::make_unique<unsigned char[]>(size);
  instance.frame_data.resize(context.frame_queue_depth);
  instance.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; //  @TODO
  instance.sample_in_stages = info.sample_in_stages;
  if (info.data) {
    memcpy(instance.cpu_data.get(), info.data, size);
  }

  for (auto& fd : instance.frame_data) {
    if (auto res = create_staging_buffer(context.allocator, size)) {
      fd.staging_buffer = std::move(res.value);
    } else {
      return NullOpt{};
    }

    if (info.data) {
      fd.needs_update = true;
    }

    const uint32_t array_layers = 1;  //  @TODO
    const uint32_t mip_levels = 1;  //  @TODO
    const VkSampleCountFlagBits num_samples = VK_SAMPLE_COUNT_1_BIT; //  @TODO

    {
      //  @TODO: Need to check whether the image format, usage, and tiling are actually
      //   supported before attempting to create the image.
      //  https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceImageFormatProperties.html
      const VkExtent3D image_extent = to_vk_extent(im_desc.shape);
      const VkImageType image_type = to_vk_image_type(info.image_type);
      const auto create_info = make_image_create_info(
        image_type,
        image_format,
        image_extent,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        array_layers,
        mip_levels,
        num_samples,
        VK_SHARING_MODE_EXCLUSIVE);

      auto im_res = create_device_local_image(context.allocator, &create_info);
      if (!im_res) {
        return NullOpt{};
      } else {
        fd.image = std::move(im_res.value);
      }
    }

    {
      const uint32_t layer = 0;
      const uint32_t num_layers = array_layers;
      const uint32_t mip = 0;
      const uint32_t num_mips = mip_levels;
      const VkImageViewType image_view_type = to_vk_image_view_type(info.image_type);

      auto view_create_info = make_image_view_create_info(
        fd.image.contents().image.handle,
        image_view_type,
        image_format,
        make_identity_component_mapping(),
        make_color_aspect_image_subresource_range(layer, num_layers, mip, num_mips));

      auto view_res = vk::create_image_view(context.core.device.handle, &view_create_info);
      if (!view_res) {
        return NullOpt{};
      } else {
        fd.view = ManagedImageView{std::move(view_res.value), context.core.device.handle};
      }
    }
  }

  return Optional<Instance>(std::move(instance));
}

Optional<Handle> vk::DynamicSampledImageManager::create_sync(const CreateContext& context,
                                                             const ImageCreateInfo& info) {
  auto inst_res = create_instance(context, info);
  if (!inst_res) {
    return NullOpt{};
  }
  auto instance = std::move(inst_res.value());
  auto transfer = [&instance](VkCommandBuffer cmd) {
    for (auto& fd : instance.frame_data) {
      const auto* im = &fd.image;
      const auto* buff = &fd.staging_buffer;
      cmd_image_upload(cmd, buff, im, instance.sample_in_stages);
    }
  };
  auto err = context.uploader->sync_graphics_queue(context.core, std::move(transfer));
  if (err) {
    return NullOpt{};
  }

  Handle handle{next_instance_id++};
  instances[handle] = std::move(instance);
  return Optional<Handle>(handle);
}

Optional<DynamicSampledImageManager::FutureHandle>
DynamicSampledImageManager::create_async(const CreateContext& context, const ImageCreateInfo& info) {
  auto inst_res = create_instance(context, info);
  if (!inst_res) {
    return NullOpt{};
  }
  auto instance = std::move(inst_res.value());
  auto transfer = [&instance](VkCommandBuffer cmd) {
    for (auto& fd : instance.frame_data) {
      const auto* im = &fd.image;
      const auto* buff = &fd.staging_buffer;
      cmd_image_upload(cmd, buff, im, instance.sample_in_stages);
    }
  };
  auto fut_res = context.uploader->async_graphics_queue(context.core, std::move(transfer));
  if (!fut_res) {
    return NullOpt{};
  }

  Handle handle{next_instance_id++};
  instances[handle] = std::move(instance);

  auto result_fut = std::make_shared<Future<Handle>>();
  PendingInstance pend{};
  pend.handle = handle;
  pend.upload_future = std::move(fut_res.value);
  pend.result_future = result_fut;
  pending_instances.push_back(std::move(pend));
  return Optional<FutureHandle>(std::move(result_fut));
}

void DynamicSampledImageManager::begin_frame(const RenderFrameInfo& info) {
  current_frame_index = info.current_frame_index;

  auto pend_it = pending_instances.begin();
  while (pend_it != pending_instances.end()) {
    auto& inst = *pend_it;
    if (inst.upload_future->is_ready()) {
      inst.result_future->data = inst.handle;
      inst.result_future->mark_ready();
      pend_it = pending_instances.erase(pend_it);
    } else {
      ++pend_it;
    }
  }
}

void DynamicSampledImageManager::begin_render(const BeginRenderInfo& info) {
  for (auto& [handle, instance] : instances) {
    auto& fd = instance.frame_data[current_frame_index];
    if (fd.needs_update) {
      fd.staging_buffer.write(instance.cpu_data.get(), instance.descriptor.total_size_bytes());
      cmd_image_upload(info.cmd, &fd.staging_buffer, &fd.image, instance.sample_in_stages);
      fd.needs_update = false;
    }
  }
}

size_t DynamicSampledImageManager::num_instances() const {
  return instances.size();
}

size_t DynamicSampledImageManager::approx_image_memory_usage() const {
  size_t res{};
  for (auto& [_, inst] : instances) {
    for (auto& fd : inst.frame_data) {
      if (fd.image.is_valid()) {
        res += fd.image.get_allocation_size();
      }
    }
  }
  return res;
}

GROVE_NAMESPACE_END
