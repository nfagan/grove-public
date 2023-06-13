#include "AudioGraph.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

bool AudioGraph::ConnectionResult::success() const {
  return status == ConnectionStatus::Success;
}

const char* to_string(AudioGraph::ConnectionStatus status) {
  switch (status) {
    case AudioGraph::ConnectionStatus::Success:
      return "Success";
    case AudioGraph::ConnectionStatus::ErrorTypeMismatch:
      return "ErrorTypeMismatch";
    case AudioGraph::ConnectionStatus::ErrorWouldCreateCycle:
      return "ErrorWouldCreateCycle";
    case AudioGraph::ConnectionStatus::ErrorInputAlreadyConnected:
      return "ErrorInputAlreadyConnected";
    case AudioGraph::ConnectionStatus::ErrorOutputAlreadyConnected:
      return "ErrorOutputAlreadyConnected";
    case AudioGraph::ConnectionStatus::ErrorOutputNotConnected:
      return "ErrorOutputNotConnected";
    case AudioGraph::ConnectionStatus::ErrorInputNotConnected:
      return "ErrorInputNotConnected";
    case AudioGraph::ConnectionStatus::ErrorNodesNotConnected:
      return "ErrorNodesNotConnected";
    default:
      assert(false);
      return "";
  }
}

AudioGraph::ConnectionResult AudioGraph::connect(OutputAudioPort output, InputAudioPort to_input) {
  auto res = connect_impl(output, to_input);
  return res;
}

AudioGraph::ConnectionResult AudioGraph::disconnect(InputAudioPort input) {
  auto* maybe_output = maybe_get_connected_output(input);

  if (maybe_output) {
    return disconnect(*maybe_output);
  } else {
    return {ConnectionStatus::ErrorInputNotConnected};
  }
}

AudioGraph::ConnectionResult AudioGraph::disconnect(OutputAudioPort output) {
  auto res = disconnect_impl(output);
  return res;
}

AudioGraph::ConnectionResult
AudioGraph::disconnect(OutputAudioPort output, InputAudioPort from_input) {
  auto maybe_input = maybe_get_connected_input(output);
  if (!maybe_input || *maybe_input != from_input) {
    return {ConnectionStatus::ErrorNodesNotConnected};
  } else {
    return disconnect(output);
  }
}

AudioGraph::ConnectionResult AudioGraph::delete_node(AudioProcessorNode* node) {
  ConnectionResult result{};

  auto outs = node->outputs();
  for (const auto& out : outs) {
    auto res = disconnect(out);
    if (res.success()) {
      for (auto& connect : res.connections) {
        result.connections.push_back(connect);
      }
    }
  }

  auto ins = node->inputs();
  for (const auto& in : ins) {
    auto* maybe_out = maybe_get_connected_output(in);
    if (maybe_out) {
      auto res = disconnect(*maybe_out);

      if (res.success()) {
        for (auto& connect : res.connections) {
          result.connections.push_back(connect);
        }
      }
    }
  }

  return result;
}

AudioGraph::ConnectionResult AudioGraph::disconnect_impl(const OutputAudioPort& output) {
  using Status = AudioGraph::ConnectionStatus;

  auto output_node = output.parent_node;
  assert(output_node);

  if (!has_connection_to(output)) {
    return {Status::ErrorOutputNotConnected};
  }

  auto graph_it = graph.find(output_node);
  assert(graph_it != graph.end());
  auto& connections = graph_it->second;
  assert(!connections.empty());

  //  Erase the connection from output -> some input port.
  for (int i = 0; i < connections.size(); i++) {
    if (output == connections[i].from) {
      connections.erase(connections.begin() + i);
      break;
    }
  }

  //  Check whether this was the last connection for the output node. If it was, remove the node
  //  from the graph.
  if (connections.empty()) {
    graph.erase(graph_it);

    if (output_nodes.count(output_node) > 0) {
      output_nodes.erase(output_node);
    }
  }

  auto output_port_it = connected_output_ports.find(output);
  assert(output_port_it != connected_output_ports.end());

  auto connected_input = output_port_it->second;
  auto input_port_it = connected_input_ports.find(connected_input);
  assert(input_port_it != connected_input_ports.end());

  auto* connected_input_node = connected_input.parent_node;
  assert(connected_input_node != output_node);

  //  Eliminate the explicit connection between output and input ports.
  connected_output_ports.erase(output_port_it);
  connected_input_ports.erase(input_port_it);

  //  For the node previously connected to this output port, check whether the node has
  //  any additional input ports that are connected by some output port. If not, and if
  //  this input node was previously in the `input_node` set, remove it from the set.
  auto ins = connected_input_node->inputs();
  bool has_other_connected_input = false;
  bool all_inputs_were_optional = true;

  for (const auto& in : ins) {
    if (maybe_get_connected_output(in)) {
      has_other_connected_input = true;
      break;

    } else if (!in.is_optional()) {
      all_inputs_were_optional = false;
    }
  }

  if (!has_other_connected_input &&
      input_nodes.count(connected_input_node) > 0) {
    input_nodes.erase(connected_input_node);

    if (all_inputs_were_optional) {
      //  Check whether to re-classify the previously connected input node as a pure output node.
      auto outs = connected_input_node->outputs();

      for (auto& out : outs) {
        if (maybe_get_connected_input(out)) {
          output_nodes.insert(connected_input_node);
          break;
        }
      }
    }
  }

  layout_needs_reevaluation = true;

  ConnectionResult result{};
  result.connections.push_back({output, connected_input});
  return result;
}

