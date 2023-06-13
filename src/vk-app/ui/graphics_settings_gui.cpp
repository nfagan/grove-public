#include "graphics_settings_gui.hpp"
#include "ui_common.hpp"
#include "gui_draw.hpp"
#include "gui_components.hpp"
#include "../render/graphics.hpp"
#include "../render/graphics_context.hpp"
#include "../render/graphics_preset.hpp"
#include "grove/common/common.hpp"
#include "grove/gui/gui_layout.hpp"
#include "grove/gui/gui_cursor.hpp"
#include "grove/gui/gui_elements.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gui;

void get_quality_options(const char**& labels, gfx::QualityPreset*& opts, int* num_opts) {
  static const char* src_labels[2] = {"normal", "low"};
  static gfx::QualityPreset src_opts[2] = {gfx::QualityPreset::Normal, gfx::QualityPreset::Low};
  labels = src_labels;
  opts = src_opts;
  *num_opts = 2;
}

void get_resolution_options(const char**& labels, Vec2<int>*& opts, int* num_opts) {
  static const char* src_labels[6] = {
    "1280x720",
    "1280x800",
    "1920x1080",
    "1920x1200",
    "3840x2160",
    "3840x2400"
  };
  static Vec2<int> src_opts[6] = {
    {1280, 720},
    {1280, 800},
    {1920, 1080},
    {1920, 1200},
    {3840, 2160},
    {3840, 2400},
  };
  labels = src_labels;
  opts = src_opts;
  *num_opts = 6;
}

void set_resolution(int opt, void* context) {
  auto* ctx = static_cast<MenuGUIContext*>(context);
  int num_opts{};
  const char** ignore{};
  Vec2<int>* opts{};
  get_resolution_options(ignore, opts, &num_opts);
  if (opt < num_opts) {
    VkExtent2D res{};
    res.width = uint32_t(opts[opt].x);
    res.height = uint32_t(opts[opt].y);
    vk::set_internal_forward_resolution(&ctx->vk_graphics_context, res);
  }
}

void set_preset(int opt, void* context) {
  auto* ctx = static_cast<MenuGUIContext*>(context);
  int num_opts{};
  const char** ignore{};
  gfx::QualityPreset* opts{};
  get_quality_options(ignore, opts, &num_opts);
  if (opt < num_opts) {
    gfx::set_quality_preset(&ctx->graphics_quality_preset_system, gfx::QualityPreset(opts[opt]));
  }
}

void set_render_at_native_res(bool v, void* context) {
  auto* ctx = static_cast<MenuGUIContext*>(context);
  vk::set_present_pass_enabled(&ctx->vk_graphics_context, !v);
}

void set_volumetrics_disabled(bool v, void* context) {
  auto* ctx = static_cast<MenuGUIContext*>(context);
  gfx::get_set_feature_volumetrics_disabled(&ctx->graphics_quality_preset_system, &v);
}

struct GraphicsSettingsGUIData {
  elements::DropdownData resolution_dropdown{};
  elements::DropdownData quality_dropdown{};
  elements::CheckboxData native_res_checkbox{};
  elements::CheckboxData volumetrics_disabled_checkbox{};
};

struct {
  GraphicsSettingsGUIData gui_data;
} globals;

} //  anon

