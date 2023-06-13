#pragma once

#include "../audio_node.hpp"

namespace grove {

struct MIDIMessageStreamSystem;

class MIDIMessageStreamNode : public AudioProcessorNode {
public:
  MIDIMessageStreamNode(uint32_t stream_id, const MIDIMessageStreamSystem* sys);
  ~MIDIMessageStreamNode() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()

private:
  uint32_t stream_id;
  const MIDIMessageStreamSystem* stream_system;
};

}