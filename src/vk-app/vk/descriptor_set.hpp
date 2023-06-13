#pragma once

#include "grove/vk/vk.hpp"
#include "common.hpp"
#include "grove/common/DynamicArray.hpp"
#include "grove/common/ArrayView.hpp"
#include <unordered_map>
#include <unordered_set>
#include <array>

namespace grove::vk {

struct BufferResource {
  VkBuffer buffer;
  size_t offset;
  size_t range;
};

struct CombinedImageSamplerResource {
  VkImageView view;
  VkSampler sampler;
  VkImageLayout layout;
};

struct StorageImageResource {
  VkImageView view;
  VkImageLayout layout;
};

struct BufferViewResource {
  VkBufferView view;
};

struct ShaderResourceDescriptor {
  bool is_array() const {
    return array_range > 0;
  }
  uint32_t num_elements() const {
    return is_array() ? array_range : 1;
  }

  ShaderResourceType type;
  uint32_t binding;
  uint32_t array_range;
  uint32_t array_element;
  union {
    BufferResource buffer;
    BufferViewResource buffer_view;
    CombinedImageSamplerResource combined_image_sampler;
    CombinedImageSamplerResource* combined_image_sampler_array;
    StorageImageResource storage_image;
  };
};

namespace impl {

//  @TODO: Revisit this
inline size_t hash(const CombinedImageSamplerResource& sampler) {
  return std::hash<uint64_t>{}((uint64_t) sampler.view) ^
         std::hash<uint64_t>{}((uint64_t) sampler.sampler);
}

} //  impl

inline size_t hash(const ShaderResourceDescriptor& a) {
  switch (a.type) {
    case ShaderResourceType::UniformBuffer:
    case ShaderResourceType::DynamicUniformBuffer:
    case ShaderResourceType::StorageBuffer:
    case ShaderResourceType::DynamicStorageBuffer: {
      GROVE_ASSERT(!a.is_array());
      return std::hash<uint64_t>{}((uint64_t) a.buffer.buffer);
    }
    case ShaderResourceType::UniformTexelBuffer: {
      GROVE_ASSERT(!a.is_array());
      return std::hash<uint64_t>{}((uint64_t) a.buffer_view.view);
    }
    case ShaderResourceType::CombinedImageSampler: {
      if (a.is_array()) {
        GROVE_ASSERT(a.array_range > 0);
        return impl::hash(a.combined_image_sampler_array[0]);
      } else {
        return impl::hash(a.combined_image_sampler);
      }
    }
    case ShaderResourceType::StorageImage: {
      GROVE_ASSERT(!a.is_array());
      return std::hash<uint64_t>{}((uint64_t) a.storage_image.view);
    }
    default:
      assert(false);
      return 0;
  }
}

namespace impl {

inline bool equal(const CombinedImageSamplerResource& a, const CombinedImageSamplerResource& b) {
  return a.view == b.view && a.sampler == b.sampler && a.layout == b.layout;
}

inline bool equal(const StorageImageResource& a, const StorageImageResource& b) {
  return a.view == b.view && a.layout == b.layout;
}

} //  impl

inline bool operator==(const ShaderResourceDescriptor& a, const ShaderResourceDescriptor& b) {
  if (a.type != b.type ||
      a.binding != b.binding ||
      a.array_range != b.array_range ||
      a.array_element != b.array_element) {
    return false;
  }
  switch (a.type) {
    case ShaderResourceType::UniformBuffer:
    case ShaderResourceType::DynamicUniformBuffer:
    case ShaderResourceType::StorageBuffer:
    case ShaderResourceType::DynamicStorageBuffer: {
      return a.buffer.buffer == b.buffer.buffer &&
             a.buffer.offset == b.buffer.offset &&
             a.buffer.range == b.buffer.range;
    }
    case ShaderResourceType::UniformTexelBuffer: {
      return a.buffer_view.view == b.buffer_view.view;
    }
    case ShaderResourceType::CombinedImageSampler: {
      if (a.is_array()) {
        for (uint32_t i = 0; i < a.array_range; i++) {
          if (!impl::equal(a.combined_image_sampler_array[i],
                           b.combined_image_sampler_array[i])) {
            return false;
          }
        }
        return true;
      } else {
        return impl::equal(a.combined_image_sampler, b.combined_image_sampler);
      }
    }
    case ShaderResourceType::StorageImage: {
      assert(!a.is_array());
      return impl::equal(a.storage_image, b.storage_image);
    }
    default:
      assert(false);
      return false;
  }
}

inline bool operator!=(const ShaderResourceDescriptor& a, const ShaderResourceDescriptor& b) {
  return !(a == b);
}

struct DescriptorSetScaffold {
  struct Hash {
    size_t operator()(const DescriptorSetScaffold& a) const noexcept {
      size_t result{std::hash<uint64_t>{}(a.descriptors.size())};
      result ^= a.set;
      for (auto& descr : a.descriptors) {
        result ^= std::hash<size_t>{}(vk::hash(descr));
      }
      return result;
    }
  };

