#include "debug_node_connection_representation.hpp"
#include "../render/render_particles_gpu.hpp"
#include "../procedural_tree/resource_flow_along_nodes.hpp"
#include "../audio_core/audio_port_placement.hpp"
#include "NodeSignalValueSystem.hpp"
#include "grove/math/random.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/ease.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Stopwatch.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace audio::debug;
using UpdateInfo = NodeConnectionReprUpdateInfo;

constexpr int max_num_spiral_handles = 4;

struct DebugConnection {
  AudioNodeStorage::NodeID first_node;
  AudioNodeStorage::PortID first_port;
  AudioNodeStorage::NodeID second_node;
  AudioNodeStorage::PortID second_port;
  tree::ResourceSpiralAroundNodesHandle spiral_handles[max_num_spiral_handles];
  int num_spiral_handles;
};

struct PartiallyConnectedNode {
  Vec3f position{};
  float scale_t{};
  double phase{};
  bool marked{};
};

struct PartiallyConnectedNodes {
  std::unordered_map<AudioNodeStorage::NodeID, PartiallyConnectedNode> nodes;
};

using NodePair = std::pair<uint32_t, uint32_t>;

void draw_nodes_linked_by_selection(const std::vector<NodePair>& pairs, const UpdateInfo& info) {
  for (auto& connect : pairs) {
    auto bounds0 = info.port_placement.get_bounds(connect.first);
    auto bounds1 = info.port_placement.get_bounds(connect.second);
    if (!bounds0 || !bounds1) {
      continue;
    }

    Vec3f c0 = bounds0.value().center();
    Vec3f c1 = bounds1.value().center();

    float ys = 0.125f * 0.5f;

    particle::SegmentedQuadVertexDescriptor vert_descs[6];
    for (auto& desc : vert_descs) {
      desc.min_depth_weight = 0;
      desc.translucency = 0.5f;
      desc.color = Vec3f{1.0f, 0.0f, 0.0f};
    }

    vert_descs[0].position = Vec3f{c0.x, c0.y + ys, c0.z};
    vert_descs[1].position = Vec3f{c0.x, c0.y - ys, c0.z};
    vert_descs[2].position = Vec3f{c1.x, c1.y - ys, c1.z};

    vert_descs[3].position = Vec3f{c1.x, c1.y - ys, c1.z};
    vert_descs[4].position = Vec3f{c1.x, c1.y + ys, c1.z};
    vert_descs[5].position = Vec3f{c0.x, c0.y + ys, c0.z};

    particle::push_segmented_quad_sample_depth_image_particle_vertices(vert_descs, 6);
  }
}

std::vector<NodePair> get_nodes_linked_by_selection(const UpdateInfo& info) {
  std::unordered_set<AudioNodeStorage::NodeID> evaluated;
  std::vector<AudioNodeStorage::NodeID> pend;
  std::vector<std::pair<uint32_t, uint32_t>> result;

  for (AudioNodeStorage::PortID port_id : info.selected.selected_port_ids) {
    auto port_info = info.node_storage.get_port_info(port_id);
    if (!port_info) {
      continue;
    }
    if (evaluated.count(port_info.value().node_id) == 0) {
      evaluated.insert(port_info.value().node_id);
      pend.push_back(port_info.value().node_id);
    }
  }

  while (!pend.empty()) {
    AudioNodeStorage::NodeID node_id = pend.back();
    pend.pop_back();

    auto port_info = info.node_storage.get_port_info_for_node(node_id);
    if (!port_info) {
      continue;
    }

    for (auto& p : port_info.value()) {
      if (p.connected()) {
        auto vis_it = std::find_if(result.begin(), result.end(), [&](const auto& conn) {
          return conn.first == p.id || conn.first == p.connected_to ||
                 conn.second == p.id || conn.second == p.connected_to;
        });

        if (vis_it == result.end()) {
          auto& next = result.emplace_back();
          next.first = p.id;
          next.second = p.connected_to;
        }

        auto sec_info = info.node_storage.get_port_info(p.connected_to);
        if (sec_info && evaluated.count(sec_info.value().node_id) == 0) {
          pend.push_back(sec_info.value().node_id);
          evaluated.insert(sec_info.value().node_id);
        }
      }
    }
  }

  return result;
}

