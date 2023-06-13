#pragma once

#include "world_gui_common.hpp"

namespace grove::gui {

void clear_roots_gui();
void prepare_roots_gui(layout::Layout* layout, int box, gui::elements::Elements& elements,
                       const WorldGUIContext& context);
void render_roots_gui(const layout::Layout* layout, const WorldGUIContext& context);

}