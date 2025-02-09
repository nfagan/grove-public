#include "editor/editor.hpp"
#include "transform/transform_system.hpp"
#include "camera/CameraComponent.hpp"
#include "grass/GrassComponent.hpp"
#include "model/ModelComponent.hpp"
#include "wind/WindComponent.hpp"
#include "terrain/TerrainComponent.hpp"
#include "terrain/SoilComponent.hpp"
#include "terrain/soil_parameter_modulator.hpp"
#include "terrain/DebugTerrainComponent.hpp"
#include "ui/UIPlaneComponent.hpp"
#include "sky/SkyComponent.hpp"
#include "particle/PollenComponent.hpp"
#include "render/graphics_context.hpp"
#include "render/graphics.hpp"
#include "render/graphics_preset.hpp"
#include "render/RenderComponent.hpp"
#include "render/ShadowComponent.hpp"
#include "render/debug_draw.hpp"
#include "render/frustum_cull_gpu.hpp"
#include "render/frustum_cull_data.hpp"
#include "environment/WeatherComponent.hpp"
#include "environment/SeasonComponent.hpp"
#include "environment/environment_instruments.hpp"
#include "environment/environment_global_sound_control.hpp"
#include "imgui/IMGUIComponent.hpp"
#include "util/ProfileComponent.hpp"
#include "cloud/FogComponent.hpp"
#include "bounds/BoundsComponent.hpp"
#include "bounds/debug.hpp"
#include "procedural_tree/debug_growth_system.hpp"
#include "procedural_tree/DebugProceduralTreeComponent.hpp"
#include "procedural_tree/ProceduralTreeComponent.hpp"
#include "procedural_tree/projected_nodes.hpp"
#include "procedural_tree/tree_message_system.hpp"
#include "procedural_tree/DebugTreeRootsComponent.hpp"
#include "procedural_tree/roots_instrument.hpp"
#include "procedural_tree/LSystemComponent.hpp"
#include "procedural_tree/vine_system.hpp"
#include "procedural_tree/render_vine_system.hpp"
#include "procedural_tree/vine_ornamental_foliage.hpp"
#include "procedural_tree/roots_system.hpp"
#include "procedural_tree/render_roots_system.hpp"
#include "procedural_tree/TreeRootsComponent.hpp"
#include "procedural_tree/resource_flow_along_nodes.hpp"
#include "render/render_resource_flow_along_nodes_particles.hpp"
#include "procedural_tree/resource_flow_along_nodes_instrument.hpp"
#include "procedural_flower/ProceduralFlowerComponent.hpp"
#include "architecture/DebugArchComponent.hpp"
#include "architecture/ArchComponent.hpp"
#include "audio_core/SimpleAudioNodePlacement.hpp"
#include "audio_core/node_bounds.hpp"
#include "audio_core/pitch_sampling.hpp"
#include "audio_core/rhythm_parameters.hpp"
#include "audio_core/control_note_clip_state_machine.hpp"
#include "audio_core/AudioComponent.hpp"
#include "audio_core/UIAudioConnectionManager.hpp"
#include "audio_core/UITrackSystem.hpp"
#include "audio_core/keyboard.hpp"
#include "audio_core/audio_port_placement.hpp"
#include "audio_observation/AudioObservation.hpp"
#include "cabling/CablePathFinder.hpp"
#include "environment/EnvironmentComponent.hpp"
#include "util/command_line.hpp"
#include "./imgui/imgui.hpp"
#include "./glfw/glfw.hpp"
#include "audio_processors/note_sets.hpp"
#include "audio_core/debug_audio_parameter_events.hpp"
#include "audio_core/debug_node_connection_representation.hpp"
#include "audio_core/debug_audio_nodes.hpp"
#include "audio_core/debug_note_clip_state_machine.hpp"
#include "render/render_gui_data.hpp"
#include "ui/audio_editors.hpp"
#include "ui/UIComponent.hpp"
#include "ui/world_gui.hpp"
#include "ui/menu_gui.hpp"
#include "ui/screen0_gui.hpp"
#include "ui/tutorial_gui.hpp"

#include "grove/visual/FirstPersonCamera.hpp"
#include "grove/gl/GLKeyboard.hpp"
#include "grove/gl/GLMouse.hpp"
#include "grove/input/KeyTrigger.hpp"
#include "grove/input/MouseButtonTrigger.hpp"
#include "grove/input/controllers/KeyboardMouse.hpp"
#include "grove/math/random.hpp"
#include "grove/math/string_cast.hpp"
#include "grove/env.hpp"

#include <GLFW/glfw3.h>
#include <iostream>

#define UI_PLANE_IN_WORLD_SPACE (0)

GROVE_NAMESPACE_BEGIN

namespace {

[[maybe_unused]] constexpr const char* logging_id() {
  return "App/log";
}

void log_error(const std::string& msg) {
  GROVE_LOG_ERROR_CAPTURE_META(msg.c_str(), logging_id());
  (void) msg;
}

} //  anon

namespace vk {

template <typename T>
std::string to_string(const Result<T>& cr) {
  std::string res{"Message: "};
  res += cr.message;
  return res;
}

std::string to_string(const Error& err) {
  std::string res{"Message: "};
  res += err.message;
  return res;
}

} //  vk

namespace {

struct MouseState {
  bool left_mouse_pressed{};
  bool left_mouse_clicked{};
  bool right_mouse_pressed{};
  bool right_mouse_clicked{};
  bool cursor_hidden{};
  bool cursor_over_imgui_window{};
  bool cursor_over_new_ui_window{};
};

struct KeyState {
  bool is_super_pressed{};
};

constexpr Key super_key() {
  return Key::LeftControl;
}

struct App {
  struct Params {
    bool keyboard_moves_camera{true};
    bool tuning_controlled_by_environment{true};
    bool ui_hidden{};
    bool world_ui_hidden{true};
    bool menu_ui_hidden{true};
    int ui_mode{};
    bool screen0_hidden{};
    bool tutorial_ui_hidden{};
    bool need_quit{};
  };

  vk::GLFWContext glfw_context;
  vk::GraphicsContext graphics_context;
  gfx::Context* opaque_graphics_context{};
  vk::ImGuiImpl imgui_impl;
  FirstPersonCamera camera;
  Params params{};

  GLMouse mouse;
  GLKeyboard keyboard;
  MouseButtonTrigger mouse_button_trigger{&mouse};
  MouseState mouse_state;
  KeyTrigger key_trigger{&keyboard};
  KeyState key_state;
  KeyboardMouseController controller{&keyboard, &mouse};
  editor::Editor editor;
  transform::TransformSystem transform_system;

  ProfileComponent profile_component;
  CameraComponent camera_component;
  RenderComponent render_component;
  ShadowComponent shadow_component;
  IMGUIComponent imgui_component;
  GrassComponent grass_component;
  ModelComponent model_component;
  WindComponent wind_component;
  SkyComponent sky_component;
  TerrainComponent terrain_component;
  DebugTerrainComponent debug_terrain_component;
  SoilComponent soil_component;
  soil::ParameterModulator soil_parameter_modulator;
  UIPlaneComponent ui_plane_component;
  UIComponent ui_component;
  WeatherComponent weather_component;
  EnvironmentComponent environment_component;
  SeasonComponent* season_component{};
  FogComponent fog_component;
  BoundsComponent bounds_component;
  bounds::BoundsSystem bounds_system;
  bounds::RadiusLimiterElementTag roots_bounds_element_tag{bounds::RadiusLimiterElementTag::create()};
  bounds::RadiusLimiter* roots_radius_limiter{};
  tree::RootsSystem* roots_system{};
  tree::RenderRootsSystem* render_roots_system{};
  tree::AccelInsertAndPrune tree_accel_insert_and_prune;
  tree::GrowthSystem2 tree_growth_system;
  tree::TreeSystem tree_system;
  tree::RenderTreeSystem* render_tree_system{};
  tree::TreeMessageSystem tree_message_system;
  tree::VineSystem* vine_system{};
  tree::RenderVineSystem* render_vine_system{};
  DebugProceduralTreeComponent debug_procedural_tree_component;
  ProceduralTreeComponent procedural_tree_component;
  TreeRootsComponent* tree_roots_component{};
  DebugTreeRootsComponent debug_procedural_tree_roots_component;
  ls::LSystemComponent* lsystem_component{};
  tree::ProjectedNodesSystem projected_nodes_system;
  ProceduralFlowerComponent procedural_flower_component;
  PollenComponent pollen_component;
  DebugArchComponent debug_arch_component;
  ArchComponent* arch_component{};

  AudioComponent audio_component;
  AudioObservation audio_observation;
  ui::AudioEditorData new_audio_editor_data;
  AudioPortPlacement audio_port_placement;
  UIAudioConnectionManager ui_audio_connection_manager;
  SimpleAudioNodePlacement simple_audio_node_placement;
  SelectedInstrumentComponents selected_instrument_components;
  CablePathFinder cable_path_finder;
  RhythmParameters rhythm_params{};
  pss::PitchSamplingParameters pitch_sampling_params{};
  int music_keyboard_octave{3};

