#include "IMGUIWrapper.hpp"
#include "grove/math/intersect.hpp"
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <cassert>
#include <iostream>

namespace grove {

IMGUIWrapper::IMGUIWrapper() : is_initialized(false) {
  //
}

IMGUIWrapper::~IMGUIWrapper() {
  if (is_initialized) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }
}

void IMGUIWrapper::new_frame() {
  window_info.clear();
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void IMGUIWrapper::render() {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void IMGUIWrapper::push_window_info(WindowInfo info) {
  window_info.push_back(info);
}

void IMGUIWrapper::new_null_frame() {
  window_info.clear();
}

void IMGUIWrapper::end_window() {
  push_window_info(get_window_info());
  ImGui::End();
}

IMGUIWrapper::WindowInfo IMGUIWrapper::get_window_info() const {
  auto p = ImGui::GetWindowPos();
  auto s = ImGui::GetWindowSize();

  auto p0 = Vec2f{p.x, p.y};
  auto p1 = Vec2f{p.x + s.x, p.y + s.y};

  return {p0, p1};
}

bool IMGUIWrapper::cursor_intersects_with_window(double x, double y) const {
  for (const auto& win : window_info) {
    if (point_aabb_intersect(Vec2f(float(x), float(y)), win.p0, win.p1)) {
      return true;
    }
  }
  return false;
}

void IMGUIWrapper::initialize(GLFWwindow* window) {
  assert(!is_initialized);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void) io;

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  is_initialized = true;
}

}
