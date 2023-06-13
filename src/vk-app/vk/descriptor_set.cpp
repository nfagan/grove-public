#include "descriptor_set.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/ArrayView.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

[[maybe_unused]] constexpr const char* logging_id() {
  return "vk/descriptor_set";
}

vk::BufferViewResource make_buffer_view_resource(VkBufferView view) {
  vk::BufferViewResource result{};
  result.view = view;
  return result;
}

vk::BufferResource make_buffer_resource(VkBuffer buffer, size_t range, size_t offset) {
  vk::BufferResource resource{};
  resource.buffer = buffer;
  resource.range = range;
  resource.offset = offset;
  return resource;
}

vk::CombinedImageSamplerResource make_combined_image_sampler_resource(VkImageView view,
                                                                      VkSampler sampler,
                                                                      VkImageLayout layout) {
  vk::CombinedImageSamplerResource resource{};
  resource.view = view;
  resource.sampler = sampler;
  resource.layout = layout;
  return resource;
}

vk::StorageImageResource make_storage_image_resource(VkImageView view, VkImageLayout layout) {
  vk::StorageImageResource result{};
  result.view = view;
  result.layout = layout;
  return result;
}

using AllocatedPool = vk::DescriptorPoolAllocator::AllocatedPool;
using PoolSize = vk::DescriptorPoolAllocator::PoolSize;
using Scaffold = vk::DescriptorSetScaffold;

bool pool_has_types(const PoolSize* pools, uint32_t num_pools, const Scaffold& scaffold) {
  for (auto& descr : scaffold.descriptors) {
    bool has_type{};
    for (uint32_t i = 0; i < num_pools; i++) {
      if (pools[i].type == descr.type) {
        has_type = true;
        break;
      }
    }
    if (!has_type) {
      return false;
    }
  }
  return true;
}

bool pool_has_types(const vk::DescriptorPoolAllocator& allocator, const Scaffold& scaffold) {
  return pool_has_types(
    allocator.pool_capacities.data(),
    uint32_t(allocator.pool_capacities.size()),
    scaffold);
}

constexpr const char* missing_type_err_message() {
  return "Cannot allocate a descriptor pool with a resource type not specified during the allocator's construction.";
}

vk::Error allocate_existing_descriptor_pool(vk::DescriptorPoolAllocator& allocator,
                                            const vk::DescriptorSetScaffold& scaffold,
                                            Optional<AllocatedPool>* allocated_pool) {
  if (!pool_has_types(allocator, scaffold)) {
    GROVE_LOG_ERROR_CAPTURE_META(missing_type_err_message(), logging_id());
    return {VK_ERROR_UNKNOWN, missing_type_err_message()};
  }

  const uint32_t reserve_num_candidates_stack = 32;
  uint32_t stack_candidate_pool_sizes[reserve_num_candidates_stack];
  std::unique_ptr<uint32_t[]> heap_candidate_pool_sizes;

  uint32_t* candidate_pool_sizes = stack_candidate_pool_sizes;
  const auto num_types = uint32_t(allocator.pool_capacities.size());
  if (num_types > reserve_num_candidates_stack) {
    heap_candidate_pool_sizes = std::make_unique<uint32_t[]>(num_types);
    candidate_pool_sizes = heap_candidate_pool_sizes.get();
  }

  uint32_t free_pool_ind{uint32_t(allocator.free_pools.size())};
  while (free_pool_ind > 0) {
    const uint32_t pool_ind = allocator.free_pools[--free_pool_ind];
    auto& pool = allocator.descriptor_pools[pool_ind];
    GROVE_ASSERT(uint32_t(pool.descriptor_counts.size()) == num_types);
    bool ok_pool{true};
    for (uint32_t i = 0; i < num_types; i++) {
      const auto& counts = pool.descriptor_counts[i];
      const auto& caps = allocator.pool_capacities[i];
      GROVE_ASSERT(caps.type == counts.type);
      const uint32_t scaffold_size = scaffold.num_descriptors_of_type(counts.type);
      const uint32_t desired_count = counts.count + scaffold_size;
      if (desired_count > caps.count) {
        ok_pool = false;
        break;
      } else {
        candidate_pool_sizes[i] = desired_count;
      }
    }
    if (ok_pool) {
      bool filled_pool = ++pool.set_count == allocator.max_num_sets_per_pool;
      for (uint32_t i = 0; i < num_types; i++) {
        auto& pool_size = pool.descriptor_counts[i];
        const uint32_t pool_size_cap = allocator.pool_capacities[i].count;
        pool_size.count = candidate_pool_sizes[i];
        if (pool_size.count == pool_size_cap) {
          filled_pool = true;
        }
      }
      if (filled_pool) {
        allocator.free_pools.erase(allocator.free_pools.begin() + free_pool_ind);
      }

      AllocatedPool result{};
      result.index = pool_ind;
      result.pool_handle = pool.pool.handle;
      *allocated_pool = result;
      return {};
    }
  }

  *allocated_pool = NullOpt{};
  return {};
}