void destroy_connections_upon_disconnection(
  std::vector<DebugConnection>& connections, const UpdateInfo& info) {
  //
  for (auto& conn : info.connect_update_res.new_disconnections) {
    auto it = std::find_if(
      connections.begin(), connections.end(), [&](const auto& connect) {
        return connect.first_port == conn.first.id || connect.first_port == conn.second.id ||
               connect.second_port == conn.first.id || connect.second_port == conn.second.id;
      });
    if (it != connections.end()) {
      for (int i = 0; i < it->num_spiral_handles; i++) {
        tree::destroy_resource_spiral(info.resource_spiral_sys, it->spiral_handles[i]);
      }
      connections.erase(it);
    }
  }
}

void prepare_new_connections(std::vector<DebugConnection>& connections, const UpdateInfo& info) {
  for (auto& connect : info.connect_update_res.new_connections) {
    auto& conn = connections.emplace_back();
    //  Prefer first as output.
    if (connect.first.descriptor.is_output()) {
      conn.first_node = connect.first.node_id;
      conn.first_port = connect.first.id;
      conn.second_node = connect.second.node_id;
      conn.second_port = connect.second.id;
    } else {
      conn.first_node = connect.second.node_id;
      conn.first_port = connect.second.id;
      conn.second_port = connect.first.id;
      conn.second_node = connect.first.node_id;
    }
    conn.num_spiral_handles = 0;
  }
}

void acquire_resource_spirals(std::vector<DebugConnection>& connections, const UpdateInfo& info) {
  for (auto& connect : connections) {
    if (connect.num_spiral_handles == max_num_spiral_handles) {
      continue;
    }

    auto bounds0 = info.port_placement.get_bounds(connect.first_port);
    auto bounds1 = info.port_placement.get_bounds(connect.second_port);
    if (!bounds0 || !bounds1) {
      continue;
    }

    Vec3f c0 = bounds0.value().center();
    Vec3f c1 = bounds1.value().center();

    while (connect.num_spiral_handles < max_num_spiral_handles) {
      tree::ResourceSpiralCylinderNode nodes[2];
      nodes[0].position = c0;
      nodes[0].radius = 0.125f;

      nodes[1].position = c1;
      nodes[1].radius = 0.125f;

      tree::CreateResourceSpiralParams params{};
      params.theta_offset = 0.25f + float(connect.num_spiral_handles) * 0.2f;
      params.linear_color = Vec3<uint8_t>{255, 0, 0};
      params.render_pipeline_index = 1;
      params.global_param_set_index = 1;
      params.scale = 0.75f;
      params.burrows_into_target = true;
      params.non_fixed_parent_origin = true;
      connect.spiral_handles[connect.num_spiral_handles++] =
        tree::create_resource_spiral_around_line_of_cylinders(
          info.resource_spiral_sys, nodes, 2, params);
    }
  }
}

bool is_partially_connected_node(const AudioNodeStorage::PortInfoForNode& infos) {
  for (auto& p : infos) {
    if (!p.connected() && !p.descriptor.is_optional()) {
      return true;
    }
  }
  return false;
}

bool contains_node(const std::vector<AudioNodeStorage::NodeID>& ids, AudioNodeStorage::NodeID id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}

std::vector<AudioNodeStorage::NodeID> get_partially_connected_nodes(
  const std::vector<DebugConnection>& connections, const AudioNodeStorage& node_storage) {
  //
  std::vector<AudioNodeStorage::NodeID> result;

  for (auto& conn : connections) {
    auto port_info0 = node_storage.get_port_info_for_node(conn.first_node);
    if (port_info0 && is_partially_connected_node(port_info0.value())) {
      if (!contains_node(result, conn.first_node)) {
        result.push_back(conn.first_node);
      }
    }

    auto port_info1 = node_storage.get_port_info_for_node(conn.second_node);
    if (port_info1 && is_partially_connected_node(port_info1.value())) {
      if (!contains_node(result, conn.second_node)) {
        result.push_back(conn.second_node);
      }
    }
  }

  return result;
}

