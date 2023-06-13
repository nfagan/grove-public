#pragma once

#include "AmbientEnvironmentSound.hpp"
#include "grove/audio/tuning.hpp"

namespace grove {

namespace weather {
  struct Status;
}

class EnvironmentComponent {
public:
  struct InitResult {
    AmbientEnvironmentSound::InitResult ambient_sound_init_res;
  };

  struct UpdateResult {
    AmbientEnvironmentSound::UpdateResult ambient_sound_update_res;
    Optional<Tuning> new_tuning;
  };

  struct UpdateInfo {
    const weather::Status& weather_status;
    const Tuning& current_tuning;
  };

public:
  InitResult initialize();
  UpdateResult update(const UpdateInfo& update_info);

private:
  AmbientEnvironmentSound ambient_sound;
};

}