[[nodiscard]] vk::Error add_descriptor_pool(vk::DescriptorPoolAllocator& allocator, VkDevice device) {
  DynamicArray<VkDescriptorPoolSize, 16> pool_sizes;
  for (auto& res : allocator.pool_capacities) {
    VkDescriptorPoolSize vk_pool_size{};
    vk_pool_size.type = to_vk_descriptor_type(res.type);
    vk_pool_size.descriptorCount = res.count;
    pool_sizes.push_back(vk_pool_size);
  }

  auto create_info = vk::make_empty_descriptor_pool_create_info();
  create_info.pPoolSizes = pool_sizes.data();
  create_info.poolSizeCount = uint32_t(pool_sizes.size());
  create_info.maxSets = allocator.max_num_sets_per_pool;
  auto pool_res = vk::create_descriptor_pool(device, &create_info);
  if (!pool_res) {
    return {pool_res.status, pool_res.message};
  }

  vk::DescriptorPoolAllocator::Pool new_pool{};
  new_pool.pool = pool_res.value;
  new_pool.set_count = 0;
  for (auto& cap : allocator.pool_capacities) {
    auto sz = cap;
    sz.count = 0;
    new_pool.descriptor_counts.push_back(sz);
  }

  const auto pool_ind = uint32_t(allocator.descriptor_pools.size());
  allocator.free_pools.push_back(pool_ind);
  allocator.descriptor_pools.push_back(new_pool);
  return {};
}

[[maybe_unused]] bool supports_individual_descriptor_set_release(VkDescriptorPoolCreateFlags flags) {
  return flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
}

} //  anon

vk::ShaderResourceDescriptor
vk::make_buffer_resource_descriptor(ShaderResourceType type,
                                    uint32_t binding,
                                    VkBuffer buffer,
                                    size_t range,
                                    size_t offset) {
  ShaderResourceDescriptor descr{};
  descr.type = type;
  descr.buffer = make_buffer_resource(buffer, range, offset);
  descr.binding = binding;
  return descr;
}

vk::ShaderResourceDescriptor
vk::make_combined_image_sampler_resource_descriptor(uint32_t binding,
                                                    VkImageView view,
                                                    VkSampler sampler,
                                                    VkImageLayout layout) {
  ShaderResourceDescriptor descr{};
  descr.type = ShaderResourceType::CombinedImageSampler;
  descr.combined_image_sampler = make_combined_image_sampler_resource(view, sampler, layout);
  descr.binding = binding;
  return descr;
}

vk::ShaderResourceDescriptor
vk::make_buffer_view_resource_descriptor(ShaderResourceType type, uint32_t binding,
                                         VkBufferView view) {
  assert(type == ShaderResourceType::UniformTexelBuffer);
  vk::ShaderResourceDescriptor desc{};
  desc.type = type;
  desc.buffer_view = make_buffer_view_resource(view);
  desc.binding = binding;
  return desc;
}

void vk::push_combined_image_sampler(DescriptorSetScaffold& scaffold,
                                     uint32_t binding, VkImageView view,
                                     VkSampler sampler, VkImageLayout layout) {
  auto desc = make_combined_image_sampler_resource_descriptor(binding, view, sampler, layout);
  scaffold.descriptors.push_back(desc);
}

