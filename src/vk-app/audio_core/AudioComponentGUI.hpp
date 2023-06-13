#pragma once

namespace grove {

class AudioComponent;
class IMGUIWrapper;
class SelectedInstrumentComponents;

class AudioComponentGUI {
public:
  struct UpdateResult {
    bool close_window{};
  };
public:
  UpdateResult render_gui(AudioComponent& component,
                          const SelectedInstrumentComponents& selected_components,
                          IMGUIWrapper& wrapper);
};

}