  Stopwatch frame_timer;
  Stopwatch elapsed_timer;
};

vk::DynamicSampledImageManager::CreateContext
make_dynamic_sampled_image_manager_create_context(App& app) {
  return vk::DynamicSampledImageManager::CreateContext{
    app.graphics_context.frame_queue_depth,
    app.graphics_context.core,
    &app.graphics_context.allocator,
    &app.graphics_context.command_processor
  };
}

RenderComponent::InitInfo make_render_component_init_info(App& app) {
  const bool enable_post_processing = true;
  return RenderComponent::InitInfo{
    app.opaque_graphics_context,
    app.graphics_context.core,
    &app.graphics_context.allocator,
    vk::make_forward_pass_pipeline_render_pass_info(&app.graphics_context),
    vk::make_shadow_pass_pipeline_render_pass_info(&app.graphics_context),
    vk::make_post_process_pipeline_render_pass_info(&app.graphics_context),
    app.graphics_context.frame_queue_depth,
    enable_post_processing,
    app.graphics_context.sampler_system,
    app.graphics_context.buffer_system,
    app.graphics_context.staging_buffer_system,
    app.graphics_context.pipeline_system,
    app.graphics_context.descriptor_system,
    app.graphics_context.command_processor,
    app.graphics_context.dynamic_sampled_image_manager,
    app.graphics_context.sampled_image_manager,
    make_dynamic_sampled_image_manager_create_context(app),
  };
}

void play_quantized_midi_notes(App& app, int count, audio::Quantization quant, bool quick) {
  auto* pitch_sys = app.audio_component.get_pitch_sampling_system();
  const PitchSampleSetGroupHandle group = pss::ui_get_ith_group(
    pitch_sys, app.pitch_sampling_params.primary_pitch_sample_group_index);

  double durs[3] = {1.0, 1.0, 0.5};
  if (quick) {
    std::fill(durs, durs + 3, 0.25);
  }
//  const int8_t base_octaves[3] = {2, 3, 4};
  for (int i = 0; i < count; i++) {
//    int8_t oct = *uniform_array_sample(base_octaves, 3);
    MIDINote note = pss::ui_uniform_sample_midi_note(pitch_sys, group, 0, 3, MIDINote::C3);
    double dur = *uniform_array_sample(durs, 3);
    auto* qtn = app.audio_component.get_quantized_triggered_notes();
    qtn::ui_trigger(qtn, 0, note, quant, dur);
  }
}

void play_midi_notes(App& app, int count) {
  auto* pitch_sys = app.audio_component.get_pitch_sampling_system();
  const PitchSampleSetGroupHandle group = pss::ui_get_ith_group(
    pitch_sys, app.pitch_sampling_params.primary_pitch_sample_group_index);

  for (int i = 0; i < count; i++) {
    MIDINote note = pss::ui_uniform_sample_midi_note(pitch_sys, group, 0, 3, MIDINote::C3);
    app.audio_component.get_ui_timeline_system()->note_on_timeout(
      *app.audio_component.get_triggered_notes(), note, 0.25f);
    track::note_on_timeout(track::get_global_ui_track_system(), app.audio_component, note, 0.25f);
  }
}

Ray make_mouse_ray(float mx, float my, float width, float height, const Camera& camera) {
  return Ray{
    camera.get_position(),
    mouse_ray_direction(
      inverse(camera.get_view()),
      inverse(camera.get_projection()),
      Vec2f{mx, my},
      Vec2f{width, height})
  };
}

void set_ui_mode(App& app, bool hidden, int mode) {
  app.params.ui_hidden = hidden;
  app.params.ui_mode = mode;

  app.params.world_ui_hidden = true;
  app.new_audio_editor_data.hidden = true;
  if (!app.params.ui_hidden) {
    if (app.params.ui_mode == 0) {
      app.new_audio_editor_data.hidden = false;
    } else {
      app.params.world_ui_hidden = false;
    }
  }
}

void global_key_listener(App& app,
                         const KeyTrigger::KeyState& pressed,
                         const KeyTrigger::KeyState&) {
  const bool alt_pressed = app.keyboard.is_pressed(Key::LeftAlt);
  const bool shift_pressed = app.keyboard.is_pressed(Key::LeftShift);
  if (alt_pressed && pressed.count(Key::Q) > 0) {
    app.ui_audio_connection_manager.attempt_to_connect();
  }
  if (alt_pressed && pressed.count(Key::E) > 0) {
    if (!app.selected_instrument_components.selected_port_ids.empty()) {
      app.ui_audio_connection_manager.attempt_to_disconnect(
        *app.selected_instrument_components.selected_port_ids.begin());
    }
  }
  if (alt_pressed && pressed.count(Key::X) > 0) {
    app.params.keyboard_moves_camera = !app.params.keyboard_moves_camera;
  }
  if (alt_pressed && pressed.count(Key::H) > 0) {
    app.imgui_component.enabled = !app.imgui_component.enabled;
  }
  if (pressed.count(Key::Escape) > 0) {
    app.params.menu_ui_hidden = !app.params.menu_ui_hidden;
  }
  if (alt_pressed && pressed.count(Key::F) > 0) {
    set_ui_mode(app, !app.params.ui_hidden, app.params.ui_mode);
  }
  if (!app.params.ui_hidden && pressed.count(Key::Tab)) {
    set_ui_mode(app, false, (app.params.ui_mode + 1) % 2);
  }
  if (alt_pressed && pressed.count(Key::U) > 0) {
    app.camera_component.toggle_free_roaming();
  }
  if (alt_pressed && pressed.count(Key::Number1) > 0) {
    app.camera_component.toggle_high_up_position_target();
  }
  if (alt_pressed && pressed.count(Key::Number2) > 0) {
    app.camera_component.toggle_below_ground_position_target();
  }
  if (pressed.count(Key::GraveAccent) > 0) {
    bool cycle_forwards = !shift_pressed;
    ui::maybe_cycle_mode(app.new_audio_editor_data, cycle_forwards);
  }
}

void audio_key_listener(App& app,
                        const KeyTrigger::KeyState& pressed,
                        const KeyTrigger::KeyState& released) {
  const bool is_alt_pressed = app.keyboard.is_pressed(Key::LeftAlt);
  const bool is_ctrl_pressed = app.keyboard.is_pressed(Key::LeftControl);
  const bool is_cmd_pressed = app.keyboard.is_pressed(Key::Command);
  const bool is_modifier_pressed = is_alt_pressed || is_cmd_pressed || is_ctrl_pressed;

  if (pressed.count(Key::Space) > 0 && !is_alt_pressed) {
    app.audio_component.audio_transport.toggle_play_stop();
  }
  if (pressed.count(Key::Z) > 0 && !is_modifier_pressed) {
    app.music_keyboard_octave--;
  }
  if (pressed.count(Key::X) > 0 && !is_modifier_pressed) {
    app.music_keyboard_octave++;
  }
  if (app.params.keyboard_moves_camera) {
    return;
  }
  auto oct = app.music_keyboard_octave;
  auto pressed_notes = audio::key_press_notes_to_midi_notes(
    audio::gather_key_press_notes(pressed), oct);
  auto released_notes = audio::key_press_notes_to_midi_notes(
    audio::gather_key_press_notes(released), oct);

  auto* ui_timeline_sys = app.audio_component.get_ui_timeline_system();
  auto* triggered_notes = app.audio_component.get_triggered_notes();
  auto* ui_track_sys = track::get_global_ui_track_system();

  if (!app.keyboard.is_pressed(Key::LeftAlt)) {
    for (auto& on : pressed_notes) {
      ui_timeline_sys->note_on(*triggered_notes, on);
      track::note_on(ui_track_sys, app.audio_component, on);
    }
    for (auto& off : released_notes) {
      ui_timeline_sys->note_off(*triggered_notes, off);
      track::note_off(ui_track_sys, app.audio_component, off);
    }
#if 1
    if (!pressed_notes.empty()) {
      auto* pitch_sys = app.audio_component.get_pitch_sampling_system();
      auto pitch_group = pss::ui_get_ith_group(
        pitch_sys, app.pitch_sampling_params.primary_pitch_sample_group_index);
      pss::ui_push_triggered_notes(
        pitch_sys, pitch_group, 0, pressed_notes.data(), int(pressed_notes.size()), MIDINote::C3);
    }
#endif
  }
}

void place_simple_audio_node_port(AudioPortPlacement& port_placement,
                                  const SimpleAudioNodePlacement::PortInfo& port) {
  port_placement.add_selectable(port.id);
  port_placement.set_bounds(port.id, port.world_bound);
  port_placement.set_path_finding_position(port.id, port.world_bound.center());
}

void remove_placed_audio_node(App& app, AudioNodeStorage::NodeID id) {
  app.simple_audio_node_placement.delete_node(id, app.render_component.simple_shape_renderer);
  auto port_info = app.audio_component.audio_node_storage.get_port_info_for_node(id);
  if (port_info) {
    for (auto& port : port_info.value()) {
      app.audio_port_placement.remove_port(port.id);
      app.selected_instrument_components.remove(port.id);
    }
  }
}

bool insert_audio_node_bounds_ignoring_handles(
  App& app, bounds::AccelInstanceHandle accel_handle, const Bounds3f& bounds) {
  //
  auto node_bounds = OBB3f::axis_aligned(bounds.center(), bounds.size() * 0.5f);
  return bounds::insert_audio_node_bounds(
    bounds::get_audio_node_bounds_impl(),
    &node_bounds, 1, &app.bounds_system, accel_handle, app.roots_radius_limiter, nullptr, nullptr);
}

void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
  auto* app = static_cast<App*>(glfwGetWindowUserPointer(window));
  app->glfw_context.framebuffer_width = width;
  app->glfw_context.framebuffer_height = height;
  app->graphics_context.need_recreate_swapchain = true;
}

bool initialize_audio_core(App* app, bool initialize_default_audio_stream) {
  app->audio_component.initialize(initialize_default_audio_stream);

  ncsm::initialize(
    ncsm::get_global_control_note_clip_state_machine(),
    app->audio_component.get_note_clip_state_machine_system());

  return true;
}

bool initialize_glfw(vk::GLFWContext* context, App* app, const cmd::Arguments& args) {
  assert(!context->initialized && !context->window);
  vk::GLFWContextCreateInfo info{};
  if (args.full_screen) {
    info.fullscreen_window_index = vk::GLFWContextCreateInfo::default_monitor_index;
  }
  info.user_data = app;
  info.mouse_button_callback = &grove::glfw::mouse_button_callback;
  info.key_callback = &grove::glfw::key_callback;
  info.cursor_position_callback = &grove::glfw::cursor_position_callback;
  info.framebuffer_resize_callback = &framebuffer_resize_callback;
  info.scroll_callback = &grove::glfw::scroll_callback;
#ifdef GROVE_DEBUG
  info.window_title = "<debug>";
#else
  info.window_title = "<release>";
#endif
  auto res = vk::create_and_initialize_glfw_context(info);
  if (!res) {
    log_error(to_string(res));
    return false;
  } else {
    *context = res.value;
    return true;
  }
}

bool initialize_graphics_context(vk::GraphicsContext* context, gfx::Context** opaque_context,
                                 GLFWwindow* window) {
  vk::GraphicsContextCreateInfo create_info{};
  create_info.instance_create_info = vk::make_default_instance_create_info();
  create_info.window = window;
  if (auto err = vk::create_graphics_context(context, &create_info)) {
    log_error(to_string(err));
    return false;
  } else {
    *opaque_context = gfx::init_context(context);
    return true;
  }
}

bool initialize_imgui(vk::ImGuiImpl* imgui_impl, vk::GraphicsContext& context, GLFWwindow* window) {
  auto* graphics_queue = context.core.ith_graphics_queue(0);
  if (!graphics_queue) {
    return false;
  }

  auto pass_info = make_post_process_pipeline_render_pass_info(&context);

  vk::ImGuiImplCreateInfo create_info{
    context.core,
    *graphics_queue,
    &context.command_processor,
    pass_info.render_pass,
    window,
    context.frame_queue_depth,
    pass_info.raster_samples
  };

  auto imgui_impl_res = vk::create_and_initialize_imgui_impl(create_info);
  if (!imgui_impl_res) {
    log_error(to_string(imgui_impl_res));
    return false;
  }

  *imgui_impl = imgui_impl_res.value;
  return true;
}

void initialize_editor(App& app) {
  editor::initialize(&app.editor, {
    &app.transform_system,
    SimpleShapeRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.simple_shape_renderer
  });
}

void initialize_camera(App& app) {
  app.camera_component.initialize({
    app.camera,
    app.glfw_context.window_aspect_ratio()
  });
}

void initialize_input(App& app) {
  app.mouse_button_trigger.add_listener([&app](auto& pressed, auto&) {
    if (pressed.contains(Mouse::Button::Left)) {
      app.mouse_state.left_mouse_clicked = true;
    }
    if (pressed.contains(Mouse::Button::Right)) {
      app.mouse_state.right_mouse_clicked = true;
    }
  });
  app.key_trigger.add_listener([&app](auto& pressed, auto& released) {
    global_key_listener(app, pressed, released);
    audio_key_listener(app, pressed, released);
  });
}

void initialize_render_component(App& app) {
  app.render_component.initialize(make_render_component_init_info(app));
  vk::debug::initialize_rendering(
    &app.render_component.point_buffer_renderer,
    &app.render_component.simple_shape_renderer,
    &app.graphics_context.core,
    &app.graphics_context.allocator,
    &app.graphics_context.buffer_system,
    &app.graphics_context.staging_buffer_system,
    &app.graphics_context.command_processor,
    app.graphics_context.frame_queue_depth);
}

void initialize_shadow_component(App& app) {
  ShadowComponent::InitInfo info{};
  info.sun_shadow_projection_sign_y = -1.0f;
  info.sun_shadow_layer_size = 128.0f;
  info.num_sun_shadow_cascades = GROVE_NUM_SUN_SHADOW_CASCADES;
  info.sun_shadow_texture_dim = int(app.graphics_context.shadow_pass.extent.width);
  app.shadow_component.initialize(info);
}

void initialize_grass_component(App& app) {
  app.grass_component.initialize({app.camera});
}

void initialize_model_component(App& app) {
  auto init_res = app.model_component.initialize({
    app.render_component.static_model_renderer,
    app.graphics_context.sampled_image_manager,
    StaticModelRenderer::make_add_resource_context(app.graphics_context),
    app.transform_system,
    app.terrain_component.get_terrain()
  });
  for (auto& mod : init_res.modify_transform_editor) {
    if (mod.add_instance) {
      auto& add_info = mod.add_instance.value();
      auto inst = app.editor.transform_editor.create_instance(
        add_info.target,
        app.transform_system,
        app.editor.cursor_monitor,
        add_info.at_offset);
      app.model_component.register_transform_editor(add_info.register_with, inst);
    } else if (mod.remove_instance) {
      app.editor.transform_editor.destroy_instance(
        mod.remove_instance.value().handle, app.editor.cursor_monitor);
    }
  }
}

void initialize_sky_component(App& app) {
  app.sky_component.initialize({
    app.graphics_context.sampled_image_manager,
    app.graphics_context.dynamic_sampled_image_manager,
    app.render_component.sky_renderer,
    make_dynamic_sampled_image_manager_create_context(app)
  });
}

void initialize_environment_components(App& app) {
  {
    auto init_res = app.environment_component.initialize();
    for (auto& pend : init_res.ambient_sound_init_res.pending_buffers) {
      app.audio_component.add_pending_audio_buffer(std::move(pend));
    }
  }

  app.weather_component.initialize({
    RainParticleRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.rain_particle_renderer
  });

  app.season_component = get_global_season_component();
}

void initialize_wind_component(App& app) {
  auto wind_init_res = app.wind_component.initialize({
    app.graphics_context.dynamic_sampled_image_manager,
    make_dynamic_sampled_image_manager_create_context(app)
  });
  if (wind_init_res.wind_displacement_image) {
    app.render_component.set_wind_displacement_image(wind_init_res.wind_displacement_image.value());
  }
}

void initialize_terrain_component(App& app) {
  app.terrain_component.initialize({
    app.graphics_context.sampled_image_manager,
    app.graphics_context.dynamic_sampled_image_manager,
    app.render_component.terrain_renderer,
    app.render_component.grass_renderer,
    make_dynamic_sampled_image_manager_create_context(app)
  });
}

void initialize_soil_component(App& app) {
  app.soil_component.initialize({
    app.graphics_context.dynamic_sampled_image_manager,
    make_dynamic_sampled_image_manager_create_context(app)
  });
}

void initialize_ui_components(App& app, const vk::GLFWContext& context) {
  app.ui_component.initialize();
  app.mouse.set_frame(context.monitor_content_scale_x, context.monitor_content_scale_y);
}

void initialize_bounds_component(App& app) {
  app.bounds_component.initialize({
    &app.bounds_system
  });

  auto* debug_bounds_tform = app.transform_system.create(
    TRS<float>::make_translation_scale(Vec3f{-16.0f, 8.0f, 0.0f}, Vec3f{8.0f}));
#if 1
  auto debug_bounds_tform_editor = editor::create_transform_editor(&app.editor, debug_bounds_tform, {});
#endif
  bounds::debug::create_debug_accel_instance(
    app.bounds_component.default_accel, debug_bounds_tform, debug_bounds_tform_editor);
}

void initialize_fog_component(App& app) {
  auto init_res = app.fog_component.initialize({
    app.transform_system,
    make_dynamic_sampled_image_manager_create_context(app),
    app.graphics_context.dynamic_sampled_image_manager
  });
  for (auto* tform : init_res.add_transform_editor) {
    app.editor.transform_editor.create_instance(
      tform,
      app.transform_system,
      app.editor.cursor_monitor,
      {});
  }
}

