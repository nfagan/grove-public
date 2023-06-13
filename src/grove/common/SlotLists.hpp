#pragma once

#include <vector>
#include <cassert>

namespace grove {

template <typename T>
struct SlotListsDefaultNode {
  static constexpr uint32_t invalid = uint32_t(~0u);

  void clear() {
    data = {};
    next = invalid;
    in_use = false;
  }

  template <typename D>
  void insert(D&& d) {
    assert(!in_use);
    data = std::forward<D>(d);
    in_use = true;
  }

  T data;
  uint32_t next;
  bool in_use;
};

template <typename T, size_t Padding>
struct SlotListsPaddedNode {
  static constexpr uint32_t invalid = uint32_t(~0u);

  void clear() {
    data = {};
    next = invalid;
    in_use = false;
  }

  template <typename D>
  void insert(D&& d) {
    assert(!in_use);
    data = std::forward<D>(d);
    in_use = true;
  }

  T data;
  uint32_t next;
  bool in_use;
  unsigned char pad[Padding];
};

template <typename T, typename Node = SlotListsDefaultNode<T>>
class SlotLists {
public:
  static constexpr uint32_t invalid = uint32_t(~0u);

  struct List {
    bool empty() const {
      return head == invalid;
    }

    uint32_t head{invalid};
  };

  struct SequenceIterator {
    T& operator*() {
      assert(lists->nodes[list].in_use);
      return lists->nodes[list].data;
    }

    bool operator==(const SequenceIterator& other) const {
      return list == other.list;
    }

    bool operator!=(const SequenceIterator& other) const {
      return list != other.list;
    }

    SequenceIterator& operator++() {
      parent = list;
      list = lists->nodes[list].next;
      return *this;
    }

    SlotLists* lists;
    uint32_t list;
    uint32_t parent;
  };

  struct ConstSequenceIterator {
    const T& operator*() const {
      assert(lists->nodes[list].in_use);
      return lists->nodes[list].data;
    }

    bool operator!=(const ConstSequenceIterator& other) const {
      return list != other.list;
    }

    bool operator==(const ConstSequenceIterator& other) const {
      return list == other.list;
    }

    ConstSequenceIterator& operator++() {
      parent = list;
      list = lists->nodes[list].next;
      return *this;
    }

    const SlotLists* lists;
    uint32_t list;
    uint32_t parent;
  };

public:
  SequenceIterator begin(List head) {
    SequenceIterator result{};
    result.lists = this;
    result.list = head.head;
    result.parent = invalid;
    return result;
  }
  SequenceIterator end() {
    SequenceIterator result{};
    result.lists = this;
    result.list = invalid;
    result.parent = invalid;
    return result;
  }
  ConstSequenceIterator cbegin(List head) const {
    ConstSequenceIterator result{};
    result.lists = this;
    result.list = head.head;
    result.parent = invalid;
    return result;
  }
  ConstSequenceIterator cend() const {
    ConstSequenceIterator result{};
    result.lists = this;
    result.list = invalid;
    result.parent = invalid;
    return result;
  }

  [[nodiscard]] List free_list(List head_node) {
    uint32_t list = head_node.head;
    while (list != invalid) {
      free_nodes.push_back(list);
      auto& node = nodes[list];
      const uint32_t next = node.next;
      node.clear();
      list = next;
    }
    return List{invalid};
  }

  template <typename D>
  [[nodiscard]] List insert(List head_node, D&& data) {
    uint32_t list = head_node.head;
    const uint32_t head = list;
    uint32_t parent = invalid;

    while (list != invalid) {
      auto& node = nodes[list];
      if (!node.in_use) {
        node.insert(std::forward<D>(data));
        return List{head};
      } else {
        parent = list;
        list = node.next;
      }
    }

    const uint32_t next_node = acquire_node();
    auto& node = nodes[next_node];
    assert(!node.in_use);
    node.insert(std::forward<D>(data));

    if (parent != invalid) {
      assert(nodes[parent].next == invalid);
      nodes[parent].next = next_node;
    }

    if (head == invalid) {
      return List{next_node};
    } else {
      return List{head};
    }
  }

  template <typename It>
  It erase(List* head_node, It it) {
    assert(it.list != invalid);
    auto& node = nodes[it.list];
    const uint32_t next = node.next;

    free_nodes.push_back(it.list);
    if (it.parent != invalid) {
      auto& par = nodes[it.parent];
      par.next = next;
    } else {
      *head_node = List{next};
    }

    it.list = next;
    return it;
  }

  [[nodiscard]] List clone(List head_node) {
    uint32_t list = head_node.head;
    if (list == invalid) {
      return List{list};
    }

    const uint32_t head = acquire_node();
    uint32_t dst = head;
    while (true) {
      nodes[dst] = nodes[list];
      if (nodes[list].next != invalid) {
        nodes[dst].next = acquire_node();
        list = nodes[list].next;
        dst = nodes[dst].next;
      } else {
        break;
      }
    }

    return List{head};
  }

  uint32_t size(List list) const {
    auto it = cbegin(list);
    uint32_t s{};
    for (; it != cend(); ++it) {
      s++;
    }
    return s;
  }

  uint32_t num_free_nodes() const {
    return uint32_t(free_nodes.size());
  }

  uint32_t num_nodes() const {
    return uint32_t(nodes.size());
  }

  const Node* read_node_begin() const {
    return nodes.data();
  }
  const Node* read_node_end() const {
    return nodes.data() + nodes.size();
  }

private:
  uint32_t acquire_node() {
    uint32_t ni;
    if (free_nodes.empty()) {
      ni = uint32_t(nodes.size());
      nodes.emplace_back();
    } else {
      ni = free_nodes.back();
      free_nodes.pop_back();
    }
    auto& node = nodes[ni];
    node.next = invalid;
    node.in_use = false;
    return ni;
  }

private:
  std::vector<Node> nodes;
  std::vector<uint32_t> free_nodes;
};

}