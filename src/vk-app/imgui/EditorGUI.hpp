#pragma once

#include "grove/common/Optional.hpp"

namespace grove::editor {

struct Editor;

class EditorGUI {
public:
  struct UpdateResult {
    bool close{};
    Optional<bool> transform_editor_enabled;
  };

public:
  UpdateResult render(const Editor& editor);
};

}