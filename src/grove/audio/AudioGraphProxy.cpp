#include "AudioGraphProxy.hpp"
#include "AudioGraphRenderData.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

void apply_command(AudioGraph& graph, const AudioGraphProxy::Command& cmd) {
  using CommandType = AudioGraphProxy::CommandType;

  if (cmd.type == CommandType::ProbeConnect) {
    cmd.pending_result->connection_result =
      graph.could_connect(cmd.output_port, cmd.input_port);

  } else if (cmd.type == CommandType::Connect) {
    cmd.pending_result->connection_result =
      graph.connect(cmd.output_port, cmd.input_port);

  } else if (cmd.type == CommandType::DisconnectOutput) {
    cmd.pending_result->connection_result =
      graph.disconnect(cmd.output_port);

  } else if (cmd.type == CommandType::DisconnectInput) {
    cmd.pending_result->connection_result =
      graph.disconnect(cmd.input_port);

  } else if (cmd.type == CommandType::DisconnectPair) {
    cmd.pending_result->connection_result =
      graph.disconnect(cmd.output_port, cmd.input_port);

  } else if (cmd.type == CommandType::DeleteNode) {
    assert(cmd.node);
    cmd.pending_result->connection_result =
      graph.delete_node(cmd.node);
  }
}

template <typename T>
void resolve_pending(T& pending_results) {
  for (auto& pend : pending_results) {
    pend->ready.store(true);
  }
  pending_results.clear();
}

} //  anon

AudioGraphProxy::PendingResult::PendingResult() :
 ready(false),
 connection_result{},
 command{} {
  //
}

bool AudioGraphProxy::PendingResult::is_ready() const {
  return ready.load();
}

void AudioGraphProxy::push_command(const Command& command) {
  pending_commands.push_back(command);
}

void AudioGraphProxy::update(AudioGraph& graph,
                             AudioGraphDoubleBuffer& render_data,
                             int reserve_frames) {
  if (render_data.can_modify()) {
    assert(pending_results.empty());
    for (auto& command : pending_commands) {
      apply_command(graph, command);
      pending_results.push_back(command.pending_result);
    }
    pending_commands.clear();

    if (graph.layout_needs_reevaluation) {
      render_data.modify(graph, reserve_frames);
      graph.layout_needs_reevaluation = false;

#ifdef GROVE_DEBUG
      graph.sanity_check_node_sets();
#endif

    } else {
      //  If the layout didn't change, we can immediately resolve these commands, because we know
      //  the audio thread wasn't processing any of the nodes associated with them.
      resolve_pending(pending_results);
    }

    auto res = render_data.update();
    assert(!res.changed);
    (void) res;

  } else {
    auto res = render_data.update();
    if (res.changed) {
      //  Now we know for sure that the audio thread is processing the new layout due to the
      //  commands in `pending_results`.
      resolve_pending(pending_results);
    }
  }
}

GROVE_NAMESPACE_END
