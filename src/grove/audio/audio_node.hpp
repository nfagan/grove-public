#pragma once

#include "types.hpp"
#include "data_channel.hpp"
#include "audio_parameters.hpp"
#include "audio_events.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/Temporary.hpp"
#include <cassert>

namespace grove {

#define GROVE_ASSERT_MATCHES_PORT_LAYOUT(process_data, ports) \
  assert((process_data).descriptors.size() == (ports).size())

#define GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, in_ports, out, out_ports) \
  assert((in).descriptors.size() == (in_ports).size() && (out).descriptors.size() == (out_ports).size())

/*
 * AudioPort
 */

class AudioProcessorNode;

struct AudioPort {
  struct Flags {
  public:
    using Flag = uint8_t;
    static constexpr Flag optional = 1u;

    void mark_optional() {
      data |= optional;
    }

    bool is_optional() const {
      return data & optional;
    }

    static Flags marked_optional() {
      Flags result{0};
      result.mark_optional();
      return result;
    }

  public:
    Flag data;
  };

public:
  struct Hash {
    std::size_t operator()(const AudioPort& port) const noexcept {
      auto t = std::hash<BufferDataType>{}(port.type);
      auto p = std::hash<decltype(port.parent_node)>{}(port.parent_node);
      auto ind = std::hash<int>{}(port.index);
      return (t ^ p) ^ ind;
    }
  };

public:
  AudioPort() = default;
  AudioPort(BufferDataType type,
            AudioProcessorNode* parent_node,
            int index,
            Flags flags = Flags{0});

  bool is_optional() const {
    return flags.is_optional();
  }

  friend inline bool operator==(const AudioPort& a, const AudioPort& b) {
    return a.type == b.type && a.parent_node == b.parent_node && a.index == b.index;
  }

  friend inline bool operator!=(const AudioPort& a, const AudioPort& b) {
    return !(a == b);
  }

public:
  BufferDataType type{BufferDataType::Float};
  AudioProcessorNode* parent_node{nullptr};
  int index{0};
  Flags flags{};
};

struct InputAudioPort : public AudioPort {
  using AudioPort::AudioPort;
};

struct OutputAudioPort : public AudioPort {
  using AudioPort::AudioPort;
};

using InputAudioPorts = DynamicArray<InputAudioPort, 8>;
using OutputAudioPorts = DynamicArray<OutputAudioPort, 8>;

/*
 * Node
 */

#define GROVE_DECLARE_AUDIO_NODE_INTERFACE()            \
  InputAudioPorts inputs() const override;              \
  OutputAudioPorts outputs() const override;            \
  void process(const AudioProcessData& in,              \
               const AudioProcessData& out,             \
               AudioEvents* events,                     \
               const AudioRenderInfo& info) override;

#define GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS() \
  void parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>&) const override;

class AudioProcessorNode {
public:
  virtual ~AudioProcessorNode() = default;

  virtual InputAudioPorts inputs() const = 0;
  virtual OutputAudioPorts outputs() const = 0;
  virtual uint32_t get_id() const {
    return 0;
  }

  virtual void parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const;

  virtual void process(const AudioProcessData& in,
                       const AudioProcessData& out,
                       AudioEvents* events,
                       const AudioRenderInfo& info) = 0;

  template <int N>
  Optional<AudioProcessData> match_process_data_to_inputs(const AudioProcessData& src) const;

  template <int N>
  Optional<AudioProcessData> match_process_data_to_outputs(const AudioProcessData& src) const;
};

/*
 * impl
 */

template <typename Ports, int N>
inline Optional<DynamicArray<int, N>>
find_audio_ports(const Ports& ports, const BufferChannelDescriptors& channels) {
  DynamicArray<int, N> ok_result;

  for (auto& port : ports) {
    bool found{};
    int channel_index{};

    for (auto& chan : channels) {
      if (chan.type == port.type) {
        //  Maybe a match, but we first need to make sure it hasn't already been assigned.
        auto dup_it = std::find(ok_result.begin(), ok_result.end(), channel_index);
        if (dup_it == ok_result.end()) {
          ok_result.push_back(channel_index);
          found = true;
          break;
        }
      }

      channel_index++;
    }

    if (!found) {
      return NullOpt{};
    }
  }

  return Optional<decltype(ok_result)>(std::move(ok_result));
}

template <typename Ports, int N>
inline Optional<AudioProcessData>
match_process_data_to_ports(const Ports& ports, const AudioProcessData& src) {
  auto port_indices = find_audio_ports<Ports, N>(ports, src.descriptors);
  if (!port_indices) {
    return NullOpt{};
  }

  auto ok_result = AudioProcessData::copy_excluding_descriptors(src);
  for (const auto& ind : port_indices.value()) {
    ok_result.descriptors.push_back(src.descriptors[ind]);
  }

  return Optional<AudioProcessData>(std::move(ok_result));
}

template <int N>
Optional<AudioProcessData>
AudioProcessorNode::match_process_data_to_inputs(const AudioProcessData& src) const {
  return match_process_data_to_ports<InputAudioPorts, N>(inputs(), src);
}

template <int N>
Optional<AudioProcessData>
AudioProcessorNode::match_process_data_to_outputs(const AudioProcessData& src) const {
  return match_process_data_to_ports<OutputAudioPorts, N>(outputs(), src);
}

}