#include "AudioConnectionManager.hpp"
#include "grove/common/common.hpp"
#include "grove/common/vector_util.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/config.hpp"
#include <iostream>

GROVE_NAMESPACE_BEGIN

namespace {

struct ExtractedPortInfo {
  InputAudioPort input_port;
  OutputAudioPort output_port;
};

#ifdef GROVE_DEBUG
template <typename T>
bool contains_connection(const T& connections,
                         const AudioConnectionManager::Connection& connection) {
  auto it = std::find_if(connections.begin(), connections.end(), [connection](auto& connect) {
    AudioConnectionManager::EqConnectionPortOrderIndependent eq{};
    return eq(connection, connect);
  });

  return it != connections.end();
}
#endif

ExtractedPortInfo
extract_audio_processor_node_ports(const AudioNodeStorage::PortInfo& first_info,
                                   AudioProcessorNode* first_node,
                                   const AudioNodeStorage::PortInfo& second_info,
                                   AudioProcessorNode* second_node) {
  ExtractedPortInfo extracted_node_info{};

  if (first_info.descriptor.direction == AudioNodeStorage::PortDirection::Input) {
    assert(second_info.descriptor.direction == AudioNodeStorage::PortDirection::Output);
    auto inputs = first_node->inputs();
    auto outputs = second_node->outputs();

    extracted_node_info.input_port = inputs[first_info.descriptor.index];
    extracted_node_info.output_port = outputs[second_info.descriptor.index];

  } else {
    assert(first_info.descriptor.direction == AudioNodeStorage::PortDirection::Output &&
           second_info.descriptor.direction == AudioNodeStorage::PortDirection::Input);
    auto inputs = second_node->inputs();
    auto outputs = first_node->outputs();

    extracted_node_info.input_port = inputs[second_info.descriptor.index];
    extracted_node_info.output_port = outputs[first_info.descriptor.index];
  }

  return extracted_node_info;
}

void log_graph_connect_error(AudioGraph::ConnectionStatus status) {
#if GROVE_LOGGING_ENABLED
  std::string message{"Graph connect / disconnect failed: "};
  message += to_string(status);
  GROVE_LOG_INFO_CAPTURE_META(message.c_str(), "AudioConnectionManager");
#else
  (void) status;
#endif
}

} //  anon

AudioConnectionManager::AudioConnectionManager(AudioNodeStorage* node_storage,
                                               AudioGraphProxy* graph_proxy) :
  node_storage(node_storage),
  graph_proxy(graph_proxy) {
  //
}

AudioConnectionManager::UpdateResult AudioConnectionManager::update() {
  UpdateResult result{};

  newly_completed_connections.clear();
  newly_completed_disconnections.clear();
  newly_completed_node_deletions.clear();

  update_pending_graph_connection_results();

  for (const auto& connection : completed_connections) {
    push_new_connection(connection);
  }
  for (const auto& disconnection : completed_disconnections) {
    push_new_disconnection(disconnection);
  }
  for (const auto& to_delete : completed_node_deletions) {
    newly_completed_node_deletions.push_back(to_delete);
  }

  completed_connections.clear();
  completed_disconnections.clear();
  completed_node_deletions.clear();

  for (const auto& connection : newly_completed_connections) {
    node_storage->mark_connected(connection.first, connection.second);
    node_storage->mark_connected(connection.second, connection.first);
  }

  for (const auto& disconnection : newly_completed_disconnections) {
    node_storage->unmark_connected(disconnection.first);
    node_storage->unmark_connected(disconnection.second);
  }

  for (const auto& to_delete : newly_completed_node_deletions) {
    node_storage->delete_node(to_delete);
  }

  result.new_connections =
    make_data_array_view<Connection>(newly_completed_connections);
  result.new_disconnections =
    make_data_array_view<Connection>(newly_completed_disconnections);
  result.new_node_deletions =
    make_data_array_view<AudioNodeStorage::NodeID>(newly_completed_node_deletions);

  return result;
}

void AudioConnectionManager::push_new_connection(Connection connection) {
#ifdef GROVE_DEBUG
  assert(!contains_connection(newly_completed_connections, connection));
#endif
  newly_completed_connections.push_back(connection);
}

void AudioConnectionManager::push_new_disconnection(Connection disconnection) {
#ifdef GROVE_DEBUG
  assert(!contains_connection(newly_completed_disconnections, disconnection));
#endif
  newly_completed_disconnections.push_back(disconnection);
}

