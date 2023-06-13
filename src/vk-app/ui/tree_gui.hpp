#pragma once

#include "world_gui_common.hpp"

namespace grove::gui {

void clear_tree_gui();
void prepare_tree_gui(layout::Layout* layout, int box, gui::elements::Elements& elements,
                      const WorldGUIContext& context);
void render_tree_gui(const layout::Layout* layout, const WorldGUIContext& context);

}