  bool matches_structure_for_descriptor_set_allocation(const DescriptorSetScaffold& other) const {
    if (descriptors.size() != other.descriptors.size()) {
      return false;
    }
    for (uint32_t i = 0; i < uint32_t(descriptors.size()); i++) {
      auto& a = descriptors[i];
      auto& b = other.descriptors[i];
      if (a.type != b.type || a.num_elements() != b.num_elements()) {
        return false;
      }
    }
    return true;
  }

  uint32_t num_descriptors_of_type(ShaderResourceType type) const {
    uint32_t ct{};
    for (auto& descr : descriptors) {
      if (descr.type == type) {
        ct += descr.num_elements();
      }
    }
    return ct;
  }

  uint32_t total_num_descriptors() const {
    uint32_t ct{};
    for (auto& desc : descriptors) {
      ct += desc.num_elements();
    }
    return ct;
  }

  void sort_descriptors_by_binding() {
    std::sort(descriptors.begin(), descriptors.end(),
              [](const ShaderResourceDescriptor& a, const ShaderResourceDescriptor& b) {
      return a.binding < b.binding;
    });
  }

  uint32_t set;
  DynamicArray<ShaderResourceDescriptor, 16> descriptors;
};

inline bool operator==(const DescriptorSetScaffold& a, const DescriptorSetScaffold& b) {
  if (a.set != b.set || a.descriptors.size() != b.descriptors.size()) {
    return false;
  }
  return std::equal(a.descriptors.begin(), a.descriptors.end(), b.descriptors.begin());
}

ShaderResourceDescriptor make_buffer_resource_descriptor(ShaderResourceType type,
                                                         uint32_t binding,
                                                         VkBuffer buffer,
                                                         size_t range,
                                                         size_t offset = 0);
ShaderResourceDescriptor make_combined_image_sampler_resource_descriptor(uint32_t binding,
                                                                         VkImageView view,
                                                                         VkSampler sampler,
                                                                         VkImageLayout layout);
ShaderResourceDescriptor make_buffer_view_resource_descriptor(ShaderResourceType type,
                                                              uint32_t binding,
                                                              VkBufferView view);

void push_buffer(DescriptorSetScaffold& scaffold, ShaderResourceType type, uint32_t binding,
                 VkBuffer buffer, size_t range, size_t offset = 0);
void push_buffer(DescriptorSetScaffold& scaffold, ShaderResourceType type,
                 uint32_t binding, const vk::ManagedBuffer& buffer);
void push_buffer(DescriptorSetScaffold& scaffold, ShaderResourceType type,
                 uint32_t binding, const vk::ManagedBuffer& buffer, size_t range, size_t offset = 0);
void push_combined_image_sampler(DescriptorSetScaffold& scaffold, uint32_t binding,
                                 VkImageView view, VkSampler sampler, VkImageLayout layout);
void push_combined_image_sampler(DescriptorSetScaffold& scaffold, uint32_t binding,
                                 const vk::SampleImageView& view, VkSampler sampler);
void push_uniform_texel_buffer(DescriptorSetScaffold& scaffold, uint32_t binding, VkBufferView view);
void push_storage_image(DescriptorSetScaffold& scaffold, uint32_t binding,
                        VkImageView view, VkImageLayout layout);

template <typename... Args>
void push_uniform_buffer(DescriptorSetScaffold& scaffold, Args&&... args) {
  push_buffer(scaffold, ShaderResourceType::UniformBuffer, std::forward<Args>(args)...);
}
template <typename... Args>
void push_dynamic_uniform_buffer(DescriptorSetScaffold& scaffold, Args&&... args) {
  push_buffer(scaffold, ShaderResourceType::DynamicUniformBuffer, std::forward<Args>(args)...);
}
template <typename... Args>
void push_dynamic_storage_buffer(DescriptorSetScaffold& scaffold, Args&&... args) {
  push_buffer(scaffold, ShaderResourceType::DynamicStorageBuffer, std::forward<Args>(args)...);
}
template <typename... Args>
void push_storage_buffer(DescriptorSetScaffold& scaffold, Args&&... args) {
  push_buffer(scaffold, ShaderResourceType::StorageBuffer, std::forward<Args>(args)...);
}

inline VkDescriptorBufferInfo to_vk_descriptor_buffer_info(const BufferResource& res) {
  VkDescriptorBufferInfo info{};
  info.buffer = res.buffer;
  info.offset = res.offset;
  info.range = res.range;
  return info;
}

inline VkDescriptorImageInfo to_vk_descriptor_image_info(const CombinedImageSamplerResource& res) {
  VkDescriptorImageInfo info{};
  info.imageLayout = res.layout;
  info.imageView = res.view;
  info.sampler = res.sampler;
  return info;
}

inline VkDescriptorImageInfo to_vk_descriptor_image_info(const StorageImageResource& res) {
  VkDescriptorImageInfo info{};
  info.imageLayout = res.layout;
  info.imageView = res.view;
  info.sampler = VK_NULL_HANDLE;
  return info;
}

template <int N>
struct DescriptorWrites {
  std::array<VkWriteDescriptorSet, N> writes;
  std::array<VkDescriptorBufferInfo, N> buffer_info;
  std::array<VkDescriptorImageInfo, N> image_info;
  uint32_t num_writes;
  uint32_t num_buffers;
  uint32_t num_images;
};

void push_descriptor_write(VkDescriptorSet set,
                           const ShaderResourceDescriptor& descr,
                           VkWriteDescriptorSet*& write_to,
                           VkDescriptorBufferInfo*& buffer_info,
                           VkDescriptorImageInfo*& image_info);

template <int N>
void make_descriptor_writes(DescriptorWrites<N>& result,
                            VkDescriptorSet set,
                            const DescriptorSetScaffold& scaffold) {
  auto* write_to = result.writes.data();
  auto* buffer_info = result.buffer_info.data();
  auto* image_info = result.image_info.data();

  for (auto& descr : scaffold.descriptors) {
    GROVE_ASSERT(write_to - result.writes.data() < N &&
                 buffer_info - result.buffer_info.data() < N &&
                 image_info - result.image_info.data() < N);
    push_descriptor_write(set, descr, write_to, buffer_info, image_info);
  }

  result.num_writes = int(write_to - result.writes.data());
  result.num_images = int(image_info - result.image_info.data());
  result.num_buffers = int(buffer_info - result.buffer_info.data());
}

template <int N>
void update_descriptor_sets(VkDevice device, const DescriptorWrites<N>& writes,
                            uint32_t copy_count = 0, const VkCopyDescriptorSet* copy_sets = nullptr) {
  vkUpdateDescriptorSets(device, writes.num_writes, writes.writes.data(), copy_count, copy_sets);
}

//  An allocator that keeps track of the descriptors allocated from VkDescriptorPools and adds
//  new pools on demand. To my knowledge something like this is necessary if we're targeting
//  Vulkan 1.1 since it's an error to try to allocate more descriptors from a VkDescriptorPool
//  than it has room for.
struct DescriptorPoolAllocator {
  struct AllocatedPool {
    VkDescriptorPool pool_handle;
    uint32_t index;
  };

