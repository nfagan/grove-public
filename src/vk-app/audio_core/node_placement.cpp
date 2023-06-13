#include "node_placement.hpp"
#include "../terrain/terrain.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

void place_audio_node_in_world(
  AudioNodeStorage::NodeID node, const Vec3f& p, const AudioNodeStorage& node_storage,
  AudioPortPlacement& port_placement, SimpleAudioNodePlacement& node_placement,
  const PlaceAudioNodeInWorldParams& params) {
  //
  auto port_info = node_storage.get_port_info_for_node(node);
  if (!port_info) {
    assert(false);
    return;
  }

  Vec3f pos = p;
  if (params.terrain) {
    pos.y = params.terrain->height_nearest_position_xz(pos) + params.y_offset;
  }

  auto place_res = node_placement.create_node(
    node, port_info.value(), pos, params.y_offset, params.orientation);
  for (auto& info : place_res) {
    port_placement.add_selectable_with_bounds(info.id, info.world_bound);
  }
}

GROVE_NAMESPACE_END