particle::CircleQuadInstanceDescriptor make_partially_connected_node_instance_desc(
  const PartiallyConnectedNode& last) {
  //
  const double scl_adjust = std::sin(last.phase) * 0.0625 * 0.5;
  particle::CircleQuadInstanceDescriptor circle_desc{};
  circle_desc.position = last.position;
  circle_desc.color = Vec3f{1.0f, 0.0f, 0.0f};
  circle_desc.translucency = 0.0f;
  circle_desc.scale = (0.125f + float(scl_adjust)) * ease::in_out_expo(last.scale_t);
  return circle_desc;
}

void draw_partially_connected_nodes(
  PartiallyConnectedNodes& last_set,
  const std::vector<AudioNodeStorage::NodeID>& curr_set, double dt, const UpdateInfo& info) {
  //
  const auto scale_t_incr = float(dt * 2.0);

  for (auto& node : curr_set) {
    auto port_infos = info.node_storage.get_port_info_for_node(node);
    if (!port_infos) {
      continue;
    }

    Vec3f centroids{};
    int num_centroids{};
    float max_y{-1.0f};
    for (auto& p : port_infos.value()) {
      auto bounds = info.port_placement.get_bounds(p.id);
      if (!bounds) {
        continue;
      }

      auto cent = bounds.value().center();
      centroids += cent;
      max_y = std::max(cent.y, max_y);
      num_centroids++;
    }

    if (num_centroids == 0) {
      continue;
    }

    centroids /= float(num_centroids);
    const Vec3f p = Vec3f{centroids.x, max_y + 1.0f, centroids.z};

    auto last_it = last_set.nodes.find(node);
    if (last_it == last_set.nodes.end()){
      PartiallyConnectedNode new_node{};
      new_node.phase = urand() * pi();
      last_set.nodes[node] = {};
    }

    auto& last = last_set.nodes.at(node);
    last.scale_t = clamp01(last.scale_t + scale_t_incr);
    last.marked = true;
    last.phase += dt * 12.0;
    last.position = p;

    const auto desc = make_partially_connected_node_instance_desc(last);
    particle::push_circle_quad_sample_depth_instances(&desc, 1);
  }

  auto last_it = last_set.nodes.begin();
  while (last_it != last_set.nodes.end()) {
    if (!last_it->second.marked) {
      last_it->second.scale_t -= scale_t_incr;
      if (last_it->second.scale_t <= 0.0f) {
        last_it = last_set.nodes.erase(last_it);
      } else {
        const auto desc = make_partially_connected_node_instance_desc(last_it->second);
        particle::push_circle_quad_sample_depth_instances(&desc, 1);
        ++last_it;
      }
    } else {
      last_it->second.marked = false;
      ++last_it;
    }
  }
}

#if 0
void style_resource_spirals_according_to_signal(
  std::vector<DebugConnection>& connections, const UpdateInfo& info) {
  //
  for (auto& connect : connections) {
    if (connect.num_spiral_handles != max_num_spiral_handles) {
      continue;
    }

    auto read_value = audio::read_node_signal_value(
      info.node_signal_value_system, connect.first_node);

    if (read_value) {
      for (int i = 0; i < connect.num_spiral_handles; i++) {
        tree::set_resource_spiral_frac_white(
          info.resource_spiral_sys,
          connect.spiral_handles[i],
          read_value.value().value01);
      }
    }
  }
}
#endif

struct {
  std::vector<DebugConnection> connections;
  PartiallyConnectedNodes partially_connected;
  Stopwatch timer;
} globals;

} //  anon

void audio::debug::update_node_connection_representation(const UpdateInfo& info) {
  destroy_connections_upon_disconnection(globals.connections, info);
  prepare_new_connections(globals.connections, info);

  draw_nodes_linked_by_selection(get_nodes_linked_by_selection(info), info);
  acquire_resource_spirals(globals.connections, info);
//  style_resource_spirals_according_to_signal(globals.connections, info);

  const double dt = globals.timer.delta_update().count();
  auto curr_partially_connected = get_partially_connected_nodes(globals.connections, info.node_storage);
  draw_partially_connected_nodes(globals.partially_connected, curr_partially_connected, dt, info);

  tree::set_global_velocity_scale(info.resource_spiral_sys, 1, 6.0f);
  tree::set_global_theta(info.resource_spiral_sys, 1, pif() * 0.25f);
}

GROVE_NAMESPACE_END
