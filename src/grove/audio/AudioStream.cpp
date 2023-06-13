#include "AudioStream.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "audio_device.hpp"
#include <portaudio.h>
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {

AudioStreamInfo stream_info_from_io_params(const AudioStream::Parameters& in,
                                           const AudioStream::Parameters& out,
                                           double sample_rate,
                                           int frames_per_buffer,
                                           int frames_per_render_quantum) {
  AudioStreamInfo stream_info{};
  stream_info.input_device_index = in.device_index;
  stream_info.output_device_index = out.device_index;

  stream_info.num_input_channels = in.num_channels;
  stream_info.num_output_channels = out.num_channels;

  stream_info.input_sample_format = in.sample_format;
  stream_info.output_sample_format = out.sample_format;

  stream_info.sample_rate = sample_rate;
  stream_info.frames_per_buffer = frames_per_buffer;
  stream_info.frames_per_render_quantum = frames_per_render_quantum;

  return stream_info;
}

PaStreamParameters to_pa_stream_parameters(const AudioStream::Parameters& from_params) {
  PaStreamParameters out_params{};
  out_params.hostApiSpecificStreamInfo = nullptr;
  out_params.channelCount = from_params.num_channels;
  out_params.device = from_params.device_index;
  out_params.sampleFormat = audio::to_pa_sample_format(from_params.sample_format);
  out_params.suggestedLatency = from_params.suggested_latency;

  return out_params;
}

std::string make_pa_error_message(const char* message, PaError status) {
  std::string msg{message};
  msg += Pa_GetErrorText(status);
  return msg;
}

} //  anon

namespace globals {

bool is_port_audio_initialized = false;

} //  globals

bool initialize_port_audio() {
  assert(!globals::is_port_audio_initialized);
  auto err = Pa_Initialize();
  const bool success = err == paNoError;
  globals::is_port_audio_initialized = success;
  return success;
}

bool terminate_port_audio() {
  if (globals::is_port_audio_initialized) {
    return Pa_Terminate() == paNoError;
  } else {
    return true;
  }
}

/*
 * AudioStream
 */

AudioStream::Parameters
AudioStream::Parameters::from_device_info(const AudioDeviceInfo& device_info,
                                          int num_channels,
                                          audio::SampleFormat sample_format) {
  AudioStream::Parameters params{};

  params.active = num_channels > 0;
  params.num_channels = num_channels;
  params.device_index = device_info.device_index;
  params.sample_format = sample_format;
  params.suggested_latency = device_info.default_low_output_latency;

  return params;
}

AudioStream::AudioStream() :
  stream(nullptr),
  is_open(false),
  is_started(false) {
  //
}

AudioStream::~AudioStream() {
  terminate();
}

void AudioStream::terminate() {
  std::lock_guard<std::mutex> lock(mutex);

  if (is_open) {
    unlocked_close();
  }
}

bool AudioStream::is_stream_open() const {
  return is_open;
}

bool AudioStream::is_stream_started() const {
  return is_started;
}

double AudioStream::current_time() const {
  assert(is_open && is_started);
  return Pa_GetStreamTime(stream);
}

const AudioStreamInfo* AudioStream::get_stream_info() const {
  return &stream_info;
}

double AudioStream::get_stream_load() const {
  if (is_stream_started()) {
    return Pa_GetStreamCpuLoad(stream);
  } else {
    return 0.0;
  }
}

AudioStream::Status AudioStream::start() {
  std::lock_guard<std::mutex> lock(mutex);
  assert(is_open && !is_started);

  auto err = Pa_StartStream(stream);
  bool success = err == paNoError;
  is_started = success;

  if (!success) {
    auto msg = make_pa_error_message("Failed to start stream: ", err);
    GROVE_LOG_ERROR_CAPTURE_META(msg.c_str(), "AudioStream");
  }

  return {success};
}

AudioStream::Status AudioStream::stop() {
  std::lock_guard<std::mutex> lock(mutex);
  return unlocked_stop();
}

AudioStream::Status AudioStream::close() {
  std::lock_guard<std::mutex> lock(mutex);
  return unlocked_close();
}

AudioStream::Status AudioStream::unlocked_stop() {
  assert(is_open && is_started);

  auto err = Pa_StopStream(stream);
  bool success = err == paNoError;

  if (success) {
    is_started = false;

  } else {
    auto msg = make_pa_error_message("Failed to stop stream: ", err);
    GROVE_LOG_ERROR_CAPTURE_META(msg.c_str(), "AudioStream");
  }

  return {success};
}

AudioStream::Status AudioStream::unlocked_close() {
  assert(is_open);

  bool failed_to_stop = false;

  if (is_started) {
    auto stop_status = unlocked_stop();
    if (!stop_status.success) {
      failed_to_stop = true;
    }
  }

  auto status = Pa_CloseStream(stream);
  bool success = status == paNoError;

  if (success) {
    stream = nullptr;
    is_open = false;
    stream_info = {};

    if (failed_to_stop) {
      //  Stream should never be closed and also started.
      is_started = false;
    }

  } else {
    auto msg = make_pa_error_message("Failed to close stream: ", status);
    GROVE_LOG_ERROR_CAPTURE_META(msg.c_str(), "AudioStream");
  }

  return {success};
}

AudioStream::Status AudioStream::open(const Parameters& input_params,
                                      const Parameters& output_params,
                                      double sample_rate,
                                      int frames_per_buffer,
                                      int frames_per_render_quantum,
                                      audio::AudioProcessCallback callback,
                                      void* user_data) {
  std::lock_guard<std::mutex> lock(mutex);
  assert(!is_open);

  auto pa_input_params = to_pa_stream_parameters(input_params);
  auto pa_output_params = to_pa_stream_parameters(output_params);
  auto pa_params_i = input_params.active ? &pa_input_params : nullptr;
  auto pa_params_o = output_params.active ? &pa_output_params : nullptr;

  auto status = Pa_OpenStream(&stream, pa_params_i, pa_params_o, sample_rate,
                              frames_per_buffer, paNoFlag, callback, user_data);

  bool success = status == paNoError;

  if (success) {
    stream_info = stream_info_from_io_params(
      input_params, output_params, sample_rate, frames_per_buffer, frames_per_render_quantum);
  }

  is_open = success;

  if (!success) {
    auto message = make_pa_error_message("Failed to open stream: ", status);
    GROVE_LOG_ERROR_CAPTURE_META(message.c_str(), "AudioStream");
  }

  return {success};
}

AudioStream::Status AudioStream::open_asio_or_default(int num_output_channels,
                                                      audio::SampleFormat sample_format,
                                                      double sample_rate,
                                                      int frames_per_buffer,
                                                      int frames_per_render_quantum,
                                                      audio::AudioProcessCallback callback,
                                                      void* user_data) {
  using Params = AudioStream::Parameters;
  assert(!is_open);

  const auto devices = audio::enumerate_devices();
  auto maybe_device = std::find_if(devices.begin(), devices.end(), [](auto& device) {
    return device.is_maybe_asio();
  });

  if (maybe_device == devices.end()) {
    maybe_device = std::find_if(devices.begin(), devices.end(), [&](auto& device) {
      return device.max_num_output_channels >= num_output_channels;
    });
  }

  if (maybe_device == devices.end()) {
    return {false};

  } else {
    auto output_params =
      Params::from_device_info(*maybe_device, num_output_channels, sample_format);

    Parameters input_params{};
    input_params.active = false;

    return open(input_params, output_params,
                sample_rate, frames_per_buffer, frames_per_render_quantum, callback, user_data);
  }
}

GROVE_NAMESPACE_END
