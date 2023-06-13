#include "debug.hpp"
#include "GLWindow.hpp"
#include "Program.hpp"
#include "grove/common/common.hpp"
#include "grove/common/fs.hpp"
#include <glad/glad.h>
#include <string>
#include <sstream>
#include <utility>
#include <iostream>

GROVE_NAMESPACE_BEGIN

Program debug::make_program_from_files(const char* vertex_file, const char* fragment_file) {
  bool dummy_status;
  return make_program_from_files(vertex_file, fragment_file, &dummy_status);
}

Program debug::make_program_from_files(const char* vertex_file,
                                       const char* fragment_file,
                                       bool* success) {
  const std::string vert_source = grove::read_text_file(vertex_file, success);
  if (!(*success)) {
    return grove::Program();
  }
  
  const std::string frag_source = grove::read_text_file(fragment_file, success);
  if (!(*success)) {
    return grove::Program();
  }
  
  return grove::make_program(vert_source.c_str(), frag_source.c_str(), success);
}

void debug::display_gl_version_info() {
  std::cout << "GL vendor: " << glGetString(GL_VENDOR) << std::endl;
  std::cout << "GL context version: " << glGetString(GL_VERSION) << std::endl;
  std::cout << "GL shader version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
}

const char* debug::get_error_code_str(unsigned int code) {
  switch (code) {
    case GL_NO_ERROR:
      return "";
    case GL_INVALID_ENUM:
      return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
      return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
      return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY:
      return "GL_OUT_OF_MEMORY";
    default:
      return "";
  }
}

void debug::begin_render(const Window* window, float clear_depth) {
  const auto dims = window->framebuffer_dimensions();
  begin_render(dims.width, dims.height, clear_depth);
}

void debug::begin_render(int window_width, int window_height, float clear_depth) {
  glViewport(0, 0, window_width, window_height);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClearDepth(clear_depth);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void debug::begin_render(int window_width, int window_height, const float* clear_color) {
  glViewport(0, 0, window_width, window_height);
  glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
  glClearDepth(1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

debug::Application::Application() : Application(nullptr) {
  //
}

debug::Application::Application(std::function<void(Window*)> main_loop) :
  window(nullptr),
  main_loop(std::move(main_loop)),
  swap_buffers_timer(32),
  main_loop_timer(32),
  frame_timer(32),
  print_time_info(true) {
  //
}

bool debug::Application::initialize(const gl::WindowOptions& window_options,
                                    const gl::ContextOptions& context_options) {
  window = gl::make_initialized_window_or_terminate(window_options, context_options);
  return window != nullptr;
}

bool debug::Application::can_run() const {
  return window != nullptr && main_loop != nullptr;
}

void debug::Application::summarize_stats(std::stringstream& into) const {
  main_loop_timer.summarize_stats(into, "main loop:    ");
  into << "\n";
  swap_buffers_timer.summarize_stats(into, "swap buffers: ");
  into << "\n";
  frame_timer.summarize_stats(into, "total frame:  ");
  into << "\n";
}

void debug::Application::run(std::function<void(Window*)> alt_main_loop) {
  if (alt_main_loop != nullptr) {
    main_loop = std::move(alt_main_loop);
  }

  if (!can_run()) {
    return;
  }

  uint64_t frame_number = 0;
  const uint64_t summary_interval = 60;

  while (!window->should_close()) {
    frame_timer.tick();
    main_loop_timer.tick();
    main_loop(window.get());
    main_loop_timer.tock();

    swap_buffers_timer.tick();
    window->swap_buffers();
    swap_buffers_timer.tock();

    window->poll_events();
    window->close_if_escape_pressed();
    frame_timer.tock();

    if (++frame_number % summary_interval == 0 && print_time_info) {
      main_loop_timer.summarize_stats("main loop:    ");
      swap_buffers_timer.summarize_stats("swap buffers: ");
      frame_timer.summarize_stats("total frame:  ");
    }
  }

  gl::terminate_glfw();
}

GROVE_NAMESPACE_END

