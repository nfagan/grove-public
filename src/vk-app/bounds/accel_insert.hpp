#pragma once

#include "common.hpp"
#include <functional>

namespace grove::bounds {

using PermitIntersection = std::function<bool(const Element*)>;
using MakeElement = std::function<Element(ElementID, const OBB3f&)>;

struct InsertBoundsParams {
  bounds::Accel* accel;
  PermitIntersection permit_intersection;
  MakeElement make_element;
  const OBB3f* bounds;
  bool* inserted; //  size = `num_bounds`
  bounds::ElementID* dst_element_ids; //  size = `num_bounds`; 0 where inserted = false
  int num_bounds;
};

[[nodiscard]] int insert_bounds(const InsertBoundsParams& params);

}