#pragma once

#include "intrin.hpp"
#include <vector>
#include <array>
#include <cassert>
#include <memory>
#include <cmath>

namespace grove {

namespace detail {

constexpr uint64_t u64_ceil_div(const uint64_t a, const uint64_t b) {
  return a / b + uint64_t(a % b != 0);
}

constexpr uint64_t num_required_buckets(const uint64_t s, const uint64_t i, const uint64_t levels) {
  const auto base = u64_ceil_div(uint64_t(1) << (levels - i - 1), 64);
  return i + 1 == levels ? s + base : num_required_buckets(s + base, i + 1, levels);
}

constexpr uint64_t u64_shift1(const uint64_t n) {
  return uint64_t(1) << n;
}

inline uint64_t to_level0_slot_index(uint64_t level, uint64_t local_slot, uint64_t bit) {
  return (detail::u64_shift1(level)) * (local_slot * 64 + bit);
}

inline void to_local_slot_index(uint64_t level, uint64_t l0_slot_index,
                                uint64_t* slot, uint64_t* bit) {
  const auto bin = l0_slot_index / detail::u64_shift1(level);
  *slot = bin / 64u;
  *bit = bin - (*slot) * 64u;
}

inline uint64_t bitset_range(uint64_t beg, uint64_t end) {
  assert(beg == end || (beg < end && beg < 64 && end <= 64));
  uint64_t res{};
  for (uint64_t i = beg; i < end; i++) {
    res |= detail::u64_shift1(i);
  }
  return res;
}

inline uint64_t free_masked(uint64_t a, uint64_t m) {
#ifdef GROVE_DEBUG
  //  If freeing (based on m), assert in-use (in a).
  for (uint64_t i = 0; i < 64; i++) {
    if (m & detail::u64_shift1(i)) {
      assert(a & detail::u64_shift1(i));
    }
  }
#endif
  a &= ~m;
  return a;
}

inline uint64_t acquire_masked(uint64_t a, uint64_t m) {
#ifdef GROVE_DEBUG
  //  If acquiring (based on m), assert free (in a).
  for (uint64_t i = 0; i < 64; i++) {
    if (m & detail::u64_shift1(i)) {
      assert(!(a & detail::u64_shift1(i)));
    }
  }
#endif
  a |= m;
  return a;
}

inline uint64_t level_slots(uint64_t num_slots, uint64_t level) {
  return num_slots / detail::u64_shift1(level);
}

} //  detail

//  Slot: atom of an allocation. Each slot is `SlotSizeBytes` in size.
//  Bucket: integer representing free vs. in-use state of 64 slots.
//  Levels: defines the number of slots per page as a power-of-two exponent, and consequently the
//  page size and maximum allocation size. For example, if levels == 4, then each page has
//  2^(levels-1) == 8 slots, and a capacity of 8 * `SlotSizeBytes`.

template <uint64_t SlotSizeBytes, uint64_t Levels, typename BlockPageIndex = uint16_t>
class BuddyAllocator {
public:
  static constexpr uint64_t slot_size_bytes = SlotSizeBytes;
  static constexpr uint64_t levels = Levels;
  static constexpr uint64_t num_slots = detail::u64_shift1(levels - 1);
  static constexpr uint64_t page_size_bytes = num_slots * slot_size_bytes;
  static constexpr uint64_t total_num_buckets = detail::num_required_buckets(0, 0, levels);

public:
  struct Block {
    unsigned char* data;
    BlockPageIndex page;
    uint8_t level;
  };

  struct Page {
  public:
    template <bool Free>
    void set_free_masked(uint64_t level, uint64_t local_slot, uint64_t bit_beg, uint64_t bit_end,
                         const uint64_t* bucket_offs) {
      uint64_t& buck = bucket(level, local_slot, bucket_offs);
      const uint64_t m = detail::bitset_range(bit_beg, bit_end);
      if constexpr (Free) {
        buck = detail::free_masked(buck, m);
        increment_num_free(level, bit_end - bit_beg);
      } else {
        buck = detail::acquire_masked(buck, m);
        decrement_num_free(level, bit_end - bit_beg);
      }
    }