void vk::push_combined_image_sampler(DescriptorSetScaffold& scaffold,
                                     uint32_t binding,
                                     const vk::SampleImageView& view,
                                     VkSampler sampler) {
  push_combined_image_sampler(scaffold, binding, view.view, sampler, view.layout);
}

void vk::push_uniform_texel_buffer(DescriptorSetScaffold& scaffold, uint32_t binding,
                                   VkBufferView view) {
  auto desc = make_buffer_view_resource_descriptor(
    ShaderResourceType::UniformTexelBuffer, binding, view);
  scaffold.descriptors.push_back(desc);
}

void vk::push_storage_image(DescriptorSetScaffold& scaffold, uint32_t binding, VkImageView view,
                            VkImageLayout layout) {
  vk::ShaderResourceDescriptor desc{};
  desc.type = ShaderResourceType::StorageImage;
  desc.binding = binding;
  desc.storage_image = make_storage_image_resource(view, layout);
  scaffold.descriptors.push_back(desc);
}

void vk::push_buffer(DescriptorSetScaffold& scaffold, ShaderResourceType type,
                     uint32_t binding, VkBuffer buffer, size_t range, size_t offset) {
  auto descr = make_buffer_resource_descriptor(type, binding, buffer, range, offset);
  scaffold.descriptors.push_back(descr);
}

void vk::push_buffer(DescriptorSetScaffold& scaffold, ShaderResourceType type,
                     uint32_t binding, const vk::ManagedBuffer& buffer) {
  auto contents = buffer.contents();
  push_buffer(scaffold, type, binding, contents.buffer.handle, contents.size);
}

void vk::push_buffer(DescriptorSetScaffold& scaffold, ShaderResourceType type,
                     uint32_t binding, const vk::ManagedBuffer& buffer,
                     size_t range, size_t offset) {
  auto contents = buffer.contents();
  push_buffer(scaffold, type, binding, contents.buffer.handle, range, offset);
}

void vk::push_descriptor_write(VkDescriptorSet set,
                               const ShaderResourceDescriptor& descr,
                               VkWriteDescriptorSet*& write_to,
                               VkDescriptorBufferInfo*& buffer_info,
                               VkDescriptorImageInfo*& image_info) {
  switch (descr.type) {
    case ShaderResourceType::UniformBuffer:
    case ShaderResourceType::DynamicUniformBuffer:
    case ShaderResourceType::StorageBuffer:
    case ShaderResourceType::DynamicStorageBuffer: {
      GROVE_ASSERT(!descr.is_array());
      auto* dst_info = buffer_info++;
      *dst_info = to_vk_descriptor_buffer_info(descr.buffer);
      auto write = vk::make_empty_write_descriptor_set();
      write.descriptorCount = 1;
      write.descriptorType = to_vk_descriptor_type(descr.type);
      write.dstBinding = descr.binding;
      write.dstSet = set;
      write.pBufferInfo = dst_info;
      auto* dst_write = write_to++;
      *dst_write = write;
      break;
    }
    case ShaderResourceType::UniformTexelBuffer: {
      GROVE_ASSERT(!descr.is_array());
      auto write = vk::make_empty_write_descriptor_set();
      write.descriptorCount = 1;
      write.descriptorType = to_vk_descriptor_type(descr.type);
      write.dstBinding = descr.binding;
      write.dstSet = set;
      write.pTexelBufferView = &descr.buffer_view.view;
      auto* dst_write = write_to++;
      *dst_write = write;
      break;
    }
    case ShaderResourceType::CombinedImageSampler: {
      if (descr.is_array()) {
        const uint32_t array_range = descr.array_range;
        auto* dst_info = image_info;
        image_info += array_range;
        for (uint32_t i = 0; i < array_range; i++) {
          dst_info[i] = to_vk_descriptor_image_info(descr.combined_image_sampler_array[i]);
        }
        auto write = vk::make_empty_write_descriptor_set();
        write.descriptorCount = array_range;
        write.descriptorType = to_vk_descriptor_type(descr.type);
        write.dstArrayElement = descr.array_element;
        write.dstBinding = descr.binding;
        write.dstSet = set;
        write.pImageInfo = dst_info;
        auto* dst_write = write_to++;
        *dst_write = write;
      } else {
        auto* dst_info = image_info++;
        *dst_info = to_vk_descriptor_image_info(descr.combined_image_sampler);
        auto write = vk::make_empty_write_descriptor_set();
        write.descriptorCount = 1;
        write.descriptorType = to_vk_descriptor_type(descr.type);
        write.dstBinding = descr.binding;
        write.dstSet = set;
        write.pImageInfo = dst_info;
        auto* dst_write = write_to++;
        *dst_write = write;
      }
      break;
    }
    case ShaderResourceType::StorageImage: {
      auto* dst_info = image_info++;
      *dst_info = to_vk_descriptor_image_info(descr.storage_image);
      auto write = vk::make_empty_write_descriptor_set();
      write.descriptorCount = 1;
      write.descriptorType = to_vk_descriptor_type(descr.type);
      write.dstBinding = descr.binding;
      write.dstSet = set;
      write.pImageInfo = dst_info;
      auto* dst_write = write_to++;
      *dst_write = write;
      break;
    }
    default:
      assert(false);
  }
}

