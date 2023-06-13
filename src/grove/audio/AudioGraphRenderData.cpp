#include "AudioGraphRenderData.hpp"
#include "AudioGraph.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/profile.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using NodeStack = std::vector<AudioProcessorNode*>;
using NodeSet = std::unordered_set<AudioProcessorNode*>;
using OutputPortSet = std::unordered_set<OutputAudioPort, OutputAudioPort::Hash>;
using OutputPortIndexMap = std::unordered_map<OutputAudioPort, int, OutputAudioPort::Hash>;
using InputPortIndexMap = std::unordered_map<InputAudioPort, int, InputAudioPort::Hash>;
using NodeData = std::unordered_map<AudioProcessorNode*, int>;
using BufferDescriptorSet = BufferChannelSet<16>;

void topo_sort(const AudioGraph& graph,
               std::vector<AudioProcessorNode*>& sorted,
               std::vector<AudioProcessorNode*>& origins) {
  //
  std::unordered_map<AudioProcessorNode*, int> edge_counts;
  std::unordered_set<OutputAudioPort, OutputAudioPort::Hash> visited_outs;

  while (!origins.empty()) {
    auto o = origins.back();
    origins.pop_back();
    sorted.push_back(o);

    auto outs = o->outputs();

    for (const auto& out : outs) {
      auto* maybe_in = graph.maybe_get_connected_input(out);
      if (!maybe_in) {
        continue;
      }

      auto* dest_node = maybe_in->parent_node;

      if (edge_counts.count(dest_node) == 0) {
        edge_counts[dest_node] =
          graph.count_connected_outputs(dest_node->inputs());
      }

      if (visited_outs.count(out) == 0) {
        visited_outs.insert(out);
        auto& count = edge_counts[dest_node];
        count--;
        if (count == 0) {
          //  No more incoming edges.
          origins.push_back(dest_node);
        }
      }
    }
  }
}

bool build_subgraph(const AudioGraph& graph, AudioProcessorNode* src, NodeStack& topo_sorted) {
  NodeSet marked;
  NodeStack s1{src};
  NodeStack sub_graph;
  NodeStack origins;

  bool is_complete_subgraph = true;
  bool has_optional_inputs = false;

  while (!s1.empty()) {
    auto s = s1.back();
    s1.pop_back();

    if (marked.count(s) > 0) {
      continue;
    } else {
      marked.insert(s);
      sub_graph.push_back(s);
    }

    auto ins = s->inputs();
    auto outs = s->outputs();

    if (ins.empty()) {
      origins.push_back(s);

    } else {
      bool is_origin_via_optional_inputs = true;

      for (const auto& in : ins) {
        auto* maybe_out = graph.maybe_get_connected_output(in);
        if (!maybe_out) {
          if (!in.is_optional()) {
            is_complete_subgraph = false;
          } else {
            has_optional_inputs = true;
          }

        } else {
          is_origin_via_optional_inputs = false;
          if (marked.count(maybe_out->parent_node) == 0) {
            s1.push_back(maybe_out->parent_node);
          }
        }
      }

      if (is_origin_via_optional_inputs) {
        origins.push_back(s);
      }
    }

    for (const auto& out : outs) {
      auto* maybe_in = graph.maybe_get_connected_input(out);
      if (!maybe_in) {
        is_complete_subgraph = false;

      } else if (marked.count(maybe_in->parent_node) == 0) {
        s1.push_back(maybe_in->parent_node);
      }
    }
  }

  if (is_complete_subgraph) {
    topo_sort(graph, topo_sorted, origins);

    if (!has_optional_inputs) {
      assert(topo_sorted.size() == sub_graph.size());
    }
  }

  return is_complete_subgraph;
}

struct RebuildGraphData {
  NodeData input_node_data;
  NodeData output_node_data;
  InputPortIndexMap input_port_indices;
  OutputPortIndexMap output_port_indices;
  OutputPortSet written_to;
  NodeSet all_processed;
};

