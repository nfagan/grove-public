#include "GLWindow.hpp"
#include "grove/common/common.hpp"
#include <GLFW/glfw3.h>

GROVE_NAMESPACE_BEGIN

GLWindow::GLWindow() : grove::Window(), window(nullptr), is_open(false) {
  //
}

GLWindow::GLWindow(GLFWwindow* window) : grove::Window(), window(window), is_open(true) {
  //
}

GLWindow::~GLWindow() {
  GLWindow::close();
}

GLFWwindow* GLWindow::window_ptr() const {
  return window;
}

void GLWindow::set_cursor_hidden(bool state) const {
  if (state) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
  } else {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  }
}

void GLWindow::close() {
  if (is_open) {
    glfwSetWindowShouldClose(window, true);
    is_open = false;
  }
}

void GLWindow::swap_buffers() const {
  glfwSwapBuffers(window);
}

void GLWindow::set_vsync(bool to) const {
  set_swap_interval(int(to));
}

void GLWindow::set_swap_interval(int interval) const {
  glfwSwapInterval(interval);
}

void GLWindow::poll_events() const {
  glfwPollEvents();
}

void GLWindow::close_if_escape_pressed() {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    close();
  }
}

bool GLWindow::should_close() const {
  return glfwWindowShouldClose(window);
}

Window::Dimensions GLWindow::dimensions() const {
  Window::Dimensions dims;
  glfwGetWindowSize(window, &dims.width, &dims.height);
  return dims;
}

Window::Dimensions GLWindow::framebuffer_dimensions() const {
  Window::Dimensions dims;
  glfwGetFramebufferSize(window, &dims.width, &dims.height);
  return dims;
}

GROVE_NAMESPACE_END