void initialize_debug_audio_parameter_events(App& app) {
#if 1
  auto create_res = debug::initialize_debug_audio_parameter_events({
    app.audio_component.audio_node_storage,
    *app.audio_component.get_ui_parameter_manager(),
    app.simple_audio_node_placement,
    app.audio_component.get_parameter_system(),
    app.terrain_component.get_terrain(),
    app.key_trigger
  });
  for (auto& port : create_res) {
    place_simple_audio_node_port(app.audio_port_placement, port);
  }
#else
  (void) app;
#endif
}

void initialize_arch_component(App& app) {
  assert(!app.arch_component);
  app.arch_component = get_global_arch_component();
  initialize_arch_component(app.arch_component, {
    &app.render_component.arch_renderer,
    app.debug_arch_component.bounds_arch_element_tag,
    app.debug_arch_component.arch_radius_limiter_element_tag
  });
}

void initialize_debug_arch_component(App& app) {
  auto init_res = app.debug_arch_component.initialize({
    &app.transform_system,
    ArchRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.arch_renderer,
    vk::PointBufferRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.point_buffer_renderer,
    ProceduralFlowerStemRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.procedural_flower_stem_renderer,
    app.graphics_context.sampled_image_manager,
    app.terrain_component.get_terrain(),
  });
  for (auto* tform : init_res.add_transform_editors) {
    app.editor.transform_editor.create_instance(
      tform,
      app.transform_system,
      app.editor.cursor_monitor,
      Vec3f{0.0f, 0.0f, 4.0f});
  }
}

void initialize_tree_systems(App& app) {
  tree::destroy_render_tree_system(&app.render_tree_system);
  app.render_tree_system = tree::create_render_tree_system();
  tree::initialize(app.render_tree_system, {});
}

void initialize_vine_systems(App& app) {
  tree::destroy_vine_system(&app.vine_system);
  app.vine_system = tree::create_vine_system();

  tree::destroy_render_vine_system(&app.render_vine_system);
  app.render_vine_system = tree::create_render_vine_system();
}

void initialize_root_systems(App& app) {
  bounds::destroy_radius_limiter(&app.roots_radius_limiter);
  app.roots_radius_limiter = bounds::create_radius_limiter();

  tree::destroy_roots_system(&app.roots_system);
  app.roots_system = tree::create_roots_system(app.roots_bounds_element_tag);

  tree::destroy_render_roots_system(&app.render_roots_system);
  app.render_roots_system = tree::create_render_roots_system();
}

void initialize_debug_procedural_tree_component(App& app) {
  app.debug_procedural_tree_component.initialize({
    ProceduralFlowerStemRenderer::make_add_resource_context(app.graphics_context),
    ArchRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.arch_renderer,
    app.render_component.procedural_flower_stem_renderer,
    app.terrain_component.get_terrain()
  });
}

void initialize_debug_procedural_tree_roots_component(App& app) {
  auto init_res = app.debug_procedural_tree_roots_component.initialize({
    app.roots_radius_limiter,
    app.roots_bounds_element_tag,
    ProceduralTreeRootsRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.procedural_tree_roots_renderer,
    app.transform_system,
    app.graphics_context.sampled_image_manager,
    &app.editor
  });
  (void) init_res;
}

void initialize_procedural_tree_component(App& app, int init_num_trees) {
  auto place_tform = app.transform_system.create(
    TRS<float>::make_translation(Vec3f{32.0f, 12.0f, -32.0f}));
  auto tform_inst = app.editor.transform_editor.create_instance(
    place_tform, app.transform_system, app.editor.cursor_monitor, {});
  app.editor.transform_editor.set_color(tform_inst, Vec3f{1.0f, 0.0f, 0.0f});
  (void) tform_inst;
  auto init_res = app.procedural_tree_component.initialize({
    place_tform,
    &app.tree_system,
    &app.tree_growth_system,
    app.audio_component.ui_audio_parameter_manager,
    app.audio_component.get_parameter_system(),
    app.keyboard,
    init_num_trees
  });
  if (init_res.key_listener) {
    app.key_trigger.add_listener(std::move(init_res.key_listener.value()));
  }
}

void initialize_tree_roots_component(App& app) {
  app.tree_roots_component = get_global_tree_roots_component();
  init_tree_roots_component(app.tree_roots_component, {
    app.roots_system,
    app.render_roots_system
  });
}

void initialize_lsystem_component(App& app) {
  app.lsystem_component = ls::create_lsystem_component();
}

void initialize_procedural_flower_component(App& app) {
  vk::PointBufferRenderer::DrawableParams point_params{};
  point_params.point_size = 4.0f;
  point_params.color = Vec3f{1.0f, 1.0f, 0.0f};
  Optional<vk::PointBufferRenderer::DrawableHandle> debug_oct_drawable{
    app.render_component.point_buffer_renderer.create_drawable(
      vk::PointBufferRenderer::DrawableType::Points, point_params)
  };

  auto init_res = app.procedural_flower_component.initialize({
    app.render_component.get_num_foliage_material1_alpha_texture_layers(),
    &app.audio_component.audio_transport,
    app.audio_component.audio_node_storage,
    app.audio_observation,
    app.simple_audio_node_placement,
    debug_oct_drawable,
    app.terrain_component.get_terrain()
  });

  for (auto& port : init_res.pending_placement) {
    place_simple_audio_node_port(app.audio_port_placement, port);
  }

  if (init_res.insert_audio_node_bounds_into_accel) {
    bool success = insert_audio_node_bounds_ignoring_handles(
      app, app.bounds_component.default_accel,
      init_res.insert_audio_node_bounds_into_accel.value());
    assert(success);
    (void) success;
  }
}

bool initialize(App& app, const cmd::Arguments& args) {
  if (!initialize_glfw(&app.glfw_context, &app, args)) {
    return false;
  }

  const bool gfx_res = initialize_graphics_context(
    &app.graphics_context, &app.opaque_graphics_context, app.glfw_context.window);
  if (!gfx_res) {
    return false;
  }
  if (!initialize_imgui(&app.imgui_impl, app.graphics_context, app.glfw_context.window)) {
    return false;
  }
  if (!initialize_audio_core(&app, args.initialize_default_audio_stream)) {
    return false;
  }

  initialize_camera(app);
  app.profile_component.initialize();
  initialize_render_component(app);
  initialize_editor(app);
  initialize_shadow_component(app);
  initialize_grass_component(app);
  initialize_sky_component(app);
  initialize_wind_component(app);
  initialize_terrain_component(app);
  initialize_model_component(app);
  initialize_soil_component(app);
  initialize_ui_components(app, app.glfw_context);
  initialize_bounds_component(app);
  initialize_fog_component(app);
  initialize_root_systems(app);
  initialize_tree_systems(app);
  initialize_vine_systems(app);
  initialize_procedural_tree_component(app, args.num_trees);
  initialize_tree_roots_component(app);
  initialize_lsystem_component(app);
  initialize_procedural_flower_component(app);
  initialize_debug_procedural_tree_component(app);
  initialize_debug_procedural_tree_roots_component(app);
  initialize_environment_components(app);
  initialize_debug_arch_component(app);
  initialize_arch_component(app);
  app.pollen_component.initialize();
  initialize_input(app);
  initialize_debug_audio_parameter_events(app);
  return true;
}

void update_editor(App& app, const Ray& cursor_ray) {
  editor::update(&app.editor, {
    SimpleShapeRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.simple_shape_renderer,
    cursor_ray,
    app.mouse_state.left_mouse_pressed,
    app.mouse_state.cursor_over_imgui_window || app.mouse_state.cursor_over_new_ui_window,
    app.key_state.is_super_pressed,
  });
}

void update_transform_system(App& app) {
  app.transform_system.update();
}

Vec2f get_ui_root_dimensions(const App& app) {
  Vec2f container_dimensions{
    float(app.graphics_context.swapchain.extent.width),
    float(app.graphics_context.swapchain.extent.height),
  };

  if (!app.render_component.prefer_to_render_ui_at_native_resolution &&
      vk::get_present_pass_enabled(&app.graphics_context)) {
    auto extent = vk::get_internal_forward_resolution(&app.graphics_context);
    container_dimensions.x = float(extent.width);
    container_dimensions.y = float(extent.height);
  }

  return container_dimensions;
}

void update_input(App& app) {
  app.mouse_state.left_mouse_clicked = false;
  app.mouse_state.right_mouse_clicked = false;
  app.mouse_state.cursor_over_imgui_window =
    app.imgui_component.enabled && vk::imgui_want_capture_mouse(&app.imgui_impl);

  app.mouse_button_trigger.update();
  app.key_trigger.update();
  app.controller.update();

  const bool left_pressed = app.mouse.is_pressed(Mouse::Button::Left);
  const bool right_pressed = app.mouse.is_pressed(Mouse::Button::Right);

  app.mouse_state.left_mouse_pressed = left_pressed;
  app.mouse_state.right_mouse_pressed = right_pressed;
  app.key_state.is_super_pressed = app.keyboard.is_pressed(super_key());

  const auto mouse_scroll = app.mouse.get_clear_scroll();
  {
    auto coords = app.mouse.get_coordinates();
    Vec2f raw_coords{float(coords.first), float(coords.second)};
    Vec2f scroll{float(mouse_scroll.first), float(mouse_scroll.second)};
    Vec2f fb_dims{
      float(app.glfw_context.framebuffer_width),
      float(app.glfw_context.framebuffer_height)
    };
    auto frac_dims = clamp_each(raw_coords / fb_dims, Vec2f{}, Vec2f{1.0f});
    auto pos = frac_dims * get_ui_root_dimensions(app);
    bool disabled = app.mouse_state.cursor_over_imgui_window;
    app.ui_component.begin_cursor_update(pos, scroll, left_pressed, right_pressed, disabled);
  }
}

void process_midi_stream_note_onsets(App& app, const MIDIMessageStreamSystemUpdateResult& res) {
  //  Make note onsets from e.g. timeline system visible to pitch sampling streams.
  if (!res.note_onsets.empty()) {
    const uint8_t ref_note_num = MIDINote::C3.note_number();
    auto* pitch_sys = app.audio_component.get_pitch_sampling_system();
    auto pitch_group = pss::ui_get_ith_group(
      pitch_sys, app.pitch_sampling_params.primary_pitch_sample_group_index);

    for (const auto& onset : res.note_onsets) {
      //  @NOTE: This ignores which midi stream the onset originated from.
      const uint8_t note_num = onset.note_number;
      pss::ui_push_triggered_note_numbers(pitch_sys, pitch_group, 0, &note_num, 1, ref_note_num);
    }
  }
}

AudioComponent::UpdateResult begin_update_audio_component(App& app, double real_dt) {
  const auto analysis_frame_callback = [&app](const auto& frame) {
    app.wind_component.wind.update_spectrum(frame);
  };
  auto res = app.audio_component.ui_begin_update({real_dt, std::move(analysis_frame_callback)});
  app.audio_observation.update(
    app.audio_component.ui_audio_parameter_manager, app.audio_component.audio_node_storage);
#if 1
  process_midi_stream_note_onsets(app, res.midi_message_stream_update_result);
#endif
  return res;
}

void end_update_audio_component(App& app, double real_dt, const AudioComponent::UpdateResult& res) {
  app.audio_component.ui_end_update(real_dt, res);
  ncsm::update(
    ncsm::get_global_control_note_clip_state_machine(),
    app.audio_component.get_note_clip_state_machine_system());
}

void begin_update_render_component(App& app) {
  vk::debug::reset_rendering();
  app.render_component.begin_update();
}

using UIConnectResult = UIAudioConnectionManager::UpdateResult;
using ConnectResult = AudioConnectionManager::UpdateResult;

void update_cursor_state(App& app, const UIPlane::HitInfo& ui_plane_hit_info) {
#if UI_PLANE_IN_WORLD_SPACE
  if (ui_plane_hit_info.hit && !app.mouse_state.cursor_hidden) {
    app.glfw_context.set_cursor_hidden(true);
    app.mouse_state.cursor_hidden = true;

  } else if (!ui_plane_hit_info.hit && app.mouse_state.cursor_hidden) {
    app.glfw_context.set_cursor_hidden(false);
    app.mouse_state.cursor_hidden = false;
  }
#else
  (void) app;
  (void) ui_plane_hit_info;
#endif
}

