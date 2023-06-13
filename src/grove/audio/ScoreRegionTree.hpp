#pragma once

#include "types.hpp"
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace grove {

template <typename Data>
struct ScoreRegionTreeDataAllocator {
  Data* acquire_item(uint32_t* item_index) {
    uint32_t ind{};
    if (free_items.empty()) {
      ind = uint32_t(items.size());
      items.emplace_back();
    } else {
      ind = free_items.back();
      free_items.pop_back();
    }
    *item_index = ind;
    return &items[ind];
  }

  void return_item(uint32_t item_index) {
    free_items.push_back(item_index);
  }

  std::vector<Data> items;
  std::vector<uint32_t> free_items;
};

template <int NumIndices>
struct ScoreRegionTreeDataIndexPacket {
  static constexpr uint32_t invalid = uint32_t(~0u);

  int size() const {
    for (int i = 0; i < NumIndices; i++) {
      if (indices[i] == invalid) {
        return i;
      }
    }
    return NumIndices;
  }

  void insert(int at, uint32_t index) {
    assert(at < NumIndices && indices[at] == invalid);
    indices[at] = index;
    if (at + 1 < NumIndices) {
      indices[at+1] = invalid;
    }
  }

  void clear() {
    std::fill(indices, indices + NumIndices, invalid);
    next = invalid;
  }

  void erase(int index, int size) {
    assert(index >= 0 && index < NumIndices);
    assert(size > 0);
    std::rotate(indices + index, indices + index + 1, indices + size);
    indices[size-1] = invalid;
  }

  int find_index(uint32_t ind, int* size) {
    int res{-1};
    for (int i = 0; i < NumIndices; i++) {
      if (indices[i] == ind) {
        res = i;
      } else if (indices[i] == invalid) {
        *size = i;
        return res;
      }
    }
    *size = NumIndices;
    return res;
  }

  uint32_t indices[NumIndices];
  uint32_t next;
};

template <int Ni>
struct ScoreRegionTreeDataIndexAllocator {
  static constexpr uint32_t invalid = uint32_t(~0u);

  uint32_t acquire_index_packet() {
    uint32_t pi;
    if (free_packets.empty()) {
      pi = uint32_t(packets.size());
      packets.emplace_back();
    } else {
      pi = free_packets.back();
      free_packets.pop_back();
    }
    auto& inds = packets[pi];
    inds.indices[0] = invalid;
    inds.next = invalid;
    return pi;
  }

  void free_list(uint32_t list) {
    while (list != invalid) {
      free_packets.push_back(list);
      auto& packet = packets[list];
      const uint32_t next = packet.next;
      packet.clear();
      list = next;
    }
  }

  uint32_t insert_index(uint32_t list, uint32_t index) {
    assert(index != invalid);

    const uint32_t head = list;
    uint32_t parent = invalid;

    while (list != invalid) {
      auto& packet = packets[list];
      const int sz = packet.size();
      if (sz < Ni) {
        packet.insert(sz, index);
        return head;
      } else {
        parent = list;
        list = packet.next;
      }
    }

    const uint32_t next_packet = acquire_index_packet();
    auto& packet = packets[next_packet];
    assert(packet.size() == 0);
    packet.insert(0, index);

    if (parent != invalid) {
      assert(packets[parent].next == invalid);
      packets[parent].next = next_packet;
    }

    if (head == invalid) {
      return next_packet;
    } else {
      return head;
    }
  }

  uint32_t remove_index(uint32_t list, uint32_t index) {
    assert(list != invalid);
    uint32_t parent = invalid;
    const uint32_t head = list;

    while (list != invalid) {
      auto& packet = packets[list];
      int packet_size;
      int ind_ind = packet.find_index(index, &packet_size);
      if (ind_ind == -1) {
        //  not found.
        parent = list;
        list = packet.next;
        continue;
      }

      assert(packet_size > 0);
      packet.erase(ind_ind, packet_size);
      if (--packet_size > 0) {
        return head;
      }

      //  Return to the free list.
      free_packets.push_back(list);
      if (parent != invalid) {
        auto& par = packets[parent];
        par.next = packet.next;
        return head;
      } else {
        return packet.next;
      }
    }

    assert(false);
    return head;
  }

  uint32_t clone(uint32_t list) {
    if (list == invalid) {
      return list;
    }

    const uint32_t head = acquire_index_packet();
    uint32_t dst = head;
    while (true) {
      packets[dst] = packets[list];
      if (packets[list].next != invalid) {
        packets[dst].next = acquire_index_packet();
        list = packets[list].next;
        dst = packets[dst].next;
      } else {
        break;
      }
    }

    return head;
  }

  std::vector<ScoreRegionTreeDataIndexPacket<Ni>> packets;
  std::vector<uint32_t> free_packets;
};

template <typename Data>
struct ScoreRegionTree {
  static constexpr uint32_t no_child = uint32_t(~0u);
  static constexpr double modulus = double(reference_time_signature().numerator);

  struct Node {
    ScoreRegion left_span() const {
      auto res = span;
      res.size.wrapped_scale(0.5, modulus);
      return res;
    }
    ScoreRegion right_span() const {
      auto res = span;
      res.size.wrapped_scale(0.5, modulus);
      res.begin.wrapped_add_cursor(res.size, modulus);
      return res;
    }

    ScoreRegion span;
    uint32_t left;
    uint32_t right;
    uint32_t data_index_list;
  };

  std::vector<Node> nodes;
  ScoreCursor max_span_size_split{1, 0.0};
  uint32_t root{};
  bool odd{};
};

struct ScoreRegionTreeIndexStack {
  std::vector<uint32_t> indices;
};

inline bool push(ScoreRegionTreeIndexStack& stack, uint32_t ind) {
  stack.indices.push_back(ind);
  return true;
}

inline uint32_t pop(ScoreRegionTreeIndexStack& stack) {
  assert(!stack.indices.empty());
  uint32_t ind = stack.indices.back();
  stack.indices.pop_back();
  return ind;
}

inline bool empty(const ScoreRegionTreeIndexStack& stack) {
  return stack.indices.empty();
}

template <int Size>
struct ScoreRegionTreeStaticIndexStack {
  uint32_t indices[Size];
  uint32_t size;
};

template <int Size>
inline bool push(ScoreRegionTreeStaticIndexStack<Size>& stack, uint32_t ind) {
  if (stack.size < Size) {
    stack.indices[stack.size++] = ind;
    return true;
  } else {
    return false;
  }
}

template <int Size>
inline uint32_t pop(ScoreRegionTreeStaticIndexStack<Size>& stack) {
  assert(stack.size > 0);
  return stack.indices[--stack.size];
}

template <int Size>
inline bool empty(const ScoreRegionTreeStaticIndexStack<Size>& stack) {
  return stack.size == 0;
}

template <typename Data>
uint32_t push_node(ScoreRegionTree<Data>* tree, ScoreRegion span) {
  using Node = typename ScoreRegionTree<Data>::Node;
  Node next{};
  next.span = span;
  next.left = ScoreRegionTree<Data>::no_child;
  next.right = ScoreRegionTree<Data>::no_child;
  next.data_index_list = ScoreRegionTree<Data>::no_child;
  const uint32_t ni = uint32_t(tree->nodes.size());
  tree->nodes.push_back(next);
  return ni;
}

template <typename Data>
uint32_t require_root(ScoreRegionTree<Data>* tree, ScoreRegion span) {
  using Node = typename ScoreRegionTree<Data>::Node;
  constexpr double modulus = ScoreRegionTree<Data>::modulus;

  if (tree->nodes.empty()) {
    tree->root = push_node(tree, ScoreRegion{{}, ScoreCursor{1, 0.0}});
    tree->odd = false;
  }

  Node* root = &tree->nodes[tree->root];
  while (union_of(root->span, span, modulus) != root->span) {
    auto new_size = root->span.size;
    new_size.wrapped_scale(2.0, modulus);

    ScoreCursor new_begin = root->span.begin;
    if (!tree->odd) {
      new_begin.wrapped_sub_cursor(root->span.size, modulus);
      uint32_t new_root = push_node(tree, ScoreRegion{new_begin, new_size});
      root = &tree->nodes[new_root];
      root->right = tree->root;
      tree->root = new_root;
    } else {
      uint32_t new_root = push_node(tree, ScoreRegion{new_begin, new_size});
      root = &tree->nodes[new_root];
      root->left = tree->root;
      tree->root = new_root;
    }
    tree->odd = !tree->odd;
  }

  return tree->root;
}

template <typename Tree, typename Data, int Ni, typename TravIf, typename F, typename Stack>
bool map_if(Tree* tree, const TravIf& pred, const F& func,
            ScoreRegionTreeDataIndexAllocator<Ni>* index_alloc,
            ScoreRegionTreeDataAllocator<Data>* data_alloc, Stack& index_stack) {
  constexpr uint32_t no_child = ScoreRegionTree<Data>::no_child;

  if (tree->nodes.empty()) {
    return true;
  }

  if (!push(index_stack, tree->root)) {
    return false;
  }

  while (!empty(index_stack)) {
    const uint32_t ni = pop(index_stack);
    auto* node = &tree->nodes[ni];
    if (!pred(*node)) {
      continue;
    }

    if (node->left != no_child) {
      assert(node->data_index_list == no_child);
      if (!push(index_stack, node->left)) {
        return false;
      }
    }

    if (node->right != no_child) {
      assert(node->data_index_list == no_child);
      if (!push(index_stack, node->right)) {
        return false;
      }
    }

    uint32_t list = node->data_index_list;
    bool proceed = true;
    while (list != no_child && proceed) {
      const ScoreRegionTreeDataIndexPacket<Ni>* packet = &index_alloc->packets[list];
      for (int i = 0; i < Ni; i++) {
        const uint32_t pi = packet->indices[i];
        if (pi == ScoreRegionTreeDataIndexPacket<Ni>::invalid) {
          break;
        }
        const Data& item = data_alloc->items[pi];
        if (func(node, item, pi)) {
          proceed = false;
          break;
        }
      }
      list = packet->next;
    }
  }

  return true;
}

template <typename Tree, typename Data, int Ni, typename TravIf, typename F, typename Stack>
auto test(Tree* tree, const TravIf& pred, const F& func,
          ScoreRegionTreeDataIndexAllocator<Ni>* index_alloc,
          ScoreRegionTreeDataAllocator<Data>* data_alloc, Stack& index_stack) {
  struct Result {
    bool traversed;
    bool result;
  };

  constexpr uint32_t no_child = ScoreRegionTree<Data>::no_child;

  Result result{};
  if (tree->nodes.empty()) {
    result.traversed = true;
    return result;
  }

  if (!push(index_stack, tree->root)) {
    return result;
  }

  while (!empty(index_stack)) {
    const uint32_t ni = pop(index_stack);
    auto* node = &tree->nodes[ni];
    if (!pred(*node)) {
      continue;
    }

    if (node->left != no_child) {
      assert(node->data_index_list == no_child);
      if (!push(index_stack, node->left)) {
        return result;
      }
    }

    if (node->right != no_child) {
      assert(node->data_index_list == no_child);
      if (!push(index_stack, node->right)) {
        return result;
      }
    }

    uint32_t list = node->data_index_list;
    bool proceed = true;
    while (list != no_child && proceed) {
      const ScoreRegionTreeDataIndexPacket<Ni>* packet = &index_alloc->packets[list];
      for (int i = 0; i < Ni; i++) {
        const uint32_t pi = packet->indices[i];
        if (pi == ScoreRegionTreeDataIndexPacket<Ni>::invalid) {
          break;
        }
        const Data& item = data_alloc->items[pi];
        if (func(node, item, pi)) {
          result.traversed = true;
          result.result = true;
          return result;
        }
      }
      list = packet->next;
    }
  }

  result.traversed = true;
  return result;
}

template <typename Tree, typename Data, int Ni, typename F, typename Stack>
bool map_span(Tree* tree, const ScoreRegion& span, const F& func,
              ScoreRegionTreeDataIndexAllocator<Ni>* index_alloc,
              ScoreRegionTreeDataAllocator<Data>* data_alloc, Stack& index_stack) {
  constexpr double modulus = ScoreRegionTree<Data>::modulus;
  auto pred = [&span, modulus](auto& node) { return node.span.intersects(span, modulus); };
  return map_if(tree, pred, func, index_alloc, data_alloc, index_stack);
}

template <typename Tree, typename Data, int Ni, typename F, typename Stack>
bool map_cursor(Tree* tree, ScoreCursor cursor, const F& func,
                ScoreRegionTreeDataIndexAllocator<Ni>* index_alloc,
                ScoreRegionTreeDataAllocator<Data>* data_alloc, Stack& index_stack) {
  constexpr double modulus = ScoreRegionTree<Data>::modulus;
  auto pred = [&cursor, modulus](auto& node) { return node.span.contains(cursor, modulus); };
  return map_if(tree, pred, func, index_alloc, data_alloc, index_stack);
}

template <typename Tree, typename Data, int Ni, typename F, typename Stack>
auto test_cursor(Tree* tree, ScoreCursor cursor, const F& func,
                 ScoreRegionTreeDataIndexAllocator<Ni>* index_alloc,
                 ScoreRegionTreeDataAllocator<Data>* data_alloc, Stack& index_stack) {
  constexpr double modulus = ScoreRegionTree<Data>::modulus;

  auto pred = [&cursor, modulus](auto& node) { return node.span.contains(cursor, modulus); };
  auto f = [&func](auto, const Data& item, uint32_t) { return func(item); };
  return test(tree, pred, f, index_alloc, data_alloc, index_stack);
}

template <typename Tree, typename Data, int Ni, typename F, typename Stack>
auto collect_if(Tree* tree, const ScoreRegion& span, const F& func,
                ScoreRegionTreeDataIndexAllocator<Ni>* index_alloc,
                ScoreRegionTreeDataAllocator<Data>* data_alloc,
                Stack& index_stack, Data* dst, uint32_t max_num_dst) {
  struct Result {
    bool traversed;
    uint32_t num_would_collect;
  };

  using Node = typename ScoreRegionTree<Data>::Node;

  uint32_t num_would_collect{};
  auto f = [&func, index_alloc, data_alloc, dst, max_num_dst, &num_would_collect](
    Node*, const Data& item, uint32_t) {
    //
    if (func(item)) {
      if (num_would_collect < max_num_dst) {
        dst[num_would_collect] = item;
      }
      num_would_collect++;
    }
    return false;
  };

  const bool traversed = map_span(tree, span, f, index_alloc, data_alloc, index_stack);
  Result result{};
  result.traversed = traversed;
  result.num_would_collect = num_would_collect;
  return result;
}

template <typename Tree, typename Data, int Ni, typename F, typename Stack>
auto collect_unique_if(Tree* tree, const ScoreRegion& span, const F& func,
                       ScoreRegionTreeDataIndexAllocator<Ni>* index_alloc,
                       ScoreRegionTreeDataAllocator<Data>* data_alloc,
                       Stack& index_stack, uint32_t* dst, uint32_t max_num_dst) {
  struct Result {
    bool traversed;
    uint32_t num_would_collect;
    uint32_t num_collected;
  };

  uint32_t num_would_collect{};
  auto f = [&func, dst, max_num_dst, &num_would_collect](
    auto, const Data& item, uint32_t item_index) {
    //
    if (func(item)) {
      const uint32_t num_collected = std::min(num_would_collect, max_num_dst);
      auto it = std::find(dst, dst + num_collected, item_index);
      if (it == dst + num_collected) {
        if (num_collected < max_num_dst) {
          dst[num_collected] = item_index;
        }
        num_would_collect++;
      }
    }
    return false;
  };

  const bool traversed = map_span(tree, span, f, index_alloc, data_alloc, index_stack);
  Result result{};
  result.traversed = traversed;
  result.num_would_collect = num_would_collect;
  result.num_collected = std::min(num_would_collect, max_num_dst);
  return result;
}

template <typename Data, int Ni>
void clear_contents(ScoreRegionTree<Data>* tree,
                    ScoreRegionTreeDataIndexAllocator<Ni>* index_alloc,
                    ScoreRegionTreeDataAllocator<Data>* data_alloc) {
  std::unordered_set<uint32_t> unique_indices;

  for (auto& node : tree->nodes) {
    const uint32_t head = node.data_index_list;
    uint32_t list = head;
    while (list != ScoreRegionTreeDataIndexAllocator<Ni>::invalid) {
      const auto* ind_packet = &index_alloc->packets[list];
      const int num_inds = ind_packet->size();
      for (int i = 0; i < num_inds; i++) {
        unique_indices.insert(ind_packet->indices[i]);
      }
      list = index_alloc->packets[list].next;
    }
    index_alloc->free_list(head);
  }

  for (uint32_t el : unique_indices) {
    data_alloc->return_item(el);
  }

  *tree = ScoreRegionTree<Data>{};
}

template <typename Data, int Ni>
ScoreRegionTree<Data> clone(const ScoreRegionTree<Data>* tree,
                            ScoreRegionTreeDataIndexAllocator<Ni>* index_alloc,
                            ScoreRegionTreeDataAllocator<Data>* data_alloc) {
  using Node = typename ScoreRegionTree<Data>::Node;

  auto dst = *tree;
  for (Node& node : dst.nodes) {
    node.data_index_list = index_alloc->clone(node.data_index_list);
  }

  std::unordered_map<uint32_t, uint32_t> remapped_indices;
  for (Node& node : dst.nodes) {
    uint32_t list = node.data_index_list;
    while (list != ScoreRegionTreeDataIndexAllocator<Ni>::invalid) {
      auto* ind_packet = &index_alloc->packets[list];
      const int num_inds = ind_packet->size();
      for (int i = 0; i < num_inds; i++) {
        uint32_t& curr_index = ind_packet->indices[i];
        uint32_t new_index;

        auto remap_it = remapped_indices.find(curr_index);
        if (remap_it == remapped_indices.end()) {
          *data_alloc->acquire_item(&new_index) = data_alloc->items[curr_index];
          remapped_indices[curr_index] = new_index;
        } else {
          new_index = remap_it->second;
        }
        curr_index = new_index;
      }
      list = index_alloc->packets[list].next;
    }
  }

  return dst;
}

template <typename Data, int Ni>
void insert(ScoreRegionTree<Data>* tree, const ScoreRegion& span, Data&& data,
            ScoreRegionTreeDataIndexAllocator<Ni>* index_alloc,
            ScoreRegionTreeDataAllocator<Data>* data_alloc) {
  constexpr double modulus = ScoreRegionTree<Data>::modulus;
  constexpr uint32_t no_child = ScoreRegionTree<Data>::no_child;

  //  Place the data at index `item_index`.
  uint32_t item_index;
  *data_alloc->acquire_item(&item_index) = std::move(data);

  std::vector<uint32_t> node_stack;
  node_stack.push_back(require_root(tree, span));

  while (!node_stack.empty()) {
    const uint32_t ni = node_stack.back();
    node_stack.pop_back();
    auto* node = &tree->nodes[ni];

    if (node->span.size < tree->max_span_size_split) {
      assert(node->left == no_child && node->right == no_child);
      node->data_index_list = index_alloc->insert_index(node->data_index_list, item_index);

    } else {
      auto ls = node->left_span();
      auto rs = node->right_span();

      if (ls.intersects(span, modulus)) {
        if (node->left == no_child) {
          const uint32_t left_ni = push_node(tree, ls);
          node = &tree->nodes[ni];  //  re-acquire pointer
          node->left = left_ni;
        }
        node_stack.push_back(node->left);
      }

      if (rs.intersects(span, modulus)) {
        if (node->right == no_child) {
          const uint32_t right_ni = push_node(tree, rs);
          node = &tree->nodes[ni];  //  re-acquire pointer
          node->right = right_ni;
        }
        node_stack.push_back(node->right);
      }
    }
  }
}

template <typename Data, int Ni, typename F>
bool remove_if(ScoreRegionTree<Data>* tree, const ScoreRegion& span, const F& func,
               ScoreRegionTreeDataIndexAllocator<Ni>* index_alloc,
               ScoreRegionTreeDataAllocator<Data>* data_alloc) {
  using Node = typename ScoreRegionTree<Data>::Node;

  uint32_t dst_item_index = ScoreRegionTree<Data>::no_child;
  auto f = [&func, index_alloc, &dst_item_index](
    Node* node, const Data& item, uint32_t item_index) {
    //
    if (func(item)) {
      dst_item_index = item_index;
      node->data_index_list = index_alloc->remove_index(node->data_index_list, item_index);
      return true;
    } else {
      return false;
    }
  };

  ScoreRegionTreeIndexStack index_stack;
  map_span(tree, span, f, index_alloc, data_alloc, index_stack);

  if (dst_item_index != ScoreRegionTree<Data>::no_child) {
    data_alloc->return_item(dst_item_index);
    return true;
  } else {
    return false;
  }
}

}