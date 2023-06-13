#pragma once

#include "../vk/vk.hpp"

namespace grove::vk {

class SampledImageManager {
public:
  enum class ImageType {
    None = 0,
    Image2D,
    Image2DArray
  };

  struct Handle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(Handle, id)
    GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, Handle, id)
    GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
    uint32_t id{};
  };

  struct Instance {
    image::Descriptor descriptor;
    ManagedImage image;
    ManagedImageView image_view;
    VkImageLayout layout;
    VkFormat format;
    PipelineStages sample_in_stages;
    ImageType image_type;
  };

  struct ReadInstance {
    vk::SampleImageView to_sample_image_view() const {
      return vk::SampleImageView{view, layout};
    }
    bool fragment_shader_sample_ok() const {
      return (PipelineStage::FragmentShader & sample_in_stages.flags) != 0;
    }
    bool is_2d() const {
      return image_type == ImageType::Image2D;
    }
    bool is_2d_array() const {
      return image_type == ImageType::Image2DArray;
    }

    image::Descriptor descriptor;
    VkImageView view;
    VkImageLayout layout;
    VkFormat format;
    PipelineStages sample_in_stages;
    ImageType image_type;
  };

  struct ImageCreateInfo {
    union {
      const void* data;
      const void** mip_levels;
    };
    image::Descriptor descriptor;
    Optional<VkFormat> format;
    IntConversion int_conversion;
    ImageType image_type;
    PipelineStages sample_in_stages;
    uint32_t num_mip_levels;
  };

  struct PendingDelete {
    Instance instance;
    uint64_t frame_id{};
  };

public:
  void initialize(const Core* core, Allocator* allocator, CommandProcessor* cmd);
  void begin_frame(const RenderFrameInfo& info);

  void destroy();
  Optional<Handle> create_sync(const ImageCreateInfo& info);
  bool recreate_sync(Handle handle, const ImageCreateInfo& info);
  bool require_sync(Handle* handle, const ImageCreateInfo& info);

  Optional<ReadInstance> get(Handle handle) const;
  size_t num_instances() const;
  size_t approx_image_memory_usage() const;

private:
  const Core* core{};
  Allocator* allocator{};
  CommandProcessor* command_processor{};

  std::unordered_map<Handle, Instance, Handle::Hash> instances;
  DynamicArray<PendingDelete, 32> pending_deletion;
  uint32_t next_instance_id{1};

  RenderFrameInfo frame_info{};
};

}