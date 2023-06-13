#pragma once

#include "context.hpp"
#include <memory>
#include <functional>
#include "grove/common/StatStopwatch.hpp"

struct GLFWwindow;

namespace grove {
  class Window;
  class Program;
  
  namespace debug {
    const char* get_error_code_str(unsigned int code);
    void display_gl_version_info();

    Program make_program_from_files(const char* vertex_file, const char* fragment_file);
    Program make_program_from_files(const char* vertex_file, const char* fragment_file, bool* success);
    
    void begin_render(int window_width, int window_height, float clear_depth = 1.0f);
    void begin_render(int window_width, int window_height, const float* clear_color);
    void begin_render(const Window* window, float clear_depth = 1.0f);
    
    class Application;
  }
}

class grove::debug::Application {
public:
  Application();
  explicit Application(std::function<void(Window*)> main_loop);
  
  bool initialize(const gl::WindowOptions& window_options,
                  const gl::ContextOptions& context_options);
  void run(std::function<void(Window*)> main_loop = nullptr);
  void summarize_stats(std::stringstream& into) const;

private:
  bool can_run() const;

public:
  std::unique_ptr<Window> window;
  std::function<void(Window*)> main_loop;

  StatStopwatch swap_buffers_timer;
  StatStopwatch main_loop_timer;
  StatStopwatch frame_timer;

public:
  bool print_time_info;
};
