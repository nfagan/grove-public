#include "AudioGraphRenderer.hpp"
#include "AudioGraph.hpp"
#include "AudioGraphProxy.hpp"
#include "AudioNodeIsolator.hpp"
#include "grove/common/common.hpp"

#define ENABLE_NODE_ISOLATOR (1)

GROVE_NAMESPACE_BEGIN

namespace {

void render(AudioGraphRenderData& render_data, AudioEvents* events, const AudioRenderInfo& info) {
  for (auto& renderable : render_data.ready_to_render) {
    assert(renderable.output_buffer_index >= 0);

    auto& alloc_info = render_data.alloc_info[renderable.output_buffer_index];
    const auto& channel_set = alloc_info.channel_set;

    auto& output = renderable.output;
    auto& input = renderable.input;

    if (renderable.requires_allocation) {
      alloc_info.buffer = channel_set.allocate(*alloc_info.arena, info.num_frames);
      alloc_info.buffer.zero();
    }

    if (renderable.input_buffer_index >= 0) {
      input.buffer = render_data.alloc_info[renderable.input_buffer_index].buffer;
    } else {
      assert(std::all_of(input.descriptors.begin(), input.descriptors.end(), [](const auto& descr) {
        return descr.is_missing();
      }));
    }

    output.buffer = alloc_info.buffer;

#if ENABLE_NODE_ISOLATOR
    const uint32_t node_id = renderable.node->get_id();
    GROVE_MAYBE_ISOLATE_INPUT(node_id, input, info);
#endif

    renderable.node->process(input, output, events, info);

#if ENABLE_NODE_ISOLATOR
    GROVE_MAYBE_ISOLATE_OUTPUT(node_id, output, info);
#endif
  }
}

} //  anon

AudioGraphRenderer::AudioGraphRenderer(AudioGraphDoubleBuffer* double_buffer) :
  double_buffer{double_buffer} {
  //
}

void AudioGraphRenderer::render(const AudioRenderer&,
                                Sample* samples,
                                AudioEvents* events,
                                const AudioRenderInfo& info) {
  //  Feed samples from the buffer assigned to the destination node -> out_samples.
  destination_nodes.set_output_sample_buffer(samples);

  auto& use_render_data = double_buffer->maybe_swap_and_read();
  grove::render(use_render_data, events, info);
}

DestinationNode* AudioGraphRenderer::create_destination(AudioParameterID node_id,
                                                        const AudioParameterSystem* parameter_system,
                                                        int num_output_channels) {
  return destination_nodes.create(node_id, parameter_system, num_output_channels);
}

void AudioGraphRenderer::delete_destination(DestinationNode* node) {
  destination_nodes.delete_node(node);
}

/*
 * DestinationNodes
 */

DestinationNode*
AudioGraphRenderer::DestinationNodes::create(AudioParameterID node_id,
                                             const AudioParameterSystem* parameter_system,
                                             int num_output_channels) {
  auto node = std::make_unique<DestinationNode>(node_id, parameter_system, num_output_channels);
  auto node_ptr = node.get();

  {
    std::lock_guard<std::mutex> lock(mutex);
    nodes.push_back(std::move(node));
  }

  return node_ptr;
}

void AudioGraphRenderer::DestinationNodes::delete_node(DestinationNode* node) {
  std::lock_guard<std::mutex> lock(mutex);
  auto node_it = std::find_if(nodes.begin(), nodes.end(), [node](auto& boxed_node) {
    return boxed_node.get() == node;
  });

  assert(node_it != nodes.end());
  if (node_it != nodes.end()) {
    nodes.erase(node_it);
  }
}

void AudioGraphRenderer::DestinationNodes::set_output_sample_buffer(Sample* out) {
  std::lock_guard<std::mutex> lock(mutex);
  for (auto& node : nodes) {
    node->set_output_sample_buffer(out);
  }
}

GROVE_NAMESPACE_END
