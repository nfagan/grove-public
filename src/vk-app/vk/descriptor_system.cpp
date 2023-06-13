#include "descriptor_system.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace vk;

using PoolAllocatorHandle = DescriptorSystem::PoolAllocatorHandle;
using SetAllocatorHandle = DescriptorSystem::SetAllocatorHandle;

[[maybe_unused]] constexpr const char* logging_id() {
  return "DescriptorSystem";
}

[[maybe_unused]] constexpr const char* message_no_associated_pool() {
  return "No associated descriptor pool allocator found when destroying descriptor set allocator.";
}

} //  anon

void vk::DescriptorSystem::initialize(uint32_t frame_queue_depth) {
  for (uint32_t i = 0; i < frame_queue_depth; i++) {
    pool_allocators.emplace_back();
    set_allocators.emplace_back();
  }
}

void vk::DescriptorSystem::terminate(const Core& core) {
  {
    //  Release sets from corresponding pools before destroying pool allocators.
    GROVE_ASSERT(pool_allocators.size() == set_allocators.size());
    for (uint32_t i = 0; i < uint32_t(pool_allocators.size()); i++) {
      auto& set_allocs = set_allocators[i];
      auto& pool_allocs = pool_allocators[i];
      for (auto& [_, entry] : set_allocs) {
        if (auto it = pool_allocs.find(entry.associated_pool); it != pool_allocs.end()) {
          entry.allocator->release(*it->second.allocator, core.device.handle);
        }
      }
    }
  }

  for (auto& allocators : pool_allocators) {
    for (auto& [_, entry] : allocators) {
      vk::destroy_descriptor_pool_allocator(entry.allocator.get(), core.device.handle);
    }
  }

  pool_allocators.clear();
  set_allocators.clear();
  current_pool_allocators = nullptr;
  current_set_allocators = nullptr;
}

void vk::DescriptorSystem::delete_pending_set_allocators(const Core& core) {
  //  Delete descriptor set allocators. First release sets and descriptors from associated pools.
  auto del_it = sets_pending_destruction.begin();
  while (del_it != sets_pending_destruction.end()) {
    if (del_it->frame_id != current_frame_info.finished_frame_id) {
      ++del_it;
      continue;
    }

    for (int i = 0; i < int(set_allocators.size()); i++) {
      auto& set_allocs = set_allocators[i];
      auto& pool_allocs = pool_allocators[i];

      auto set_it = set_allocs.find(del_it->handle);
      if (set_it != set_allocs.end()) {
        auto& set_entry = set_it->second;
        auto pool_it = pool_allocs.find(set_entry.associated_pool);
        if (pool_it != pool_allocs.end()) {
          set_entry.allocator->release(*pool_it->second.allocator, core.device.handle);
        } else {
          //  This may not be an error if the set allocator has never been used in the time
          //  since its associated pool was deleted, but it is suspicious.
          GROVE_LOG_WARNING_CAPTURE_META(message_no_associated_pool(), logging_id());
        }
        set_allocs.erase(set_it);
      } else {
        assert(false);
      }
    }

    del_it = sets_pending_destruction.erase(del_it);
  }

#ifdef GROVE_DEBUG
  //  We should always have the same number of allocators across frames.
  if (!set_allocators.empty()) {
    auto size0 = int(set_allocators[0].size());
    for (int i = 1; i < int(set_allocators.size()); i++) {
      assert(int(set_allocators[i].size()) == size0 &&
             "Each frame should have the same number of allocators.");
    }
  }
#endif
}

void vk::DescriptorSystem::delete_pending_pool_allocators(const Core& core) {
  auto del_it = pools_pending_destruction.begin();
  while (del_it != pools_pending_destruction.end()) {
    if (del_it->frame_id != current_frame_info.finished_frame_id) {
      ++del_it;
      continue;
    }
#if 1 //  Do this check even in release builds.
    for (int i = 0; i < int(pool_allocators.size()); i++) {
      auto& set_allocs = set_allocators[i];
      auto& pool_allocs = pool_allocators[i];
      for (auto& [_, set_entry] : set_allocs) {
        assert(set_entry.associated_pool != del_it->handle &&
               "Descriptor set allocators that depend on a descriptor pool allocator should be deleted before the pool is deleted.");
        (void) set_entry;
      }
      if (auto pool_it = pool_allocs.find(del_it->handle); pool_it != pool_allocs.end()) {
        vk::destroy_descriptor_pool_allocator(pool_it->second.allocator.get(), core.device.handle);
        pool_allocs.erase(pool_it);
      } else {
        assert(false);
      }
    }
#else
    for (auto& alloc : pool_allocators) {
      if (auto it = alloc.find(del_it->handle); it != alloc.end()) {
        vk::destroy_descriptor_pool_allocator(it->second.allocator.get(), core.device.handle);
        alloc.erase(it);
      } else {
        assert(false);
      }
    }
#endif
    del_it = pools_pending_destruction.erase(del_it);
  }

#ifdef GROVE_DEBUG
  //  We should always have the same number of allocators across frames.
  if (!pool_allocators.empty()) {
    auto size0 = int(pool_allocators[0].size());
    for (int i = 1; i < int(pool_allocators.size()); i++) {
      assert(int(pool_allocators[i].size()) == size0 &&
             "Each frame should have the same number of allocators.");
    }
  }
#endif
}

