cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 17)

project(grove)

#  Main library
add_library(${PROJECT_NAME} STATIC)

if (APPLE)
  add_subdirectory(deps/portaudio)
  target_link_libraries(${PROJECT_NAME} PUBLIC portaudio)
else()
  target_link_directories(${PROJECT_NAME} PUBLIC deps/portaudio-win)
  target_include_directories(${PROJECT_NAME} PUBLIC deps/portaudio/include)
  target_link_libraries(${PROJECT_NAME} PUBLIC portaudio_x64)
endif()

# OpenGL
if (APPLE)
  set(GROVE_OPENGL_VERSION_STR 4.1)
  target_compile_definitions(${PROJECT_NAME} PUBLIC
    GROVE_OPENGL_CONTEXT_VERSION_MAJOR=4
    GROVE_OPENGL_CONTEXT_VERSION_MINOR=1
  )
else()
  set(GROVE_OPENGL_VERSION_STR 4.5)
  target_compile_definitions(${PROJECT_NAME} PUBLIC
    GROVE_OPENGL_CONTEXT_VERSION_MAJOR=4
    GROVE_OPENGL_CONTEXT_VERSION_MINOR=5
  )
endif()

option(GROVE_GL_OBJECT_LABELS_ENABLED "Enable OpenGL object labels." ON)
option(GROVE_GL_DEBUG_GROUPS_ENABLED "Enable OpenGL debug groups." ON)

if (GROVE_GL_OBJECT_LABELS_ENABLED)
  target_compile_definitions(${PROJECT_NAME} PUBLIC GROVE_GL_OBJECT_LABELS_ENABLED=1)
else()
  target_compile_definitions(${PROJECT_NAME} PUBLIC GROVE_GL_OBJECT_LABELS_ENABLED=0)
endif()

if (GROVE_GL_DEBUG_GROUPS_ENABLED)
  target_compile_definitions(${PROJECT_NAME} PUBLIC GROVE_GL_DEBUG_GROUPS_ENABLED=1)
else()
  target_compile_definitions(${PROJECT_NAME} PUBLIC GROVE_GL_DEBUG_GROUPS_ENABLED=0)
endif()

add_subdirectory(deps/glfw)
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)

set(GROVE_IMGUI_ROOT_DIR ${PROJECT_SOURCE_DIR}/deps/imgui)

target_include_directories(${PROJECT_NAME} PUBLIC
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/deps/glad/${GROVE_OPENGL_VERSION_STR}/include
  # imgui
  ${GROVE_IMGUI_ROOT_DIR}/imgui
  ${GROVE_IMGUI_ROOT_DIR}
  # geometric predicates
  ${PROJECT_SOURCE_DIR}/deps/GeometricPredicates
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${PROJECT_SOURCE_DIR}/deps/stb
  ${PROJECT_SOURCE_DIR}/deps/tinyobjloader
)

target_sources(${PROJECT_NAME} PRIVATE
  # glad
  ${PROJECT_SOURCE_DIR}/deps/glad/${GROVE_OPENGL_VERSION_STR}/src/glad.c
  # TinyObjLoader
  ${PROJECT_SOURCE_DIR}/deps/tinyobjloader/tiny_obj_loader.cc
  # imgui
  ${GROVE_IMGUI_ROOT_DIR}/imgui/imgui.cpp
  ${GROVE_IMGUI_ROOT_DIR}/imgui/imgui_draw.cpp
  ${GROVE_IMGUI_ROOT_DIR}/imgui/imgui_tables.cpp
  ${GROVE_IMGUI_ROOT_DIR}/imgui/imgui_widgets.cpp
  ${GROVE_IMGUI_ROOT_DIR}/imgui/imgui.h
  ${GROVE_IMGUI_ROOT_DIR}/imgui/backends/imgui_impl_opengl3.cpp
  ${GROVE_IMGUI_ROOT_DIR}/imgui/backends/imgui_impl_glfw.cpp
)

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC GROVE_HAS_VULKAN=1)

if (APPLE)
  set(GROVE_VULKAN_SDK_DIR ~/VulkanSDK/1.3.236.0)
  set(GROVE_VULKAN_INCLUDE_DIR ${GROVE_VULKAN_SDK_DIR}/macOS/include)
  set(GROVE_VULKAN_LIB_DIR ${GROVE_VULKAN_SDK_DIR}/macOS/lib)
  set(GROVE_VULKAN_LIB_NAME libvulkan.dylib)
else()
  set(GROVE_VULKAN_SDK_DIR C:\\VulkanSDK\\1.3.250.0)
  set(GROVE_VULKAN_INCLUDE_DIR ${GROVE_VULKAN_SDK_DIR}\\Include)
  set(GROVE_VULKAN_LIB_DIR ${GROVE_VULKAN_SDK_DIR}\\Lib)
  set(GROVE_VULKAN_LIB_NAME vulkan-1.lib)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
  deps/vma/include
  ${GROVE_VULKAN_INCLUDE_DIR}
)

if (APPLE)
  set(GROVE_SPIRV_LIB_NAMES
          spirv-cross-core
          spirv-cross-reflect
          spirv-cross-util
          spirv-cross-glsl)
else()
  if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(GROVE_SPIRV_LIB_NAMES
            spirv-cross-cored.lib
            spirv-cross-reflectd.lib
            spirv-cross-utild.lib
            spirv-cross-glsld.lib)
  else()
    set(GROVE_SPIRV_LIB_NAMES
            spirv-cross-core.lib
            spirv-cross-reflect.lib
            spirv-cross-util.lib
            spirv-cross-glsl.lib)
  endif()
endif()

target_link_directories(${PROJECT_NAME} PUBLIC
  ${GROVE_VULKAN_LIB_DIR}
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ${GROVE_VULKAN_LIB_NAME}
    shaderc_shared
    ${GROVE_SPIRV_LIB_NAMES}
)

set(GROVE_DEPENDENCIES_DIR ${PROJECT_SOURCE_DIR}/deps)
set(GROVE_ROOT_DIR ${PROJECT_SOURCE_DIR})
set(GROVE_PLAYGROUND_RES_DIR ${PROJECT_SOURCE_DIR}/playground/res)

target_compile_definitions(${PROJECT_NAME} PUBLIC "$<$<CONFIG:DEBUG>:GROVE_DEBUG>")
#target_compile_definitions(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELWITHDEBINFO>:GROVE_DEBUG>")

add_subdirectory(src/grove)
add_subdirectory(src/vk-app)