  struct PoolSize {
    ShaderResourceType type;
    uint32_t count;
  };

  using PoolSizes = DynamicArray<PoolSize, 8>;

  struct Pool {
    vk::DescriptorPool pool;
    PoolSizes descriptor_counts;
    uint32_t set_count{};
  };

  DynamicArray<Pool, 4> descriptor_pools;
  DynamicArray<uint32_t, 4> free_pools;
  PoolSizes pool_capacities;
  uint32_t max_num_sets_per_pool{};
  VkDescriptorPoolCreateFlags pool_create_flags{};
};

void push_pool_sizes_from_layout_bindings(DescriptorPoolAllocator::PoolSizes& sizes,
                                          const DescriptorSetLayoutBindings* layout_bindings,
                                          uint32_t num_layouts,
                                          const std::function<uint32_t(ShaderResourceType)>& get_size);
void push_pool_sizes_from_layout_bindings(DescriptorPoolAllocator::PoolSizes& sizes,
                                          const ArrayView<const DescriptorSetLayoutBindings>& layout_bindings,
                                          const std::function<uint32_t(ShaderResourceType)>& get_size);

DescriptorPoolAllocator
create_descriptor_pool_allocator(const DescriptorPoolAllocator::PoolSize* pool_sizes,
                                 size_t num_pool_sizes,
                                 uint32_t max_num_sets,
                                 VkDescriptorPoolCreateFlags flags = 0);

DescriptorPoolAllocator
create_descriptor_pool_allocator(const ArrayView<const DescriptorPoolAllocator::PoolSize>& pool_sizes,
                                 uint32_t max_num_sets,
                                 VkDescriptorPoolCreateFlags flags = 0);

void destroy_descriptor_pool_allocator(DescriptorPoolAllocator* allocator, VkDevice device);

//  Require a descriptor pool that can be used to allocate a descriptor set whose structure
//  (layout) is given by `scaffold`. The descriptor types in `scaffold` *must* be among those
//  specified when the allocator was constructed.
Result<DescriptorPoolAllocator::AllocatedPool>
require_pool_for_descriptor_set(DescriptorPoolAllocator& allocator,
                                VkDevice device,
                                const DescriptorSetScaffold& scaffold);

//  Mark that the set of resources in the descriptor set described by `scaffold` is no longer
//  in use. Resets the associated VkDescriptorPool if the pool is completely empty and returns it
//  to a free list in that case. `scaffold` *must* have the same structure as that used during the
//  call to `require_pool_for_descriptor_set`.
void release_descriptor_set(DescriptorPoolAllocator& allocator,
                            VkDevice device,
                            const DescriptorPoolAllocator::AllocatedPool& pool,
                            const DescriptorSetScaffold& scaffold);

//  Mark that the set of resources in the descriptor set described by `scaffold` is no longer
//  in use and free the allocated descriptor `set` from the pool. The allocator *must* have been
//  created with VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT bit set.
void release_and_free_descriptor_set(DescriptorPoolAllocator& allocator,
                                     VkDevice device,
                                     VkDescriptorSet set,
                                     const DescriptorPoolAllocator::AllocatedPool& pool,
                                     const DescriptorSetScaffold& scaffold);

void reset_descriptor_pool_allocator(DescriptorPoolAllocator& allocator, VkDevice device);

//  A class for managing descriptor set allocation and updates. Inspired by
//  https://themaister.net/blog/2019/04/20/a-tour-of-granites-vulkan-backend-part-3/
//  Hashes descriptor set scaffolds (layouts) to reuse an allocated + updated descriptor set
//  from a previous frame if possible. Otherwise, allocates + updates a new descriptor set from
//  a pool and caches the result.
//
//  ***Like the approach above, this class assumes the _structure_ of the
//  descriptor set layout (i.e., descriptor types and counts in the set; the VkDescriptorSetLayout
//  handle can change) does not change between frames. You need to manage separate
//  DescriptorSetAllocators for each descriptor set structure.
class DescriptorSetAllocator {
  static constexpr uint32_t num_frames_untouched_before_release = 16;
public:
  struct Set {
    VkDescriptorSet handle;
    DescriptorPoolAllocator::AllocatedPool parent_pool;
    uint32_t frames_untouched;
    bool is_free;
  };
public:
  void begin_frame();
  void end_frame(DescriptorPoolAllocator& pool_allocator, VkDevice device);
  void release(DescriptorPoolAllocator& allocator, VkDevice device);
  Result<VkDescriptorSet> require_updated_descriptor_set(VkDevice device,
                                                         VkDescriptorSetLayout layout,
                                                         DescriptorPoolAllocator& pool_allocator,
                                                         const DescriptorSetScaffold& scaffold);
  [[nodiscard]] Error require_updated_descriptor_set(VkDevice device,
                                                     VkDescriptorSetLayout layout,
                                                     DescriptorPoolAllocator& pool_allocator,
                                                     const DescriptorSetScaffold& scaffold,
                                                     VkDescriptorSet* out);
  size_t num_sets() const {
    return sets.size();
  }
private:
  using HashedSets =
    std::unordered_map<DescriptorSetScaffold, uint32_t, DescriptorSetScaffold::Hash>;