void vk::push_pool_sizes_from_layout_bindings(DescriptorPoolAllocator::PoolSizes& sizes,
                                              const ArrayView<const DescriptorSetLayoutBindings>& layout_bindings,
                                              const std::function<uint32_t(ShaderResourceType)>& get_size) {
  push_pool_sizes_from_layout_bindings(
    sizes, layout_bindings.begin(), uint32_t(layout_bindings.size()), get_size);
}

void vk::push_pool_sizes_from_layout_bindings(vk::DescriptorPoolAllocator::PoolSizes& result,
                                              const DescriptorSetLayoutBindings* layout_bindings,
                                              uint32_t num_layouts,
                                              const std::function<uint32_t(ShaderResourceType)>& get_size) {
  for (uint32_t i = 0; i < num_layouts; i++) {
    auto& bindings = layout_bindings[i];
    for (auto& element : bindings) {
      auto vk_type = element.descriptorType;
      auto grove_type = to_shader_resource_type(vk_type);
      const uint32_t count = get_size(grove_type);
      auto it = std::find_if(result.begin(), result.end(), [grove_type](const PoolSize& sz) {
        return sz.type == grove_type;
      });
      if (it == result.end()) {
        result.push_back({grove_type, count});
      } else {
        it->count = std::max(it->count, count);
      }
    }
  }
}

vk::DescriptorPoolAllocator
vk::create_descriptor_pool_allocator(const DescriptorPoolAllocator::PoolSize* pool_sizes,
                                     size_t num_pool_sizes,
                                     uint32_t max_num_sets,
                                     VkDescriptorPoolCreateFlags pool_create_flags) {
  vk::DescriptorPoolAllocator result{};
  result.max_num_sets_per_pool = max_num_sets;
  result.pool_create_flags = pool_create_flags;
  for (size_t i = 0; i < num_pool_sizes; i++) {
    result.pool_capacities.push_back(pool_sizes[i]);
  }
  return result;
}

vk::DescriptorPoolAllocator
vk::create_descriptor_pool_allocator(const ArrayView<const DescriptorPoolAllocator::PoolSize>& pool_sizes,
                                     uint32_t max_num_sets,
                                     VkDescriptorPoolCreateFlags flags) {
  return create_descriptor_pool_allocator(
    pool_sizes.begin(), uint32_t(pool_sizes.size()), max_num_sets, flags);
}

void vk::destroy_descriptor_pool_allocator(DescriptorPoolAllocator* allocator, VkDevice device) {
  for (auto& pool : allocator->descriptor_pools) {
    vk::destroy_descriptor_pool(&pool.pool, device);
  }
  allocator->descriptor_pools.clear();
  allocator->free_pools.clear();
  allocator->pool_capacities.clear();
  allocator->max_num_sets_per_pool = 0;
}