    template <bool Free>
    void set_all_free(uint64_t level, uint64_t local_slot, const uint64_t* bucket_offs) {
      uint64_t& buck = bucket(level, local_slot, bucket_offs);
      if constexpr (Free) {
        buck = 0;
        increment_num_free(level, 64);
      } else {
        buck = ~uint64_t(0);
        decrement_num_free(level, 64);
      }
    }

    template <bool Free>
    void set_one_free(uint64_t level, uint64_t local_slot, uint64_t bit,
                      const uint64_t* bucket_offs) {
      uint64_t& buck = bucket(level, local_slot, bucket_offs);
      const uint64_t m = detail::u64_shift1(bit);
      if constexpr (Free) {
        buck = detail::free_masked(buck, m);
        increment_num_free(level, 1);
      } else {
        buck = detail::acquire_masked(buck, m);
        decrement_num_free(level, 1);
      }
    }

    uint64_t& bucket(uint64_t level, uint64_t local_slot, const uint64_t* bucket_offs) {
      assert(level < buckets.size());
      return *(buckets.data() + bucket_offs[level] + local_slot);
    }

    const uint64_t& bucket(uint64_t level, uint64_t local_slot, const uint64_t* bucket_offs) const {
      assert(level < buckets.size());
      return *(buckets.data() + bucket_offs[level] + local_slot);
    }

    void increment_num_free(uint64_t level, uint64_t n) {
      auto& nf = num_free[level];
      assert(nf + n <= detail::level_slots(num_slots, level));
      nf += n;
    }

    void decrement_num_free(uint64_t level, uint64_t n) {
      auto& nf = num_free[level];
      assert(nf >= n);
      nf -= n;
    }

    bool is_free(uint64_t level, uint64_t local_slot, uint64_t bit,
                 const uint64_t* bucket_offs) const {
      const uint64_t buck = bucket(level, local_slot, bucket_offs);
      return (buck & detail::u64_shift1(bit)) == 0;
    }

    uint64_t count_free(uint64_t level) const {
      assert(level < levels);
      return num_free[level];
    }

    uint64_t bytes_allocated() const {
      auto num_allocated = detail::u64_shift1(levels - 1) - num_free[0];
      return num_allocated * slot_size_bytes;
    }

    bool empty() const {
      for (auto& bucket : buckets) {
        if (bucket != 0) {
          return false;
        }
      }
      for (uint64_t i = 0; i < levels; i++) {
        if (num_free[i] != detail::u64_shift1(levels - i - 1)) {
          return false;
        }
      }
      return true;
    }

    void init_num_free() {
      for (uint64_t i = 0; i < levels; i++) {
        num_free[i] = uint64_t(1) << (levels - i - 1);
      }
    }

    static Page create_with_data(std::unique_ptr<unsigned char[]> data, size_t data_size) {
      assert(data_size >= page_size_bytes);
      (void) data_size;
      Page result{};
      result.init_num_free();
      result.data = std::move(data);
      return result;
    }

    static Page create() {
      Page result{};
      result.init_num_free();
      result.data = std::make_unique<unsigned char[]>(page_size_bytes);
      return result;
    }

  public:
    std::array<uint64_t, total_num_buckets> buckets;
    std::array<uint64_t, levels> num_free;
    std::unique_ptr<unsigned char[]> data;
  };

public:
  BuddyAllocator() {
    static_assert(levels > 0);
    static_assert(levels < 0xff);

    bucket_offsets[0] = 0;
    for (uint64_t i = 1; i < levels; i++) {
      auto base = detail::u64_ceil_div(detail::u64_shift1(levels - i), 64);
      bucket_offsets[i] = bucket_offsets[i - 1] + base;
    }

    for (uint64_t i = 0; i < levels; i++) {
      level_counts[i] = detail::u64_ceil_div(detail::level_slots(num_slots, i), 64);
    }
  }

  ~BuddyAllocator() {
#ifdef GROVE_DEBUG
    for (auto& page : pages) {
      assert(page.empty() && "Some blocks not freed.");
    }
#endif
  }

  void clear() {
    pages.clear();
  }

