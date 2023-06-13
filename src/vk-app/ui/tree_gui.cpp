#include "tree_gui.hpp"
#include "ui_common.hpp"
#include "../render/render_gui_data.hpp"
#include "../render/font.hpp"
#include "gui_components.hpp"
#include "gui_draw.hpp"
#include "../procedural_tree/ProceduralTreeComponent.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gui;

struct TreeGUIData {
  BoxDrawList box_draw_list;
  elements::DropdownData branch_type_dropdown{};
  elements::DropdownData leaves_type_dropdown{};
  elements::DropdownData num_trees_dropdown{};
  elements::SliderData growth_rate_slider{};
  elements::CheckboxData grow_by_signal_checkbox{};
  elements::CheckboxData disable_auto_recede_checkbox{};
};

void create_tree(void* context) {
  const auto* ctx = static_cast<const WorldGUIContext*>(context);
  for (int i = 0; i < ctx->procedural_tree_component.num_trees_manually_add; i++) {
    ctx->procedural_tree_component.create_tree(true);
  }
}

void toggle_growth_by_signal(bool v, void* context) {
  const auto* ctx = static_cast<const WorldGUIContext*>(context);
  ctx->procedural_tree_component.axis_growth_by_signal = v;
}

void toggle_disable_auto_recede(bool v, void* context) {
  const auto* ctx = static_cast<const WorldGUIContext*>(context);
  ctx->procedural_tree_component.can_trigger_death = !v;
}

void choose_branch_type(int opt, void* context) {
  const auto* ctx = static_cast<const WorldGUIContext*>(context);
  auto& component = ctx->procedural_tree_component;
  if (opt == 0) {
    component.is_pine = true;
  } else {
    component.is_pine = false;
    if (opt == 1) {
      component.spawn_params_type = 0;
      component.attraction_points_type = 0;
    } else if (opt == 2) {
      component.spawn_params_type = 0;
      component.attraction_points_type = 1;
    } else if (opt == 3) {
      component.spawn_params_type = 1;
      component.attraction_points_type = 0;
    } else {
      component.spawn_params_type = 1;
      component.attraction_points_type = 1;
    }
  }
}

void choose_leaves_type(int opt, void* context) {
  const auto* ctx = static_cast<const WorldGUIContext*>(context);
  auto& component = ctx->procedural_tree_component;
  component.foliage_leaves_type = clamp(opt, 0, 4);
}

void choose_num_trees(int opt, void* context) {
  const auto* ctx = static_cast<const WorldGUIContext*>(context);
  auto& component = ctx->procedural_tree_component;
  component.num_trees_manually_add = [opt]() {
    switch (opt) {
      case 0:
        return 1;
      case 1:
        return 5;
      case 2:
        return 20;
      case 3:
        return 100;
      default:
        assert(false);
        return 1;
    }
  }();
  if (component.num_trees_manually_add == 1) {
    component.new_tree_origin_span = 0.0f;
  } else if (component.num_trees_manually_add == 5) {
    component.new_tree_origin_span = 16.0f;
  } else if (component.num_trees_manually_add == 20) {
    component.new_tree_origin_span = 32.0f;
  } else if (component.num_trees_manually_add == 100) {
    component.new_tree_origin_span = 72.0f;
  }
}

void set_growth_rate(float v, void* context) {
  const auto* ctx = static_cast<const WorldGUIContext*>(context);
  auto& component = ctx->procedural_tree_component;
  component.axis_growth_incr = clamp(v, 0.0f, 1.0f);
}

int current_branch_type(const ProceduralTreeComponent& component) {
  if (component.is_pine) {
    return 0;
  } else if (component.spawn_params_type == 0) {
    return component.attraction_points_type == 0 ? 1 : 2;
  } else {
    return component.attraction_points_type == 0 ? 3 : 4;
  }
}

int current_leaves_type(const ProceduralTreeComponent& component) {
  return component.foliage_leaves_type;
}

int current_num_trees_index(const ProceduralTreeComponent& component) {
  switch (component.num_trees_manually_add) {
    case 1:
      return 0;
    case 5:
      return 1;
    case 20:
      return 2;
    case 100:
      return 3;
    default:
      return 0;
  }
}

int current_num_trees_in_world(const ProceduralTreeComponent& component) {
  return component.num_trees_in_world();
}

struct {
  TreeGUIData data;
} globals;

} //  anon

void gui::clear_tree_gui() {
  auto& data = globals.data;
  data.box_draw_list.clear();
}