void vk::release_and_free_descriptor_set(DescriptorPoolAllocator& allocator,
                                         VkDevice device,
                                         VkDescriptorSet set_to_free,
                                         const DescriptorPoolAllocator::AllocatedPool& pool,
                                         const DescriptorSetScaffold& scaffold) {
  GROVE_ASSERT(pool.index < allocator.descriptor_pools.size());
  auto& dst_pool = allocator.descriptor_pools[pool.index];
  GROVE_ASSERT(dst_pool.set_count > 0);
  dst_pool.set_count--;
  bool empty_pool = dst_pool.set_count == 0;
  for (auto& cts : dst_pool.descriptor_counts) {
    const uint32_t num_in_set = scaffold.num_descriptors_of_type(cts.type);
    GROVE_ASSERT(cts.count >= num_in_set);
    cts.count = cts.count - num_in_set;
    if (cts.count > 0) {
      GROVE_ASSERT(!empty_pool);
      empty_pool = false;
    }
  }
  auto free_it = std::find(allocator.free_pools.begin(), allocator.free_pools.end(), pool.index);
  if (free_it == allocator.free_pools.end()) {
    //  This pool is not free, see if we can free it.
    bool can_return_to_free_list = empty_pool;
    if (empty_pool) {
      vk::reset_descriptor_pool(device, pool.pool_handle);
    } else if (set_to_free) {
      GROVE_ASSERT(supports_individual_descriptor_set_release(allocator.pool_create_flags));
      vkFreeDescriptorSets(device, pool.pool_handle, 1, &set_to_free);
      can_return_to_free_list = true;
    }
    if (can_return_to_free_list) {
      allocator.free_pools.push_back(pool.index);
    }
  }
}

void vk::release_descriptor_set(DescriptorPoolAllocator& allocator,
                                VkDevice device,
                                const DescriptorPoolAllocator::AllocatedPool& pool,
                                const DescriptorSetScaffold& scaffold) {
  release_and_free_descriptor_set(allocator, device, nullptr, pool, scaffold);
}

void vk::reset_descriptor_pool_allocator(DescriptorPoolAllocator& allocator, VkDevice device) {
  for (auto& pool : allocator.descriptor_pools) {
    if (pool.set_count > 0) {
      vk::reset_descriptor_pool(device, pool.pool.handle);
    }

    pool.set_count = 0;
    for (auto& ct : pool.descriptor_counts) {
      ct.count = 0;
    }
  }

  allocator.free_pools.clear();
  for (uint32_t i = 0; i < uint32_t(allocator.descriptor_pools.size()); i++) {
    allocator.free_pools.push_back(i);
  }
}

vk::Result<vk::DescriptorPoolAllocator::AllocatedPool>
vk::require_pool_for_descriptor_set(DescriptorPoolAllocator& allocator,
                                    VkDevice device,
                                    const DescriptorSetScaffold& scaffold) {
  {
    //  Look for an existing pool that meets the scaffold requirements.
    Optional<AllocatedPool> result;
    auto err = allocate_existing_descriptor_pool(allocator, scaffold, &result);
    if (err) {
      return error_cast<AllocatedPool>(err);
    } else if (result) {
      return result.value();
    }
  }

  if (auto err = add_descriptor_pool(allocator, device)) {
    return error_cast<AllocatedPool>(err);
  }

  {
    //  Retry from the added pool.
    Optional<AllocatedPool> result;
    auto err = allocate_existing_descriptor_pool(allocator, scaffold, &result);
    if (err) {
      return error_cast<AllocatedPool>(err);
    } else if (result) {
      return result.value();
    } else {
      return {VK_ERROR_UNKNOWN, "Failed to allocate descriptor pool."};
    }
  }
}

void vk::DescriptorSetAllocator::begin_frame() {
  GROVE_ASSERT(sets.size() >= free_sets.size() &&
               sets.size() >= scaffolds_to_sets.size() &&
               scaffolds_to_sets.size() + free_sets.size() == sets.size());
  for (auto& set : sets) {
    set.frames_untouched++;
  }
}

