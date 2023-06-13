#pragma once

#include <unordered_map>

namespace grove::tri {

template <typename T>
struct EdgeToIndex {
  struct Edge {
    T i0;
    T i1;
  };

  struct HashEdge {
    std::size_t operator()(const Edge& edge) const noexcept {
      return std::hash<T>{}(edge.i0) ^ std::hash<T>{}(edge.i1);
    }
  };

  struct EqualEdge {
    bool operator()(const Edge& a, const Edge& b) const noexcept {
      return (a.i0 == b.i0 && a.i1 == b.i1) || (a.i0 == b.i1 && a.i1 == b.i0);
    }
  };

  struct Indices {
    T tis[2];
    uint8_t num_tis;
  };

  std::unordered_map<Edge, Indices, HashEdge, EqualEdge> map;
};

EdgeToIndex<uint32_t> build_edge_to_index_map(const uint16_t* tris, uint32_t num_tris);
EdgeToIndex<uint32_t> build_edge_to_index_map(const uint32_t* tris, uint32_t num_tris);
//  Find triangle adjacent to `ti` by edge with vertex indices `pia` and `pib`
uint32_t find_adjacent(const EdgeToIndex<uint32_t>& map, uint32_t ti, uint32_t pia, uint32_t pib);
uint32_t has_edge_order_independent(const EdgeToIndex<uint32_t>& map, uint32_t pia, uint32_t pib);

EdgeToIndex<uint32_t>::Indices find_ti_with_edge(const EdgeToIndex<uint32_t>& map,
                                                 uint32_t pia, uint32_t pib);

}