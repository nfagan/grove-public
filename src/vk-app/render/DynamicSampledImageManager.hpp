#pragma once

#include "../vk/vk.hpp"

namespace grove::vk {

class DynamicSampledImageManager {
public:
  enum class ImageType {
    None = 0,
    Image2D,
    Image3D
  };

  struct Handle {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(Handle, id)
    GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, Handle, id)
    GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
    uint32_t id{};
  };

  struct FrameData {
    ManagedImage image;
    ManagedImageView view;
    ManagedBuffer staging_buffer;
    bool needs_update{};
  };

  struct Instance {
    void set_needs_update();

    ImageType image_type;
    image::Descriptor descriptor;
    PipelineStages sample_in_stages;
    VkImageLayout image_layout;
    std::vector<FrameData> frame_data;
    std::unique_ptr<unsigned char[]> cpu_data;
  };

  struct ReadInstance {
    SampleImageView to_sample_image_view() const {
      return SampleImageView{view, layout};
    }
    bool fragment_shader_sample_ok() const {
      return (PipelineStage::FragmentShader & sample_in_stages.flags) != 0;
    }
    bool vertex_shader_sample_ok() const {
      return (PipelineStage::VertexShader & sample_in_stages.flags) != 0;
    }
    bool is_2d() const {
      return image_type == ImageType::Image2D;
    }
    bool is_3d() const {
      return image_type == ImageType::Image3D;
    }

    VkImageView view;
    VkImageLayout layout;
    PipelineStages sample_in_stages;
    ImageType image_type;
    image::Descriptor descriptor;
  };

  struct CreateContext {
    uint32_t frame_queue_depth;
    const Core& core;
    Allocator* allocator;
    CommandProcessor* uploader;
  };

  struct ImageCreateInfo {
    const void* data;
    image::Descriptor descriptor;
    Optional<VkFormat> format;
    IntConversion int_conversion;
    ImageType image_type;
    PipelineStages sample_in_stages;
  };

  struct BeginRenderInfo {
    const vk::Core& core;
    VkCommandBuffer cmd;
  };

  using FutureHandle = std::shared_ptr<Future<Handle>>;
  using ModifyData = std::function<bool(void*, const image::Descriptor&)>;

public:
  void destroy();
  void begin_frame(const RenderFrameInfo& info);
  void begin_render(const BeginRenderInfo& info);

  Optional<Handle> create_sync(const CreateContext& context, const ImageCreateInfo& info);
  Optional<FutureHandle> create_async(const CreateContext& context, const ImageCreateInfo& info);
  Optional<ReadInstance> get(Handle handle) const;
  void set_data(Handle handle, const void* data);
  void set_data_from_contiguous_subset(Handle handle,
                                       const void* src_data,
                                       const image::Descriptor& src_desc);
  void modify_data(Handle handle, const ModifyData& modifier);
  size_t num_instances() const;
  size_t approx_image_memory_usage() const;

private:
  Optional<Instance> create_instance(const CreateContext& context, const ImageCreateInfo& info);

private:
  struct PendingInstance {
    Handle handle;
    FutureHandle result_future;
    CommandProcessor::CommandFuture upload_future;
  };

  uint32_t current_frame_index{};

  uint32_t next_instance_id{1};
  std::unordered_map<Handle, Instance, Handle::Hash> instances;
  std::vector<PendingInstance> pending_instances;
};

}