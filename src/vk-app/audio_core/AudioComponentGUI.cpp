#include "AudioComponentGUI.hpp"
#include "AudioComponent.hpp"
#include "vk-app/audio_core/audio_port_placement.hpp"
#include "grove/env.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/audio/io.hpp"
#include "grove/audio/audio_device.hpp"
#include "grove/imgui/IMGUIWrapper.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

namespace {

Optional<double> maybe_parse_double(const std::string& str) {
  try {
    return Optional<double>(std::stod(str));
  } catch (...) {
    return NullOpt{};
  }
}

#if GROVE_LOGGING_ENABLED
constexpr const char* logging_id() {
  return "render_audio_setup_info";
}
#endif

void try_to_save_selected_audio_buffer(AudioBufferHandle handle,
                                       const AudioComponent& component,
                                       const char* file_path) {
  if (auto maybe_chunk = component.get_audio_buffer_store()->ui_load(handle)) {
    auto full_file_path = std::string{GROVE_ASSET_DIR} + "/audio/output/" + file_path;

    auto& chunk = maybe_chunk.value();
    bool success = io::write_audio_buffer(chunk.descriptor, chunk.data, full_file_path.c_str());
    if (!success) {
      GROVE_LOG_ERROR_CAPTURE_META("Failed to save audio buffer to file.", logging_id());
    }
  } else {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to acquire audio buffer from store.", logging_id());
  }
}

void try_to_load_wav_audio_buffer(AudioComponent& component, const char* file_path) {
  auto full_path = AudioBuffers::audio_buffer_full_path(file_path);
  auto res = io::read_wav_as_float(full_path.c_str());
  if (res.success) {
    auto fut = component.get_audio_buffer_store()->ui_add_in_memory(
      res.descriptor, std::move(res.data));
    component.ui_audio_buffer_store.on_buffer_available(std::move(fut));
  }
}

} //  anon

AudioComponentGUI::UpdateResult
AudioComponentGUI::render_gui(AudioComponent& component,
                              const SelectedInstrumentComponents& selected_components,
                              IMGUIWrapper& wrapper) {
  UpdateResult result;

  auto& audio_core = component.audio_core;
  auto& audio_transport = component.audio_transport;
  auto& global_attenuator = component.global_attenuator;

  static int change_to_device_index{-1};
  static AudioCore::FrameInfo maybe_new_frame_info{};

  ImGui::Begin("Audio Setup");

  const auto devices = audio::enumerate_devices();

  for (int i = 0; i < int(devices.size()); i++) {
    auto& device = devices[i];

    auto max_ins = device.max_num_input_channels;
    auto max_outs = device.max_num_output_channels;
    auto latency_in = device.default_low_input_latency * 1e3;
    auto latency_out = device.default_low_output_latency * 1e3;

    std::string label{"Use"};
    label += std::to_string(i);

    if (change_to_device_index == i) {
      label += "(*)";
    }

    if (ImGui::Button(label.c_str())) {
      change_to_device_index = i;
    }

    ImGui::SameLine();
    ImGui::Text("(%d) %s\n\t%d In, %d Out\n\t%0.2fms In, %0.2fms Out",
                device.device_index, device.name.c_str(), max_ins, max_outs,
                latency_in, latency_out);
  }

  auto frame_info = audio_core.get_frame_info();
  if (maybe_new_frame_info.frames_per_render_quantum == 0) {
    maybe_new_frame_info = frame_info;
  }

  if (ImGui::InputInt("FramesPerBuffer", &maybe_new_frame_info.frames_per_buffer)) {
    //
  }

#if !GROVE_RENDER_AUDIO_IN_CALLBACK
  if (ImGui::InputInt("FramesPerRenderQuantum", &maybe_new_frame_info.frames_per_render_quantum)) {
    //
  }
#else
  maybe_new_frame_info.frames_per_render_quantum = maybe_new_frame_info.frames_per_buffer;
#endif

  if (ImGui::Button("ChangeStream") && change_to_device_index != -1) {
    if (audio_core.change_stream(devices[change_to_device_index], maybe_new_frame_info)) {
      GROVE_LOG_INFO_CAPTURE_META("Changed audio device.", logging_id());
    } else {
      GROVE_LOG_ERROR_CAPTURE_META("Failed to change audio device.", logging_id());
      maybe_new_frame_info = frame_info;
    }
  }

  auto& stream = audio_core.audio_stream;

  if (ImGui::Button(stream.is_stream_started() ? "StopAudio" : "StartAudio")) {
    if (stream.is_stream_started()) {
      audio_core.audio_stream.stop();
    } else {
      audio_core.audio_stream.start();
    }
  }

  auto maybe_new_bpm = audio_transport.get_bpm();
  auto flag = ImGuiInputTextFlags_EnterReturnsTrue;

  if (ImGui::InputDouble("BPM", &maybe_new_bpm, 0.0, 0.0, "%0.2f", flag)) {
    maybe_new_bpm = std::round(maybe_new_bpm * 2.0) * 0.5;
    if (maybe_new_bpm >= 20.0 && maybe_new_bpm <= 240.0) {
      audio_transport.set_bpm(maybe_new_bpm);
    }
  }

  char text_buffer[1024];
  memset(text_buffer, 0, 1024);
  auto enter_flag = ImGuiInputTextFlags_EnterReturnsTrue;

  if (ImGui::InputText("global gain", text_buffer, 1024, enter_flag)) {
    std::string copy_buffer(text_buffer);
    auto maybe_gain = maybe_parse_double(copy_buffer);

    if (maybe_gain && maybe_gain.value() >= minimum_finite_gain() &&
        maybe_gain.value() <= 0.0) {
      global_attenuator->set_gain(maybe_gain.value());
    }
  }

  memset(text_buffer, 0, 1024);
  if (ImGui::InputText("SaveSelectedAudioBuffer", text_buffer, 1024, enter_flag)) {
    const auto& sel_buffers = selected_components.read_selected_audio_buffers();
    if (!sel_buffers.empty()) {
      try_to_save_selected_audio_buffer(*sel_buffers.begin(), component, text_buffer);
    }
  }

  memset(text_buffer, 0, 1024);
  if (ImGui::InputText("LoadWav", text_buffer, 1024, enter_flag)) {
    try_to_load_wav_audio_buffer(component, text_buffer);
  }

  if (ImGui::Button("Close")) {
    result.close_window = true;
  }

  wrapper.end_window();

  return result;
}

GROVE_NAMESPACE_END
