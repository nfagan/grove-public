#pragma once

#include "audio_node.hpp"
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace grove {

class AudioGraph {
  friend class AudioGraphRenderer;
  friend class AudioGraphProxy;

public:
  struct Connection {
    OutputAudioPort from;
    InputAudioPort to;
  };

  enum class ConnectionStatus {
    Success = 0,
    ErrorTypeMismatch,
    ErrorWouldCreateCycle,
    ErrorInputAlreadyConnected,
    ErrorOutputAlreadyConnected,
    ErrorOutputNotConnected,
    ErrorInputNotConnected,
    ErrorNodesNotConnected,
  };

  struct ConnectionResult {
  public:
    bool success() const;

  public:
    ConnectionStatus status{};
    std::vector<Connection> connections{};
  };

  friend const char* to_string(ConnectionStatus status);

private:
  using Connections = DynamicArray<Connection, 8>;
  using Graph = std::unordered_map<AudioProcessorNode*, Connections>;
  using GraphNodeSet = std::unordered_set<AudioProcessorNode*>;

public:
  ConnectionResult could_connect(OutputAudioPort output, InputAudioPort input) const;
  ConnectionResult connect(OutputAudioPort output, InputAudioPort to_input);
  ConnectionResult disconnect(OutputAudioPort output);
  ConnectionResult disconnect(InputAudioPort input);
  ConnectionResult disconnect(OutputAudioPort output, InputAudioPort from_input);
  ConnectionResult delete_node(AudioProcessorNode* node);

  const InputAudioPort* maybe_get_connected_input(const OutputAudioPort& to_output) const;
  const OutputAudioPort* maybe_get_connected_output(const InputAudioPort& input) const;
  int count_connected_outputs(const InputAudioPorts& ins) const;

  const GraphNodeSet& get_output_nodes() const {
    return output_nodes;
  }

private:
  ConnectionResult connect_impl(const OutputAudioPort& output,
                                const InputAudioPort& to_input);
  ConnectionResult disconnect_impl(const OutputAudioPort& output);

  bool has_connection_from(const InputAudioPort& input) const;
  bool has_connection_to(const OutputAudioPort& output) const;

  int count_connected_inputs(const OutputAudioPorts& outs) const;
  void sanity_check_node_sets() const;

private:
  bool layout_needs_reevaluation{false};

  std::unordered_map<InputAudioPort, OutputAudioPort, AudioPort::Hash> connected_input_ports;
  std::unordered_map<OutputAudioPort, InputAudioPort, AudioPort::Hash> connected_output_ports;

  GraphNodeSet output_nodes;
  GraphNodeSet input_nodes;

  Graph graph;
};

}