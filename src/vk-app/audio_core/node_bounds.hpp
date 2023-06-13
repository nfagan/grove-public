#pragma once

#include "../bounds/bounds_system.hpp"
#include "../procedural_tree/radius_limiter.hpp"

namespace grove::bounds {

struct AudioNodeBoundsImpl;

AudioNodeBoundsImpl* get_audio_node_bounds_impl();

bool insert_audio_node_bounds(
  AudioNodeBoundsImpl* impl,
  const OBB3f* node_bounds, int num_bounds,
  BoundsSystem* bounds_sys, AccelInstanceHandle accel_handle, RadiusLimiter* radius_limiter,
  bounds::ElementID* dst_inserted_accel_ids,
  bounds::RadiusLimiterElementHandle* dst_inserted_radius_lim_handles);

}