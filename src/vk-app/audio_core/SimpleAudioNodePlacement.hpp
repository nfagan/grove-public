#pragma once

#include "AudioNodeStorage.hpp"
#include "../render/SimpleShapeRenderer.hpp"
#include "grove/math/Bounds3.hpp"

namespace grove {

class Camera;
class SelectedInstrumentComponents;
class Terrain;
struct AudioNodeIsolator;

class SimpleAudioNodePlacement {
public:
  struct PortInfo {
    AudioNodeStorage::PortID id{};
    Bounds3f world_bound{};
  };

  enum class NodeOrientation : uint8_t {
    Vertical = 0,
    Horizontal
  };

  struct Node {
    AudioNodeStorage::NodeID id{};
    Vec3f position{};
    float y_offset{};
    NodeOrientation orientation{};
    bool marked_for_deletion{};
    SimpleShapeRenderer::DrawableHandle drawable{};
    int num_instances_reserved{};
    float scale_t{};
  };

  using CreateNodeResult = DynamicArray<PortInfo, 3>;
  using MovedPortResult = std::vector<PortInfo>;

public:
  CreateNodeResult create_node(
    AudioNodeStorage::NodeID node_id,
    const AudioNodeStorage::PortInfoForNode& port_info_for_node,
    const Vec3f& position, float y_offset, NodeOrientation orientation = NodeOrientation::Vertical);

  void delete_node(AudioNodeStorage::NodeID node_id, SimpleShapeRenderer& renderer);

  void update(const AudioNodeStorage& node_storage,
              const AudioNodeIsolator* node_isolator,
              SimpleShapeRenderer& shape_renderer,
              const SimpleShapeRenderer::AddResourceContext& context,
              const SelectedInstrumentComponents& selected_components, double real_dt);

  Bounds3f get_node_bounds(AudioNodeStorage::NodeID node_id,
                           const AudioNodeStorage& node_storage, const Terrain& terrain) const;

  MovedPortResult apply_height_map(const Terrain& terrain,
                                   const AudioNodeStorage& node_storage);
  int num_nodes() const {
    return int(nodes.size());
  }

private:
  void destroy_node(Node& node, SimpleShapeRenderer& renderer);
  bool update_pending_deletion(Node& node, SimpleShapeRenderer& renderer, double real_dt);

private:
  std::vector<Node> nodes;
};

}