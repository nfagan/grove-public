#pragma once

#include <vector>
#include <string>

namespace grove {

struct AudioDeviceInfo {
public:
  bool is_maybe_asio() const;

public:
  std::string name;
  int device_index{-1};
  int pa_host_api_index{-1};

  int max_num_input_channels{};
  int max_num_output_channels{};

  double default_low_input_latency{};
  double default_low_output_latency{};

  double default_sample_rate{};
};

namespace audio {
  std::vector<AudioDeviceInfo> enumerate_devices();
}

}