void update_ui(App& app) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("App/new_ui");
  (void) profiler;

  const auto container_dimensions = get_ui_root_dimensions(app);

  ui::AudioEditorCommonContext audio_editor_context{
    app.audio_component,
    app.ui_audio_connection_manager,
    *track::get_global_ui_track_system(),
    app.pitch_sampling_params,
    app.rhythm_params,
    *ncsm::get_global_control_note_clip_state_machine(),
    app.key_trigger,
    app.mouse_button_trigger,
    app.selected_instrument_components,
    *app.ui_component.cursor_state,
    *gui::get_global_gui_render_data(),
    container_dimensions,
    app.new_audio_editor_data.hidden || !app.params.menu_ui_hidden || !app.params.screen0_hidden,
    app.new_audio_editor_data.mode
  };

  gui::WorldGUIContext world_gui_context{
    container_dimensions,
    gui::get_global_gui_render_data(),
    *app.ui_component.cursor_state,
    app.key_trigger,
    app.params.world_ui_hidden || !app.params.menu_ui_hidden || !app.params.screen0_hidden,
    app.procedural_tree_component,
    *app.tree_roots_component,
    app.debug_procedural_tree_roots_component,
    app.procedural_flower_component,
    *app.arch_component,
    app.debug_arch_component
  };

  gui::MenuGUIResult menu_gui_result{};
  gui::MenuGUIContext menu_gui_context{
    &menu_gui_result,
    gui::get_global_menu_gui_data(),
    container_dimensions,
    gui::get_global_gui_render_data(),
    *app.ui_component.cursor_state,
    app.audio_component,
    app.graphics_context,
    *gfx::get_global_quality_preset_system(),
    app.params.menu_ui_hidden || !app.params.screen0_hidden
  };

  gui::Screen0GUIResult screen0_gui_result{};
  gui::Screen0GUIContext screen0_gui_context{
    &screen0_gui_result,
    container_dimensions,
    gui::get_global_gui_render_data(),
    *app.ui_component.cursor_state,
    app.params.screen0_hidden
  };

  gui::TutorialGUIResult tutorial_gui_result{};
  gui::TutorialGUIContext tutorial_gui_context{
    &tutorial_gui_result,
    container_dimensions,
    gui::get_global_gui_render_data(),
    *app.ui_component.cursor_state,
    app.params.tutorial_ui_hidden || !app.params.screen0_hidden,
  };

  ui::prepare_audio_editors(app.new_audio_editor_data, audio_editor_context);
  gui::prepare_world_gui(world_gui_context);
  gui::prepare_menu_gui(menu_gui_context);
  gui::prepare_screen0_gui(screen0_gui_context);
  gui::prepare_tutorial_gui(tutorial_gui_context);
  //  @NOTE - end cursor update after preparing guis
  app.ui_component.end_cursor_update();

  //  @TODO - this should probably happen in update_input() rather than here.
  app.mouse_state.cursor_over_new_ui_window =
    gui::cursor::hovered_over_any(app.ui_component.cursor_state);

  ui::evaluate_audio_editors(app.new_audio_editor_data, audio_editor_context);
  ui::render_audio_editors(app.new_audio_editor_data, audio_editor_context);

  gui::evaluate_world_gui(world_gui_context);
  gui::render_world_gui(world_gui_context);

  gui::evaluate_menu_gui(menu_gui_context);
  gui::render_menu_gui(menu_gui_context);

  gui::evaluate_screen0_gui(screen0_gui_context);
  gui::render_screen0_gui(screen0_gui_context);

  gui::evaluate_tutorial_gui(tutorial_gui_context);
  gui::render_tutorial_gui(tutorial_gui_context);

  if (screen0_gui_result.close_screen) {
    app.params.screen0_hidden = true;
  }
  if (tutorial_gui_result.close_screen) {
    app.params.tutorial_ui_hidden = true;
  }

  if (menu_gui_result.close_gui) {
    app.params.menu_ui_hidden = true;
  }
  if (menu_gui_result.quit_app) {
    app.params.need_quit = true;
  }
  if (menu_gui_result.enable_tutorial_gui) {
    gui::jump_to_first_tutorial_gui_slide();
    app.params.tutorial_ui_hidden = false;
  }
}

UIConnectResult
update_ui_audio_connection_manager(App& app, const ConnectResult& audio_connect_result) {
  auto ui_connect_update_result = app.ui_audio_connection_manager.update({
    &app.audio_component.audio_node_storage,
    &app.audio_component.audio_connection_manager,
    &app.audio_port_placement,
    &app.cable_path_finder,
    &app.selected_instrument_components,
    audio_connect_result.new_connections,
    audio_connect_result.new_disconnections
  });
  return ui_connect_update_result;
}

void update_debug_audio_systems(App& app, const ConnectResult& audio_connect_result) {
#if 1
  debug::update_debug_audio_parameter_events({
    app.audio_component.audio_node_storage,
    *app.audio_component.get_ui_parameter_manager(),
    app.simple_audio_node_placement,
    app.audio_component.get_parameter_system(),
    app.terrain_component.get_terrain(),
    app.key_trigger
  });
#else
  (void) app;
#endif
  audio::debug::update_node_connection_representation({
    app.audio_port_placement,
    app.selected_instrument_components,
    tree::get_global_resource_spiral_around_nodes_system(),
    app.audio_component.audio_node_storage,
    app.audio_component.get_node_signal_value_system(),
    audio_connect_result,
  });
}

struct UpdateCameraResult {
  Ray mouse_ray;
};

UpdateCameraResult update_camera(App& app, double real_dt) {
  const bool allow_movement =
    app.params.keyboard_moves_camera &&
    !app.key_trigger.is_pressed(Key::LeftAlt);
  app.controller.allow_movement = allow_movement;
  auto cam_res = app.camera_component.update({
    app.camera,
    app.controller,
    app.glfw_context.window_aspect_ratio(),
    app.terrain_component.get_terrain(),
    real_dt
  });

#if 1
  auto& terrain_renderer = app.render_component.terrain_renderer;
  terrain_renderer.prefer_inverted_winding_new_material_pipeline = cam_res.is_below_ground;
#endif

  const auto mouse_coords = app.mouse.get_coordinates();
  UpdateCameraResult result{};
  result.mouse_ray = make_mouse_ray(
    float(mouse_coords.first),
    float(mouse_coords.second),
    float(app.glfw_context.framebuffer_width),
    float(app.glfw_context.framebuffer_height),
    app.camera);
  return result;
}

void update_shadow_component(App& app) {
  auto& sun = app.sky_component.get_sun();
  app.shadow_component.update(app.camera, sun.position);
}

void update_grass_component(App& app, const weather::Status& weather_status) {
  auto update_res = app.grass_component.update({
    app.camera,
    0.0f,
    app.camera.get_position(),
    weather_status
  });

  auto& render_params = app.render_component.grass_renderer.get_render_params();
  render_params.min_shadow = update_res.min_shadow;
  render_params.global_color_scale = update_res.global_color_scale;
  render_params.frac_global_color_scale = update_res.frac_global_color_scale;
}

void update_grass_renderer(App& app, const season::Status& status) {
  auto& sun = app.sky_component.get_sun();
  auto& params = app.render_component.grass_renderer.get_render_params();
  params.sun_position = sun.position;
  params.sun_color = sun.color;
  params.wind_world_bound_xz = app.wind_component.wind.world_bound_xz();
  params.terrain_grid_scale = Terrain::terrain_dim;

  const float frac_fall = status.current == season::Season::Fall ?
    1.0f - status.frac_next : status.frac_next;
  auto mat_params = GrassRenderer::NewMaterialParams::from_frac_fall(
    frac_fall, params.prefer_revised_new_material_params);
  params.season_controlled_new_material_params = mat_params;
}

void update_arch_renderer(App& app) {
  auto& sun = app.sky_component.get_sun();
  auto& params = *app.render_component.arch_renderer.get_render_params();
  params.sun_position = sun.position;
  params.sun_color = sun.color;
}

void update_render_component(App& app, double current_time) {
  auto& sun = app.sky_component.get_sun();
  auto& params = app.render_component.common_render_params;
  params.sun_color = sun.color;
  params.sun_position = sun.position;
  params.wind_world_bound_xz = app.wind_component.wind.world_bound_xz();
  params.wind_displacement_limits = app.wind_component.approx_displacement_limits();
  params.branch_wind_strength_limits = app.wind_component.render_axis_strength_limits();
  params.elapsed_time = float(current_time);
}

void update_procedural_tree_roots_renderer(App& app, double) {
  auto& sun = app.sky_component.get_sun();
  auto& params = app.render_component.procedural_tree_roots_renderer.get_render_params();
  params.sun_color = sun.color;
  params.sun_position = sun.position;
}

void update_procedural_flower_stem_renderer(App& app, double current_time) {
  auto& sun = app.sky_component.get_sun();
  auto& params = app.render_component.procedural_flower_stem_renderer.get_render_params();
  params.wind_world_bound_xz = app.wind_component.wind.world_bound_xz();
  params.sun_color = sun.color;
  params.elapsed_time = float(current_time);
}

void update_static_model_renderer(App& app) {
  auto& sun = app.sky_component.get_sun();
  auto& params = app.render_component.static_model_renderer.get_render_params();
  params.sun_position = sun.position;
  params.sun_color = sun.color;
}

void update_terrain_renderer(App& app) {
  auto& sun = app.sky_component.get_sun();
  auto& params = app.render_component.terrain_renderer.get_render_params();
  params.terrain_dim = Terrain::terrain_dim;
  params.sun_position = sun.position;
  params.sun_color = sun.color;
  params.wind_world_bound_xz = app.wind_component.wind.world_bound_xz();
}

void update_sky_component(App& app, const weather::Status& weather_status) {
  auto update_res = app.sky_component.update({
    app.graphics_context.dynamic_sampled_image_manager,
    weather_status
  });
  if (update_res.sky_image) {
    app.render_component.sky_renderer.set_color_image(update_res.sky_image.value());
  }
}

void update_model_component(App& app) {
  editor::UIRenderer::DrawContext draw_context{
    SimpleShapeRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.simple_shape_renderer
  };
  auto update_res = app.model_component.update({
    draw_context,
    app.editor.ui_renderer,
    app.render_component.static_model_renderer
  });
  (void) update_res;
}

void update_terrain_components(App& app, const weather::Status& weather_status, double real_dt) {
  {
    auto update_res = app.terrain_component.update({
      weather_status,
      app.graphics_context.sampled_image_manager
    });

    auto& render_params = app.render_component.terrain_renderer.get_render_params();
    render_params.global_color_scale = update_res.global_color_scale;
    render_params.min_shadow = update_res.min_shadow;
    render_params.frac_global_color_scale = update_res.frac_global_color_scale;

    if (update_res.new_material_image_handle) {
      app.render_component.grass_renderer.set_alt_terrain_color_image(
        update_res.new_material_image_handle.value());
      app.render_component.terrain_renderer.set_new_material_image(
        update_res.new_material_image_handle.value());
    }
  }
  {
    constexpr int max_num_trees = 512;
    Bounds3f tree_aabbs[max_num_trees];
    Vec3f tree_base_positions[max_num_trees];
    int num_trees{};

    if (auto* trees = app.procedural_tree_component.maybe_read_trees()) {
      for (auto& [_, tree] : *trees) {
        auto inst = tree::read_tree(&app.tree_system, tree.instance);
        if (num_trees < max_num_trees && inst.src_aabb && inst.nodes) {
          tree_aabbs[num_trees] = *inst.src_aabb;
          tree_base_positions[num_trees] = inst.nodes->origin();
          num_trees++;
        }
      }
    }

    OBB3f wall_bounds[128];
    const int num_wall_bounds = app.debug_arch_component.gather_wall_bounds(wall_bounds, 128);

    auto res = app.debug_terrain_component.update({
      real_dt,
      tree_aabbs,
      tree_base_positions,
      num_trees,
      wall_bounds,
      num_wall_bounds,
      &app.bounds_system,
      app.bounds_component.default_accel,
      app.roots_radius_limiter,
      app.render_component.static_model_renderer,
      StaticModelRenderer::make_add_resource_context(app.graphics_context),
      app.render_component.terrain_renderer,
      TerrainRenderer::make_add_resource_context(app.graphics_context),
      app.render_component.procedural_tree_roots_renderer,
      ProceduralTreeRootsRenderer::make_add_resource_context(app.graphics_context),
      app.graphics_context.sampled_image_manager,
      app.transform_system,
      app.terrain_component.get_terrain(),
    });
#if 0
    for (int i = 0; i < res.num_add; i++) {
      auto& add = res.add_tform_editors[i];
      auto inst = app.editor.transform_editor.create_instance(
        add.inst,
        app.transform_system,
        app.editor.cursor_monitor,
        Vec3f{16.0f, 8.0f, 0.0f});
      app.editor.transform_editor.set_color(inst, add.color);
    }
#endif
    if (res.new_splotch_image) {
      app.render_component.terrain_renderer.set_splotch_image(res.new_splotch_image.value());
    }
    if (res.new_ground_color_image) {
      app.render_component.terrain_renderer.set_alt_color_image(res.new_ground_color_image.value());
    }
  }
}

void update_soil_component(App& app) {
  auto update_res = app.soil_component.update({
    app.graphics_context.dynamic_sampled_image_manager,
    app.camera.get_position_xz()
  });
  if (update_res.show_debug_image) {
    app.render_component.debug_image_renderer.push_drawable(
      update_res.show_debug_image.value(),
      update_res.debug_image_params);
  }
}

void begin_update_projected_nodes_system(App& app) {
  begin_update(&app.projected_nodes_system);
}

void update_projected_nodes_systems(App& app, double real_dt) {
  update(&app.projected_nodes_system, {real_dt});
}

void update_arch_component(App& app, double real_dt, const Ray& mouse_ray) {
  const tree::Internode* proj_internodes{};
  int num_proj_internodes{};
  if (auto* proj_inodes = app.debug_arch_component.get_projection_source_internodes()) {
    proj_internodes = proj_inodes->data();
    num_proj_internodes = int(proj_inodes->size());
  }

  update_arch_component(app.arch_component, {
    real_dt,
    &app.render_component.arch_renderer,
    &app.tree_system,
    app.roots_system,
    &app.projected_nodes_system,
    app.vine_system,
    app.render_vine_system,
    app.bounds_component.default_accel,
    &app.bounds_system,
    app.debug_arch_component.isect_wall_obb,
    app.roots_radius_limiter,
    mouse_ray,
    app.mouse_state.left_mouse_clicked,
    proj_internodes,
    num_proj_internodes
  });
}