void vk::DescriptorSystem::begin_frame(const Core& core, const RenderFrameInfo& frame_info) {
  current_frame_info = frame_info;
  current_pool_allocators = &pool_allocators[frame_info.current_frame_index];
  current_set_allocators = &set_allocators[frame_info.current_frame_index];

  for (auto& [_, entry] : *current_set_allocators) {
    entry.allocator->begin_frame();
  }

  delete_pending_set_allocators(core);
  delete_pending_pool_allocators(core);
}

void vk::DescriptorSystem::end_frame(const Core& core) {
  for (auto& [_, entry] : *current_set_allocators) {
    if (auto* pool_allocator = get(entry.associated_pool)) {
      entry.allocator->end_frame(*pool_allocator, core.device.handle);
    }
  }
}

DescriptorSetAllocator* vk::DescriptorSystem::get(SetAllocatorHandle handle) {
  if (auto it = current_set_allocators->find(handle);
      it != current_set_allocators->end()) {
    return it->second.allocator.get();
  } else {
    return nullptr;
  }
}

DescriptorPoolAllocator* vk::DescriptorSystem::get(PoolAllocatorHandle handle) {
  if (auto it = current_pool_allocators->find(handle);
      it != current_pool_allocators->end()) {
    return it->second.allocator.get();
  } else {
    return nullptr;
  }
}

bool vk::DescriptorSystem::get(PoolAllocatorHandle handle, DescriptorPoolAllocator** out) {
  if (auto* alloc = get(handle)) {
    *out = alloc;
    return true;
  } else {
    return false;
  }
}

bool vk::DescriptorSystem::get(SetAllocatorHandle handle, DescriptorSetAllocator** out) {
  if (auto* alloc = get(handle)) {
    *out = alloc;
    return true;
  } else {
    return false;
  }
}

Unique<SetAllocatorHandle>
vk::DescriptorSystem::create_set_allocator(PoolAllocatorHandle pool_allocator) {
  SetAllocatorHandle handle{next_set_allocator_id++};
  for (auto& allocators : set_allocators) {
    SetAllocatorEntry entry{};
    entry.allocator = std::make_unique<DescriptorSetAllocator>();
    entry.associated_pool = pool_allocator;
    allocators[handle] = std::move(entry);
  }
  return Unique<SetAllocatorHandle>{std::move(handle), [this](SetAllocatorHandle* handle) {
    destroy_set_allocator(std::move(*handle));
  }};
}

Unique<PoolAllocatorHandle>
vk::DescriptorSystem::create_pool_allocator(const PoolSizesView& pool_sizes,
                                            uint32_t max_num_sets,
                                            VkDescriptorPoolCreateFlags flags) {
  PoolAllocatorHandle handle{next_pool_allocator_id++};
  for (auto& allocators : pool_allocators) {
    PoolAllocatorEntry entry{};
    entry.allocator = std::make_unique<DescriptorPoolAllocator>(
      create_descriptor_pool_allocator(pool_sizes, max_num_sets, flags));
    allocators[handle] = std::move(entry);
  }
  return {std::move(handle), [this](PoolAllocatorHandle* handle) {
    destroy_pool_allocator(std::move(*handle));
  }};
}

void vk::DescriptorSystem::destroy_pool_allocator(PoolAllocatorHandle&& handle) {
  PendingDestruction<PoolAllocatorHandle> pend{};
  pend.handle = handle;
  pend.frame_id = current_frame_info.current_frame_id;
  pools_pending_destruction.push_back(pend);
}

void vk::DescriptorSystem::destroy_set_allocator(SetAllocatorHandle&& handle) {
  PendingDestruction<SetAllocatorHandle> pend{};
  pend.handle = handle;
  pend.frame_id = current_frame_info.current_frame_id;
  sets_pending_destruction.push_back(pend);
}

size_t vk::DescriptorSystem::num_descriptor_pool_allocators() const {
  size_t res{};
  for (auto& allocators : pool_allocators) {
    res += allocators.size();
  }
  return res;
}

size_t vk::DescriptorSystem::num_descriptor_set_allocators() const {
  size_t res{};
  for (auto& allocators : set_allocators) {
    res += allocators.size();
  }
  return res;
}

size_t vk::DescriptorSystem::num_descriptor_sets() const {
  size_t res{};
  for (auto& allocators : set_allocators) {
    for (auto& [_, entry] : allocators) {
      res += entry.allocator->num_sets();
    }
  }
  return res;
}

size_t vk::DescriptorSystem::num_descriptor_pools() const {
  size_t res{};
  for (auto& allocators : pool_allocators) {
    for (auto& [_, entry] : allocators) {
      res += entry.allocator->descriptor_pools.size();
    }
  }
  return res;
}

GROVE_NAMESPACE_END
