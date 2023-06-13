#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/audio_processor_nodes/DestinationNode.hpp"

namespace grove {

//  @TODO: Remove this once we refactor AudioNodeStorage.
class WrapDestinationNode : public AudioProcessorNode {
public:
  explicit WrapDestinationNode(DestinationNode* node) : node{node} {
    //
  }
  ~WrapDestinationNode() override = default;

  InputAudioPorts inputs() const override {
    return node->inputs();
  }

  OutputAudioPorts outputs() const override {
    return node->outputs();
  }

  void process(const AudioProcessData& in,
               const AudioProcessData& out,
               AudioEvents* events,
               const AudioRenderInfo& info) override {
    node->process(in, out, events, info);
  }

  void parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const override {
    node->parameter_descriptors(mem);
  }

public:
  DestinationNode* node;
};

}