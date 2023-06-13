#include "MIDIMessageStreamNode.hpp"
#include "../MIDIMessageStreamSystem.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

MIDIMessageStreamNode::MIDIMessageStreamNode(
  uint32_t stream_id, const MIDIMessageStreamSystem* sys) :
  stream_id{stream_id}, stream_system{sys} {
  //
}

InputAudioPorts MIDIMessageStreamNode::inputs() const {
  return {};
}

OutputAudioPorts MIDIMessageStreamNode::outputs() const {
  OutputAudioPorts result;
  result.push_back(OutputAudioPort{
    BufferDataType::MIDIMessage, const_cast<MIDIMessageStreamNode*>(this), 0});
  return result;
}

void MIDIMessageStreamNode::process(const AudioProcessData&, const AudioProcessData& out,
                                    AudioEvents*, const AudioRenderInfo& info) {
  //
  auto maybe_messages = midi::render_read_stream_messages(
    stream_system, MIDIMessageStreamHandle{stream_id});
  if (!maybe_messages) {
    return;
  }

  auto& desc = out.descriptors[0];
  auto& messages = maybe_messages.value();
  assert(messages.size() == int64_t(info.num_frames));
  for (int i = 0; i < info.num_frames; i++) {
    auto message = messages[i];
    desc.write(out.buffer.data, i, &message);
  }
}

GROVE_NAMESPACE_END
