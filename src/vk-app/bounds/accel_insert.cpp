#include "accel_insert.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

int bounds::insert_bounds(const InsertBoundsParams& params) {
  std::vector<const bounds::Element*> hit;
  int num_inserted{};

  for (int i = 0; i < params.num_bounds; i++) {
    const auto& obb = params.bounds[i];
    params.accel->intersects(bounds::make_query_element(obb), hit);

    bool accept = true;
    for (const bounds::Element* el : hit) {
      if (!params.permit_intersection(el)) {
        accept = false;
        break;
      }
    }

    bounds::ElementID el_id{};
    if (accept) {
      el_id = bounds::ElementID::create();
      params.accel->insert(params.make_element(el_id, obb));
      num_inserted++;
    }

    params.dst_element_ids[i] = el_id;
    params.inserted[i] = accept;
    hit.clear();
  }

  return num_inserted;
}

GROVE_NAMESPACE_END
