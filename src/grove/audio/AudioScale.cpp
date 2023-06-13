#include "AudioScale.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

void AudioScale::begin_render() {
  modified = false;
  int num_mods = modifications.size();
  for (int i = 0; i < num_mods; i++) {
    auto mod = modifications.read();
    if (mod.set_tuning) {
      tuning = mod.set_tuning->data;
      mod.set_tuning->mark_ready();
      modified = true;
    }
  }
}

bool AudioScale::ui_set_tuning(Future<Tuning>* future) {
  Modification mod{};
  mod.set_tuning = future;
  return modifications.maybe_write(mod);
}

GROVE_NAMESPACE_END
