#include "UIMIDIMessageStreamNodes.hpp"
#include "grove/audio/MIDIMessageStreamSystem.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

UIMIDIMessageStreamNodes::List UIMIDIMessageStreamNodes::create(
  int n, const MIDIMessageStreamHandle& stream, AudioNodeStorage& node_storage) {
  //
  List result;
  for (int i = 0; i < n; i++) {
    auto node_ctor = [stream_id = stream.id](uint32_t) {
      return new MIDIMessageStreamNode(stream_id, midi::get_global_midi_message_stream_system());
    };
    auto node = node_storage.create_node(
      node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
    result = nodes.insert(result, node);
  }
  return result;
}

void UIMIDIMessageStreamNodes::destroy(List list, AudioConnectionManager& connect_manager) {
  auto it = nodes.begin(list);
  for (; it != nodes.end(); ++it) {
    connect_manager.maybe_delete_node(*it);
  }
  (void) nodes.free_list(list);
}

GROVE_NAMESPACE_END
