#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/intersect.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/DynamicArray.hpp"
#include "grove/common/ArrayView.hpp"
#include <vector>
#include <array>
#include <cstdint>
#include <cassert>
#include <functional>

#define GROVE_USE_OLD_INSERT_METHOD (0)

namespace grove {

template <typename Data, typename Traits>
class PointOctree {
public:
  static constexpr int max_num_children_per_node = 8;

  using NodeIndex = uint32_t;
  using Float = float;
  using Vec = Vec3<Float>;

  struct Span {
    Vec end() const {
      return begin + size;
    }
    friend inline bool operator==(const Span& a, const Span& b) {
      return a.begin == b.begin && a.size == b.size;
    }
    friend inline bool operator!=(const Span& a, const Span& b) {
      return !(a == b);
    }

    Vec begin{};
    Float size{};
  };

  struct Node {
    bool is_leaf() const {
      return num_children == 0;
    }

    Span span;
    std::array<NodeIndex, max_num_children_per_node> children{};
    uint8_t num_children{};
    Data data;
  };

  using MapFunction = std::function<void(Node*)>;

public:
  PointOctree() noexcept = default;
  PointOctree(Float initial_span_size, Float max_span_size_split) noexcept :
    initial_span_size{initial_span_size}, max_span_size_split{max_span_size_split} {
    //
  }

  bool insert(const Vec& point, Data&& data);
  bool clear(const Vec& point);

  template <typename F>
  size_t clear_if(F&& func);

  const Data* find(const Vec& point);
  void map_over_sphere(const MapFunction& map_func, const Vec& c, Float r);
  void collect_within_sphere(std::vector<Node*>& out, const Vec& c, Float r) {
    map_over_sphere([&out](auto* node) { out.push_back(node); }, c, r);
  }

  ArrayView<const Node> read_nodes() const {
    return make_data_array_view<const Node>(nodes);
  }

  std::size_t num_nodes() const {
    return nodes.size();
  }
  std::size_t count_non_empty() const;
  std::size_t count_non_empty_leaves() const;
  std::size_t count_empty_leaves() const;

  void validate() const;
  static PointOctree<Data, Traits> rebuild_active(PointOctree<Data, Traits>&& old,
                                                  Float init_span_size, Float max_span_size_split);
  static PointOctree<Data, Traits> rebuild_active(PointOctree<Data, Traits>&& old);

private:
  Optional<NodeIndex> find_first_in_span(const NodeIndex* indices,
                                         int num_indices,
                                         const Vec& v) const;

  Optional<NodeIndex> find_node(NodeIndex root_ind, const Vec& p, bool spawn_if_not_found);

  NodeIndex require_node(NodeIndex root_ind, const Vec& p) {
    if (auto ind = find_node(root_ind, p, true)) {
      return ind.value();
    } else {
      assert(false);
      return 0;
    }
  }

  NodeIndex spawn_node(Node& curr, const Vec& p);
  void insert_new_node(Node& parent, const Span& span, Data&& new_data);
  NodeIndex insert_new_node_clear_parent_data(Node& parent, const Span& span, Data&& new_data);
  void insert_root(const Vec& p, Data&& data);

  int num_roots() const {
    return int(roots.size());
  }

  NodeIndex next_node_index() const {
    return NodeIndex(nodes.size());
  }

  static inline Vec bucket_index(Float span_size, const Vec& v) {
    return floor(v / span_size) * span_size;
  }

  static inline Node make_leaf_node(Span span, Data&& data) {
    Node node{};
    node.span = span;
    node.data = std::move(data);
    return node;
  }

private:
  std::vector<NodeIndex> roots;
  std::vector<Node> nodes;

