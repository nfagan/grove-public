#include "simple_descriptor_system.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/scope.hpp"
#include <string>

GROVE_NAMESPACE_BEGIN

namespace {

const VkDescriptorSetLayoutBinding*
find_binding(const ArrayView<const VkDescriptorSetLayoutBinding>& binds, uint32_t binding) {
  for (auto& bind : binds) {
    if (bind.binding == binding) {
      return &bind;
    }
  }
  return nullptr;
}

bool is_compatible(const vk::ShaderResourceDescriptor& desired,
                   const VkDescriptorSetLayoutBinding& pipeline_binding) {
  if (desired.num_elements() != pipeline_binding.descriptorCount) {
    return false;
  }

  const auto desired_type = vk::to_vk_descriptor_type(desired.type);
  return desired_type == pipeline_binding.descriptorType;
}

[[maybe_unused]] std::string
make_unknown_descriptor_error_message(const vk::ShaderResourceDescriptor& desc) {
  std::string message{"Incompatible or non-existent resource of type "};
  message += vk::vk_descriptor_type_name(vk::to_vk_descriptor_type(desc.type));
  message += " at binding " + std::to_string(desc.binding);
  return message;
}

Optional<vk::DescriptorSetLayoutBindings>
reconcile_scaffold_and_pipeline_descriptors(
  const vk::DescriptorSetScaffold& scaffold,
  const ArrayView<const VkDescriptorSetLayoutBinding>& pipeline_bindings) {
  //
  vk::DescriptorSetLayoutBindings dst_bindings;

  for (auto& desc : scaffold.descriptors) {
    const uint32_t scaffold_binding = desc.binding;
    const auto* pipe_bind = find_binding(pipeline_bindings, scaffold_binding);
    if (!pipe_bind || !is_compatible(desc, *pipe_bind)) {
#ifdef GROVE_LOGGING_ENABLED
      auto msg = make_unknown_descriptor_error_message(desc);
      GROVE_LOG_ERROR_CAPTURE_META(msg.c_str(), "SimpleDescriptorSystem");
#endif
      return NullOpt{};
    }

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = desc.binding;
    binding.descriptorCount = desc.num_elements();
    binding.descriptorType = vk::to_vk_descriptor_type(desc.type);
    binding.stageFlags = pipe_bind->stageFlags;
    dst_bindings.push_back(binding);
  }

  std::sort(dst_bindings.begin(), dst_bindings.end(), [](const auto& a, const auto& b) {
    return a.binding < b.binding;
  });

  return Optional<vk::DescriptorSetLayoutBindings>(std::move(dst_bindings));
}

vk::DescriptorPoolAllocator::PoolSizes make_default_pool_sizes(uint32_t* max_num_sets) {
  vk::DescriptorPoolAllocator::PoolSizes result;
  result.push_back(vk::DescriptorPoolAllocator::PoolSize{
    vk::ShaderResourceType::UniformBuffer, 128});
  result.push_back(vk::DescriptorPoolAllocator::PoolSize{
    vk::ShaderResourceType::DynamicUniformBuffer, 128});
  result.push_back(vk::DescriptorPoolAllocator::PoolSize{
    vk::ShaderResourceType::StorageBuffer, 128});
  result.push_back(vk::DescriptorPoolAllocator::PoolSize{
    vk::ShaderResourceType::DynamicStorageBuffer, 128});
  result.push_back(vk::DescriptorPoolAllocator::PoolSize{
    vk::ShaderResourceType::CombinedImageSampler, 128});
  result.push_back(vk::DescriptorPoolAllocator::PoolSize{
    vk::ShaderResourceType::UniformTexelBuffer, 128});
  result.push_back(vk::DescriptorPoolAllocator::PoolSize{
    vk::ShaderResourceType::StorageImage, 128});
  *max_num_sets = 128;
  return result;
}

} //  anon

void vk::SimpleDescriptorSystem::initialize(VkDevice, uint32_t frame_queue_depth) {
  assert(frame_queue_depth > 0);
  frame_contexts.resize(int64_t(frame_queue_depth));

  uint32_t max_num_sets{};
  auto pool_sizes = make_default_pool_sizes(&max_num_sets);
  for (auto& ctx : frame_contexts) {
    ctx.pool_allocator = vk::create_descriptor_pool_allocator(
      pool_sizes.data(), uint32_t(pool_sizes.size()), max_num_sets);
  }
}

