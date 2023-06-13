#include "transform_allocator.hpp"
#include "transform_system.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include <numeric>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace transform;

struct Config {
  static constexpr uint16_t num_instances_per_page = 512;
  static constexpr uint16_t num_instances_per_pool = 32;
};

static_assert(Config::num_instances_per_page % Config::num_instances_per_pool == 0);

constexpr uint16_t num_pools_per_page() {
  return Config::num_instances_per_page / Config::num_instances_per_pool;
}

using Page = TransformAllocator::Page;
using Pool = TransformAllocator::Pool;

Page create_page() {
  Page page;
  page.instances = std::make_unique<TransformInstance[]>(Config::num_instances_per_page);
  page.pools.resize(num_pools_per_page());
  page.free_pools.resize(num_pools_per_page());
  for (uint16_t i = 0; i < num_pools_per_page(); i++) {
    auto& pool = page.pools[i];
    pool.begin = page.instances.get() + i * Config::num_instances_per_pool;
  }
  std::iota(page.free_pools.begin(), page.free_pools.end(), uint16_t(0));
  return page;
}

} //  anon

uint16_t transform::TransformAllocator::find_next_entry(const Pool& pool) {
  if (pool.allocated_range == pool.size) {
    return pool.size;
  } else {
    for (uint16_t i = 0; i < Config::num_instances_per_pool; i++) {
      if (!pool.begin[i].allocated) {
        return i;
      }
    }
    assert(false);
    return uint16_t(~uint16_t(0));
  }
}

void transform::TransformAllocator::require_page(uint16_t* out_page, uint16_t* out_pool) {
  uint16_t page_ind{};
  uint16_t pool_ind{};
  bool found_page{};
  for (auto& page : pages) {
    if (!page.free_pools.empty()) {
      pool_ind = page.free_pools.back();
      found_page = true;
      break;
    }
    page_ind++;
  }
  if (!found_page) {
    pages.push_back(create_page());
    const auto& page = pages.back();
    page_ind = uint16_t(pages.size() - 1);
    pool_ind = page.free_pools.back();
  }
  *out_page = page_ind;
  *out_pool = pool_ind;
}

TransformInstance* transform::TransformAllocator::create_instance(TransformSystem* system,
                                                                  const TRS<float>& source) {
  uint16_t page_ind;
  uint16_t pool_ind;
  require_page(&page_ind, &pool_ind);

  auto& page = pages[page_ind];
  auto& pool = page.pools[pool_ind];
  assert(pool.size < Config::num_instances_per_pool);
  uint16_t entry_ind = find_next_entry(pool);

  pool.begin[entry_ind].allocated = true;
  pool.size++;
  pool.allocated_range = std::max(uint16_t(entry_ind + 1), pool.allocated_range);
  if (pool.size == Config::num_instances_per_pool) {
    page.free_pools.pop_back();
  }

  auto* res = pool.begin + entry_ind;
  res->source = source;
  res->current = source;
  res->system = system;
  res->maybe_push_pending();
  return res;
}

void transform::TransformAllocator::destroy_instance(TransformInstance* inst) {
  assert(inst->allocated);
  for (auto& page : pages) {
    const auto* beg = page.instances.get();
    const auto* end = beg + Config::num_instances_per_page;
    if (inst >= beg && inst < end) {
      auto off = uint16_t(inst - beg);
      uint16_t pool_ind = off / Config::num_instances_per_pool;
      uint16_t entry_ind = off - pool_ind * Config::num_instances_per_pool;
      auto& pool = page.pools[pool_ind];
      assert(pool.size > 0 && pool.allocated_range > 0);
      if (pool.size == Config::num_instances_per_pool) {
        assert(std::find(
          page.free_pools.begin(), page.free_pools.end(), pool_ind) == page.free_pools.end());
        page.free_pools.push_back(pool_ind);
      }
      pool.size--;
      if (pool.allocated_range == entry_ind + 1) {
        pool.allocated_range--;
      }
      *inst = {};
      return;
    }
  }
  assert(false);
}

void transform::TransformInstance::maybe_push_pending() {
  if (!pushed) {
    system->push_pending(this);
    pushed = true;
    for (auto* child : children) {
      child->maybe_push_pending();
    }
  }
}

void transform::TransformInstance::clear_pushed_pending() {
  pushed = false;
}

void transform::TransformInstance::add_child(TransformInstance* child) {
#ifdef GROVE_DEBUG
  for (auto* c : children) {
    assert(c != child);
  }
#endif
  children.push_back(child);
}

void transform::TransformInstance::remove_child(TransformInstance* child) {
  for (auto* c : children) {
    if (c == child) {
      children.erase(&c);
      return;
    }
  }
  assert(false);
}

void transform::TransformInstance::set(const TRS<float>& src) {
  assert(allocated && "Attempting to set a previously freed instance.");
  source = src;
  maybe_push_pending();
}

void transform::TransformInstance::set_parent(TransformInstance* inst) {
  assert(allocated && "Attempting to set a previously freed instance.");
  if (parent) {
    parent->remove_child(this);
  }
  parent = inst;
  if (parent) {
    parent->add_child(this);
  }
  maybe_push_pending();
}

GROVE_NAMESPACE_END