void update_debug_arch_component(App& app, const Ray& mouse_ray, double real_dt,
                                 const tree::TreeSystem::UpdateResult& tree_sys_res) {
  app.debug_arch_component.update({
    &app.projected_nodes_system,
    ArchRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.arch_renderer,
    vk::PointBufferRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.point_buffer_renderer,
    ProceduralFlowerStemRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.procedural_flower_stem_renderer,
    app.terrain_component.get_terrain(),
    app.debug_terrain_component.get_terrain_bounds_element_tag(),
    real_dt,
    app.procedural_tree_component.centroid_of_tree_origins(),
    app.tree_system,
    app.bounds_system,
    app.bounds_component.default_accel,
    app.roots_radius_limiter,
    app.roots_bounds_element_tag,
    *tree_sys_res.just_deleted,
    mouse_ray,
    app.mouse_state.left_mouse_clicked && !app.mouse_state.cursor_over_imgui_window,
  });
}

void update_soil_parameter_modulator(App& app) {
  auto& node_storage = app.audio_component.audio_node_storage;
  auto& sel_components = app.selected_instrument_components;
  auto selected_node = sel_components.first_selected_node_id(node_storage);
  auto soil_quality = app.soil_component.get_soil()->sample_quality01(
    app.camera.get_position_xz(), 12.0f);

  soil::update_parameter_modulator(app.soil_parameter_modulator, {
    app.audio_component.ui_audio_parameter_manager,
    app.audio_component.get_parameter_system(),
    app.audio_component.audio_node_storage,
    selected_node,
    soil_quality
  });
}

void update_wind_component(App& app, double real_dt) {
  app.wind_component.update({
    app.graphics_context.dynamic_sampled_image_manager,
    app.camera.get_position(),
    real_dt
  });
}

[[nodiscard]] season::StatusAndEvents update_season_component(App& app, double) {
  auto res = grove::update_season_component(app.season_component);
  return res.status_and_events;
}

[[nodiscard]] weather::Status update_weather_component(App& app, double real_dt) {
  auto res = app.weather_component.update({
    app.render_component.rain_particle_renderer,
    app.camera,
    app.wind_component.wind,
    app.camera.get_position(),
    real_dt
  });

  for (auto& deposit : res.soil_deposits) {
    app.soil_component.get_soil()->add_quality01(
      deposit.position, deposit.radius, deposit.amount);
  }

  return res.weather_status;
}

void update_environment_components(App& app, const weather::Status& weather_status, double real_dt) {
  auto& env_component = app.environment_component;
  auto& triggered_buff_renderer = app.audio_component.triggered_buffer_renderer;

  auto res = env_component.update({
    weather_status,
    *app.audio_component.ui_audio_scale.get_tuning()
  });

  for (auto& to_play : res.ambient_sound_update_res.to_play) {
    *to_play.assign_instance = triggered_buff_renderer.ui_play(to_play.handle, to_play.params);
  }

  for (auto& mod : res.ambient_sound_update_res.triggered_modifications) {
    triggered_buff_renderer.ui_set_modification(std::move(mod));
  }

  if (res.new_tuning) {
    if (app.params.tuning_controlled_by_environment) {
      app.audio_component.ui_audio_scale.set_tuning(res.new_tuning.value());
    }
    //  only copy over change to frequency
    auto* scale_sys = app.audio_component.get_audio_scale_system();
    auto curr_tuning = *scale_system::ui_get_tuning(scale_sys);
    curr_tuning.reference_frequency = res.new_tuning.value().reference_frequency;
    scale_system::ui_set_tuning(scale_sys, curr_tuning);
  }

  env::update_environment_instruments({
    app.audio_component,
    app.simple_audio_node_placement,
    app.audio_port_placement,
    app.rhythm_params,
    app.pitch_sampling_params,
    app.terrain_component.get_terrain(),
    real_dt,
    weather_status
  });

  env::begin_update(env::get_global_global_sound_control(), {
    app.audio_component,
    *ncsm::get_global_control_note_clip_state_machine(),
    app.pitch_sampling_params,
    weather_status
  });
}

UIPlane::HitInfo begin_update_ui_plane_component(App& app, const Ray& mouse_ray) {
  const auto& terrain = app.terrain_component.get_terrain();
  const Vec3f plane_ori = app.ui_plane_component.get_ui_plane_center();
  auto height_at_plane_ori = terrain.height_nearest_position_xz(plane_ori);

  const auto mouse_coords = app.mouse.get_coordinates();
  const auto win_dims = Vec2<double>{
    double(app.glfw_context.framebuffer_width),
    double(app.glfw_context.framebuffer_height)};

  auto update_res = app.ui_plane_component.begin_update({
    mouse_ray,
    height_at_plane_ori,
    Vec2<double>{mouse_coords.first, mouse_coords.second},
    win_dims
  });

  return update_res.ui_plane_hit_info;
}

void update_fog_component(App& app, const weather::Status& weather_status, double real_dt) {
  auto cam_pos = app.camera.get_position();
  Vec2f cam_pos_xz{cam_pos.x, cam_pos.z};
  app.fog_component.update({
    CloudRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.cloud_renderer,
    make_dynamic_sampled_image_manager_create_context(app),
    app.graphics_context.dynamic_sampled_image_manager,
    real_dt,
    app.wind_component.wind.get_dominant_wind_direction(),
    app.wind_component.wind.wind_force01_no_spectral_influence(cam_pos_xz),
    weather_status,
    app.camera,
    app.terrain_component.get_terrain(),
    app.wind_component.wind
  });
}

void update_bounds_system(App& app) {
  bounds::update(&app.bounds_system);
  bounds::debug::update_debug_bounds_system({&app.editor, &app.bounds_system});
}

void update_debug_tree_roots_component(App& app, double real_dt) {
  auto instr_update_res = tree::update_roots_spectrum_growth_instrument({
    app.audio_component,
    app.simple_audio_node_placement,
    app.audio_port_placement,
    app.pitch_sampling_params,
    app.terrain_component.get_terrain(),
  });

  if (instr_update_res.new_spectral_fraction) {
    app.debug_procedural_tree_roots_component.set_spectral_fraction(
      instr_update_res.new_spectral_fraction.value());
  }

  Temporary<Vec3f, 256> tmp_newly_created_origins;
  Vec3f* origins;
  int num_origins{};

  {
    auto newly_created = app.procedural_tree_component.read_newly_created();
    origins = tmp_newly_created_origins.require(int(newly_created.size()));

    if (auto* trees = app.procedural_tree_component.maybe_read_trees()) {
      for (auto& id : newly_created) {
        if (auto it = trees->find(id); it != trees->end()) {
          origins[num_origins++] = it->second.origin;
        }
      }
    }
  }

  app.debug_procedural_tree_roots_component.update({
    &app.editor,
    app.roots_radius_limiter,
    app.roots_bounds_element_tag,
    ProceduralTreeRootsRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.procedural_tree_roots_renderer,
    real_dt,
    origins,
    num_origins,
    app.camera.get_position(),
    app.wind_component.wind,
    app.terrain_component.get_terrain(),
    app.wind_component.wind_displacement,
    app.terrain_component.world_aabb()
  });
}

void update_lsystem_component(App& app) {
  ls::update_lsystem_component(app.lsystem_component, {
    app.render_component.procedural_tree_roots_renderer,
    ProceduralTreeRootsRenderer::make_add_resource_context(app.graphics_context),
    app.terrain_component.get_terrain()
  });
}

void update_debug_procedural_tree_component(App& app, const Ray& mouse_ray, double real_dt) {
  auto roots_renderer_context = ProceduralTreeRootsRenderer::make_add_resource_context(app.graphics_context);

  const auto* radius_lim = app.roots_radius_limiter;
  const auto* accel = request_read(
    &app.bounds_system,
    app.bounds_component.default_accel,
    app.debug_procedural_tree_component.bounds_accessor_id);

  auto update_res = app.debug_procedural_tree_component.update({
    roots_renderer_context,
    app.render_component.procedural_tree_roots_renderer,
    app.wind_component.wind,
    app.procedural_tree_component,
    &app.tree_message_system,
    app.vine_system,
    &app.tree_system,
    app.render_tree_system,
    tree::get_global_branch_nodes_data(),
    radius_lim,
    app.roots_system,
    accel,
    tree::get_global_resource_spiral_around_nodes_system(),
    app.camera,
    mouse_ray,
    real_dt
  });

  if (accel) {
    bounds::release_read(
      &app.bounds_system,
      app.bounds_component.default_accel,
      app.debug_procedural_tree_component.bounds_accessor_id);
  }

  app.render_component.set_foliage_occlusion_system_modified(
    update_res.occlusion_system_data_structure_modified,
    update_res.occlusion_system_clusters_modified);
  if (update_res.set_tree_leaves_renderer_enabled) {
    app.render_component.set_tree_leaves_renderer_enabled(
      update_res.set_tree_leaves_renderer_enabled.value());
  }
}

void update_vine_systems(App& app, double real_dt) {
  tree::update_vine_system(app.vine_system, {
    &app.tree_system,
    app.render_vine_system,
    &app.bounds_system,
    app.bounds_component.default_accel,
    app.debug_arch_component.bounds_arch_element_tag,
    real_dt
  });
  auto res = tree::update_ornamental_foliage_on_vines({
    app.vine_system,
    &app.tree_system,
    foliage::get_global_ornamental_foliage_data()
  });
  if (res.num_finished_growing > 0) {
    play_midi_notes(app, res.num_finished_growing);
  }
}

void update_root_systems(App& app, double real_dt) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("update_root_systems");
  (void) profiler;

  const auto& db_roots_comp = app.debug_procedural_tree_roots_component;
  const auto& db_roots_params = db_roots_comp.params;

  tree::set_global_growth_rate_scale(app.roots_system, db_roots_params.growth_rate);
  tree::set_global_attractor_point(app.roots_system, db_roots_comp.get_attractor_point());
  tree::set_global_attractor_point_scale(app.roots_system, db_roots_params.attractor_point_scale);
  tree::set_attenuate_growth_rate_by_spectral_fraction(
    app.roots_system, db_roots_params.scale_growth_rate_by_signal);
  tree::set_spectral_fraction(app.roots_system, db_roots_comp.spectral_fraction);
  tree::set_prefer_global_p_spawn_lateral_branch(
    app.roots_system, db_roots_comp.params.prefer_global_p_spawn_lateral);
  tree::set_global_p_spawn_lateral_branch(
    app.roots_system, db_roots_comp.params.p_spawn_lateral);

  auto root_sys_update_res = tree::update_roots_system(app.roots_system, {
    app.roots_radius_limiter,
    real_dt
  });

  tree::update_render_roots_system(app.render_roots_system, {
    app.roots_system,
    tree::get_global_branch_nodes_data(),
    cull::get_global_branch_nodes_frustum_cull_data()
  });

  auto& pollen = app.pollen_component.pollen_particles;
  for (int i = 0; i < root_sys_update_res.num_new_branch_infos; i++) {
    auto& branch_info = root_sys_update_res.new_branch_infos[i];
    (void) pollen.create_particle(branch_info.position);
  }

  const int num_new = root_sys_update_res.num_new_branches;
  if (num_new > 0) {
    play_quantized_midi_notes(app, num_new, audio::Quantization::Eighth, false);
  }

  tree::update_roots_branch_spawn_instrument({
    app.audio_component,
    app.simple_audio_node_placement,
    app.audio_port_placement,
    app.pitch_sampling_params,
    app.terrain_component.get_terrain()
  }, root_sys_update_res.new_branch_infos, num_new);
}

void end_update_root_systems(App& app) {
  tree::end_update_roots_system(app.roots_system);
}

void update_resource_spiral_around_nodes(App& app, double real_dt) {
  const auto& comp = app.procedural_tree_component;

  auto* sys = tree::get_global_resource_spiral_around_nodes_system();
  tree::set_global_velocity_scale(sys, 0, comp.resource_spiral_global_particle_velocity);
  tree::set_global_theta(sys, 0, comp.resource_spiral_global_particle_theta);

  tree::set_global_velocity_scale(sys, 2, 4.0f);
  tree::set_global_theta(sys, 2, -pif() * 0.5f);

  //  allow instrument to override global settings
  auto pitch_group = pss::ui_get_ith_group(
    pss::get_global_pitch_sampling_system(),
    app.pitch_sampling_params.secondary_pitch_sample_group_index);
  auto res = tree::update_resource_flow_along_nodes_instrument(
    sys, app.audio_component, app.simple_audio_node_placement,
    app.audio_port_placement, pitch_group, app.terrain_component.get_terrain(), real_dt);

  if (res.insert_node_bounds_into_accel) {
    const auto accel_handle = app.bounds_component.default_accel;
    auto& bounds = res.insert_node_bounds_into_accel.value();
    if (insert_audio_node_bounds_ignoring_handles(app, accel_handle, bounds)) {
      *res.acknowledge_inserted = true;
    }
  }

  tree::update_resource_spiral_around_nodes(sys, {
    &app.tree_system,
    app.roots_system,
    real_dt,
    app.camera.get_position()
  });
  int num_contexts{};
  auto* contexts = tree::read_contexts(sys, &num_contexts);
  particle::push_resource_flow_along_nodes_particles(contexts, num_contexts);
}

