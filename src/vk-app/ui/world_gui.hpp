#pragma once

#include "world_gui_common.hpp"

namespace grove::gui {

void prepare_world_gui(const WorldGUIContext& context);
void evaluate_world_gui(const WorldGUIContext& context);
void render_world_gui(const WorldGUIContext& context);
void terminate_world_gui();

}