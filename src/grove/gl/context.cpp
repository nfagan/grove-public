#include "context.hpp"
#include "GLKeyboard.hpp"
#include "GLMouse.hpp"
#include "GLWindow.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {

inline bool load_gl_pointers() {
  return gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
}

namespace globals {
  bool is_glfw_initialized{false};
}

} //  anon

gl::WindowOptions::WindowOptions() :
  width(800),
  height(600),
  is_full_screen(false),
  title("") {
  //
}

gl::ContextOptions::ContextOptions() :
  context_version_major(GROVE_OPENGL_CONTEXT_VERSION_MAJOR),
  context_version_minor(GROVE_OPENGL_CONTEXT_VERSION_MINOR),
  swap_interval(1),
  msaa_samples(0),
  prefer_high_dpi_framebuffer(false) {
  //
}

void gl::initialize_glfw(const ContextOptions& context_options) {
  assert(!globals::is_glfw_initialized);

  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, context_options.context_version_major);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, context_options.context_version_minor);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

  if (!context_options.prefer_high_dpi_framebuffer) {
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
  }
#endif

  globals::is_glfw_initialized = true;
}

GLFWwindow* gl::make_initialized_window(const WindowOptions& window_options,
                                        const ContextOptions& context_options) {
  initialize_glfw(context_options);

  GLFWwindow* window = make_window(window_options, context_options);

  if (window == nullptr) {
    return nullptr;
  }

  glfwMakeContextCurrent(window);

  if (!load_gl_pointers()) {
    glfwDestroyWindow(window);
    return nullptr;
  }

  glfwSetKeyCallback(window, &grove::glfw::key_callback);
  glfwSetCursorPosCallback(window, &grove::glfw::cursor_position_callback);
  glfwSetMouseButtonCallback(window, &grove::glfw::mouse_button_callback);
  glfwSetScrollCallback(window, &grove::glfw::scroll_callback);

  if (window_options.is_full_screen) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  }

  return window;
}

std::unique_ptr<grove::Window>
gl::make_initialized_window_or_terminate(const WindowOptions& window_options,
                                         const ContextOptions& context_options) {
  GLFWwindow* window = gl::make_initialized_window(window_options, context_options);

  if (window == nullptr) {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to initialize OpenGL / GLFW.", "glfw_initialize");
    gl::terminate_glfw();
    return nullptr;
  }

  auto window_object = std::make_unique<grove::GLWindow>(window);
  window_object->set_swap_interval(context_options.swap_interval);
  return window_object;
}

GLFWwindow* gl::make_window(const WindowOptions& window_options,
                            const ContextOptions& context_options) {
  if (context_options.msaa_samples > 0) {
    glfwWindowHint(GLFW_SAMPLES, context_options.msaa_samples);
  }

  if (context_options.prefer_srgb_framebuffer) {
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
  }

  GLFWmonitor* monitor = window_options.is_full_screen ? glfwGetPrimaryMonitor() : nullptr;
  auto w = window_options.width;
  auto h = window_options.height;
  auto title = window_options.title;

  return glfwCreateWindow(w, h, title, monitor, nullptr);
}

void gl::terminate_glfw() {
  glfwTerminate();
  globals::is_glfw_initialized = false;
}

namespace {
std::string from_gl_string(const unsigned char* s) {
  if (s) {
    std::size_t sz{};
    while (s[sz++] != 0) {
      //
    }
    std::string str(sz-1, 'c');
    std::memcpy(str.data(), s, sz-1);
    return str;
  } else {
    return {};
  }
}
} //  anon

void gl::ContextStrings::show() const {
  GROVE_LOG_INFO_CAPTURE_META(vendor.c_str(), "ContextStrings");
  GROVE_LOG_INFO_CAPTURE_META(renderer.c_str(), "ContextStrings");
  GROVE_LOG_INFO_CAPTURE_META(version.c_str(), "ContextStrings");
  GROVE_LOG_INFO_CAPTURE_META(glsl_version.c_str(), "ContextStrings");
}

gl::ContextStrings gl::get_context_strings() {
  gl::ContextStrings result;
  if (globals::is_glfw_initialized) {
    result.renderer = from_gl_string(glGetString(GL_RENDERER));
    result.vendor = from_gl_string(glGetString(GL_VENDOR));
    result.version = from_gl_string(glGetString(GL_VERSION));
    result.glsl_version = from_gl_string(glGetString(GL_SHADING_LANGUAGE_VERSION));
  }
  return result;
}

/*
 * Capabilities
 */

int gl::max_num_array_texture_layers() {
  int v{};
  glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &v);
  return v;
}

int gl::max_num_fbo_color_attachments() {
  int v{};
  glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &v);
  return v;
}

size_t gl::max_uniform_block_size() {
  int v{};
  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &v);
  return size_t(v);
}

size_t gl::uniform_buffer_offset_alignment() {
  int v{};
  glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &v);
  return size_t(v);
}

void gl::set_program_label(unsigned int name, const char* label) {
  glObjectLabel(GL_PROGRAM, name, label == nullptr ? 0 : -1, label);
}

void gl::push_debug_group(unsigned int id, const char* message) {
  glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, id, message == nullptr ? 0 : -1, message);
}

void gl::pop_debug_group() {
  glPopDebugGroup();
}

GROVE_NAMESPACE_END
