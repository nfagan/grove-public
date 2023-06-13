#pragma once

#include "grove/audio/AudioScale.hpp"

namespace grove {

class UIAudioScale {
public:
  explicit UIAudioScale(const Tuning& tuning);
  void set_tuning(const Tuning& tuning);
  const Tuning* get_tuning() const;
  void update(AudioScale* scale);

private:
  Tuning canonical_tuning;
  Optional<Tuning> pending_set_tuning;
  bool awaiting_response{};
  std::unique_ptr<Future<Tuning>> future_tuning;
};

}