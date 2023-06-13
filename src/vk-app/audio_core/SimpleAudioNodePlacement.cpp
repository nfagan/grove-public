#include "SimpleAudioNodePlacement.hpp"
#include "audio_port_placement.hpp"
#include "audio_node_attributes.hpp"
#include "../terrain/terrain.hpp"
#include "grove/math/ease.hpp"
#include "grove/audio/AudioNodeIsolator.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using NodeOrientation = SimpleAudioNodePlacement::NodeOrientation;

Vec3f port_scale(float s = 1.0f) {
  return Vec3f{0.25f} * s;
}

Vec3f input_port_scale(float s = 1.0f) {
//  return port_scale() * Vec3f{0.75f, 1.5f, 1.5f};
  return port_scale(s) * Vec3f{1.5f, 1.5f, 0.75f};
}

Bounds3f make_port_bounds(const Vec3f& position) {
  return {position - port_scale(), position + port_scale()};
}

Vec3f nth_port_position(const Vec3f& base_pos, NodeOrientation orientation, int ind) {
  if (orientation == NodeOrientation::Vertical) {
    return base_pos + Vec3f{0.0f, float(ind), 0.0f};
  } else {
    return base_pos + Vec3f{float(ind), 0.0f, 0.0f};
  }
}

int num_cubes_reserve(const AudioNodeStorage::PortInfoForNode& info) {
  int count{};
  for (auto& port : info) {
    count += port.descriptor.is_input() ? 2 : 1;  //  extra outer cube for input
    count += port.descriptor.is_optional() ? 1 : 0; //  extra cube for optional port
    count += 2; //  extra cube for connected, front and back
  }
  return count;
}

} //  anon

SimpleAudioNodePlacement::CreateNodeResult
SimpleAudioNodePlacement::create_node(AudioNodeStorage::NodeID node_id,
                                      const AudioNodeStorage::PortInfoForNode& src_port_info,
                                      const Vec3f& position,
                                      float y_offset, NodeOrientation orientation) {
  CreateNodeResult result;
  Node node{};
  node.id = node_id;
  node.position = position;
  node.y_offset = y_offset;
  node.orientation = orientation;
  int ind{};
  for (auto& info : src_port_info) {
    auto port_pos = nth_port_position(position, orientation, ind++);
    PortInfo result_info{};
    result_info.id = info.id;
    result_info.world_bound = make_port_bounds(port_pos);
    result.push_back(result_info);
  }
  nodes.push_back(node);
  return result;
}

void SimpleAudioNodePlacement::destroy_node(Node& node, SimpleShapeRenderer& renderer) {
  if (node.drawable.is_valid()) {
    renderer.destroy_instances(node.drawable);
    node.drawable = {};
  }
}

void SimpleAudioNodePlacement::delete_node(AudioNodeStorage::NodeID node_id,
                                           SimpleShapeRenderer& renderer) {
  auto it = std::find_if(nodes.begin(), nodes.end(), [node_id](const Node& node) {
    return node.id == node_id;
  });
  if (it != nodes.end()) {
#if 1
    assert(!it->marked_for_deletion);
    it->marked_for_deletion = true;
    (void) renderer;
#else
    destroy_node(*it, renderer);
    nodes.erase(it);
#endif
  } else {
    assert(false);
  }
}

SimpleAudioNodePlacement::MovedPortResult
SimpleAudioNodePlacement::apply_height_map(const Terrain& terrain,
                                           const AudioNodeStorage& node_storage) {
  MovedPortResult result;
  for (auto& node : nodes) {
    int ind{};
    node.position.y = terrain.height_nearest_position_xz(node.position) + node.y_offset;
    if (auto info = node_storage.get_port_info_for_node(node.id)) {
      for (auto& port : info.value()) {
        PortInfo result_info{};
        result_info.id = port.id;
        result_info.world_bound = make_port_bounds(
          nth_port_position(node.position, node.orientation, ind++));
        result.push_back(result_info);
      }
    }
  }
  return result;
}