[[nodiscard]] tree::TreeSystem::UpdateResult update_tree_systems(App& app, double real_dt) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("update_tree_systems");
  (void) profiler;

  auto update_res = tree::update(&app.tree_system, {
#if GROVE_INCLUDE_TREE_INTERNODES_IN_RADIUS_LIMITER
    app.roots_radius_limiter,
    app.roots_bounds_element_tag,
#endif
    &app.tree_growth_system,
    &app.tree_accel_insert_and_prune,
    &app.bounds_system,
    real_dt
  });
  tree::update(&app.tree_growth_system);
  tree::update(&app.tree_accel_insert_and_prune, {
    &app.bounds_system,
  });
  tree::update(&app.tree_message_system, {
    &app.bounds_system,
    app.bounds_component.default_accel,
    &app.tree_system,
    update_res.just_deleted,
    real_dt
  });
  auto render_tree_sys_update_res = tree::update(app.render_tree_system, {
    &app.tree_system,
    &app.bounds_system,
    app.debug_procedural_tree_component.get_foliage_occlusion_system(),
    cull::get_global_tree_leaves_frustum_cull_data(),
    cull::get_global_branch_nodes_frustum_cull_data(),
    tree::get_global_branch_nodes_data(),
    real_dt
  });
  tree::debug::update_debug_growth_contexts({
    &app.tree_growth_system,
    app.render_component.point_buffer_renderer,
    vk::PointBufferRenderer::make_add_resource_context(app.graphics_context)
  });
  update_resource_spiral_around_nodes(app, real_dt);
  if (render_tree_sys_update_res.num_just_reached_leaf_season_change_target > 0) {
    play_midi_notes(app, render_tree_sys_update_res.num_just_reached_leaf_season_change_target);
  }
  return update_res;
}

void begin_update_procedural_tree_component(App& app) {
  const auto bpm11 = float(
    (clamp(app.audio_component.audio_transport.get_bpm(), 0.0, 240.0) - 120.0) / 120.0);
  app.procedural_tree_component.begin_update({
    app.audio_component.get_node_signal_value_system(),
    app.params.tuning_controlled_by_environment,
    bpm11,
    &app.tree_system,
  });
}

void update_procedural_tree_component(
  App& app, const PollenParticles::UpdateResult& pollen_update_res,
  const AudioConnectionManager::UpdateResult& connect_update_res,
  const ni::AudioNodeIsolatorUpdateResult& node_isolator_update_res,
  const season::StatusAndEvents& season_status, double real_dt) {
  //
  app.procedural_tree_component.evaluate_audio_node_isolator_update_result(
    app.render_tree_system,
    node_isolator_update_res.newly_will_activate,
    node_isolator_update_res.newly_will_deactivate);

  auto update_res = app.procedural_tree_component.update({
    &app.tree_system,
    app.render_tree_system,
    &app.tree_growth_system,
    &app.tree_message_system,
    app.vine_system,
    &app.bounds_system,
    app.bounds_component.default_accel,
    app.camera,
    app.terrain_component.get_terrain(),
    *app.soil_component.get_soil(),
    real_dt,
    pollen_update_res,
    app.wind_component.wind,
    app.audio_component.audio_node_storage,
    app.audio_observation,
    app.audio_component.audio_scale,
    connect_update_res,
    app.audio_component.ui_audio_parameter_manager,
    app.audio_component.get_parameter_system(),
    season_status
  });

  for (auto& info : update_res.pending_placement) {
    auto create_res = app.simple_audio_node_placement.create_node(
      info->node_id, info->port_info, info->position, info->y_offset);
    for (auto& port : create_res) {
      place_simple_audio_node_port(app.audio_port_placement, port);
    }
  }

  if (!update_res.pending_placement.empty()) {
    const int n = int(update_res.pending_placement.size());
    play_midi_notes(app, n);
//    play_quantized_midi_notes(app, n, audio::Quantization::Sixteenth, true);
  }

  if (update_res.num_leaves_finished_growing > 0) {
    const int n = update_res.num_leaves_finished_growing;
    play_midi_notes(app, n);
//    play_quantized_midi_notes(app, n, audio::Quantization::Sixteenth, true);
  }

  for (auto& release : update_res.release_parameter_writes) {
    auto& write_access = *param_system::ui_get_write_access(
      app.audio_component.get_parameter_system());
    write_access.release(release.writer_id, release.param_ids);
  }

  for (auto& remove : update_res.nodes_to_delete) {
    auto& param_monitor = app.audio_observation.parameter_monitor;
    param_monitor.remove_node(remove.id, app.audio_component.ui_audio_parameter_manager);

    (void) app.audio_component.audio_connection_manager.maybe_delete_node(remove.id);
    if (remove.remove_placed_node) {
      remove_placed_audio_node(app, remove.id);
    }
  }

  for (auto& particle : update_res.spawn_pollen_particles) {
    auto& pollen = app.pollen_component.pollen_particles;
    auto part = pollen.create_particle(particle.position);
    if (particle.enable_tree_spawn) {
      app.procedural_tree_component.register_pollen_particle(part.id);
    }
  }

  for (auto& patch : update_res.new_ornamental_foliage_patches) {
    app.procedural_flower_component.add_patch(patch.position);
  }

  if (update_res.toggle_debug_attraction_points_drawable) {
    app.render_component.point_buffer_renderer.toggle_active_drawable(
      update_res.toggle_debug_attraction_points_drawable.value());
  }

  for (auto& deposit : update_res.soil_deposits) {
    app.soil_component.get_soil()->add_quality01(
      deposit.position, deposit.radius, deposit.amount);
  }

  if (update_res.num_began_dying > 0) {
    if (auto buff = app.audio_component.audio_buffers.find_by_name("chime_c3.wav")) {
      for (int i = 0; i < std::min(16, update_res.num_began_dying); i++) {
        auto rm = semitone_to_rate_multiplier(urand_11() * 8.0);
        TriggeredBufferPlayParams params{};
        params.playback_rate_multiplier = rm;
        params.gain = float(db_to_amplitude(-8.0));
        params.timeout_s = float(urand() * 100e-3);
        app.audio_component.triggered_buffer_renderer.ui_play(buff.value(), params);
      }
    }
  }
}

void update_tree_roots_component(App& app, double) {
  Temporary<Vec3f, 256> tmp_newly_created_origins;
  Vec3f* origins;
  int num_origins{};
  {
    auto newly_created = app.procedural_tree_component.read_newly_created();
    origins = tmp_newly_created_origins.require(int(newly_created.size()));
    if (auto* trees = app.procedural_tree_component.maybe_read_trees()) {
      for (auto& id : newly_created) {
        if (auto it = trees->find(id); it != trees->end()) {
          origins[num_origins++] = it->second.origin;
        }
      }
    }
  }

  update_tree_roots_component(app.tree_roots_component, {
    app.roots_system,
    app.render_roots_system,
    tree::get_global_resource_spiral_around_nodes_system(),
    origins,
    num_origins,
    app.debug_procedural_tree_roots_component.params.allow_recede,
    app.terrain_component.get_terrain()
  });
}

void update_procedural_flower_component(App& app, double real_dt) {
  auto update_res = app.procedural_flower_component.update({
    foliage::get_global_ornamental_foliage_data(),
    ProceduralFlowerStemRenderer::make_add_resource_context(app.graphics_context),
    app.render_component.procedural_flower_stem_renderer,
    app.terrain_component.get_terrain(),
    app.wind_component.wind,
    real_dt,
    app.procedural_tree_component.get_place_tform_translation()
  });

  for (auto& particle : update_res.spawn_pollen_particles) {
    auto& pollen = app.pollen_component.pollen_particles;
    auto part = pollen.create_particle(particle.position);
    (void) part;
  }

  if (update_res.update_debug_attraction_points) {
    app.render_component.point_buffer_renderer.update_instances(
      vk::PointBufferRenderer::make_add_resource_context(app.graphics_context),
      update_res.update_debug_attraction_points.value().handle,
      update_res.update_debug_attraction_points.value().points.data(),
      int(update_res.update_debug_attraction_points.value().points.size()));
  }

  if (update_res.toggle_debug_attraction_points_drawable) {
    app.render_component.point_buffer_renderer.toggle_active_drawable(
      update_res.toggle_debug_attraction_points_drawable.value());
  }

  const int num_finished_growing = std::min(16, update_res.num_ornaments_finished_growing);
  if (num_finished_growing > 0) {
    play_midi_notes(app, num_finished_growing);
  }
}

PollenParticles::UpdateResult update_pollen_component(App& app, double real_dt) {
  auto update_res = app.pollen_component.update({
    app.wind_component.wind,
    real_dt,
    app.render_component.pollen_particle_renderer
  });
  return update_res.particle_update_res;
}

void update_audio_port_placement(App& app,
                                 const Ray& mouse_ray,
                                 const UIPlane::HitInfo& ui_plane_hit_info) {
  AudioPortPlacement::RayIntersectResult instrument_hit_info{};
  if (!app.mouse_state.cursor_over_new_ui_window) {
    instrument_hit_info = app.audio_port_placement.update(mouse_ray);
  }

#if UI_PLANE_IN_WORLD_SPACE
  if (!ui_plane_hit_info.hit && !app.mouse_state.cursor_over_gui_window) {
#else
  if (!app.mouse_state.cursor_over_imgui_window) {
#endif
    (void) ui_plane_hit_info;
    auto res = app.selected_instrument_components.update(
      instrument_hit_info,
      app.mouse_state.left_mouse_clicked,
      app.mouse_state.right_mouse_clicked,
      app.key_state.is_super_pressed);

    if (res.newly_want_disconnect) {
      app.ui_audio_connection_manager.attempt_to_disconnect(res.newly_want_disconnect.value());

    } else if (res.newly_selected && app.keyboard.is_pressed(Key::LeftAlt)) {
#if 0
      set_audio_editors_hidden(app, false);
      app.audio_editors.editor_mode = AudioEditorMode::Parameter;
#else
      const auto& node_storage = app.audio_component.audio_node_storage;
      auto* node_isolator = app.audio_component.get_audio_node_isolator();

      if (auto info = node_storage.get_port_info(res.newly_selected.value())) {
        uint32_t node_id = info.value().node_id;
        if (info.value().descriptor.is_input()) {
          ni::ui_toggle_isolating(node_isolator, node_id, true);

        } else if (info.value().descriptor.is_output()) {
          ni::ui_toggle_isolating(node_isolator, node_id, false);
        }
      }
#endif
    }
  }
}

void update_simple_audio_node_placement(App& app, double real_dt) {
  app.simple_audio_node_placement.update(
    app.audio_component.audio_node_storage,
    app.audio_component.get_audio_node_isolator(),
    app.render_component.simple_shape_renderer,
    SimpleShapeRenderer::make_add_resource_context(app.graphics_context),
    app.selected_instrument_components,
    real_dt);
}

void update_profile_component(App& app) {
  app.profile_component.update();
}

void update_graphics_context(App& app) {
  const cull::FrustumCullData* cull_datas[2] = {
    cull::get_global_tree_leaves_frustum_cull_data(),
    cull::get_global_branch_nodes_frustum_cull_data(),
  };

  cull::frustum_cull_gpu_context_update(cull_datas, 2);

  gfx::update_quality_preset_system(gfx::get_global_quality_preset_system(), {
    app.render_component,
    app.graphics_context,
    *app.opaque_graphics_context,
    *app.render_tree_system
  });
}

void update_glfw_context(const App& app) {
  if (app.params.need_quit) {
    app.glfw_context.set_window_should_close(true);
  }
}

void update(App& app) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("App/update");
  (void) profiler;
  const double frame_dt = app.frame_timer.delta_update().count();
  const double current_time = app.elapsed_timer.delta().count();

  gui::begin_update(gui::get_global_gui_render_data());
  update_input(app);
  auto audio_core_update_res = begin_update_audio_component(app, frame_dt);
  begin_update_render_component(app);
  begin_update_projected_nodes_system(app);
  begin_update_procedural_tree_component(app);
  const auto& audio_connect_update_res = audio_core_update_res.connection_update_result;
  const auto& ni_update_res = audio_core_update_res.node_isolator_update_result;
  const auto cam_update_res = update_camera(app, frame_dt);
  const auto& mouse_ray = cam_update_res.mouse_ray;
  update_transform_system(app);
  update_editor(app, mouse_ray);
  update_profile_component(app);
  update_graphics_context(app);
  const auto ui_plane_hit_info = begin_update_ui_plane_component(app, mouse_ray);
  const auto weather_status = update_weather_component(app, frame_dt);
  const auto season_status = update_season_component(app, frame_dt);
  update_sky_component(app, weather_status);
  update_shadow_component(app);
  update_environment_components(app, weather_status, frame_dt);
  update_wind_component(app, frame_dt);
  update_fog_component(app, weather_status, frame_dt);
  const auto pollen_update_res = update_pollen_component(app, frame_dt);
  update_bounds_system(app);
  const auto tree_sys_update_res = update_tree_systems(app, frame_dt);
  update_vine_systems(app, frame_dt);
  update_root_systems(app, frame_dt);
  update_debug_procedural_tree_component(app, mouse_ray, frame_dt);
  update_procedural_tree_component(
    app, pollen_update_res, audio_connect_update_res, ni_update_res, season_status, frame_dt);
  update_tree_roots_component(app, frame_dt);
  update_procedural_flower_component(app, frame_dt);
  update_debug_tree_roots_component(app, frame_dt);
  update_lsystem_component(app);
  update_grass_component(app, weather_status);
  update_grass_renderer(app, season_status.status);
  update_audio_port_placement(app, mouse_ray, ui_plane_hit_info);
  update_simple_audio_node_placement(app, frame_dt);
  update_render_component(app, current_time);
  update_procedural_tree_roots_renderer(app, current_time);
  update_procedural_flower_stem_renderer(app, current_time);
  update_static_model_renderer(app);
  update_terrain_renderer(app);
  update_arch_renderer(app);
  update_model_component(app);
  update_terrain_components(app, weather_status, frame_dt);
  update_soil_component(app);
  update_soil_parameter_modulator(app);
  update_debug_arch_component(app, mouse_ray, frame_dt, tree_sys_update_res);
  update_arch_component(app, frame_dt, mouse_ray);
  update_projected_nodes_systems(app, frame_dt);
  update_glfw_context(app);
  update_cursor_state(app, ui_plane_hit_info);
  update_ui(app);
  update_debug_audio_systems(app, audio_connect_update_res);
  auto ui_connect_res = update_ui_audio_connection_manager(app, audio_connect_update_res);
  (void) ui_connect_res;
  end_update_root_systems(app);
  end_update_audio_component(app, frame_dt, audio_core_update_res);
}

