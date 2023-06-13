#pragma once

#include "grove/audio/audio_node.hpp"

namespace grove {

class Transport;
struct AudioParameterSystem;

class ArpNode : public AudioProcessorNode {
public:
  static constexpr int max_num_possible_notes = 8;
  static constexpr int message_queue_capacity = 16;

  struct Params {
    AudioParameter<int, StaticIntLimits<0, 2>> semitone_step{0};
    AudioParameter<int, StaticIntLimits<0, 2>> rate{0};
  };

public:
  ArpNode(uint32_t node_id, const Transport* transport, const AudioParameterSystem* param_sys);
  ~ArpNode() override = default;

  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()

private:
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  uint32_t node_id;
  const Transport* transport;
  const AudioParameterSystem* parameter_system;

  uint8_t possible_note_numbers[max_num_possible_notes]{};
  uint8_t step{};
  int num_possible_notes{};
  int note_index{};
  int32_t last_division{-1};

  Optional<uint8_t> playing_note;
  MIDIMessage message_queue[message_queue_capacity]{};
  int message_queue_size{};
  ScoreCursor transport_stopped_cursor{};

  Params params{};
};

}