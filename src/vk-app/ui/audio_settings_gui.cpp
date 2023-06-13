#include "audio_settings_gui.hpp"
#include "ui_common.hpp"
#include "gui_draw.hpp"
#include "gui_components.hpp"
#include "../audio_core/AudioComponent.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/gui/gui_layout.hpp"
#include "grove/gui/gui_cursor.hpp"
#include "grove/gui/gui_elements.hpp"
#include "grove/math/util.hpp"
#include "grove/audio/audio_device.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gui;

struct AudioSettingsGUIData {
  elements::DropdownData input_device_dropdown{};
  elements::DropdownData output_device_dropdown{};
  elements::DropdownData frames_per_buffer_dropdown{};
  int last_cpu_load{};
  Stopwatch cpu_load_update_stopwatch{};
};

const char** get_frames_per_buffer_option_labels(int* count) {
  static const char* opts[5] = {"64", "128", "256", "512", "1024"};
  *count = 5;
  return opts;
}

const int* get_frames_per_buffer_options(int* count) {
  static const int opts[5] = {64, 128, 256, 512, 1024};
  *count = 5;
  return opts;
}

void get_audio_device_info(
  const AudioDeviceInfo* devs, int num_devs, const AudioStreamInfo& stream_info,
  int* input_infos, const char** input_names, int* num_inputs,
  int* output_infos, const char** output_names, int* num_outputs,
  int* current_input_index, int* current_output_index) {
  //
  for (int i = 0; i < num_devs; i++) {
    if (devs[i].max_num_input_channels > 0) {
      if (devs[i].device_index == stream_info.input_device_index) {
        *current_input_index = *num_inputs;
      }
      input_infos[*num_inputs] = i;
      input_names[*num_inputs] = devs[i].name.c_str();
      (*num_inputs)++;
    }
    if (devs[i].max_num_output_channels > 0) {
      if (devs[i].device_index == stream_info.output_device_index) {
        *current_output_index = *num_outputs;
      }
      output_infos[*num_outputs] = i;
      output_names[*num_outputs] = devs[i].name.c_str();
      (*num_outputs)++;
    }
  }
}

void set_frames_per_buffer(int opt, void* context) {
  auto* ctx = static_cast<MenuGUIContext*>(context);
  int num_opts{};
  const int* opts = get_frames_per_buffer_options(&num_opts);
  if (opt < num_opts) {
    auto curr_info = ctx->audio_component.audio_core.get_frame_info();
    if (curr_info.frames_per_buffer != opts[opt]) {
      AudioCore::FrameInfo frame_info{};
      frame_info.frames_per_buffer = opts[opt];
      frame_info.frames_per_render_quantum = opts[opt];
      ctx->audio_component.audio_core.change_stream(frame_info);
    }
  }
}

void select_output_device(int opt, void* context) {
  auto* ctx = static_cast<MenuGUIContext*>(context);

  int input_infos[128];
  const char* input_names[128];
  int num_inputs{};

  int output_infos[128];
  const char* output_names[128];
  int num_outputs{};

  int current_output_index{};
  int current_input_index{};

  auto devs = audio::enumerate_devices();
  int num_devs = std::min(128, int(devs.size()));

  auto& stream_info = *ctx->audio_component.audio_core.audio_stream.get_stream_info();
  get_audio_device_info(
    devs.data(), num_devs, stream_info, input_infos, input_names, &num_inputs,
    output_infos, output_names, &num_outputs,
    &current_input_index, &current_output_index);

  if (opt < num_outputs) {
    auto& dev_info = devs[output_infos[opt]];
    if (dev_info.device_index != stream_info.output_device_index) {
      ctx->audio_component.audio_core.change_stream(dev_info);
    }
  }
}

struct {
  AudioSettingsGUIData gui_data;
} globals;

} //  anon

void gui::prepare_audio_settings_gui(
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

  const auto text_font = maybe_text_font.value();
  (void) text_font;

  const float font_size = ui::Constants::font_size;
  const float line_space = ui::Constants::line_height;
  const auto line_h = layout::BoxDimensions{1, line_space, line_space};

  layout::begin_group(layout, sub_container, layout::GroupOrientation::Row, 0, 0, layout::JustifyContent::Left);
  int rows[32];
  int ri{};
  rows[ri++] = prepare_row(layout, line_h, 0);
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

  auto devs = audio::enumerate_devices();
  const auto& stream_info = *context.audio_component.audio_core.audio_stream.get_stream_info();
  const AudioCore::FrameInfo frame_info = context.audio_component.audio_core.get_frame_info();

  int num_fpb_opts{};
  const char** fbp_opt_labels = get_frames_per_buffer_option_labels(&num_fpb_opts);
  const int* fbp_opts = get_frames_per_buffer_options(&num_fpb_opts);

  gui_data.frames_per_buffer_dropdown.option = 0;
  for (int i = 0; i < num_fpb_opts; i++) {
    if (fbp_opts[i] == frame_info.frames_per_buffer) {
      gui_data.frames_per_buffer_dropdown.option = i;
      break;
    }
  }

  const int num_devs = std::min(128, int(devs.size()));
  assert(num_devs == int(devs.size()));

  int input_infos[128];
  const char* input_names[128];
  int num_inputs{};

  int output_infos[128];
  const char* output_names[128];
  int num_outputs{};

  int current_output_index{};
  int current_input_index{};

  get_audio_device_info(
    devs.data(), num_devs, stream_info, input_infos, input_names, &num_inputs,
    output_infos, output_names, &num_outputs,
    &current_input_index, &current_output_index);

  gui_data.output_device_dropdown.option = current_output_index;
  gui_data.input_device_dropdown.option = current_input_index;

  const auto desc_color = Vec3f{0.75f};

  int dri{};
  text_row(rows[dri++], "output device");
  if (num_outputs > 0) {
    do_dropdown(rows[dri++], output_names, num_outputs, &gui_data.output_device_dropdown, select_output_device);
  } else {
    text_row(rows[dri++], "none available");
  }

  text_row(rows[dri++], "input device");
  if (num_inputs > 0) {
    do_dropdown(rows[dri++], input_names, num_inputs, &gui_data.input_device_dropdown, nullptr);
  } else {
    text_row(rows[dri++], "none available", desc_color);
  }

  text_row(rows[dri++], "frames per buffer");
  do_dropdown(rows[dri++], fbp_opt_labels, num_fpb_opts, &gui_data.frames_per_buffer_dropdown, set_frames_per_buffer);

  if (!gui_data.last_cpu_load || gui_data.cpu_load_update_stopwatch.delta().count() > 0.5) {
    auto cpu_load = float(context.audio_component.audio_core.audio_stream.get_stream_load()) * 100.0f;
    gui_data.cpu_load_update_stopwatch.reset();
    gui_data.last_cpu_load = int(cpu_load);
  }

  char tmp[1024];
  text_row(rows[dri++], "load");
  if (int t = std::snprintf(tmp, 1024, "%d%%", gui_data.last_cpu_load); t >= 0 && t < 1023) {
    text_row(rows[dri++], tmp, desc_color);
  }
}

GROVE_NAMESPACE_END
