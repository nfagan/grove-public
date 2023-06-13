#pragma once

#include "AudioGraph.hpp"
#include "audio_config.hpp"
#include <atomic>

namespace grove {

class AudioGraphDoubleBuffer;

class AudioGraphProxy {
public:
  struct PendingResult;

  enum class CommandType {
    ProbeConnect = 0,
    Connect,
    DisconnectOutput,
    DisconnectInput,
    DisconnectPair,
    DeleteNode,
  };

  struct Command {
    CommandType type{};
    InputAudioPort input_port{};
    OutputAudioPort output_port{};
    AudioProcessorNode* node{};
    PendingResult* pending_result{};
  };

  struct PendingResult {
    PendingResult();
    bool is_ready() const;

    std::atomic<bool> ready;
    AudioGraph::ConnectionResult connection_result;
    Command command;
  };

public:
  void push_command(const Command& command);
  void update(AudioGraph& graph,
              AudioGraphDoubleBuffer& render_data,
              int reserve_frames);

private:
  DynamicArray<Command, 16> pending_commands;
  DynamicArray<PendingResult*, 4> pending_results;
};

}