void gui::prepare_graphics_settings_gui(
  layout::Layout* layout, int box, elements::Elements& elements,
  BoxDrawList& draw_list, const MenuGUIContext& context) {
  //
  auto& gui_data = globals.gui_data;

  layout::begin_group(layout, box, layout::GroupOrientation::Row);
  int container = layout::box(layout, {1}, {1});
  layout::end_group(layout);

  draw_box(draw_list, layout, container, ui::make_render_quad_desc_style(Vec3f{0.25f}, {}, {}, {}, 0.25f));

  layout::begin_group(layout, container, layout::GroupOrientation::Row);
  int sub_container = layout::box(layout, {0.75f}, {0.75f});
  layout::end_group(layout);

  auto maybe_text_font = font::get_text_font();
  if (!maybe_text_font) {
    return;
  }

  //  resolution
  const VkExtent2D curr_res = get_internal_forward_resolution(&context.vk_graphics_context);

  const char** res_opt_labels{};
  Vec2<int>* res_opts{};
  int num_res_opts{};
  get_resolution_options(res_opt_labels, res_opts, &num_res_opts);

  gui_data.resolution_dropdown.option = 0;
  for (int i = 0; i < num_res_opts; i++) {
    if (res_opts[i].x == int(curr_res.width) && res_opts[i].y == int(curr_res.height)) {
      gui_data.resolution_dropdown.option = i;
    }
  }

  //  preset
  const char** preset_opt_labels{};
  gfx::QualityPreset* preset_opts{};
  int num_preset_opts{};
  get_quality_options(preset_opt_labels, preset_opts, &num_preset_opts);
  gui_data.quality_dropdown.option = 0;

  const gfx::QualityPreset curr_preset = gfx::get_current_quality_preset(
    &context.graphics_quality_preset_system);
  for (int i = 0; i < num_preset_opts; i++) {
    if (preset_opts[i] == curr_preset) {
      gui_data.quality_dropdown.option = i;
    }
  }

  //  native res
  gui_data.native_res_checkbox.checked = !vk::get_present_pass_enabled(&context.vk_graphics_context);

  //  volumetrics
  gui_data.volumetrics_disabled_checkbox.checked = gfx::get_set_feature_volumetrics_disabled(
    &context.graphics_quality_preset_system, nullptr);

  const auto text_font = maybe_text_font.value();
  (void) text_font;

  const float font_size = ui::Constants::font_size;
  const float line_space = ui::Constants::line_height;
  const auto line_h = layout::BoxDimensions{1, line_space, line_space};

  layout::begin_group(layout, sub_container, layout::GroupOrientation::Row, 0, 0, layout::JustifyContent::Left);
  int rows[32];
  int ri{};
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, 0);
  layout::end_group(layout);

  auto text_row = [&](int row, const char* text, const Vec3f& color = Vec3f{1.0f}) {
    const float bw = ui::font_sequence_width_ascii(text_font, text, font_size, 4.0f);
    layout::begin_group(layout, row, layout::GroupOrientation::Col, 0, 0, layout::JustifyContent::Left);
    int box = layout::box(layout, {1, bw, bw}, line_h, false);
    layout::end_group(layout);
    draw_label(context.render_data, layout::read_box(layout, box), text, text_font, font_size, color, 0, true);
  };

  auto do_dropdown = [&](
    int box, const char** opts, int num_opts,
    elements::DropdownData* dropdown, elements::DropdownCallback* cb) {
    //
    dropdown->option = clamp(dropdown->option, 0, num_opts - 1);
    auto prep_res = prepare_dropdown(elements, dropdown, layout, box, 2, {1}, line_h, num_opts, cb);
    float trans = dropdown->open ? 0.0f : 0.5f;
    auto style = ui::make_render_quad_desc_style(Vec3f{1.0f}, {}, {}, {}, trans);
    draw_boxes(draw_list, layout, prep_res.box_index_begin, prep_res.box_index_end, style, dropdown->open ? 1 : 0);
    draw_dropdown_labels(context.render_data, layout, prep_res.box_index_begin, prep_res.box_index_end, dropdown, text_font, opts, font_size, Vec3f{});
  };

  auto do_checkbox = [&](int box, const char* label, elements::CheckboxData* cb_data, elements::CheckboxCallback* cb) {
    auto prep_res = prepare_labeled_checkbox(elements, cb_data, layout, box, line_h, line_h, cb);
    draw_box(draw_list, layout, prep_res.check_box, ui::make_render_quad_desc_style(Vec3f{1}, {}, {}, {}, 0.5f));
    if (cb_data->checked) {
      draw_box(draw_list, layout, prep_res.tick_box, ui::make_render_quad_desc_style(Vec3f{1.0f}));
    }
    draw_label(context.render_data, layout::read_box(layout, prep_res.label_box), label, text_font, font_size, Vec3f{1.0f}, 4.0f, false);
  };

  int dri{};
  do_checkbox(rows[dri++], "render at native resolution", &gui_data.native_res_checkbox, set_render_at_native_res);

  if (!gui_data.native_res_checkbox.checked) {
    text_row(rows[dri++], "resolution");
    do_dropdown(rows[dri++], res_opt_labels, num_res_opts, &gui_data.resolution_dropdown, set_resolution);
  }

  text_row(rows[dri++], "quality");
  do_dropdown(rows[dri++], preset_opt_labels, num_preset_opts, &gui_data.quality_dropdown, set_preset);

  dri++;
  do_checkbox(rows[dri++], "disable volumetrics", &gui_data.volumetrics_disabled_checkbox, set_volumetrics_disabled);

  assert(dri <= ri);
}

GROVE_NAMESPACE_END
