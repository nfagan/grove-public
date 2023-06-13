#pragma once

#include "AudioNodeStorage.hpp"
#include "AudioConnectionManager.hpp"
#include "grove/audio/audio_processor_nodes/MIDIMessageStreamNode.hpp"
#include "grove/common/SlotLists.hpp"

namespace grove {

struct MIDIMessageStreamHandle;

struct UIMIDIMessageStreamNodes {
public:
  using List = SlotLists<AudioNodeStorage::NodeID>::List;
  using NodeIt = SlotLists<AudioNodeStorage::NodeID>::ConstSequenceIterator;

public:
  List create(int n, const MIDIMessageStreamHandle& stream, AudioNodeStorage& node_storage);
  void destroy(List list, AudioConnectionManager& connect_manager);

  NodeIt begin_list(List list) const {
    return nodes.cbegin(list);
  }
  NodeIt end_list() const {
    return nodes.cend();
  }

public:
  SlotLists<AudioNodeStorage::NodeID> nodes;
};

}