void begin_frame_wind_component(App& app) {
  WindParticleRenderer::SetDataContext context{
    &app.graphics_context.allocator,
    app.graphics_context.core,
    app.graphics_context.buffer_system,
    app.graphics_context.frame_info
  };

  const auto& inst_data = app.wind_component.wind_particles.read_instance_data();
  app.render_component.wind_particle_renderer.begin_frame_set_data(
    context, inst_data.data(), uint32_t(inst_data.size()));
}

void begin_frame_grass_component(App& app) {
  GrassRenderer::SetDataContext set_data_context{
    app.graphics_context.core,
    &app.graphics_context.allocator,
    app.graphics_context.buffer_system,
    app.graphics_context.command_processor,
    app.graphics_context.frame_info,
  };

  app.grass_component.begin_frame({
    app.render_component.grass_renderer,
    set_data_context
  });
}

void begin_frame_render_component(App& app) {
  auto sample_scene_depth_image =
    app.graphics_context.forward_write_back_pass.make_sample_depth_image_view();

  app.render_component.begin_frame({
    app.opaque_graphics_context,
    app.graphics_context.core,
    &app.graphics_context.allocator,
    app.graphics_context.buffer_system,
    app.graphics_context.descriptor_system,
    app.graphics_context.sampler_system,
    app.graphics_context.staging_buffer_system,
    app.graphics_context.command_processor,
    app.graphics_context.pipeline_system,
    app.graphics_context.sampled_image_manager,
    app.graphics_context.dynamic_sampled_image_manager,
    app.camera,
    app.shadow_component.get_sun_csm_descriptor(),
    app.graphics_context.frame_info,
    vk::make_forward_pass_pipeline_render_pass_info(&app.graphics_context),
    vk::make_shadow_pass_pipeline_render_pass_info(&app.graphics_context),
    app.graphics_context.shadow_pass.make_sample_image_view(),
    Optional<vk::SampleImageView>(sample_scene_depth_image),
    app.render_vine_system
  });
}

void end_frame_render_component(App& app) {
  app.render_component.end_frame();
}

void early_graphics_compute_render_component(App& app, VkCommandBuffer cmd, uint32_t frame_index) {
  app.render_component.early_graphics_compute({
    *app.opaque_graphics_context,
    app.graphics_context.core,
    cmd,
    frame_index
  });
}

void post_forward_compute_render_component(App& app, VkCommandBuffer cmd, uint32_t frame_index) {
  auto sample_scene_depth_image =
    app.graphics_context.forward_write_back_pass.make_sample_depth_image_view();
  auto extent = vk::get_forward_pass_render_image_resolution(&app.graphics_context);

  app.render_component.post_forward_compute({
    *app.opaque_graphics_context,
    app.graphics_context,
    cmd,
    frame_index,
    extent,
    Optional<vk::SampleImageView>(sample_scene_depth_image),
    app.camera
  });
}

void render_profile_component_gui(App& app) {
  if (app.imgui_component.profile_component_gui_enabled) {
    auto gui_res = app.imgui_component.profile_component_gui.render(
      app.profile_component,
      app.graphics_context.graphics_profiler,
      app.audio_component.audio_core.renderer.get_cpu_usage_estimate());
    app.profile_component.on_gui_update(gui_res);
    if (gui_res.enable_gpu_profiler) {
      app.graphics_context.graphics_profiler.set_enabled(gui_res.enable_gpu_profiler.value());
    }
    if (gui_res.close_window) {
      app.imgui_component.profile_component_gui_enabled = false;
    }
  }
}

void render_procedural_tree_component_gui(App& app) {
  if (app.imgui_component.procedural_tree_gui_enabled) {
    auto gui_res = app.imgui_component.procedural_tree_gui.render(
      app.procedural_tree_component, &app.tree_growth_system);
    app.procedural_tree_component.on_gui_update(gui_res);
    if (gui_res.close) {
      app.imgui_component.procedural_tree_gui_enabled = false;
    }

    app.debug_procedural_tree_component.render_gui(app.vine_system);

    ls::render_lsystem_component_gui(app.lsystem_component);

    if (gui_res.hide_foliage_drawable_components) {
      tree::set_all_hidden(
        app.render_tree_system, gui_res.hide_foliage_drawable_components.value());
    }
  }
}

void render_procedural_tree_roots_gui(App& app) {
  if (app.imgui_component.procedural_tree_roots_gui_enabled) {
    auto gui_res = app.imgui_component.procedural_tree_roots_gui.render(
      app.roots_radius_limiter,
      app.debug_procedural_tree_roots_component);
    app.debug_procedural_tree_roots_component.on_gui_update(gui_res);
    if (gui_res.close) {
      app.imgui_component.procedural_tree_roots_gui_enabled = false;
    }

    if (gui_res.create_roots) {
      const auto& roots_comp = app.debug_procedural_tree_roots_component;
      const auto& p = roots_comp.params.default_root_origin;
      auto p11 = p + Vec3f{urand_11f() * 16.0f, 0.0f, urand_11f() * 16.0f};
      tree_roots_component_simple_create_roots(app.tree_roots_component, p11, 1, true, false);
    }
  }
}

void render_procedural_flower_component_gui(App& app) {
  if (app.imgui_component.procedural_flower_gui_enabled) {
    auto gui_res = app.imgui_component.procedural_flower_gui.render(app.procedural_flower_component);
    app.procedural_flower_component.on_gui_update(gui_res);
    if (gui_res.close) {
      app.imgui_component.procedural_flower_gui_enabled = false;
    }
  }
}

void render_graphics_gui(App& app) {
  if (app.imgui_component.graphics_gui_enabled) {
    auto gui_res = app.imgui_component.graphics_gui.render(
      app.graphics_context,
      *app.opaque_graphics_context,
      app.render_component,
      app.shadow_component,
      *app.render_tree_system);

    app.render_component.on_gui_update(make_render_component_init_info(app), gui_res);
    app.shadow_component.on_gui_update(gui_res);
    if (gui_res.close) {
      app.imgui_component.graphics_gui_enabled = false;
    }
  }
}

void render_audio_gui(App& app) {
  if (!app.imgui_component.audio_gui_enabled) {
    return;
  }
#if 1
  debug::render_debug_audio_parameter_events_gui({
    app.audio_component.audio_node_storage,
    *app.audio_component.get_ui_parameter_manager(),
    app.simple_audio_node_placement,
    app.audio_component.get_parameter_system(),
    app.terrain_component.get_terrain(),
    app.key_trigger
  });
#endif
#if 1
  debug::render_audio_nodes_gui({
    app.audio_component,
    app.selected_instrument_components
  });
#endif
#if 1
  debug::render_debug_note_clip_state_machine_gui({
    app.audio_component,
    app.selected_instrument_components,
    *ncsm::get_global_control_note_clip_state_machine()
  });
#endif
#if 1
  env::render_debug_gui(env::get_global_global_sound_control());
#endif

  Optional<uint32_t> selected_audio_node_id;
  {
    const auto& ns = app.audio_component.audio_node_storage;
    if (auto node = app.selected_instrument_components.first_selected_node_id(ns)) {
      selected_audio_node_id = node.value();
    }
  }

  auto gui_res = app.imgui_component.audio_gui.render(app.audio_component, {
    selected_audio_node_id,
    app.audio_observation,
    app.audio_port_placement,
    app.simple_audio_node_placement,
    app.params.tuning_controlled_by_environment
  });

  if (gui_res.tuning_controlled_by_environment) {
    app.params.tuning_controlled_by_environment = gui_res.tuning_controlled_by_environment.value();
  }
  if (gui_res.tuning && !app.params.tuning_controlled_by_environment) {
    app.audio_component.ui_audio_scale.set_tuning(gui_res.tuning.value());
  }
  if (gui_res.toggle_stream_started) {
    app.audio_component.audio_core.toggle_stream_started();
  }
  if (gui_res.new_frame_info) {
    app.audio_component.audio_core.change_stream(gui_res.new_frame_info.value());
  }
  if (gui_res.change_device) {
    app.audio_component.audio_core.change_stream(gui_res.change_device.value());
  }
  if (gui_res.metronome_enabled) {
    metronome::ui_toggle_enabled(app.audio_component.get_metronome());
  }
  if (gui_res.new_bpm) {
    app.audio_component.audio_transport.set_bpm(gui_res.new_bpm.value());
  }
  if (gui_res.close) {
    app.imgui_component.audio_gui_enabled = false;
  }
}

void render_season_gui(App& app) {
  if (app.imgui_component.season_gui_enabled) {
    auto gui_res = app.imgui_component.season_gui.render(*app.season_component);
    if (gui_res.close) {
      app.imgui_component.season_gui_enabled = false;
    }
  }
}

void render_particle_gui(App& app) {
  if (app.imgui_component.particle_gui_enabled) {
    const bool close = app.imgui_component.particle_gui.render(app.pollen_component);
    if (close) {
      app.imgui_component.particle_gui_enabled = false;
    }
  }
}

void render_weather_gui(App& app) {
  if (app.imgui_component.weather_gui_enabled) {
    auto gui_res = app.imgui_component.weather_gui.render(app.weather_component);
    app.weather_component.on_gui_update(gui_res);
    if (gui_res.close) {
      app.imgui_component.weather_gui_enabled = false;
    }
  }
}

void render_editor_gui(App& app) {
  if (app.imgui_component.editor_gui_enabled) {
    auto gui_res = app.imgui_component.editor_gui.render(app.editor);
    editor::on_gui_update(&app.editor, gui_res);
    if (gui_res.close) {
      app.imgui_component.editor_gui_enabled = false;
    }
  }
}

void render_input_gui(App& app) {
  if (app.imgui_component.input_gui_enabled) {
    auto gui_res = app.imgui_component.input_gui.render(
      app.camera_component, app.controller, app.camera);
    app.camera_component.on_gui_update(gui_res);
    if (gui_res.set_position) {
      app.camera.set_position(gui_res.set_position.value());
    }
    if (gui_res.close) {
      app.imgui_component.input_gui_enabled = false;
    }
  }
}

void render_soil_gui(App& app) {
  if (app.imgui_component.soil_gui_enabled) {
    auto gui_res = app.imgui_component.soil_gui.render(
      app.soil_component, app.soil_parameter_modulator);
    app.soil_component.on_gui_update(gui_res);
    soil::on_gui_update(app.soil_parameter_modulator, gui_res);
    if (gui_res.close) {
      app.imgui_component.soil_gui_enabled = false;
    }
  }
}

void render_fog_gui(App& app) {
  if (app.imgui_component.fog_gui_enabled) {
    auto gui_res = app.imgui_component.fog_gui.render(app.fog_component);
    app.fog_component.on_gui_update(gui_res);
    if (gui_res.close) {
      app.imgui_component.fog_gui_enabled = false;
    }
  }
}

void render_arch_gui(App& app) {
  if (app.imgui_component.arch_gui_enabled) {
    auto gui_res = app.imgui_component.arch_gui.render(app.debug_arch_component);
    app.debug_arch_component.on_gui_update(gui_res);
    if (gui_res.close) {
      app.imgui_component.arch_gui_enabled = false;
    }
    render_arch_component_gui(app.arch_component);
  }
}

void render_systems_gui(App& app) {
  if (app.imgui_component.systems_gui_enabled) {
    const bounds::AccelInstanceHandle debug_instances[1] = {
      app.bounds_component.default_accel
    };
    auto gui_res = app.imgui_component.systems_gui.render({
      &app.bounds_system,
      debug_instances,
      1,
      app.bounds_component,
      app.tree_system,
      *app.render_tree_system,
      app.projected_nodes_system,
      *app.roots_system,
      *app.vine_system
    });
    app.bounds_component.on_gui_update(gui_res);
    if (gui_res.need_rebuild) {
      bounds::rebuild_accel(
        &app.bounds_system,
        gui_res.need_rebuild.value(),
        app.bounds_component.create_accel_instance_params);
    }
    if (gui_res.modify_debug_instance) {
      auto& mod = gui_res.modify_debug_instance.value();
      bounds::debug::set_draw_intersections(mod.target, mod.intersect_drawing_enabled);
      bounds::debug::set_intersection_drawing_bounds_scale(mod.target, mod.intersect_bounds_scale);
    }
    if (gui_res.close) {
      app.imgui_component.systems_gui_enabled = false;
    }
  }
}

