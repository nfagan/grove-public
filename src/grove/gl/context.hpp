#pragma once

#include <memory>
#include <string>

struct GLFWwindow;

namespace grove {
  class Window;
}

namespace grove::gl {

struct WindowOptions {
  WindowOptions();

  int width;
  int height;
  bool is_full_screen;
  const char* title;
};

struct ContextOptions {
  ContextOptions();

  int context_version_major;
  int context_version_minor;
  int swap_interval;
  int msaa_samples;
  bool prefer_high_dpi_framebuffer;
  bool prefer_srgb_framebuffer;
};

struct ContextStrings {
  void show() const;

  std::string vendor;
  std::string renderer;
  std::string version;
  std::string glsl_version;
};

void initialize_glfw(const ContextOptions& context_options);
void terminate_glfw();

std::unique_ptr<Window> make_initialized_window_or_terminate(const WindowOptions& window_options,
                                                             const ContextOptions& context_options);

GLFWwindow* make_initialized_window(const WindowOptions& window_options,
                                    const ContextOptions& context_options);

GLFWwindow* make_window(const WindowOptions& window_options,
                        const ContextOptions& context_options);

ContextStrings get_context_strings();

/*
 * Capabilities
 */

constexpr int version_major() {
  return GROVE_OPENGL_CONTEXT_VERSION_MAJOR;
}

constexpr int version_minor() {
  return GROVE_OPENGL_CONTEXT_VERSION_MINOR;
}

constexpr bool supports_ssbos() {
  return version_major() > 4 || (version_major() == 4 && version_minor() >= 3);
}

int max_num_array_texture_layers();
int max_num_fbo_color_attachments();

size_t max_uniform_block_size();
size_t uniform_buffer_offset_alignment();

void set_program_label(unsigned int name, const char* label);

#if GROVE_GL_OBJECT_LABELS_ENABLED

#define GROVE_GL_LABEL_PROGRAM(name, label) grove::gl::set_program_label((name), (label))

#else

#define GROVE_GL_LABEL_PROGRAM(name, label) \
  do {} while (0)

#endif

void push_debug_group(unsigned int id, const char* message);
void pop_debug_group();

#if GROVE_GL_DEBUG_GROUPS_ENABLED

struct DebugGroupScopeHelper {
  explicit DebugGroupScopeHelper(const char* message, unsigned int id = 0) : message{message} {
    push_debug_group(id, message);
  }
  ~DebugGroupScopeHelper() {
    pop_debug_group();
  }
  const char* message;
};

#define GROVE_GL_SCOPED_DEBUG_GROUP(message) \
  grove::gl::DebugGroupScopeHelper((message))

#define GROVE_GL_PUSH_DEBUG_GROUP(id, message) grove::gl::push_debug_group((id), (message))
#define GROVE_GL_POP_DEBUG_GROUP() grove::gl::pop_debug_group()

#else

#define GROVE_GL_PUSH_DEBUG_GROUP(id, message) \
  do {} while (0)

#define GROVE_GL_POP_DEBUG_GROUP() \
  do {} while (0)

#define GROVE_GL_SCOPED_DEBUG_GROUP(message) 0

#endif

} //  gl