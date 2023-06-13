#pragma once

#include "grove/math/Octree.hpp"
#include "grove/math/intersect.hpp"
#include "grove/common/identifier.hpp"

namespace grove::bounds {

struct AccessorID {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(AccessorID, id)
  static AccessorID create();
  uint32_t id;
};

struct ElementID {
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(ElementID, id)
  GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, ElementID, id)
  static ElementID create();
  uint32_t id;
};

struct ElementTag {
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(ElementTag, id)
  static ElementTag create();
  uint32_t id;
};

struct Element {
  OBB3f bounds;
  bool inactive;
  uint32_t id;
  uint32_t parent_id;
  uint32_t tag;
};

inline Element make_element(const OBB3f& bounds, uint32_t id, uint32_t parent_id, uint32_t tag) {
  return Element{bounds, false, id, parent_id, tag};
}

inline Element make_query_element(const OBB3f& bounds) {
  Element res{};
  res.bounds = bounds;
  return res;
}

struct ElementTraits {
  static Bounds3f get_aabb(const Element& data) {
    Vec3f vs[8];
    gather_vertices(data.bounds, vs);
    Bounds3f result;
    union_of(vs, 8, &result.min, &result.max);
    return result;
  }

  static bool active(const Element& data) {
    return !data.inactive;
  }

  static bool data_intersect(const Element& a, const Element& b) {
    return obb_obb_intersect(a.bounds, b.bounds);
  }

  static bool equal(const Element& a, const Element& b) {
    return a.id == b.id && a.parent_id == b.parent_id && a.tag == b.tag && a.bounds == b.bounds;
  }

  static void deactivate(Element& data) {
    data.inactive = true;
  }
};

using Accel = Octree<Element, ElementTraits>;

}