void vk::DescriptorSetAllocator::end_frame(DescriptorPoolAllocator&, VkDevice) {
  //  @TODO: If too many free sets, release the free ones.
#if 0
  uint32_t set_ind{};
  for (auto& set : sets) {
    if (!set.is_free && set.frames_untouched >= num_frames_untouched_before_release) {
      GROVE_ASSERT(std::find(free_sets.begin(), free_sets.end(), set_ind) == free_sets.end());
      free_sets.push_back(set_ind);
      set.is_free = true;
      set.frames_untouched = 0;
    }
    set_ind++;
  }
#else
  auto set_it = scaffolds_to_sets.begin();
  while (set_it != scaffolds_to_sets.end()) {
    uint32_t set_ind = set_it->second;
    auto& set = sets[set_ind];
    if (!set.is_free && set.frames_untouched >= num_frames_untouched_before_release) {
      GROVE_ASSERT(std::find(free_sets.begin(), free_sets.end(), set_ind) == free_sets.end());
      free_sets.push_back(set_ind);
      set.is_free = true;
      set.frames_untouched = 0;
      set_it = scaffolds_to_sets.erase(set_it);
    } else {
      ++set_it;
    }
  }
#endif
}

void vk::DescriptorSetAllocator::release(DescriptorPoolAllocator& pool_allocator, VkDevice device) {
  for (auto& [scaffold, set_ind] : scaffolds_to_sets) {
    auto& set = sets[set_ind];
    release_descriptor_set(pool_allocator, device, set.parent_pool, scaffold);
  }
  scaffolds_to_sets.clear();
  sets.clear();
  free_sets.clear();
#if GROVE_DEBUG
  debug_reference_scaffold = NullOpt{};
#endif
}

vk::Error vk::DescriptorSetAllocator::require_updated_descriptor_set(VkDevice device,
                                                                     VkDescriptorSetLayout layout,
                                                                     DescriptorPoolAllocator& pool_allocator,
                                                                     const DescriptorSetScaffold& scaffold,
                                                                     VkDescriptorSet* out) {
  if (auto res = require_updated_descriptor_set(device, layout, pool_allocator, scaffold)) {
    *out = res.value;
    return {};
  } else {
    return {res.status, res.message};
  }
}

vk::Result<VkDescriptorSet>
vk::DescriptorSetAllocator::require_updated_descriptor_set(VkDevice device,
                                                           VkDescriptorSetLayout layout,
                                                           DescriptorPoolAllocator& pool_allocator,
                                                           const DescriptorSetScaffold& scaffold) {
#ifdef GROVE_DEBUG
  if (debug_reference_scaffold) {
    GROVE_ASSERT(
      debug_reference_scaffold.value().matches_structure_for_descriptor_set_allocation(scaffold));
  }
  debug_reference_scaffold = scaffold;
#endif
  if (auto it = scaffolds_to_sets.find(scaffold); it != scaffolds_to_sets.end()) {
    const uint32_t ind = it->second;
    GROVE_ASSERT(ind < sets.size());
    auto& set = sets[ind];
    GROVE_ASSERT(!set.is_free);
    set.frames_untouched = 0;
    return set.handle;
  }

  uint32_t set_ind{~0u};
  if (!free_sets.empty()) {
    set_ind = free_sets.back();
    GROVE_ASSERT(set_ind < sets.size());
    free_sets.pop_back();
  } else {
    auto pool_handle_res = require_pool_for_descriptor_set(pool_allocator, device, scaffold);
    if (!pool_handle_res) {
      return error_cast<VkDescriptorSet>(pool_handle_res);
    }

    Set pool_set{};
    pool_set.parent_pool = pool_handle_res.value;
    auto alloc_info = vk::make_descriptor_set_allocate_info(
      pool_handle_res.value.pool_handle, &layout, 1);
    if (auto alloc_err = vk::allocate_descriptor_sets(device, &alloc_info, &pool_set.handle)) {
      //  Pool allocator should ensure the pool associated with the required pool handle has
      //  sufficient space for this set. There's a serious bug somewhere if we fail to allocate here.
      GROVE_ASSERT(false);
      return error_cast<VkDescriptorSet>(alloc_err);
    }

    set_ind = uint32_t(sets.size());
    sets.push_back(pool_set);
  }

  GROVE_ASSERT(set_ind != (~0u));
  auto& set = sets[set_ind];
  set.is_free = false;
  vk::DescriptorWrites<32> writes{};
  vk::make_descriptor_writes<32>(writes, set.handle, scaffold);
  vk::update_descriptor_sets(device, writes);
  scaffolds_to_sets[scaffold] = set_ind;
  return set.handle;
}

