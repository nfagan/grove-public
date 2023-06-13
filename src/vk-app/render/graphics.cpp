#include "graphics.hpp"
#include "graphics_context.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

struct StoreRenderPassInfo {
  const vk::PipelineRenderPassInfo* get(gfx::RenderPassHandle handle) const {
    assert(handle.is_valid());
    auto it = infos.find(handle.id);
    return it == infos.end() ? nullptr : &it->second;
  }

  std::unordered_map<uint32_t, vk::PipelineRenderPassInfo> infos;
};

struct BufferImpl {
  vk::ManagedBuffer buffer{};
};

struct PipelineImpl {
  vk::Pipeline pipeline{};
  VkPipelineLayout layout{};
  vk::refl::LayoutBindingsBySet descriptor_set_layout_bindings{};
  vk::BorrowedDescriptorSetLayouts descriptor_set_layouts{};
};

struct DeferDestruction {
  struct Entry {
    uint64_t frame_id{};
    std::shared_ptr<void> pipeline;
    std::shared_ptr<void> buffer;
  };
  std::vector<Entry> pending;
};

} //  anon

namespace gfx {

struct Context {
  vk::GraphicsContext* vk_context{};
  StoreRenderPassInfo render_pass_info;

  std::unordered_set<std::shared_ptr<void>> pipelines;
  std::unordered_set<std::shared_ptr<void>> buffers;
  DeferDestruction defer_destruction;

  uint32_t next_render_pass_handle_id{1};
  RenderPassHandle forward_write_back_pass_handle{next_render_pass_handle_id++};
  RenderPassHandle post_forward_pass_handle{next_render_pass_handle_id++};
  RenderPassHandle shadow_pass_handle{next_render_pass_handle_id++};
  RenderPassHandle post_process_pass_handle{next_render_pass_handle_id++};
};

} //  gfx

