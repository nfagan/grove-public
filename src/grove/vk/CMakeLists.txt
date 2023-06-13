function(configure_vk)

set(sources
  buffer.hpp
  buffer.cpp
  command_buffer.hpp
  command_buffer.cpp
  command_pool.hpp
  command_pool.cpp
  common.hpp
  core.hpp
  core.cpp
  debug.hpp
  debug.cpp
  descriptor_set.hpp
  descriptor_set.cpp
  device.hpp
  device.cpp
  framebuffer.hpp
  framebuffer.cpp
  image.hpp
  image.cpp
  instance.hpp
  instance.cpp
  memory.hpp
  memory.cpp
  query_pool.hpp
  query_pool.cpp
  render_pass.hpp
  render_pass.cpp
  sampler.hpp
  sampler.cpp
  shader.hpp
  shader.cpp
  sync.hpp
  sync.cpp
  surface.hpp
  surface.cpp
  swapchain.hpp
  swapchain.cpp
  physical_device.hpp
  physical_device.cpp
  vk.hpp
)

foreach(source ${sources})
    target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src/grove/vk/${source})
endforeach()

endfunction(configure_vk)

configure_vk()