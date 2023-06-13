#pragma once

#include "trs.hpp"
#include "grove/common/DynamicArray.hpp"
#include <memory>
#include <vector>

namespace grove::transform {

class TransformSystem;

class TransformInstance {
  friend class TransformSystem;
  friend class TransformAllocator;
public:
  void set(const TRS<float>& source);
  void set_parent(TransformInstance* inst);
  TransformInstance* get_parent() const {
    return parent;
  }
  const TRS<float>& get_current() const {
    return current;
  }
  const TRS<float>& get_source() const {
    return source;
  }

private:
  void maybe_push_pending();
  void clear_pushed_pending();
  void remove_child(TransformInstance* child);
  void add_child(TransformInstance* child);

private:
  TRS<float> source;
  TRS<float> current;
  TransformInstance* parent;
  DynamicArray<TransformInstance*, 2> children;
  TransformSystem* system;
  bool allocated;
  bool pushed;
};

class TransformAllocator {
public:
  struct EntryIndices {
    uint16_t page;
    uint16_t pool;
    uint16_t entry;
  };
  struct HashEntryIndices {
    size_t operator()(const EntryIndices& a) const noexcept {
      uint64_t v{a.page};
      uint64_t entry{a.entry};
      entry <<= 16;
      uint64_t pool{a.pool};
      pool <<= 32;
      v |= (entry | pool);
      return std::hash<uint64_t>{}(v);
    }
  };
  struct Pool {
    TransformInstance* begin;
    uint16_t size;
    uint16_t allocated_range;
  };
  struct Page {
    std::unique_ptr<TransformInstance[]> instances;
    std::vector<Pool> pools;
    std::vector<uint16_t> free_pools;
  };

  friend inline bool operator==(const EntryIndices& a, const EntryIndices& b) noexcept {
    return a.page == b.page && a.pool == b.pool && a.entry == b.entry;
  }

public:
  TransformInstance* create_instance(TransformSystem* system, const TRS<float>& source);
  void destroy_instance(TransformInstance* inst);

private:
  void require_page(uint16_t* page_ind, uint16_t* pool_ind);
  static uint16_t find_next_entry(const Pool& pool);

private:
  std::vector<Page> pages;
};

}