  HashedSets scaffolds_to_sets;
  std::vector<Set> sets;
  std::vector<uint32_t> free_sets;
#ifdef GROVE_DEBUG
  Optional<DescriptorSetScaffold> debug_reference_scaffold;
#endif
};

//  Map a set of VkDescriptorSetLayoutBindings to a VkDescriptorSetLayout.
struct DescriptorSetLayoutCache {
public:
  struct CachedLayout {
    vk::DescriptorSetLayout layout;
  };
  using Hash = HashDescriptorSetLayoutBindings;
  using Equal = EqualDescriptorSetLayoutBindings;
  using Cache = std::unordered_map<DescriptorSetLayoutBindings, CachedLayout, Hash, Equal>;
public:
  Cache cache;
  VkDescriptorSetLayoutCreateFlags descriptor_set_layout_create_flags{};
};

void destroy_descriptor_set_layout_cache(DescriptorSetLayoutCache* cache, VkDevice device);
Result<VkDescriptorSetLayout> require_descriptor_set_layout(DescriptorSetLayoutCache& cache,
                                                            VkDevice device,
                                                            const DescriptorSetLayoutBindings& layout_bindings);

//  Borrow cached VkDescriptorSetLayouts.
struct BorrowedDescriptorSetLayouts {
public:
  void append(const BorrowedDescriptorSetLayouts& other) {
    for (auto& layout : other.layouts) {
      layouts.push_back(layout);
    }
    for (uint32_t set : sets) {
      sets.push_back(set);
    }
  }

