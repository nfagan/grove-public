#include "UIAudioConnectionManager.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include <iostream>

GROVE_NAMESPACE_BEGIN

namespace {

CablePath make_empty_cable_path(UIAudioConnectionManager& connection_manager) {
  const auto path_id = connection_manager.next_cable_path_id++;
  CablePath new_cable_path;
  new_cable_path.id = path_id;
  return new_cable_path;
}

using UpdateInfo = UIAudioConnectionManager::UpdateInfo;

void update_cable_paths(UIAudioConnectionManager& ui_connection_manager,
                        const UpdateInfo& update_info) {
  ui_connection_manager.new_cable_paths.clear();
  ui_connection_manager.cable_paths_to_remove.clear();

  auto* port_placement = update_info.port_placement;
  auto* path_finder = update_info.cable_path_finder;

  const auto& new_connections = update_info.new_connections;
  const auto& new_disconnections = update_info.new_disconnections;

  for (const auto& connection : new_connections) {
    auto new_cable_path = make_empty_cable_path(ui_connection_manager);

    if (port_placement->has_path_finding_position(connection.first.id) &&
        port_placement->has_path_finding_position(connection.second.id)) {
      //
      auto first_pos =
        port_placement->get_path_finding_position(connection.first.id);
      auto second_pos =
        port_placement->get_path_finding_position(connection.second.id);

      auto p0 = Vec2f(first_pos.x, first_pos.z);
      auto p1 = Vec2f(second_pos.x, second_pos.z);
      auto path_result = path_finder->compute_path(p0, p1);

      if (path_result.success) {
        new_cable_path.positions = std::move(path_result.path_positions);

      } else {
        GROVE_LOG_ERROR_CAPTURE_META("Failed to compute path.", "UIAudioConnectionManager");
      }
    } else {
      GROVE_LOG_WARNING_CAPTURE_META("Can't compute path; no path finding position set.",
                                     "UIAudioConnectionManager");
    }

    ui_connection_manager.connections_to_cable_paths[connection] = new_cable_path.id;
    ui_connection_manager.new_cable_paths.push_back(std::move(new_cable_path));
  }

  for (const auto& disconnection : new_disconnections) {
    assert(ui_connection_manager.connections_to_cable_paths.count(disconnection) > 0);

    const auto path_id = ui_connection_manager.connections_to_cable_paths.at(disconnection);
    ui_connection_manager.connections_to_cable_paths.erase(disconnection);

    ui_connection_manager.cable_paths_to_remove.push_back(path_id);
  }
}

using TwoPorts =
  std::pair<AudioNodeStorage::PortInfo, AudioNodeStorage::PortInfo>;
TwoPorts extract_two_selected_ports(const std::unordered_set<uint32_t>& selected_port_ids,
                                    const AudioNodeStorage* node_storage) {
  assert(selected_port_ids.size() == 2);
  auto it = selected_port_ids.begin();
  auto id0 = *it;
  auto id1 = *(++it);

  auto port0 = node_storage->get_port_info(id0).unwrap();
  auto port1 = node_storage->get_port_info(id1).unwrap();

  return {port0, port1};
}

bool maybe_connect(UIAudioConnectionManager& ui_connection_manager,
                   const UIAudioConnectionManager::UpdateInfo& update_info, bool* did_connect) {
  *did_connect = false;

  auto& selected_port_ids = update_info.selected_components->selected_port_ids;
  if (!ui_connection_manager.update_state.attempt_to_connect || selected_port_ids.size() != 2) {
    return false;
  }

  auto ports = extract_two_selected_ports(selected_port_ids, update_info.node_storage);
  if (ports.first.connected() || ports.second.connected()) {
    return false;
  }

  auto result = update_info.connection_manager->maybe_connect(ports.first, ports.second);
  if (result.had_error()) {
#ifdef GROVE_DEBUG
    std::cout << "Failed to connect: " << to_string(result.status) << std::endl;
#endif
    return true;
  } else {
    *did_connect = true;
  }

  return false;
}

bool maybe_disconnect(UIAudioConnectionManager& ui_connection_manager,
                      const UpdateInfo& update_info, bool* did_disconnect) {
  using Status = AudioConnectionManager::ConnectionResult::Status;

  *did_disconnect = false;

  if (!ui_connection_manager.update_state.attempt_to_disconnect) {
    return false;
  }

  auto port = update_info.node_storage->get_port_info(
    ui_connection_manager.update_state.attempt_to_disconnect.value());
  if (!port) {
    return false;
  }

  AudioConnectionManager::ConnectionResult result{Status::ErrorUnspecified};
  result = update_info.connection_manager->maybe_disconnect(port.value());

  if (result.had_error()) {
#ifdef GROVE_DEBUG
    std::cout << "Failed to disconnect: " << to_string(result.status) << std::endl;
#endif
    return true;
  } else {
    *did_disconnect = true;
  }

  return false;
}

void update_selected_cable_paths(UIAudioConnectionManager& ui_audio_connection_manager,
                                 const UpdateInfo& update_info) {
  ui_audio_connection_manager.selected_cable_paths.clear();

  const auto& selected_ports = update_info.selected_components->selected_port_ids;
  const auto* node_storage = update_info.node_storage;
  const auto& connections = ui_audio_connection_manager.connections_to_cable_paths;

  for (const auto& id : selected_ports) {
    auto maybe_port_info = node_storage->get_port_info(id);
    if (!maybe_port_info) {
      continue;
    }

    auto& port_info = maybe_port_info.value();
    if (!port_info.connected()) {
      continue;
    }

    auto maybe_second_port_info = node_storage->get_port_info(port_info.connected_to);
    if (!maybe_second_port_info) {
      continue;
    }

    AudioConnectionManager::Connection connection{port_info, maybe_second_port_info.value()};
    auto connect_it = connections.find(connection);

    if (connect_it != connections.end()) {
      auto path_id = connect_it->second;
      ui_audio_connection_manager.selected_cable_paths.push_back(path_id);
    }
  }
}

} //  anon namespace

/*
 * UIAudioConnectionManager
 */

void UIAudioConnectionManager::attempt_to_connect() {
  update_state.attempt_to_connect = true;
}

void UIAudioConnectionManager::attempt_to_disconnect(AudioNodeStorage::PortID id) {
  update_state.attempt_to_disconnect = id;
}

UIAudioConnectionManager::UpdateResult
UIAudioConnectionManager::update(const UpdateInfo& update_info) {
  UpdateResult result{};

  update_state.attempt_to_connect = true;

  update_cable_paths(*this, update_info);
  update_selected_cable_paths(*this, update_info);
  bool had_connect_err = maybe_connect(*this, update_info, &result.did_connect);
  bool had_disconnect_err = maybe_disconnect(*this, update_info, &result.did_disconnect);
  update_state.clear();

  result.new_cable_paths = make_data_array_view<CablePath>(new_cable_paths);
  result.cable_paths_to_remove = make_iterator_array_view<uint32_t>(cable_paths_to_remove);
  result.selected_cable_paths = make_data_array_view<uint32_t>(selected_cable_paths);
  result.had_connection_failure = had_connect_err || had_disconnect_err;

  return result;
}

GROVE_NAMESPACE_END