Bounds3f SimpleAudioNodePlacement::get_node_bounds(AudioNodeStorage::NodeID node_id,
                                                   const AudioNodeStorage& node_storage,
                                                   const Terrain& terrain) const {
  auto it = std::find_if(nodes.begin(), nodes.end(), [node_id](const Node& node) {
    return node.id == node_id;
  });

  if (it == nodes.end()) {
    assert(false);
    return {};
  }

  const auto& node = *it;
  const auto base_h = terrain.height_nearest_position_xz(node.position) + node.y_offset;
  const auto base_pos = Vec3f{node.position.x, base_h, node.position.z};

  Bounds3f node_bounds{};
  if (auto info = node_storage.get_port_info_for_node(node_id)) {
    for (int i = 0; i < int(info.value().size()); i++) {
      auto port_bounds = make_port_bounds(nth_port_position(base_pos, node.orientation, i));
      node_bounds = union_of(node_bounds, port_bounds);
    }
  } else {
    assert(false);
  }

  return node_bounds;
}

bool SimpleAudioNodePlacement::update_pending_deletion(
  Node& node, SimpleShapeRenderer& renderer, double real_dt) {
  //
  float scale_atten = std::pow(0.75f, float(real_dt / (1.0 / 30.0)));
  node.scale_t = node.scale_t * scale_atten;

  if (node.drawable.is_valid()) {
    renderer.attenuate_active_instance_scales(node.drawable, scale_atten);
  }

  if (node.scale_t < 1e-2f) {
    destroy_node(node, renderer);
    return true;
  } else {
    return false;
  }
}

void SimpleAudioNodePlacement::update(
  const AudioNodeStorage& node_storage, const AudioNodeIsolator* node_isolator,
  SimpleShapeRenderer& shape_renderer,
  const SimpleShapeRenderer::AddResourceContext& context,
  const SelectedInstrumentComponents& selected_components, double real_dt) {
  //
  auto node_it = nodes.begin();
  while (node_it != nodes.end()) {
    auto& node = *node_it;

    if (node.marked_for_deletion) {
      if (update_pending_deletion(node, shape_renderer, real_dt)) {
        node_it = nodes.erase(node_it);
      } else {
        ++node_it;
      }
      continue;
    } else {
      node.scale_t = std::min(1.0f, node.scale_t + float(real_dt * 0.75));
    }

    auto info = node_storage.get_port_info_for_node(node.id);
    if (!info) {
      ++node_it;
      continue;
    }

    if (!node.drawable.is_valid() && !info.value().empty()) {
      if (auto geom = shape_renderer.require_cube(context)) {
        const int num_reserve = num_cubes_reserve(info.value());
        auto drawable = shape_renderer.add_instances(context, geom.value(), num_reserve);
        if (drawable) {
          shape_renderer.add_active_drawable(drawable.value());
          node.drawable = drawable.value();
          node.num_instances_reserved = num_reserve;
        } else {
          assert(false);
        }
      }
    }

    if (node.drawable.is_valid()) {
      const float s = ease::in_out_expo(node.scale_t);
      shape_renderer.clear_active_instances(node.drawable);
      int port_ind{};
      uint32_t cube_ind{};
      for (auto& port : info.value()) {
        const bool selected = selected_components.contains(port.id);
        const auto port_pos = nth_port_position(node.position, node.orientation, port_ind++);
        const auto color =
          color_for_data_type(port.descriptor.data_type) * (selected ? 0.5f : 1.0f);
        //  Main port
        shape_renderer.set_instance_params(node.drawable, cube_ind++, color, port_scale(s), port_pos);
        if (port.descriptor.is_input()) {
          //  Input port
          shape_renderer.set_instance_params(
            node.drawable, cube_ind++, Vec3f{1.0f}, input_port_scale(s), port_pos);
        }

        const bool isolating = ni::ui_is_isolating(node_isolator, node.id, port.descriptor.is_input());
        if (port.connected() || port.descriptor.is_optional() || isolating) {
          //  Additional small indicator inside the port.
          auto scl = port_scale(s) * 0.25f;
          auto pos_front = port_pos + Vec3f{0.0f, 0.0f, port_scale(s).x};
          auto pos_back = port_pos - Vec3f{0.0f, 0.0f, port_scale(s).x};
          auto sub_color = port.connected() ? Vec3f{1.0f} : Vec3f{0.25f, 0.0f, 0.0f};
          if (port.connected() && (selected || selected_components.contains(port.connected_to))) {
            sub_color = Vec3f{1.0f, 0.0f, 0.0f};
          }
          if (isolating) {
            sub_color = color_for_isolating_ports();
          }
          shape_renderer.set_instance_params(node.drawable, cube_ind++, sub_color, scl, pos_front);
          shape_renderer.set_instance_params(node.drawable, cube_ind++, sub_color, scl, pos_back);
        }
      }
    }

    ++node_it;
  }
}

GROVE_NAMESPACE_END
