#pragma once

#include "AudioNodeStorage.hpp"
#include "grove/audio/audio_buffer.hpp"
#include "grove/math/Bounds3.hpp"
#include "grove/math/Ray.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace grove {

class AudioPortBounds {
public:
  struct Node {
    uint32_t id{};
    Bounds3f bounds;
  };

public:
  uint32_t add_aabb(const Bounds3f& aabb);
  void remove_aabb(uint32_t id);
  void set_aabb(uint32_t id, const Bounds3f& aabb);
  const Bounds3f& get_aabb(uint32_t id) const;

  bool intersects(const Ray& ray, uint32_t* closest_id);
  uint32_t num_nodes() const {
    return uint32_t(nodes.size());
  }

private:
  std::vector<Node> nodes;
  uint32_t next_id{0};
};

class AudioPortPlacement {
public:
  using SelectableID = uint32_t;

  struct RayIntersectResult {
    bool hit = false;
    AudioNodeStorage::PortID hit_port{};
  };

  struct Stats {
    int num_bounds;
    int num_selectable_ids_to_port_ids;
    int num_port_ids_to_selectable_ids;
    int num_path_finding_positions;
  };

public:
  void remove_port(AudioNodeStorage::PortID port_id);

  void add_selectable(AudioNodeStorage::PortID port_id);
  void remove_selectable(AudioNodeStorage::PortID port_id);
  void add_selectable_with_bounds(AudioNodeStorage::PortID port_id, const Bounds3f& bounds);

  int64_t num_selectables() const {
    return selectable_id_to_port_id.size();
  }
  int64_t num_path_findable() const {
    return path_finding_positions.size();
  }

  void set_bounds(AudioNodeStorage::PortID port_id, const Bounds3f& bounds);
  Optional<Bounds3f> get_bounds(AudioNodeStorage::PortID port_id) const;
  void set_path_finding_position(AudioNodeStorage::PortID port_id, const Vec3f& pos);
  Vec3f get_path_finding_position(AudioNodeStorage::PortID port_id) const;
  bool has_path_finding_position(AudioNodeStorage::PortID port_id) const;

  RayIntersectResult update(const Ray& mouse_ray);

  Stats get_stats() const;

private:
  AudioPortBounds port_bounds;

  std::unordered_map<SelectableID, AudioNodeStorage::PortID> selectable_id_to_port_id;
  std::unordered_map<AudioNodeStorage::PortID, SelectableID> port_id_to_selectable_id;

  std::unordered_map<AudioNodeStorage::PortID, Vec3f> path_finding_positions;
};

/*
 * SelectedInstrumentComponents
 */

class SelectedInstrumentComponents {
private:
  using IntersectResult = AudioPortPlacement::RayIntersectResult;
  using SelectedAudioBuffers = std::unordered_set<AudioBufferHandle, AudioBufferHandle::Hash>;

  struct UpdateResult {
    Optional<AudioNodeStorage::PortID> newly_selected;
    Optional<AudioNodeStorage::PortID> newly_want_disconnect;
  };

public:
  void insert(AudioBufferHandle buffer_handle) {
    selected_audio_buffers.insert(buffer_handle);
  }
  void clear_selected_audio_buffers() {
    selected_audio_buffers.clear();
  }

  void insert(AudioNodeStorage::PortID port) {
    selected_port_ids.insert(port);
  }

  void remove(AudioNodeStorage::PortID port) {
    selected_port_ids.erase(port);
  }

  UpdateResult update(const IntersectResult& intersect_result,
                      bool left_clicked, bool right_clicked, bool command_pressed);

  bool contains(AudioNodeStorage::PortID port) const {
    return selected_port_ids.count(port) > 0;
  }

  bool contains(AudioBufferHandle buffer_handle) const {
    return selected_audio_buffers.count(buffer_handle) > 0;
  }

  const SelectedAudioBuffers& read_selected_audio_buffers() const {
    return selected_audio_buffers;
  }
  Optional<AudioNodeStorage::NodeID>
  first_selected_node_id(const AudioNodeStorage& node_storage) const;

public:
  std::unordered_set<AudioNodeStorage::PortID> selected_port_ids;

private:
  SelectedAudioBuffers selected_audio_buffers;
};

}