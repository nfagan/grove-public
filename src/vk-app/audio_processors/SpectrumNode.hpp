#pragma once

#include "grove/audio/audio_node.hpp"
#include <array>

namespace grove {

class SpectrumNode : public AudioProcessorNode {
public:
  static constexpr int block_size = 128;
  static constexpr float refresh_interval_s = 10e-3f;

public:
  SpectrumNode(uint32_t node_id);
  ~SpectrumNode() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()

private:
  uint32_t node_id;

  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  std::array<float, block_size> samples;
  std::array<float, block_size * 2> dft_buff;
  int dft_sample_index{};
  bool between_blocks{};
  int inter_block_index{};
};

}