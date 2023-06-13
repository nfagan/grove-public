#include "Window.hpp"

namespace grove {

float Window::framebuffer_aspect_ratio() const {
  return framebuffer_dimensions().aspect_ratio();
}

}