#include "audio_node.hpp"
#include "grove/common/common.hpp"
#include <functional>

GROVE_NAMESPACE_BEGIN

AudioPort::AudioPort(BufferDataType type,
                     AudioProcessorNode* parent_node,
                     int index,
                     Flags flags) :
  type(type),
  parent_node(parent_node),
  index(index),
  flags(flags) {
  //
}

void AudioProcessorNode::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>&) const {
  //
}

GROVE_NAMESPACE_END
