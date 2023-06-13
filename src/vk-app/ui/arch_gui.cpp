#include "arch_gui.hpp"
#include "gui_draw.hpp"
#include "gui_components.hpp"
#include "ui_common.hpp"
#include "../architecture/ArchComponent.hpp"
#include "../architecture/DebugArchComponent.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gui;

struct ArchGUIData {
  BoxDrawList box_draw_list;
  elements::SliderData wall_x_angle_slider{};
  elements::SliderData wall_y_angle_slider{};
  elements::DropdownData wall_x_size_dropdown{};
  elements::DropdownData wall_y_size_dropdown{};
  elements::DropdownData wall_z_size_dropdown{};
  elements::CheckboxData extrude_from_parent_checkbox{};
  elements::CheckboxData enable_bounds_highlight_checkbox{};
};

void set_wall_extrude_theta(float v, void* context) {
  auto* ctx = static_cast<WorldGUIContext*>(context);
  auto p = get_arch_component_params(&ctx->arch_component);
  p.extrude_theta = v;
  set_arch_component_params(&ctx->arch_component, p);
}

void set_wall_x_angle(float v, void* context) {
  auto* ctx = static_cast<WorldGUIContext*>(context);
  auto& db_comp = ctx->db_arch_component;
  db_comp.collide_through_hole_params.wall_angles.x = v;
}

void set_wall_y_angle(float v, void* context) {
  auto* ctx = static_cast<WorldGUIContext*>(context);
  auto& db_comp = ctx->db_arch_component;
  db_comp.collide_through_hole_params.wall_angles.y = v;
}

float parse_scale(int opt) {
  switch (opt) {
    case 0:
      return 16.0f;
    case 1:
      return 24.0f;
    case 2:
      return 32.0f;
    default:
      assert(false);
      return 16.0f;
  }
}

float parse_z_scale(int opt) {
  switch (opt) {
    case 0:
      return 2.0f;
    case 1:
      return 16.0f;
    case 2:
      return 24.0f;
    case 3:
      return 32.0f;
    default:
      assert(false);
      return 16.0f;
  }
}

void set_wall_size(float s, void* context, int dim) {
  auto* ctx = static_cast<WorldGUIContext*>(context);
  auto& db_comp = ctx->db_arch_component;
  auto curr = db_comp.obb_isect_wall_tform->get_current();
  curr.scale[dim] = s;
  db_comp.obb_isect_wall_tform->set(curr);
}

void set_wall_x_size(int opt, void* context) {
  set_wall_size(parse_scale(opt), context, 0);
}

void set_wall_y_size(int opt, void* context) {
  set_wall_size(parse_scale(opt), context, 1);
}

void set_wall_z_size(int opt, void* context) {
  set_wall_size(parse_z_scale(opt), context, 2);
}

void extrude_wall(void* context) {
  auto* ctx = static_cast<WorldGUIContext*>(context);
  set_arch_component_need_extrude_structure(&ctx->arch_component);
}

void recede_wall(void* context) {
  auto* ctx = static_cast<WorldGUIContext*>(context);
  set_arch_component_need_recede_structure(&ctx->arch_component);
}

void project_onto_wall(void* context) {
  auto* ctx = static_cast<WorldGUIContext*>(context);
  set_arch_component_need_project_onto_structure(&ctx->arch_component);
}

void toggle_extrude_wall_from_parent(bool checked, void* context) {
  auto* ctx = static_cast<WorldGUIContext*>(context);
  auto p = get_arch_component_params(&ctx->arch_component);
  p.extrude_from_parent = checked;
  set_arch_component_params(&ctx->arch_component, p);
}

void toggle_disable_bounds_highlight(bool checked, void* context) {
  auto* ctx = static_cast<WorldGUIContext*>(context);
  auto p = get_arch_component_params(&ctx->arch_component);
  p.disable_tentative_bounds_highlight = !checked;
  set_arch_component_params(&ctx->arch_component, p);
}

void wall_size_dropdown(
  elements::DropdownData* dropdown, elements::DropdownCallback* cb,
  const char** opts, int num_opts, font::FontHandle font, float font_size,
  elements::Elements& elements, layout::Layout* layout, int container,
  const layout::BoxDimensions& line_h, BoxDrawList& draw_list, gui::RenderData* render_data) {
  //
  dropdown->option = clamp(dropdown->option, 0, num_opts - 1);
  auto prep_res = prepare_dropdown(elements, dropdown, layout, container, 2, {1}, line_h, num_opts, cb);
  float trans = dropdown->open ? 0.0f : 0.5f;
  auto style = ui::make_render_quad_desc_style(Vec3f{1.0f}, {}, {}, {}, trans);
  draw_boxes(draw_list, layout, prep_res.box_index_begin, prep_res.box_index_end, style, dropdown->open ? 1 : 0);
  draw_dropdown_labels(render_data, layout, prep_res.box_index_begin, prep_res.box_index_end, dropdown, font, opts, font_size, Vec3f{});
}