  Float initial_span_size{Float(8.0)};
  Float max_span_size_split{Float(0.5)};
};

/*
 * impl
 */

namespace detail {

template <typename Span, typename Vec>
inline bool in_span(const Span& span, const Vec& v) {
  auto beg = span.begin;
  auto end = span.end();
  return v.x >= beg.x && v.x < end.x &&
         v.y >= beg.y && v.y < end.y &&
         v.z >= beg.z && v.z < end.z;
}

template <typename Span, typename Vec, typename Float>
inline bool span_sphere_intersect(const Span& span, const Vec& c, Float r) {
  Bounds3<Float> aabb{span.begin, span.end()};
  return aabb_sphere_intersect(aabb, c, r);
}

template <typename Float>
inline bool axis_intersect(Float a0, Float a1, Float b0, Float b1) {
  if (a0 <= b0) {
    return a1 > b0;
  } else {
    return b1 > a0;
  }
}

template <typename Span>
inline bool span_span_intersect(const Span& a, const Span& b) {
  const auto a_end = a.end();
  const auto b_end = b.end();

  for (int i = 0; i < 3; i++) {
    if (!axis_intersect(a.begin[i], a_end[i], b.begin[i], b_end[i])) {
      return false;
    }
  }

  return true;
}

template <typename Vec>
inline Vec make_ith_child_span_begin(uint8_t i, const Vec* begs) {
  const uint8_t i0 = i & uint8_t(1);
  const uint8_t i1 = (i >> uint8_t(1)) & uint8_t(1);
  const uint8_t i2 = (i >> uint8_t(2)) & uint8_t(1);
  return Vec{begs[i0].x, begs[i1].y, begs[i2].z};
}

} //  detail

template <typename Data, typename Traits>
std::size_t PointOctree<Data, Traits>::count_non_empty() const {
  std::size_t count{};
  for (auto& node : nodes) {
    if (!Traits::empty(node.data)) {
      count++;
    }
  }
  return count;
}

template <typename Data, typename Traits>
std::size_t PointOctree<Data, Traits>::count_empty_leaves() const {
  std::size_t count{};
  for (auto& node : nodes) {
    if (node.is_leaf() && Traits::empty(node.data)) {
      count++;
    }
  }
  return count;
}

template <typename Data, typename Traits>
std::size_t PointOctree<Data, Traits>::count_non_empty_leaves() const {
  std::size_t count{};
  for (auto& node : nodes) {
    if (node.is_leaf() && !Traits::empty(node.data)) {
      count++;
    }
  }
  return count;
}

template <typename Data, typename Traits>
typename PointOctree<Data, Traits>::NodeIndex
PointOctree<Data, Traits>::insert_new_node_clear_parent_data(Node& parent,
                                                             const Span& span,
                                                             Data&& new_data) {
  auto child = make_leaf_node(span, std::move(new_data));
  Traits::clear(parent.data);
  assert(parent.num_children < max_num_children_per_node);
  auto next_ind = next_node_index();
  parent.children[parent.num_children++] = next_ind;
  nodes.push_back(std::move(child));
  return next_ind;
}

template <typename Data, typename Traits>
void PointOctree<Data, Traits>::insert_new_node(Node& parent, const Span& span, Data&& new_data) {
  auto child = make_leaf_node(span, std::move(new_data));
  assert(parent.num_children < max_num_children_per_node);
  parent.children[parent.num_children++] = next_node_index();
  nodes.push_back(std::move(child));
}

template <typename Data, typename Traits>
void PointOctree<Data, Traits>::insert_root(const Vec& point, Data&& data) {
  auto bucket = bucket_index(initial_span_size, point);
  auto node = make_leaf_node({bucket, initial_span_size}, std::move(data));
  auto new_root_ind = next_node_index();
  nodes.push_back(std::move(node));
  roots.push_back(new_root_ind);
}

template <typename Data, typename Traits>
typename PointOctree<Data, Traits>::NodeIndex
PointOctree<Data, Traits>::spawn_node(Node& curr, const Vec& p) {
  auto new_size = curr.span.size * Float(0.5);
  auto node = make_leaf_node({bucket_index(new_size, p), new_size}, {});
  auto new_ind = next_node_index();

  assert(curr.num_children < max_num_children_per_node);
  curr.children[curr.num_children++] = new_ind;
  nodes.push_back(std::move(node));

  return new_ind;
}

template <typename Data, typename Traits>
Optional<typename PointOctree<Data, Traits>::NodeIndex>
PointOctree<Data, Traits>::find_node(NodeIndex root_ind, const Vec& p, bool spawn_if_not_found) {
  using Res = PointOctree<Data, Traits>::NodeIndex;

  assert(root_ind < nodes.size());
  auto& root = nodes[root_ind];
  if (root.num_children == 0) {
    return Optional<Res>(root_ind);
  }

  auto child_ind = find_first_in_span(root.children.data(), root.num_children, p);
  if (!child_ind) {
    if (spawn_if_not_found) {
      //  Create a node at this location.
      return Optional<Res>(spawn_node(root, p));
    } else {
      return NullOpt{};
    }
  }

  DynamicArray<NodeIndex, 32> node_indices;
  node_indices.push_back(child_ind.value());

  while (!node_indices.empty()) {
    auto ind = node_indices.back();
    node_indices.pop_back();

    assert(ind < nodes.size());
    auto& node = nodes[ind];
    if (node.num_children > 0) {
      if (auto maybe_child = find_first_in_span(node.children.data(), node.num_children, p)) {
        node_indices.push_back(maybe_child.value());

      } else if (spawn_if_not_found) {
        //  Create a node at this location.
        return Optional<Res>(spawn_node(node, p));

      } else {
        return NullOpt{};
      }
    } else {
      assert(node_indices.empty() && detail::in_span(node.span, p));
      return Optional<Res>(ind);
    }
  }

  assert(false);
  return NullOpt{};
}

template <typename Data, typename Traits>
Optional<typename PointOctree<Data, Traits>::NodeIndex>
PointOctree<Data, Traits>::find_first_in_span(const NodeIndex* indices,
                                              int num_indices,
                                              const Vec& v) const {
  using Res = PointOctree<Data, Traits>::NodeIndex;

  for (int i = 0; i < num_indices; i++) {
    auto& node = nodes[indices[i]];
    if (detail::in_span(node.span, v)) {
      return Optional<Res>(indices[i]);
    }
  }

  return NullOpt{};
}

#if !GROVE_USE_OLD_INSERT_METHOD
template <typename Data, typename Traits>
bool PointOctree<Data, Traits>::insert(const Vec& point, Data&& data) {
  auto root_ind = find_first_in_span(roots.data(), num_roots(), point);
  if (!root_ind) {
    //  New root node.
    insert_root(point, std::move(data));
    return true;
  }

  Optional<NodeIndex> maybe_candidate_ind = root_ind;
  while (maybe_candidate_ind) {
    auto candidate_ind = maybe_candidate_ind.value();
    maybe_candidate_ind = NullOpt{};
    auto& candidate = nodes[candidate_ind];

    if (auto child_ind = find_first_in_span(
        candidate.children.data(), candidate.num_children, point)) {
      //  Traverse.
      maybe_candidate_ind = child_ind;
      continue;
    }

    if (candidate.span.size < max_span_size_split) {
      //  Can't subdivide anymore, so either this is an empty leaf node we can fill, or we fail to
      //  insert the point.
      if (Traits::empty(candidate.data)) {
        assert(candidate.is_leaf());
        candidate.data = std::move(data);
        Traits::fill(candidate.data);
        return true;
      } else {
        return false;
      }
    }

    const auto beg0 = candidate.span.begin;
    const auto new_size = candidate.span.size * Float(0.5);
    const auto beg1 = beg0 + new_size;
    const Vec begs[2]{beg0, beg1};

    if (Traits::empty(candidate.data)) {
      //  Add a new leaf node in the missing span.
      for (uint8_t i = 0; i < 8; i++) {
        const Span check_span{detail::make_ith_child_span_begin(i, begs), new_size};
        if (detail::in_span(check_span, point)) {
          insert_new_node(nodes[candidate_ind], check_span, std::move(data));
          return true;
        }
      }
      assert(false);  //  Should always find a span to insert into.
    }

    //  Otherwise, we have to subdivide the current leaf.
    assert(candidate.is_leaf());
    const auto parent_point = Traits::position(candidate.data);
    bool complete = false;

    for (uint8_t i = 0; i < 8; i++) {
      const Span check_span{detail::make_ith_child_span_begin(i, begs), new_size};
      const bool incoming_in_span = detail::in_span(check_span, point);

      if (detail::in_span(check_span, parent_point)) {
        //  Move the parent data to this new leaf. Read from the nodes array to avoid dangling ref.
        auto new_parent_ind = insert_new_node_clear_parent_data(
          nodes[candidate_ind], check_span, std::move(nodes[candidate_ind].data));
        if (incoming_in_span) {
          //  Point maps to same bucket as parent, so we need to keep subdividing.
          maybe_candidate_ind = new_parent_ind;
          break;
        } else if (complete) {
          break;
        }
      }
      if (incoming_in_span) {
        assert(!complete);
        insert_new_node(nodes[candidate_ind], check_span, std::move(data));
        complete = true;
      }
    }

    if (complete) {
      return true;
    }
  }
  //  No candidate nodes found.
  return false;
}
#endif

template <typename Data, typename Traits>
bool PointOctree<Data, Traits>::clear(const Vec& point) {
  if (auto root_ind = find_first_in_span(roots.data(), num_roots(), point)) {
    if (auto node_ind = find_node(root_ind.value(), point, false)) {
      auto& node = nodes[node_ind.value()];
      if (!Traits::empty(node.data) && Traits::position(node.data) == point) {
        Traits::clear(nodes[node_ind.value()].data);
        return true;
      }
    }
  }
  return false;
}

template <typename Data, typename Traits>
template <typename F>
size_t PointOctree<Data, Traits>::clear_if(F&& func) {
  size_t count{};
  for (auto& node : nodes) {
    const Data* nd = &node.data;
    if (!Traits::empty(node.data) && func(nd)) {
      Traits::clear(node.data);
      count++;
    }
  }
  return count;
}

template <typename Data, typename Traits>
const Data* PointOctree<Data, Traits>::find(const Vec& point) {
  if (auto root_ind = find_first_in_span(roots.data(), num_roots(), point)) {
    if (auto node_ind = find_node(root_ind.value(), point, false)) {
      auto& node = nodes[node_ind.value()];
      if (!Traits::empty(node.data) && Traits::position(node.data) == point) {
        return &node.data;
      }
    }
  }
  return nullptr;
}

template <typename Data, typename Traits>
void PointOctree<Data, Traits>::map_over_sphere(const MapFunction& func, const Vec& c, Float r) {
  DynamicArray<NodeIndex, 32> rest_stack;
  for (auto& root_ind : roots) {
    auto& root = nodes[root_ind];
    if (detail::span_sphere_intersect(root.span, c, r)) {
      if (root.num_children == 0) {
        //  Leaf
        func(&root);
      } else {
        for (int i = 0; i < root.num_children; i++) {
          auto child_ind = root.children[i];
          if (detail::span_sphere_intersect(nodes[child_ind].span, c, r)) {
            rest_stack.push_back(child_ind);
          }
        }
      }
    }
  }

  while (!rest_stack.empty()) {
    auto rest_ind = rest_stack.back();
    rest_stack.pop_back();
    auto& node = nodes[rest_ind];
    if (node.num_children == 0) {
      //  Leaf
      func(&node);
    } else {
      for (int i = 0; i < node.num_children; i++) {
        auto child_ind = node.children[i];
        if (detail::span_sphere_intersect(nodes[child_ind].span, c, r)) {
          rest_stack.push_back(child_ind);
        }
      }
    }
  }
}

template <typename Data, typename Traits>
void PointOctree<Data, Traits>::validate() const {
  std::vector<bool> visited(nodes.size());

  for (auto& root : roots) {
    DynamicArray<NodeIndex, 32> pending;
    pending.push_back(root);

    while (!pending.empty()) {
      auto pend = pending.back();
      pending.pop_back();
      const Node& parent = nodes[pend];

      assert(!visited[pend]);
      visited[pend] = true;

      if (!Traits::empty(parent.data)) {
        assert(parent.is_leaf());
        const auto p = Traits::position(parent.data);
        (void) p;
        assert(detail::in_span(parent.span, p));
      }

      for (int i = 0; i < parent.num_children; i++) {
        const Node& child0 = nodes[parent.children[i]];
        assert(detail::span_span_intersect(child0.span, parent.span));
        for (int j = 0; j < parent.num_children; j++) {
          if (i != j) {
            const Node& child1 = nodes[parent.children[j]];
            assert(child0.span != child1.span);
            assert(!detail::span_span_intersect(child0.span, child1.span));
            (void) child0;
            (void) child1;
          }
        }

        pending.push_back(parent.children[i]);
      }
    }
  }

  for (auto v : visited) {
    assert(v);
    (void) v;
  }
}

template <typename Data, typename Traits>
PointOctree<Data, Traits>
PointOctree<Data, Traits>::rebuild_active(PointOctree<Data, Traits>&& old,
                                          Float init_span_size, Float max_span_size_split) {
  PointOctree<Data, Traits> result{init_span_size, max_span_size_split};
  for (auto&& node : old.nodes) {
    if (!Traits::empty(node.data)) {
      assert(node.is_leaf());
      result.insert(Traits::position(node.data), std::move(node.data));
    }
  }
  return result;
}

template <typename Data, typename Traits>
PointOctree<Data, Traits>
PointOctree<Data, Traits>::rebuild_active(PointOctree<Data, Traits>&& old) {
  auto init_span = old.initial_span_size;
  auto max_split = old.max_span_size_split;
  return rebuild_active(std::move(old), init_span, max_split);
}

#if GROVE_USE_OLD_INSERT_METHOD
template <typename Data, typename Traits>
bool PointOctree<Data, Traits>::insert(const Vec& point, Data&& data) {
  if (auto root_ind = find_first_in_span(roots.data(), num_roots(), point)) {
    auto parent_ind = require_node(root_ind.value(), point);

    assert(parent_ind < nodes.size());
    auto parent_span = nodes[parent_ind].span;
    auto parent_size = parent_span.size;
    auto parent_loc = Traits::position(nodes[parent_ind].data);
    auto parent_empty = Traits::empty(nodes[parent_ind].data);

    if (parent_size >= max_span_size_split && !parent_empty) {
      auto beg0 = nodes[parent_ind].span.begin;
      auto new_size = parent_size * Float(0.5);
      auto beg1 = beg0 + new_size;
      bool found_parent_bucket = false;

      for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
          for (int k = 0; k < 2; k++) {
            Vec check{
              i == 0 ? beg0.x : beg1.x,
              j == 0 ? beg0.y : beg1.y,
              k == 0 ? beg0.z : beg1.z
            };
            Span check_span{check, new_size};
            //  Move the parent data to the new node. Read from the nodes array to avoid a dangling
            //  reference.
            auto& parent = nodes[parent_ind];
            auto& parent_data = parent.data;
            bool used_parent_bucket = false;

            if (!found_parent_bucket && detail::in_span(check_span, parent_loc)) {
              insert_new_node_clear_parent_data(parent, check_span, std::move(parent_data));
              used_parent_bucket = true;
              found_parent_bucket = true;
            }

            if (detail::in_span(check_span, point)) {
              if (used_parent_bucket) {
                //  This point maps to the same bucket as the parent.
                return false;
              } else {
//                insert_new_node_clear_parent_data(parent, check_span, std::move(data));
                insert_new_node(parent, check_span, std::move(data));
                return true;
              }
            }
          }
        }
      }

      assert(found_parent_bucket);
      return false;

    } else if (parent_empty) {
      //  Fill the empty parent with this new data.
      nodes[parent_ind].data = std::move(data);
      Traits::fill(nodes[parent_ind].data);
      return true;

    } else {
      return false;
    }
  } else {
    //  New root node.
    auto bucket = bucket_index(initial_span_size, point);
    auto node = make_leaf_node({bucket, initial_span_size}, std::move(data));
    auto new_root_ind = next_node_index();
    nodes.push_back(std::move(node));
    roots.push_back(new_root_ind);
    return true;
  }
}
#endif

}