void AudioConnectionManager::on_graph_connection_success(AudioGraphProxy::PendingResult* res) {
  //
  using Type = AudioGraphProxy::CommandType;

  switch (res->command.type) {
    case Type::Connect: {
      push_new_connection(pending_graph_connections.at(res));
      break;
    }
    case Type::DisconnectPair: {
      push_new_disconnection(pending_graph_connections.at(res));
      break;
    }
    case Type::DeleteNode: {
      auto& node_id = pending_deleted_graph_nodes.at(res);
      auto maybe_port_info = node_storage->get_port_info_for_node(node_id);
      auto& port_info = maybe_port_info.unwrap();

      for (auto& port : port_info) {
        if (!port.connected()) {
          continue;
        }

        if (auto second_port_info = node_storage->get_port_info(port.connected_to)) {
          Connection new_disconnection{port, second_port_info.value()};
          push_new_disconnection(new_disconnection);

          //  @TODO: Remove this inconsistency in where / when ports are actually
          //   marked as disconnected. We need to unmark these as connected because, in the case
          //  that two connected nodes are deleted in the same frame, their disconnections will
          //  be added twice to `newly_completed_disconnections`.
          node_storage->unmark_connected(port);
          node_storage->unmark_connected(second_port_info.value());
        }
      }
      //  The node is ready to be deleted.
      completed_node_deletions.push_back(pending_deleted_graph_nodes.at(res));
      break;
    }
    default: {
      assert(false);
    }
  }
}

void AudioConnectionManager::update_pending_graph_connection_results() {
  DynamicArray<int, 4> erase_inds;

  for (int i = 0; i < pending_graph_connection_results.size(); i++) {
    auto& res = pending_graph_connection_results[i];
    if (!res->is_ready()) {
      continue;
    }

    erase_inds.push_back(i);
    auto res_ptr = res.get();

    if (res->connection_result.success()) {
      on_graph_connection_success(res_ptr);

    } else {
      log_graph_connect_error(res->connection_result.status);
    }

    pending_graph_connections.erase(res_ptr);
    pending_deleted_graph_nodes.erase(res_ptr);
  }

  erase_set(pending_graph_connection_results, erase_inds);
}

/*
 * Connect
 */

AudioConnectionManager::ConnectionResult
AudioConnectionManager::connect_audio_processor_nodes(const PortInfo& first,
                                                      const PortInfo& second) {
  auto first_node_info = node_storage->get_node_info(first.node_id).unwrap();
  auto second_node_info = node_storage->get_node_info(second.node_id).unwrap();

  if (first.descriptor.direction == second.descriptor.direction) {
    return {ConnectionResult::Status::ErrorPortDirectionMismatch};

  } else if (first.node_id == second.node_id) {
    return {ConnectionResult::Status::ErrorWouldCreateCycle};
  }

  node_storage->require_instance(first_node_info);
  node_storage->require_instance(second_node_info);

  auto* first_node = node_storage->get_audio_processor_node_instance(first.node_id);
  auto* second_node = node_storage->get_audio_processor_node_instance(second.node_id);
  assert(first_node && second_node);

  auto extracted_info =
    extract_audio_processor_node_ports(first, first_node, second, second_node);

  auto pending_result = make_pending_audio_graph_connection_result();

  AudioGraphProxy::Command command{};
  command.type = AudioGraphProxy::CommandType::Connect;
  command.input_port = extracted_info.input_port;
  command.output_port = extracted_info.output_port;
  command.pending_result = pending_result;

  pending_result->command = command;
  graph_proxy->push_command(command);

  pending_graph_connections[pending_result] = {first, second};

  return {ConnectionResult::Status::Pending};
}

AudioConnectionManager::ConnectionResult
AudioConnectionManager::maybe_connect(const AudioNodeStorage::PortInfo& first,
                                      const AudioNodeStorage::PortInfo& second) {
  using NodeType = AudioNodeStorage::NodeType;

  if (first.connected() || second.connected()) {
    return {ConnectionResult::Status::ErrorAlreadyConnected};
  }

  auto first_node_info = node_storage->get_node_info(first.node_id).unwrap();
  auto second_node_info = node_storage->get_node_info(second.node_id).unwrap();

  auto first_node_type = first_node_info.type;
  auto second_node_type = second_node_info.type;

  if (first_node_type == NodeType::AudioProcessorNode &&
      second_node_type == NodeType::AudioProcessorNode) {
    return connect_audio_processor_nodes(first, second);

  } else {
    return {ConnectionResult::Status::ErrorNodeTypeMismatch};
  }
}

/*
 * Disconnect
 */

AudioConnectionManager::ConnectionResult
AudioConnectionManager::disconnect_audio_processor_nodes(const PortInfo& first,
                                                         const PortInfo& second) {
  auto first_node_info = node_storage->get_node_info(first.node_id).unwrap();
  auto second_node_info = node_storage->get_node_info(second.node_id).unwrap();

  if (first.descriptor.direction == second.descriptor.direction) {
    return {ConnectionResult::Status::ErrorPortDirectionMismatch};

  } else if (!first_node_info.instance_created || !second_node_info.instance_created) {
    //  Either one of the underlying instances doesn't exist, so it isn't possible
    //  for these two to have been previously connected.
    return {ConnectionResult::Status::ErrorNotYetConnected};
  }

  auto* first_node = node_storage->get_audio_processor_node_instance(first.node_id);
  auto* second_node = node_storage->get_audio_processor_node_instance(second.node_id);
  assert(first_node && second_node);

  auto extracted_node_info =
    extract_audio_processor_node_ports(first, first_node, second, second_node);

  auto pending_result = make_pending_audio_graph_connection_result();

  AudioGraphProxy::Command command{};
  command.type = AudioGraphProxy::CommandType::DisconnectPair;
  command.output_port = extracted_node_info.output_port;
  command.input_port = extracted_node_info.input_port;
  command.pending_result = pending_result;

  pending_result->command = command;
  graph_proxy->push_command(command);

  pending_graph_connections[pending_result] = {first, second};

  return {ConnectionResult::Status::Pending};
}