void gui::prepare_tree_gui(layout::Layout* layout, int container, elements::Elements& elements,
                           const WorldGUIContext& context) {
  auto& data = globals.data;

  auto maybe_text_font = font::get_text_font();
  if (!maybe_text_font) {
    return;
  }

  const auto text_font = maybe_text_font.value();

  layout::begin_group(layout, container, layout::GroupOrientation::Row);
  int create_section = layout::box(layout, {1}, {1});
  layout::end_group(layout);

//  draw_box(data.box_draw_list, layout, create_section, ui::make_render_quad_desc_style(Vec3f{1, 0, 0}));

  const float font_size = ui::Constants::font_size;
  const float line_space = ui::Constants::line_height;
  const gui::layout::BoxDimensions line_h{1, line_space, line_space};

  layout::begin_group(layout, create_section, layout::GroupOrientation::Row, 0, 0, layout::JustifyContent::Left);
  int row0 = prepare_row(layout, line_h, 0);
  int row1 = prepare_row(layout, line_h, line_space);
  int row2 = prepare_row(layout, line_h, line_space);
  int row3 = prepare_row(layout, line_h, line_space);
  int checkbox0 = prepare_row(layout, line_h, line_space);
  int checkbox1 = prepare_row(layout, line_h, line_space);
  int slider_section0 = prepare_row(layout, line_h, line_space);
  layout::end_group(layout);

  {
    data.branch_type_dropdown.option = clamp(current_branch_type(context.procedural_tree_component), 0, 4);
    const char* branch_opts[5] = {"pine branches", "thin tall branches", "thin wide branches", "thick tall branches", "thick wide branches"};
    auto prep_res = prepare_dropdown(elements, &data.branch_type_dropdown, layout, row0, 1, {1}, line_h, 5, choose_branch_type);
    float trans = data.branch_type_dropdown.open ? 0.0f : 0.5f;
    draw_boxes(data.box_draw_list, layout, prep_res.box_index_begin, prep_res.box_index_end, ui::make_render_quad_desc_style(Vec3f{1.0f}, {}, {}, {}, trans), data.branch_type_dropdown.open ? 1 : 0);
    draw_dropdown_labels(context.render_data, layout, prep_res.box_index_begin, prep_res.box_index_end, &data.branch_type_dropdown, text_font, branch_opts, font_size, Vec3f{});
  }
  {
    data.leaves_type_dropdown.option = clamp(current_leaves_type(context.procedural_tree_component), 0, 3);
    const char* leaves_opts[4] = {"maple leaves", "willow leaves", "curved leaves", "broad leaves"};
    auto prep_res = prepare_dropdown(elements, &data.leaves_type_dropdown, layout, row1, 1, {1}, line_h, 4, choose_leaves_type);
    float trans = data.leaves_type_dropdown.open ? 0.0f : 0.5f;
    draw_boxes(data.box_draw_list, layout, prep_res.box_index_begin, prep_res.box_index_end, ui::make_render_quad_desc_style(Vec3f{1.0f}, {}, {}, {}, trans), data.leaves_type_dropdown.open ? 1 : 0);
    draw_dropdown_labels(context.render_data, layout, prep_res.box_index_begin, prep_res.box_index_end, &data.leaves_type_dropdown, text_font, leaves_opts, font_size, Vec3f{});
  }
  {
    data.num_trees_dropdown.option = clamp(current_num_trees_index(context.procedural_tree_component), 0, 3);
    const char* num_trees_opts[4] = {"one", "five", "twenty", "one hundred"};
    auto prep_res = prepare_dropdown(elements, &data.num_trees_dropdown, layout, row2, 1, {1}, line_h, 4, choose_num_trees);
    float trans = data.num_trees_dropdown.open ? 0.0f : 0.5f;
    draw_boxes(data.box_draw_list, layout, prep_res.box_index_begin, prep_res.box_index_end, ui::make_render_quad_desc_style(Vec3f{1.0f}, {}, {}, {}, trans), data.num_trees_dropdown.open ? 1 : 0);
    draw_dropdown_labels(context.render_data, layout, prep_res.box_index_begin, prep_res.box_index_end, &data.num_trees_dropdown, text_font, num_trees_opts, font_size, Vec3f{});
  }
  {
    bool any_growing = context.procedural_tree_component.any_growing();
    const char* create_text = any_growing ? "growing" : "create";
    const auto callback = any_growing ? nullptr : create_tree;
    const float create_bw = ui::font_sequence_width_ascii(text_font, create_text, font_size, 4.0f);

    char num_trees_str[256];
    const int nt = current_num_trees_in_world(context.procedural_tree_component);
    const char* plural = nt == 1 ? "" : "s";
    if (int len = std::snprintf(num_trees_str, 256, "%d tree%s", nt, plural); len > 0 && len < 256) {
      (void) len;
    } else {
      num_trees_str[0] = '\0';
    }

    const float num_trees_w = ui::font_sequence_width_ascii(text_font, num_trees_str, font_size, 4.0f);

    layout::begin_group(layout, row3, layout::GroupOrientation::Col, 0, 0, layout::JustifyContent::Left);
    int create_button{};
    if (any_growing) {
      create_button = layout::box(layout, {1, create_bw, create_bw}, line_h, false);
    } else {
      create_button = prepare_button(elements, layout, {1, create_bw, create_bw}, line_h, false, callback);
    }
    int label_box = layout::box(layout, {1, num_trees_w, num_trees_w}, line_h);
    layout::end_group(layout);

    draw_label(context.render_data, layout::read_box(layout, create_button), create_text, text_font, font_size, Vec3f{}, 0, true);
    if (!any_growing) {
      draw_box(data.box_draw_list, layout, create_button, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));
    }

    if (!any_growing) {
      draw_label(context.render_data, layout::read_box(layout, label_box), num_trees_str, text_font, font_size, Vec3f{}, 0, true);
    }
  }
  {
    data.grow_by_signal_checkbox.checked = context.procedural_tree_component.axis_growth_by_signal;
    auto prep_res = prepare_labeled_checkbox(elements, &data.grow_by_signal_checkbox, layout, checkbox0, line_h, line_h, toggle_growth_by_signal);
    draw_box(data.box_draw_list, layout, prep_res.check_box, ui::make_render_quad_desc_style(Vec3f{1}, {}, {}, {}, 0.5f));
    if (data.grow_by_signal_checkbox.checked) {
      draw_box(data.box_draw_list, layout, prep_res.tick_box, ui::make_render_quad_desc_style(Vec3f{}));
    }
    draw_label(context.render_data, layout::read_box(layout, prep_res.label_box), "grow by sound", text_font, font_size, Vec3f{}, 4.0f, false);
  }
  {
    data.disable_auto_recede_checkbox.checked = !context.procedural_tree_component.can_trigger_death;
    auto prep_res = prepare_labeled_checkbox(elements, &data.disable_auto_recede_checkbox, layout, checkbox1, line_h, line_h, toggle_disable_auto_recede);
    draw_box(data.box_draw_list, layout, prep_res.check_box, ui::make_render_quad_desc_style(Vec3f{1}, {}, {}, {}, 0.5f));
    if (data.disable_auto_recede_checkbox.checked) {
      draw_box(data.box_draw_list, layout, prep_res.tick_box, ui::make_render_quad_desc_style(Vec3f{}));
    }
    draw_label(context.render_data, layout::read_box(layout, prep_res.label_box), "prevent death", text_font, font_size, Vec3f{}, 4.0f, false);
  }
  {
    data.growth_rate_slider.min_value = 0.0f;
    data.growth_rate_slider.max_value = 0.1f;
    data.growth_rate_slider.value = context.procedural_tree_component.axis_growth_incr;

    float label_w = ui::font_sequence_width_ascii(text_font, "growth rate", font_size, 4.0f);
    auto prep_res = prepare_labeled_slider(elements, &data.growth_rate_slider, layout, slider_section0, {0.5f}, {0.5f}, {1, 16, 16}, {1, label_w, label_w}, &context.cursor_state, set_growth_rate);
    draw_slider_boxes(data.box_draw_list, layout, prep_res.slider_section, prep_res.handle, ui::make_render_quad_desc_style(Vec3f{1.0f}, {}, {}, {}, 0.5f), ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f, Vec3f{}));
    draw_label(context.render_data, layout::read_box(layout, prep_res.label_section), "growth rate", text_font, font_size, Vec3f{}, 4.0f, false);
  }
}

void gui::render_tree_gui(const layout::Layout* layout, const WorldGUIContext& context) {
  auto& draw_list = globals.data.box_draw_list;
  gui::modify_style_from_cursor_events(draw_list, &context.cursor_state, 0.75f);
  gui::set_box_quad_positions(draw_list, layout);
  gui::push_draw_list(context.render_data, draw_list);
}

GROVE_NAMESPACE_END
