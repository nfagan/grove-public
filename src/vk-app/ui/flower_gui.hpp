#pragma once

#include "world_gui_common.hpp"

namespace grove::gui {

void clear_flower_gui();
void prepare_flower_gui(layout::Layout* layout, int box, gui::elements::Elements& elements,
                        const WorldGUIContext& context);
void render_flower_gui(const layout::Layout* layout, const WorldGUIContext& context);

}