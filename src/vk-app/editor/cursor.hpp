#pragma once

#include "grove/common/identifier.hpp"
#include "grove/common/DynamicArray.hpp"
#include "grove/math/Bounds3.hpp"
#include "grove/math/Ray.hpp"
#include <functional>
#include <vector>

namespace grove::cursor {

struct CursorEvent {
  static constexpr uint32_t Entry = 1u;
  static constexpr uint32_t Exit = 1u << 1u;
  static constexpr uint32_t Down = 1u << 2u;
  static constexpr uint32_t Up = 1u << 3u;
  static constexpr uint32_t Click = 1u << 4u;
public:
  bool is_down() const {
    return flags & Down;
  }
  bool is_up() const {
    return flags & Up;
  }
  bool is_click() const {
    return flags & Click;
  }
  bool is_entry() const {
    return flags & Entry;
  }
  bool is_exit() const {
    return flags & Exit;
  }
public:
  uint32_t flags{};
};

struct CursorState {
public:
  static constexpr uint32_t Hovering = 1u;
  static constexpr uint32_t Down = 1u << 1u;
public:
  bool is_down() const {
    return flags & Down;
  }
  bool is_hovering() const {
    return flags & Hovering;
  }
  bool is_over() const {
    return is_down() || is_hovering();
  }
public:
  uint32_t flags{};
};

struct SelectionLayer {
  uint32_t layer{};
};

struct MonitorableID {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(MonitorableID, id)
  GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, MonitorableID, id)
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  uint32_t id;
};

struct StateChangeInfo {
  MonitorableID id;
  CursorEvent event;
};

using TestIntersect = std::function<bool(const Ray&, float*)>;
using StateChange = std::function<void(const StateChangeInfo&)>;

class Monitorable {
  friend class Monitor;
public:
  void set_bounds(const Bounds3f& b) {
    bounds = b;
  }
  CursorState get_state() const {
    return cursor_state;
  }
  MonitorableID get_id() const {
    return id;
  }
private:
  MonitorableID id{};
  SelectionLayer layer{};
  CursorState cursor_state{};
  CursorState last_cursor_state{};
  CursorEvent pending_events{};
  Bounds3f bounds{};
  TestIntersect test{};
  StateChange on_change{};
};

class Monitor {
  struct Pool {
    std::unique_ptr<Monitorable[]> entries;
    std::vector<bool> in_use;
    int size;
    int capacity;
  };

  struct PoolIndices {
    uint32_t pool;
    uint32_t entry;
  };

  struct HashPoolIndices {
    size_t operator()(const PoolIndices& inds) const noexcept {
      uint64_t v{inds.pool};
      uint64_t entry{inds.entry};
      entry <<= 32;
      v |= entry;
      return std::hash<uint64_t>{}(v);
    }
  };

  struct EqualPoolIndices {
    bool operator()(const PoolIndices& a, const PoolIndices& b) const noexcept {
      return a.pool == b.pool && a.entry == b.entry;
    }
  };

  struct TestIntersectInfo {
    MonitorableID id;
    float min_t;
    bool has_id;
    PoolIndices indices;
  };

public:
  void update(const Ray& cursor_ray, bool cursor_is_down, bool disabled);
  Monitorable* create_monitorable(SelectionLayer layer, const Bounds3f& bounds,
                                  TestIntersect&& test_intersect, StateChange&& on_change);
  void destroy_monitorable(Monitorable* entry);
private:
  std::vector<Pool> pools;
  std::vector<int> free_pools;
  DynamicArray<TestIntersectInfo, 4> intersect_info_by_layer;
  std::vector<PoolIndices> pending_callback;
  bool last_cursor_down{};
  uint32_t next_monitorable_id{1};
};

}