  void push_page(std::unique_ptr<unsigned char[]> data, size_t data_size) {
    pages.emplace_back(Page::create_with_data(std::move(data), data_size));
  }

  void free(Block block) {
    if (block.data) {
      assert(block.page < BlockPageIndex(pages.size()));
      assert(block.level < levels);
      assert(block.data >= pages[block.page].data.get() &&
             uint64_t(block.data - pages[block.page].data.get()) <= page_size_bytes);
      release(pages[block.page], block, true);
    }
  }

  [[nodiscard]] bool try_allocate(uint64_t s, Block* block) {
    if (!s || s > page_size_bytes) {
      *block = {};
      return s == 0;
    } else {
      *block = maybe_allocate(s);
      return block->data != nullptr;
    }
  }

  [[nodiscard]] Block allocate(uint64_t s) {
    if (!s || s > page_size_bytes) {
      assert(s == 0);
      return {};
    }

    auto block = maybe_allocate(s);
    if (block.data) {
      return block;
    }

    //  Add a page and complete the allocation. Because `s` is less than the page size, it
    //  must succeed.
    pages.emplace_back(Page::create());
    return maybe_allocate(s);
  }

  void shrink_to_fit() {
    auto it = pages.begin();
    while (it != pages.end()) {
      if (it->bytes_allocated() == 0) {
        it = pages.erase(it);
      } else {
        ++it;
      }
    }
  }

  uint64_t bytes_allocated() const {
    uint64_t s{};
    for (auto& page : pages) {
      s += page.bytes_allocated();
    }
    return s;
  }

  uint64_t bytes_reserved() const {
    return pages.size() * page_size_bytes;
  }

  uint64_t num_pages() const {
    return pages.size();
  }

  static uint64_t bytes_to_level(uint64_t s) {
    return uint64_t(std::ceil(std::log2(double(std::max(s, slot_size_bytes)) / slot_size_bytes)));
  }

private:
  Block maybe_allocate(size_t s) {
    assert(s > 0 && s <= page_size_bytes);

    const uint64_t level = bytes_to_level(s);
    const uint64_t buckets = level_counts[level];

    const auto num_pages = BlockPageIndex(pages.size());
    for (BlockPageIndex pi = 0; pi < num_pages; pi++) {
      auto& page = pages[pi];
      if (page.count_free(level) == 0) {
        continue;
      }

      for (uint64_t i = 0; i < buckets; i++) {
        const uint64_t first_set = ffsll_one_based(~page.bucket(level, i, bucket_offsets.data()));
        if (first_set == 0) {
          continue;
        }

        const uint64_t bit = first_set - 1;
#if 1
        assert(detail::to_level0_slot_index(level, i, bit) < num_slots);
        return acquire(page, pi, level, i, bit);
#else
        const uint64_t l0_slot_index = detail::to_level0_slot_index(level, i, bit);
        if (l0_slot_index >= num_slots) {
          break;
        } else {
          return acquire(page, pi, level, i, bit);
        }
#endif
      }
    }

    return {};
  }

  template <bool Free>
  void set_children_free(Page& page, uint64_t level, uint64_t l0_slot_beg) {
    auto l0_slot_end = l0_slot_beg + detail::u64_shift1(level);

    for (uint64_t c = 0; c < level; c++) {
      uint64_t slot_beg;
      uint64_t bit_beg;
      detail::to_local_slot_index(c, l0_slot_beg, &slot_beg, &bit_beg);

      uint64_t slot_end;
      uint64_t bit_end;
      detail::to_local_slot_index(c, l0_slot_end, &slot_end, &bit_end);

      if (slot_beg == slot_end) {
        page.template set_free_masked<Free>(c, slot_beg, bit_beg, bit_end, bucket_offsets.data());

      } else {
        assert(slot_end > slot_beg);
        //  First bucket
        page.template set_free_masked<Free>(c, slot_beg, bit_beg, 64, bucket_offsets.data());
        //  Intermediate buckets
        for (uint64_t i = slot_beg + 1; i < slot_end; i++) {
          page.template set_all_free<Free>(c, i, bucket_offsets.data());
        }
        //  Last bucket
#if 1
        assert(bit_end == 0);
#else
        page.template set_free_masked<Free>(c, slot_end, 0, bit_end, bucket_offsets.data());
#endif
      }
    }
  }

