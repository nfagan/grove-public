#pragma once

#include "audio_processor_nodes/DestinationNode.hpp"
#include "AudioGraphRenderData.hpp"
#include "AudioRenderable.hpp"
#include "audio_config.hpp"
#include <mutex>

namespace grove {

class AudioGraph;
class AudioGraphProxy;
class AudioProcessorNode;
class DestinationNode;
struct AudioParameterSystem;

class AudioGraphRenderer : public AudioRenderable {
public:
  struct DestinationNodes {
  public:
    DestinationNode* create(AudioParameterID node_id,
                            const AudioParameterSystem* parameter_system, int num_outputs);
    void delete_node(DestinationNode* node);
    void set_output_sample_buffer(Sample* out);

  public:
    mutable std::mutex mutex;
    std::vector<std::unique_ptr<DestinationNode>> nodes;
  };

public:
  explicit AudioGraphRenderer(AudioGraphDoubleBuffer* double_buffer);
  ~AudioGraphRenderer() override = default;

  void render(const AudioRenderer&,
              Sample* out_samples,
              AudioEvents* out_events,
              const AudioRenderInfo& info) override;

  DestinationNode* create_destination(AudioParameterID node_id,
                                      const AudioParameterSystem* parameter_system,
                                      int num_outputs);
  void delete_destination(DestinationNode* node);

private:
  AudioGraphDoubleBuffer* double_buffer;
  DestinationNodes destination_nodes;
};

}