#pragma once

#include "menu_gui_common.hpp"

namespace grove::gui {

void prepare_help_gui(
  layout::Layout* layout, int box, elements::Elements& elements,
  BoxDrawList& draw_list, const MenuGUIContext& context);

}