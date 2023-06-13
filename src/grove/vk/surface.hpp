#pragma once

#include "common.hpp"
#include <vector>

struct GLFWwindow;

namespace grove::vk {

struct FramebufferDimensions {
  uint32_t width{};
  uint32_t height{};
};

struct Surface {
  VkSurfaceKHR handle{VK_NULL_HANDLE};
};

Result<Surface> create_surface(VkInstance instance, GLFWwindow* window);
void destroy_surface(Surface* surface, VkInstance instance);
FramebufferDimensions get_framebuffer_dimensions(GLFWwindow* window);

}