#pragma once

#include "world_gui_common.hpp"

namespace grove::gui {

void clear_arch_gui();
void prepare_arch_gui(layout::Layout* layout, int box, gui::elements::Elements& elements,
                      const WorldGUIContext& context);
void render_arch_gui(const layout::Layout* layout, const WorldGUIContext& context);

}