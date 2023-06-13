#include "roots_gui.hpp"
#include "gui_draw.hpp"
#include "gui_components.hpp"
#include "ui_common.hpp"
#include "../procedural_tree/TreeRootsComponent.hpp"
#include "../procedural_tree/DebugTreeRootsComponent.hpp"
#include "../procedural_tree/ProceduralTreeComponent.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gui;

struct RootsGUIData {
  BoxDrawList box_draw_list;
  elements::DropdownData roots_direction_dropdown{};
  elements::DropdownData num_roots_dropdown{};
  elements::CheckboxData grow_by_signal{};
  elements::CheckboxData disable_auto_recede_checkbox{};
  elements::SliderData growth_rate_slider{};
};

int get_roots_direction_dropdown_data(const int*& opts, const char**& opt_labels,
                                      int* num_opts, const int* curr_value_index) {
  static const int src_opts[2] = {1, 0};
  static const char* src_opt_labels[2] = {"up", "down"};
  static int src_value_index = 0;
  opts = src_opts;
  opt_labels = src_opt_labels;
  *num_opts = 2;
  if (curr_value_index) {
    src_value_index = *curr_value_index;
  }
  return src_value_index;
}

int get_roots_direction_dropdown_value() {
  const int* opts{};
  const char** opt_labels{};
  int num_opts{};
  int opt_index = get_roots_direction_dropdown_data(opts, opt_labels, &num_opts, nullptr);
  return opts[opt_index];
}

void set_roots_direction_dropdown_value_index(int opt) {
  const int* opts{};
  const char** opt_labels{};
  int num_opts{};
  (void) get_roots_direction_dropdown_data(opts, opt_labels, &num_opts, &opt);
  assert(opt < num_opts);
}

int get_num_roots_dropdown_data(const int*& opts, const char**& opt_labels,
                                int* num_opts, const int* curr_value_index) {
  static const int src_opts[3] = {1, 5, 10};
  static const char* src_opt_labels[3] = {"one", "five", "ten"};
  static int src_value_index = 0;
  opts = src_opts;
  opt_labels = src_opt_labels;
  *num_opts = 3;
  if (curr_value_index) {
    src_value_index = *curr_value_index;
  }
  return src_value_index;
}

int get_num_roots_dropdown_value() {
  const int* opts{};
  const char** opt_labels{};
  int num_opts{};
  int opt_index = get_num_roots_dropdown_data(opts, opt_labels, &num_opts, nullptr);
  return opts[opt_index];
}

void set_num_roots_dropdown_value_index(int opt) {
  const int* opts{};
  const char** opt_labels{};
  int num_opts{};
  (void) get_num_roots_dropdown_data(opts, opt_labels, &num_opts, &opt);
  assert(opt < num_opts);
}

void create_roots(void* context) {
  auto* ctx = static_cast<const WorldGUIContext*>(context);
  auto& roots_comp = ctx->tree_roots_component;
  auto& tree_comp = ctx->procedural_tree_component;
  auto pos = tree_comp.get_place_tform_translation();
  const bool up = get_roots_direction_dropdown_value() == 1;
  const int n = get_num_roots_dropdown_value();
  tree_roots_component_simple_create_roots(&roots_comp, pos, n, up);
}

void choose_num_roots(int opt, void*) {
  set_num_roots_dropdown_value_index(opt);
}

void choose_roots_direction(int opt, void*) {
  set_roots_direction_dropdown_value_index(opt);
}

void set_growth_rate(float v, void* context) {
  auto* ctx = static_cast<const WorldGUIContext*>(context);
  ctx->db_tree_roots_component.params.growth_rate = v;
}

void toggle_growth_by_signal(bool v, void* context) {
  auto* ctx = static_cast<const WorldGUIContext*>(context);
  ctx->db_tree_roots_component.params.scale_growth_rate_by_signal = v;
}

void toggle_disable_auto_recede(bool v, void* context) {
  auto* ctx = static_cast<const WorldGUIContext*>(context);
  ctx->db_tree_roots_component.params.allow_recede = !v;
}

struct {
  RootsGUIData data;
} globals;

} //  anon

void gui::clear_roots_gui() {
  globals.data.box_draw_list.clear();
}