AudioConnectionManager::ConnectionResult
AudioConnectionManager::maybe_disconnect(const PortInfo& port) {
  if (!port.connected()) {
    return {ConnectionResult::Status::ErrorNotYetConnected};
  }

  auto maybe_connected_to = node_storage->get_port_info(port.connected_to);
  if (!maybe_connected_to) {
    assert(false);
    return {ConnectionResult::Status::ErrorNotYetConnected};
  }

  return maybe_disconnect(port, maybe_connected_to.value());
}

AudioConnectionManager::ConnectionResult
AudioConnectionManager::maybe_disconnect(const PortInfo& first,
                                         const PortInfo& second) {
  using NodeType = AudioNodeStorage::NodeType;

  if (!first.connected() || !second.connected()) {
    return {ConnectionResult::Status::ErrorNotYetConnected};
  }

  auto first_node_info = node_storage->get_node_info(first.node_id).unwrap();
  auto second_node_info = node_storage->get_node_info(second.node_id).unwrap();

  auto first_type = first_node_info.type;
  auto second_type = second_node_info.type;

  if (first_type == NodeType::AudioProcessorNode &&
      second_type == NodeType::AudioProcessorNode) {
    return disconnect_audio_processor_nodes(first, second);

  } else {
    return {ConnectionResult::Status::ErrorNodeTypeMismatch};
  }
}

AudioConnectionManager::ConnectionResult
AudioConnectionManager::maybe_delete_node(AudioNodeStorage::NodeID node_id) {
  auto maybe_node_info = node_storage->get_node_info(node_id);
  if (!maybe_node_info) {
    return {ConnectionResult::Status::ErrorNoSuchNode};
  }

  auto& node_info = maybe_node_info.value();

  if (node_info.type != AudioNodeStorage::NodeType::AudioProcessorNode) {
    //  @TODO: Delete MIDIInstrument and other types of nodes.
    return {ConnectionResult::Status::ErrorNodeTypeMismatch};
  }

  if (node_info.instance_created) {
    auto* node = node_storage->get_audio_processor_node_instance(node_id);
    auto pending_result = make_pending_audio_graph_connection_result();

    AudioGraphProxy::Command command{};
    command.type = AudioGraphProxy::CommandType::DeleteNode;
    command.node = node;
    command.pending_result = pending_result;

    pending_result->command = command;
    graph_proxy->push_command(command);

    pending_deleted_graph_nodes[pending_result] = node_id;

  } else {
    //  If no instance was created, then no connections with the node are possible. However, even
    //  in this case, wait for update() to actually perform the deletion.
    completed_node_deletions.push_back(node_id);
  }

  return {ConnectionResult::Status::Pending};
}

AudioGraphProxy::PendingResult*
AudioConnectionManager::make_pending_audio_graph_connection_result() {
  auto res = std::make_unique<AudioGraphProxy::PendingResult>();
  auto* ptr = res.get();
  pending_graph_connection_results.push_back(std::move(res));
  return ptr;
}

/*
 * util
 */

const char* to_string(AudioConnectionManager::ConnectionResult::Status status) {
  switch (status) {
    case AudioConnectionManager::ConnectionResult::Status::CompletedSuccessfully:
      return "CompletedSuccessfully";
    case AudioConnectionManager::ConnectionResult::Status::Pending:
      return "Pending";
    case AudioConnectionManager::ConnectionResult::Status::ErrorAlreadyConnected:
      return "ErrorAlreadyConnected";
    case AudioConnectionManager::ConnectionResult::Status::ErrorNotYetConnected:
      return "ErrorNotYetConnected";
    case AudioConnectionManager::ConnectionResult::Status::ErrorPortDirectionMismatch:
      return "ErrorPortDirectionMismatch";
    case AudioConnectionManager::ConnectionResult::Status::ErrorUnspecified:
      return "ErrorUnspecified";
    case AudioConnectionManager::ConnectionResult::Status::ErrorNodeTypeMismatch:
      return "ErrorNodeTypeMismatch";
    case AudioConnectionManager::ConnectionResult::Status::ErrorWouldCreateCycle:
      return "ErrorWouldCreateCycle";
    case AudioConnectionManager::ConnectionResult::Status::ErrorNoSuchNode:
      return "ErrorNoSuchNode";
    default:
      return "<unhandled>";
  }
}

GROVE_NAMESPACE_END
