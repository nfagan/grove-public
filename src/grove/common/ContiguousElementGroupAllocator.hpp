#pragma once

#include <vector>
#include <bitset>

namespace grove {

class ContiguousElementGroupAllocator {
public:
  struct ElementGroupHandle {
    friend inline bool operator==(const ElementGroupHandle& a, const ElementGroupHandle& b) {
      return a.index == b.index;
    }
    friend inline bool operator!=(const ElementGroupHandle& a, const ElementGroupHandle& b) {
      return a.index != b.index;
    }

    uint32_t index;
  };

  static constexpr ElementGroupHandle invalid_element_group = ElementGroupHandle{~0u};

  struct ElementGroup {
    bool available() const;
    bool pending_release() const;
    void set_available(bool v);
    void set_pending_release(bool v);

    uint32_t offset{};
    uint32_t count{};
    std::bitset<8> state{};
  };

  struct Movement {
    void apply(void* data, size_t element_size) const;

    uint32_t dst;
    uint32_t src;
    uint32_t count;
  };

public:
  [[nodiscard]] uint32_t reserve(uint32_t count, ElementGroupHandle* gh);
  void release(ElementGroupHandle gh);
  [[nodiscard]] bool arrange(unsigned char* data, size_t element_size, uint32_t* tail);
  [[nodiscard]] uint32_t arrange_implicit(Movement* movements, uint32_t* tail);
  const ElementGroup* read_group(ElementGroupHandle gh) const;
  uint32_t num_groups() const {
    return uint32_t(groups.size());
  }

  const ElementGroup* read_group_begin() const {
    return groups.data();
  }
  const ElementGroup* read_group_end() const {
    return groups.data() + groups.size();
  }

  static void apply(const Movement* movements, uint32_t num_movements,
                    void* data, size_t element_size);

private:
  std::vector<ElementGroup> groups;
  uint32_t tail{};
};

}