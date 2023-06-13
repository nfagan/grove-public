#include "triangle_search.hpp"
#include "triangle.hpp"
#include "grove/common/common.hpp"
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {

template <typename I>
tri::EdgeToIndex<uint32_t> build_edge_to_index_map(const I* tris, uint32_t num_tris) {
  using Edge = tri::EdgeToIndex<uint32_t>::Edge;
  using Indices = tri::EdgeToIndex<uint32_t>::Indices;

  tri::EdgeToIndex<uint32_t> result;
  for (uint32_t i = 0; i < num_tris; i++) {
    for (int j0 = 0; j0 < 3; j0++) {
      const int j1 = (j0 + 1) % 3;
      Edge edge{tris[i * 3 + j0], tris[i * 3 + j1]};
      auto it = result.map.find(edge);
      if (it == result.map.end()) {
        Indices indices{};
        indices.tis[indices.num_tis++] = i;
        result.map[edge] = indices;
      } else {
        auto& indices = it->second;
        assert(indices.num_tis == 1);
        indices.tis[indices.num_tis++] = i;
      }
    }
  }

  return result;
}

} //  anon

tri::EdgeToIndex<uint32_t> tri::build_edge_to_index_map(const uint16_t* tris, uint32_t num_tris) {
  return grove::build_edge_to_index_map<uint16_t>(tris, num_tris);
}

tri::EdgeToIndex<uint32_t> tri::build_edge_to_index_map(const uint32_t* tris, uint32_t num_tris) {
  return grove::build_edge_to_index_map<uint32_t>(tris, num_tris);
}

tri::EdgeToIndex<uint32_t>::Indices tri::find_ti_with_edge(const EdgeToIndex<uint32_t>& map,
                                                           uint32_t pia, uint32_t pib) {
  tri::EdgeToIndex<uint32_t>::Edge edge{pia, pib};
  auto it = map.map.find(edge);
  if (it == map.map.end()) {
    return {};
  } else {
    return it->second;
  }
}

uint32_t tri::has_edge_order_independent(const EdgeToIndex<uint32_t>& map,
                                         uint32_t pia, uint32_t pib) {
  tri::EdgeToIndex<uint32_t>::Edge edge{pia, pib};
  return map.map.count(edge) > 0;
}

uint32_t tri::find_adjacent(const EdgeToIndex<uint32_t>& map, uint32_t ti, uint32_t ia, uint32_t ib) {
  auto indices = find_ti_with_edge(map, ia, ib);
  for (uint8_t i = 0; i < indices.num_tis; i++) {
    if (indices.tis[i] != ti) {
      return indices.tis[i];
    }
  }
  return tri::no_adjacent_triangle();
}

GROVE_NAMESPACE_END

