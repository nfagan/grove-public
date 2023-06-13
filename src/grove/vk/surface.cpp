#include "surface.hpp"
#include "grove/common/common.hpp"
#include <GLFW/glfw3.h>

GROVE_NAMESPACE_BEGIN

vk::FramebufferDimensions vk::get_framebuffer_dimensions(GLFWwindow* window) {
  FramebufferDimensions result{};
  int width;
  int height;
  glfwGetFramebufferSize(window, &width, &height);
  result.width = uint32_t(width);
  result.height = uint32_t(height);
  return result;
}

vk::Result<vk::Surface> vk::create_surface(VkInstance instance, GLFWwindow* window) {
  VkSurfaceKHR surface{};
  auto create_res = glfwCreateWindowSurface(instance, window, GROVE_VK_ALLOC, &surface);
  if (create_res != VK_SUCCESS) {
    return {create_res, "Failed to create window surface."};
  } else {
    vk::Surface result{};
    result.handle = surface;
    return result;
  }
}

void vk::destroy_surface(Surface* surface, VkInstance instance) {
  if (instance != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance, surface->handle, GROVE_VK_ALLOC);
    surface->handle = VK_NULL_HANDLE;
  } else {
    GROVE_ASSERT(surface->handle == VK_NULL_HANDLE);
  }
}

GROVE_NAMESPACE_END