AudioGraph::ConnectionResult
AudioGraph::could_connect(OutputAudioPort output, InputAudioPort input) const {
  using Status = AudioGraph::ConnectionStatus;

  assert(input.parent_node && output.parent_node);

  if (input.type != output.type) {
    return {Status::ErrorTypeMismatch};

  } else if (has_connection_from(input)) {
    return {Status::ErrorInputAlreadyConnected};

  } else if (has_connection_to(output)) {
    return {Status::ErrorOutputAlreadyConnected};
  }

  auto* input_node = input.parent_node;
  auto* output_node = output.parent_node;

  //  Check whether connecting these ports would create a cycle.
  GraphNodeSet to_search{input_node};
  GraphNodeSet marked;

  bool cycle = false;

  while (!to_search.empty()) {
    auto next_input_node = *to_search.begin();
    to_search.erase(next_input_node);

    if (marked.count(next_input_node) > 0) {
      continue;
    } else {
      marked.insert(next_input_node);
    }

    if (next_input_node == output_node) {
      cycle = true;
      break;
    }

    auto maybe_it = graph.find(next_input_node);
    if (maybe_it != graph.end()) {
      for (const auto& connect : maybe_it->second) {
        auto maybe_search = connect.to.parent_node;
        if (marked.count(maybe_search) == 0) {
          to_search.insert(maybe_search);
        }
      }
    }
  }

  if (cycle) {
    return {Status::ErrorWouldCreateCycle};
  }

  ConnectionResult result{};
  result.connections.push_back({output, input});
  return result;
}

AudioGraph::ConnectionResult AudioGraph::connect_impl(const OutputAudioPort& output,
                                                      const InputAudioPort& input) {
  auto connect_status = could_connect(output, input);
  if (!connect_status.success()) {
    return connect_status;
  }

  auto* output_node = output.parent_node;
  auto* input_node = input.parent_node;
  assert(output_node != input_node);

  //  Insert the explicit connection.
  assert(connected_input_ports.count(input) == 0);
  assert(connected_output_ports.count(output) == 0);

  connected_input_ports[input] = output;
  connected_output_ports[output] = input;

  //  Add the connection to the graph.
  auto maybe_present_it = graph.find(output_node);
  if (maybe_present_it != graph.end()) {
    maybe_present_it->second.push_back({output, input});

  } else {
    Connections connects;
    connects.push_back({output, input});
    graph[output_node] = std::move(connects);
  }

  //  Check whether the node associated with the output port is a pure output node.
  input_nodes.insert(input_node);

  if (input_nodes.count(output_node) == 0) {
    //  This node is not an input, so it's a source.
    output_nodes.insert(output_node);
  }
  if (output_nodes.count(input_node) > 0) {
    //  This node is an input of output_node, so it's not a source.
    output_nodes.erase(input_node);
  }

  layout_needs_reevaluation = true;
  return connect_status;
}

bool AudioGraph::has_connection_to(const OutputAudioPort& output) const {
  return connected_output_ports.count(output) > 0;
}

bool AudioGraph::has_connection_from(const InputAudioPort& input) const {
  return connected_input_ports.count(input) > 0;
}

const InputAudioPort* AudioGraph::maybe_get_connected_input(const OutputAudioPort& to_output) const {
  auto it = connected_output_ports.find(to_output);
  if (it != connected_output_ports.end()) {
    return &it->second;
  } else {
    return nullptr;
  }
}

const OutputAudioPort* AudioGraph::maybe_get_connected_output(const InputAudioPort& input) const {
  auto it = connected_input_ports.find(input);
  if (it != connected_input_ports.end()) {
    return &it->second;
  } else {
    return nullptr;
  }
}

int AudioGraph::count_connected_outputs(const InputAudioPorts& ins) const {
  int count = 0;

  for (const auto& in : ins) {
    if (maybe_get_connected_output(in)) {
      count++;
    }
  }

  return count;
}

int AudioGraph::count_connected_inputs(const OutputAudioPorts& outs) const {
  int count{0};
  std::for_each(outs.begin(), outs.end(), [this, &count](auto&& out) {
    count += maybe_get_connected_input(out) == nullptr ? 0 : 1;
  });
  return count;
}

void AudioGraph::sanity_check_node_sets() const {
  for (auto& in : input_nodes) {
    assert(output_nodes.count(in) == 0);
    (void) in;
  }
}

GROVE_NAMESPACE_END