void gui::prepare_roots_gui(layout::Layout* layout, int container, elements::Elements& elements,
                            const WorldGUIContext& context) {
  auto& data = globals.data;
  (void) context;

  auto maybe_text_font = font::get_text_font();
  if (!maybe_text_font) {
    return;
  }

  const auto text_font = maybe_text_font.value();
  const float font_size = ui::Constants::font_size;
  const float line_space = ui::Constants::line_height;
  const auto line_h = layout::BoxDimensions{1, line_space, line_space};

  layout::begin_group(layout, container, layout::GroupOrientation::Row);
  int section0 = layout::box(layout, {1}, {1});
  layout::end_group(layout);

  layout::begin_group(layout, section0, layout::GroupOrientation::Row, 0, 0, layout::JustifyContent::Left);
  int rows[32];
  int ri{};
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  layout::end_group(layout);

  int dri{};
  {
    const int* opts;
    const char** opt_labels;
    int num_opts{};
    auto& dd = data.roots_direction_dropdown;
    get_roots_direction_dropdown_data(opts, opt_labels, &num_opts, &dd.option);

    auto prep_res = prepare_dropdown(elements, &dd, layout, rows[dri++], 1, {1}, line_h, num_opts, choose_roots_direction);
    float trans = dd.open ? 0.0f : 0.5f;
    draw_boxes(data.box_draw_list, layout, prep_res.box_index_begin, prep_res.box_index_end, ui::make_render_quad_desc_style(Vec3f{1.0f}, {}, {}, {}, trans), dd.open ? 1 : 0);
    draw_dropdown_labels(context.render_data, layout, prep_res.box_index_begin, prep_res.box_index_end, &dd, text_font, opt_labels, font_size, Vec3f{});
  }
  {
    const int* opts;
    const char** opt_labels;
    int num_opts{};
    auto& dd = data.num_roots_dropdown;
    get_num_roots_dropdown_data(opts, opt_labels, &num_opts, &dd.option);

    auto prep_res = prepare_dropdown(elements, &dd, layout, rows[dri++], 1, {1}, line_h, num_opts, choose_num_roots);
    float trans = dd.open ? 0.0f : 0.5f;
    draw_boxes(data.box_draw_list, layout, prep_res.box_index_begin, prep_res.box_index_end, ui::make_render_quad_desc_style(Vec3f{1.0f}, {}, {}, {}, trans), dd.open ? 1 : 0);
    draw_dropdown_labels(context.render_data, layout, prep_res.box_index_begin, prep_res.box_index_end, &dd, text_font, opt_labels, font_size, Vec3f{});
  }
  {
    layout::begin_group(layout, rows[dri++], layout::GroupOrientation::Col, 0, 0, layout::JustifyContent::Left);
    float w = ui::font_sequence_width_ascii(text_font, "create", font_size, 4.0f);
    int button = prepare_button(elements, layout, {1, w, w}, line_h, false, create_roots);
    layout::end_group(layout);
    draw_box(data.box_draw_list, layout, button, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));
    draw_label(context.render_data, layout::read_box(layout, button), "create", text_font, font_size, Vec3f{}, 4.0f, false);
  }
  {
    data.grow_by_signal.checked = context.db_tree_roots_component.params.scale_growth_rate_by_signal;
    auto prep_res = prepare_labeled_checkbox(elements, &data.grow_by_signal, layout, rows[dri++], line_h, line_h, toggle_growth_by_signal);
    draw_box(data.box_draw_list, layout, prep_res.check_box, ui::make_render_quad_desc_style(Vec3f{1}, {}, {}, {}, 0.5f));
    if (data.grow_by_signal.checked) {
      draw_box(data.box_draw_list, layout, prep_res.tick_box, ui::make_render_quad_desc_style(Vec3f{}));
    }
    draw_label(context.render_data, layout::read_box(layout, prep_res.label_box), "grow by sound", text_font, font_size, Vec3f{}, 4.0f, false);
  }
  {
    data.disable_auto_recede_checkbox.checked = !context.db_tree_roots_component.params.allow_recede;
    auto prep_res = prepare_labeled_checkbox(elements, &data.disable_auto_recede_checkbox, layout, rows[dri++], line_h, line_h, toggle_disable_auto_recede);
    draw_box(data.box_draw_list, layout, prep_res.check_box, ui::make_render_quad_desc_style(Vec3f{1}, {}, {}, {}, 0.5f));
    if (data.disable_auto_recede_checkbox.checked) {
      draw_box(data.box_draw_list, layout, prep_res.tick_box, ui::make_render_quad_desc_style(Vec3f{}));
    }
    draw_label(context.render_data, layout::read_box(layout, prep_res.label_box), "prevent death", text_font, font_size, Vec3f{}, 4.0f, false);
  }
  {
    data.growth_rate_slider.value = context.db_tree_roots_component.params.growth_rate;
    data.growth_rate_slider.min_value = 0.0f;
    data.growth_rate_slider.max_value = 4.0f;
    float label_w = ui::font_sequence_width_ascii(text_font, "growth rate", font_size, 4.0f);
    auto prep_res = prepare_labeled_slider(elements, &data.growth_rate_slider, layout, rows[dri++], {0.5f}, {0.5f}, {1, 16, 16}, {1, label_w, label_w}, &context.cursor_state, set_growth_rate);
    auto slide_style = ui::make_render_quad_desc_style(Vec3f{1.0f}, {}, {}, {}, 0.5f);
    auto handle_style = ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f, Vec3f{});
    draw_slider_boxes(data.box_draw_list, layout, prep_res.slider_section, prep_res.handle, slide_style, handle_style);
    draw_label(context.render_data, layout::read_box(layout, prep_res.label_section), "growth rate", text_font, font_size, Vec3f{}, 4.0f, false);
  }

  assert(dri == ri);
}

void gui::render_roots_gui(const layout::Layout* layout, const WorldGUIContext& context) {
  auto& draw_list = globals.data.box_draw_list;
  gui::modify_style_from_cursor_events(draw_list, &context.cursor_state, 0.75f);
  gui::set_box_quad_positions(draw_list, layout);
  gui::push_draw_list(context.render_data, draw_list);
}

GROVE_NAMESPACE_END
