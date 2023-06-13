function(configure_visual)

set(sources
  brush.hpp
  Camera.hpp
  Camera.cpp
  distribute_along_axis.hpp
  distribute_along_axis.cpp
  geometry.hpp
  geometry.cpp
  HeightMap.hpp
  Image.hpp
  image_process.hpp
  image_process.cpp
  FirstPersonCamera.hpp
  FirstPersonCamera.cpp
  OrbitCamera.hpp
  OrbitCamera.cpp
  Texture.hpp
  types.cpp
  types.hpp
  VertexBufferDataStore.hpp
  VertexBufferDataStore.cpp
  Window.hpp
  Window.cpp
)

foreach(source ${sources})
  target_sources(${PROJECT_NAME} PRIVATE ${GROVE_MODULE_SOURCE_DIR}/visual/${source})
endforeach()

endfunction(configure_visual)

configure_visual()