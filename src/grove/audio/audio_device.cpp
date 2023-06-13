#include "audio_device.hpp"
#include "grove/common/common.hpp"
#include <portaudio.h>

GROVE_NAMESPACE_BEGIN

namespace {

AudioDeviceInfo from_pa_device_info(const PaDeviceInfo* info, int device_index) {
  AudioDeviceInfo device_info{};

  device_info.name = info->name;
  device_info.device_index = device_index;
  device_info.pa_host_api_index = info->hostApi;
  device_info.max_num_input_channels = info->maxInputChannels;
  device_info.max_num_output_channels = info->maxOutputChannels;
  device_info.default_low_input_latency = info->defaultLowInputLatency;
  device_info.default_low_output_latency = info->defaultLowOutputLatency;
  device_info.default_sample_rate = info->defaultSampleRate;

  return device_info;
}

} //  anon

bool AudioDeviceInfo::is_maybe_asio() const {
  const char* const ref = "asio";

  if (name.size() < 4) {
    return false;
  }

  for (int i = 0; i < 4; i++) {
    if (std::tolower(name[i]) != ref[i]) {
      return false;
    }
  }

  return true;
}

std::vector<AudioDeviceInfo> audio::enumerate_devices() {
  int num_devices = Pa_GetDeviceCount();
  std::vector<AudioDeviceInfo> devices;

  for (int i = 0; i < num_devices; i++) {
    auto* device_info = Pa_GetDeviceInfo(i);
    devices.push_back(from_pa_device_info(device_info, i));
  }

  return devices;
}

GROVE_NAMESPACE_END
