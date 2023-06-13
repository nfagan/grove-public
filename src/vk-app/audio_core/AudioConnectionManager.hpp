#pragma once

#include "grove/audio/AudioGraphProxy.hpp"
#include "AudioNodeStorage.hpp"
#include "grove/common/ArrayView.hpp"

namespace grove {

class MIDITrack;
class AudioEffect;

class AudioConnectionManager {
public:
  struct Connection {
    AudioNodeStorage::PortInfo first;
    AudioNodeStorage::PortInfo second;
  };

  struct HashConnection {
    inline std::size_t operator()(const Connection& connection) const noexcept {
      return std::hash<AudioNodeStorage::PortID>{}(connection.first.id) ^
             std::hash<AudioNodeStorage::PortID>{}(connection.second.id);
    }
  };

  struct EqConnectionPortOrderIndependent {
    inline bool operator()(const Connection& a, const Connection& b) const noexcept {
      return (a.first.id == b.first.id && a.second.id == b.second.id) ||
             (a.second.id == b.first.id && a.first.id == b.second.id);
    }
  };

  struct ConnectionResult {
  public:
    enum class Status {
      CompletedSuccessfully = 0,
      Pending,
      ErrorAlreadyConnected,
      ErrorNotYetConnected,
      ErrorNodeTypeMismatch,
      ErrorPortDirectionMismatch,
      ErrorWouldCreateCycle,
      ErrorNoSuchNode,
      ErrorUnspecified
    };

  public:
    bool had_error() const {
      return status != Status::CompletedSuccessfully && status != Status::Pending;
    }

  public:
    Status status;
  };

  using PortInfo = AudioNodeStorage::PortInfo;
  using PendingResult = std::unique_ptr<AudioGraphProxy::PendingResult>;
  using Connections = DynamicArray<Connection, 4>;

  struct UpdateResult {
    bool empty() const {
      return new_connections.empty() && new_disconnections.empty() && new_node_deletions.empty();
    }

    ArrayView<Connection> new_connections{};
    ArrayView<Connection> new_disconnections{};
    ArrayView<AudioNodeStorage::NodeID> new_node_deletions{};
  };

public:
  AudioConnectionManager(AudioNodeStorage* node_storage,
                         AudioGraphProxy* graph_proxy);

  UpdateResult update();

  ConnectionResult maybe_connect(const PortInfo& first, const PortInfo& second);
  ConnectionResult maybe_disconnect(const PortInfo& first, const PortInfo& second);
  ConnectionResult maybe_disconnect(const PortInfo& first);
  ConnectionResult maybe_delete_node(AudioNodeStorage::NodeID node_id);

private:
  ConnectionResult connect_audio_processor_nodes(const PortInfo& first, const PortInfo& second);
  ConnectionResult disconnect_audio_processor_nodes(const PortInfo& first, const PortInfo& second);

  AudioGraphProxy::PendingResult* make_pending_audio_graph_connection_result();
  void update_pending_graph_connection_results();
  void on_graph_connection_success(AudioGraphProxy::PendingResult* result);

  void push_new_connection(Connection connection);
  void push_new_disconnection(Connection disconnection);

private:
  AudioNodeStorage* node_storage;
  AudioGraphProxy* graph_proxy;

  Connections newly_completed_connections;
  Connections newly_completed_disconnections;
  DynamicArray<AudioNodeStorage::NodeID, 2> newly_completed_node_deletions;

  std::vector<Connection> completed_connections;
  std::vector<Connection> completed_disconnections;
  DynamicArray<AudioNodeStorage::NodeID, 2> completed_node_deletions;

  DynamicArray<PendingResult, 4> pending_graph_connection_results;
  std::unordered_map<AudioGraphProxy::PendingResult*, Connection> pending_graph_connections;
  std::unordered_map<AudioGraphProxy::PendingResult*,
                     AudioNodeStorage::NodeID> pending_deleted_graph_nodes;
};

/*
 * util
 */

const char* to_string(AudioConnectionManager::ConnectionResult::Status status);

}