BufferDescriptorSet build_output_descriptor_set(AudioProcessorNode* ori,
                                                const AudioGraph& graph,
                                                NodeData& output_node_data,
                                                NodeData& input_node_data,
                                                OutputPortIndexMap& output_port_indices,
                                                InputPortIndexMap& input_port_indices,
                                                int output_data_index) {
  BufferDescriptorSet output_descriptor_set;

  NodeSet source_nodes;
  NodeSet dependent_nodes;
  NodeStack pending{ori};

  while (!pending.empty()) {
    auto p = pending.back();
    pending.pop_back();

    if (source_nodes.count(p) > 0) {
      continue;
    } else {
      source_nodes.insert(p);
    }

    auto outputs = p->outputs();

    for (const auto& out : outputs) {
      auto id = int(output_descriptor_set.add(out.type));
      assert(output_port_indices.count(out) == 0);
      output_port_indices[out] = id;

      auto* maybe_in = graph.maybe_get_connected_input(out);
      if (!maybe_in) {
        continue;
      }

      assert(input_port_indices.count(*maybe_in) == 0);
      input_port_indices[*maybe_in] = id;
      dependent_nodes.insert(maybe_in->parent_node);

      auto dest_ins = maybe_in->parent_node->inputs();
      for (const auto& in : dest_ins) {
        auto maybe_out = graph.maybe_get_connected_output(in);
        if (maybe_out && source_nodes.count(maybe_out->parent_node) == 0) {
          pending.push_back(maybe_out->parent_node);
        }
      }
    }
  }

  output_descriptor_set.finalize();
  if (output_descriptor_set.stride() >= 1024) {
    GROVE_LOG_WARNING_CAPTURE_META("Stride >= 1024 bytes.", "AudioGraphRenderer");
  }

  for (const auto& node : source_nodes) {
    assert(output_node_data.count(node) == 0);
    output_node_data[node] = output_data_index;
  }
  for (const auto& node : dependent_nodes) {
    assert(input_node_data.count(node) == 0);
    input_node_data[node] = output_data_index;
  }

  return output_descriptor_set;
}

BufferChannelDescriptors
collect_input_descriptors(const AudioGraph& graph,
                          const InputAudioPorts& ins,
                          const InputPortIndexMap& input_port_indices,
                          const BufferDescriptorSet& input_descriptor_set,
                          const OutputPortSet& written_to) {
  BufferChannelDescriptors input_descriptors;
  (void) written_to;

  for (const auto& in : ins) {
    auto* maybe_out = graph.maybe_get_connected_output(in);
    if (maybe_out) {
      assert(written_to.count(*maybe_out) > 0 && input_port_indices.count(in) > 0);
      auto ind = input_port_indices.at(in);
      input_descriptors.push_back(input_descriptor_set.channel_descriptor(ind));

    } else {
      assert(in.is_optional() && input_port_indices.count(in) == 0);
      input_descriptors.push_back(BufferChannelDescriptor::missing());
    }
  }

  return input_descriptors;
}

BufferChannelDescriptors
collect_output_descriptors(const OutputAudioPorts& outs,
                           const OutputPortIndexMap& output_port_indices,
                           const BufferDescriptorSet& output_descriptor_set) {
  BufferChannelDescriptors output_descriptors;

  for (const auto& out : outs) {
    assert(output_port_indices.count(out) > 0);
    auto ind = output_port_indices.at(out);
    auto descrip = output_descriptor_set.channel_descriptor(ind);
    output_descriptors.push_back(descrip);
  }

  return output_descriptors;
}

