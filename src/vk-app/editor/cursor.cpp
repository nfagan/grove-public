#include "cursor.hpp"
#include "grove/common/common.hpp"
#include "grove/math/intersect.hpp"
#include "grove/math/constants.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace cursor;

constexpr int monitorable_pool_capacity() {
  return 32;
}

int find_entry_index(const std::vector<bool>& in_use) {
  auto it = std::find(in_use.begin(), in_use.end(), false);
  assert(it != in_use.end());
  return int(it - in_use.begin());
}

StateChangeInfo make_state_change_info(MonitorableID id, CursorEvent event) {
  StateChangeInfo result;
  result.id = id;
  result.event = event;
  return result;
}

} //  anon

void cursor::Monitor::update(const Ray& cursor_ray, bool cursor_is_down, bool disabled) {
  pending_callback.clear();
  for (auto& info : intersect_info_by_layer) {
    info.min_t = infinityf();
    info.has_id = false;
  }

  if (disabled) {
    last_cursor_down = cursor_is_down;
    return;
  }

  for (int pool_ind = 0; pool_ind < int(pools.size()); pool_ind++) {
    auto& pool = pools[pool_ind];
    int num_visited{};
    for (int i = 0; i < pool.capacity; i++) {
      if (!pool.in_use[i]) {
        continue;
      } else if (num_visited == pool.size) {
        break;
      } else {
        num_visited++;
      }

      auto& entry = pool.entries[i];
      const uint32_t layer = entry.layer.layer;
      assert(layer < uint32_t(intersect_info_by_layer.size()));
      auto& info = intersect_info_by_layer[layer];
      float t0;
      float t1;
      bool hit;
      if (entry.test) {
        hit = entry.test(cursor_ray, &t0);
      } else {
        hit = ray_aabb_intersect(cursor_ray, entry.bounds, &t0, &t1);
      }
      entry.last_cursor_state = entry.cursor_state;
      entry.cursor_state = {};
      entry.pending_events = {};
      if (!hit) {
        if (entry.last_cursor_state.is_over()) {
          entry.pending_events.flags |= CursorEvent::Exit;
          PoolIndices pool_inds;
          pool_inds.pool = uint32_t(pool_ind);
          pool_inds.entry = uint32_t(i);
          pending_callback.push_back(pool_inds);
        }
      } else if (t0 < info.min_t) {
        info.min_t = t0;
        info.id = entry.id;
        info.indices.pool = uint32_t(pool_ind);
        info.indices.entry = uint32_t(i);
        info.has_id = true;
      }
    }
  }

  for (auto& layer : intersect_info_by_layer) {
    if (!layer.has_id) {
      continue;
    }

    auto& entry = pools[layer.indices.pool].entries[layer.indices.entry];
    const bool already_pushed_pending = entry.pending_events.flags != 0;
    if (cursor_is_down) {
      entry.cursor_state.flags |= CursorState::Down;
      if (!entry.last_cursor_state.is_down()) {
        entry.pending_events.flags |= CursorEvent::Down;
      }
    } else {
      entry.cursor_state.flags |= CursorState::Hovering;
      if (last_cursor_down) {
        //  Was down last frame, but not necessarily on this element.
        entry.pending_events.flags |= CursorEvent::Up;
      }
      if (entry.last_cursor_state.is_down()) {
        //  Was down on this element last frame.
        entry.pending_events.flags |= CursorEvent::Click;
      }
    }
    if (!entry.last_cursor_state.is_over()) {
      entry.pending_events.flags |= CursorEvent::Entry;
    }

    const bool need_push_pending = !already_pushed_pending && entry.pending_events.flags != 0;
    if (need_push_pending) {
      pending_callback.push_back(layer.indices);
    }
  }

  for (auto& pend : pending_callback) {
    auto& entry = pools[pend.pool].entries[pend.entry];
    assert(entry.pending_events.flags != 0);
    entry.on_change(make_state_change_info(entry.id, entry.pending_events));
  }

  last_cursor_down = cursor_is_down;
}

Monitorable* cursor::Monitor::create_monitorable(SelectionLayer layer,
                                                 const Bounds3f& bounds,
                                                 TestIntersect&& test_intersect,
                                                 StateChange&& on_change) {
  while (layer.layer >= uint32_t(intersect_info_by_layer.size())) {
    intersect_info_by_layer.push_back({});
  }

  if (free_pools.empty()) {
    Pool pool{};
    pool.capacity = monitorable_pool_capacity();
    pool.entries = std::make_unique<Monitorable[]>(monitorable_pool_capacity());
    pool.in_use.resize(monitorable_pool_capacity());
    free_pools.push_back(int(pools.size()));
    pools.push_back(std::move(pool));
  }

  auto& pool = pools[free_pools.back()];
  const int entry_ind = find_entry_index(pool.in_use);
  assert(pool.size < pool.capacity && pool.in_use[entry_ind] == false);
  Monitorable* entry = &pool.entries[entry_ind];
  pool.in_use[entry_ind] = true;
  pool.size++;
  if (pool.size == pool.capacity) {
    free_pools.pop_back();
  }

  *entry = {};
  entry->id = MonitorableID{next_monitorable_id++};
  entry->layer = layer;
  entry->bounds = bounds;
  entry->test = std::move(test_intersect);
  entry->on_change = std::move(on_change);
  return entry;
}

void cursor::Monitor::destroy_monitorable(Monitorable* entry) {
  int pool_ind{};
  for (auto& pool : pools) {
    const auto* beg = pool.entries.get();
    const auto* end = beg + pool.capacity;
    if (entry >= beg && entry < end) {
      const auto index = int(entry - beg);
      assert(pool.size > 0 && pool.in_use[index]);
      pool.in_use[index] = false;
      if (pool.size == pool.capacity) {
        assert(std::find(free_pools.begin(), free_pools.end(), pool_ind) == free_pools.end());
        free_pools.push_back(pool_ind);
      }
      pool.size--;
      return;
    }
    pool_ind++;
  }
}

GROVE_NAMESPACE_END
