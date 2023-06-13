#include "UIAudioScale.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

UIAudioScale::UIAudioScale(const Tuning& tuning) :
  canonical_tuning{tuning},
  future_tuning{std::make_unique<Future<Tuning>>()} {
  //
}

void UIAudioScale::set_tuning(const Tuning& tuning) {
  pending_set_tuning = tuning;
}

const Tuning* UIAudioScale::get_tuning() const {
  return &canonical_tuning;
}

void UIAudioScale::update(AudioScale* scale) {
  if (awaiting_response) {
    if (future_tuning->is_ready()) {
      canonical_tuning = future_tuning->data;
      future_tuning->ready.store(false);
      awaiting_response = false;
    }
  } else if (pending_set_tuning) {
    future_tuning->data = pending_set_tuning.value();
    if (scale->ui_set_tuning(future_tuning.get())) {
      awaiting_response = true;
      pending_set_tuning = NullOpt{};
    }
  }
}

GROVE_NAMESPACE_END
