#pragma once

#include "Vec3.hpp"
#include "Bounds3.hpp"
#include "intersect.hpp"
#include <cstdint>
#include <cmath>
#include <vector>
#include <unordered_set>

namespace grove {

template <typename Data, typename Traits>
class Octree {
public:
  struct Node {
    static Vec3f ith_begin(const Vec3f& beg, const Vec3f& end, uint8_t i) {
      assert(i >= 0 && i < 8);
      auto i0 = uint8_t(1) & uint8_t(i);
      auto i1 = uint8_t(1) & uint8_t(i >> 1u);
      auto i2 = uint8_t(1) & uint8_t(i >> 2u);
      const Vec3f* ptrs[2]{&beg, &end};
      return Vec3f{ptrs[i0]->x, ptrs[i1]->y, ptrs[i2]->z};
    }

    static Bounds3f aabb(const Node& node) {
      return Bounds3f{
        node.begin,
        node.begin + node.size
      };
    }

    static Node create(const Vec3f& beg, float size) {
      Node result{};
      result.begin = beg;
      result.size = size;
      return result;
    }

    void add_child(uint32_t ind) {
      assert(num_children < 8);
      children[num_children++] = ind;
    }

    void push_contents(uint32_t ind) {
      contents.push_back(ind);
    }

    void mark_present(uint8_t i) {
      assert(i >= 0 && i < 8);
      assert(!is_present(i));
      present_children |= uint8_t(1u << i);
    }

    bool is_present(uint8_t i) const {
      assert(i >= 0 && i < 8);
      return present_children & uint8_t(1u << i);
    }

    std::vector<uint32_t> contents;
    uint32_t children[8];
    int8_t num_children;
    Vec3f begin;
    float size;
    uint8_t present_children;
  };

  Octree() = default;
  Octree(float initial_span_size, float max_span_size_split);
  void insert(Data&& data);
  void intersects(const Data& data, std::vector<const Data*>& hit) const;
  void deactivate(const Data& data);
  template <typename F>
  size_t deactivate_if(F&& func);
  void validate() const;
  size_t num_nodes() const {
    return nodes.size();
  }
  size_t num_elements() const {
    return elements.size();
  }
  void num_contents_per_node(uint32_t* out) const;
  size_t num_inactive() const;
  static Octree rebuild_active(Octree&& src, float initial_span_size, float max_span_size_split);

private:
  uint32_t require_root(const Bounds3f& bounds);