void render_sky_gui(App& app) {
  if (app.imgui_component.sky_gui_enabled) {
    auto gui_res = app.imgui_component.sky_gui.render(app.sky_component);
    app.sky_component.on_gui_update(gui_res);
    if (gui_res.close) {
      app.imgui_component.sky_gui_enabled = false;
    }
  }
}

void render_terrain_gui(App& app) {
  if (app.imgui_component.terrain_gui_enabled) {
    auto gui_res = app.imgui_component.terrain_gui.render(app.debug_terrain_component);
    if (gui_res.alt_terrain_color_image_file_path) {
      app.terrain_component.set_new_material_image_file_path(
        gui_res.alt_terrain_color_image_file_path.value(), true);
    }
    app.debug_terrain_component.on_gui_update(gui_res);
    if (gui_res.close) {
      app.imgui_component.terrain_gui_enabled = false;
    }
  }
}

void render_gui(App& app, VkCommandBuffer cmd) {
  if (app.imgui_component.enabled) {
    vk::imgui_new_frame();
    app.imgui_component.render();
    render_profile_component_gui(app);
    render_procedural_tree_component_gui(app);
    render_procedural_tree_roots_gui(app);
    render_procedural_flower_component_gui(app);
    render_graphics_gui(app);
    render_audio_gui(app);
    render_weather_gui(app);
    render_editor_gui(app);
    render_input_gui(app);
    render_soil_gui(app);
    render_fog_gui(app);
    render_arch_gui(app);
    render_systems_gui(app);
    render_sky_gui(app);
    render_terrain_gui(app);
    render_season_gui(app);
    render_particle_gui(app);
    vk::imgui_render_frame(cmd);
  }
}

void render_shadow_pass(App& app, VkCommandBuffer cmd, uint32_t frame_index) {
  auto& context = app.graphics_context;

  auto render_begin_info = vk::make_empty_render_pass_begin_info();
  VkClearValue clear_value{};
  clear_value.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  clear_value.depthStencil = {0.0f, 0};
  render_begin_info.renderPass = context.shadow_pass.render_pass.handle;
  render_begin_info.renderArea.extent = context.shadow_pass.extent;
  render_begin_info.clearValueCount = 1;
  render_begin_info.pClearValues = &clear_value;

  const auto viewport = vk::make_full_viewport(context.shadow_pass.extent);
  const auto scissor = vk::make_full_scissor_rect(context.shadow_pass.extent);
  const uint32_t num_cascades = uint32_t(context.shadow_pass.framebuffers.size());

  for (uint32_t c = 0; c < num_cascades; c++) {
    render_begin_info.framebuffer = context.shadow_pass.framebuffers[c].handle;
    vkCmdBeginRenderPass(cmd, &render_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    const auto& csm_desc = app.shadow_component.get_sun_csm_descriptor();
    const auto& shadow_view_proj = csm_desc.light_space_view_projections[c];
    app.render_component.render_shadow({
      context.core.device,
      context.descriptor_system,
      context.sampler_system,
      context.sampled_image_manager,
      cmd,
      frame_index,
      viewport,
      scissor,
      c,
      shadow_view_proj,
      app.camera
    });

    vkCmdEndRenderPass(cmd);
  }
}

void render_forward_pass(App& app, VkCommandBuffer cmd, uint32_t frame_index, uint32_t) {
  auto& context = app.graphics_context;
  auto pass_res = vk::begin_forward_pass(&context);

  const bool enable_post_processing = true;

  vkCmdBeginRenderPass(cmd, &pass_res.pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
  app.render_component.render_forward({
    app.opaque_graphics_context,
    context.core,
    &context.allocator,
    context.sampler_system,
    context.descriptor_system,
    context.buffer_system,
    context.staging_buffer_system,
    context.command_processor,
    context.pipeline_system,
    vk::make_forward_pass_pipeline_render_pass_info(&context),
    context.sampled_image_manager,
    context.dynamic_sampled_image_manager,
    cmd,
    frame_index,
    context.frame_queue_depth,
    pass_res.viewport,
    pass_res.scissor,
    context.shadow_pass.make_sample_image_view(),
    enable_post_processing,
    app.camera,
    app.shadow_component.get_sun_csm_descriptor(),
  });

  vkCmdEndRenderPass(cmd);
}

void render_post_forward_pass(App& app, VkCommandBuffer cmd, uint32_t frame_index) {
  auto& context = app.graphics_context;
  auto pass_res = vk::begin_post_forward_pass(&context);

  vkCmdPipelineBarrier(
    cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
    0, 0, nullptr, 0, nullptr, 0, nullptr);

  vkCmdBeginRenderPass(cmd, &pass_res.pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
  app.render_component.render_post_forward({
    cmd,
    frame_index,
    pass_res.viewport,
    pass_res.scissor
  });
  vkCmdEndRenderPass(cmd);
}

void render_post_process_pass(App& app, VkCommandBuffer cmd,
                              uint32_t frame_index, uint32_t image_index) {
  auto& context = app.graphics_context;
  auto pass_res = vk::begin_post_process_pass(&context, image_index);

  Optional<vk::SampleImageView> scene_color_image;
  Optional<vk::SampleImageView> scene_depth_image;
#if 1
  scene_color_image = context.forward_write_back_pass.make_sample_color_image_view();
  scene_depth_image = context.forward_write_back_pass.make_sample_depth_image_view();
#endif
  const bool enable_post_processing = true;
  const bool present_pass_enabled = vk::get_present_pass_enabled(&context);

  vkCmdBeginRenderPass(cmd, &pass_res.pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
  app.render_component.render_post_process_pass({
    app.opaque_graphics_context,
    context.core,
    &context.allocator,
    context.sampler_system,
    context.descriptor_system,
    context.sampled_image_manager,
    context.dynamic_sampled_image_manager,
    cmd,
    frame_index,
    context.frame_queue_depth,
    pass_res.viewport,
    pass_res.scissor,
    scene_color_image,
    scene_depth_image,
    enable_post_processing,
    present_pass_enabled,
    app.camera
  });

  if (!present_pass_enabled) {
    render_gui(app, cmd);
  }

  vkCmdEndRenderPass(cmd);
}

void render_present_pass(App& app, VkCommandBuffer cmd, uint32_t frame_index, uint32_t image_index) {
  auto& context = app.graphics_context;
  auto pass_res = vk::begin_present_pass(&context, image_index);
  vk::SampleImageView color_image = context.post_process_pass.make_sample_color_image_view();

  vkCmdBeginRenderPass(cmd, &pass_res.pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
  app.render_component.render_present_pass({
    app.opaque_graphics_context,
    app.graphics_context.core,
    app.graphics_context.sampler_system,
    app.graphics_context.descriptor_system,
    cmd,
    frame_index,
    pass_res.viewport,
    pass_res.scissor,
    color_image
  });

  render_gui(app, cmd);

  vkCmdEndRenderPass(cmd);
}

[[nodiscard]] vk::Error render(App& app) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("App/render");
  (void) profiler;

  auto& context = app.graphics_context;
  const auto top_of_render_res = vk::top_of_render(&context, app.glfw_context.window);
  if (!top_of_render_res) {
    return {top_of_render_res.status, top_of_render_res.message};
  }
  {
    GROVE_PROFILE_TIC("App/begin_frame");
    gfx::begin_frame(app.opaque_graphics_context);
    begin_frame_wind_component(app);
    begin_frame_grass_component(app);
    begin_frame_render_component(app);
    GROVE_PROFILE_TOC("App/begin_frame");
  }
  const auto acq_res = vk::acquire_next_image(
    &context, top_of_render_res.value.image_available_semaphore);
  if (!acq_res) {
    return {acq_res.status, acq_res.message};
  } else if (acq_res.value.need_recreate_swapchain) {
    return {};
  }
  const uint32_t frame_index = top_of_render_res.value.frame_index;
  const uint32_t image_index = acq_res.value.image_index;
  //  Begin
  const auto& cmd_pool = context.swapchain_command_pools.pools[frame_index];
  vk::reset_command_pool(context.core.device.handle, cmd_pool.handle);
  const auto& cmd_buffer = cmd_pool.command_buffers[0];
  auto cmd_begin_info = vk::make_empty_command_buffer_begin_info();
  if (auto err = vk::begin_command_buffer(cmd_buffer.handle, &cmd_begin_info)) {
    return err;
  }
  app.graphics_context.graphics_profiler.begin_render({
    cmd_buffer.handle,
    context.frame_info
  });
  app.graphics_context.dynamic_sampled_image_manager.begin_render({
    context.core,
    cmd_buffer.handle
  });

  early_graphics_compute_render_component(app, cmd_buffer.handle, frame_index);

  {
    auto gfx_profiler = GROVE_VK_PROFILE_SCOPE("App/shadow_pass", cmd_buffer.handle);
    (void) gfx_profiler;
    GROVE_PROFILE_TIC("App/shadow_pass");
    render_shadow_pass(app, cmd_buffer.handle, frame_index);
    GROVE_PROFILE_TOC("App/shadow_pass");
  }
  {
    auto gfx_profiler = GROVE_VK_PROFILE_SCOPE("App/forward_pass", cmd_buffer.handle);
    (void) gfx_profiler;
    GROVE_PROFILE_TIC("App/forward_pass");
    render_forward_pass(app, cmd_buffer.handle, frame_index, image_index);
    GROVE_PROFILE_TOC("App/forward_pass");
  }
  {
    post_forward_compute_render_component(app, cmd_buffer.handle, frame_index);
  }
  {
    render_post_forward_pass(app, cmd_buffer.handle, frame_index);
  }
  {
    auto gfx_profiler = GROVE_VK_PROFILE_SCOPE("App/post_process_pass", cmd_buffer.handle);
    (void) gfx_profiler;
    GROVE_PROFILE_TIC("App/post_process_pass");
    render_post_process_pass(app, cmd_buffer.handle, frame_index, image_index);
    GROVE_PROFILE_TOC("App/post_process_pass");
  }
  if (vk::get_present_pass_enabled(&app.graphics_context)) {
    render_present_pass(app, cmd_buffer.handle, frame_index, image_index);
  }

  end_frame_render_component(app);

  if (auto err = vk::end_command_buffer(cmd_buffer.handle)) {
    return err;
  }

  return vk::end_frame(
    &context,
    image_index,
    cmd_buffer.handle,
    top_of_render_res.value.in_flight_fence,
    top_of_render_res.value.image_available_semaphore,
    top_of_render_res.value.render_finished_semaphore);
}

void terminate_ui_components(App& app) {
  ui::destroy_audio_editors(app.new_audio_editor_data);
  app.ui_component.terminate();
  gui::terminate_menu_gui();
  gui::terminate_world_gui();
  gui::terminate_screen0_gui();
  gui::terminate_tutorial_gui();
}

void terminate_tree_systems(App& app) {
  tree::destroy_render_tree_system(&app.render_tree_system);
  tree::terminate_resource_spiral_around_nodes_system(
    tree::get_global_resource_spiral_around_nodes_system());
}

void terminate_roots_systems(App& app) {
  bounds::destroy_radius_limiter(&app.roots_radius_limiter);
  tree::destroy_roots_system(&app.roots_system);
  tree::destroy_render_roots_system(&app.render_roots_system);
}

void terminate(App& app) {
  auto& device = app.graphics_context.core.device;
  if (device.handle != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device.handle);
  }

  terminate_ui_components(app);
  terminate_tree_systems(app);
  terminate_roots_systems(app);
  ls::destroy_lsystem_component(&app.lsystem_component);
  app.render_component.terminate(app.graphics_context.core);
  vk::destroy_and_terminate_imgui_impl(&app.imgui_impl, device.handle);
  if (app.opaque_graphics_context) {
    gfx::terminate_context(app.opaque_graphics_context);
  }
  vk::destroy_graphics_context(&app.graphics_context);
  vk::destroy_and_terminate_glfw_context(&app.glfw_context);
  app.audio_component.terminate();
}

Optional<cmd::Arguments> parse_arguments(int argc, char** argv) {
  cmd::Arguments args;
  args.parse(argc, argv);
  if (args.had_parse_error) {
    args.show_help();
    return NullOpt{};
  } else if (args.show_help_text) {
    return NullOpt{};
  } else {
    return Optional<cmd::Arguments>(args);
  }
}

} //  anon

GROVE_NAMESPACE_END

int main(int argc, char** argv) {
  using namespace grove;

  auto args = parse_arguments(argc, argv);
  if (!args) {
    return 0;
  }

  env::init_env(args.value().root_resource_directory.c_str());

  auto profiler = std::make_unique<profile::Profiler>();
  profile::set_global_profiler(profiler.get());
  profile::Runner profiler_runner;  //  start profiling

  vk::initialize_default_debug_callbacks();
  glsl::set_default_shader_directory(args.value().root_shader_directory);

  auto app = std::make_unique<App>();
  if (initialize(*app, args.value())) {
    while (!glfwWindowShouldClose(app->glfw_context.window)) {
      glfwPollEvents();
      update(*app);
      if (auto err = render(*app)) {
        log_error(to_string(err));
        break;
      }
    }
  }

  terminate(*app);
  return 0;
}

