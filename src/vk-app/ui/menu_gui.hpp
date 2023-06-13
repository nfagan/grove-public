#pragma once

#include "menu_gui_common.hpp"

namespace grove::gui {

void* get_global_menu_gui_data();
void prepare_menu_gui(const MenuGUIContext& context);
void evaluate_menu_gui(const MenuGUIContext& context);
void render_menu_gui(const MenuGUIContext& context);
void terminate_menu_gui();

}