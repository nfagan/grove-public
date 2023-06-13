#pragma once

#include "descriptor_set.hpp"

namespace grove::vk {

class DescriptorSystem {
public:
  struct PoolAllocatorHandle {
    GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, PoolAllocatorHandle, id)
    GROVE_INTEGER_IDENTIFIER_EQUALITY(PoolAllocatorHandle, id)
    GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
    uint32_t id{};
  };

  struct SetAllocatorHandle {
    GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, SetAllocatorHandle, id)
    GROVE_INTEGER_IDENTIFIER_EQUALITY(SetAllocatorHandle, id)
    GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
    uint32_t id{};
  };

  using PoolSizesView = ArrayView<const DescriptorPoolAllocator::PoolSize>;

public:
  void initialize(uint32_t frame_queue_depth);
  void terminate(const Core& core);
  void begin_frame(const Core& core, const RenderFrameInfo& frame_info);
  void end_frame(const Core& core);

  DescriptorPoolAllocator* get(PoolAllocatorHandle handle);
  bool get(PoolAllocatorHandle handle, DescriptorPoolAllocator** out);

  DescriptorSetAllocator* get(SetAllocatorHandle handle);
  bool get(SetAllocatorHandle handle, DescriptorSetAllocator** out);

  Unique<SetAllocatorHandle> create_set_allocator(PoolAllocatorHandle pool_allocator);
  Unique<PoolAllocatorHandle> create_pool_allocator(const PoolSizesView& pool_sizes,
                                                    uint32_t max_num_sets,
                                                    VkDescriptorPoolCreateFlags flags = 0);
  void destroy_pool_allocator(PoolAllocatorHandle&& handle);
  void destroy_set_allocator(SetAllocatorHandle&& handle);

  size_t num_descriptor_pool_allocators() const;
  size_t num_descriptor_set_allocators() const;
  size_t num_descriptor_sets() const;
  size_t num_descriptor_pools() const;

private:
  void delete_pending_pool_allocators(const Core& core);
  void delete_pending_set_allocators(const Core& core);

private:
  struct SetAllocatorEntry {
    std::unique_ptr<DescriptorSetAllocator> allocator;
    PoolAllocatorHandle associated_pool;
  };
  struct PoolAllocatorEntry {
    std::unique_ptr<DescriptorPoolAllocator> allocator;
  };

  template <typename Handle>
  struct PendingDestruction {
    uint64_t frame_id{};
    Handle handle;
  };

  using PoolAllocators =
    std::unordered_map<PoolAllocatorHandle, PoolAllocatorEntry, PoolAllocatorHandle::Hash>;
  using SetAllocators =
    std::unordered_map<SetAllocatorHandle, SetAllocatorEntry, SetAllocatorHandle::Hash>;

  RenderFrameInfo current_frame_info;

  DynamicArray<PoolAllocators, 2> pool_allocators;
  DynamicArray<SetAllocators, 2> set_allocators;

  PoolAllocators* current_pool_allocators{};
  SetAllocators* current_set_allocators{};

  uint32_t next_pool_allocator_id{1};
  uint32_t next_set_allocator_id{1};

  std::vector<PendingDestruction<PoolAllocatorHandle>> pools_pending_destruction;
  std::vector<PendingDestruction<SetAllocatorHandle>> sets_pending_destruction;
};

}