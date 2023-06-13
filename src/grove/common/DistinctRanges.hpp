#pragma once

#include <vector>
#include <cassert>

namespace grove {

template <typename Int>
struct DistinctRanges {
public:
  struct Range {
    bool empty() const {
      return end <= begin;
    }

    bool intersects(const Range& other) const {
      return (other.begin >= begin && other.begin <= end) ||
             (begin >= other.begin && begin <= other.end);
    }

    Range intersect_with(const Range& other) const {
      return Range{std::max(begin, other.begin), std::min(end, other.end), false};
    }

    bool equals(const Range& other) const {
      return begin == other.begin && end == other.end;
    }

    Int begin;
    Int end;
    bool eraseable;
  };

public:
  bool empty() const {
    return ranges.empty();
  }

  void clear() {
    ranges.clear();
  }
  void push(const Range& range);
  void push(const DistinctRanges<Int>& other);
  void push(Int beg, Int end);
  bool is_valid() const;
  bool contains(const Range& query) const;

public:
  std::vector<Range> ranges;
};

template <typename Int>
bool DistinctRanges<Int>::is_valid() const {
  for (int i = 0; i < int(ranges.size()); i++) {
    if (ranges[i].eraseable) {
      return false;
    }
    for (int j = i + 1; j < int(ranges.size()); j++) {
      if (ranges[i].intersects(ranges[j])) {
        return false;
      }
    }
  }
  for (int i = 0; i < int(ranges.size()) - 1; i++) {
    if (ranges[i].begin >= ranges[i+1].begin) {
      return false;
    }
  }
  return true;
}

template <typename Int>
bool DistinctRanges<Int>::contains(const Range& query) const {
  for (auto& range : ranges) {
    if (range.intersect_with(query).equals(query)) {
      return true;
    }
  }
  return false;
}

template <typename Int>
void DistinctRanges<Int>::push(Int beg, Int end) {
  Range range{};
  range.begin = beg;
  range.end = end;
  push(range);
}

template <typename Int>
void DistinctRanges<Int>::push(const DistinctRanges<Int>& other) {
  for (auto& range : other.ranges) {
    push(range);
  }
}

template <typename Int>
void DistinctRanges<Int>::push(const Range& range) {
  assert(!range.eraseable);

  if (range.empty()) {
    return;
  }

  bool merged{};
  int insert_at{};

  for (int i = 0; i < int(ranges.size()); i++) {
    auto& rng = ranges[i];

    if (rng.intersects(range)) {
      rng.begin = std::min(rng.begin, range.begin);
      rng.end = std::max(rng.end, range.end);

      for (int j = i - 1; j >= 0; j--) {
        auto& prev = ranges[j];
        if (prev.intersects(rng)) {
          rng.begin = std::min(rng.begin, prev.begin);
          prev.eraseable = true;
        } else {
          break;
        }
      }

      for (int j = i + 1; j < int(ranges.size()); j++) {
        auto& next = ranges[j];
        if (next.intersects(rng)) {
          rng.end = std::max(rng.end, next.end);
          next.eraseable = true;
        } else {
          break;
        }
      }

      merged = true;
      break;
    } else if (range.begin > rng.begin) {
      insert_at++;
    }
  }

  if (merged) {
    auto it = ranges.begin();
    while (it != ranges.end()) {
      if (it->eraseable) {
        it = ranges.erase(it);
      } else {
        ++it;
      }
    }
  } else {
    assert(insert_at <= int(ranges.size()));
    ranges.insert(ranges.begin() + insert_at, range);
  }

  assert(is_valid());
  assert(contains(range));
}

}