vk::Result<VkDescriptorSetLayout>
vk::require_descriptor_set_layout(DescriptorSetLayoutCache& cache,
                                  VkDevice device,
                                  const DescriptorSetLayoutBindings& layout_bindings) {
  if (auto it = cache.cache.find(layout_bindings); it != cache.cache.end()) {
    return it->second.layout.handle;
  } else {
    auto info = vk::make_descriptor_set_layout_create_info(
      uint32_t(layout_bindings.size()),
      layout_bindings.data(),
      cache.descriptor_set_layout_create_flags);
    auto create_res = vk::create_descriptor_set_layout(device, &info);
    if (!create_res) {
      return error_cast<VkDescriptorSetLayout>(create_res);
    }
    DescriptorSetLayoutCache::CachedLayout cached{};
    cached.layout = create_res.value;
    cache.cache[layout_bindings] = cached;
    return create_res.value.handle;
  }
}

void vk::destroy_descriptor_set_layout_cache(DescriptorSetLayoutCache* cache, VkDevice device) {
  for (auto& [_, cached] : cache->cache) {
    vk::destroy_descriptor_set_layout(&cached.layout, device);
  }
  cache->cache.clear();
}

bool vk::push_required_descriptor_set_layout(DescriptorSetLayoutCache& layout_cache,
                                             BorrowedDescriptorSetLayouts& dst_layouts,
                                             VkDevice device,
                                             uint32_t dst_set,
                                             const DescriptorSetLayoutBindings& layout_bindings) {
  auto required_res = require_descriptor_set_layout(layout_cache, device, layout_bindings);
  if (!required_res) {
    return false;
  } else {
    dst_layouts.sets.push_back(dst_set);
    dst_layouts.layouts.push_back(required_res.value);
    return true;
  }
}

bool vk::push_required_descriptor_set_layouts(DescriptorSetLayoutCache& cache,
                                              BorrowedDescriptorSetLayouts& layouts,
                                              VkDevice device,
                                              const DescriptorSetLayoutBindings* set_bindings,
                                              uint32_t num_sets) {
  for (uint32_t i = 0; i < num_sets; i++) {
    auto& bindings = set_bindings[i];
    if (!push_required_descriptor_set_layout(cache, layouts, device, i, bindings)) {
      return false;
    }
  }
  return true;
}

vk::Result<vk::BorrowedDescriptorSetLayouts>
vk::make_borrowed_descriptor_set_layouts(DescriptorSetLayoutCache& cache,
                                         VkDevice device,
                                         const DescriptorSetLayoutBindings* bindings,
                                         uint32_t num_sets) {
  vk::BorrowedDescriptorSetLayouts result{};
  if (push_required_descriptor_set_layouts(cache, result, device, bindings, num_sets)) {
    return std::move(result);
  } else {
    return {VK_ERROR_UNKNOWN, "Failed to require some descriptor sets."};
  }
}

vk::Result<vk::BorrowedDescriptorSetLayouts>
vk::make_borrowed_descriptor_set_layouts(vk::DescriptorSetLayoutCache& cache,
                                         VkDevice device,
                                         const ArrayView<const DescriptorSetLayoutBindings>& bindings) {
  return make_borrowed_descriptor_set_layouts(
    cache,
    device,
    bindings.begin(),
    uint32_t(bindings.size()));
}

const char* vk::vk_descriptor_type_name(VkDescriptorType type) {
  switch (type) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
      return "VK_DESCRIPTOR_TYPE_SAMPLER";
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      return "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER";
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      return "VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE";
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      return "VK_DESCRIPTOR_TYPE_STORAGE_IMAGE";
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      return "VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER";
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      return "VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER";
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      return "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER";
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      return "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER";
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      return "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC";
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      return "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC";
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      return "VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT";
    default:
      assert(false);
      return "";
  }
}

GROVE_NAMESPACE_END
