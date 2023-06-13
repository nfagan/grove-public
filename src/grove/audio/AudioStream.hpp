#pragma once

#include "types.hpp"
#include <mutex>
#include <atomic>

namespace grove {

struct AudioDeviceInfo;

/*
 * PortAudio
 */

bool initialize_port_audio();
bool terminate_port_audio();

/*
 * AudioStream
 */

struct AudioStreamInfo {
  int input_device_index{-1};
  int output_device_index{-1};

  int num_output_channels{};
  int num_input_channels{};
  audio::SampleFormat input_sample_format{};
  audio::SampleFormat output_sample_format{};

  double sample_rate{};
  int frames_per_buffer{};
  int frames_per_render_quantum{};
};

class AudioStream {
public:
  struct Parameters {
  public:
    static Parameters from_device_info(const AudioDeviceInfo& device_info,
                                       int num_channels,
                                       audio::SampleFormat sample_format);
  public:
    bool active;
    int device_index;
    int num_channels;
    audio::SampleFormat sample_format;
    double suggested_latency;
  };

  struct Status {
    bool success;
  };

public:
  AudioStream();
  ~AudioStream();

  Status open(const Parameters& input_params,
              const Parameters& output_params,
              double sample_rate,
              int frames_per_buffer,
              int frames_per_render_quantum,
              audio::AudioProcessCallback callback,
              void* user_data);

  Status open_asio_or_default(int num_output_channels,
                              audio::SampleFormat sample_format,
                              double sample_rate,
                              int frames_per_buffer,
                              int frames_per_render_quantum,
                              audio::AudioProcessCallback callback,
                              void* user_data);

  Status close();
  Status start();
  Status stop();
  void terminate();

  bool is_stream_open() const;
  bool is_stream_started() const;

  double current_time() const;
  const AudioStreamInfo* get_stream_info() const;
  double get_stream_load() const;

private:
  Status unlocked_close();
  Status unlocked_stop();

private:
  mutable std::mutex mutex;

  void* stream;
  std::atomic<bool> is_open;
  std::atomic<bool> is_started;

  AudioStreamInfo stream_info{};
};

}