void prepare_subgraph(const NodeStack& topo_sorted,
                      const AudioGraph& graph,
                      RebuildGraphData& rebuild_data,
                      AudioMemoryArenas& arenas,
                      AudioGraphRenderData& result,
                      int num_frames_alloc) {
  auto& [input_node_data, output_node_data, input_port_indices, output_port_indices,
  written_to, all_processed] = rebuild_data;

  uint64_t source_index = 0;
  while (source_index < topo_sorted.size()) {
    auto ori = topo_sorted[source_index++];

    int output_data_index = -1;
    int input_data_index = -1;

    BufferDescriptorSet output_descriptor_set;
    BufferDescriptorSet input_descriptor_set;
    bool has_output_buffer = false;

    auto ins = ori->inputs();
    auto outs = ori->outputs();

    if (output_node_data.count(ori) > 0) {
      output_data_index = output_node_data.at(ori);
      output_descriptor_set = result.alloc_info[output_data_index].channel_set;
      has_output_buffer = true;
    }

    if (input_node_data.count(ori) > 0) {
      input_data_index = input_node_data.at(ori);
      input_descriptor_set = result.alloc_info[input_data_index].channel_set;
    } else {
      assert(std::all_of(ins.begin(), ins.end(), [](const auto& in) {
        return in.is_optional();
      }));
    }

    if (!has_output_buffer) {
      output_data_index = int(result.alloc_info.size());
      output_descriptor_set =
        build_output_descriptor_set(ori, graph, output_node_data, input_node_data,
                                    output_port_indices, input_port_indices, output_data_index);

      auto* arena = arenas.require();
      output_descriptor_set.reserve(*arena, num_frames_alloc);
      result.alloc_info.push_back({output_descriptor_set, {}, arena});
    }

    auto input_descriptors =
      collect_input_descriptors(graph, ins, input_port_indices, input_descriptor_set, written_to);

    auto output_descriptors =
      collect_output_descriptors(outs, output_port_indices, output_descriptor_set);

#ifdef GROVE_DEBUG
    assert(all_processed.count(ori) == 0);
    all_processed.insert(ori);
#endif

    AudioGraphRenderData::ReadyToRender renderable{};
    renderable.input_buffer_index = input_data_index;
    renderable.output_buffer_index = output_data_index;
    renderable.input.descriptors = std::move(input_descriptors);
    renderable.output.descriptors = std::move(output_descriptors);
    renderable.node = ori;
    renderable.requires_allocation = !has_output_buffer;

    result.ready_to_render.push_back(std::move(renderable));

#ifdef GROVE_DEBUG
    for (const auto& out : outs) {
      assert(written_to.count(out) == 0);
      written_to.insert(out);
    }
#endif
  }
}

} //  anon

/*
 * AudioGraphRenderData
 */

AudioGraphRenderData AudioGraphRenderData::build(const AudioGraph& graph,
                                                 AudioMemoryArenas& arenas,
                                                 int num_frames) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("AudioGraphRenderData/build");

  arenas.make_all_available();

  const auto& graph_output_nodes = graph.get_output_nodes();
  std::vector<AudioProcessorNode*> sources{graph_output_nodes.begin(),
                                           graph_output_nodes.end()};

  NodeSet all_visited;
  RebuildGraphData rebuild_data;
  AudioGraphRenderData result;

  while (!sources.empty()) {
    auto src = sources.back();
    sources.pop_back();

    if (all_visited.count(src) > 0) {
      continue;
    }

    NodeStack topo_sorted;
    bool is_complete_subgraph = build_subgraph(graph, src, topo_sorted);

    for (const auto& s : topo_sorted) {
      assert(all_visited.count(s) == 0);
      all_visited.insert(s);
    }

    if (!is_complete_subgraph) {
      continue;
    }

    prepare_subgraph(topo_sorted, graph, rebuild_data, arenas, result, num_frames);
    arenas.make_all_available();
  }

  return result;
}

/*
 * AudioGraphDoubleBuffer
 */

bool AudioGraphDoubleBuffer::can_modify() const {
  return render_data_accessor.writer_can_modify();
}

void AudioGraphDoubleBuffer::modify(const AudioGraph& graph, int reserve_frames) {
  auto res = render_data_accessor.writer_modify(graph, *write_arenas, reserve_frames);
  assert(res);
  (void) res;
}

AudioGraphDoubleBuffer::Accessor::WriterUpdateResult
AudioGraphDoubleBuffer::update() {
  auto res = render_data_accessor.writer_update();
  if (res.changed) {
    std::swap(write_arenas, read_arenas);
  }

  return res;
}

using AccessorTraits = AudioGraphDoubleBuffer::AccessorTraits;
void AccessorTraits::modify(AudioGraphRenderData& data,
                            const AudioGraph& graph,
                            AudioMemoryArenas& arenas,
                            int reserve_frames) {
  data = AudioGraphRenderData::build(graph, arenas, reserve_frames);
}

/*
 * AudioMemoryArenas
 */

void AudioMemoryArenas::make_all_available() {
  free_list.clear();
  for (int i = 0; i < int(arenas.size()); i++) {
    free_list.push_back(i);
  }
}

AudioMemoryArena* AudioMemoryArenas::require() {
  AudioMemoryArena* arena;

  if (free_list.empty()) {
    auto new_arena = std::make_unique<AudioMemoryArena>();
    arena = new_arena.get();
    arenas.push_back(std::move(new_arena));

  } else {
    auto free_ind = free_list.back();
    free_list.pop_back();
    arena = arenas[free_ind].get();
  }

  return arena;
}

GROVE_NAMESPACE_END