void vk::SimpleDescriptorSystem::terminate(VkDevice device) {
  for (auto& ctx : frame_contexts) {
    vk::destroy_descriptor_pool_allocator(&ctx.pool_allocator, device);
  }
  *this = {};
}

void vk::SimpleDescriptorSystem::begin_frame(VkDevice device, uint32_t frame_index) {
  current_frame_index = frame_index;

  auto& ctx = frame_contexts[current_frame_index];
  vk::reset_descriptor_pool_allocator(ctx.pool_allocator, device);
  ctx.cached_descriptor_sets.clear();

  latest_ms_spent_requiring_descriptor_sets = ctx.ms_spent_requiring_descriptor_sets;
  max_ms_spent_requiring_descriptor_sets = std::max(
    max_ms_spent_requiring_descriptor_sets,
    ctx.ms_spent_requiring_descriptor_sets);

  ctx.ms_spent_requiring_descriptor_sets = 0.0f;
}

uint32_t vk::SimpleDescriptorSystem::total_num_descriptor_pools() const {
  uint32_t result{};
  for (auto& context : frame_contexts) {
    result += uint32_t(context.pool_allocator.descriptor_pools.size());
  }
  return result;
}

uint32_t vk::SimpleDescriptorSystem::total_num_descriptor_sets() const {
  uint32_t result{};
  for (auto& context : frame_contexts) {
    for (auto& pool : context.pool_allocator.descriptor_pools) {
      result += pool.set_count;
    }
  }
  return result;
}

Optional<VkDescriptorSet> vk::SimpleDescriptorSystem::require_updated_descriptor_set(
  VkDevice device,
  vk::DescriptorSetLayoutCache* layout_cache,
  const DescriptorSetScaffold& scaffold,
  const ArrayView<const VkDescriptorSetLayoutBinding>& pipeline_bindings,
  bool disable_cache) {
  //
  Stopwatch stopwatch;
  auto& frame_ctx = frame_contexts[current_frame_index];
  GROVE_SCOPE_EXIT {
    frame_ctx.ms_spent_requiring_descriptor_sets += float(stopwatch.delta().count() * 1e3);
  };

  //  Check for consistency between desired layout and reflected bindings
  auto maybe_bindings = reconcile_scaffold_and_pipeline_descriptors(scaffold, pipeline_bindings);
  if (!maybe_bindings) {
    return NullOpt{};
  }

  //  Since the layouts are compatible, reuse a previously allocated & updated descriptor set if
  //  possible.
  if (!disable_cache) {
    for (auto& cached : frame_ctx.cached_descriptor_sets) {
      if (cached.scaffold == scaffold) {
        return Optional<VkDescriptorSet>(cached.set);
      }
    }
  }

  auto& bindings = maybe_bindings.value();
  auto layout_res = vk::require_descriptor_set_layout(*layout_cache, device, bindings);
  if (!layout_res) {
    return NullOpt{};
  }

  auto pool_res = vk::require_pool_for_descriptor_set(frame_ctx.pool_allocator, device, scaffold);
  if (!pool_res) {
    return NullOpt{};
  }

  auto alloc_info = vk::make_descriptor_set_allocate_info(
    pool_res.value.pool_handle, &layout_res.value, 1);

  VkDescriptorSet result{};
  auto alloc_err = vk::allocate_descriptor_sets(device, &alloc_info, &result);
  if (alloc_err) {
    return NullOpt{};
  }

  vk::DescriptorWrites<32> writes{};
  vk::make_descriptor_writes<32>(writes, result, scaffold);
  vk::update_descriptor_sets(device, writes);

  if (!disable_cache) {
    frame_ctx.cached_descriptor_sets.emplace_back();
    auto& cache_entry = frame_ctx.cached_descriptor_sets.back();
    cache_entry.set = result;
    cache_entry.scaffold = scaffold;
  }

  return Optional<VkDescriptorSet>(result);
}

GROVE_NAMESPACE_END