  Block acquire(Page& page, const BlockPageIndex page_index,
                const uint64_t level, const uint64_t local_slot, const uint64_t bit) {
    const uint64_t l0_slot_beg = detail::to_level0_slot_index(level, local_slot, bit);

    page.template set_one_free<false>(level, local_slot, bit, bucket_offsets.data());
    set_children_free<false>(page, level, l0_slot_beg);

    for (uint64_t p = level + 1; p < levels; p++) {
      uint64_t p_slot;
      uint64_t p_bit;
      detail::to_local_slot_index(p, l0_slot_beg, &p_slot, &p_bit);
      if (page.is_free(p, p_slot, p_bit, bucket_offsets.data())) {
        page.template set_one_free<false>(p, p_slot, p_bit, bucket_offsets.data());
      } else {
        break;
      }
    }

    Block result{};
    result.data = page.data.get() + l0_slot_beg * slot_size_bytes;
    result.level = uint8_t(level);
    result.page = page_index;
    return result;
  }

  void release(Page& page, Block block, bool free_children) {
    assert(block.data);
    const uint64_t p = block.data - page.data.get();
    assert((p % slot_size_bytes) == 0);

    uint64_t blk_slot;
    uint64_t blk_bit;
    detail::to_local_slot_index(block.level, p / slot_size_bytes, &blk_slot, &blk_bit);
    const uint64_t l0_slot_beg = detail::to_level0_slot_index(block.level, blk_slot, blk_bit);

    page.template set_one_free<true>(block.level, blk_slot, blk_bit, bucket_offsets.data());
    if (free_children) {
      set_children_free<true>(page, block.level, l0_slot_beg);
    }

    const uint64_t self_sz = detail::u64_shift1(block.level);
    const uint64_t next_sz = self_sz * 2;

    uint64_t buddy_slot;
    uint64_t buddy_bit;
    if ((l0_slot_beg % next_sz) == 0) {
      detail::to_local_slot_index(block.level, l0_slot_beg + self_sz, &buddy_slot, &buddy_bit);
    } else {
      assert(l0_slot_beg >= self_sz && (l0_slot_beg % next_sz) == self_sz);
      detail::to_local_slot_index(block.level, l0_slot_beg - self_sz, &buddy_slot, &buddy_bit);
    }

    if (block.level + 1 < levels &&
        page.is_free(block.level, buddy_slot, buddy_bit, bucket_offsets.data())) {
      auto next_block = block;
      next_block.level++;
      release(page, next_block, false);
    }
  }

public:
  std::vector<Page> pages;
  std::array<uint64_t, levels> bucket_offsets;
  std::array<uint64_t, levels> level_counts;
};

template <typename Alloc>
bool any_overlapped_ranges(const Alloc& alloc, const typename Alloc::Block* blocks,
                           const uint64_t* block_sizes, int num_blocks, bool* mem_used) {
  uint64_t max_page{};
  for (int i = 0; i < num_blocks; i++) {
    max_page = std::max(uint64_t(blocks[i].page), max_page);
  }

  for (uint64_t i = 0; i <= max_page; i++) {
    std::fill(mem_used, mem_used + Alloc::page_size_bytes, false);
    for (int j = 0; j < num_blocks; j++) {
      auto& blk = blocks[j];
      const uint64_t sz = block_sizes[j];
      if (blk.page != i) {
        continue;
      }
      if (!blk.data) {
        assert(sz == 0);
        continue;
      }
      assert(blk.data >= alloc.pages[blk.page].data.get() &&
             blk.data - alloc.pages[blk.page].data.get() <= Alloc::page_size_bytes);
      auto off = blk.data - alloc.pages[blk.page].data.get();
      for (uint64_t k = off; k < off + sz; k++) {
        if (mem_used[k]) {
          return true;
        } else {
          mem_used[k] = true;
        }
      }
    }
  }

  return false;
}

}