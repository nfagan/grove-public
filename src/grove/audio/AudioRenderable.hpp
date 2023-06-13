#pragma once

#include "types.hpp"
#include "audio_events.hpp"

namespace grove {

class AudioRenderer;
class MIDIInstrument;
class AudioEffect;
class MIDIEffect;

class AudioRenderable {
public:
  virtual ~AudioRenderable() = default;

  virtual void render(const AudioRenderer& renderer,
                      Sample* out_samples,
                      AudioEvents* out_events,
                      const AudioRenderInfo& info) = 0;

  virtual bool add_instrument(MIDIInstrument*) {
    return false;
  }

  virtual bool remove_instrument(MIDIInstrument*) {
    return false;
  }

  virtual bool add_audio_effect(AudioEffect*) {
    return false;
  }

  virtual bool remove_audio_effect(AudioEffect*) {
    return false;
  }

  virtual bool add_midi_effect(MIDIEffect*) {
    return false;
  }

  virtual bool remove_midi_effect(MIDIEffect*) {
    return false;
  }
};

}