namespace {

const vk::ManagedBuffer* underlying_buffer(const gfx::BufferHandle& handle) {
  assert(handle.impl);
  auto* buff = static_cast<const BufferImpl*>(handle.impl.get());
  return &buff->buffer;
}

[[maybe_unused]] bool is_valid_deferred_destruction_entry(const DeferDestruction::Entry& entry) {
  int buff_valid = int(bool(entry.buffer));
  int pipe_valid = int(bool(entry.pipeline));
  return buff_valid + pipe_valid == 1;
}

void destroy_buffer(gfx::Context* context, std::shared_ptr<void>&& impl) {
  DeferDestruction::Entry entry;
  entry.frame_id = context->vk_context->frame_info.current_frame_id;
  entry.buffer = std::move(impl);
  context->defer_destruction.pending.push_back(std::move(entry));
}

void destroy_buffer_impl(BufferImpl&& impl) {
  impl.buffer = {};
}

void destroy_pipeline(gfx::Context* context, std::shared_ptr<void>&& impl) {
  DeferDestruction::Entry entry;
  entry.frame_id = context->vk_context->frame_info.current_frame_id;
  entry.pipeline = std::move(impl);
  context->defer_destruction.pending.push_back(std::move(entry));
}

void destroy_pipeline_impl(PipelineImpl&& impl, VkDevice device) {
  if (impl.pipeline.is_valid()) {
    vk::destroy_pipeline(&impl.pipeline, device);
  }
}

VkBufferUsageFlags to_vk_buffer_usage_flags(gfx::BufferUsage usage) {
  VkBufferUsageFlags result{};
  if (usage.bits & gfx::BufferUsageFlagBits::Uniform) {
    result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }
  if (usage.bits & gfx::BufferUsageFlagBits::Storage) {
    result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  }
  if (usage.bits & gfx::BufferUsageFlagBits::Vertex) {
    result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  if (usage.bits & gfx::BufferUsageFlagBits::Index) {
    result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  if (usage.bits & gfx::BufferUsageFlagBits::Indirect) {
    result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  }
  assert(result != 0);
  return result;
}

void register_render_pass_infos(StoreRenderPassInfo& info, const gfx::Context* context) {
  info.infos[context->forward_write_back_pass_handle.id] =
    vk::make_forward_pass_pipeline_render_pass_info(context->vk_context);
  info.infos[context->post_forward_pass_handle.id] =
    vk::make_post_forward_pass_pipeline_render_pass_info(context->vk_context);
  info.infos[context->shadow_pass_handle.id] =
    vk::make_shadow_pass_pipeline_render_pass_info(context->vk_context);
  info.infos[context->post_process_pass_handle.id] =
    vk::make_post_process_pipeline_render_pass_info(context->vk_context);
}

void delete_pending(gfx::Context* context) {
  //  @NOTE: Can only call this procedure after vk_context has begun its frame.
  auto& pending_destruction = context->defer_destruction.pending;
  const auto& frame_info = context->vk_context->frame_info;

  auto del_it = pending_destruction.begin();
  while (del_it != pending_destruction.end()) {
    if (del_it->frame_id != frame_info.finished_frame_id) {
      GROVE_ASSERT(del_it->frame_id + frame_info.frame_queue_depth > frame_info.current_frame_id);
      ++del_it;
      continue;
    }

    auto& entry = *del_it;
    assert(is_valid_deferred_destruction_entry(entry));

    size_t num_erased{};
    if (entry.buffer) {
      auto* impl = static_cast<BufferImpl*>(entry.buffer.get());
      destroy_buffer_impl(std::move(*impl));
      num_erased = context->buffers.erase(entry.buffer);

    } else if (entry.pipeline) {
      auto* impl = static_cast<PipelineImpl*>(entry.pipeline.get());
      destroy_pipeline_impl(std::move(*impl), context->vk_context->core.device.handle);
      num_erased = context->pipelines.erase(entry.pipeline);

    } else {
      assert(false);
    }

    GROVE_ASSERT(num_erased == 1);
    (void) num_erased;
    del_it = pending_destruction.erase(del_it);
  }
}

Optional<vk::Pipeline> do_create_pipeline(
  gfx::Context* context,
  const std::vector<uint32_t>& vert_bytecode,
  const std::vector<uint32_t>& frag_bytecode,
  VkPipelineLayout pipe_layout,
  const gfx::GraphicsPipelineCreateInfo& info,
  const vk::PipelineRenderPassInfo* pass_info) {

  vk::VertexInputDescriptors input_descs{};
  vk::to_vk_vertex_input_descriptors(
    info.num_vertex_buffer_descriptors, info.vertex_buffer_descriptors, &input_descs);

  vk::DefaultConfigureGraphicsPipelineStateParams params{input_descs};
  params.raster_samples = pass_info->raster_samples;
  params.num_color_attachments = info.num_color_attachments;
  if (info.disable_cull_face) {
    params.cull_mode = VK_CULL_MODE_NONE;
  }

  assert(params.num_color_attachments <= 16);
  for (uint32_t i = 0; i < params.num_color_attachments; i++) {
    params.blend_enabled[i] = info.enable_blend[i];
  }

  vk::GraphicsPipelineStateCreateInfo state{};
  default_configure(&state, params);
  if (info.enable_alpha_to_coverage) {
    state.multisampling.alphaToCoverageEnable = true;
  }

  if (info.depth_compare_op != gfx::DepthCompareOp::Unspecified) {
    switch (info.depth_compare_op) {
      case gfx::DepthCompareOp::LessOrEqual:
        state.depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        break;
      default:
        assert(false);
    }
  }

  if (info.cull_mode != gfx::CullMode::Unspecified) {
    assert(!info.disable_cull_face);
    switch (info.cull_mode) {
      case gfx::CullMode::Back:
        state.rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
        break;
      case gfx::CullMode::Front:
        state.rasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
        break;
      default:
        assert(false);
    }
  }

  if (info.disable_depth_test) {
    state.depth_stencil.depthTestEnable = VK_FALSE;
  }
  if (info.disable_depth_write) {
    state.depth_stencil.depthWriteEnable = VK_FALSE;
  }

  if (info.primitive_topology != gfx::PrimitiveTopology::TriangleList) {
    switch (info.primitive_topology) {
      case gfx::PrimitiveTopology::TriangleStrip: {
        state.input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        break;
      }
      default: {
        assert(false);
      }
    }
  }

  auto pipe_res = create_vert_frag_graphics_pipeline(
    context->vk_context->core.device.handle, vert_bytecode, frag_bytecode, &state,
    pipe_layout, pass_info->render_pass, pass_info->subpass);
  if (!pipe_res) {
    return NullOpt{};
  }

  return Optional<vk::Pipeline>(pipe_res.value);
}

Optional<gfx::PipelineHandle> create_pipeline(
  gfx::Context* context,
  const std::vector<uint32_t>& vert_bytecode,
  const std::vector<uint32_t>& frag_bytecode,
  vk::refl::PushConstantRanges&& push_constant_ranges,
  vk::refl::LayoutBindingsBySet&& descriptor_set_layout_bindings,
  const gfx::GraphicsPipelineCreateInfo& info,
  const vk::PipelineRenderPassInfo* pass_info) {
  //
  VkPipelineLayout pipe_layout{};
  vk::BorrowedDescriptorSetLayouts desc_set_layouts{};
  bool req_res = context->vk_context->pipeline_system.require_layouts(
    context->vk_context->core.device.handle,
    make_view(push_constant_ranges),
    make_view(descriptor_set_layout_bindings),
    &pipe_layout, &desc_set_layouts);
  if (!req_res) {
    return NullOpt{};
  }

  auto pipe_res = do_create_pipeline(
    context, vert_bytecode, frag_bytecode, pipe_layout, info, pass_info);
  if (!pipe_res) {
    return NullOpt{};
  }

  auto impl = std::make_shared<PipelineImpl>();
  impl->pipeline = pipe_res.value();
  impl->layout = pipe_layout;
  impl->descriptor_set_layouts = std::move(desc_set_layouts);
  impl->descriptor_set_layout_bindings = std::move(descriptor_set_layout_bindings);
  context->pipelines.insert(impl);

  gfx::PipelineHandle result{};
  result.impl = impl;
  result.context = context;
  return Optional<gfx::PipelineHandle>(std::move(result));
}

gfx::BufferHandle emplace_buffer(gfx::Context* context, vk::ManagedBuffer&& buff) {
  auto impl = std::make_shared<BufferImpl>();
  impl->buffer = std::move(buff);
  context->buffers.insert(impl);

  gfx::BufferHandle result{};
  result.context = context;
  result.impl = impl;
  return result;
}

struct {
  gfx::Context context{};
} globals;

} //  anon

gfx::BufferHandle::~BufferHandle() {
  assert((context && impl) || (!context && !impl));
  if (context && impl) {
    destroy_buffer(context, std::move(impl));
  }
}

gfx::BufferHandle::BufferHandle(BufferHandle&& other) noexcept :
  context{other.context}, impl{std::move(other.impl)} {
  other.context = nullptr;
}

gfx::BufferHandle& gfx::BufferHandle::operator=(BufferHandle&& other) noexcept {
  gfx::BufferHandle tmp{std::move(other)};
  swap(*this, tmp);
  return *this;
}

bool gfx::BufferHandle::is_valid() const {
  return impl && static_cast<const BufferImpl*>(impl.get())->buffer.is_valid();
}

void gfx::BufferHandle::write(const void* data, size_t size, size_t off) const {
  assert(impl);
  auto* buff_impl = static_cast<const BufferImpl*>(impl.get());
  buff_impl->buffer.write(data, size, off);
}

void gfx::BufferHandle::read(void* into, size_t size, size_t off) const {
  assert(impl);
  auto* buff_impl = static_cast<const BufferImpl*>(impl.get());
  buff_impl->buffer.read(into, size, off);
}

VkBuffer gfx::BufferHandle::get() const {
  auto* managed_buff = underlying_buffer(*this);
  assert(managed_buff->is_valid());
  return managed_buff->contents().buffer.handle;
}

gfx::PipelineHandle::~PipelineHandle() {
  assert((context && impl) || (!context && !impl));
  if (context && impl) {
    destroy_pipeline(context, std::move(impl));
  }
}

gfx::PipelineHandle::PipelineHandle(PipelineHandle&& other) noexcept :
  context{other.context}, impl{std::move(other.impl)} {
  other.context = nullptr;
}

gfx::PipelineHandle& gfx::PipelineHandle::operator=(PipelineHandle&& other) noexcept {
  gfx::PipelineHandle tmp{std::move(other)};
  swap(*this, tmp);
  return *this;
}

bool gfx::PipelineHandle::is_valid() const {
  return impl && static_cast<const PipelineImpl*>(impl.get())->pipeline.is_valid();
}

VkPipeline gfx::PipelineHandle::get() const {
  assert(impl);
  auto* pipe_impl = static_cast<const PipelineImpl*>(impl.get());
  assert(pipe_impl->pipeline.is_valid());
  return pipe_impl->pipeline.handle;
}

VkPipelineLayout gfx::PipelineHandle::get_layout() const {
  assert(impl);
  auto* pipe_impl = static_cast<const PipelineImpl*>(impl.get());
  assert(pipe_impl->pipeline.is_valid() && pipe_impl->layout != VK_NULL_HANDLE);
  return pipe_impl->layout;
}

Optional<ArrayView<const VkDescriptorSetLayoutBinding>>
gfx::PipelineHandle::get_descriptor_set_layout_bindings(uint32_t set) const {
  assert(impl);
  auto* pipe_impl = static_cast<const PipelineImpl*>(impl.get());
  assert(pipe_impl->pipeline.is_valid());

  if (set >= uint32_t(pipe_impl->descriptor_set_layout_bindings.size())) {
    return NullOpt{};
  }

  auto& bindings = pipe_impl->descriptor_set_layout_bindings[set];
  return Optional<ArrayView<const VkDescriptorSetLayoutBinding>>(make_view(bindings));
}

Optional<gfx::BufferHandle> gfx::create_host_visible_vertex_buffer(Context* context, size_t size) {
  auto use = BufferUsage{BufferUsageFlagBits::Vertex};
  auto mem = MemoryType{MemoryTypeFlagBits::HostVisible};
  return gfx::create_buffer(context, use, mem, size);
}

Optional<gfx::BufferHandle> gfx::create_host_visible_index_buffer(Context* context, size_t size) {
  auto use = BufferUsage{BufferUsageFlagBits::Index};
  auto mem = MemoryType{MemoryTypeFlagBits::HostVisible};
  return gfx::create_buffer(context, use, mem, size);
}

Optional<gfx::BufferHandle> gfx::create_storage_buffer(Context* context, size_t size) {
  auto use = BufferUsage{BufferUsageFlagBits::Storage};
  auto mem = MemoryType{MemoryTypeFlagBits::HostVisible};
  return gfx::create_buffer(context, use, mem, size);
}

Optional<gfx::BufferHandle> gfx::create_device_local_storage_buffer(Context* context, size_t size) {
  auto use = BufferUsage{BufferUsageFlagBits::Storage};
  auto mem = MemoryType{MemoryTypeFlagBits::DeviceLocal};
  return gfx::create_buffer(context, use, mem, size);
}

Optional<gfx::BufferHandle> gfx::create_uniform_buffer(Context* context, size_t size) {
  auto use = BufferUsage{BufferUsageFlagBits::Uniform};
  auto mem = MemoryType{MemoryTypeFlagBits::HostVisible};
  return gfx::create_buffer(context, use, mem, size);
}

Optional<gfx::BufferHandle> gfx::create_device_local_vertex_buffer_sync(
  Context* context, size_t size, const void* data) {

  auto buff_res = vk::create_device_local_vertex_buffer_sync(
    &context->vk_context->allocator, size, data,
    &context->vk_context->core, &context->vk_context->command_processor);

  if (!buff_res) {
    return NullOpt{};
  } else {
    return Optional<gfx::BufferHandle>(emplace_buffer(context, std::move(buff_res.value)));
  }
}

Optional<gfx::BufferHandle> gfx::create_device_local_index_buffer_sync(
  Context* context, size_t size, const void* data) {

  auto buff_res = vk::create_device_local_index_buffer_sync(
    &context->vk_context->allocator, size, data,
    &context->vk_context->core, &context->vk_context->command_processor);

  if (!buff_res) {
    return NullOpt{};
  } else {
    return Optional<gfx::BufferHandle>(emplace_buffer(context, std::move(buff_res.value)));
  }
}

Optional<gfx::DynamicUniformBuffer>
gfx::create_dynamic_uniform_buffer(Context* context, size_t element_size, size_t num_elements) {
  size_t actual_element_stride{};
  size_t actual_buffer_size{};

  const auto& phys_dev = context->vk_context->core.physical_device;
  auto buff_res = vk::create_dynamic_uniform_buffer(
    &context->vk_context->allocator,
    phys_dev.info.properties.limits.minUniformBufferOffsetAlignment,
    element_size, num_elements, &actual_element_stride, &actual_buffer_size);

  if (!buff_res) {
    return NullOpt{};
  }

  gfx::DynamicUniformBuffer result;
  result.buffer = emplace_buffer(context, std::move(buff_res.value));
  result.element_stride = actual_element_stride;
  return Optional<gfx::DynamicUniformBuffer>(std::move(result));
}

Optional<gfx::BufferHandle> gfx::create_buffer(
  Context* context, BufferUsage usage, MemoryType mem_type, size_t size) {

  const auto use = to_vk_buffer_usage_flags(usage);
  auto* alloc = &context->vk_context->allocator;

  vk::Result<vk::ManagedBuffer> buff_res;
  if (mem_type.bits & MemoryTypeFlagBits::DeviceLocal) {
    assert((mem_type.bits & MemoryTypeFlagBits::HostCoherent) == 0 &&
           (mem_type.bits & MemoryTypeFlagBits::HostVisible) == 0);
    buff_res = vk::create_device_local_buffer(alloc, size, use);
  } else {
    assert(mem_type.bits & MemoryTypeFlagBits::HostVisible);
    if (mem_type.bits & MemoryTypeFlagBits::HostCoherent) {
      buff_res = vk::create_host_visible_host_coherent_buffer(alloc, size, use);
    } else {
      buff_res = vk::create_host_visible_buffer(alloc, size, use);
    }
  }

  if (!buff_res) {
    return NullOpt{};
  }

  return Optional<gfx::BufferHandle>(emplace_buffer(context, std::move(buff_res.value)));
}

Optional<gfx::PipelineHandle>
gfx::create_pipeline(
  Context* context, glsl::VertFragProgramSource&& source,
  const GraphicsPipelineCreateInfo& info, RenderPassHandle in_pass) {
  //
  const auto* pass_info = context->render_pass_info.get(in_pass);
  assert(pass_info);
  return grove::create_pipeline(
    context, source.vert_bytecode, source.frag_bytecode,
    std::move(source.push_constant_ranges),
    std::move(source.descriptor_set_layout_bindings), info, pass_info);
}

Optional<gfx::PipelineHandle> gfx::create_pipeline(
  Context* context,
  const uint32_t* vert_spv, size_t vert_spv_size,
  const uint32_t* frag_spv, size_t frag_spv_size, const GraphicsPipelineCreateInfo& info,
  RenderPassHandle in_pass) {
  //
  std::vector<uint32_t> vert_bytecode{vert_spv, vert_spv + vert_spv_size};
  std::vector<uint32_t> frag_bytecode{frag_spv, frag_spv + frag_spv_size};
  auto refl = glsl::reflect_vert_frag_spv(vert_bytecode, frag_bytecode);
  if (!refl) {
    return NullOpt{};
  }

  const auto* pass_info = context->render_pass_info.get(in_pass);
  assert(pass_info);
  return grove::create_pipeline(
    context, vert_bytecode, frag_bytecode,
    std::move(refl.value().push_constant_ranges),
    std::move(refl.value().descriptor_set_layout_bindings), info, pass_info);
}

Optional<gfx::PipelineHandle> gfx::create_compute_pipeline(
  Context* context, glsl::ComputeProgramSource&& source) {
  //
  VkPipelineLayout pipe_layout{};
  vk::BorrowedDescriptorSetLayouts desc_set_layouts{};
  bool req_res = context->vk_context->pipeline_system.require_layouts(
    context->vk_context->core.device.handle,
    make_view(source.push_constant_ranges),
    make_view(source.descriptor_set_layout_bindings), &pipe_layout, &desc_set_layouts);
  if (!req_res) {
    return NullOpt{};
  }

  auto pipe_res = vk::create_compute_pipeline(
    context->vk_context->core.device.handle, source.bytecode, pipe_layout);
  if (!pipe_res) {
    return NullOpt{};
  }

  auto impl = std::make_shared<PipelineImpl>();
  impl->pipeline = pipe_res.value;
  impl->layout = pipe_layout;
  impl->descriptor_set_layouts = std::move(desc_set_layouts);
  impl->descriptor_set_layout_bindings = std::move(source.descriptor_set_layout_bindings);
  context->pipelines.insert(impl);

  gfx::PipelineHandle result{};
  result.impl = impl;
  result.context = context;
  return Optional<gfx::PipelineHandle>(std::move(result));
}

Optional<VkDescriptorSet> gfx::require_updated_descriptor_set(
  Context* context, const vk::DescriptorSetScaffold& scaffold, const gfx::PipelineHandle& pipeline,
  bool disable_cache) {
  //
  auto pipe_binds = pipeline.get_descriptor_set_layout_bindings(scaffold.set);
  if (!pipe_binds) {
    return NullOpt{};
  }

  auto& vk_context = *context->vk_context;
  auto& layout_cache = vk_context.pipeline_system.get_default_descriptor_set_layout_cache();
  VkDevice device = vk_context.core.device.handle;
  return vk_context.simple_descriptor_system.require_updated_descriptor_set(
    device, &layout_cache, scaffold, pipe_binds.value(), disable_cache);
}

Optional<gfx::RenderPassHandle>
gfx::get_forward_write_back_render_pass_handle(const Context* context) {
  if (context->forward_write_back_pass_handle.is_valid()) {
    return Optional<gfx::RenderPassHandle>(context->forward_write_back_pass_handle);
  } else {
    return NullOpt{};
  }
}

Optional<gfx::RenderPassHandle>
gfx::get_post_forward_render_pass_handle(const Context* context) {
  if (context->post_forward_pass_handle.is_valid()) {
    return Optional<gfx::RenderPassHandle>(context->post_forward_pass_handle);
  } else {
    return NullOpt{};
  }
}

Optional<gfx::RenderPassHandle>
gfx::get_shadow_render_pass_handle(const Context* context) {
  if (context->shadow_pass_handle.is_valid()) {
    return Optional<gfx::RenderPassHandle>(context->shadow_pass_handle);
  } else {
    return NullOpt{};
  }
}

Optional<gfx::RenderPassHandle> gfx::get_post_process_pass_handle(const Context* context) {
  if (context->post_process_pass_handle.is_valid()) {
    return Optional<gfx::RenderPassHandle>(context->post_process_pass_handle);
  } else {
    return NullOpt{};
  }
}

VkSampler gfx::get_image_sampler_linear_repeat(const Context* context) {
  auto& vk_context = *context->vk_context;
  return vk_context.sampler_system.require_linear_repeat(vk_context.core.device.handle);
}

VkSampler gfx::get_image_sampler_linear_edge_clamp(const Context* context) {
  auto& vk_context = *context->vk_context;
  return vk_context.sampler_system.require_linear_edge_clamp(vk_context.core.device.handle);
}

VkSampler gfx::get_image_sampler_nearest_edge_clamp(const Context* context) {
  auto& vk_context = *context->vk_context;
  return vk_context.sampler_system.require_nearest_edge_clamp(vk_context.core.device.handle);
}

uint32_t gfx::get_frame_queue_depth(const Context* context) {
  assert(context->vk_context->frame_info.frame_queue_depth == context->vk_context->frame_queue_depth);
  return context->vk_context->frame_info.frame_queue_depth;
}

void gfx::begin_frame(gfx::Context* context) {
  register_render_pass_infos(context->render_pass_info, context);
  delete_pending(context);
}

gfx::Context* gfx::init_context(vk::GraphicsContext* vk_context) {
  auto& res = globals.context;
  res.vk_context = vk_context;
  register_render_pass_infos(res.render_pass_info, &res);
  return &res;
}

void gfx::terminate_context(gfx::Context* context) {
  for (auto& pipe : context->pipelines) {
    auto* impl = static_cast<PipelineImpl*>(pipe.get());
    destroy_pipeline_impl(std::move(*impl), context->vk_context->core.device.handle);
  }

  for (auto& buff : context->buffers) {
    auto* impl = static_cast<BufferImpl*>(buff.get());
    destroy_buffer_impl(std::move(*impl));
  }

  *context = {};
}

vk::Allocator* gfx::get_vk_allocator(Context* context) {
  return &context->vk_context->allocator;
}

gfx::ContextStats gfx::get_stats(const Context* context) {
  gfx::ContextStats result{};
  for (auto& buff : context->buffers) {
    auto* buff_impl = static_cast<const BufferImpl*>(buff.get());
    if (buff_impl->buffer.is_valid()) {
      result.buffer_mb += buff_impl->buffer.get_allocation_size();
      result.num_buffers++;
    }
  }
  result.num_pipelines = uint32_t(context->pipelines.size());
  return result;
}

GROVE_NAMESPACE_END
