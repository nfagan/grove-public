#include "audio_port_placement.hpp"
#include "grove/common/common.hpp"
#include "grove/math/intersect.hpp"
#include "grove/math/constants.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

[[maybe_unused]]
bool less_by_id(const AudioPortBounds::Node& a, const AudioPortBounds::Node& b) {
  return a.id < b.id;
}

template <typename T>
auto maybe_find_node(T&& nodes, uint32_t by_id) {
  return std::lower_bound(nodes.begin(), nodes.end(), by_id, [](auto& a, auto& b) {
    return a.id < b;
  });
}

} //  anon

uint32_t AudioPortBounds::add_aabb(const Bounds3f& aabb) {
  uint32_t id = next_id++;
  nodes.push_back({id, aabb});
  return id;
}

void AudioPortBounds::remove_aabb(uint32_t id) {
  assert(!nodes.empty());
  auto it = maybe_find_node(nodes, id);
  assert(it != nodes.end() && it->id == id);
  nodes.erase(it);
  assert(std::is_sorted(nodes.begin(), nodes.end(), less_by_id));
}

void AudioPortBounds::set_aabb(uint32_t id, const Bounds3f& aabb) {
  auto it = maybe_find_node(nodes, id);
  assert(it != nodes.end() && it->id == id);
  it->bounds = aabb;
}

const Bounds3f& AudioPortBounds::get_aabb(uint32_t id) const {
  auto it = maybe_find_node(nodes, id);
  assert(it != nodes.end() && it->id == id);
  return it->bounds;
}

bool AudioPortBounds::intersects(const Ray& ray, uint32_t* closest_id) {
  float min_t0 = grove::infinityf();
  uint32_t closest;
  bool any_intersect = false;

  for (const auto& node : nodes) {
    float t0;
    float t1;

    if (ray_aabb_intersect(ray, node.bounds, &t0, &t1)) {
      if (t0 < min_t0) {
        min_t0 = t0;
        closest = node.id;
        any_intersect = true;
      }
    }
  }

  if (any_intersect) {
    *closest_id = closest;
  }

  return any_intersect;
}

void AudioPortPlacement::remove_port(AudioNodeStorage::PortID port_id) {
  remove_selectable(port_id);
  path_finding_positions.erase(port_id);
}

void AudioPortPlacement::add_selectable(AudioNodeStorage::PortID port_id) {
  Bounds3f tmp_bounds;
  auto id = port_bounds.add_aabb(tmp_bounds);
  selectable_id_to_port_id[id] = port_id;
  port_id_to_selectable_id[port_id] = id;
}

void AudioPortPlacement::add_selectable_with_bounds(AudioNodeStorage::PortID port_id,
                                                    const Bounds3f& bounds) {
  add_selectable(port_id);
  set_bounds(port_id, bounds);
  set_path_finding_position(port_id, bounds.center());
}

void AudioPortPlacement::remove_selectable(AudioNodeStorage::PortID port_id) {
  assert(port_id_to_selectable_id.count(port_id) > 0);
  auto selectable_id = port_id_to_selectable_id.at(port_id);
  port_id_to_selectable_id.erase(port_id);
  selectable_id_to_port_id.erase(selectable_id);
  port_bounds.remove_aabb(selectable_id);
}

void AudioPortPlacement::set_bounds(AudioNodeStorage::PortID port_id, const Bounds3f& bounds) {
  auto selectable_id = port_id_to_selectable_id.at(port_id);
  port_bounds.set_aabb(selectable_id, bounds);
}

Optional<Bounds3f> AudioPortPlacement::get_bounds(AudioNodeStorage::PortID port_id) const {
  auto it = port_id_to_selectable_id.find(port_id);
  if (it != port_id_to_selectable_id.end()) {
    return Optional<Bounds3f>(port_bounds.get_aabb(it->second));
  } else {
    return NullOpt{};
  }
}

void AudioPortPlacement::set_path_finding_position(AudioNodeStorage::PortID port_id,
                                                   const Vec3f& pos) {
  path_finding_positions[port_id] = pos;
}

Vec3f AudioPortPlacement::get_path_finding_position(AudioNodeStorage::PortID port_id) const {
  return path_finding_positions.at(port_id);
}

bool AudioPortPlacement::has_path_finding_position(AudioNodeStorage::PortID port_id) const {
  return path_finding_positions.count(port_id) > 0;
}

AudioPortPlacement::RayIntersectResult AudioPortPlacement::update(const Ray& mouse_ray) {
  uint32_t hit_id;
  bool any_intersects = port_bounds.intersects(mouse_ray, &hit_id);

  RayIntersectResult result{};

  if (any_intersects) {
    result.hit_port = selectable_id_to_port_id.at(hit_id);
    result.hit = true;
  }

  return result;
}

AudioPortPlacement::Stats AudioPortPlacement::get_stats() const {
  AudioPortPlacement::Stats result{};
  result.num_bounds = int(port_bounds.num_nodes());
  result.num_selectable_ids_to_port_ids = int(selectable_id_to_port_id.size());
  result.num_port_ids_to_selectable_ids = int(port_id_to_selectable_id.size());
  result.num_path_finding_positions = int(path_finding_positions.size());
  return result;
}

SelectedInstrumentComponents::UpdateResult
SelectedInstrumentComponents::update(
  const IntersectResult& intersect_result, bool left_clicked, bool right_clicked, bool command_pressed) {
  UpdateResult result{};

  if (right_clicked) {
    if (intersect_result.hit) {
      selected_port_ids.erase(intersect_result.hit_port);
      result.newly_want_disconnect = intersect_result.hit_port;
    }
    return result;
  }

  if (!left_clicked) {
    return result;
  }

  if (!command_pressed) {
    selected_port_ids.clear();
  }

  if (intersect_result.hit) {
    selected_port_ids.insert(intersect_result.hit_port);
    result.newly_selected = intersect_result.hit_port;
  }

  return result;
}

Optional<AudioNodeStorage::NodeID>
SelectedInstrumentComponents::first_selected_node_id(const AudioNodeStorage& node_storage) const {
  if (!selected_port_ids.empty()) {
    const AudioNodeStorage::PortID first_port = *selected_port_ids.begin();
    if (auto port_info = node_storage.get_port_info(first_port)) {
      return Optional<AudioNodeStorage::NodeID>(port_info.value().node_id);
    }
  }
  return NullOpt{};
}

GROVE_NAMESPACE_END
