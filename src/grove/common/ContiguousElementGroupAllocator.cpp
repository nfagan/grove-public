#include "ContiguousElementGroupAllocator.hpp"
#include "grove/common/common.hpp"
#include <cassert>

GROVE_NAMESPACE_BEGIN

using ElementGroupHandle = ContiguousElementGroupAllocator::ElementGroupHandle;
using ElementGroup = ContiguousElementGroupAllocator::ElementGroup;

bool ContiguousElementGroupAllocator::ElementGroup::available() const {
  return !state[0];
}

void ContiguousElementGroupAllocator::ElementGroup::set_available(bool v) {
  state[0] = !v;
}

bool ContiguousElementGroupAllocator::ElementGroup::pending_release() const {
  return state[1];
}

void ContiguousElementGroupAllocator::ElementGroup::set_pending_release(bool v) {
  state[1] = v;
}

const ElementGroup* ContiguousElementGroupAllocator::read_group(ElementGroupHandle gh) const {
  assert(gh.index < uint32_t(groups.size()));
  auto* res = groups.data() + gh.index;
  assert(!res->available());
  return res;
}

uint32_t ContiguousElementGroupAllocator::reserve(uint32_t count, ElementGroupHandle* dst) {
  ElementGroupHandle dst_handle{};
  bool found_group{};
  for (uint32_t i = 0; i < uint32_t(groups.size()); i++) {
    if (groups[i].available()) {
      dst_handle.index = i;
      found_group = true;
      break;
    }
  }
  if (!found_group) {
    dst_handle.index = uint32_t(groups.size());
    groups.emplace_back();
  }

  auto& group = groups[dst_handle.index];
  assert(group.available() && !group.pending_release());
  group.set_available(false);
  group.offset = tail;
  group.count = count;
  tail += count;
  *dst = dst_handle;
  return tail;
}

void ContiguousElementGroupAllocator::release(ElementGroupHandle gh) {
  assert(gh.index < uint32_t(groups.size()));
  auto& group = groups[gh.index];
  assert(!group.available() && !group.pending_release());
  group.set_pending_release(true);
}

bool ContiguousElementGroupAllocator::arrange(unsigned char* data, size_t element_size,
                                              uint32_t* dst_tail) {
  bool modified{};
  for (auto& group : groups) {
    if (group.pending_release()) {
      assert(!group.available());

      const uint32_t release_beg = group.offset;
      const uint32_t release_end = release_beg + group.count;
      const uint32_t release_count = group.count;
      assert(tail >= release_end);

      memmove(
        data + release_beg * element_size,
        data + release_end * element_size,
        (tail - release_end) * element_size);
      tail -= release_count;

      for (auto& gp : groups) {
        if (&gp != &group && !gp.available() && gp.offset >= release_end) {
          gp.offset -= release_count;
        }
      }

      group = {};
      modified = true;
    }
  }

  *dst_tail = tail;
  return modified;
}

uint32_t ContiguousElementGroupAllocator::arrange_implicit(Movement* movements, uint32_t* dst_tail) {
  uint32_t num_movements{};

  for (auto& group : groups) {
    if (group.pending_release()) {
      assert(!group.available());

      const uint32_t release_beg = group.offset;
      const uint32_t release_end = release_beg + group.count;
      const uint32_t release_count = group.count;
      assert(tail >= release_end);

      Movement move{};
      move.dst = release_beg;
      move.src = release_end;
      move.count = tail - release_end;
      movements[num_movements++] = move;

      tail -= release_count;

      for (auto& gp : groups) {
        if (&gp != &group && !gp.available() && gp.offset >= release_end) {
          gp.offset -= release_count;
        }
      }

      group = {};
    }
  }

  *dst_tail = tail;
  return num_movements;
}

void ContiguousElementGroupAllocator::apply(const Movement* movements, uint32_t num_movements,
                                            void* data, size_t element_size) {
  for (uint32_t i = 0; i < num_movements; i++) {
    movements[i].apply(data, element_size);
  }
}

void ContiguousElementGroupAllocator::Movement::apply(void* data, size_t element_size) const {
  auto* dc = static_cast<unsigned char*>(data);
  memmove(dc + dst * element_size, dc + src * element_size, count * element_size);
}

GROVE_NAMESPACE_END