  template <typename F, typename Oct>
  static void map_elements(F&& func, Oct&& oct, const Bounds3f& bounds);

private:
  float initial_span_size{};
  float max_span_size_split{};
  std::vector<Node> nodes;
  uint32_t root{};
  std::vector<Data> elements;
  bool odd_expand{true};
};

template <typename Data, typename Traits>
Octree<Data, Traits>::Octree(float initial_span_size, float max_span_size_split) :
  initial_span_size{initial_span_size}, max_span_size_split{max_span_size_split} {
  assert(initial_span_size > 0.0f && max_span_size_split > 0.0f);
  nodes.push_back(Node::create({}, initial_span_size));
}

template <typename Data, typename Traits>
uint32_t Octree<Data, Traits>::require_root(const Bounds3f& bounds) {
  const Node* root_node = &nodes[root];
  auto root_bounds = Node::aabb(*root_node);
  auto union_bounds = union_of(root_bounds, bounds);

  while (union_bounds != root_bounds) {
    const float curr_size = root_node->size;
    const float new_size = curr_size * 2.0f;
    auto new_beg = odd_expand ? root_bounds.min : root_bounds.min - curr_size;
    auto half_new_beg = new_beg + curr_size;
    odd_expand = !odd_expand;

    Node new_root = Node::create(new_beg, new_size);
    new_root.add_child(root);
    for (uint8_t i = 0; i < 8; i++) {
      auto beg = Node::ith_begin(new_beg, half_new_beg, i);
      if (beg == root_node->begin) {
        new_root.mark_present(i);
        break;
      }
    }

    root = uint32_t(nodes.size());
    nodes.push_back(std::move(new_root));
    root_node = &nodes.back();
    root_bounds = Node::aabb(*root_node);
    union_bounds = union_of(root_bounds, bounds);
  }

  return root;
}

template <typename Data, typename Traits>
template <typename F, typename Oct>
void Octree<Data, Traits>::map_elements(F&& func, Oct&& oct, const Bounds3f& data_bounds) {
  std::vector<uint32_t> node_indices{oct.root};
  while (!node_indices.empty()) {
    const uint32_t node_ind = node_indices.back();
    node_indices.pop_back();
    const auto& node = oct.nodes[node_ind];

    bool traverse{};
    for (uint8_t i = 0; i < node.num_children; i++) {
      auto child_bounds = Node::aabb(oct.nodes[node.children[i]]);
      if (aabb_aabb_intersect_closed(child_bounds, data_bounds)) {
        node_indices.push_back(node.children[i]);
        traverse = true;
      }
    }

    if (!traverse) {
      for (uint32_t ci : node.contents) {
        if (!func(oct.elements[ci], ci)) {
          return;
        }
      }
    } else {
      assert(node.contents.empty());
    }
  }
}

template <typename Data, typename Traits>
void Octree<Data, Traits>::insert(Data&& data) {
  Bounds3f data_bounds = Traits::get_aabb(data);
  std::vector<uint32_t> node_indices{require_root(data_bounds)};

  const auto data_ind = uint32_t(elements.size());
  elements.push_back(std::move(data));

  bool did_insert{};
  while (!node_indices.empty()) {
    const uint32_t node_ind = node_indices.back();
    node_indices.pop_back();
    auto* node = &nodes[node_ind];
    const float node_size = node->size;

    bool traverse{};
    for (uint8_t i = 0; i < node->num_children; i++) {
      auto child_bounds = Node::aabb(nodes[node->children[i]]);
      if (aabb_aabb_intersect_closed(child_bounds, data_bounds)) {
        node_indices.push_back(node->children[i]);
        traverse = true;
      }
    }

    if (node_size >= max_span_size_split) {
      auto node_begin = node->begin;
      const float child_size = node_size * 0.5f;
      auto node_half_begin = node_begin + child_size;
      for (uint8_t i = 0; i < 8; i++) {
        if (!node->is_present(i)) {
          Vec3f child_beg = Node::ith_begin(node_begin, node_half_begin, i);
          Bounds3f child_bounds{child_beg, child_beg + child_size};
          if (aabb_aabb_intersect_closed(child_bounds, data_bounds)) {
            node->mark_present(i);
            const auto ni = uint32_t(nodes.size());
            node->add_child(ni);
            nodes.push_back(Node::create(child_beg, child_size));
            node_indices.push_back(ni);
            //  @NOTE: Reevaluate `node` after pushing the new child.
            node = &nodes[node_ind];
            traverse = true;
          }
        }
      }
    }

    if (!traverse) {
      assert(aabb_aabb_intersect_closed(Node::aabb(*node), data_bounds));
      assert(node_size < max_span_size_split);
      node->push_contents(data_ind);
      did_insert = true;
    } else {
      assert(node->contents.empty());
    }
  }

  (void) did_insert;
  assert(did_insert);
}

template <typename Data, typename Traits>
template <typename F>
size_t Octree<Data, Traits>::deactivate_if(F&& func) {
  size_t ct{};
  for (Data& element : elements) {
    const Data* el = &element;
    if (func(el)) {
      Traits::deactivate(element);
      ct++;
    }
  }
  return ct;
}

template <typename Data, typename Traits>
void Octree<Data, Traits>::deactivate(const Data& data) {
  auto f = [&data](Data& el, uint32_t) -> bool {
    if (Traits::equal(el, data)) {
      Traits::deactivate(el);
      return false;
    }
    return true;
  };
  map_elements(std::move(f), *this, Traits::get_aabb(data));
};

#if 1
template <typename Data, typename Traits>
void Octree<Data, Traits>::intersects(const Data& data, std::vector<const Data*>& hit) const {
  std::vector<uint32_t> element_indices;
  auto f = [&data, &element_indices](const Data& el, uint32_t ci) -> bool {
    if (Traits::active(el) && Traits::data_intersect(el, data)) {
      element_indices.push_back(ci);
    }
    return true;
  };
  map_elements(std::move(f), *this, Traits::get_aabb(data));

  std::sort(element_indices.begin(), element_indices.end());
  auto end = std::unique(element_indices.begin(), element_indices.end());
  for (auto it = element_indices.begin(); it != end; ++it) {
    hit.push_back(elements.data() + *it);
  }
}
#else
template <typename Data, typename Traits>
void Octree<Data, Traits>::intersects(const Data& data, std::vector<const Data*>& hit) const {
  const Bounds3f data_bounds = Traits::get_aabb(data);

  std::vector<uint32_t> node_indices{root};
  std::vector<uint32_t> element_indices;

  while (!node_indices.empty()) {
    const uint32_t node_ind = node_indices.back();
    node_indices.pop_back();
    const auto& node = nodes[node_ind];

    bool traverse{};
    for (uint8_t i = 0; i < node.num_children; i++) {
      auto child_bounds = Node::aabb(nodes[node.children[i]]);
      if (aabb_aabb_intersect_closed(child_bounds, data_bounds)) {
        node_indices.push_back(node.children[i]);
        traverse = true;
      }
    }

    if (!traverse) {
      for (uint32_t ci : node.contents) {
        assert(ci < elements.size());
        auto& el = elements[ci];
        if (Traits::active(el) && Traits::data_intersect(el, data)) {
          element_indices.push_back(ci);
        }
      }
    } else {
      assert(node.contents.empty());
    }
  }

  std::sort(element_indices.begin(), element_indices.end());
  auto end = std::unique(element_indices.begin(), element_indices.end());
  for (auto it = element_indices.begin(); it != end; ++it) {
    hit.push_back(elements.data() + *it);
  }
}
#endif

template <typename Data, typename Traits>
void Octree<Data, Traits>::validate() const {
  std::vector<uint32_t> node_indices{root};
  std::unordered_set<uint32_t> visited;
  while (!node_indices.empty()) {
    const uint32_t node_ind = node_indices.back();
    node_indices.pop_back();
    const auto& node = nodes[node_ind];

    assert(!visited.count(node_ind));
    visited.insert(node_ind);

    if (node.num_children > 0) {
      assert(node.contents.empty());
      for (uint8_t i = 0; i < node.num_children; i++) {
        auto& child0 = nodes[node.children[i]];
        assert(aabb_aabb_intersect_half_open(Node::aabb(child0), Node::aabb(node)));
        (void) child0;
        for (uint8_t j = 0; j < node.num_children; j++) {
          if (i != j) {
            auto& child1 = nodes[node.children[j]];
            assert(!aabb_aabb_intersect_half_open(Node::aabb(child0), Node::aabb(child1)));
            (void) child1;
          }
        }
        node_indices.push_back(node.children[i]);
      }
    } else {
      for (uint32_t ci : node.contents) {
        auto& el = elements[ci];
        assert(aabb_aabb_intersect_closed(Traits::get_aabb(el), Node::aabb(node)));
        (void) el;
      }
    }
  }
  assert(visited.size() == nodes.size());
}

template <typename Data, typename Traits>
void Octree<Data, Traits>::num_contents_per_node(uint32_t* out) const {
  for (auto& node : nodes) {
    *out++ = uint32_t(node.contents.size());
  }
}

template <typename Data, typename Traits>
size_t Octree<Data, Traits>::num_inactive() const {
  size_t sum{};
  for (auto& el : elements) {
    if (!Traits::active(el)) {
      sum++;
    }
  }
  return sum;
}

template <typename Data, typename Traits>
Octree<Data, Traits> Octree<Data, Traits>::rebuild_active(Octree<Data, Traits>&& src,
                                                          float initial_span_size,
                                                          float max_span_size_split) {
  Octree<Data, Traits> dst{initial_span_size, max_span_size_split};
  for (auto& el : src.elements) {
    if (Traits::active(el)) {
      dst.insert(std::move(el));
    }
  }
  return dst;
}

}