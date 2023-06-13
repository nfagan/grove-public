set(GROVE_MODULE_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/grove)

target_sources(${PROJECT_NAME} PRIVATE
  ${GROVE_MODULE_SOURCE_DIR}/gl.hpp
  ${GROVE_MODULE_SOURCE_DIR}/input.hpp
  ${GROVE_MODULE_SOURCE_DIR}/load.hpp
  ${GROVE_MODULE_SOURCE_DIR}/math.hpp
  ${GROVE_MODULE_SOURCE_DIR}/visual.hpp
  ${GROVE_MODULE_SOURCE_DIR}/imgui.hpp
  ${GROVE_MODULE_SOURCE_DIR}/glsl.hpp
  ${GROVE_MODULE_SOURCE_DIR}/audio.hpp
  ${GROVE_MODULE_SOURCE_DIR}/env.hpp
  ${GROVE_MODULE_SOURCE_DIR}/env.cpp
)

add_subdirectory(common)
add_subdirectory(math)
add_subdirectory(gl)
add_subdirectory(input)
add_subdirectory(load)
add_subdirectory(visual)
add_subdirectory(imgui)
add_subdirectory(glsl)
add_subdirectory(serialize)
add_subdirectory(audio)
add_subdirectory(ls)
add_subdirectory(gui)
add_subdirectory(vk)