void default_slider(
  elements::SliderData* slider_data, elements::SliderDragCallback* cb,
  int container, const char* label, font::FontHandle font, float font_size,
  elements::Elements& elements, layout::Layout* layout, cursor::CursorState* cursor_state,
  BoxDrawList& draw_list, gui::RenderData* render_data) {
  //
  float label_w = ui::font_sequence_width_ascii(font, label, font_size, 4.0f);
  auto prep_res = prepare_labeled_slider(
    elements, slider_data, layout, container, {0.5f}, {0.5f}, {1, 16, 16}, {1, label_w, label_w}, cursor_state, cb);
  auto slide_style = ui::make_render_quad_desc_style(Vec3f{1.0f}, {}, {}, {}, 0.5f);
  auto handle_style = ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f, Vec3f{});
  draw_slider_boxes(draw_list, layout, prep_res.slider_section, prep_res.handle, slide_style, handle_style);
  draw_label(render_data, layout::read_box(layout, prep_res.label_section), label, font, font_size, Vec3f{}, 4.0f, false);
}

struct {
  ArchGUIData data;
} globals;

} //  anon

void gui::clear_arch_gui() {
  globals.data.box_draw_list.clear();
}

void gui::prepare_arch_gui(layout::Layout* layout, int container, elements::Elements& elements,
                           const WorldGUIContext& context) {
  auto& data = globals.data;
  (void) context;

  auto maybe_text_font = font::get_text_font();
  if (!maybe_text_font) {
    return;
  }

  const auto arch_params = get_arch_component_params(&context.arch_component);
  const auto arch_extrude_info = get_arch_component_extrude_info(&context.arch_component);
  const bool can_modify = arch_extrude_info.can_extrude && arch_extrude_info.can_recede;

  const auto text_font = maybe_text_font.value();
  const float font_size = ui::Constants::font_size;
  const float line_space = ui::Constants::line_height;
  const auto line_h = layout::BoxDimensions{1, line_space, line_space};

  layout::begin_group(layout, container, layout::GroupOrientation::Row);
  int section0 = layout::box(layout, {1}, {1});
  layout::end_group(layout);

  layout::begin_group(layout, section0, layout::GroupOrientation::Row, 0, 0, layout::JustifyContent::Left);
  int row0 = prepare_row(layout, line_h, 0);
  int row1 = prepare_row(layout, line_h, line_space);
  int row2 = prepare_row(layout, line_h, line_space);
  int row3 = prepare_row(layout, line_h, line_space);
  int row4 = prepare_row(layout, line_h, line_space);
  int row5 = prepare_row(layout, line_h, line_space);
  int row6 = prepare_row(layout, line_h, line_space);
  layout::end_group(layout);

  if (can_modify) {
    if (arch_params.extrude_from_parent) {
      data.wall_x_angle_slider.value = arch_params.extrude_theta;
      data.wall_x_angle_slider.min_value = -pif() * 0.5f;
      data.wall_x_angle_slider.max_value = pif() * 0.5f;
      default_slider(
        &data.wall_x_angle_slider, set_wall_extrude_theta, row0, "angle", text_font, font_size,
        elements, layout, &context.cursor_state, data.box_draw_list, context.render_data);
    } else {
      data.wall_x_angle_slider.value = context.db_arch_component.collide_through_hole_params.wall_angles.x;
      data.wall_x_angle_slider.min_value = 0.0f;
      data.wall_x_angle_slider.max_value = 2.0f * pif();
      default_slider(
        &data.wall_x_angle_slider, set_wall_x_angle, row0, "x angle", text_font, font_size,
        elements, layout, &context.cursor_state, data.box_draw_list, context.render_data);

      data.wall_y_angle_slider.value = context.db_arch_component.collide_through_hole_params.wall_angles.y;
      data.wall_y_angle_slider.min_value = 0.0f;
      data.wall_y_angle_slider.max_value = 2.0f * pif();
      default_slider(
        &data.wall_y_angle_slider, set_wall_y_angle, row1, "y angle", text_font, font_size,
        elements, layout, &context.cursor_state, data.box_draw_list, context.render_data);
    }
  }

  if (can_modify) {
    layout::begin_group(layout, row2, layout::GroupOrientation::Col);
    int dd0 = layout::box(layout, {0.3f}, {1});
    int dd1 = layout::box(layout, {0.3f}, {1});
    int dd2 = layout::box(layout, {0.3f}, {1});
    layout::end_group(layout);
    {
      const char* opts[3] = {"small width", "medium width", "large width"};
      wall_size_dropdown(&data.wall_x_size_dropdown, set_wall_x_size, opts, 3, text_font,
                         font_size, elements, layout, dd0, line_h, data.box_draw_list, context.render_data);
    }
    {
      const char* opts[3] = {"small height", "medium height", "large height"};
      wall_size_dropdown(&data.wall_y_size_dropdown, set_wall_y_size, opts, 3, text_font,
                         font_size, elements, layout, dd1, line_h, data.box_draw_list, context.render_data);
    }
    if (!arch_params.extrude_from_parent) {
      const char* opts[4] = {"tiny depth", "small depth", "medium depth", "large depth"};
      wall_size_dropdown(&data.wall_z_size_dropdown, set_wall_z_size, opts, 4, text_font,
                         font_size, elements, layout, dd2, line_h, data.box_draw_list, context.render_data);
    }
  }
  if (can_modify) {
    data.extrude_from_parent_checkbox.checked = arch_params.extrude_from_parent;
    auto prep_res = prepare_labeled_checkbox(elements, &data.extrude_from_parent_checkbox, layout, row3, line_h, line_h, toggle_extrude_wall_from_parent);
    draw_box(data.box_draw_list, layout, prep_res.check_box, ui::make_render_quad_desc_style(Vec3f{1}, {}, {}, {}, 0.5f));
    if (data.extrude_from_parent_checkbox.checked) {
      draw_box(data.box_draw_list, layout, prep_res.tick_box, ui::make_render_quad_desc_style(Vec3f{}));
    }
    draw_label(context.render_data, layout::read_box(layout, prep_res.label_box), "extrude from parent", text_font, font_size, Vec3f{}, 4.0f, false);
  }
  if (!arch_extrude_info.growing && !arch_extrude_info.receding) {
    data.enable_bounds_highlight_checkbox.checked = !arch_params.disable_tentative_bounds_highlight;
    auto prep_res = prepare_labeled_checkbox(elements, &data.enable_bounds_highlight_checkbox, layout, row4, line_h, line_h, toggle_disable_bounds_highlight);
    draw_box(data.box_draw_list, layout, prep_res.check_box, ui::make_render_quad_desc_style(Vec3f{1}, {}, {}, {}, 0.5f));
    if (data.enable_bounds_highlight_checkbox.checked) {
      draw_box(data.box_draw_list, layout, prep_res.tick_box, ui::make_render_quad_desc_style(Vec3f{}));
    }
    draw_label(context.render_data, layout::read_box(layout, prep_res.label_box), "preview", text_font, font_size, Vec3f{}, 4.0f, false);
  }
  if (can_modify) {
    const float extrude_bw = ui::font_sequence_width_ascii(text_font, "extrude", font_size, 4.0f);
    const float recede_bw = ui::font_sequence_width_ascii(text_font, "recede", font_size, 4.0f);

    layout::begin_group(layout, row5, layout::GroupOrientation::Col, 0, 0, layout::JustifyContent::Left);
    int extrude_button = prepare_button(elements, layout, {1, extrude_bw, extrude_bw}, line_h, false, extrude_wall);
    layout::set_box_margin(layout, extrude_button, 0, 0, 16, 0);
    int recede_button = prepare_button(elements, layout, {1, recede_bw, recede_bw}, line_h, false, recede_wall);
    layout::end_group(layout);

    draw_label(context.render_data, layout::read_box(layout, extrude_button), "extrude", text_font, font_size, Vec3f{}, 0, true);
    draw_box(data.box_draw_list, layout, extrude_button, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));

    draw_label(context.render_data, layout::read_box(layout, recede_button), "recede", text_font, font_size, Vec3f{}, 0, true);
    draw_box(data.box_draw_list, layout, recede_button, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));

  } else {
    const char* text = [&arch_extrude_info]() {
      if (arch_extrude_info.receding) {
        return "receding";
      } else if (arch_extrude_info.growing) {
        return "growing";
      } else if (arch_extrude_info.waiting_on_trees_or_roots_to_finish_pruning) {
        return "waiting for trees and roots to finish pruning";
      } else {
        return "";
      }
    }();

    const float pend_w = ui::font_sequence_width_ascii(text_font, text, font_size, 4.0f);
    if (pend_w > 0) {
      layout::begin_group(layout, row5, layout::GroupOrientation::Col, 0, 0, layout::JustifyContent::Left);
      int label = layout::box(layout, {1, pend_w, pend_w}, {1}, false);
      layout::end_group(layout);
      draw_label(context.render_data, layout::read_box(layout, label), text, text_font, font_size, Vec3f{}, 0, true);
    }
  }
  if (can_modify) {
    const char* text = "project onto structure";
    const float bw = ui::font_sequence_width_ascii(text_font, text, font_size, 4.0f);
    layout::begin_group(layout, row6, layout::GroupOrientation::Col, 0, 0, layout::JustifyContent::Left);
    int button = prepare_button(elements, layout, {1, bw, bw}, line_h, false, project_onto_wall);
    layout::end_group(layout);

    draw_label(context.render_data, layout::read_box(layout, button), text, text_font, font_size, Vec3f{}, 0, true);
    draw_box(data.box_draw_list, layout, button, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));
  }
}

void gui::render_arch_gui(const layout::Layout* layout, const WorldGUIContext& context) {
  auto& draw_list = globals.data.box_draw_list;
  gui::modify_style_from_cursor_events(draw_list, &context.cursor_state, 0.75f);
  gui::set_box_quad_positions(draw_list, layout);
  gui::push_draw_list(context.render_data, draw_list);
}

GROVE_NAMESPACE_END
