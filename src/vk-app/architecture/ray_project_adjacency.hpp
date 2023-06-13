#pragma once

#include "grove/math/vector.hpp"
#include <vector>

namespace grove::tri {
template <typename T>
struct EdgeToIndex;
}

namespace grove::ray_project {

struct NonAdjacentConnections {
  struct Edge {
    uint32_t i0;
    uint32_t i1;
  };

  struct Key {
    uint32_t ti;
    Edge edge;
  };

  struct Entry {
    Key src;
    Key target;
    Vec2f target_edge_fractional_coordinates;
  };

  struct Iterator {
    const Entry* begin;
    const Entry* end;
  };

  std::vector<Entry> entries;
  bool need_build{};
};

NonAdjacentConnections::Key
make_non_adjacent_connection_key(uint32_t ti, uint32_t pia, uint32_t pib);

NonAdjacentConnections::Entry
make_non_adjacent_connection_entry(const NonAdjacentConnections::Key& src,
                                   const NonAdjacentConnections::Key& target,
                                   const Vec2f& edge_coords);

void push_pending_non_adjacent_connection(NonAdjacentConnections* connections,
                                          const NonAdjacentConnections::Entry& entry);
void build_non_adjacent_connections(NonAdjacentConnections* connections);

NonAdjacentConnections::Iterator
find_non_adjacent_connections(const NonAdjacentConnections* connections,
                              const NonAdjacentConnections::Key& src_key);

void push_axis_aligned_non_adjacent_connections(NonAdjacentConnections* connections,
                                                const uint32_t* i0, uint32_t i0_size,
                                                const uint32_t* i1, uint32_t i1_size,
                                                const tri::EdgeToIndex<uint32_t>& edge_indices,
                                                const void* vertices, uint32_t stride, uint32_t p_off,
                                                float tol, int axis);

}