  const VkDescriptorSetLayout* find(uint32_t id) const {
    uint32_t i{};
    for (uint32_t s : sets) {
      if (s == id) {
        return &layouts[i];
      }
      i++;
    }
    return nullptr;
  }

public:
  DynamicArray<VkDescriptorSetLayout, 4> layouts;
  DynamicArray<uint32_t, 4> sets;
};

//  Require a handle to a VkDescriptorSetLayout that matches the structure of `layout_bindings` --
//  creating one and adding it to the `layout_cache`, if necessary -- and push it along with the
//  set index to the corresponding arrays in `BorrowedDescriptorSetLayouts`.
[[nodiscard]] bool push_required_descriptor_set_layout(DescriptorSetLayoutCache& layout_cache,
                                                       BorrowedDescriptorSetLayouts& dst_layouts,
                                                       VkDevice device,
                                                       uint32_t dst_set,
                                                       const DescriptorSetLayoutBindings& layout_bindings);

//  Require handles to VkDescriptorSetLayouts for each set in `set_bindings`.
[[nodiscard]] bool push_required_descriptor_set_layouts(DescriptorSetLayoutCache& layout_cache,
                                                        BorrowedDescriptorSetLayouts& dst_layouts,
                                                        VkDevice device,
                                                        const DescriptorSetLayoutBindings* bindings,
                                                        uint32_t num_sets);

//  Create a `BorrowedDescriptorSetLayouts` structure and push VkDescriptorSetLayout handles
//  into it as above.
Result<BorrowedDescriptorSetLayouts>
make_borrowed_descriptor_set_layouts(DescriptorSetLayoutCache& cache,
                                     VkDevice device,
                                     const DescriptorSetLayoutBindings* set_bindings,
                                     uint32_t num_sets);

Result<BorrowedDescriptorSetLayouts>
make_borrowed_descriptor_set_layouts(DescriptorSetLayoutCache& cache,
                                     VkDevice device,
                                     const ArrayView<const DescriptorSetLayoutBindings>& bindings);

const char* vk_descriptor_type_name(VkDescriptorType type);

}