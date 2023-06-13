#include "node_bounds.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace bounds {

struct AudioNodeBoundsImpl {
  bounds::ElementTag audio_node_accel_tag{};
  bounds::RadiusLimiterElementTag audio_node_radius_limiter_tag{};
  bool initialized{};
};

} //  bounds

namespace {

struct {
  bounds::AudioNodeBoundsImpl impl;
} globals;

} //  anon

bounds::AudioNodeBoundsImpl* bounds::get_audio_node_bounds_impl() {
  return &globals.impl;
}

bool bounds::insert_audio_node_bounds(
  AudioNodeBoundsImpl* impl, const OBB3f* node_bounds, int num_nodes,
  BoundsSystem* bounds_sys, AccelInstanceHandle accel_handle, RadiusLimiter* radius_limiter,
  bounds::ElementID* dst_inserted_accel_ids,
  bounds::RadiusLimiterElementHandle* dst_inserted_radius_lim_handles) {
  //
  if (!impl->initialized) {
    impl->audio_node_accel_tag = bounds::ElementTag::create();
    impl->audio_node_radius_limiter_tag = bounds::RadiusLimiterElementTag::create();
    impl->initialized = true;
  }

  auto* accel = bounds::request_transient_write(bounds_sys, accel_handle);
  if (!accel) {
    return false;
  }

  for (int i = 0; i < num_nodes; i++) {
    {
      auto el_id = bounds::ElementID::create();
      accel->insert(bounds::make_element(
        node_bounds[i], el_id.id, 0, impl->audio_node_accel_tag.id));
      if (dst_inserted_accel_ids) {
        dst_inserted_accel_ids[i] = el_id;
      }
    }
    {
      auto agg_id = bounds::RadiusLimiterAggregateID::create();
      auto el = bounds::RadiusLimiterElement::create_enclosing_obb3(
        node_bounds[i], agg_id, impl->audio_node_radius_limiter_tag);
      auto handle = bounds::insert(radius_limiter, el, false);
      if (dst_inserted_radius_lim_handles) {
        dst_inserted_radius_lim_handles[i] = handle;
      }
    }
  }

  bounds::release_transient_write(bounds_sys, accel_handle);
  return true;
}

GROVE_NAMESPACE_END
