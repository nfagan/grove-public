#include "AudioBuffers.hpp"
#include "grove/env.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/load/wav.hpp"
#include "grove/math/random.hpp"

#define USE_DEMO_FILES (0)

GROVE_NAMESPACE_BEGIN

namespace {

inline std::string audio_file_full_path(const std::string& file_name) {
  return std::string{GROVE_ASSET_DIR} + "/audio/" + file_name;
}

} //  anon

void AudioBuffers::push(std::string name, AudioBufferHandle buffer_handle) {
#ifdef GROVE_DEBUG
  auto exist_buff = find_by_name(name);
  assert(!exist_buff && "Duplicate buffer name.");
  (void) exist_buff;
#endif
  audio_buffer_handles.push_back({buffer_handle, std::move(name)});
}

Optional<AudioBufferHandle> AudioBuffers::find_by_name(std::string_view name) const {
  auto it = std::find_if(
    audio_buffer_handles.begin(), audio_buffer_handles.end(), [name](const auto& buff) {
      return buff.name == name;
    });

  return it == audio_buffer_handles.end() ? NullOpt{} : Optional<AudioBufferHandle>(it->handle);
}

std::string AudioBuffers::audio_buffer_full_path(const char* file) {
  return audio_file_full_path(file);
}

std::vector<std::string> AudioBuffers::default_audio_buffer_file_names() {
#if USE_DEMO_FILES
  std::vector<std::string> audio_files{"operator-c.wav", "piano-c.wav", "flute-c.wav",
                                        "csv-pad.wav", "choir-c.wav"};
#else
  std::vector<std::string> audio_files{"operator-c.wav", "piano-c.wav", "flute-c.wav", "flute-c2.wav",
                                       "csv-guitar-c.wav", "detune-analog-c-2.wav",
                                       "csv-pad.wav", "choir-c.wav"};
#endif
  return audio_files;
}

const char** AudioBuffers::addtl_audio_buffer_file_names_no_max_norm(int* count) {
  constexpr int ct = 5;
  static const char* files[ct]{
    "chime_c3.wav", "chime2_c3.wav", "whitney_bird.wav", "vocal_unison.wav", "cajon.wav"};
  *count = ct;
  return files;
}

GROVE_NAMESPACE_END
