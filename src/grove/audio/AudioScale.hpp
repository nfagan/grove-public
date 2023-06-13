#pragma once

#include "tuning.hpp"
#include "grove/common/RingBuffer.hpp"
#include "grove/common/Future.hpp"
#include "grove/common/Optional.hpp"

namespace grove {

class AudioScale {
public:
  struct Modification {
    Future<Tuning>* set_tuning{};
  };

public:
  AudioScale() = default;
  explicit AudioScale(const Tuning& tuning) : tuning{tuning} {
    //
  }
  const Tuning* render_get_tuning() const {
    return &tuning;
  }
  bool render_was_modified() const {
    return modified;
  }
  void begin_render();
  [[nodiscard]] bool ui_set_tuning(Future<Tuning>* future);

private:
  Tuning tuning{default_tuning()};
  RingBuffer<Modification, 4> modifications;
  bool modified{};
};

}